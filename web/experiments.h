#ifndef QDD_EXPERIMENTS_H
#define QDD_EXPERIMENTS_H
/* Host/Worker API; allocates result buffers. Not suitable for an MCU ISR.
 * SI units except motor RPM and dead time (ns). See docs/POWERTRAIN.md.
 * Failure preserves the previous complete result. Invalid reads return NaN. */
#ifdef __cplusplus
extern "C" {
#endif
int exp_run(int kind, double iq, double rpm, double noise, double bandwidth_hz,
            double deadtime_ns, double ambient_c, double rotor_viscous);
int exp_rows(int table);
double exp_get(int table, int row, int column);
#ifdef __cplusplus
}
#endif
#endif
