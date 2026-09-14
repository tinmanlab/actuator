#pragma once
#include "qdd/control.hpp"
#include <array>
namespace qdd::sim {
using ABCd=Phase<double>;
struct Mechanical {
 double rotor_inertia=8e-5,output_inertia=0.04,ratio=6;
 double stiffness=600,damping=0.6,backlash=0.001;
 double rotor_viscous=0.0002,rotor_coulomb=0.005;
 double output_viscous=0.02,output_coulomb=0.03;
 bool lock_rotor=false,lock_output=false;
};
struct Thermal {
 double ambient=25,winding_capacity=60,case_capacity=180;
 double winding_case_r=0.5,case_ambient_r=2,copper_alpha=0.00393;
};
struct PlantConfig {MotorElectrical motor{}; Mechanical mechanical{}; Thermal thermal{};};
struct PlantState {
 double id=0,iq=0,rotor_angle=0,rotor_speed=0,output_angle=0,output_speed=0;
 double winding_c=25,case_c=25;
};
class Plant {
 PlantConfig c_;
 using Vector=std::array<double,8>;
 Vector derivative(const Vector&,ABCd,double load) const;
public:
 PlantState state{};
 explicit Plant(PlantConfig c={});
 void step(ABCd terminal_voltage,double output_load,double dt);
 // Host-only implicit diode step supplies a physically admissible current endpoint.
 void step_electrical_endpoint(Dq<double> endpoint,double output_load,double dt);
 ABCd currents() const;
 double torque() const;
 double gear_torque() const;
 double resistance() const;
 double energy() const; // electrical + kinetic + elastic, excluding thermal
 const PlantConfig& config() const {return c_;}
 static double coupling_torque(const Mechanical&,double displacement,double relative_speed);
};
enum class Fidelity {Averaged,Switched};
struct InverterConfig {
 double period=50e-6,deadtime=150e-9,rds_on=0.0025,diode_drop=0.8;
 double fet_capacity=20,fet_ambient_r=3,ambient=25;
 Fidelity fidelity=Fidelity::Averaged;
};
struct Leg {bool high=false,low=false; double pole_voltage=0,bus_current=0,loss=0;};
struct InverterOutput {
 ABCd voltage{};double bus_current=0,loss=0;std::array<Leg,3> legs{};
 // Same evaluation state as voltage/loss, not the later plant endpoint.
 ABCd power_current{};double power_vbus=0;
};
class Inverter {
 InverterConfig c_; ABC duty_{0.5f,0.5f,0.5f};
 std::array<double,16> edges_{};int edge_count_=0;
public:
 double fet_c=25;
 explicit Inverter(InverterConfig c={});
 void begin_period(ABC duty);
 double next_event(double phase) const;
 Leg leg(double duty,double phase,double current,double vbus,bool enable) const;
 InverterOutput evaluate(double phase,ABCd current,double vbus,bool enable) const;
 void heat(double loss,double dt);
 const InverterConfig& config() const {return c_;}
};
struct BusConfig {
 double source_voltage=48,capacitance=0.002,source_resistance=0.15;
 bool source_enabled=true,source_can_sink=false,brake_enabled=true;
 double brake_on=53,brake_off=51,brake_resistance=6;
};
class DcLink {
 BusConfig c_;bool braking_=false;
public:
 double voltage=48,source_current=0,brake_power=0;
 explicit DcLink(BusConfig c={});
 void step(double inverter_current,double dt);
 double energy() const {return 0.5*c_.capacitance*voltage*voltage;}
};
// The coupled path handles floating diode legs with a backward-Euler
// complementarity solve; explicit voltage stepping alone cannot extinguish diode current.
InverterOutput advance_bridge(Plant&,Inverter&,DcLink&,double phase,double load,double dt,bool enable);
struct SensorConfig {
 bool linear_filter=false; // Explicit opt-in: preserve the v0.4 reference by default.
 double current_bandwidth=20000,current_range=60,current_noise_std=0.01;
 ABCd offset{0.06,-0.03,0.01};
 int adc_bits=12,rotor_counts=16384,output_counts=65536;
 int current_delay_cycles=0,encoder_delay_cycles=1;
 std::uint32_t seed=42;
};
class Sensors {
 struct Frame {ABC current{};float rotor{},speed{},output{},output_speed{},bus=48,tw=25,tf=25;double time=0;bool valid=true;};
 SensorConfig c_;ABCd filtered_{};std::array<Frame,64> ring_{};int newest_=0;
 std::uint32_t rng_;double last_rotor_=0,last_output_=0,unwrapped_rotor_=0,unwrapped_output_=0;
 float speed_=0,output_speed_=0;bool seeded_=false;
 double noise();
public:
 explicit Sensors(SensorConfig c={});
 void analog_step(ABCd,double dt);
 void analog_step_linear(ABCd begin,ABCd end,double dt);
 ABCd filtered_current() const {return filtered_;}
 void capture(const PlantState&,double bus,double fet,double time,double sample_period,bool valid=true);
 Measurement read(double now) const;
};
}
