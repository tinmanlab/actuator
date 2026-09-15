#include "live.h"
#include "stop.hpp"
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
 Command command{}, applied_command{}; Measurement measurement{}; int algorithm=0; std::uint64_t ticks=0;
 double load=0,pulse=0,pulse_until=0,applied_load=0,contact_reaction=0,peak=0;
 qdd::web::PeriodicStop stop;
 std::array<double,16> power{};
 bool injected_fault=false,fatal=false;
 explicit Live(int a):drive(drive_config(a)),algorithm(a){
  power[12]=plant.state.case_c;power[13]=plant.resistance();power[14]=plant.energy();
  command.mode=Mode::Impedance;command.position=.4f;
  sensors.capture(plant.state,bus.voltage,inverter.fet_c,0,period);
 }
 void step(){
  const double t=double(ticks)*period;
  trip.observe(t,plant.currents());auto m=sensors.read(t);
  m.driver_fault=injected_fault||trip.latched();trip.supervisor_observed(t);
  command.calibrate=ticks==0;command.arm=drive.state()==State::Ready;
  command.enable=drive.state()==State::Ready||drive.state()==State::Armed||drive.state()==State::Fault;
  measurement=m;applied_command=command;
  output=drive.tick(m,command);
  if(!trip.latched())inverter.begin_period(output.duty);
  double phase=0;bool captured=false;peak=0;
  power.fill(0);const double energy0=plant.energy();
  while(phase<period-1e-14){
   // Preserve independent protection/PWM event boundaries and carrier-midpoint ADC.
   double h=std::min({5e-6,period-phase,inverter.next_event(phase)-phase,trip.next_event(t+phase)-(t+phase)});
   if(!captured&&phase<period/2)h=std::min(h,period/2-phase);
   if(h<=0)throw std::runtime_error("non-advancing browser integration step");
   applied_load=load+((t+phase<pulse_until)?pulse:0);
   contact_reaction=stop.reaction(plant.state.output_angle,plant.state.output_speed);
   const auto before=plant.currents();
   const auto previous=plant.state;
   const double resistance=plant.resistance();
   const auto io=advance_bridge(plant,inverter,bus,phase+.5*h,applied_load+contact_reaction,h,trip.gate_allowed(output.gate_enable));
   const auto& mc=plant.config().mechanical;
   const double wm=(previous.rotor_speed+plant.state.rotor_speed)/2;
   const double wo=(previous.output_speed+plant.state.output_speed)/2;
   const double x=((previous.rotor_angle+plant.state.rotor_angle)/mc.ratio-previous.output_angle-plant.state.output_angle)/2;
   const double v=wm/mc.ratio-wo;
   const double z=std::copysign(std::max(0.0,std::abs(x)-mc.backlash/2),x);
   const double gearloss=std::max(0.0,(Plant::coupling_torque(mc,x,v)-mc.stiffness*z)*v);
   const double friction=(mc.rotor_viscous*wm+mc.rotor_coulomb*std::tanh(wm/.1))*wm+
      (mc.output_viscous*wo+mc.output_coulomb*std::tanh(wo/.01))*wo;
   const auto i=io.power_current;
   const double ac=io.voltage.a*i.a+io.voltage.b*i.b+io.voltage.c*i.c;
   power[1]+=io.power_vbus*io.bus_current*h;power[2]+=ac*h;power[3]+=io.loss*h;
   power[4]+=resistance*(i.a*i.a+i.b*i.b+i.c*i.c)*h;
   power[5]+=friction*h;power[6]+=gearloss*h;
   power[7]+=applied_load*wo*h;power[8]+=contact_reaction*wo*h;power[9]+=io.bus_current*h;
   sensors.analog_step_linear(before,plant.currents(),h);
   phase+=h;trip.observe(t+phase,plant.currents());peak=std::max(peak,peak_abs(plant.currents()));
   if(!captured&&phase>=period/2-1e-14){sensors.capture(plant.state,bus.voltage,inverter.fet_c,t+phase,period);captured=true;}
  }
  for(int k=1;k<=9;k++)power[k]/=period;
  power[10]=(plant.energy()-energy0)/period;
  power[11]=power[1]-power[3]-power[4]-power[5]-power[6]-power[7]-power[8]-power[10];
  power[12]=plant.state.case_c;power[13]=plant.resistance();power[14]=plant.energy();power[15]=bus.brake_power;
  ++ticks;power[0]=double(ticks)*period;
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
  case 5:if(v!=0&&v!=1)return 0;if(v==0)l.stop.disable();else if(!l.stop.enable(l.plant.state.output_angle))return 0;break;
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
 try{std::array<double,12> sum{};for(int i=0;i<n;++i){live->step();for(int k=1;k<=11;k++)sum[k]+=live->power[k];}if(n)for(int k=1;k<=11;k++)live->power[k]=sum[k]/n;return n;}catch(...){live->fatal=true;return 0;}
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
  case 27:return l.stop.lower();case 28:return l.stop.upper();case 29:return l.stop.enabled();
  case 30:return l.stop.penetration(s.output_angle);
  default:return std::numeric_limits<double>::quiet_NaN();
 }
}

