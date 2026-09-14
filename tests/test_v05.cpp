#include "qdd/bench.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace qdd; using namespace qdd::sim;
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
int main(int argc,char** argv) {
 std::map<std::string,std::function<void()>> tests;
 tests["filter_linear_ramp_analytic"]=[]{
  SensorConfig c;c.current_bandwidth=1000;Sensors s(c);const double dt=1e-4;
  s.analog_step_linear({}, {1,-.5,-.5}, dt);
  const double z=2*3.141592653589793*c.current_bandwidth*dt;
  const double expected=1+std::expm1(-z)/z;
  CHECK(std::abs(s.filtered_current().a-expected)<1e-12);
 };
 tests["filter_linear_ramp_partition_invariant"]=[]{
  Sensors a,b;const double T=50e-6;a.analog_step_linear({}, {2,-1,-1},T);
  for(int k=0;k<10;k++)b.analog_step_linear({.2*k,-.1*k,-.1*k},{.2*(k+1),-.1*(k+1),-.1*(k+1)},T/10);
  CHECK(std::abs(a.filtered_current().a-b.filtered_current().a)<1e-12);
 };
 tests["filter_linear_constant_matches_zoh"]=[]{
  Sensors a,b;for(int n=0;n<100;n++){a.analog_step({2,-1,-1},1e-6);b.analog_step_linear({2,-1,-1},{2,-1,-1},1e-6);}
  CHECK(std::abs(a.filtered_current().a-b.filtered_current().a)<1e-12);
 };
 tests["filter_linear_small_step_finite"]=[]{
  Sensors s;s.analog_step_linear({}, {1,-.5,-.5},1e-16);
  CHECK(std::isfinite(s.filtered_current().a));CHECK(s.filtered_current().a>0);
 };
 tests["filter_linear_rejects_invalid_input"]=[]{
  Sensors s;bool ok=false;try{s.analog_step_linear({}, {},-1);}catch(const std::invalid_argument&){ok=true;}CHECK(ok);
 };
 tests["linear_filter_averaged_onset_refinement"]=[]{
  BenchConfig a;a.scenario="locked-current";a.duration=.08;a.sensor.current_delay_cycles=8;a.sensor.linear_filter=true;a.protection.enabled=true;a.log_csv=false;
  auto b=a;b.step=2.5e-6;const auto x=run_bench(a),y=run_bench(b);
  auto off=[](const BenchResult& r){for(auto e:r.protection_events)if(e.kind==TripEventKind::GatesOff)return e.time;throw std::runtime_error("missing gate event");};
  CHECK(std::abs(off(x)-off(y))<=50e-6);CHECK(std::abs(x.max_current-y.max_current)<=.75);
 };
 tests["linear_filter_switched_onset_refinement"]=[]{
  BenchConfig a;a.scenario="locked-current";a.duration=.08;a.sensor.current_delay_cycles=8;a.sensor.linear_filter=true;a.protection.enabled=true;a.log_csv=false;a.inverter.fidelity=Fidelity::Switched;a.step=1e-6;
  auto b=a;b.step=.5e-6;const auto x=run_bench(a),y=run_bench(b);
  auto off=[](const BenchResult& r){for(auto e:r.protection_events)if(e.kind==TripEventKind::GatesOff)return e.time;throw std::runtime_error("missing gate event");};
  CHECK(std::abs(off(x)-off(y))<=50e-6);CHECK(std::abs(x.max_current-y.max_current)<=.75);
 };
 tests["native_contact_fixture_bounded_penetration"]=[]{
  BenchConfig c;c.scenario="contact";c.duration=1.4;c.sensor.linear_filter=true;auto r=run_bench(c);
  CHECK(r.fault==Fault::None);CHECK(r.final_position>.60);CHECK(r.final_position<.61);
  CHECK(r.final_gear_torque>5);for(auto row:r.rows)CHECK(row.pos<.62);
 };
 if(argc!=2||!tests.count(argv[1]))return 2;
 try{tests.at(argv[1])();std::cout<<argv[1]<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<argv[1]<<": "<<e.what()<<'\n';return 1;}
}
