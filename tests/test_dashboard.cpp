#include "../web/live.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
extern "C" double lab_power(int);
extern "C" double lab_pwm(double,int);
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
 check(std::isnan(lab_power(0)),"power before initialization");
 lab_reset(0);lab_step(10000);double q=lab_get(1),i=lab_get(4);
 for(int k=0;k<16;k++)check(std::isfinite(lab_power(k)),"nonfinite power");
 check(lab_power(0)==lab_get(0),"power time");
 check(std::abs(lab_power(1)-lab_power(2)-lab_power(3))<1e-7,"DC != AC + inverter dissipation");
 check(lab_power(3)>=0&&lab_power(4)>=0&&lab_power(5)>=0,"negative loss");
 for(int n=0;n<5000;n++){
  double t=n*1e-8;
  for(int k=0;k<3;k++)check(!(lab_pwm(t,3+2*k)&&lab_pwm(t,4+2*k)),"reconstructed shoot through");
 }
 for(int k=10;k<=15;k++)check(std::isfinite(lab_pwm(12.5e-6,k)),"missing reconstructed bridge voltage");
 check(std::abs(lab_pwm(12.5e-6,13)+lab_pwm(12.5e-6,14)+lab_pwm(12.5e-6,15))<1e-8,"phase-neutral reconstruction violates KVL");
 check(std::isnan(lab_pwm(-1,0))&&std::isnan(lab_pwm(0,16)),"bad PWM reads");
 check(q==lab_get(1)&&i==lab_get(4),"diagnostics mutate physics");
 lab_step(1000);double final=lab_get(1);
 lab_reset(0);lab_step(10000);lab_step(1000);check(final==lab_get(1),"diagnostics alter future trajectory");
 lab_set(6,1);lab_step(100);for(int k=3;k<9;k++)check(lab_pwm(20e-6,k)==0,"trip must inhibit reconstructed gates");
 check(std::isnan(lab_power(16)),"invalid power read");
 std::cout<<"dashboard diagnostics PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
