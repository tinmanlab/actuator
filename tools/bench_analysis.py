"""Standard-library-only, host-side metrics. No plant/controller code is replaced here."""
from __future__ import annotations
import csv
import gzip
import math
from pathlib import Path
from typing import Any


def read_trace(path: str | Path) -> list[dict[str, float]]:
    path = Path(path)
    opener = gzip.open if path.suffix == '.gz' else open
    with opener(path, 'rt', newline='', encoding='utf-8') as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or len(set(reader.fieldnames)) != len(reader.fieldnames):
            raise ValueError('Missing or duplicate trace column names')
        rows = [{k: float(v) for k, v in row.items()} for row in reader]
    if not rows or any(not math.isfinite(v) for row in rows for v in row.values()):
        raise ValueError('Trace is empty or contains nonfinite values')
    if any(b['time_s'] <= a['time_s'] for a, b in zip(rows, rows[1:])):
        raise ValueError('Trace timestamps must be strictly increasing')
    return rows


def step_metrics(rows: list[dict[str, float]], step_s: float = .01, target_A: float = 4,
                 hold_s: float = .005, band_fraction: float = .02) -> dict[str, Any]:
    if target_A <= 0 or hold_s <= 0 or not 0 < band_fraction < 1:
        raise ValueError('Invalid positive-step metric settings')
    samples = [(r['time_s'], r['iq_A']) for r in rows if r['time_s'] >= step_s - 1e-12]
    if len(samples) < 2 or any(not math.isfinite(x) for p in samples for x in p):
        raise ValueError('Need finite post-step samples')
    if any(b[0] <= a[0] for a, b in zip(samples, samples[1:])):
        raise ValueError('Non-monotonic step timestamps')
    end = samples[-1][0]
    tail = [y for t, y in samples if t >= end - .2 * (end - step_s)]
    last_out = -1
    for n, (_, y) in enumerate(samples):
        if abs(y - target_A) > target_A * band_fraction:
            last_out = n
    entry = samples[last_out + 1][0] if last_out + 1 < len(samples) else None
    settling = max(0.0, entry - step_s) if entry is not None and end-entry >= hold_s-1e-12 else None
    def crossing(level: float) -> float | None:
        for n, (t, y) in enumerate(samples):
            if y >= level:
                if n and samples[n-1][1] < level and y != samples[n-1][1]:
                    t0, y0 = samples[n-1]
                    return t0 + (level-y0)/(y-y0)*(t-t0)
                return t
        return None
    t10, t90 = crossing(.1*target_A), crossing(.9*target_A)
    return {
        'target_A': target_A, 'step_s': step_s, 'observed_horizon_s': end-step_s,
        'settling_2pct_s': settling, 'settling_hold_s': hold_s,
        'rise_10_90_s': t90-t10 if t90 is not None and t10 is not None else None,
        'overshoot_pct': max(0.0, (max(y for _, y in samples)-target_A)/target_A*100),
        'tail_mean_A': sum(tail)/len(tail),
        'tail_rmse_A': math.sqrt(sum((y-target_A)**2 for y in tail)/len(tail)),
        'iq_rms_A': math.sqrt(sum(y*y for _, y in samples)/len(samples)),
        'iq_peak_A': max(abs(y) for _, y in samples),
        'settling_definition': 'All remaining observed samples within the commanded-target band; finite-horizon result, not asymptotic stability.'
    }


def sine_fit(t: list[float], y: list[float], frequency: float, minimum_cycles: int = 8) -> dict[str, float]:
    """Least-squares y = DC + a*sin(wt) + b*cos(wt), including non-integer windows."""
    if len(t) != len(y) or len(t) < 32 or frequency <= 0 or (t[-1]-t[0])*frequency < minimum_cycles:
        raise ValueError('Sine fit requires >=32 samples and >=8 observed cycles')
    if any(not math.isfinite(v) for v in t+y) or any(b <= a for a,b in zip(t,t[1:])):
        raise ValueError('Invalid sine-fit samples')
    basis = [(1.0, math.sin(2*math.pi*frequency*x), math.cos(2*math.pi*frequency*x)) for x in t]
    matrix = [[sum(v[i]*v[j] for v in basis) for j in range(3)] +
              [sum(v[i]*z for v,z in zip(basis,y))] for i in range(3)]
    for col in range(3):
        pivot = max(range(col, 3), key=lambda n: abs(matrix[n][col]))
        matrix[col], matrix[pivot] = matrix[pivot], matrix[col]
        scale = matrix[col][col]
        if abs(scale) < 1e-12:
            raise ValueError('Singular sine-fit window')
        matrix[col] = [v/scale for v in matrix[col]]
        for row in range(3):
            if row != col:
                factor = matrix[row][col]
                matrix[row] = [a-factor*b for a,b in zip(matrix[row],matrix[col])]
    dc, a, b = (matrix[n][3] for n in range(3))
    residual = math.sqrt(sum((z-dc-a*v[1]-b*v[2])**2 for v,z in zip(basis,y))/len(y))
    return {'dc': dc, 'amplitude': math.hypot(a,b), 'phase_rad': math.atan2(b,a), 'residual_rms': residual}


