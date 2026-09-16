#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Single-instance browser adapter; physics lives in src/control, plant, protection.
int lab_reset(int predictive);
int lab_set(int field, double value);
int lab_step(int ticks);
double lab_get(int field);
// Latest read-only controller/measurement snapshot; field map in docs/WEB_LAB.md.
// No previous control tick is indicated by field 1 == -1 after reset.
enum { LAB_SIGNAL_COUNT = 29 };
double lab_signal(int field);
/* Read-only period-mean power [16] and frozen-duty PWM reconstruction [16]. */
double lab_power(int field);
double lab_pwm(double phase_seconds, int field);
#ifdef __cplusplus
}
#endif
