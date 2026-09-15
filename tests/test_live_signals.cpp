#include "../web/live.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
extern "C" double lab_signal(int); // New read-only diagnostics, separate from lab_get.
static void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
int main(){try{
 check(std::isnan(lab_signal(0)),"uninitialized read must be NaN");
 lab_reset(0);check(lab_signal(0)==0&&lab_signal(1)==-1,"fresh run has no previous control tick");
 check(std::isnan(lab_signal(-1))&&std::isnan(lab_signal(28)),"invalid field rejected");
 lab_set(1,.4);for(int i=0;i<150;i++)lab_step(100);
 std::array<double,31> before{};for(int k=0;k<31;k++)before[k]=lab_get(k);
 for(int repeat=0;repeat<20;repeat++)for(int k=0;k<28;k++)check(std::isfinite(lab_signal(k)),"diagnostic finite");
 for(int k=0;k<31;k++)check(before[k]==lab_get(k),"inspection changes telemetry/physics");
 check(lab_signal(0)==lab_get(0),"snapshot timestamp differs from scene");
 check(std::abs(lab_signal(0)-lab_signal(1)-50e-6)<1e-10,"FOC tick is not distinguished from endpoint");
 check(std::abs(lab_signal(11)+lab_signal(12)+lab_signal(13))<1e-6,"phase currents violate KCL");
 check(std::abs(lab_signal(3)-lab_get(5))<.03,"FOC measured current mismatch");
 check(lab_signal(6)>=0&&lab_signal(6)<.001,"sensor age contract");
 double old=lab_signal(19);lab_set(1,.8);check(lab_signal(19)==old,"pending command mislabeled as applied");
 lab_step(1);check(std::abs(lab_signal(19)-.8)<1e-6,"next tick fails to report applied target");
 lab_set(6,1);lab_step(100);check(lab_get(12)==9&&lab_get(13)==0,"fault disabled gate");
 check(lab_signal(4)==0&&lab_signal(5)==0,"disabled FOC voltage shown as live");
 // Getters must not change the next trajectory, seed, filters or controller state.
 lab_reset(0);for(int i=0;i<100;i++)lab_step(100);double q=lab_get(1),iq=lab_get(4);
 lab_reset(0);for(int i=0;i<100;i++){lab_step(100);for(int k=0;k<28;k++)(void)lab_signal(k);}
 check(q==lab_get(1)&&iq==lab_get(4),"diagnostics alter trajectory");
 std::cout<<"read-only live signals PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
