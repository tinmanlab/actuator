// Host/SIL example of the SAME facade exported to WebAssembly. No hardware I/O.
#include "../web/live.h"
#include <cmath>
#include <iostream>
int main() {
 if (!lab_reset(0) || !lab_set(0, 4) || !lab_set(1, 0.4)) return 1;
 // reset(0) chooses PI; set(0,4) chooses impedance; set(1,...) is output radians.
 for (int i=0; i<160; ++i) if (lab_step(100)!=100) return 2;
 // 16,000 ticks * 50 us = 0.8 simulated seconds; not wall-clock pacing.
 const double position=lab_get(1), current=lab_get(4);
 std::cout << "output_rad=" << position << ", iq_A=" << current << '\n';
 if (lab_get(12)!=0 || std::abs(position-0.4)>0.03) return 3;
 return 0;
}
