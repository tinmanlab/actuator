#include "qdd/protection.hpp"
#include <stdexcept>
namespace qdd::sim {
namespace {
constexpr double eps=1e-14;
bool nonnegative(double v){return std::isfinite(v)&&v>=0;}
}
const char* trip_event_name(TripEventKind k) noexcept {
 switch(k) {
 case TripEventKind::CurrentCrossing:return "current_crossing";
 case TripEventKind::ComparatorAssert:return "comparator_assert";
 case TripEventKind::ComparatorClear:return "comparator_clear";
 case TripEventKind::BreakLatch:return "break_latch";
 case TripEventKind::PwmInhibit:return "pwm_inhibit";
 case TripEventKind::GatesOff:return "gates_off";
 case TripEventKind::SupervisorObserved:return "supervisor_observed";
 case TripEventKind::LatchCleared:return "latch_cleared";
 case TripEventKind::Rearmed:return "rearmed";
 case TripEventKind::InvalidInput:return "invalid_input";
 }
 return "unknown";
}
FastTrip::FastTrip(FastTripConfig c,double p):c_(c),period_(p) {
 if(!std::isfinite(p)||p<=0||!std::isfinite(c.threshold)||c.threshold<=0||
 !nonnegative(c.hysteresis)||c.hysteresis>=c.threshold||
 !nonnegative(c.propagation)||!nonnegative(c.break_delay)||!nonnegative(c.gate_off_delay)||
 !nonnegative(c.blanking)||!nonnegative(c.blanking_start)||!nonnegative(c.reset_hold)||
 c.blanking>=p||c.blanking_start>=p||c.blanking+c.blanking_start>p||
 c.propagation>p||c.break_delay>p||c.gate_off_delay>p)
  throw std::invalid_argument("invalid synthetic fast-trip timing/threshold/hysteresis/blanking");
}
bool FastTrip::blanked(double t) const {
 if(!c_.enabled||c_.blanking==0)return false;
 // Snap roundoff at a PWM boundary; retain half-open [start,end) semantics.
 double phase=t-std::floor((t+eps)/period_)*period_;
 if(phase<0&&phase>-2*eps)phase=0;
 return phase+eps>=c_.blanking_start&&phase<c_.blanking_start+c_.blanking-eps;
}
void FastTrip::record(TripEventKind k,double t) {
 events_.push_back({k,t,peak_,crossing_lo_,crossing_hi_,input_valid_,blanked(t),episode_});
}
void FastTrip::latch(double t,bool invalid) {
 if(latched_)return;
 latched_=true;observed_=false;request_at_trip_=last_gate_request_;++episode_;
 record(TripEventKind::BreakLatch,t);record(TripEventKind::PwmInhibit,t);
 break_due_=infinity;gate_due_=t+(invalid?0:c_.gate_off_delay);
}
void FastTrip::observe(double t,Phase<double> currents) {
 if(!std::isfinite(t)||t<0||(now_>=0&&t<now_-eps))
  throw std::invalid_argument("fast-trip observation time must be finite and monotone");
 if(!c_.enabled){now_=t;return;}
 if(std::min({comparator_due_,break_due_,gate_due_})<t-eps)
  throw std::invalid_argument("fast-trip scheduled deadline skipped; resolve next_event before stepping");
 const double previous=now_;now_=t;input_valid_=finite(currents);
 if(!input_valid_) {
  // Model validity fail-closed, NOT a claim that a real comparator detects NaN.
  peak_=0;safe_since_=-1;comparator_due_=break_due_=infinity;
  if(!latched_) {record(TripEventKind::InvalidInput,t);latch(t,true);}
  if(!gate_off_)record(TripEventKind::GatesOff,t);
  gate_off_=true;gate_due_=infinity;
  return;
 }
 peak_=peak_abs(currents);
 const double values[]={currents.a,currents.b,currents.c};
 const bool old_raw=raw_;
 for(int n=0;n<3;++n) {
  const double a=std::abs(values[n]);
  if(a>=c_.threshold)window_[n]=true;
  else if(a<=c_.threshold-c_.hysteresis)window_[n]=false;
 }
 raw_=window_[0]||window_[1]||window_[2];
 if(raw_&&!old_raw) {
  crossing_lo_=previous<0?t:previous;crossing_hi_=t;
  record(TripEventKind::CurrentCrossing,t);
 }
 // Inertial propagation model: a sub-delay pulse can be rejected. Input changes
 // at an exact scheduled boundary are applied before resolving that boundary.
 if(raw_==comparator_)comparator_due_=infinity;
 else if(!std::isfinite(comparator_due_)||pending_target_!=raw_) {
  pending_target_=raw_;comparator_due_=t+c_.propagation;
 }
 if(comparator_due_<=t+eps) {
  comparator_=pending_target_;comparator_due_=infinity;
  record(comparator_?TripEventKind::ComparatorAssert:TripEventKind::ComparatorClear,t);
 }
 // Output mask only. Once an unmasked assertion has entered the break path,
 // the break transport delay is not cancelled by a later falling edge/blank.
 if(!latched_&&comparator_&&!blanked(t)&&!std::isfinite(break_due_))break_due_=t+c_.break_delay;
 if(break_due_<=t+eps)latch(t,false);
 if(gate_due_<=t+eps) {gate_off_=true;gate_due_=infinity;record(TripEventKind::GatesOff,t);}
 if(!raw_&&!comparator_&&peak_<=c_.threshold-c_.hysteresis) {
  if(safe_since_<0)safe_since_=t;
 }else safe_since_=-1;
}
double FastTrip::next_event(double t) const {
 if(!std::isfinite(t)||t<0)throw std::invalid_argument("invalid fast-trip event query time");
 if(!c_.enabled)return infinity;
 t=std::max(t,now_); // A past query must never return an event before current state.
 double next=infinity;
 auto add=[&](double event){if(event>t+eps)next=std::min(next,event);};
 add(comparator_due_);add(break_due_);add(gate_due_);
 if(c_.blanking>0) {
  const double base=std::floor((t+eps)/period_)*period_;
  for(double b:{base,base+period_}) {add(b+c_.blanking_start);add(b+c_.blanking_start+c_.blanking);}
 }
 return next;
}
bool FastTrip::gate_allowed(bool request) noexcept {
 if(gates_inhibited())return false;
 if(latched_)return request_at_trip_;
 last_gate_request_=request;return request;
}
void FastTrip::supervisor_observed(double t) {
 if(!std::isfinite(t)||std::abs(t-now_)>eps)throw std::invalid_argument("supervisor must observe current protection time");
 if(latched_&&!observed_){observed_=true;record(TripEventKind::SupervisorObserved,t);}
}
bool FastTrip::clear_latch(double t,Phase<double> currents,bool disabled,bool request) {
 observe(t,currents);
 if(!c_.enabled||!latched_||!gate_off_||!disabled||!request||!input_valid_||raw_||comparator_||
 blanked(t)||safe_since_<0||t-safe_since_+eps<c_.reset_hold)return false;
 latched_=false;restart_hold_=true;arm_low_seen_=false;clear_time_=t;record(TripEventKind::LatchCleared,t);
 return true;
}
bool FastTrip::rearm(double t,bool request,State state) {
 if(!std::isfinite(t)||std::abs(t-now_)>eps)return false;
 if(!c_.enabled||!restart_hold_||latched_)return false;
 if(!request){arm_low_seen_=true;return false;}
 const bool fresh_edge=arm_low_seen_;arm_low_seen_=false;
 if(!fresh_edge||state!=State::Ready||!input_valid_||
 raw_||comparator_||blanked(t)||t<=clear_time_+eps)return false;
 restart_hold_=false;gate_off_=false;last_gate_request_=false;record(TripEventKind::Rearmed,t);return true;
}
}
