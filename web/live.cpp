#include "live.h"
#include "qdd/plant.hpp"
#include "qdd/protection.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <stdexcept>
namespace {
using namespace qdd; using namespace qdd::sim;
constexpr double period=50e-6;
DriveConfig drive_config(int a){DriveConfig c;c.algorithm=a?Algorithm::Predictive:Algorithm::Pi;return c;}
SensorConfig sensor_config(){SensorConfig c;c.linear_filter=true;return c;}
FastTripConfig protection_config(){FastTripConfig c;c.enabled=true;return c;}
struct Live {
 Plant plant{}; Inverter inverter{}; DcLink bus{}; Sensors sensors{sensor_config()};
 Drive drive; FastTrip trip{protection_config(),period}; DriveOutput output{};
 Command command{}; std::uint64_t ticks=0;
 double load=0,pulse=0,pulse_until=0,applied_load=0,contact_reaction=0,peak=0;
 bool contact=false,injected_fault=false,fatal=false;
 explicit Live(int a):drive(drive_config(a)){
  command.mode=Mode::Impedance;command.position=.4f;
  sensors.capture(plant.state,bus.voltage,inverter.fet_c,0,period);
 }
 void step(){
  const double t=double(ticks)*period;
  trip.observe(t,plant.currents());auto m=sensors.read(t);
  m.driver_fault=injected_fault||trip.latched();trip.supervisor_observed(t);
  command.calibrate=ticks==0;command.arm=drive.state()==State::Ready;
  command.enable=drive.state()==State::Ready||drive.state()==State::Armed||drive.state()==State::Fault;
  output=drive.tick(m,command);
  if(!trip.latched())inverter.begin_period(output.duty);
  double phase=0;bool captured=false;peak=0;
  while(phase<period-1e-14){
   // Preserve independent protection/PWM event boundaries and carrier-midpoint ADC.
   double h=std::min({5e-6,period-phase,inverter.next_event(phase)-phase,trip.next_event(t+phase)-(t+phase)});
   if(!captured&&phase<period/2)h=std::min(h,period/2-phase);
   if(h<=0)throw std::runtime_error("non-advancing browser integration step");
   applied_load=load+((t+phase<pulse_until)?pulse:0);
   contact_reaction=contact&&plant.state.output_angle>.60 ? std::max(0.0,1200*(plant.state.output_angle-.60)+4*plant.state.output_speed) : 0;
   const auto before=plant.currents();
   advance_bridge(plant,inverter,bus,phase+.5*h,applied_load+contact_reaction,h,trip.gate_allowed(output.gate_enable));
   sensors.analog_step_linear(before,plant.currents(),h);
   phase+=h;trip.observe(t+phase,plant.currents());peak=std::max(peak,peak_abs(plant.currents()));
   if(!captured&&phase>=period/2-1e-14){sensors.capture(plant.state,bus.voltage,inverter.fet_c,t+phase,period);captured=true;}
  }
  ++ticks;
 }
};
std::unique_ptr<Live> live;
}
int lab_reset(int predictive){
 if(predictive<0||predictive>1)return 0;
 try{auto fresh=std::make_unique<Live>(predictive);live=std::move(fresh);return 1;}catch(...){return 0;}
}
int lab_set(int k,double v){
 if(!live||!std::isfinite(v))return 0;
 auto& l=*live;
 switch(k){
  case 0:if(v<0||v>4||v!=std::floor(v))return 0;l.command.mode=static_cast<Mode>(int(v));break;
  case 1:if(std::abs(v)>1.2)return 0;l.command.position=float(v);break;
  case 2:if(std::abs(v)>6)return 0;l.command.velocity=float(v);break;
  case 3:if(std::abs(v)>8)return 0;l.command.torque=float(v);break;
  case 4:if(std::abs(v)>8)return 0;l.load=v;break;
  case 5:if(v!=0&&v!=1)return 0;l.contact=v==1;break;
  case 6:if(v!=0&&v!=1)return 0;l.injected_fault=v==1;break;
  case 7:if(std::abs(v)>8)return 0;l.pulse=v;l.pulse_until=double(l.ticks)*period+.12;break;
  case 8:if(std::abs(v)>15)return 0;l.command.current.q=float(v);break;
  case 9:if(v<1||v>150)return 0;l.command.kp=float(v);break;
  case 10:if(v<0||v>10)return 0;l.command.kd=float(v);break;
  default:return 0;
 }
 return 1;
}
int lab_step(int n){
 if(!live||live->fatal||n<0||n>10000)return 0;
 try{for(int i=0;i<n;++i)live->step();return n;}catch(...){live->fatal=true;return 0;}
}
double lab_get(int k){
 if(!live)return std::numeric_limits<double>::quiet_NaN();
 auto& l=*live;const auto& s=l.plant.state;const auto& o=l.output;
 switch(k){
  case 0:return double(l.ticks)*period;case 1:return s.output_angle;case 2:return s.output_speed;
  case 3:return s.id;case 4:return s.iq;case 5:return o.reference.q;case 6:return l.plant.gear_torque();
  case 7:return l.bus.voltage;case 8:return s.rotor_angle;case 9:return s.winding_c;case 10:return l.inverter.fet_c;
  case 11:return int(o.state);case 12:return int(o.fault);case 13:return !l.fatal&&l.trip.gate_allowed(o.gate_enable);
  case 14:return o.duty.a;case 15:return o.duty.b;case 16:return o.duty.c;
  case 17:return l.peak;case 18:return l.command.position;case 19:return l.applied_load;case 20:return l.contact_reaction;
  case 21:return o.torque_reference;case 22:return o.voltage_saturated;case 23:return l.trip.latched();
  case 24:return s.rotor_speed;case 25:return o.reference_limited;case 26:return l.fatal;
  default:return std::numeric_limits<double>::quiet_NaN();
 }
}
