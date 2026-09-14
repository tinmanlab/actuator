#include "qdd/control.hpp"
namespace qdd {
namespace {
bool good(float x) noexcept {return std::isfinite(x);}
}
bool MotorElectrical::valid() const noexcept {
 return good(resistance)&&resistance>0&&good(ld)&&ld>0&&good(lq)&&lq>0&&
        good(flux)&&flux>0&&pole_pairs>0&&pole_pairs<100;
}
Modulation svpwm(DQ v,float angle,float bus,float margin) noexcept {
 Modulation out;
 if(!good(v.d)||!good(v.q)||!good(angle)||!good(bus)||bus<=0||!good(margin)||margin<0||margin>=0.5f)return out;
 out.voltage=circle_limit(v,bus/sqrt3*(1-2*margin));
 const ABC phase=inverse_clarke(inverse_park(out.voltage,angle));
 // Zero-sequence injection: correct linear SVM utilization, not clipped SPWM.
 const float common=-0.5f*(std::max({phase.a,phase.b,phase.c})+std::min({phase.a,phase.b,phase.c}));
 out.duty={limit(0.5f+(phase.a+common)/bus,margin,1-margin),
           limit(0.5f+(phase.b+common)/bus,margin,1-margin),
           limit(0.5f+(phase.c+common)/bus,margin,1-margin)};
 out.valid=finite(out.duty);return out;
}
DQ PiFoc::voltage(DQ ref,const CurrentFeedback& f,float dt) noexcept {
 const float wc=2*pi*bandwidth_;
 const DQ e{ref.d-f.current.d,ref.q-f.current.q};
 integral_.d+=p_.resistance*wc*e.d*dt;
 integral_.q+=p_.resistance*wc*e.q*dt;
 return {p_.ld*wc*e.d+integral_.d-f.electrical_speed*p_.lq*f.current.q,
         p_.lq*wc*e.q+integral_.q+f.electrical_speed*(p_.ld*f.current.d+p_.flux)};
}
void PiFoc::track(DQ requested,DQ applied,float dt) noexcept {
 // Back-calculation uses the FINAL vector limit, not per-axis PI limits.
 const float a=limit(2*pi*bandwidth_*dt,0.0f,1.0f);
 integral_.d+=a*(applied.d-requested.d);
 integral_.q+=a*(applied.q-requested.q);
}
DQ PredictiveCurrent::voltage(DQ ref,const CurrentFeedback& f,float dt) noexcept {
 constexpr float relaxation=0.2f;
 // Slow integral disturbance correction for inverter/parameter voltage errors.
 bias_.d+=50.0f*(ref.d-f.current.d)*dt;
 bias_.q+=50.0f*(ref.q-f.current.q)*dt;
 return {bias_.d+p_.resistance*f.current.d+p_.ld*relaxation*(ref.d-f.current.d)/dt-f.electrical_speed*p_.lq*f.current.q,
         bias_.q+p_.resistance*f.current.q+p_.lq*relaxation*(ref.q-f.current.q)/dt+f.electrical_speed*(p_.ld*f.current.d+p_.flux)};
}
void PredictiveCurrent::track(DQ requested,DQ applied,float dt) noexcept {
 const float a=limit(500.0f*dt,0.0f,1.0f);
 bias_.d+=a*(applied.d-requested.d);bias_.q+=a*(applied.q-requested.q);
}
bool DriveConfig::valid() const noexcept {
 const float values[]={dt,current_bandwidth_hz,gear_ratio,current_limit,overcurrent,voltage_min,voltage_max,torque_limit,
 velocity_limit,motor_derate,motor_trip,fet_derate,fet_trip,sensor_timeout,command_timeout};
 for(float v:values)if(!good(v))return false;
 return motor.valid()&&dt>0&&dt<=0.001f&&current_bandwidth_hz>0&&current_bandwidth_hz<=0.2f/dt&&gear_ratio>0&&current_limit>0&&overcurrent>current_limit&&
 voltage_min>0&&voltage_max>voltage_min&&torque_limit>0&&velocity_limit>0&&
 motor_trip>motor_derate&&fet_trip>fet_derate&&sensor_timeout>=dt&&command_timeout>=dt&&
 servo_divider>0&&servo_divider<=1000&&calibration_samples>0&&calibration_samples<=100000&&
 (algorithm==Algorithm::Pi||algorithm==Algorithm::Predictive);
}
const char* fault_name(Fault f) noexcept {
 const char* names[]={"none","configuration","nonfinite","overcurrent","undervoltage","overvoltage",
 "overtemperature","sensor","command_timeout","gate_driver","calibration"};
 const auto i=static_cast<unsigned>(f);return i<11?names[i]:"unknown";
}
const char* state_name(State s) noexcept {
 const char* names[]={"disabled","calibrating","ready","armed","fault"};
 const auto i=static_cast<unsigned>(s);return i<5?names[i]:"unknown";
}
Drive::Drive(DriveConfig c) noexcept:c_(c),pi_(c.motor,c.current_bandwidth_hz),predictive_(c.motor),
 algorithm_(c.algorithm==Algorithm::Pi?static_cast<CurrentAlgorithm*>(&pi_):static_cast<CurrentAlgorithm*>(&predictive_)) {
 if(!c_.valid())trip(Fault::Configuration);
}
void Drive::trip(Fault f) noexcept {
 if(fault_==Fault::None)fault_=f;
 state_=State::Fault;algorithm_->reset();velocity_integral_=torque_ref_=0;
}
bool Drive::use_algorithm(CurrentAlgorithm& a) noexcept {
 if(state_!=State::Disabled&&state_!=State::Ready)return false;
 algorithm_=&a;algorithm_->reset();return true;
}
Fault Drive::health(const Measurement& m,const Command& c) const noexcept {
 if(!c_.valid())return Fault::Configuration;
 const float x[]={m.rotor_angle,m.rotor_speed,m.output_angle,m.output_speed,m.vbus,m.winding_c,m.fet_c,
 m.current_age,m.encoder_age,c.current.d,c.current.q,c.position,c.velocity,c.torque,c.kp,c.kd,c.age};
 if(!finite(m.currents))return Fault::NonFinite;
 for(float v:x)if(!good(v))return Fault::NonFinite;
 if(c.kp<0||c.kd<0||static_cast<unsigned>(c.mode)>4)return Fault::Configuration;
 if(m.driver_fault)return Fault::GateDriver;
 if(peak_abs(m.currents)>c_.overcurrent)return Fault::OverCurrent;
 if(m.vbus<c_.voltage_min)return Fault::UnderVoltage;
 if(m.vbus>c_.voltage_max)return Fault::OverVoltage;
 if(m.winding_c>=c_.motor_trip||m.fet_c>=c_.fet_trip)return Fault::OverTemperature;
 if(!m.valid||m.current_age<0||m.encoder_age<0||m.current_age>c_.sensor_timeout||m.encoder_age>c_.sensor_timeout)return Fault::Sensor;
 if(c.age<0||c.age>c_.command_timeout)return Fault::CommandTimeout;
 return Fault::None;
}
DriveOutput Drive::tick(const Measurement& m,const Command& cmd) noexcept {
 DriveOutput out;
 const Fault issue=health(m,cmd);
 if(issue!=Fault::None)trip(issue);
 if(state_==State::Fault) {
  if(issue==Fault::None&&cmd.acknowledge_fault&&!cmd.enable&&!cmd.arm) {
   state_=State::Disabled;fault_=Fault::None;offset_=sum_={};calibration_count_=0;
  }
  out.state=state_;out.fault=fault_;return out;
 }
 if(state_==State::Armed&&!cmd.enable) {
  state_=State::Ready;algorithm_->reset();velocity_integral_=torque_ref_=0;servo_count_=0;
 }
 if(cmd.calibrate&&state_==State::Disabled&&!cmd.enable){state_=State::Calibrating;sum_={};calibration_count_=0;}
 if(state_==State::Calibrating) {
  if(peak_abs(m.currents)>1.0f)trip(Fault::Calibration);
  else {
   sum_.a+=m.currents.a;sum_.b+=m.currents.b;sum_.c+=m.currents.c;
   if(++calibration_count_>=c_.calibration_samples) {
    float n=float(calibration_count_);offset_={sum_.a/n,sum_.b/n,sum_.c/n};state_=State::Ready;
   }
  }
 }
 if(cmd.arm&&cmd.enable&&state_==State::Ready){state_=State::Armed;servo_count_=0;algorithm_->reset();}
 out.state=state_;out.fault=fault_;
 if(state_!=State::Armed)return out;
 const float thermal=std::min(limit((c_.motor_trip-m.winding_c)/(c_.motor_trip-c_.motor_derate),0.0f,1.0f),
                              limit((c_.fet_trip-m.fet_c)/(c_.fet_trip-c_.fet_derate),0.0f,1.0f));
 out.current_limit=c_.current_limit*thermal;
 const ABC measured{m.currents.a-offset_.a,m.currents.b-offset_.b,m.currents.c-offset_.c};
 // Reconstruct angle at CURRENT sample time; separately predict to PWM midpoint.
 const float p=float(c_.motor.pole_pairs);
 const float sample_angle=p*(m.rotor_angle+m.rotor_speed*(m.encoder_age-m.current_age));
 const float actuation_angle=p*(m.rotor_angle+m.rotor_speed*(m.encoder_age+0.5f*c_.dt));
 out.current=park(clarke(measured),sample_angle);
 if(cmd.mode!=last_mode_){servo_count_=0;velocity_integral_=torque_ref_=0;last_mode_=cmd.mode;}
 if(servo_count_==0) {
  const float servo_dt=c_.dt*float(c_.servo_divider);
  switch(cmd.mode) {
   case Mode::Current:torque_ref_=0;break;
   case Mode::Torque:torque_ref_=cmd.torque;break;
   case Mode::Impedance:torque_ref_=cmd.torque+cmd.kp*(cmd.position-m.output_angle)+cmd.kd*(cmd.velocity-m.output_speed);break;
   case Mode::Position:
   case Mode::Velocity: {
    float target=cmd.velocity;
    if(cmd.mode==Mode::Position)target+=10.0f*(cmd.position-m.output_angle);
    target=limit(target,-c_.velocity_limit,c_.velocity_limit);
    const float error=target-m.output_speed;
    const float candidate=velocity_integral_+8.0f*error*servo_dt;
    const float raw=1.2f*error+candidate+cmd.torque;
    torque_ref_=limit(raw,-c_.torque_limit,c_.torque_limit);
    if(raw==torque_ref_||(raw>torque_ref_&&error<0)||(raw<torque_ref_&&error>0))velocity_integral_=candidate;
    break;
   }
  }
 }
 servo_count_=(servo_count_+1)%c_.servo_divider;
 torque_ref_=limit(torque_ref_,-c_.torque_limit,c_.torque_limit);
 out.torque_reference=torque_ref_;
 out.reference=(cmd.mode==Mode::Current)?cmd.current:DQ{0,torque_ref_/(c_.gear_ratio*c_.motor.torque_per_iq())};
 out.reference_limited=std::hypot(out.reference.d,out.reference.q)>out.current_limit;
 out.reference=circle_limit(out.reference,out.current_limit);
 CurrentFeedback fb{out.current,p*m.rotor_speed,m.vbus};
 const DQ requested=algorithm_->voltage(out.reference,fb,c_.dt);
 auto modulation=svpwm(requested,actuation_angle,m.vbus);
 if(!modulation.valid){trip(Fault::NonFinite);out.state=state_;out.fault=fault_;return out;}
 out.voltage_saturated=std::hypot(requested.d-modulation.voltage.d,requested.q-modulation.voltage.q)>1e-5f;
 algorithm_->track(requested,modulation.voltage,c_.dt);
 out.duty=modulation.duty;out.voltage=modulation.voltage;out.gate_enable=true;
 return out;
}
}
