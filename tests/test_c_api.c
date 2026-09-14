#include "qdd/c_api.h"
#include <math.h>
#include <stdio.h>
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"C API failure line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
 qdd_context context={0};qdd_config config=qdd_default_config();
 qdd_measurement m={0};qdd_command c={0};qdd_output out={0};
 m.bus_V=48;m.winding_C=25;m.fet_C=25;m.valid=1;
 REQUIRE(qdd_init(&context,&config)==0);
 c.arm=1;c.enable=1;
 REQUIRE(qdd_tick(&context,&m,&c,&out)==0);REQUIRE(out.gate_enable==0);
 c.arm=0;c.enable=0;c.calibrate=1;
 for(int n=0;n<64;n++){REQUIRE(qdd_tick(&context,&m,&c,&out)==0);c.calibrate=0;}
 REQUIRE(out.state==2);
 c.arm=1;c.enable=1;c.iq_A=3;
 REQUIRE(qdd_tick(&context,&m,&c,&out)==0);REQUIRE(out.gate_enable==1);REQUIRE(fabsf(out.iq_ref_A-3)<1e-6f);
 m.rotor_rad=NAN;
 REQUIRE(qdd_tick(&context,&m,&c,&out)==0);REQUIRE(out.gate_enable==0);REQUIRE(out.fault==2);
 qdd_destroy(&context);
 REQUIRE(qdd_tick(&context,&m,&c,&out)==-1);REQUIRE(out.gate_enable==0);
 REQUIRE(qdd_init(&context,&config)==0);qdd_destroy(&context);
 puts("PASS C11 caller -> exact same C++ Drive");return 0;
}
