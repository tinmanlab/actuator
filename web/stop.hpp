#pragma once
#include "generated_stop.hpp"
#include <algorithm>
#include <cmath>
namespace qdd::web {
// A finite solid obstacle occupies [entry, exit] modulo 2*pi. While enabled,
// keep the same connected free arc: never jump to its other side at mid-contact.
// These are load torques (Plant SUBTRACTS load); they are not commanded torques.
class PeriodicStop {
 static constexpr double turn=6.28318530717958647692;
 bool enabled_=false;
 double lower_=stop_exit-turn,upper_=stop_entry;
public:
 static constexpr double stiffness=6000, damping=25;
 bool enable(double q) noexcept {
  if(!std::isfinite(q))return false;
  if(enabled_)return true;
  const double base=std::floor(q/turn)*turn,phase=q-base;
  if(phase>stop_entry+1e-12&&phase<stop_exit-1e-12)return false;
  lower_=stop_exit+base;upper_=stop_entry+base+turn;
  if(phase<=stop_entry+1e-12){lower_-=turn;upper_-=turn;}
  enabled_=true;return true;
 }
 void disable() noexcept {enabled_=false;}
 bool enabled() const noexcept {return enabled_;}
 double lower() const noexcept {return lower_;}
 double upper() const noexcept {return upper_;}
 double penetration(double q) const noexcept {
  return enabled_?std::max({0.,lower_-q,q-upper_}):0.;
 }
 double reaction(double q,double velocity) const noexcept {
  if(!enabled_)return 0;
  if(q>upper_)return std::max(0.,stiffness*(q-upper_)+damping*velocity);
  if(q<lower_)return -std::max(0.,stiffness*(lower_-q)-damping*velocity);
  return 0;
 }
};
}
