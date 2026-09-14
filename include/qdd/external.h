#ifndef QDD_EXTERNAL_H
#define QDD_EXTERNAL_H
#include <stdint.h>
#ifdef _WIN32
#define QDD_EXPORT __declspec(dllexport)
#else
#define QDD_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Host-only C ABI v1. SI units. Mechanics MUST live in exactly one engine.
   Does not replace the allocation-free embedded qdd/c_api.h interface. */
typedef struct {
 double rotor_angle,rotor_speed,output_angle,output_speed;
 double position_ref,velocity_ref,torque_ref,iq_ref,kp,kd;
 int32_t mode,enable,inject_driver_fault;
} QddExternalInput;
typedef struct {
 double time_s,rotor_torque,output_torque,id,iq,iq_ref,vbus,winding_c,fet_c;
 double duty_a,duty_b,duty_c,phase_peak,current_age,encoder_age;
 int32_t state,fault,gate_enabled,break_latched;
} QddExternalOutput;
QDD_EXPORT uint32_t qdd_external_abi_version(void);
QDD_EXPORT void* qdd_external_create(int32_t switched,int32_t predictive);
QDD_EXPORT int qdd_external_tick(void*,const QddExternalInput*,QddExternalOutput*);
QDD_EXPORT void qdd_external_destroy(void*);
QDD_EXPORT double qdd_external_period(void);
#ifdef __cplusplus
}
#endif
#endif
