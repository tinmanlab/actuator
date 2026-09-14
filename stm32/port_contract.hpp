#pragma once
#include "qdd/control.hpp"
namespace qdd::stm32 {
/* BSP CONTRACT ONLY: no HAL/register implementation and no hardware access.
 * Intended initial part: STM32G474. TIM1/ADC trigger/shunt topology must be
 * selected for a SPECIFIC power board, not inferred from the MCU family. */
class BoardPort {
public:
 virtual ~BoardPort()=default;
 // Return a coherent ADC/encoder snapshot including ages and validity.
 virtual Measurement read_frame() noexcept=0;
 // This must represent a hardware inhibit / timer BREAK latch, not only software.
 virtual bool break_latched() const noexcept=0;
 // False until board-specific commissioning is explicitly accepted externally.
 virtual bool commissioned() const noexcept=0;
 virtual void force_gate_off() noexcept=0;
 // BSP must latch all 3 preloads at one known update boundary BEFORE gate enable.
 // It must independently refuse enable while BREAK/nFAULT/commissioning inhibit is active.
 virtual void commit_pwm_and_gate(ABC duty) noexcept=0;
};
inline DriveOutput control_irq(Drive& drive,BoardPort& board,Command cmd) noexcept {
 Measurement m=board.read_frame();m.driver_fault=m.driver_fault||board.break_latched();
 if(!board.commissioned()){cmd.enable=false;cmd.arm=false;}
 DriveOutput out=drive.tick(m,cmd);
 if(out.gate_enable&&!board.break_latched()&&board.commissioned())board.commit_pwm_and_gate(out.duty);
 else {board.force_gate_off();out.gate_enable=false;}
 return out;
}
}
