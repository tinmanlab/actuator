#include "qdd/external.h"
#include "qdd/bench.hpp"
#include <exception>
#include <new>
#include <stdexcept>
namespace {
using namespace qdd;
using namespace qdd::sim;
constexpr double period=50e-6;
BenchConfig config(int switched,int predictive) {
 BenchConfig c;c.inverter.fidelity=switched?Fidelity::Switched:Fidelity::Averaged;
 c.step=switched?1e-6:5e-6;c.drive.algorithm=predictive?Algorithm::Predictive:Algorithm::Pi;
 // External engine alone owns BOTH inertias, coordinates and velocities.
 c.plant.mechanical.lock_rotor=true;c.plant.mechanical.lock_output=true;
 c.protection.enabled=true;c.sensor.linear_filter=true;return c;
}
class External {
 BenchConfig c_;Plant plant_;Inverter inverter_;DcLink bus_;Sensors sensors_;Drive drive_;FastTrip trip_;
 std::uint64_t ticks_=0;bool fatal_=false;
public:
 External(int switched,int predictive):c_(config(switched,predictive)),plant_(c_.plant),inverter_(c_.inverter),bus_(c_.bus),sensors_(c_.sensor),drive_(c_.drive),trip_(c_.protection,period) {}
 void fail() noexcept {fatal_=true;}
 int tick(const QddExternalInput& in,QddExternalOutput& result) {
  result={};
  if(fatal_)return -1;
  for(double x:{in.rotor_angle,in.rotor_speed,in.output_angle,in.output_speed,in.position_ref,in.velocity_ref,in.torque_ref,in.iq_ref,in.kp,in.kd})
   if(!std::isfinite(x)){fatal_=true;return -1;}
  if(in.mode<0||in.mode>4||in.enable<0||in.enable>1||in.inject_driver_fault<0||in.inject_driver_fault>1||in.kp<0||in.kd<0){fatal_=true;return -1;}
  const double t=double(ticks_)*period;
  auto impose=[&](double phase){
   // Causal constant-speed extrapolation only inside this electrical macro step.
   plant_.state.rotor_angle=in.rotor_angle+in.rotor_speed*phase;
   plant_.state.rotor_speed=in.rotor_speed;
   plant_.state.output_angle=in.output_angle+in.output_speed*phase;
   plant_.state.output_speed=in.output_speed;
  };
  impose(0);
  if(ticks_==0)sensors_.capture(plant_.state,bus_.voltage,inverter_.fet_c,t,period);
  trip_.observe(t,plant_.currents());auto measurement=sensors_.read(t);
  measurement.driver_fault=trip_.latched()||in.inject_driver_fault;trip_.supervisor_observed(t);
  Command cmd;cmd.mode=static_cast<Mode>(in.mode);
  cmd.calibrate=ticks_==0;cmd.enable=in.enable&&(drive_.state()==State::Ready||drive_.state()==State::Armed||drive_.state()==State::Fault);
  cmd.arm=cmd.enable&&drive_.state()==State::Ready;
  cmd.position=float(in.position_ref);cmd.velocity=float(in.velocity_ref);cmd.torque=float(in.torque_ref);
  cmd.current.q=float(in.iq_ref);cmd.kp=float(in.kp);cmd.kd=float(in.kd);
  const auto out=drive_.tick(measurement,cmd);
  if(!trip_.latched())inverter_.begin_period(out.duty);
  double phase=0,rotor_impulse=0,output_impulse=0,peak=0;bool captured=false;
  while(phase<period-1e-14) {
   const double h=std::min({c_.step,inverter_.next_event(phase)-phase,period-phase,trip_.next_event(t+phase)-(t+phase)});
   if(h<=0)throw std::runtime_error("external event scheduler stalled");
   const auto before=plant_.currents();const double te0=plant_.torque(),tg0=plant_.gear_torque();
   const bool gates=trip_.gate_allowed(out.gate_enable);
   // Freeze the external speed/angle at each substep midpoint for the dq solve.
   impose(phase+h/2);
   advance_bridge(plant_,inverter_,bus_,phase+h/2,0,h,gates);
   phase+=h;impose(phase);
   const auto after=plant_.currents();sensors_.analog_step_linear(before,after,h);
   trip_.observe(t+phase,after);peak=std::max(peak,peak_abs(after));
   const auto& m=c_.plant.mechanical;
   const double fm=m.rotor_viscous*in.rotor_speed+m.rotor_coulomb*std::tanh(in.rotor_speed/.1);
   const double tg=.5*(tg0+plant_.gear_torque());
   rotor_impulse+=h*(.5*(te0+plant_.torque())-tg/m.ratio-fm);output_impulse+=h*tg;
   if(!captured&&phase>=period/2-1e-14){sensors_.capture(plant_.state,bus_.voltage,inverter_.fet_c,t+phase,period);captured=true;}
  }
  ++ticks_;
  result={double(ticks_)*period,rotor_impulse/period,output_impulse/period,plant_.state.id,plant_.state.iq,out.reference.q,bus_.voltage,plant_.state.winding_c,inverter_.fet_c,
   out.duty.a,out.duty.b,out.duty.c,peak,measurement.current_age,measurement.encoder_age,
   int32_t(out.state),int32_t(out.fault),int32_t(trip_.gate_allowed(out.gate_enable)),int32_t(trip_.latched())};
  return 0;
 }
};
}
uint32_t qdd_external_abi_version(void){return 1;}
void* qdd_external_create(int32_t switched,int32_t predictive) {
 if(switched<0||switched>1||predictive<0||predictive>1)return nullptr;
 try{return new External(switched,predictive);}catch(...){return nullptr;}
}
int qdd_external_tick(void* h,const QddExternalInput* in,QddExternalOutput* out) {
 if(out)*out={};
 if(!h||!in||!out)return -1;
 try{return static_cast<External*>(h)->tick(*in,*out);}catch(...){static_cast<External*>(h)->fail();*out={};return -1;}
}
void qdd_external_destroy(void* h){delete static_cast<External*>(h);}
double qdd_external_period(void){return period;}
