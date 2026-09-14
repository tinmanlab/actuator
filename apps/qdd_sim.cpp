#include "qdd/bench.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace qdd;
using namespace qdd::sim;
namespace {
void help(){std::cout<<R"(QDD virtual bench v0.5 (desktop SIL; no motor/board I/O)
Usage: qdd_sim [options]
  --case impedance|locked-current|current-sine|torque-step|velocity|position|regeneration|chirp
         |gate-fault|watchdog|sensor-fault|overvoltage|thermal|contact|disturbance
  --model averaged|switched       Default: averaged
  --algorithm pi|predictive       Default: pi
  --duration SECONDS              Default: 1; maximum 60
  --step-us MICROSECONDS          Default: 5 averaged / 1 switched
  --pwm-hz HZ                    Default: 20000
  --profile profiles/example_qdd.ini
  --output DIRECTORY             Default: results
  --log-every N                  Default: 20; 1 records every control tick
  --frequency-hz HZ              current-sine: 4 A bias + 1 A sine, starts at 20 ms
  --fast-trip                    Enable synthetic independent comparator/BREAK
  --protection-trace             Log every plant endpoint and protection state
  --diagnostic-trace             Log aligned sensor/PI/PWM boundaries
  --no-csv                       Benchmark compute without trace file I/O
  --help
All motor values are an uncalibrated example. Predictive is relaxed one-step,
not constrained MPC. Fault cases exit 0 only when the expected fault is observed.
)";}
double number(const std::string& s){std::size_t n=0;double x=std::stod(s,&n);if(n!=s.size()||!std::isfinite(x))throw std::invalid_argument("invalid number: "+s);return x;}
}
int main(int argc,char** argv) {
 try {
  BenchConfig c;std::string folder="results";bool explicit_step=false;
  for(int n=1;n<argc;n++) {
   std::string arg=argv[n];
   if(arg=="--help"){help();return 0;}
   if(arg=="--fast-trip"){c.protection.enabled=true;continue;}
   if(arg=="--protection-trace"){c.log_protection=true;continue;}
   if(arg=="--diagnostic-trace"){c.log_diagnostic=true;continue;}
   if(arg=="--no-csv"){c.log_csv=false;continue;}
   if(n+1>=argc)throw std::invalid_argument("missing value for "+arg);
   std::string value=argv[++n];
   if(arg=="--case")c.scenario=value;
   else if(arg=="--model") {
    if(value=="averaged")c.inverter.fidelity=Fidelity::Averaged;
    else if(value=="switched")c.inverter.fidelity=Fidelity::Switched;
    else throw std::invalid_argument("model must be averaged or switched");
   }else if(arg=="--algorithm") {
    if(value=="pi")c.drive.algorithm=Algorithm::Pi;
    else if(value=="predictive")c.drive.algorithm=Algorithm::Predictive;
    else throw std::invalid_argument("algorithm must be pi or predictive");
   }else if(arg=="--duration")c.duration=number(value);
   else if(arg=="--step-us"){c.step=number(value)*1e-6;explicit_step=true;}
   else if(arg=="--pwm-hz"){double hz=number(value);if(hz<1000||hz>100000)throw std::invalid_argument("PWM rate outside bench range");c.inverter.period=1/hz;c.drive.dt=float(1/hz);}
   else if(arg=="--log-every"){double n=number(value);if(n!=std::floor(n)||n<1||n>1000000)throw std::invalid_argument("log-every must be a positive integer <= 1000000");c.trace_divider=int(n);}
   else if(arg=="--frequency-hz")c.excitation_frequency=number(value);
   else if(arg=="--profile")load_profile(c,value);
   else if(arg=="--output")folder=value;
   else throw std::invalid_argument("unknown argument: "+arg);
  }
  if(!explicit_step&&c.inverter.fidelity==Fidelity::Switched)c.step=1e-6;
  validate(c);
  std::filesystem::create_directories(folder);
  const std::string stem=c.scenario+"_"+(c.inverter.fidelity==Fidelity::Averaged?"averaged":"switched")+"_"+(c.drive.algorithm==Algorithm::Pi?"pi":"predictive");
  const auto prefix=std::filesystem::path(folder)/stem;
  // A directory may contain profiles/other cases, but existing case evidence is immutable.
  for(const char* suffix:{".json",".csv","_pwm.csv","_protection.csv","_diagnostic.csv"})
   if(std::filesystem::exists(prefix.string()+suffix))throw std::runtime_error("refusing to overwrite existing case evidence: "+prefix.string());
  c.diagnostic_path=prefix.string()+"_diagnostic.csv";
  c.csv_path=prefix.string()+".csv";c.waveform_path=prefix.string()+"_pwm.csv";c.protection_path=prefix.string()+"_protection.csv";
  auto r=run_bench(c);const auto json=result_json(c,r);
  std::ofstream report(prefix.string()+".json");if(!report)throw std::runtime_error("cannot write result JSON");report<<json;
  std::cout<<json;
  Fault expected=Fault::None;
  if(c.scenario=="gate-fault")expected=Fault::GateDriver;
  if(c.scenario=="watchdog")expected=Fault::CommandTimeout;
  if(c.scenario=="sensor-fault")expected=Fault::Sensor;
  if(c.scenario=="overvoltage")expected=Fault::OverVoltage;
  if(r.fault!=expected){std::cerr<<"Expected "<<fault_name(expected)<<", observed "<<fault_name(r.fault)<<'\n';return 3;}
  return 0;
 }catch(const std::exception& e){std::cerr<<"qdd_sim: "<<e.what()<<'\n';return 2;}
}
