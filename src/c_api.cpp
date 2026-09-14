#include "qdd/c_api.h"
#include "qdd/control.hpp"
#include <cstring>
#include <new>
namespace {
constexpr uint32_t magic=0x51444432u;
static_assert(sizeof(qdd::Drive)<=1024,"C ABI storage too small");
static_assert(alignof(qdd::Drive)<=16,"C ABI alignment too small");
qdd::Drive* object(qdd_context* c){return std::launder(reinterpret_cast<qdd::Drive*>(c->storage));}
}
extern "C" {
qdd_config qdd_default_config(void){return {0.08f,80e-6f,90e-6f,0.011f,50e-6f,6,30,45,7,0};}
void qdd_destroy(qdd_context* c){if(c&&c->initialized==magic){object(c)->~Drive();c->initialized=0;}}
int qdd_init(qdd_context* ctx,const qdd_config* c) {
 if(!ctx||!c)return -1;
 qdd_destroy(ctx);
 qdd::DriveConfig cfg;
 cfg.motor={c->resistance,c->ld,c->lq,c->flux,c->pole_pairs};cfg.dt=c->period_s;
 cfg.gear_ratio=c->gear_ratio;cfg.current_limit=c->current_limit_A;cfg.overcurrent=c->overcurrent_A;
 if(c->algorithm!=0&&c->algorithm!=1)return -2;
 cfg.algorithm=static_cast<qdd::Algorithm>(c->algorithm);
 if(!cfg.valid())return -2;
 new(ctx->storage) qdd::Drive(cfg);ctx->initialized=magic;return 0;
}
int qdd_tick(qdd_context* ctx,const qdd_measurement* m,const qdd_command* c,qdd_output* out) {
 if(out)std::memset(out,0,sizeof(*out));
 if(!ctx||ctx->initialized!=magic||!m||!c||!out)return -1;
 qdd::Measurement in;
 in.currents={m->phase_A[0],m->phase_A[1],m->phase_A[2]};
 in.rotor_angle=m->rotor_rad;in.rotor_speed=m->rotor_rad_s;in.output_angle=m->output_rad;in.output_speed=m->output_rad_s;
 in.vbus=m->bus_V;in.winding_c=m->winding_C;in.fet_c=m->fet_C;
 in.current_age=m->current_age_s;in.encoder_age=m->encoder_age_s;in.valid=m->valid!=0;in.driver_fault=m->driver_fault!=0;
 qdd::Command cmd;
 // An out-of-range C enum is passed as an invalid core mode to latch a config fault.
 cmd.mode=(c->mode>=0&&c->mode<=4)?static_cast<qdd::Mode>(c->mode):static_cast<qdd::Mode>(255);
 cmd.current={c->id_A,c->iq_A};cmd.position=c->position_rad;cmd.velocity=c->velocity_rad_s;
 cmd.torque=c->torque_Nm;cmd.kp=c->kp_Nm_rad;cmd.kd=c->kd_Nms_rad;cmd.age=c->age_s;
 cmd.enable=c->enable!=0;cmd.calibrate=c->calibrate!=0;cmd.arm=c->arm!=0;cmd.acknowledge_fault=c->acknowledge_fault!=0;
 auto r=object(ctx)->tick(in,cmd);
 out->duty[0]=r.duty.a;out->duty[1]=r.duty.b;out->duty[2]=r.duty.c;
 out->id_A=r.current.d;out->iq_A=r.current.q;out->id_ref_A=r.reference.d;out->iq_ref_A=r.reference.q;
 out->current_limit_A=r.current_limit;out->gate_enable=r.gate_enable;
 out->state=static_cast<uint8_t>(r.state);out->fault=static_cast<uint8_t>(r.fault);
 return 0;
}
}