// Read-only sidecar: keep the public 31-field scene/CSV ABI unchanged.
// The plant is at snapshot time; held DriveOutput/Measurement are from the last tick.
double lab_signal(int k){
 if(!live)return std::numeric_limits<double>::quiet_NaN();
 const auto& l=*live;const auto& o=l.output;const auto& m=l.measurement;const auto& c=l.applied_command;
 const auto phase=l.plant.currents();
 switch(k){
  case 0:return double(l.ticks)*period;
  case 1:return l.ticks?double(l.ticks-1)*period:-1;
  case 2:return o.current.d;case 3:return o.current.q;
  case 4:return o.voltage.d;case 5:return o.voltage.q;
  case 6:return m.current_age;case 7:return m.encoder_age;
  case 8:return m.currents.a;case 9:return m.currents.b;case 10:return m.currents.c;
  case 11:return phase.a;case 12:return phase.b;case 13:return phase.c;
  case 14:return o.current_limit;case 15:return m.output_angle;case 16:return m.output_speed;
  case 17:return l.sensors.filtered_current().a;case 18:return int(c.mode);
  case 19:return c.position;case 20:return c.velocity;case 21:return c.torque;case 22:return c.current.q;
  case 23:return c.kp;case 24:return c.kd;case 25:return l.algorithm;case 26:return o.reference.d;
  case 27:return m.vbus;
  default:return std::numeric_limits<double>::quiet_NaN();
 }
}

// Diagnostics only. Power values are period means; residual retains integration/splitting error.
double lab_power(int k){
 if(!live||k<0||k>=16)return std::numeric_limits<double>::quiet_NaN();
 return live->power[k];
}
// A frozen-duty reconstruction, NOT a switched integration of the live averaged plant.
// 0..2: three PWM requests; 3..8: effective six gates with dead-time; 9: carrier.
double lab_pwm(double phase,int k){
 if(!live||!std::isfinite(phase)||phase<0||phase>=period||k<0||k>9)return std::numeric_limits<double>::quiet_NaN();
 const double carrier=1-std::abs(2*phase/period-1);
 const bool enabled=!live->fatal&&live->trip.gate_allowed(live->output.gate_enable);
 if(k==9)return carrier;
 const auto d=live->output.duty;
 if(k<3)return enabled&&carrier<(k==0?d.a:k==1?d.b:d.c);
 InverterConfig cfg=live->inverter.config();cfg.fidelity=Fidelity::Switched;Inverter inv(cfg);inv.begin_period(d);
 const auto io=inv.evaluate(phase,live->plant.currents(),live->bus.voltage,enabled);
 return (k%2)?io.legs[(k-3)/2].high:io.legs[(k-3)/2].low;
}
