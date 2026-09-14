#include "qdd/plant.hpp"
#include <cmath>
#include <stdexcept>
namespace qdd::sim {
namespace {
constexpr double tau=6.283185307179586476925286766559;
bool nonnegative(double x){return std::isfinite(x)&&x>=0;}
bool positive(double x){return std::isfinite(x)&&x>0;}
double sign(double x){return x>0?1.0:(x<0?-1.0:0.0);}
double deadzone(double x,double width){return sign(x)*std::max(0.0,std::abs(x)-0.5*width);}
}
Plant::Plant(PlantConfig c):c_(c) {
 const auto& m=c.mechanical;const auto& th=c.thermal;const auto& e=c.motor;
 if(!nonnegative(e.resistance)||!positive(e.ld)||!positive(e.lq)||!positive(e.flux)||e.pole_pairs<=0||
 !positive(m.rotor_inertia)||!positive(m.output_inertia)||!positive(m.ratio)||
 !nonnegative(m.stiffness)||!nonnegative(m.damping)||!nonnegative(m.backlash)||
 !nonnegative(m.rotor_viscous)||!nonnegative(m.rotor_coulomb)||!nonnegative(m.output_viscous)||!nonnegative(m.output_coulomb)||
 !positive(th.winding_capacity)||!positive(th.case_capacity)||!positive(th.winding_case_r)||!positive(th.case_ambient_r)||
 !std::isfinite(th.ambient)||!nonnegative(th.copper_alpha))throw std::invalid_argument("invalid plant parameters");
 state.winding_c=state.case_c=th.ambient;
}
double Plant::coupling_torque(const Mechanical& m,double x,double v) {
 if(m.backlash==0)return m.stiffness*x+m.damping*v;
 const double z=deadzone(x,m.backlash);
 if(z>0)return std::max(0.0,m.stiffness*z+m.damping*v);
 if(z<0)return std::min(0.0,m.stiffness*z+m.damping*v);
 return 0;
}
Plant::Vector Plant::derivative(const Vector& x,ABCd terminal,double load) const {
 const auto& e=c_.motor;const auto& m=c_.mechanical;const auto& th=c_.thermal;
 const double id=x[0],iq=x[1],wm=x[3],wo=x[5];
 const auto v=park(clarke(terminal),double(e.pole_pairs)*x[2]);
 const double r=e.resistance*std::max(0.0,1+th.copper_alpha*(x[6]-25));
 const double we=e.pole_pairs*wm;
 const double te=1.5*e.pole_pairs*(e.flux*iq+(double(e.ld)-e.lq)*id*iq);
 const double tg=coupling_torque(m,x[2]/m.ratio-x[4],wm/m.ratio-wo);
 const double fm=m.rotor_viscous*wm+m.rotor_coulomb*std::tanh(wm/0.1);
 const double fo=m.output_viscous*wo+m.output_coulomb*std::tanh(wo/0.01);
 const double winding_case=(x[6]-x[7])/th.winding_case_r;
 return {(v.d-r*id+we*e.lq*iq)/e.ld,
         (v.q-r*iq-we*(e.ld*id+e.flux))/e.lq,
         m.lock_rotor?0:wm,m.lock_rotor?0:(te-tg/m.ratio-fm)/m.rotor_inertia,
         m.lock_output?0:wo,m.lock_output?0:(tg-load-fo)/m.output_inertia,
         (1.5*r*(id*id+iq*iq)-winding_case)/th.winding_capacity,
         (winding_case-(x[7]-th.ambient)/th.case_ambient_r)/th.case_capacity};
}
void Plant::step(ABCd v,double load,double dt) {
 if(!finite(v)||!std::isfinite(load)||!positive(dt))throw std::invalid_argument("invalid plant input/time step");
 Vector x{state.id,state.iq,state.rotor_angle,state.rotor_speed,state.output_angle,state.output_speed,state.winding_c,state.case_c};
 auto add=[](Vector a,const Vector& b,double scale){for(std::size_t k=0;k<a.size();k++)a[k]+=scale*b[k];return a;};
 const auto k1=derivative(x,v,load);
 const auto k2=derivative(add(x,k1,dt*0.5),v,load);
 const auto k3=derivative(add(x,k2,dt*0.5),v,load);
 const auto k4=derivative(add(x,k3,dt),v,load);
 for(std::size_t k=0;k<x.size();k++)x[k]+=dt*(k1[k]+2*k2[k]+2*k3[k]+k4[k])/6;
 for(double a:x)if(!std::isfinite(a))throw std::runtime_error("nonfinite plant state; reduce step and inspect profile");
 state={x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7]};
}
void Plant::step_electrical_endpoint(Dq<double> end,double load,double dt) {
 if(!std::isfinite(end.d)||!std::isfinite(end.q)||!std::isfinite(load)||!positive(dt))throw std::invalid_argument("invalid diode endpoint/time step");
 Vector x{state.id,state.iq,state.rotor_angle,state.rotor_speed,state.output_angle,state.output_speed,state.winding_c,state.case_c};
 const double old_d=x[0],old_q=x[1];
 auto deriv=[&](Vector y,double fraction){
  y[0]=old_d+fraction*(end.d-old_d);y[1]=old_q+fraction*(end.q-old_q);
  auto k=derivative(y,{},load);k[0]=k[1]=0;return k;
 };
 auto add=[](Vector a,const Vector& b,double scale){for(std::size_t k=0;k<a.size();k++)a[k]+=scale*b[k];return a;};
 const auto k1=deriv(x,0),k2=deriv(add(x,k1,dt/2),0.5),k3=deriv(add(x,k2,dt/2),0.5),k4=deriv(add(x,k3,dt),1);
 for(std::size_t k=2;k<x.size();k++)x[k]+=dt*(k1[k]+2*k2[k]+2*k3[k]+k4[k])/6;
 for(double a:x)if(!std::isfinite(a))throw std::runtime_error("nonfinite diode/mechanical state");
 state={end.d,end.q,x[2],x[3],x[4],x[5],x[6],x[7]};
}
ABCd Plant::currents() const {return inverse_clarke(inverse_park(Dq<double>{state.id,state.iq},double(c_.motor.pole_pairs)*state.rotor_angle));}
double Plant::torque() const {return 1.5*c_.motor.pole_pairs*(c_.motor.flux*state.iq+(double(c_.motor.ld)-c_.motor.lq)*state.id*state.iq);}
double Plant::gear_torque() const {const auto& m=c_.mechanical;return coupling_torque(m,state.rotor_angle/m.ratio-state.output_angle,state.rotor_speed/m.ratio-state.output_speed);}
double Plant::resistance() const {return c_.motor.resistance*std::max(0.0,1+c_.thermal.copper_alpha*(state.winding_c-25));}
double Plant::energy() const {
 const auto& m=c_.mechanical;const double z=deadzone(state.rotor_angle/m.ratio-state.output_angle,m.backlash);
 return 0.75*(c_.motor.ld*state.id*state.id+c_.motor.lq*state.iq*state.iq)+
 0.5*m.rotor_inertia*state.rotor_speed*state.rotor_speed+0.5*m.output_inertia*state.output_speed*state.output_speed+0.5*m.stiffness*z*z;
}
Inverter::Inverter(InverterConfig c):c_(c),fet_c(c.ambient) {
 if(!positive(c.period)||!nonnegative(c.deadtime)||c.deadtime>=c.period*0.01||
 !nonnegative(c.rds_on)||!nonnegative(c.diode_drop)||!positive(c.fet_capacity)||!positive(c.fet_ambient_r)||!std::isfinite(c.ambient))
 throw std::invalid_argument("invalid inverter parameters (deadtime must be < 1% of period)");
 begin_period(duty_);
}
void Inverter::begin_period(ABC d) {
 if(!finite(d)||d.a<0||d.b<0||d.c<0||d.a>1||d.b>1||d.c>1)throw std::invalid_argument("invalid duty");
 duty_=d;edge_count_=0;
 edges_[edge_count_++]=c_.period/2;edges_[edge_count_++]=c_.period;
 if(c_.fidelity==Fidelity::Switched)for(double x:{double(d.a),double(d.b),double(d.c)}) {
  if(x==0||x==1)continue;
  const double fall=x*c_.period/2,rise=c_.period-fall;
  for(double t:{fall,fall+c_.deadtime,rise,rise+c_.deadtime})
   if(t>0&&t<c_.period)edges_[edge_count_++]=t;
 }
 std::sort(edges_.begin(),edges_.begin()+edge_count_);
}
double Inverter::next_event(double phase) const {
 for(int n=0;n<edge_count_;n++)if(edges_[n]>phase+1e-14)return edges_[n];
 return c_.period;
}
Leg Inverter::leg(double d,double phase,double i,double bus,bool enabled) const {
 Leg r;
 if(enabled) {
  const double fall=d*c_.period/2,rise=c_.period-fall;
  if(d>=1)r.high=true;
  else if(d<=0)r.low=true;
  else if(phase<fall||phase>=rise+c_.deadtime)r.high=true;
  else if(phase>=fall+c_.deadtime&&phase<rise)r.low=true;
 }
 if(r.high){r.pole_voltage=bus-c_.rds_on*i;r.bus_current=i;r.loss=c_.rds_on*i*i;}
 else if(r.low){r.pole_voltage=-c_.rds_on*i;r.loss=c_.rds_on*i*i;}
 else if(i>1e-12){r.pole_voltage=-c_.diode_drop-c_.rds_on*i;r.loss=c_.diode_drop*i+c_.rds_on*i*i;}
 else if(i<-1e-12){r.pole_voltage=bus+c_.diode_drop-c_.rds_on*i;r.bus_current=i;r.loss=-c_.diode_drop*i+c_.rds_on*i*i;}
 else {r.pole_voltage=0.5*bus;}
 return r;
}
InverterOutput Inverter::evaluate(double phase,ABCd currents,double bus,bool enable) const {
 InverterOutput r;r.power_current=currents;r.power_vbus=bus;
 const double values[]={currents.a,currents.b,currents.c};const double duties[]={duty_.a,duty_.b,duty_.c};
 for(int k=0;k<3;k++) {
  const double i=values[k],d=duties[k];
  if(c_.fidelity==Fidelity::Averaged&&enable) {
   const double blank=(d==0||d==1)?0:c_.deadtime/c_.period;
   const double effective=d-sign(i)*blank;
   r.legs[k].pole_voltage=effective*bus-c_.rds_on*i-2*blank*c_.diode_drop*sign(i);
   r.legs[k].bus_current=effective*i;
   r.legs[k].loss=c_.rds_on*i*i+2*blank*c_.diode_drop*std::abs(i);
  }else r.legs[k]=leg(d,phase,i,bus,enable);
  r.bus_current+=r.legs[k].bus_current;r.loss+=r.legs[k].loss;
 }
 const double neutral=(r.legs[0].pole_voltage+r.legs[1].pole_voltage+r.legs[2].pole_voltage)/3;
 r.voltage={r.legs[0].pole_voltage-neutral,r.legs[1].pole_voltage-neutral,r.legs[2].pole_voltage-neutral};
 return r;
}
void Inverter::heat(double loss,double dt) {
 if(!nonnegative(loss)||!positive(dt))throw std::invalid_argument("invalid inverter dissipation/time step");
 const double equilibrium=c_.ambient+loss*c_.fet_ambient_r;
 fet_c=equilibrium+(fet_c-equilibrium)*std::exp(-dt/(c_.fet_capacity*c_.fet_ambient_r));
}
DcLink::DcLink(BusConfig c):c_(c),voltage(c.source_voltage) {
 if(!positive(c.source_voltage)||!positive(c.capacitance)||!positive(c.source_resistance)||
 !positive(c.brake_resistance)||!positive(c.brake_off)||c.brake_on<=c.brake_off||!std::isfinite(c.brake_on))throw std::invalid_argument("invalid DC-link parameters");
}
void DcLink::step(double current,double dt) {
 if(!std::isfinite(current)||!positive(dt))throw std::invalid_argument("invalid DC-link input");
 if(!c_.brake_enabled)braking_=false;
 else if(voltage>=c_.brake_on)braking_=true;
 else if(voltage<=c_.brake_off)braking_=false;
 auto supply=[&](double v){if(!c_.source_enabled)return 0.0;double i=(c_.source_voltage-v)/c_.source_resistance;return c_.source_can_sink?i:std::max(0.0,i);};
 auto deriv=[&](double v){return (supply(v)-current-(braking_?v/c_.brake_resistance:0))/c_.capacitance;};
 const double first=deriv(voltage);
 voltage+=0.5*dt*(first+deriv(voltage+dt*first));
 if(!positive(voltage))throw std::runtime_error("DC bus depleted/nonfinite: use smaller step or review source");
 source_current=supply(voltage);brake_power=braking_?voltage*voltage/c_.brake_resistance:0;
}

