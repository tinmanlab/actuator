#pragma once
#include "qdd/control.hpp"
#include <array>
#include <limits>
#include <vector>

namespace qdd::sim {
// Host SIL only. All defaults are synthetic, NOT STM32/driver specifications.
struct FastTripConfig {
 bool enabled=false; // Preserve v0.3 software-only fixtures unless explicitly enabled.
 double threshold=45, hysteresis=2;
 double propagation=200e-9, break_delay=100e-9, gate_off_delay=200e-9;
 double blanking_start=0, blanking=0, reset_hold=10e-6;
};
enum class TripEventKind {
 CurrentCrossing, ComparatorAssert, ComparatorClear, BreakLatch, PwmInhibit,
 GatesOff, SupervisorObserved, LatchCleared, Rearmed, InvalidInput
};
const char* trip_event_name(TripEventKind) noexcept;
struct TripEvent {
 TripEventKind kind{};
 double time=0, current_peak=0;
 double crossing_lower=-1, crossing_upper=-1;
 bool current_valid=true, blanked=false;
 unsigned episode=0;
};
// Causal endpoint-observed comparator. Resolve next_event() before stepping the
// plant; never interpolate a trip into an already integrated interval.
// Comparator hysteresis/propagation precede the timer-output blanking mask.
// The timer BREAK latches independently of ADC queues and Drive::tick().
class FastTrip {
 static constexpr double infinity=std::numeric_limits<double>::infinity();
 FastTripConfig c_; double period_;
 std::array<bool,3> window_{};
 bool raw_=false, comparator_=false, pending_target_=false;
 bool latched_=false, gate_off_=false, restart_hold_=false, observed_=false;
 bool input_valid_=true, arm_low_seen_=false;
 bool last_gate_request_=false, request_at_trip_=false;
 double now_=-1, peak_=0, crossing_lo_=-1, crossing_hi_=-1;
 double comparator_due_=infinity, break_due_=infinity, gate_due_=infinity;
 double safe_since_=-1, clear_time_=-1;
 unsigned episode_=0;
 std::vector<TripEvent> events_;
 void record(TripEventKind,double);
 void latch(double,bool invalid);
public:
 explicit FastTrip(FastTripConfig c={},double pwm_period=50e-6);
 void observe(double now,Phase<double> true_currents);
 double next_event(double now) const;
 bool blanked(double now) const;
 bool raw_asserted() const noexcept {return raw_;}
 bool comparator_asserted() const noexcept {return comparator_;}
 bool latched() const noexcept {return latched_;}
 bool pwm_inhibited() const noexcept {return latched_||restart_hold_;}
 bool gates_inhibited() const noexcept {return gate_off_||restart_hold_;}
 // Pending turn-off preserves the pre-trip request until the modeled driver
 // delay expires; a CPU observation must not silently shorten that delay.
 bool gate_allowed(bool request) noexcept;
 void supervisor_observed(double now);
 // These APIs do not acknowledge Drive faults or issue an arm command.
 // Explicit clear leaves a separate restart inhibit; a fresh Ready+arm is required.
 bool clear_latch(double now,Phase<double> currents,bool disabled_request,bool explicit_reset);
 bool rearm(double now,bool explicit_arm,State supervisor_state);
 const FastTripConfig& config() const noexcept {return c_;}
 const std::vector<TripEvent>& events() const noexcept {return events_;}
};
}
