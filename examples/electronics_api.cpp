#include "../web/experiments.h"
#include <iostream>
// Motor-shaft map. Each row is a fresh synthetic averaged-inverter dyno run.
int main() {
    if (!exp_run(2, 4, 600, .015, 10000, 150, 25, .0002)) return 1;
    std::cout << "rpm,iq_ref_A,shaft_torque_Nm,dc_W,shaft_W,eta,qualified\n";
    for (int r = 0; r < exp_rows(3); ++r) {
        for (int c = 0; c < 7; ++c) {
            if (c) std::cout << ',';
            std::cout << exp_get(3, r, c);
        }
        std::cout << '\n';
    }
    return exp_rows(3) == 28 ? 0 : 2;
}
