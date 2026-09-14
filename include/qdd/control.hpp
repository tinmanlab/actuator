#pragma once
#include "qdd/math.hpp"
#include <cstdint>
namespace qdd {
struct MotorElectrical {
 float resistance=0.08f, ld=80e-6f, lq=90e-6f, flux=0.011f;
 int pole_pairs=7;
 bool valid() const noexcept;
 float torque_per_iq(float id=0) const noexcept {return 1.5f*float(pole_pairs)*(flux+(ld-lq)*id);}
};
struct Modulation {ABC duty{0.5f,0.5f,0.5f}; DQ voltage{}; bool valid=false;};
Modulation svpwm(DQ voltage,float electrical_angle,float vbus,float margin=0.02f) noexcept;
struct CurrentFeedback {DQ current{}; float electrical_speed{}, vbus=48;};
class CurrentAlgorithm {
public:
 virtual ~CurrentAlgorithm()=default;
 virtual DQ voltage(DQ reference,const CurrentFeedback&,float dt) noexcept=0;
 virtual void track(DQ requested,DQ applied,float dt) noexcept=0;
 virtual void reset() noexcept=0;
};
class PiFoc final:public CurrentAlgorithm {
 MotorElectrical p_; DQ integral_{}; float bandwidth_;
public:
 explicit PiFoc(MotorElectrical p={},float bandwidth_hz=600):p_(p),bandwidth_(bandwidth_hz){}
 DQ voltage(DQ,const CurrentFeedback&,float) noexcept override;
 void track(DQ,DQ,float) noexcept override;
 void reset() noexcept override {integral_={};}
 DQ integral() const noexcept {return integral_;}
};
// Relaxed one-step model prediction, not constrained MPC and not a sensorless observer.
class PredictiveCurrent final:public CurrentAlgorithm {
 MotorElectrical p_; DQ bias_{};
public:
 explicit PredictiveCurrent(MotorElectrical p={}):p_(p){}
 DQ voltage(DQ,const CurrentFeedback&,float) noexcept override;
 void track(DQ,DQ,float) noexcept override;
 void reset() noexcept override {bias_={};}
};
enum class Algorithm:std::uint8_t {Pi=0,Predictive=1};
enum class Mode:std::uint8_t {Current=0,Torque=1,Velocity=2,Position=3,Impedance=4};
enum class State:std::uint8_t {Disabled=0,Calibrating=1,Ready=2,Armed=3,Fault=4};
enum class Fault:std::uint8_t {None=0,Configuration=1,NonFinite=2,OverCurrent=3,
 UnderVoltage=4,OverVoltage=5,OverTemperature=6,Sensor=7,CommandTimeout=8,
 GateDriver=9,Calibration=10};
const char* fault_name(Fault) noexcept;
const char* state_name(State) noexcept;
struct DriveConfig {
 MotorElectrical motor{};
 float dt=50e-6f, gear_ratio=6, current_limit=30, overcurrent=45;
 float current_bandwidth_hz=600; // PI design parameter, not measured closed-loop bandwidth.
 float voltage_min=12,voltage_max=58,torque_limit=15,velocity_limit=12;
 float motor_derate=80,motor_trip=115,fet_derate=75,fet_trip=100;
 float sensor_timeout=0.001f,command_timeout=0.020f;
 int servo_divider=20, calibration_samples=64;
 Algorithm algorithm=Algorithm::Pi;
 bool valid() const noexcept;
};
struct Measurement {
 ABC currents{};
 float rotor_angle{},rotor_speed{},output_angle{},output_speed{};
 float vbus=48,winding_c=25,fet_c=25;
 float current_age{},encoder_age{};
 bool valid=true,driver_fault=false;
};
struct Command {
 Mode mode=Mode::Current;
 DQ current{};
 float position{},velocity{},torque{},kp=30,kd=1.8f,age{};
 bool enable=false,calibrate=false,arm=false,acknowledge_fault=false;
};
struct DriveOutput {
 ABC duty{0.5f,0.5f,0.5f};
 DQ current{},reference{},voltage{};
 float torque_reference{},current_limit{};
 bool gate_enable=false,voltage_saturated=false,reference_limited=false;
 State state=State::Disabled; Fault fault=Fault::None;
};
class Drive {
 DriveConfig c_; PiFoc pi_; PredictiveCurrent predictive_;
 CurrentAlgorithm* algorithm_;
 State state_=State::Disabled; Fault fault_=Fault::None;
 ABC offset_{},sum_{}; int calibration_count_=0,servo_count_=0;
 float velocity_integral_=0,torque_ref_=0;
 Mode last_mode_=Mode::Current;
 void trip(Fault) noexcept;
 Fault health(const Measurement&,const Command&) const noexcept;
public:
 explicit Drive(DriveConfig c={}) noexcept;
 Drive(const Drive&)=delete; Drive& operator=(const Drive&)=delete;
 // Caller owns algorithm lifetime. Swap only while Disabled or Ready.
 bool use_algorithm(CurrentAlgorithm&) noexcept;
 DriveOutput tick(const Measurement&,const Command&) noexcept;
 State state() const noexcept {return state_;}
 Fault fault() const noexcept {return fault_;}
 ABC offsets() const noexcept {return offset_;}
 DQ pi_integral() const noexcept {return pi_.integral();}
};
}
