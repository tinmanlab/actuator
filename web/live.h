#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Single-instance browser adapter; physics lives in src/control, plant, protection.
int lab_reset(int predictive);
int lab_set(int field, double value);
int lab_step(int ticks);
double lab_get(int field);
#ifdef __cplusplus
}
#endif
