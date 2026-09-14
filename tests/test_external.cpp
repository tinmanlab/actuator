#include "qdd/external.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <limits>
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
struct Handle {void* p=qdd_external_create(0,0);~Handle(){qdd_external_destroy(p);}};
int main(int argc,char** argv) {
 std::map<std::string,std::function<void()>> tests;
 tests["external_calibrates_and_tracks_current"]=[]{
  Handle h;CHECK(h.p);QddExternalInput i{};i.mode=0;i.enable=1;i.iq_ref=4;QddExternalOutput o{};
  for(int n=0;n<2000;n++)CHECK(qdd_external_tick(h.p,&i,&o)==0);
  CHECK(o.state==3);CHECK(o.fault==0);CHECK(o.gate_enabled);CHECK(std::abs(o.iq-4)<.05);
  CHECK(std::abs(o.time_s-.1)<1e-12);CHECK(o.rotor_torque>.4);
 };
 tests["external_gear_reaction_is_reciprocal"]=[]{
  Handle h;QddExternalInput i{};i.rotor_angle=.06;i.mode=0;QddExternalOutput o{};
  CHECK(qdd_external_tick(h.p,&i,&o)==0);CHECK(std::abs(o.output_torque-5.7)<1e-10);
  CHECK(std::abs(o.rotor_torque+o.output_torque/6)<1e-10);
 };
 tests["external_rejects_nan_and_stays_inhibited"]=[]{
  Handle h;QddExternalInput i{};i.output_speed=std::numeric_limits<double>::quiet_NaN();QddExternalOutput o{};
  CHECK(qdd_external_tick(h.p,&i,&o)==-1);CHECK(!o.gate_enabled);
  i.output_speed=0;CHECK(qdd_external_tick(h.p,&i,&o)==-1);
 };
 tests["external_driver_fault_is_latched"]=[]{
  Handle h;QddExternalInput i{};i.enable=1;i.iq_ref=4;QddExternalOutput o{};
  for(int n=0;n<500;n++)CHECK(qdd_external_tick(h.p,&i,&o)==0);
  i.inject_driver_fault=1;CHECK(qdd_external_tick(h.p,&i,&o)==0);CHECK(o.fault==9);CHECK(!o.gate_enabled);
  i.inject_driver_fault=0;for(int n=0;n<300;n++)CHECK(qdd_external_tick(h.p,&i,&o)==0);
  CHECK(o.fault==9);CHECK(std::abs(o.iq)<1e-5);
 };
 tests["external_invalid_creation_and_null_arguments"]=[]{
  CHECK(!qdd_external_create(2,0));QddExternalOutput o{};QddExternalInput i{};
  CHECK(qdd_external_tick(nullptr,&i,&o)==-1);qdd_external_destroy(nullptr);
 };
 if(argc!=2||!tests.count(argv[1]))return 2;
 try{tests.at(argv[1])();std::cout<<argv[1]<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<argv[1]<<": "<<e.what()<<'\n';return 1;}
}
