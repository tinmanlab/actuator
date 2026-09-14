#include "experiments.h"
#include "qdd/plant.hpp"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
namespace {
using namespace qdd;using namespace qdd::sim;
constexpr double dt=50e-6,twopi=6.2831853071795864769;
using Row=std::array<double,48>;
using Tables=std::array<std::vector<Row>,4>;
Tables result;
struct Options {double iq,rpm,noise,bw,dead,ambient,friction;};
bool valid(const Options& o){
 const double a[]={o.iq,o.rpm,o.noise,o.bw,o.dead,o.ambient,o.friction};
 for(double x:a)if(!std::isfinite(x))return false;
 return std::abs(o.iq)<=15&&std::abs(o.rpm)<=4000&&o.noise>=0&&o.noise<=.5&&
 o.bw>=1000&&o.bw<=40000&&o.dead>=0&&o.dead<=450&&o.ambient>=0&&o.ambient<=60&&o.friction>=0&&o.friction<=.003;
}
// A laboratory dynamometer imposes speed. The two mechanical DOFs are NOT also
// integrated here; rotor angle is advanced from the imposed speed at substep
// midpoints. This midpoint split is a fixed-resolution synthetic experiment, not a real dyno.
Row dyno(const Options& o,bool switched,Tables* data,double maxstep=1e-6){
 PlantConfig pc;pc.mechanical.lock_rotor=pc.mechanical.lock_output=true;
 pc.mechanical.stiffness=pc.mechanical.damping=pc.mechanical.backlash=0;
 pc.mechanical.rotor_viscous=o.friction;pc.thermal.ambient=o.ambient;Plant p(pc);
 InverterConfig ic;ic.fidelity=switched?Fidelity::Switched:Fidelity::Averaged;ic.deadtime=o.dead*1e-9;ic.ambient=o.ambient;Inverter inv(ic);
 DcLink bus;SensorConfig sc;sc.linear_filter=true;sc.current_noise_std=o.noise;sc.current_bandwidth=o.bw;Sensors sensors(sc);
 Drive drive;DriveOutput out;Measurement m;Command c;c.mode=Mode::Current;
 sensors.capture(p.state,bus.voltage,inv.fet_c,0,dt);
 const double w=o.rpm*twopi/60,friction=o.friction*w+.005*std::tanh(w/.1);
 auto impose=[&](double t){
  const double u=std::max(0.0,t-.004),ramp=.030;
  p.state.rotor_speed=w*std::min(1.0,u/ramp);
  p.state.rotor_angle=w*(u<ramp?.5*u*u/ramp:u-.5*ramp);
 };
 double sumPin=0,sumShaft=0,sumTorque=0,sumIq=0,sumCu=0,sumFet=0,sumFriction=0,elapsed=0,limited=0,e0=0;
 auto snapshot=[&](double t){
  Row r{};const auto phase=p.currents();const auto f=sensors.filtered_current();
  const double angle=pc.motor.pole_pairs*(m.rotor_angle+m.rotor_speed*(m.encoder_age+dt*.5));
  const auto v=inverse_park(out.voltage,float(angle));
  r[0]=t;r[1]=out.reference.q;r[2]=angle;r[3]=p.state.id;r[4]=p.state.iq;
  r[5]=phase.a;r[44]=phase.b;r[45]=phase.c;r[6]=f.a;r[7]=m.currents.a;r[8]=out.current.d;r[9]=out.current.q;
  r[10]=out.voltage.d;r[11]=out.voltage.q;r[12]=v.alpha;r[13]=v.beta;
  const auto cmd=inverse_clarke(v);r[14]=cmd.a;r[15]=cmd.b;r[16]=cmd.c;
  r[17]=out.duty.a;r[18]=out.duty.b;r[19]=out.duty.c;r[20]=bus.voltage;r[21]=o.rpm;
  const double actual_w=p.state.rotor_speed, actual_friction=o.friction*actual_w+.005*std::tanh(actual_w/.1);
  r[22]=p.torque()-actual_friction;r[23]=p.state.winding_c;r[24]=inv.fet_c;
  r[34]=r[22]*actual_w;r[35]=1.5*p.resistance()*(p.state.id*p.state.id+p.state.iq*p.state.iq);r[36]=actual_friction*actual_w;
  r[37]=out.gate_enable;r[38]=int(out.state);r[39]=int(out.fault);r[40]=pc.motor.pole_pairs*p.state.rotor_angle;
  r[41]=m.current_age;r[42]=out.current_limit;r[43]=out.voltage_saturated;
  return r;
 };
 for(int tick=0;tick<2400;tick++){
  const double t=tick*dt;impose(t);m=sensors.read(t);
  c.calibrate=tick==0;c.arm=drive.state()==State::Ready;c.enable=drive.state()==State::Ready||drive.state()==State::Armed;
  c.current.q=t>=.006?float(o.iq):0;out=drive.tick(m,c);inv.begin_period(out.duty);
  if(data)(*data)[0].push_back(snapshot(t));
  if(tick==2000)e0=p.energy();
  double ph=0;bool captured=false;
  while(ph<dt-1e-14){
   double h=std::min({switched?maxstep:5e-6,dt-ph,inv.next_event(ph)-ph});
   if(!captured&&ph<dt*.5)h=std::min(h,dt*.5-ph);
   if(!(h>0))throw 1;
   const auto begin=p.currents();impose(t+ph+.5*h);const auto mid=snapshot(t+ph+.5*h);
   auto io=advance_bridge(p,inv,bus,ph+.5*h,0,h,out.gate_enable);
   const auto idq=park(clarke(io.power_current),mid[40]);
   const double tem=1.5*pc.motor.pole_pairs*(pc.motor.flux*idq.q+(pc.motor.ld-pc.motor.lq)*idq.d*idq.q);
   const double pin=io.power_vbus*io.bus_current,pac=io.voltage.a*io.power_current.a+io.voltage.b*io.power_current.b+io.voltage.c*io.power_current.c;
   if(tick>=2000){sumPin+=pin*h;sumShaft+=(tem-friction)*w*h;sumTorque+=(tem-friction)*h;sumIq+=idq.q*h;
    sumCu+=1.5*p.resistance()*(idq.d*idq.d+idq.q*idq.q)*h;sumFet+=io.loss*h;sumFriction+=friction*w*h;elapsed+=h;limited+=int(out.voltage_saturated)*h;}
   if(data&&tick>=2360){
    auto r=mid;r[3]=idq.d;r[4]=idq.q;r[5]=io.power_current.a;r[44]=io.power_current.b;r[45]=io.power_current.c;
    r[14]=io.voltage.a;r[15]=io.voltage.b;r[16]=io.voltage.c;
    for(int k=0;k<3;k++){r[25+k*2]=io.legs[k].high;r[26+k*2]=io.legs[k].low;}
    r[31]=pin;r[32]=pac;r[33]=io.loss;r[34]=(tem-friction)*w;
    r[35]=1.5*p.resistance()*(idq.d*idq.d+idq.q*idq.q);r[36]=friction*w;
    r[46]=1-std::abs(2*(ph+.5*h)/dt-1);r[47]=h;
    (*data)[1].push_back(r);
   }
   ph+=h;impose(t+ph);sensors.analog_step_linear(begin,p.currents(),h);
   if(!captured&&ph>=dt*.5-1e-14){sensors.capture(p.state,bus.voltage,inv.fet_c,t+ph,dt);captured=true;}
  }
 }
 Row r{};r[0]=o.rpm;r[1]=o.iq;r[2]=sumTorque/elapsed;
 r[3]=sumPin/elapsed;r[4]=sumShaft/elapsed;r[5]=-1;r[6]=0;r[7]=sumIq/elapsed;r[8]=std::abs(r[7]-o.iq);
 r[9]=limited/elapsed;r[10]=int(out.fault);r[11]=sumCu/elapsed;r[12]=sumFet/elapsed;r[13]=sumFriction/elapsed;
 r[14]=(p.energy()-e0)/elapsed;r[15]=r[3]-r[4]-r[11]-r[12]-r[13]-r[14];r[16]=p.state.winding_c;
 if(r[3]>.01&&r[4]>.01&&r[8]<std::max(.15,.03*std::abs(o.iq))&&r[10]==0&&
    std::abs(r[14])<.02*std::max(1.0,std::abs(r[3]))&&r[4]/r[3]<=1){r[5]=r[4]/r[3];r[6]=1;}
 return r;
}
}
int exp_run(int kind,double iq,double rpm,double noise,double bw,double dead,double ambient,double friction){
 const Options o{iq,rpm,noise,bw,dead,ambient,friction};if(kind<0||kind>2||!valid(o))return 0;
 try{
  Tables next;
  if(kind==0)next[3].push_back(dyno(o,true,&next));
  if(kind==1){
   PlantConfig pc;pc.mechanical.lock_rotor=pc.mechanical.lock_output=true;pc.thermal.ambient=ambient;Plant p(pc);
   InverterConfig ic;ic.ambient=ambient;Inverter inv(ic);p.state.iq=iq;
   for(int k=0;k<=60000;k++){
    if(k%100==0){Row r{};r[0]=k*.01;r[1]=p.state.winding_c;r[2]=p.state.case_c;r[3]=inv.fet_c;r[4]=p.resistance();r[5]=1.5*p.resistance()*iq*iq;next[2].push_back(r);}
    if(k==60000)break;
    p.step_electrical_endpoint({0,iq},0,.01);inv.heat(inv.evaluate(0,p.currents(),48,true).loss,.01);
   }
  }
  if(kind==2)for(double a:{2.,5.,10.,15.})for(double speed:{0.,300.,600.,1200.,1800.,2400.,3600.}){
   auto x=o;x.iq=a;x.rpm=speed;next[3].push_back(dyno(x,false,nullptr));
  }
  result.swap(next);return 1;
 }catch(...){return 0;}
}
int exp_rows(int t){return t>=0&&t<4?int(result[t].size()):0;}
double exp_get(int t,int r,int c){
 if(t<0||t>=4||r<0||r>=int(result[t].size())||c<0||c>=48)return std::numeric_limits<double>::quiet_NaN();
 return result[t][r][c];
}
