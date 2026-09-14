#include "qdd/bench.hpp"
#include <iomanip>
#include <sstream>
namespace qdd::sim {
namespace {
// Small host-only writer: keys are fixed literals and strings are validated enums.
struct Object {
 std::ostringstream& s;bool first=true;
 explicit Object(std::ostringstream& stream):s(stream){s<<'{';}
 ~Object(){s<<'}';}
 template<class T> void field(const char* key,T value){if(!first)s<<',';first=false;s<<'"'<<key<<"\":"<<value;}
};
std::string motor_json(const MotorElectrical& m){
 std::ostringstream s;s<<std::setprecision(17);{
 Object o(s);o.field("resistance_ohm",m.resistance);o.field("ld_H",m.ld);o.field("lq_H",m.lq);
 o.field("flux_Wb",m.flux);o.field("pole_pairs",m.pole_pairs);
 }return s.str();
}
}
std::string config_json(const BenchConfig& input){
 const auto c=resolved_config(input);std::ostringstream s;s<<std::setprecision(17)<<std::boolalpha;
 {
 Object o(s);o.field("schema",2);o.field("duration_s",c.duration);o.field("max_step_s",c.step);
 o.field("trace_divider",c.trace_divider);o.field("csv_logging",c.log_csv);
 o.field("excitation_frequency_Hz",c.excitation_frequency);
 o.field("scenario","\""+c.scenario+"\"");
 o.field("initial_winding_C",c.scenario=="thermal"?105.0:c.plant.thermal.ambient);
 o.field("controller_motor",motor_json(c.drive.motor));o.field("plant_motor",motor_json(c.plant.motor));
 auto object=[&](auto writer){std::ostringstream sub;sub<<std::setprecision(17)<<std::boolalpha;{Object v(sub);writer(v);}return sub.str();};
 o.field("drive",object([&](Object& v){const auto& x=c.drive;
 v.field("period_s",x.dt);v.field("current_bandwidth_design_Hz",x.current_bandwidth_hz);v.field("gear_ratio",x.gear_ratio);v.field("current_limit_A",x.current_limit);
 v.field("overcurrent_A",x.overcurrent);v.field("voltage_min_V",x.voltage_min);v.field("voltage_max_V",x.voltage_max);
 v.field("torque_limit_Nm",x.torque_limit);v.field("velocity_limit_rad_s",x.velocity_limit);
 v.field("motor_derate_C",x.motor_derate);v.field("motor_trip_C",x.motor_trip);v.field("fet_derate_C",x.fet_derate);v.field("fet_trip_C",x.fet_trip);
 v.field("sensor_timeout_s",x.sensor_timeout);v.field("command_timeout_s",x.command_timeout);v.field("servo_divider",x.servo_divider);
 v.field("calibration_samples",x.calibration_samples);v.field("algorithm",int(x.algorithm));
 }));
 o.field("mechanical",object([&](Object& v){const auto& x=c.plant.mechanical;
 v.field("rotor_inertia",x.rotor_inertia);v.field("output_inertia",x.output_inertia);v.field("ratio",x.ratio);
 v.field("stiffness",x.stiffness);v.field("damping",x.damping);v.field("backlash",x.backlash);
 v.field("rotor_viscous",x.rotor_viscous);v.field("rotor_coulomb",x.rotor_coulomb);
 v.field("output_viscous",x.output_viscous);v.field("output_coulomb",x.output_coulomb);
 v.field("lock_rotor",x.lock_rotor);v.field("lock_output",x.lock_output);
 }));
 o.field("thermal",object([&](Object& v){const auto& x=c.plant.thermal;
 v.field("ambient_C",x.ambient);v.field("winding_capacity",x.winding_capacity);v.field("case_capacity",x.case_capacity);
 v.field("winding_case_r",x.winding_case_r);v.field("case_ambient_r",x.case_ambient_r);v.field("copper_alpha",x.copper_alpha);
 }));
 o.field("inverter",object([&](Object& v){const auto& x=c.inverter;
 v.field("period_s",x.period);v.field("deadtime_s",x.deadtime);v.field("rds_on_ohm",x.rds_on);v.field("diode_drop_V",x.diode_drop);
 v.field("fet_capacity",x.fet_capacity);v.field("fet_ambient_r",x.fet_ambient_r);v.field("ambient_C",x.ambient);
 v.field("fidelity",int(x.fidelity));
 }));
 o.field("bus",object([&](Object& v){const auto& x=c.bus;
 v.field("voltage_V",x.source_voltage);v.field("capacitance_F",x.capacitance);v.field("source_resistance_ohm",x.source_resistance);
 v.field("source_enabled",x.source_enabled);v.field("source_can_sink",x.source_can_sink);v.field("brake_enabled",x.brake_enabled);
 v.field("brake_on_V",x.brake_on);v.field("brake_off_V",x.brake_off);v.field("brake_resistance_ohm",x.brake_resistance);
 }));
 o.field("fast_trip",object([&](Object& v){const auto& x=c.protection;
 v.field("enabled",x.enabled);v.field("threshold_A",x.threshold);v.field("hysteresis_A",x.hysteresis);
 v.field("propagation_s",x.propagation);v.field("break_delay_s",x.break_delay);v.field("gate_off_delay_s",x.gate_off_delay);
 v.field("blanking_start_s",x.blanking_start);v.field("blanking_s",x.blanking);v.field("reset_hold_s",x.reset_hold);
 }));
 o.field("protection_trace",c.log_protection);
 o.field("sensor",object([&](Object& v){const auto& x=c.sensor;
 v.field("linear_filter",x.linear_filter);v.field("current_bandwidth_Hz",x.current_bandwidth);v.field("current_range_A",x.current_range);v.field("current_noise_A",x.current_noise_std);
 v.field("offset_a_A",x.offset.a);v.field("offset_b_A",x.offset.b);v.field("offset_c_A",x.offset.c);
 v.field("adc_bits",x.adc_bits);v.field("rotor_counts",x.rotor_counts);v.field("output_counts",x.output_counts);
 v.field("current_delay_cycles",x.current_delay_cycles);v.field("encoder_delay_cycles",x.encoder_delay_cycles);v.field("seed",x.seed);
 }));
 }
 return s.str();
}
}
