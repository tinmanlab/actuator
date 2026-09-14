#pragma once
#include "qdd/plant.hpp"
#include "qdd/protection.hpp"
#include <string>
#include <vector>
namespace qdd::sim {
struct BenchConfig {
 FastTripConfig protection{};
 DriveConfig drive{};PlantConfig plant{};InverterConfig inverter{};BusConfig bus{};SensorConfig sensor{};
 double duration=1,step=5e-6;
 int trace_divider=20;
 double excitation_frequency=100;
 std::string scenario="impedance",csv_path,waveform_path,protection_path;
 bool log_protection=false;
 bool log_diagnostic=false;
 std::string diagnostic_path;
 std::string power_path; // Optional period-averaged diagnostic; default traces unchanged.
 bool log_csv=true;
};
struct Row {
 double time,pos,vel,id,iq,iq_ref,torque,tau_ref,vbus,winding,fet,ia,ib,ic,da,db,dc;
 int state,fault;
 double boundary_time=0,boundary_iq=0,sensor_iq=0,current_age=0,encoder_age=0;
 double vd=0,vq=0;
 double rotor_angle=0,rotor_speed=0;
 bool requested_gate=false,break_latched=false;
 bool gate_enabled=false,voltage_saturated=false,reference_limited=false;
};
struct BenchResult {
 double duration=0,wall=0,current_rms_error=0,max_current=0,max_bus=0,final_position=0,final_speed=0;
 double final_iq=0,final_iq_ref=0,final_gear_torque=0,final_winding=0,final_fet=0;
 std::uint64_t plant_steps=0,control_ticks=0,armed_ticks=0,saturated_ticks=0,reference_limited_ticks=0;
 double first_fault=-1,injection_time=-1;
 Fault fault=Fault::None;State state=State::Disabled;
 std::vector<Row> rows;
 std::vector<TripEvent> protection_events;
};
BenchConfig resolved_config(BenchConfig);
std::string config_json(const BenchConfig&);
void validate(const BenchConfig&);
void load_profile(BenchConfig&,const std::string& path);
BenchResult run_bench(BenchConfig);
std::string result_json(const BenchConfig&,const BenchResult&);
}
