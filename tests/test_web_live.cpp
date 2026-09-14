#include "../web/live.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <cstdlib>
static int checks=0;
void require(bool p,const char* what){++checks;if(!p){std::cerr<<"FAIL: "<<what<<"\n";std::exit(1);}}
void advance(double s){for(int i=0;i<int(s*20000);i+=100)require(lab_step(100)==100,"deterministic stepping");}
int main(){
 require(lab_reset(0)==1,"reset PI instance");
 require(lab_set(0,4)==1 && lab_set(1,.4)==1,"set impedance position");
 advance(.8);
 require(lab_get(11)==3 && lab_get(12)==0,"calibrates then arms without fault");
 require(std::abs(lab_get(1)-.4)<.015,"tracks .4 rad");
 require(lab_set(4,2)==1,"constant output load");advance(.8);
 require(std::abs(lab_get(1)-(.4-2./30))<.015,"load causes impedance displacement");
 require(lab_reset(0)==1,"reset contact trial");lab_set(1,.85);lab_set(5,1);advance(1.2);
 require(lab_get(1)>.59 && lab_get(1)<.63,"unilateral angular contact constrains output");
 require(lab_get(20)>1,"contact reaction reported");
 require(lab_set(6,1)==1,"inject gate driver fault");advance(.02);
 require(lab_get(12)==9 && lab_get(13)==0,"gate fault latched and PWM disabled");
 lab_set(6,0);advance(.02);require(lab_get(12)==9,"fault does not auto rearm");
 require(lab_set(1,std::numeric_limits<double>::quiet_NaN())==0,"reject NaN command");
 require(lab_set(0,5)==0 && lab_set(9,9000)==0,"reject invalid mode/gain");
 require(lab_step(-1)==0 && lab_step(10001)==0,"bound work per worker call");
 require(lab_reset(1)==1,"reset predictive instance");lab_set(1,.3);advance(.8);
 require(lab_get(12)==0 && std::abs(lab_get(1)-.3)<.02,"predictive control tracks target");
 lab_reset(0);lab_set(1,.3);lab_step(2000);double q=lab_get(1),iq=lab_get(4);
 lab_reset(0);lab_set(1,.3);for(int i=0;i<20;i++)lab_step(100);
 require(q==lab_get(1) && iq==lab_get(4),"chunk sizes do not change plant trajectory");
 lab_reset(0);lab_set(1,.4);advance(.5);lab_set(7,4);advance(.02);
 require(lab_get(19)==4,"disturbance pulse applied in simulation time");advance(.2);
 require(lab_get(19)==0,"pulse expires after .12 simulated seconds");
 std::cout<<"PASS: live browser adapter contract ("<<checks<<" checks)\n";
}
