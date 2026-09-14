#include "qdd/bench.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace qdd;
using namespace qdd::sim;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(std::string("check failed: ")+#x); } while(false)
struct Profile {
 std::filesystem::path path;
 Profile(const std::string& id,const std::string& text):path("v04_profile_"+id+".ini") {std::ofstream f(path);f<<text;}
 ~Profile(){std::error_code ec;std::filesystem::remove(path,ec);}
};
// Feed analytic held inputs at every scheduled event, without a control loop.
void until(FastTrip& p,double target,Phase<double> current) {
 double next=p.next_event(0);
 // next_event is queried using the latest event time; the helper's held-input
 // fixtures never rewind observe(). Due times at/before target are consumed.
 while(std::isfinite(next)&&next<=target+1e-14) {
  p.observe(next,current);const double old=next;next=p.next_event(old);
 }
 p.observe(target,current);
}
const TripEvent& event(const FastTrip& p,TripEventKind kind) {
 for(const auto& e:p.events())if(e.kind==kind)return e;
 throw std::runtime_error("missing protection event");
}
int main(int argc,char** argv) {
 std::map<std::string,std::function<void()>> tests;
 tests["fast_trip_profile_is_supported"]=[]{
  BenchConfig c;Profile p("valid","protection.enabled=1\nprotection.threshold_A=45\nprotection.hysteresis_A=2\nprotection.propagation_ns=200\nprotection.break_delay_ns=100\nprotection.gate_off_ns=200\nprotection.blanking_us=0\nprotection.reset_hold_us=10\n");
  load_profile(c,p.path.string());CHECK(config_json(c).find("\"fast_trip\"")!=std::string::npos);
 };
 tests["fast_trip_disabled_preserves_baseline"]=[]{
  BenchConfig a;a.scenario="locked-current";a.duration=0.025;a.trace_divider=1;BenchConfig b=a;
  Profile p("off","protection.enabled=0\n");load_profile(b,p.path.string());
  const auto x=run_bench(a),y=run_bench(b);CHECK(x.rows.size()==y.rows.size());CHECK(x.plant_steps==y.plant_steps);
  for(std::size_t n=0;n<x.rows.size();++n){CHECK(x.rows[n].iq==y.rows[n].iq);CHECK(x.rows[n].vbus==y.rows[n].vbus);}
 };
 tests["fast_trip_beats_delayed_software_ocp"]=[]{
  BenchConfig a;a.scenario="locked-current";a.duration=0.08;a.sensor.current_delay_cycles=8;a.log_csv=false;
  BenchConfig b=a;Profile p("fast","protection.enabled=1\n");load_profile(b,p.path.string());
  const auto sw=run_bench(a),hw=run_bench(b);
  CHECK(sw.fault==Fault::OverCurrent);CHECK(hw.fault==Fault::GateDriver);
  CHECK(sw.max_current>50);CHECK(hw.max_current<48);CHECK(hw.max_current<sw.max_current);
  CHECK(std::abs(hw.final_iq)<1e-6);
 };
 tests["fast_trip_report_has_distinct_event_times"]=[]{
  BenchConfig c;c.scenario="locked-current";c.duration=0.08;c.sensor.current_delay_cycles=8;c.log_csv=false;
  Profile p("times","protection.enabled=1\n");load_profile(c,p.path.string());
  const auto report=result_json(c,run_bench(c));
  for(auto key:{"current_crossing","comparator_assert","break_latch","pwm_inhibit","gates_off","supervisor_observed","crossing_lower_s","crossing_upper_s"}) CHECK(report.find(key)!=std::string::npos);
 };
 tests["fast_trip_invalid_profile_is_atomic"]=[]{
  BenchConfig c;const auto before=config_json(c);Profile p("invalid","motor.resistance_ohm=0.22\nprotection.enabled=1\nprotection.hysteresis_A=50\n");
  bool rejected=false;try{load_profile(c,p.path.string());}catch(const std::invalid_argument&){rejected=true;}
  CHECK(rejected);CHECK(config_json(c)==before);
 };
 tests["fast_trip_masks_output_not_comparator"]=[]{
  FastTripConfig c;c.enabled=true;c.blanking=10e-6;FastTrip p(c);
  p.observe(0,{});p.observe(1e-6,{46,-23,-23});until(p,2e-6,{46,-23,-23});
  CHECK(p.comparator_asserted());CHECK(!p.latched());CHECK(p.blanked(2e-6));
  until(p,10.5e-6,{46,-23,-23});CHECK(p.latched());CHECK(p.gates_inhibited());
  CHECK(std::abs(event(p,TripEventKind::BreakLatch).time-10.1e-6)<1e-12);
 };
 tests["fast_trip_blanked_pulse_can_hide_real_overcurrent"]=[]{
  FastTripConfig c;c.enabled=true;c.blanking=10e-6;FastTrip p(c);
  p.observe(0,{});p.observe(1e-6,{60,-30,-30});until(p,2e-6,{60,-30,-30});
  p.observe(3e-6,{});until(p,12e-6,{});
  CHECK(!p.latched());CHECK(!p.comparator_asserted());CHECK(!p.events().empty());
  CHECK(event(p,TripEventKind::CurrentCrossing).current_peak==60);
 };
 tests["fast_trip_unblanked_equivalent_pulse_trips"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});
  p.observe(1e-6,{60,-30,-30});until(p,2e-6,{60,-30,-30});CHECK(p.latched());
 };
 tests["fast_trip_threshold_all_phases_both_polarities"]=[]{
  for(auto i:{Phase<double>{46,-23,-23},Phase<double>{-46,23,23},Phase<double>{23,-46,23},
              Phase<double>{-23,46,-23},Phase<double>{23,23,-46},Phase<double>{-23,-23,46}}) {
   FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});p.observe(1e-6,i);until(p,2e-6,i);CHECK(p.latched());
  }
 };
 tests["fast_trip_no_false_trip_below_threshold"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);
  for(int n=0;n<1000;++n){double v=44+0.4*std::sin(double(n));p.observe(n*1e-6,{v,-v/2,-v/2});}
  CHECK(!p.latched());CHECK(p.events().empty());
 };
 tests["fast_trip_hysteresis_falling_only"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});
  p.observe(1e-6,{45,-22.5,-22.5});until(p,2e-6,{45,-22.5,-22.5});
  p.observe(3e-6,{44,-22,-22});CHECK(p.raw_asserted());
  p.observe(4e-6,{43,-21.5,-21.5});until(p,5e-6,{43,-21.5,-21.5});
  CHECK(!p.raw_asserted());CHECK(!p.comparator_asserted());CHECK(p.latched());
 };
 tests["fast_trip_subpropagation_pulse_is_inertially_rejected"]=[]{
  FastTripConfig c;c.enabled=true;c.propagation=1e-6;FastTrip p(c);p.observe(0,{});
  p.observe(1e-6,{46,-23,-23});p.observe(1.5e-6,{});until(p,4e-6,{});CHECK(!p.latched());
 };
 tests["fast_trip_scheduled_latencies_are_separate"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});p.observe(1e-6,{46,-23,-23});
  until(p,1.2e-6,{46,-23,-23});CHECK(p.comparator_asserted());CHECK(!p.latched());
  until(p,1.3e-6,{46,-23,-23});CHECK(p.pwm_inhibited());CHECK(!p.gates_inhibited());
  until(p,1.5e-6,{46,-23,-23});CHECK(p.gates_inhibited());
  p.observe(10e-6,{46,-23,-23});p.supervisor_observed(10e-6);
  CHECK(event(p,TripEventKind::SupervisorObserved).time>event(p,TripEventKind::GatesOff).time);
  CHECK(event(p,TripEventKind::CurrentCrossing).crossing_lower==0);
  CHECK(event(p,TripEventKind::CurrentCrossing).crossing_upper==1e-6);
 };
 tests["fast_trip_no_cpu_tick_is_needed"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});p.observe(3e-6,{50,-25,-25});
  until(p,4e-6,{50,-25,-25});CHECK(p.gates_inhibited());
  for(auto e:p.events())CHECK(e.kind!=TripEventKind::SupervisorObserved);
 };
 tests["fast_trip_break_pulse_is_not_cancelled_after_mask"]=[]{
  FastTripConfig c;c.enabled=true;c.blanking_start=1.25e-6;c.blanking=2e-6;
  FastTrip p(c);p.observe(0,{});p.observe(1e-6,{46,-23,-23});until(p,2e-6,{46,-23,-23});
  CHECK(p.latched());CHECK(event(p,TripEventKind::BreakLatch).blanked);
 };
 tests["fast_trip_zero_delay_and_period_boundary"]=[]{
  FastTripConfig c;c.enabled=true;c.propagation=c.break_delay=c.gate_off_delay=0;
  FastTrip p(c);p.observe(0,{});p.observe(50e-6,{46,-23,-23});CHECK(p.gates_inhibited());
  CHECK(event(p,TripEventKind::GatesOff).time==50e-6);
 };
 tests["fast_trip_invalid_current_fails_closed_through_blanking"]=[]{
  FastTripConfig c;c.enabled=true;c.blanking=10e-6;FastTrip p(c);
  p.observe(0,{std::numeric_limits<double>::quiet_NaN(),0,0});CHECK(p.gates_inhibited());CHECK(p.latched());
  CHECK(!event(p,TripEventKind::InvalidInput).current_valid);CHECK(!p.gate_allowed(true));
 };
 tests["fast_trip_repeated_invalid_current_does_not_duplicate_gate_event"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);const auto nan=std::numeric_limits<double>::quiet_NaN();
  p.observe(0,{nan,0,0});p.supervisor_observed(0);p.observe(1e-6,{nan,0,0});
  unsigned count=0;for(auto e:p.events())if(e.kind==TripEventKind::GatesOff)++count;CHECK(count==1);
 };
 tests["fast_trip_acknowledgement_does_not_clear_hardware"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});p.observe(1e-6,{46,-23,-23});until(p,2e-6,{46,-23,-23});
  Drive d;Measurement m;m.driver_fault=p.latched();Command cmd;cmd.acknowledge_fault=true;
  CHECK(d.tick(m,cmd).fault==Fault::GateDriver);CHECK(p.latched());CHECK(!p.gate_allowed(true));
 };
 tests["fast_trip_reset_needs_safe_dwell_and_disabled_request"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});p.observe(1e-6,{46,-23,-23});until(p,2e-6,{46,-23,-23});
  CHECK(!p.clear_latch(2e-6,{46,-23,-23},true,true));p.observe(3e-6,{});until(p,4e-6,{});
  CHECK(!p.clear_latch(5e-6,{},true,true));until(p,20e-6,{});
  CHECK(!p.clear_latch(20e-6,{},false,true));CHECK(!p.clear_latch(20e-6,{},true,false));
  CHECK(p.clear_latch(20e-6,{},true,true));CHECK(!p.latched());CHECK(p.gates_inhibited());
 };
 tests["fast_trip_clear_does_not_reenable_held_arm"]=[]{
  FastTripConfig c;c.enabled=true;c.reset_hold=0;FastTrip p(c);p.observe(0,{});p.observe(1e-6,{46,-23,-23});until(p,2e-6,{46,-23,-23});
  CHECK(!p.rearm(2e-6,true,State::Ready));p.observe(3e-6,{});until(p,4e-6,{});
  CHECK(p.clear_latch(4e-6,{},true,true));p.observe(5e-6,{});
  CHECK(!p.rearm(5e-6,true,State::Ready));CHECK(p.gates_inhibited());
  CHECK(!p.rearm(5e-6,false,State::Ready));p.observe(6e-6,{});CHECK(p.rearm(6e-6,true,State::Ready));
  CHECK(p.gate_allowed(true));CHECK(!p.gate_allowed(false));
 };
 tests["fast_trip_rearm_requires_ready_and_fresh_observation"]=[]{
  FastTripConfig c;c.enabled=true;c.reset_hold=0;FastTrip p(c);p.observe(0,{46,-23,-23});until(p,1e-6,{46,-23,-23});
  p.observe(2e-6,{});until(p,3e-6,{});CHECK(p.clear_latch(3e-6,{},true,true));p.observe(4e-6,{});
  CHECK(!p.rearm(4e-6,false,State::Ready));CHECK(!p.rearm(5e-6,true,State::Ready));
  CHECK(!p.rearm(4e-6,true,State::Fault));CHECK(!p.rearm(4e-6,true,State::Armed));
 };
 tests["fast_trip_retrips_after_explicit_rearm"]=[]{
  FastTripConfig c;c.enabled=true;c.reset_hold=0;FastTrip p(c);p.observe(0,{46,-23,-23});until(p,1e-6,{46,-23,-23});
  p.observe(2e-6,{});until(p,3e-6,{});CHECK(p.clear_latch(3e-6,{},true,true));
  p.observe(4e-6,{});CHECK(!p.rearm(4e-6,false,State::Ready));p.observe(5e-6,{});CHECK(p.rearm(5e-6,true,State::Ready));
  p.observe(6e-6,{46,-23,-23});until(p,7e-6,{46,-23,-23});CHECK(p.gates_inhibited());
  unsigned n=0;for(auto e:p.events())if(e.kind==TripEventKind::BreakLatch)++n;CHECK(n==2);
 };
 tests["fast_trip_rejects_skipped_scheduled_deadline"]=[]{
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});p.observe(1e-6,{46,-23,-23});
  bool rejected=false;try{p.observe(10e-6,{46,-23,-23});}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
 };
 tests["fast_trip_rejects_invalid_parameters_and_clock"]=[]{
  for(int n=0;n<5;++n){FastTripConfig c;c.enabled=true;
   if(n==0)c.threshold=0;
   if(n==1)c.hysteresis=45;
   if(n==2)c.blanking=50e-6;
   if(n==3)c.propagation=-1;
   if(n==4)c.break_delay=std::numeric_limits<double>::infinity();
   bool rejected=false;try{FastTrip p(c);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
  }
  FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(1e-6,{});
  bool rejected=false;try{p.observe(0,{});}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
 };
 tests["fast_trip_no_nominal_regression_when_enabled"]=[]{
  BenchConfig c;c.protection.enabled=true;c.scenario="locked-current";c.duration=0.08;c.log_csv=false;
  auto r=run_bench(c);CHECK(r.protection_events.empty());CHECK(r.fault==Fault::None);CHECK(std::abs(r.final_iq-4)<0.05);
 };
 tests["fast_trip_switched_bridge_and_diode_extinction"]=[]{
  BenchConfig c;c.protection.enabled=true;c.scenario="locked-current";c.duration=0.08;c.sensor.current_delay_cycles=8;
  c.inverter.fidelity=Fidelity::Switched;c.step=1e-6;c.trace_divider=1;
  auto r=run_bench(c);CHECK(r.fault==Fault::GateDriver);CHECK(std::abs(r.final_iq)<1e-6);
  bool off_with_current=false;for(auto row:r.rows)if(!row.gate_enabled&&row.break_latched&&std::abs(row.iq)>1)off_with_current=true;
  CHECK(off_with_current);CHECK(r.max_current<48);
 };

 tests["fast_trip_zero_hysteresis_holds_at_exact_threshold"]=[]{
  FastTripConfig c;c.enabled=true;c.hysteresis=0;c.propagation=1e-6;FastTrip p(c);
  p.observe(0,{45,-22.5,-22.5});
  for(int n=1;n<=12;++n)until(p,n*0.1e-6,{45,-22.5,-22.5});
  CHECK(p.raw_asserted());CHECK(p.latched());
 };
 tests["fast_trip_cpu_cannot_bypass_declared_gate_delay"]=[]{
  FastTripConfig c;c.enabled=true;c.propagation=0;c.break_delay=0;c.gate_off_delay=1e-6;
  FastTrip p(c);p.observe(0,{});CHECK(p.gate_allowed(true));
  p.observe(1e-6,{46,-23,-23});CHECK(p.latched());CHECK(!p.gates_inhibited());
  CHECK(p.gate_allowed(false)); // software learns the fault, but driver propagation remains
  until(p,2e-6,{46,-23,-23});CHECK(!p.gate_allowed(true));
 };
 tests["fast_trip_analytic_ramp_crossing_bracket_refines"]=[]{
  double last_error=1;const double exact_cross=4.5e-6;
  for(double h:{2e-6,1e-6,0.5e-6}) {
   FastTripConfig c;c.enabled=true;FastTrip p(c);p.observe(0,{});double t=0;
   while(t<10e-6-1e-14) {
    t=std::min({t+h,p.next_event(t),10e-6});double i=1e7*t;p.observe(t,{i,-i/2,-i/2});
   }
   const auto& cross=event(p,TripEventKind::CurrentCrossing);const auto& off=event(p,TripEventKind::GatesOff);
   CHECK(cross.crossing_lower<=exact_cross+1e-14);CHECK(cross.crossing_upper>=exact_cross-1e-14);
   CHECK(cross.crossing_upper-cross.crossing_lower<=h+1e-14);
   const double error=std::abs(off.time-(exact_cross+0.5e-6));CHECK(error<=last_error+1e-14);last_error=error;
   std::cout<<"h_s="<<h<<" crossing=["<<cross.crossing_lower<<','<<cross.crossing_upper<<"] gate_off_s="<<off.time<<'\n';
  }
 };
 tests["fast_trip_recovery_requires_drive_recalibration"]=[]{
  FastTripConfig c;c.enabled=true;c.reset_hold=0;FastTrip p(c);p.observe(0,{46,-23,-23});until(p,1e-6,{46,-23,-23});
  Drive d;Command cmd;Measurement m;m.driver_fault=true;CHECK(d.tick(m,cmd).fault==Fault::GateDriver);
  p.observe(2e-6,{});until(p,3e-6,{});CHECK(p.clear_latch(3e-6,{},true,true));m.driver_fault=false;
  cmd.acknowledge_fault=true;CHECK(d.tick(m,cmd).state==State::Disabled);cmd.acknowledge_fault=false;
  p.observe(4e-6,{});CHECK(!p.rearm(4e-6,false,d.state()));CHECK(!p.rearm(4e-6,true,d.state()));
  cmd.calibrate=true;d.tick(m,cmd);cmd.calibrate=false;for(int n=0;n<64;++n)d.tick(m,cmd);CHECK(d.state()==State::Ready);
  p.observe(5e-6,{});CHECK(!p.rearm(5e-6,false,d.state()));p.observe(6e-6,{});CHECK(p.rearm(6e-6,true,d.state()));
  cmd.enable=cmd.arm=true;CHECK(p.gate_allowed(d.tick(m,cmd).gate_enable));
 };
 if(argc!=2||!tests.count(argv[1]))return 2;
 try {tests.at(argv[1])();std::cout<<"PASS "<<argv[1]<<'\n';return 0;}
 catch(const std::exception& e){std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<'\n';return 1;}
}