namespace {
bool solve3(double a[3][4],double (&x)[3]) {
 for(int col=0;col<3;col++) {
  int pivot=col;for(int r=col+1;r<3;r++)if(std::abs(a[r][col])>std::abs(a[pivot][col]))pivot=r;
  if(std::abs(a[pivot][col])<1e-12)return false;
  for(int j=col;j<4;j++)std::swap(a[col][j],a[pivot][j]);
  const double div=a[col][col];for(int j=col;j<4;j++)a[col][j]/=div;
  for(int r=0;r<3;r++)if(r!=col){const double factor=a[r][col];for(int j=col;j<4;j++)a[r][j]-=factor*a[col][j];}
 }
 for(int r=0;r<3;r++)x[r]=a[r][3];
 return true;
}
}
InverterOutput advance_bridge(Plant& plant,Inverter& inverter,DcLink& bus,double phase,double load,double dt,bool enable) {
 auto stage=inverter.evaluate(phase,plant.currents(),bus.voltage,enable);
 bool diode_step=false;
 if(!(inverter.config().fidelity==Fidelity::Averaged&&enable))
  for(auto leg:stage.legs)diode_step=diode_step||(!leg.high&&!leg.low);
 if(!diode_step)plant.step(stage.voltage,load,dt);
 else {
  // Each floating leg is one of: open (i=0), low diode (i>=0), high diode (i<=0).
  // Solve at most 3^3 topologies. The all-open solution is the fast coast path.
  const auto& m=plant.config().motor;const auto& sw=inverter.config();
  const auto& old=plant.state;const double angle=m.pole_pairs*old.rotor_angle,we=m.pole_pairs*old.rotor_speed;
  const auto bd=inverse_clarke(inverse_park(Dq<double>{1,0},angle));
  const auto bq=inverse_clarke(inverse_park(Dq<double>{0,1},angle));
  const double d[]={bd.a,bd.b,bd.c},q[]={bq.a,bq.b,bq.c};
  double ad[3],aq[3],b[3];
  for(int k=0;k<3;k++) {
   ad[k]=d[k]*(plant.resistance()+m.ld/dt)+q[k]*we*m.ld;
   aq[k]=-d[k]*we*m.lq+q[k]*(plant.resistance()+m.lq/dt);
   b[k]=-d[k]*m.ld/dt*old.id+q[k]*(we*m.flux-m.lq/dt*old.iq);
  }
  bool found=false;Dq<double> endpoint{};
  for(int code=0;code<27&&!found;code++) {
   int remainder=code,kind[3];bool all_open=true;
   for(int k=0;k<3;k++) {
    kind[k]=remainder%3;remainder/=3;
    if(stage.legs[k].high)kind[k]=3;
    else if(stage.legs[k].low)kind[k]=4;
    all_open=all_open&&kind[k]==0;
   }
   double x[3]{};
   if(all_open) {
    double lo=-1e100,hi=1e100;
    for(int k=0;k<3;k++){lo=std::max(lo,-sw.diode_drop-b[k]);hi=std::min(hi,bus.voltage+sw.diode_drop-b[k]);}
    if(lo>hi+1e-9)continue;
    x[2]=0.5*(lo+hi);
   }else {
    double a[3][4]{};
    for(int k=0;k<3;k++) {
     if(kind[k]==0){a[k][0]=d[k];a[k][1]=q[k];}
     else {
      a[k][0]=ad[k]+sw.rds_on*d[k];a[k][1]=aq[k]+sw.rds_on*q[k];a[k][2]=1;
      const double rail=(kind[k]==1)?-sw.diode_drop:(kind[k]==2?bus.voltage+sw.diode_drop:(kind[k]==3?bus.voltage:0));
      a[k][3]=rail-b[k];
     }
    }
    if(!solve3(a,x))continue;
   }
   bool feasible=true;double amps[3],poles[3];
   for(int k=0;k<3;k++) {
    amps[k]=d[k]*x[0]+q[k]*x[1];poles[k]=ad[k]*x[0]+aq[k]*x[1]+b[k]+x[2];
    if(kind[k]==0&&(poles[k]<-sw.diode_drop-1e-7||poles[k]>bus.voltage+sw.diode_drop+1e-7))feasible=false;
    if(kind[k]==1&&amps[k]<-1e-8)feasible=false;
    if(kind[k]==2&&amps[k]>1e-8)feasible=false;
   }
   if(!feasible)continue;
   found=true;endpoint={x[0],x[1]};
   stage.bus_current=stage.loss=0;
   stage.power_current={amps[0],amps[1],amps[2]};stage.power_vbus=bus.voltage;
   const double neutral=(poles[0]+poles[1]+poles[2])/3;
   stage.voltage={poles[0]-neutral,poles[1]-neutral,poles[2]-neutral};
   for(int k=0;k<3;k++) {
    auto& leg=stage.legs[k];leg.pole_voltage=poles[k];
    leg.bus_current=(kind[k]==2||kind[k]==3)?amps[k]:0;
    leg.loss=kind[k]==0?0:sw.rds_on*amps[k]*amps[k];
    if(kind[k]==1||kind[k]==2)leg.loss+=sw.diode_drop*std::abs(amps[k]);
    stage.bus_current+=leg.bus_current;stage.loss+=leg.loss;
   }
  }
  if(!found)throw std::runtime_error("no admissible diode topology; reduce time step and inspect parameters");
  plant.step_electrical_endpoint(endpoint,load,dt);
 }
 bus.step(stage.bus_current,dt);inverter.heat(stage.loss,dt);
 return stage;
}
Sensors::Sensors(SensorConfig c):c_(c),rng_(c.seed?c.seed:1) {
 if(!positive(c.current_bandwidth)||!positive(c.current_range)||!nonnegative(c.current_noise_std)||!finite(c.offset)||
 c.adc_bits<2||c.adc_bits>24||c.rotor_counts<16||c.output_counts<16||
 c.current_delay_cycles<0||c.current_delay_cycles>=64||c.encoder_delay_cycles<0||c.encoder_delay_cycles>=64)throw std::invalid_argument("invalid sensor parameters");
}
double Sensors::noise() {
 // Irwin-Hall approximation of zero-mean unit-variance Gaussian; deterministic seed.
 double x=-6;
 for(int n=0;n<12;n++){rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;x+=double(rng_)/4294967296.0;}
 return x;
}
void Sensors::analog_step(ABCd current,double dt) {
 const double a=-std::expm1(-tau*c_.current_bandwidth*dt);
 filtered_.a+=a*(current.a-filtered_.a);filtered_.b+=a*(current.b-filtered_.b);filtered_.c+=a*(current.c-filtered_.c);
}
void Sensors::analog_step_linear(ABCd begin,ABCd end,double dt) {
 if(!finite(begin)||!finite(end)||!positive(dt))throw std::invalid_argument("invalid analog filter input");
 const double z=tau*c_.current_bandwidth*dt;
 const double a=-std::expm1(-z);
 // Exact first-order response to a LINEAR input segment, not an endpoint hold.
 // b=1-(1-exp(-z))/z; series avoids cancellation for tiny intervals.
 const double b=z<1e-4?z*(0.5+z*(-1.0/6+z*(1.0/24-z/120))):1-a/z;
 auto update=[&](double old,double x0,double x1){return old+a*(x0-old)+b*(x1-x0);};
 filtered_={update(filtered_.a,begin.a,end.a),update(filtered_.b,begin.b,end.b),update(filtered_.c,begin.c,end.c)};
}
void Sensors::capture(const PlantState& p,double bus,double fet,double time,double period,bool valid_sample) {
 const double levels=double((std::uint32_t(1)<<c_.adc_bits)-1);
 auto adc=[&](double value){const double q=limit(value/c_.current_range*0.5+0.5,0.0,1.0);return float((std::round(q*levels)/levels*2-1)*c_.current_range);};
 Frame f;
 const ABCd raw{filtered_.a+c_.offset.a+c_.current_noise_std*noise(),filtered_.b+c_.offset.b+c_.current_noise_std*noise(),filtered_.c+c_.offset.c+c_.current_noise_std*noise()};
 f.current={adc(raw.a),adc(raw.b),adc(raw.c)};
 valid_sample=valid_sample&&finite(raw)&&peak_abs(raw)<c_.current_range&&std::isfinite(bus)&&bus>=0&&bus<80;
 const double rotor_raw=std::round(std::remainder(p.rotor_angle,tau)/tau*c_.rotor_counts)/c_.rotor_counts*tau;
 const double output_raw=std::round(std::remainder(p.output_angle,tau)/tau*c_.output_counts)/c_.output_counts*tau;
 if(!seeded_){unwrapped_rotor_=rotor_raw;unwrapped_output_=output_raw;}
 else {
  const double dr=std::remainder(rotor_raw-last_rotor_,tau),dout=std::remainder(output_raw-last_output_,tau);
  unwrapped_rotor_+=dr;unwrapped_output_+=dout;
  speed_+=float(-std::expm1(-tau*200*period))*(float(dr/period)-speed_);
  output_speed_+=float(-std::expm1(-tau*80*period))*(float(dout/period)-output_speed_);
 }
 last_rotor_=rotor_raw;last_output_=output_raw;
 f.rotor=float(unwrapped_rotor_);f.output=float(unwrapped_output_);f.speed=speed_;f.output_speed=output_speed_;
 f.bus=float(std::round(limit(bus/80,0.0,1.0)*4095)/4095*80);f.tw=float(p.winding_c);f.tf=float(fet);f.time=time;f.valid=valid_sample;
 if(!seeded_){ring_.fill(f);seeded_=true;}else newest_=(newest_+1)%64;
 ring_[newest_]=f;
}
Measurement Sensors::read(double now) const {
 if(!seeded_){Measurement m;m.valid=false;return m;}
 const Frame& cur=ring_[(newest_+64-c_.current_delay_cycles)%64];
 const Frame& enc=ring_[(newest_+64-c_.encoder_delay_cycles)%64];
 Measurement m;
 m.currents=cur.current;m.rotor_angle=enc.rotor;m.rotor_speed=enc.speed;
 m.output_angle=enc.output;m.output_speed=enc.output_speed;
 m.vbus=cur.bus;m.winding_c=cur.tw;m.fet_c=cur.tf;
 m.current_age=float(now-cur.time);m.encoder_age=float(now-enc.time);m.valid=cur.valid&&enc.valid;
 return m;
}
}
