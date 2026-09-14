// Reproducible signal experiments using the existing native C++ model.
// No JS/Python motor equations. Output directory must be new or empty.
#include "qdd/bench.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sstream>
using namespace qdd;
using namespace qdd::sim;
namespace fs=std::filesystem;
static std::ofstream file(const fs::path& p) {
 std::ofstream f(p);if(!f)throw std::runtime_error("Cannot write "+p.string());f<<std::setprecision(17);return f;
}
int main(int argc,char** argv) {try {
 if(argc!=2)throw std::invalid_argument("Usage: qdd_signal_lab NEW_OUTPUT_DIRECTORY");
 const fs::path out=argv[1];
 if(fs::exists(out)&&!fs::is_empty(out))throw std::runtime_error("Refusing to overwrite non-empty evidence directory");
 fs::create_directories(out);
 auto mod=file(out/"modulation.csv");
 mod<<"electrical_deg,duty_a,duty_b,duty_c,phase_a_V,phase_b_V,common_V\n";
 for(int degrees=0;degrees<=360;degrees++) {
  const float angle=float(degrees)*pi/180;
  const auto v=inverse_clarke(inverse_park(DQ{20,0},angle));
  const auto m=svpwm({20,0},angle,48,.02f);
  const double common=-.5*(std::max({v.a,v.b,v.c})+std::min({v.a,v.b,v.c}));
  mod<<degrees<<','<<m.duty.a<<','<<m.duty.b<<','<<m.duty.c<<','<<v.a<<','<<v.b<<','<<common<<'\n';
 }
 mod.close();
 BenchConfig base;base.sensor.linear_filter=true;base.trace_divider=1;
 auto run=[&](std::string name,std::string scenario,double duration,Fidelity fidelity) {
  auto c=base;c.scenario=scenario;c.duration=duration;c.inverter.fidelity=fidelity;
  c.step=fidelity==Fidelity::Switched?1e-6:5e-6;
  c.csv_path=(out/(name+".csv")).string();c.power_path=(out/(name+"_power.csv")).string();
  if(fidelity==Fidelity::Switched)c.waveform_path=(out/"pwm_edges.csv").string();
  const auto result=run_bench(c);
  const auto expected=scenario=="gate-fault"?Fault::GateDriver:Fault::None;
  if(result.fault!=expected)throw std::runtime_error("Unexpected fault in "+name);
  auto f=file(out/(name+".json"));f<<result_json(c,result);return result;
 };
 run("current","locked-current",.04,Fidelity::Averaged);
 run("switched","locked-current",.025,Fidelity::Switched);
 run("gate_off","gate-fault",.085,Fidelity::Averaged);
 run("velocity","velocity",1,Fidelity::Averaged);
 base.plant.mechanical.output_viscous=.06;
 run("friction","velocity",1,Fidelity::Averaged);
 base.plant.mechanical.output_viscous=.02;
 run("regeneration","regeneration",.5,Fidelity::Averaged);
 run("derating","thermal",.06,Fidelity::Averaged);
 // Thermal fixture deliberately prescribes current at locked shafts. It is not
 // a drive heating test: bypassing the current limiter is explicitly labeled.
 PlantConfig pc;pc.mechanical.lock_rotor=pc.mechanical.lock_output=true;
 Plant thermal(pc);thermal.state.iq=12;Inverter heat;
 auto th=file(out/"thermal.csv");th<<"time_s,winding_C,case_C,fet_C,resistance_ohm,copper_W\n";
 constexpr double h=.01;constexpr int steps=60000;
 for(int k=0;k<=steps;k++) {
  const auto& s=thermal.state;
  if(k%100==0)th<<k*h<<','<<s.winding_c<<','<<s.case_c<<','<<heat.fet_c<<','<<thermal.resistance()<<','<<1.5*thermal.resistance()*144<<'\n';
  if(k==steps)break;
  // Existing thermal equations, with a fixed dq current endpoint and zero motion.
  thermal.step_electrical_endpoint({0,12},0,h);
  const auto stage=heat.evaluate(0,thermal.currents(),48,true);
  heat.heat(stage.loss,h);
 }
 th.close();
 // The exporter checks stationary energy before reporting a motoring ratio.
 auto avg=[&](const fs::path& p) {
  std::ifstream f(p);std::string line;std::getline(f,line);std::array<double,10> sum{};int n=0;
  while(std::getline(f,line)) {std::istringstream in(line);double x[13]{};std::string v;
   for(int j=0;j<13&&std::getline(in,v,',');j++)x[j]=std::stod(v);
   if(x[0]<.8)continue;
   for(int j=0;j<10;j++)sum[j]+=x[j+1];
   ++n;
  }
  if(!n)throw std::runtime_error("Missing steady window");
  for(auto& v:sum)v/=n;
  return sum;
 };
 const auto mean=avg(out/"velocity_power.csv");
 if(mean[0]<=0||mean[6]<=0||std::abs(mean[7])>.05)throw std::runtime_error("Efficiency window is not motoring/quasi-steady");
 auto meta=file(out/"experiment.json");
 meta<<"{\n\"claim\":\"synthetic native SIL; not hardware calibration\",\n"
 <<"\"thermal_kind\":\"prescribed current; not closed-loop drive\",\n"
 <<"\"motoring\":{\"from_s\":0.8,\"to_s\":1.0,\"dc_W\":"<<mean[0]
 <<",\"load_W\":"<<mean[6]<<",\"stored_energy_rate_W\":"<<mean[7]
 <<",\"residual_W\":"<<mean[8]<<",\"efficiency\":"<<mean[6]/mean[0]<<"},\n"
 <<"\"power_boundary\":\"inverter DC terminals to external output load; no source ESR or auxiliaries\",\n"
 <<"\"omitted_losses\":[\"iron\",\"switching energy\",\"gate charge\",\"reverse recovery\"],\n"
 <<"\"timing\":\"Power is period averaged from same-state bridge samples; currents in PWM CSV are interval endpoints.\"\n}\n";
 std::cout<<"Native signal experiments saved to "<<out<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
