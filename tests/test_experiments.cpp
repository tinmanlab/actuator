#include "../web/experiments.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
static void require(bool x,const char* message){if(!x)throw std::runtime_error(message);}
static void run(int kind=0,double noise=.015,double dead=150,double friction=.0002,double iq=4,double rpm=600){require(exp_run(kind,iq,rpm,noise,10000,dead,25,friction)==1,"native experiment failed");}
static double get(int t,int r,int c){return exp_get(t,r,c);}
int main(int argc,char**argv){try{
 const std::string name=argc>1?argv[1]:"scope";
 if(name=="scope"){
  run();require(exp_rows(0)==2400&&exp_rows(1)>1900,"missing control or switching rows");
  for(int t:{0,1})for(int r=0;r<exp_rows(t);r++){
   for(int c=0;c<48;c++)require(std::isfinite(get(t,r,c)),"nonfinite sample");
   if(r)require(get(t,r,0)>get(t,r-1,0),"nonmonotonic time");
  }
  for(int r=0;r<exp_rows(1);r++){
   double sumv=0,sumi=get(1,r,5)+get(1,r,44)+get(1,r,45);
   for(int k=0;k<3;k++){sumv+=get(1,r,14+k);require(!(get(1,r,25+2*k)&&get(1,r,26+2*k)),"shoot-through");}
   require(std::abs(sumv)<1e-8&&std::abs(sumi)<1e-7,"neutral/KCL mismatch");
   require(std::abs(get(1,r,31)-get(1,r,32)-get(1,r,33))<1e-8,"same-state bridge power mismatch");
  }
  require(std::abs(get(0,2399,4)-4)<.15,"current does not track");
 }
 if(name=="stall_mean"){
  run(0,.05,150,.0002,4,0);const double stall=get(3,0,2);
  run(0,.05,150,.0002,4,1e-8);
  require(std::abs(stall-get(3,0,2))<1e-6,"mean shaft torque discontinuous at zero speed");
 }
 if(name=="thermal"){
  run(1);require(exp_rows(2)==601&&exp_rows(0)==0,"thermal table shape");
  require(get(2,0,1)==25&&get(2,600,1)>get(2,600,2)&&get(2,600,2)>25,"thermal ordering");
  const double hot=get(2,600,1);run(1,.015,150,.0002,0);require(std::abs(get(2,600,1)-25)<1e-8&&hot>25,"zero-current heat");
 }
 if(name=="map"){
  run(2);require(exp_rows(3)==28,"incomplete map");int good=0,bad=0;
  for(int r=0;r<28;r++){
   const double eta=get(3,r,5),pin=get(3,r,3),pout=get(3,r,4);
   if(get(3,r,0)==0)require(eta==-1,"stall efficiency must be absent");
   if(get(3,r,6)){++good;require(eta>0&&eta<=1&&pin>0&&pout>0,"invalid efficiency");require(std::abs(eta-pout/pin)<1e-12,"efficiency boundary");}
   else{++bad;require(eta==-1,"unqualified result painted as efficiency");}
  }require(good>0&&bad>0,"qualification is not exercised");
 }
 if(name=="invalid"){
  run();const double old=get(0,20,0);const int n=exp_rows(1);
  require(!exp_run(0,4,600,.01,10000,500,25,.0002),"invalid deadtime accepted");
  require(!exp_run(0,4,600,.01,10000,150,std::numeric_limits<double>::quiet_NaN(),.0002),"NaN accepted");
  require(exp_rows(1)==n&&get(0,20,0)==old,"failed transaction changed data");
  require(std::isnan(get(9,0,0))&&std::isnan(get(0,-1,0))&&std::isnan(get(0,0,48)),"invalid read not NaN");
 }
 if(name=="repeatability"){
  run();std::vector<double> old;for(int r=0;r<exp_rows(0);r++)old.push_back(get(0,r,4));run();
  for(int r=0;r<exp_rows(0);r++)require(old[r]==get(0,r,4),"not deterministic");
 }
 if(name=="noise"){
  run(0,0);double clean=0;for(int r=2000;r<2400;r++)clean+=std::abs(get(0,r,9)-get(0,r,4));
  run(0,.15);double noisy=0;for(int r=2000;r<2400;r++)noisy+=std::abs(get(0,r,9)-get(0,r,4));require(noisy>clean,"noise input unused");
 }
 if(name=="deadtime"){
  run(0,0,0);int zero=0;for(int r=0;r<exp_rows(1);r++)for(int k=0;k<3;k++)if(!get(1,r,25+2*k)&&!get(1,r,26+2*k))zero++;
  run(0,0,400);int blank=0;for(int r=0;r<exp_rows(1);r++)for(int k=0;k<3;k++)if(!get(1,r,25+2*k)&&!get(1,r,26+2*k))blank++;
  require(zero==0&&blank>0,"dead time not modeled");
 }
 if(name=="friction"){
  run(2,0,150,0);double eta=get(3,9,5),torque=get(3,9,2);run(2,0,150,.0008);
  require(get(3,9,5)>0&&get(3,9,5)<eta&&get(3,9,2)<torque,"friction does not reduce shaft performance");
 }
 std::cout<<name<<" PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
