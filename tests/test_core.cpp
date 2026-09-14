#include "qdd/bench.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
using namespace qdd;
using namespace qdd::sim;
#define CHECK(x) do {if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" check failed: " #x);}while(false)
void near(double a,double b,double tol) {if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tol) throw std::runtime_error("near: "+std::to_string(a)+" vs "+std::to_string(b)+" tol "+std::to_string(tol));}
void arm(Drive& d,Measurement m={}) {
 Command c;c.calibrate=true;d.tick(m,c);c.calibrate=false;
 for(int n=0;n<70;n++) d.tick(m,c);
 CHECK(d.state()==State::Ready);c.arm=true;c.enable=true;
 CHECK(d.tick(m,c).gate_enable);
}
int main(int argc,char** argv) {
 std::map<std::string,std::function<void()>> tests;
 tests["transforms"]=[] {for(int n=-100;n<100;n++){float th=float(n)*0.073f;DQ x{4.3f,-2.1f};auto y=park(clarke(inverse_clarke(inverse_park(x,th))),th);near(x.d,y.d,2e-6);near(x.q,y.q,2e-6);}};
 tests["power_identity"]=[] {DQ i{3,-4},v{7,5};for(int n=0;n<100;n++){float t=float(n)*0.12f;near(dq_power(v,i),phase_power(inverse_clarke(inverse_park(v,t)),inverse_clarke(inverse_park(i,t))),2e-5);}};
 tests["torque_constant"]=[] {MotorElectrical m;near(m.torque_per_iq(),1.5*7*double(m.flux),1e-8);near(m.torque_per_iq(-2),1.5*7*(double(m.flux)+(double(m.ld)-m.lq)*(-2)),1e-8);};
 tests["modulation_sweep"]=[] {for(int n=0;n<720;n++){float t=float(n)*pi/360;DQ v{48/sqrt3*0.96f,0};auto u=svpwm(v,t,48);CHECK(u.valid);CHECK(u.duty.a>=0.0199f&&u.duty.a<=0.9801f);ABC poles{u.duty.a*48,u.duty.b*48,u.duty.c*48};auto actual=park(clarke(poles),t);near(actual.d,v.d,1e-5);near(actual.q,v.q,1e-5);}};
 tests["invalid_modulation"]=[] {CHECK(!svpwm({1,2},0,0).valid);CHECK(!svpwm({1,2},0,-1).valid);CHECK(!svpwm({NAN,2},0,48).valid);CHECK(!svpwm({1,2},INFINITY,48).valid);};
 tests["vector_saturation"]=[] {auto u=svpwm({100,100},0,48);CHECK(u.valid);near(magnitude(u.voltage),48/sqrt3*0.96,1e-5);near(u.voltage.d,u.voltage.q,1e-6);};
 tests["configuration_rejection"]=[] {DriveConfig c;c.motor.ld=0;Drive d(c);CHECK(d.fault()==Fault::Configuration);c={};c.dt=NAN;CHECK(!c.valid());c={};c.gear_ratio=0;CHECK(!c.valid());};
 tests["calibration_and_arm"]=[] {Drive d;Measurement m;m.currents={0.1f,-0.07f,0.03f};Command c;c.arm=true;c.enable=true;CHECK(!d.tick(m,c).gate_enable);arm(d,m);near(d.offsets().a,0.1,1e-6);near(d.offsets().b,-0.07,1e-6);};
 tests["current_circle"]=[] {Drive d;arm(d);Command c;c.enable=true;c.current={30,40};auto u=d.tick({},c);near(magnitude(u.reference),30,1e-5);};
 tests["overcurrent_latch"]=[] {Drive d;arm(d);Command c;c.enable=true;Measurement m;m.currents.a=46;CHECK(d.tick(m,c).fault==Fault::OverCurrent);m={};CHECK(!d.tick(m,c).gate_enable);c.acknowledge_fault=true;CHECK(d.tick(m,c).state==State::Fault);c.enable=false;CHECK(d.tick(m,c).state==State::Disabled);c.acknowledge_fault=false;c.enable=true;c.arm=true;CHECK(!d.tick(m,c).gate_enable);};
 tests["undervoltage"]=[] {Drive d;arm(d);Measurement m;m.vbus=8;CHECK(d.tick(m,{}).fault==Fault::UnderVoltage);};
 tests["overvoltage"]=[] {Drive d;arm(d);Measurement m;m.vbus=59;CHECK(d.tick(m,{}).fault==Fault::OverVoltage);};
 tests["gate_fault"]=[] {Drive d;arm(d);Measurement m;m.driver_fault=true;CHECK(d.tick(m,{}).fault==Fault::GateDriver);};
 tests["command_watchdog"]=[] {Drive d;arm(d);Command c;c.enable=true;c.age=0.021f;CHECK(d.tick({},c).fault==Fault::CommandTimeout);};
 tests["sensor_timeout"]=[] {Drive d;arm(d);Measurement m;m.encoder_age=0.002f;CHECK(d.tick(m,{}).fault==Fault::Sensor);};
 tests["nonfinite_input"]=[] {Drive d;arm(d);Measurement m;m.rotor_angle=NAN;CHECK(d.tick(m,{}).fault==Fault::NonFinite);Drive d2;arm(d2);Command c;c.enable=true;c.torque=NAN;CHECK(d2.tick({},c).fault==Fault::NonFinite);};
 tests["thermal_derating"]=[] {Drive d;arm(d);Command c;c.enable=true;c.current.q=30;Measurement m;m.winding_c=97.5f;auto u=d.tick(m,c);near(u.current_limit,15,1e-5);near(u.reference.q,15,1e-5);m.winding_c=116;CHECK(d.tick(m,c).fault==Fault::OverTemperature);};
 tests["mode_switch"]=[] {Drive d;arm(d);Command c;c.enable=true;c.mode=Mode::Torque;c.torque=5;auto u=d.tick({},c);CHECK(u.reference.q>1);c.mode=Mode::Position;c.position=0;c.torque=0;u=d.tick({},c);near(u.reference.q,0,1e-6);};
 tests["antiwindup"]=[] {MotorElectrical m;PiFoc f(m);CurrentFeedback fb{{},0,12};for(int n=0;n<20000;n++){auto v=f.voltage({30,30},fb,50e-6f);auto u=svpwm(v,0,12);f.track(v,u.voltage,50e-6f);}CHECK(magnitude(f.integral())<100);};
 tests["predictive_equation"]=[] {MotorElectrical m;PredictiveCurrent f(m);CurrentFeedback fb{{1,2},100,48};auto v=f.voltage({1,2},fb,50e-6f);near(v.d,m.resistance-100*m.lq*2,1e-6);near(v.q,2*m.resistance+100*(m.ld+m.flux),1e-6);};
 tests["locked_rotor_RL"]=[] {PlantConfig c;c.mechanical.lock_rotor=true;c.thermal.copper_alpha=0;Plant p(c);ABCd v{1,-0.5,-0.5};for(int n=0;n<1000;n++)p.step(v,0,2e-6);double expected=(1-std::exp(-double(c.motor.resistance)*0.002/c.motor.ld))/c.motor.resistance;near(p.state.id,expected,1e-8);near(p.state.iq,0,1e-9);};
 tests["lossless_energy"]=[] {PlantConfig c;c.motor.resistance=0;c.thermal.copper_alpha=0;auto& m=c.mechanical;m.rotor_viscous=m.rotor_coulomb=m.output_viscous=m.output_coulomb=0;m.damping=0;m.backlash=0;Plant p(c);p.state.iq=2;p.state.rotor_speed=1;p.state.output_angle=-0.003;double e=p.energy();for(int n=0;n<20000;n++)p.step({},0,1e-6);near(p.energy(),e,1e-8);};
 tests["gear_deadband"]=[] {Mechanical m;m.backlash=0.02;near(Plant::coupling_torque(m,0.005,0),0,1e-12);near(Plant::coupling_torque(m,0.02,0),m.stiffness*0.01,1e-10);near(Plant::coupling_torque(m,-0.02,0),-m.stiffness*0.01,1e-10);};
 tests["bus_regeneration"]=[] {BusConfig c;c.source_enabled=false;c.brake_enabled=false;DcLink b(c);b.step(-2,0.001);near(b.voltage,49,1e-9);CHECK(b.energy()>0.5*c.capacitance*48*48);};
 tests["brake_clamp"]=[] {DcLink b;bool braked=false;for(int n=0;n<20000;n++){b.step(-8,5e-6);braked=braked||b.brake_power>0;}CHECK(braked);CHECK(b.voltage<54);};
 tests["switched_deadtime"]=[] {InverterConfig c;c.fidelity=Fidelity::Switched;Inverter i(c);auto s=i.leg(0.5,0.25*c.period+c.deadtime/2,2,48,true);CHECK(!s.high&&!s.low);CHECK(s.pole_voltage<0);auto on=i.leg(0.5,0.25*c.period+2*c.deadtime,2,48,true);CHECK(on.low&&!on.high);i.begin_period({0.5,0.5,0.5});near(i.next_event(0),0.25*c.period,1e-15);near(i.next_event(0.25*c.period),0.25*c.period+c.deadtime,1e-15);};
 tests["inverter_power_balance"]=[] {for(auto mode:{Fidelity::Averaged,Fidelity::Switched}){InverterConfig c;c.fidelity=mode;Inverter i(c);i.begin_period({0.2,0.5,0.8});for(int n=0;n<100;n++){auto s=i.evaluate(c.period*double(n)/100,{2,-3,1},48,true);near(s.bus_current*48,phase_power(s.voltage,ABCd{2,-3,1})+s.loss,1e-10);for(auto leg:s.legs)CHECK(!(leg.high&&leg.low));}}};
 tests["sensor_repeatability"]=[] {SensorConfig c;Sensors a(c),b(c);PlantState p;for(int n=0;n<10;n++){a.analog_step({1,-2,1},50e-6);b.analog_step({1,-2,1},50e-6);a.capture(p,48,25,n*50e-6,50e-6);b.capture(p,48,25,n*50e-6,50e-6);auto x=a.read(n*50e-6),y=b.read(n*50e-6);CHECK(x.currents.a==y.currents.a);}};
 tests["sensor_latency"]=[] {SensorConfig c;c.encoder_delay_cycles=2;c.current_noise_std=0;c.offset={};Sensors s(c);PlantState p;for(int n=0;n<6;n++){p.rotor_angle=n*0.01;s.capture(p,48,25,n*50e-6,50e-6);}auto m=s.read(0.0003);near(m.rotor_angle,0.03,0.0004);near(m.encoder_age,0.00015,1e-10);};
 tests["closed_loop_pi"]=[] {BenchConfig c;c.scenario="locked-current";c.duration=0.1;c.log_csv=false;auto r=run_bench(c);CHECK(r.fault==Fault::None);near(r.final_iq,4,0.12);CHECK(r.current_rms_error<0.15);};
 tests["closed_loop_predictive"]=[] {BenchConfig c;c.scenario="locked-current";c.duration=0.1;c.log_csv=false;c.drive.algorithm=Algorithm::Predictive;auto r=run_bench(c);CHECK(r.fault==Fault::None);near(r.final_iq,4,0.15);CHECK(r.current_rms_error<0.2);};
 tests["averaged_switched_agreement"]=[] {BenchConfig c;c.scenario="locked-current";c.duration=0.03;c.log_csv=false;c.inverter.deadtime=0;c.sensor.current_noise_std=0;c.sensor.offset={};c.step=1e-6;auto a=run_bench(c);c.inverter.fidelity=Fidelity::Switched;auto b=run_bench(c);CHECK(a.fault==Fault::None&&b.fault==Fault::None);near(a.final_iq,b.final_iq,0.35);};
 tests["step_convergence"]=[] {BenchConfig c;c.scenario="impedance";c.duration=0.08;c.log_csv=false;c.sensor.current_noise_std=0;auto a=run_bench(c);c.step=2.5e-6;auto b=run_bench(c);near(a.final_position,b.final_position,1e-4);near(a.final_speed,b.final_speed,0.005);};
 tests["impedance_response"]=[] {BenchConfig c;c.scenario="impedance";c.duration=1.2;c.log_csv=false;auto r=run_bench(c);CHECK(r.fault==Fault::None);near(r.final_position,0.2-2.0/30,0.01);CHECK(std::abs(r.final_speed)<0.1);};
 tests["injected_gate_fault"]=[] {BenchConfig c;c.scenario="gate-fault";c.duration=0.12;c.log_csv=false;auto r=run_bench(c);CHECK(r.fault==Fault::GateDriver);CHECK(r.state==State::Fault);};
 tests["gate_off_current_extinction"]=[] {PlantConfig c;c.mechanical.lock_rotor=true;c.mechanical.lock_output=true;Plant p(c);p.state.iq=4;Inverter i;DcLink b;double initial=p.energy();for(int n=0;n<2000;n++){(void)advance_bridge(p,i,b,0,0,1e-6,false);}CHECK(std::hypot(p.state.id,p.state.iq)<0.02);CHECK(p.energy()<initial);};
 tests["unpowered_coast"]=[] {PlantConfig c;c.mechanical.stiffness=0;c.mechanical.damping=0;Plant p(c);p.state.rotor_speed=40;Inverter i;DcLink b;for(int n=0;n<1000;n++)(void)advance_bridge(p,i,b,0,0,5e-6,false);near(p.state.id,0,1e-10);near(p.state.iq,0,1e-10);};
 tests["unpowered_rectification"]=[] {PlantConfig c;c.mechanical.stiffness=0;c.mechanical.damping=0;Plant p(c);p.state.rotor_speed=1200;Inverter i;BusConfig bc;bc.brake_enabled=false;DcLink b(bc);for(int n=0;n<100;n++)(void)advance_bridge(p,i,b,0,0,1e-6,false);CHECK(b.voltage>48.01);CHECK(p.state.rotor_speed<1200);CHECK(p.state.iq<0);};
 tests["adc_clipping_invalid"]=[] {Sensors s;PlantState p;s.analog_step({100,-100,0},0.01);s.capture(p,48,25,0.01,50e-6);CHECK(!s.read(0.01).valid);};
 tests["external_algorithm_guard"]=[] {struct Bad final:CurrentAlgorithm {DQ voltage(DQ,const CurrentFeedback&,float) noexcept override{return {NAN,0};}void track(DQ,DQ,float) noexcept override{}void reset() noexcept override{}} bad;Drive d;CHECK(d.use_algorithm(bad));Command c;c.calibrate=true;d.tick({},c);c.calibrate=false;for(int n=0;n<70;n++)d.tick({},c);c.arm=true;c.enable=true;auto r=d.tick({},c);CHECK(!r.gate_enable);CHECK(r.fault==Fault::NonFinite);};
 tests["algorithm_swap_inhibit"]=[] {Drive d;arm(d);PiFoc other;CHECK(!d.use_algorithm(other));};
 tests["coupled_energy_convergence"]=[] {auto error=[](double h){PlantConfig c;c.motor.resistance=0;c.thermal.copper_alpha=0;c.mechanical.lock_rotor=true;c.mechanical.lock_output=true;Plant p(c);InverterConfig ic;ic.rds_on=0;ic.deadtime=0;ic.diode_drop=0;Inverter inv(ic);inv.begin_period({0.52f,0.49f,0.49f});BusConfig bc;bc.source_enabled=false;bc.brake_enabled=false;DcLink b(bc);double initial=p.energy()+b.energy();int steps=int(std::round(0.001/h));for(int n=0;n<steps;n++)advance_bridge(p,inv,b,0,0,h,true);return std::abs(p.energy()+b.energy()-initial);};double coarse=error(5e-6),fine=error(2.5e-6);CHECK(fine<0.65*coarse);CHECK(fine<1e-4);};
 tests["switched_step_convergence"]=[] {BenchConfig c;c.scenario="locked-current";c.duration=0.04;c.log_csv=false;c.inverter.fidelity=Fidelity::Switched;c.sensor.current_noise_std=0;c.step=1e-6;auto a=run_bench(c);c.step=0.5e-6;auto b=run_bench(c);CHECK(a.fault==Fault::None&&b.fault==Fault::None);near(a.final_iq,b.final_iq,0.03);};
 tests["invalid_bench_config"]=[] {BenchConfig c;c.step=0;bool rejected=false;try{validate(c);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);};
 if(argc==2){auto it=tests.find(argv[1]);if(it==tests.end()){std::cerr<<"unknown test\n";return 2;}try{it->second();std::cout<<"PASS "<<it->first<<'\n';return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<it->first<<": "<<e.what()<<'\n';return 1;}}
 int failures=0;for(const auto& t:tests){try{t.second();std::cout<<"PASS "<<t.first<<'\n';}catch(const std::exception& e){failures++;std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n';}}return failures?1:0;
}
