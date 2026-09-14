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
 Profile(const std::string& id,const std::string& text):path("test_profile_"+id+".ini") {std::ofstream f(path);f<<text;}
 ~Profile(){std::error_code e;std::filesystem::remove(path,e);}
};
bool near(double a,double b){return std::abs(a-b)<1e-7;}
int main(int argc,char** argv){
 std::map<std::string,std::function<void()>> tests;
 tests["profile_preserves_separate_models"]=[]{
  BenchConfig c;c.drive.motor.resistance=0.07f;c.plant.motor.resistance=0.13f;
  Profile p("preserve","sensor.seed=12\n");load_profile(c,p.path.string());
  CHECK(near(c.drive.motor.resistance,0.07));CHECK(near(c.plant.motor.resistance,0.13));
 };
 tests["profile_legacy_pair_compatible"]=[]{
  BenchConfig c;Profile p("legacy","motor.resistance_ohm=0.11\nmechanical.ratio=8\n");
  load_profile(c,p.path.string());CHECK(near(c.drive.motor.resistance,0.11));
  CHECK(near(c.plant.motor.resistance,0.11));CHECK(near(c.drive.gear_ratio,8));
 };
 tests["profile_explicit_order_independent"]=[]{
  BenchConfig a,b;
  Profile p("order1","controller.motor.resistance_ohm=0.07\nmotor.resistance_ohm=0.1\nplant.motor.resistance_ohm=0.14\n");
  Profile q("order2","plant.motor.resistance_ohm=0.14\nmotor.resistance_ohm=0.1\ncontroller.motor.resistance_ohm=0.07\n");
  load_profile(a,p.path.string());load_profile(b,q.path.string());
  CHECK(near(a.drive.motor.resistance,0.07));CHECK(near(a.plant.motor.resistance,0.14));
  CHECK(a.drive.motor.resistance==b.drive.motor.resistance);CHECK(a.plant.motor.resistance==b.plant.motor.resistance);
 };
 tests["profile_duplicate_rejected"]=[]{
  BenchConfig c;Profile p("duplicate","sensor.seed=5\nsensor.seed=6\n");
  bool rejected=false;try{load_profile(c,p.path.string());}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
 };
 tests["profile_failed_load_is_atomic"]=[]{
  BenchConfig c;Profile p("atomic","motor.resistance_ohm=0.22\ninvalid.key=4\n");
  bool rejected=false;try{load_profile(c,p.path.string());}catch(const std::invalid_argument&){rejected=true;}
  CHECK(rejected);CHECK(near(c.plant.motor.resistance,0.08));
 };
 tests["profile_invalid_values_rejected"]=[]{
  BenchConfig c;Profile p("invalid","motor.resistance_ohm=-1\n");
  bool rejected=false;try{load_profile(c,p.path.string());}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
 };

 tests["high_rate_trace_records_each_control_tick"]=[]{
  BenchConfig c;c.scenario="locked-current";c.duration=0.03;c.trace_divider=1;
  auto r=run_bench(c);CHECK(r.rows.size()==r.control_ticks);
 };
 tests["invalid_trace_divider_rejected"]=[]{
  BenchConfig c;c.trace_divider=0;bool rejected=false;
  try{validate(c);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
 };
 tests["current_sine_runs_through_pwm_path"]=[]{
  BenchConfig c;c.scenario="current-sine";c.duration=0.05;c.trace_divider=1;
  auto r=run_bench(c);CHECK(r.fault==Fault::None);
  double a=1e9,b=-1e9;for(const auto& row:r.rows)if(row.time>0.021){a=std::min(a,row.iq_ref);b=std::max(b,row.iq_ref);}
  CHECK(a<3.01&&b>4.99);CHECK(r.max_current>3);
 };
 tests["report_exposes_fault_time_and_config"]=[]{
  BenchConfig c;c.scenario="gate-fault";c.duration=0.1;c.log_csv=false;
  auto report=result_json(c,run_bench(c));
  CHECK(report.find("\"first_fault_s\": 0.08")!=std::string::npos);
  CHECK(report.find("\"fault_latency_s\": 0")!=std::string::npos);
  CHECK(report.find("\"effective_config\"")!=std::string::npos);
  CHECK(report.find("\"source_sha256\"")!=std::string::npos);
  CHECK(report.find("\"plant_motor\"")!=std::string::npos);
  CHECK(report.find("\"controller_motor\"")!=std::string::npos);
 };
 tests["report_exposes_saturation"]=[]{
  BenchConfig c;c.scenario="locked-current";c.duration=0.04;c.log_csv=false;
  c.plant.motor.resistance=8;c.bus.source_voltage=16;c.step=0.5e-6;
  auto result=run_bench(c);auto report=result_json(c,result);
  const std::string key="\"voltage_saturation_fraction\": ";auto at=report.find(key);CHECK(at!=std::string::npos);
  CHECK(std::stod(report.substr(at+key.size()))>0.1);CHECK(result.final_iq<4);
 };

 tests["pi_bandwidth_changes_control_not_plant"]=[]{
  DriveConfig fast,slow;slow.current_bandwidth_hz=300;Drive a(fast),b(slow);
  Command c;c.calibrate=true;a.tick({},c);b.tick({},c);c.calibrate=false;
  for(int n=0;n<70;++n){a.tick({},c);b.tick({},c);}
  c.enable=true;c.arm=true;c.current.q=4;
  const auto x=a.tick({},c),y=b.tick({},c);CHECK(x.gate_enable&&y.gate_enable);
  CHECK(y.voltage.q<0.6f*x.voltage.q);
 };
 tests["invalid_pi_bandwidth_rejected"]=[]{
  DriveConfig c;c.current_bandwidth_hz=0;CHECK(!c.valid());c.current_bandwidth_hz=5000;CHECK(!c.valid());
 };
 tests["profile_pi_bandwidth_supported"]=[]{
  BenchConfig c;Profile p("bandwidth","drive.current_bandwidth_Hz=300\n");load_profile(c,p.path.string());
  CHECK(near(c.drive.current_bandwidth_hz,300));CHECK(near(c.plant.motor.ld,80e-6));
 };
 if(argc!=2||!tests.count(argv[1]))return 2;
 try{tests.at(argv[1])();std::cout<<"PASS "<<argv[1]<<'\n';return 0;}
 catch(const std::exception& e){std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<'\n';return 1;}
}
