#include "../web/live.h"
#include "../web/stop.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
static void require(bool p,const char* why){if(!p){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
static void advance(double s){for(int i=0;i<int(s*20000);i+=100)require(lab_step(100)==100,"stepping succeeds");}
int main(){
 require(lab_reset(0)==1,"reset");
 require(lab_set(0,2)&&lab_set(2,-3)&&lab_set(5,1),"reverse velocity with stop");
 advance(3);
 std::cout<<"q="<<lab_get(1)<<" reaction="<<lab_get(20)<<" fault="<<lab_get(12)<<'\n';
 require(lab_get(1)>-5.25&&lab_get(1)<-5.10,"reverse hits the opposite face, not a full turn through the stop");
 require(lab_get(20)<-1,"opposite-face reaction has the opposite sign");
 require(lab_get(12)==0,"normal reverse contact remains powered");
 lab_set(2,3);advance(3);
 require(lab_get(1)>.59&&lab_get(1)<.63,"release and return stops at the front face");
 require(lab_get(20)>1,"front-face reaction is positive load");

 // Periodicity, signs and safe enable without injecting any plant state.
 for(int turn=-20;turn<=20;++turn){
  const double offset=turn*2*std::acos(-1.);
  qdd::web::PeriodicStop stop;
  require(stop.enable(offset),"enable from a clear pose on each turn");
  require(std::abs(stop.upper()-(offset+qdd::web::stop_entry))<1e-10,"front boundary is periodic");
  require(stop.reaction(stop.upper()+.001,1)>0,"front reaction opposes positive motion");
  require(stop.reaction(stop.lower()-.001,-1)<0,"rear reaction opposes reverse motion");
  require(stop.reaction(offset,0)==0,"free arc has no contact force");
  require(stop.reaction(stop.upper()+.001,-10)==0,"no adhesive pull on unloading");
  stop.disable();require(stop.reaction(stop.lower()-1,-1)==0,"disabled stop has no force");
  require(!stop.enable(offset+.8),"enabling in solid volume is rejected");
 }
 // Exercise the actual facade, not only the stop helper.
 for(int algorithm=0;algorithm<=1;++algorithm)for(double speed:{-6.,-3.,-1.,1.,3.,6.}){
  require(lab_reset(algorithm)&&lab_set(0,2)&&lab_set(2,speed)&&lab_set(5,1),"speed sweep setup");
  double maximum=0;const double duration=speed==-1?6.:3.;
  for(int tick=0;tick<int(duration*20000);tick+=20){
   require(lab_step(20)==20,"speed sweep remains finite");
   maximum=std::max(maximum,lab_get(30));
  }
  std::cout<<"algorithm="<<algorithm<<" speed="<<speed<<" max_deflection_rad="<<maximum<<" fault="<<lab_get(12)<<'\n';
  require(maximum<.03,"bounded compliant deflection, no obstacle traversal");
  require(lab_get(12)==0,"bounded velocity trials do not trip");
 }
 lab_reset(0);lab_set(1,.8);advance(.8);
 require(lab_set(5,1)==0&&lab_get(29)==0,"facade rejects stop insertion into occupied solid");
 const double time=lab_get(0);require(lab_step(20)==20&&lab_get(0)>time,"rejected enable does not kill physics");
 lab_set(1,0);advance(.8);require(lab_set(5,1)==1,"enable works after moving clear");
 std::cout<<"PASS: reverse contact, round trip, periodicity and sweep\n";
}
