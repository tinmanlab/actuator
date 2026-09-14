"""Select a bounded physical event from a native MuJoCo trace, in simulation seconds."""
import math

def event_window(case, rows):
    columns = {'tracking': 'output_rad', 'disturbance': 'disturbance_Nm',
               'contact': 'intended_contact_force_N', 'impact': 'intended_contact_force_N', 'fault': 'fault'}
    if case not in columns or len(rows) < 2:
        raise ValueError('Unknown case or empty trace')
    try:
        times = [float(r['time_s']) for r in rows]
        values = [float(r[columns[case]]) for r in rows]
    except (KeyError, TypeError, ValueError) as exc:
        raise ValueError('Invalid event trace') from exc
    if not all(math.isfinite(x) for x in times + values) or any(b <= a for a, b in zip(times, times[1:])):
        raise ValueError('Trace must be finite with increasing time')
    threshold = .01 if case == 'tracking' else .1
    baseline = values[0] if case == 'tracking' else 0
    indices = [i for i, v in enumerate(values) if abs(v - baseline) > threshold]
    if not indices:
        raise ValueError(f'{case}: required event absent')
    event = times[indices[0]]
    duration = .6 if case in ('impact', 'fault') else .45
    start = max(times[0], event - .10)
    end = min(times[-1], start + duration)
    if not start <= event < end or end - start < .1:
        raise ValueError('Insufficient post-event recording')
    return start, end, event
