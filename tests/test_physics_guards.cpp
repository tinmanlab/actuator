#include "qdd/plant.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qdd::sim;
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<class F> bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){
 int bad=0;const double nan=std::numeric_limits<double>::quiet_NaN();
 auto test=[&](const char* name,auto f){try{f();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++bad;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
 test("negative dissipation is not refrigeration",[]{Inverter i;require(rejects([&]{i.heat(-10,.1);}),"negative loss accepted");require(i.fet_c==25,"state changed after rejection");});
 test("invalid heat time and NaN are rejected atomically",[&]{Inverter i;require(rejects([&]{i.heat(1,-.1);}),"negative time accepted");require(rejects([&]{i.heat(nan,.1);}),"NaN loss accepted");require(i.fet_c==25,"temperature corrupted");});
 test("diode endpoint rejects invalid current and time",[&]{Plant p;require(rejects([&]{p.step_electrical_endpoint({nan,0},0,1e-6);}),"NaN current accepted");require(rejects([&]{p.step_electrical_endpoint({0,1},0,-1e-6);}),"negative time accepted");require(p.state.iq==0&&p.state.id==0,"state corrupted");});
 test("exact lumped heat response",[]{Inverter i;i.heat(2,60);require(std::abs(i.fet_c-(31-6/std::exp(1.0)))<1e-12,"RC response changed");});
 return bad?1:0;
}
