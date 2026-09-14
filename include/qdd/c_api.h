#ifndef QDD_C_API_H
#define QDD_C_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#define QDD_ALIGN alignas(16)
#else
#define QDD_ALIGN _Alignas(16)
#endif
/* Initialize with {0}. Caller owns static storage; no heap allocation.
 * One context per motor. All calls must be serialized by the caller. */
typedef struct {
 QDD_ALIGN unsigned char storage[1024];
 uint32_t initialized;
} qdd_context;
#undef QDD_ALIGN
typedef struct {
 float resistance,ld,lq,flux,period_s,gear_ratio,current_limit_A,overcurrent_A;
 int pole_pairs,algorithm; /* algorithm: 0=PI, 1=relaxed predictive + integral bias */
} qdd_config;
typedef struct {
 float phase_A[3],rotor_rad,rotor_rad_s,output_rad,output_rad_s;
 float bus_V,winding_C,fet_C,current_age_s,encoder_age_s;
 uint8_t valid,driver_fault;
} qdd_measurement;
typedef struct {
 int mode; /* 0=current, 1=output torque, 2=output velocity, 3=output position, 4=impedance */
 float id_A,iq_A,position_rad,velocity_rad_s,torque_Nm,kp_Nm_rad,kd_Nms_rad,age_s;
 uint8_t enable,calibrate,arm,acknowledge_fault;
} qdd_command;
typedef struct {
 float duty[3],id_A,iq_A,id_ref_A,iq_ref_A,current_limit_A;
 uint8_t gate_enable,state,fault;
} qdd_output;
qdd_config qdd_default_config(void);
/* Returns 0 on success, -1 for invalid pointers/storage, -2 for invalid config. */
int qdd_init(qdd_context*,const qdd_config*);
int qdd_tick(qdd_context*,const qdd_measurement*,const qdd_command*,qdd_output*);
void qdd_destroy(qdd_context*);
#ifdef __cplusplus
}
#endif
#endif
