#include "qdd/bench.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <map>
namespace qdd::sim {
namespace {
constexpr double tau=6.283185307179586476925286766559;
bool scenario_known(const std::string& s){for(auto v:{"locked-current","current-sine","torque-step","position","velocity","impedance","regeneration","chirp","gate-fault","watchdog","sensor-fault","overvoltage","thermal","contact","disturbance"})if(s==v)return true;return false;}
std::string trim(std::string s){auto a=s.find_first_not_of(" \r\n\t"),b=s.find_last_not_of(" \r\n\t");return a==std::string::npos?std::string{}:s.substr(a,b-a+1);}
}
void validate(const BenchConfig& c) {
 if(c.trace_divider<1||c.trace_divider>1000000||!std::isfinite(c.excitation_frequency)||
 c.excitation_frequency<=0||c.excitation_frequency>0.2/c.inverter.period||
 !c.drive.valid()||!scenario_known(c.scenario)||!std::isfinite(c.duration)||c.duration<=0||c.duration>60||
 !std::isfinite(c.step)||c.step<=0||c.step>c.inverter.period/2||
 std::abs(double(c.drive.dt)-c.inverter.period)>1e-9||
 c.step>0.1*std::min(double(c.plant.motor.ld),double(c.plant.motor.lq))/std::max(0.001,double(c.plant.motor.resistance)))
 throw std::invalid_argument("invalid bench duration/step/rate/scenario; step must resolve R/L and PWM");
 // Constructors are the single parameter-validity authority for these host components.
 (void)Plant(c.plant);(void)Inverter(c.inverter);(void)DcLink(c.bus);(void)Sensors(c.sensor);(void)FastTrip(c.protection,c.inverter.period);
}
void load_profile(BenchConfig& target,const std::string& path) {
 BenchConfig c=target; // Transactional: rejected profiles never partially modify the caller.
 std::map<std::string,double> entries;
 std::ifstream f(path);if(!f)throw std::runtime_error("cannot open profile: "+path);
 std::string line;int n=0;
 while(std::getline(f,line)) {
  ++n;line=line.substr(0,line.find('#'));line=trim(line);if(line.empty())continue;
  auto pos=line.find('=');if(pos==std::string::npos)throw std::invalid_argument("profile line "+std::to_string(n)+": expected key=value");
  const auto key=trim(line.substr(0,pos)),value=trim(line.substr(pos+1));
  std::size_t used=0;double v=std::stod(value,&used);
  if(used!=value.size()||!std::isfinite(v))throw std::invalid_argument("invalid profile number: "+key);
  if(!entries.emplace(key,v).second)throw std::invalid_argument("duplicate profile key: "+key);
 }
 // Paired legacy defaults first; explicitly namespaced values take precedence,
 // independent of line order. Existing independently configured values stay independent.
 auto apply=[&](const std::string& key,double v) {
  auto integer=[&](){if(v!=std::floor(v)||v<0||v>100000000)throw std::invalid_argument("expected nonnegative integer: "+key);return int(v);};
  auto boolean=[&](){if(v!=0&&v!=1)throw std::invalid_argument("expected 0 or 1: "+key);return v==1;};
  auto motor_value=[&](MotorElectrical& m,const std::string& name) {
   if(name=="resistance_ohm")m.resistance=float(v);
   else if(name=="ld_H")m.ld=float(v);
   else if(name=="lq_H")m.lq=float(v);
   else if(name=="flux_Wb")m.flux=float(v);
   else if(name=="pole_pairs")m.pole_pairs=integer();
   else throw std::invalid_argument("unknown motor profile key: "+key);
  };
  if(key.rfind("motor.",0)==0){motor_value(c.plant.motor,key.substr(6));motor_value(c.drive.motor,key.substr(6));}
  else if(key.rfind("plant.motor.",0)==0)motor_value(c.plant.motor,key.substr(12));
  else if(key.rfind("controller.motor.",0)==0)motor_value(c.drive.motor,key.substr(17));
  else if(key=="controller.gear_ratio")c.drive.gear_ratio=float(v);
  else if(key=="mechanical.rotor_inertia")c.plant.mechanical.rotor_inertia=v;
  else if(key=="mechanical.output_inertia")c.plant.mechanical.output_inertia=v;
  else if(key=="mechanical.ratio"){c.plant.mechanical.ratio=v;c.drive.gear_ratio=float(v);}
  else if(key=="mechanical.stiffness")c.plant.mechanical.stiffness=v;
  else if(key=="mechanical.damping")c.plant.mechanical.damping=v;
  else if(key=="mechanical.backlash_rad")c.plant.mechanical.backlash=v;
  else if(key=="mechanical.rotor_viscous")c.plant.mechanical.rotor_viscous=v;
  else if(key=="mechanical.rotor_coulomb")c.plant.mechanical.rotor_coulomb=v;
  else if(key=="mechanical.output_viscous")c.plant.mechanical.output_viscous=v;
  else if(key=="mechanical.output_coulomb")c.plant.mechanical.output_coulomb=v;
  else if(key=="inverter.deadtime_ns")c.inverter.deadtime=v*1e-9;
  else if(key=="inverter.rds_on_ohm")c.inverter.rds_on=v;
  else if(key=="inverter.diode_drop_V")c.inverter.diode_drop=v;
  else if(key=="bus.voltage_V")c.bus.source_voltage=v;
  else if(key=="bus.capacitance_F")c.bus.capacitance=v;
  else if(key=="bus.source_resistance_ohm")c.bus.source_resistance=v;
  else if(key=="bus.source_enabled")c.bus.source_enabled=boolean();
  else if(key=="bus.brake_on_V")c.bus.brake_on=v;
  else if(key=="bus.brake_off_V")c.bus.brake_off=v;
  else if(key=="bus.source_can_sink")c.bus.source_can_sink=boolean();
  else if(key=="bus.brake_enabled")c.bus.brake_enabled=boolean();
  else if(key=="bus.brake_resistance_ohm")c.bus.brake_resistance=v;
  else if(key=="sensor.linear_filter")c.sensor.linear_filter=boolean();
  else if(key=="sensor.adc_bits")c.sensor.adc_bits=integer();
  else if(key=="sensor.current_range_A")c.sensor.current_range=v;
  else if(key=="sensor.current_noise_A")c.sensor.current_noise_std=v;
  else if(key=="sensor.current_bandwidth_Hz")c.sensor.current_bandwidth=v;
  else if(key=="sensor.current_delay_cycles")c.sensor.current_delay_cycles=integer();
  else if(key=="sensor.encoder_delay_cycles")c.sensor.encoder_delay_cycles=integer();
  else if(key=="sensor.rotor_counts")c.sensor.rotor_counts=integer();
  else if(key=="sensor.output_counts")c.sensor.output_counts=integer();
  else if(key=="sensor.seed")c.sensor.seed=std::uint32_t(integer());
  else if(key=="protection.enabled")c.protection.enabled=boolean();
  else if(key=="protection.threshold_A")c.protection.threshold=v;
  else if(key=="protection.hysteresis_A")c.protection.hysteresis=v;
  else if(key=="protection.propagation_ns")c.protection.propagation=v*1e-9;
  else if(key=="protection.break_delay_ns")c.protection.break_delay=v*1e-9;
  else if(key=="protection.gate_off_ns")c.protection.gate_off_delay=v*1e-9;
  else if(key=="protection.blanking_start_us")c.protection.blanking_start=v*1e-6;
  else if(key=="protection.blanking_us")c.protection.blanking=v*1e-6;
  else if(key=="protection.reset_hold_us")c.protection.reset_hold=v*1e-6;
  else if(key=="drive.current_bandwidth_Hz")c.drive.current_bandwidth_hz=float(v);
  else if(key=="drive.current_limit_A")c.drive.current_limit=float(v);
  else if(key=="drive.overcurrent_A")c.drive.overcurrent=float(v);
  else if(key=="drive.output_torque_limit_Nm")c.drive.torque_limit=float(v);
  else throw std::invalid_argument("unknown profile key: "+key);
 };
 for(const auto& e:entries)if(e.first.rfind("plant.motor.",0)!=0&&e.first.rfind("controller.",0)!=0)apply(e.first,e.second);
 for(const auto& e:entries)if(e.first.rfind("plant.motor.",0)==0||e.first.rfind("controller.",0)==0)apply(e.first,e.second);
 validate(c);target=c;
}
BenchConfig resolved_config(BenchConfig c) {
 if(c.scenario=="locked-current"||c.scenario=="current-sine"||c.scenario=="gate-fault"||c.scenario=="watchdog"||
    c.scenario=="sensor-fault"||c.scenario=="overvoltage"||c.scenario=="thermal") {
  c.plant.mechanical.lock_rotor=true;c.plant.mechanical.lock_output=true;
 }
 if(c.scenario=="torque-step"||c.scenario=="chirp")c.plant.mechanical.lock_output=true;
 return c;
}
BenchResult run_bench(BenchConfig c) {
 c=resolved_config(c);validate(c);
 Plant plant(c.plant);Inverter inverter(c.inverter);DcLink bus(c.bus);Sensors sensors(c.sensor);Drive drive(c.drive);
 if(c.scenario=="thermal")plant.state.winding_c=105;
 const double period=c.inverter.period;
 FastTrip protection(c.protection,period);protection.observe(0,plant.currents());
 sensors.capture(plant.state,bus.voltage,inverter.fet_c,0,period);
 BenchResult result;
 if(c.scenario=="gate-fault"||c.scenario=="watchdog"||c.scenario=="sensor-fault"||c.scenario=="overvoltage")result.injection_time=0.08;
 std::ofstream csv,wave,protection_trace,diag;
 if(c.log_diagnostic&&!c.diagnostic_path.empty()) {
  diag.open(c.diagnostic_path);if(!diag)throw std::runtime_error("cannot write diagnostics");
  diag<<"boundary_s,true_id_A,true_iq_A,filter_a_A,filter_b_A,adc_a_A,adc_b_A,adc_c_A,current_sample_s,encoder_sample_s,pi_id_V,pi_iq_V,duty_a,duty_b,duty_c,vbus_V,state,fault\n"<<std::setprecision(17);
 }
 if(c.log_csv&&!c.csv_path.empty()) {
  csv.open(c.csv_path);if(!csv)throw std::runtime_error("cannot write trace: "+c.csv_path);
  csv<<"time_s,output_position_rad,output_velocity_rad_s,id_A,iq_A,iq_ref_A,gear_torque_Nm,output_torque_ref_Nm,vbus_V,winding_C,fet_C,ia_A,ib_A,ic_A,duty_a,duty_b,duty_c,state,fault,boundary_time_s,boundary_iq_A,sensor_iq_A,current_age_s,encoder_age_s,vd_V,vq_V,gate_enabled,voltage_saturated,reference_limited,requested_gate,break_latched,rotor_angle_rad,rotor_speed_rad_s\n"<<std::setprecision(10);
 }
 if(c.log_csv&&c.inverter.fidelity==Fidelity::Switched&&!c.waveform_path.empty()) {
  wave.open(c.waveform_path);if(!wave)throw std::runtime_error("cannot write PWM trace");
  wave<<"time_s,ia_A,ib_A,ic_A,va_V,vb_V,vc_V,high_a,low_a,high_b,low_b,high_c,low_c,idc_A,vbus_V\n"<<std::setprecision(12);
 }
 if(c.log_csv&&c.log_protection&&!c.protection_path.empty()) {
  protection_trace.open(c.protection_path);if(!protection_trace)throw std::runtime_error("cannot write protection trace");
  protection_trace<<"time_s,interval_s,ia_A,ib_A,ic_A,peak_A,iq_A,vbus_V,requested_gate,interval_gate,endpoint_gate,raw_comparator,comparator,blanked,break_latched,pwm_inhibited,gates_inhibited,supervisor_state,supervisor_fault\n"<<std::setprecision(14);
 }
 const std::uint64_t ticks=std::uint64_t(std::ceil(c.duration/period-1e-9));
 double err2=0;std::uint64_t samples=0;DriveOutput out;
 const auto start=std::chrono::steady_clock::now();
 for(std::uint64_t tick=0;tick<ticks;tick++) {
  const double t=double(tick)*period;
  const double boundary_iq=plant.state.iq;
  protection.observe(t,plant.currents());
  Measurement meas=sensors.read(t);
  meas.driver_fault=protection.latched();
  protection.supervisor_observed(t);
  Command command;
  command.calibrate=(tick==0);
  command.arm=drive.state()==State::Ready;
  command.enable=drive.state()==State::Ready||drive.state()==State::Armed||drive.state()==State::Fault;
  command.mode=Mode::Current;command.current.q=t>=0.01?4.0f:0;
  double load=0;
  if(c.scenario=="current-sine") {
   command.current.q=t>=0.02?float(4+std::sin(tau*c.excitation_frequency*(t-0.02))):0;
  }else if(c.scenario=="impedance") {
   command.mode=Mode::Impedance;command.position=t>=0.02?0.2f:0;
   load=t>=0.4?2:0;
  }else if(c.scenario=="position") {
   command.mode=Mode::Position;command.position=t>=0.02?0.2f:0;load=t>=0.4?1:0;
  }else if(c.scenario=="velocity") {
   command.mode=Mode::Velocity;command.velocity=t>=0.02?3.0f:0;load=t>=0.3?1:0;
  }else if(c.scenario=="regeneration") {
   command.mode=Mode::Velocity;command.velocity=(t>=0.02&&t<0.3)?5.0f:0;
  }else if(c.scenario=="torque-step") {
   command.mode=Mode::Torque;command.torque=t>=0.02?4.0f:0;
  }else if(c.scenario=="chirp") {
   command.mode=Mode::Torque;command.torque=t>=0.02?float(std::sin(tau*(2*t+10*t*t/c.duration))):0;
  }else if(c.scenario=="contact"||c.scenario=="disturbance") {
   command.mode=Mode::Impedance;command.kp=25;command.kd=1.8f;
   command.position=t>=0.1?0.85f:0;
   if(c.scenario=="disturbance")load=(t>=0.7&&t<0.82)?5:0;
  }else if(c.scenario=="thermal")command.current.q=t>=0.01?20.0f:0;
  if(t>=0.08) {
   if(c.scenario=="gate-fault")meas.driver_fault=true;
   if(c.scenario=="watchdog")command.age=0.021f;
   if(c.scenario=="sensor-fault")meas.valid=false;
   if(c.scenario=="overvoltage")meas.vbus=60; // sensor-level injected OVP fixture
  }
  const auto boundary_state=plant.state;const auto filter=sensors.filtered_current();
  const auto integral=drive.pi_integral();
  out=drive.tick(meas,command);
  if(diag)diag<<t<<','<<boundary_state.id<<','<<boundary_state.iq<<','<<filter.a<<','<<filter.b<<','<<meas.currents.a<<','<<meas.currents.b<<','<<meas.currents.c
  <<','<<t-meas.current_age<<','<<t-meas.encoder_age<<','<<integral.d<<','<<integral.q<<','<<out.duty.a<<','<<out.duty.b<<','<<out.duty.c<<','<<bus.voltage<<','<<int(out.state)<<','<<int(out.fault)<<'\n';
  // Hold the last PWM pattern through modeled BREAK/driver propagation.
  // Supervisor fault handling must not bypass the configured gate-off delay.
  if(!protection.latched())inverter.begin_period(out.duty);
  if(out.fault!=Fault::None&&result.first_fault<0)result.first_fault=t;
  if(out.gate_enable){++result.armed_ticks;if(out.voltage_saturated)++result.saturated_ticks;if(out.reference_limited)++result.reference_limited_ticks;}
  // Sample at carrier midpoint; computation occurs at next carrier boundary.
  double phase=0;bool captured=false;
  while(phase<period-1e-14) {
   const double event=inverter.next_event(phase);
   const double h=std::min({c.step,event-phase,period-phase,protection.next_event(t+phase)-(t+phase)});
   if(h<=0)throw std::runtime_error("invalid event scheduler step");
   const bool interval_gate=protection.gate_allowed(out.gate_enable);
   const auto before_current=plant.currents();
   // A native unilateral angular fixture, NOT a general rigid-body collision engine.
   double applied_load=load;
   if(c.scenario=="contact"&&plant.state.output_angle>0.60)
    applied_load+=std::max(0.0,1200*(plant.state.output_angle-0.60)+4*plant.state.output_speed);
   const auto stage=advance_bridge(plant,inverter,bus,phase+0.5*h,applied_load,h,interval_gate);
   if(wave&&t+phase>=0.02&&t+phase<0.0202) {
    const auto current=plant.currents(); // endpoint currents; voltage/gates were held over the preceding interval
    wave<<t+phase+h<<','<<current.a<<','<<current.b<<','<<current.c<<','<<stage.voltage.a<<','<<stage.voltage.b<<','<<stage.voltage.c;
    for(auto leg:stage.legs)wave<<','<<leg.high<<','<<leg.low;
    wave<<','<<stage.bus_current<<','<<bus.voltage<<'\n';
   }
   if(c.sensor.linear_filter)sensors.analog_step_linear(before_current,plant.currents(),h);
   else sensors.analog_step(plant.currents(),h);
   phase+=h;++result.plant_steps;
   protection.observe(t+phase,plant.currents());
   if(protection_trace) {
    const auto i=plant.currents();
    protection_trace<<t+phase<<','<<h<<','<<i.a<<','<<i.b<<','<<i.c<<','<<peak_abs(i)<<','<<plant.state.iq<<','<<bus.voltage
    <<','<<out.gate_enable<<','<<interval_gate<<','<<protection.gate_allowed(out.gate_enable)
    <<','<<protection.raw_asserted()<<','<<protection.comparator_asserted()<<','<<protection.blanked(t+phase)
    <<','<<protection.latched()<<','<<protection.pwm_inhibited()<<','<<protection.gates_inhibited()<<','<<int(out.state)<<','<<int(out.fault)<<'\n';
   }
   result.max_current=std::max(result.max_current,peak_abs(plant.currents()));
   result.max_bus=std::max(result.max_bus,bus.voltage);
   if(!captured&&phase>=period/2-1e-14) {
    sensors.capture(plant.state,bus.voltage,inverter.fet_c,t+phase,period);captured=true;
   }
  }
  if(t>=0.02&&out.gate_enable) {double e=plant.state.iq-out.reference.q;err2+=e*e;++samples;}
  if(c.log_csv&&tick%std::uint64_t(c.trace_divider)==0) {
   const auto i=plant.currents();
   Row row{t+period,plant.state.output_angle,plant.state.output_speed,plant.state.id,plant.state.iq,out.reference.q,
   plant.gear_torque(),out.torque_reference,bus.voltage,plant.state.winding_c,inverter.fet_c,i.a,i.b,i.c,
   out.duty.a,out.duty.b,out.duty.c,int(out.state),int(out.fault)};
   row.rotor_angle=plant.state.rotor_angle;row.rotor_speed=plant.state.rotor_speed;
   row.boundary_time=t;row.boundary_iq=boundary_iq;row.sensor_iq=out.current.q;
   row.current_age=meas.current_age;row.encoder_age=meas.encoder_age;
   row.vd=out.voltage.d;row.vq=out.voltage.q;
   row.requested_gate=out.gate_enable;row.break_latched=protection.latched();
   row.gate_enabled=protection.gate_allowed(out.gate_enable);row.voltage_saturated=out.voltage_saturated;row.reference_limited=out.reference_limited;
   result.rows.push_back(row);
   if(csv)csv<<row.time<<','<<row.pos<<','<<row.vel<<','<<row.id<<','<<row.iq<<','<<row.iq_ref<<','<<row.torque<<','<<row.tau_ref<<','<<row.vbus<<','<<row.winding<<','<<row.fet<<','<<row.ia<<','<<row.ib<<','<<row.ic<<','<<row.da<<','<<row.db<<','<<row.dc<<','<<row.state<<','<<row.fault<<','<<row.boundary_time<<','<<row.boundary_iq<<','<<row.sensor_iq<<','<<row.current_age<<','<<row.encoder_age<<','<<row.vd<<','<<row.vq<<','<<row.gate_enabled<<','<<row.voltage_saturated<<','<<row.reference_limited<<','<<row.requested_gate<<','<<row.break_latched<<','<<row.rotor_angle<<','<<row.rotor_speed<<'\n';
  }
 }
 result.wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 result.duration=double(ticks)*period;result.control_ticks=ticks;
 result.current_rms_error=samples?std::sqrt(err2/double(samples)):0;
 result.final_position=plant.state.output_angle;result.final_speed=plant.state.output_speed;
 result.final_iq=plant.state.iq;result.final_iq_ref=out.reference.q;result.final_gear_torque=plant.gear_torque();
 result.final_winding=plant.state.winding_c;result.final_fet=inverter.fet_c;
 result.protection_events=protection.events();
 result.fault=drive.fault();result.state=drive.state();
 return result;
}
std::string result_json(const BenchConfig& c,const BenchResult& r) {
 std::ostringstream s;s<<std::setprecision(12);
 s<<"{\n  \"scenario\": \""<<c.scenario<<"\",\n  \"fidelity\": \""<<(c.inverter.fidelity==Fidelity::Averaged?"averaged":"switched")
 <<"\",\n  \"algorithm\": \""<<(c.drive.algorithm==Algorithm::Pi?"pi":"predictive")<<"\",\n"
 <<"  \"simulated_seconds\": "<<r.duration<<",\n  \"wall_seconds\": "<<r.wall
 <<",\n  \"sim_to_wall_ratio\": "<<r.duration/r.wall<<",\n  \"pwm_Hz\": "<<1/c.inverter.period<<",\n  \"max_step_s\": "<<c.step
 <<",\n  \"csv_logging\": "<<(c.log_csv?"true":"false")<<",\n  \"plant_steps\": "<<r.plant_steps<<",\n  \"control_ticks\": "<<r.control_ticks
 <<",\n  \"iq_rms_error_after_20ms_A\": "<<r.current_rms_error<<",\n  \"max_abs_phase_current_A\": "<<r.max_current
 <<",\n  \"max_bus_V\": "<<r.max_bus<<",\n  \"final_position_rad\": "<<r.final_position<<",\n  \"final_speed_rad_s\": "<<r.final_speed
 <<",\n  \"final_iq_A\": "<<r.final_iq<<",\n  \"final_iq_ref_A\": "<<r.final_iq_ref<<",\n  \"final_gear_torque_Nm\": "<<r.final_gear_torque
 <<",\n  \"final_winding_C\": "<<r.final_winding<<",\n  \"final_fet_C\": "<<r.final_fet
 <<",\n  \"state\": \""<<state_name(r.state)<<"\",\n  \"fault\": \""<<fault_name(r.fault)
 <<"\",\n  \"source_sha256\": \""<<QDD_SOURCE_SHA256<<"\",\n  \"effective_config\": "<<config_json(c)
 <<",\n  \"voltage_saturation_fraction\": "<<(r.armed_ticks?double(r.saturated_ticks)/double(r.armed_ticks):0)
 <<",\n  \"reference_limit_fraction\": "<<(r.armed_ticks?double(r.reference_limited_ticks)/double(r.armed_ticks):0)
 <<",\n  \"first_fault_s\": ";
 if(r.first_fault<0)s<<"null";else s<<r.first_fault;
 s<<",\n  \"fault_injection_s\": ";if(r.injection_time<0)s<<"null";else s<<r.injection_time;
 s<<",\n  \"fault_latency_s\": ";
 if(r.first_fault<0||r.injection_time<0||r.first_fault<r.injection_time)s<<"null";else s<<r.first_fault-r.injection_time;
 s<<",\n  \"fast_trip_enabled\": "<<(c.protection.enabled?"true":"false");
 s<<",\n  \"protection_timing_semantics\": \"causal_endpoint_detection_with_crossing_bracket_not_exact_analog_timing\"";
 s<<",\n  \"protection_events\": [";
 bool first_event=true;
 for(const auto& e:r.protection_events) {
  if(!first_event)s<<',';
  first_event=false;
  s<<"{\"kind\":\""<<trip_event_name(e.kind)<<"\",\"time_s\":"<<e.time
   <<",\"current_peak_A\":"<<e.current_peak<<",\"current_valid\":"<<(e.current_valid?"true":"false")
   <<",\"blanked\":"<<(e.blanked?"true":"false")<<",\"episode\":"<<e.episode;
  s<<",\"crossing_lower_s\":";if(e.crossing_lower<0)s<<"null";else s<<e.crossing_lower;
  s<<",\"crossing_upper_s\":";if(e.crossing_upper<0)s<<"null";else s<<e.crossing_upper;
  s<<'}';
 }
 s<<']';
 s<<",\n  \"hardware_validation\": \"NOT_RUN\",\n  \"parameter_provenance\": \"synthetic_fixture_or_user_profile_not_hardware_identified\"\n}\n";
 return s.str();
}
}