def evaluate_frequency(report: dict, rows: list[dict], policy: dict) -> dict:
    c = report['effective_config']; f = c['excitation_frequency_Hz']
    result: dict[str, Any] = {'frequency_Hz': f, 'eligible': False, 'gain': None, 'phase_deg': None}
    if c['trace_divider'] != 1:
        return dict(result, status='INSUFFICIENT_TRACE_RATE')
    begin = .02 + max(.02, 6/f)
    window = [r for r in rows if r['boundary_time_s'] >= begin-1e-12]
    try:
        t = [r['boundary_time_s'] for r in window]
        ref = sine_fit(t, [r['iq_ref_A'] for r in window], f, policy['minimum_cycles'])
        actual = sine_fit(t, [r['boundary_iq_A'] for r in window], f, policy['minimum_cycles'])
    except ValueError as exc:
        return dict(result, status='INSUFFICIENT_WINDOW', reason=str(exc))
    saturation = sum(r['voltage_saturated'] for r in window)/len(window)
    eligible = (report['fault'] == 'none' and report['state'] == 'armed'
                and policy['minimum_reference_amplitude_A'] <= ref['amplitude'] <= policy['maximum_reference_amplitude_A']
                and actual['residual_rms'] <= policy['maximum_residual_rms_A']
                and saturation <= policy['maximum_voltage_saturation_fraction']
                and not any(r['reference_limited'] for r in window))
    phase = (actual['phase_rad']-ref['phase_rad']+math.pi)%(2*math.pi)-math.pi
    result.update(eligible=eligible, gain=actual['amplitude']/ref['amplitude'] if eligible else None,
                  phase_deg=math.degrees(phase) if eligible else None,
                  residual_rms_A=actual['residual_rms'], window_s=[t[0],t[-1]],
                  window_saturation_fraction=saturation,
                  status='ELIGIBLE_SMALL_SIGNAL_WINDOW' if eligible else 'NONLINEAR_OR_FAULTED_WINDOW')
    return result


def bandwidth_bracket(points: list[dict]) -> dict:
    ordered = sorted(points, key=lambda p: p['frequency_Hz'])
    empty = {'bracket_Hz': None, 'estimate_Hz': None}
    if len(ordered) < 2 or not ordered[0]['eligible'] or not ordered[0]['gain']:
        return dict(empty, status='INSUFFICIENT_VALID_FREQUENCIES')
    base = ordered[0]['gain']; threshold = 1/math.sqrt(2)
    eligible = [p for p in ordered if p['eligible'] and p['gain'] is not None]
    peak = max(eligible,key=lambda p:p['gain'])
    resonance = {'peak_gain_relative_dB':20*math.log10(peak['gain']/base), 'peak_frequency_Hz':peak['frequency_Hz']}
    empty.update(resonance)
    for previous, current in zip(ordered, ordered[1:]):
        # Never bridge invalid/saturated/missing intermediate points.
        if not previous['eligible'] or not current['eligible']:
            return dict(empty, status='INVALID_FREQUENCY_GAP')
        a, b = previous['gain']/base, current['gain']/base
        if a > threshold >= b and b > 0:
            fa, fb = previous['frequency_Hz'], current['frequency_Hz']
            w = (math.log(threshold)-math.log(a))/(math.log(b)-math.log(a))
            return {**resonance, 'status': 'FIRST_MINUS_3DB_CROSSING_BRACKET', 'bracket_Hz': [fa,fb],
                    'estimate_Hz': math.exp(math.log(fa)+w*(math.log(fb)-math.log(fa))),
                    'reference_frequency_Hz': ordered[0]['frequency_Hz'],
                    'meaning': 'Log-frequency interpolated estimate relative to the lowest tested frequency, not a stability margin.'}
    return dict(empty, status='NO_CROSSING_IN_TESTED_RANGE')


def evaluate_step(report: dict, rows: list[dict], policy: dict) -> dict:
    if report['effective_config']['trace_divider'] != 1:
        return {'status':'INSUFFICIENT_TRACE_RATE', 'reasons':['Requires every-control-tick trace']}
    if not rows or rows[-1]['time_s']-policy['step_s'] < policy['minimum_horizon_s']-1e-12:
        return {'status':'INSUFFICIENT_HORIZON','reasons':['Post-step observation too short']}
    m = step_metrics(rows, policy['step_s'], policy['target_A'],
                     policy['minimum_tail_hold_s'], policy['settling_band_fraction'])
    reasons = []
    if report['fault'] != 'none' or report['state'] != 'armed':
        return dict(m, status='PROTECTION_TRIP', reasons=[report['fault']])
    if m['observed_horizon_s'] < policy['minimum_horizon_s']-1e-12:
        return dict(m, status='INSUFFICIENT_HORIZON', reasons=['Post-step observation too short'])
    if m['settling_2pct_s'] is None or m['settling_2pct_s'] > policy['maximum_settling_s']:
        reasons.append('settling')
    for field, bound in [('overshoot_pct','maximum_overshoot_pct'), ('tail_rmse_A','maximum_tail_rmse_A')]:
        if m[field] > policy[bound]: reasons.append(field)
    if report['max_abs_phase_current_A'] > policy['maximum_peak_phase_current_A']:reasons.append('peak_current')
    if report['voltage_saturation_fraction'] > policy['maximum_voltage_saturation_fraction']:reasons.append('saturation')
    return dict(m, status='PERFORMANCE_FAIL' if reasons else 'PERFORMANCE_PASS', reasons=reasons)
