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
enum { LAB_SIGNAL_COUNT = 28 };
double lab_signal(int field);
#ifdef __cplusplus
}
#endif
