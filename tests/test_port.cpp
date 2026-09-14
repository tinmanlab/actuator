#include "../stm32/port_contract.hpp"
#include <cstdio>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"port failure line %d\n",__LINE__);return 1;}}while(0)
struct MockBoard final:qdd::stm32::BoardPort {
 bool ready=false,broken=false;int enables=0,offs=0;
 qdd::Measurement read_frame() noexcept override{return {};}
 bool break_latched() const noexcept override{return broken;}
 bool commissioned() const noexcept override{return ready;}
 void force_gate_off() noexcept override{offs++;}
 void commit_pwm_and_gate(qdd::ABC) noexcept override{enables++;}
};
int main(){
 qdd::Drive drive;MockBoard b;qdd::Command c;c.calibrate=true;
 for(int n=0;n<64;n++){qdd::stm32::control_irq(drive,b,c);c.calibrate=false;}
 c.enable=true;c.arm=true;
 REQUIRE(!qdd::stm32::control_irq(drive,b,c).gate_enable);REQUIRE(b.enables==0);
 b.ready=true;REQUIRE(qdd::stm32::control_irq(drive,b,c).gate_enable);REQUIRE(b.enables==1);
 b.broken=true;REQUIRE(!qdd::stm32::control_irq(drive,b,c).gate_enable);REQUIRE(b.enables==1);
 std::puts("PASS host-mocked STM32 port contract (not hardware)");return 0;
}
