#!/usr/bin/env python3
"""Strict, offline dq trace comparison. Not parameter identification or hardware validation.
RMS/line-line/abc traces and motor-side encoders are rejected rather than guessed.
"""
from __future__ import annotations
import argparse
import bisect
import csv
import hashlib
import json
import math
from pathlib import Path
import sys
from bench_analysis import read_trace


def _finite(value: object, label: str) -> float:
    number=float(value)
    if not math.isfinite(number):raise ValueError('Nonfinite '+label)
    return number


def load_measurement(csv_path: str | Path, manifest_path: str | Path) -> tuple[list[dict],dict]:
    csv_path,manifest_path=Path(csv_path),Path(manifest_path)
    metadata=json.loads(manifest_path.read_text(encoding='utf-8'))
    required={'schema','origin','run_id','provenance','current_convention','current_frame','current_unit',
              'time_unit','expected_sample_period_s','time_shift_s','columns'}
    if not required<=metadata.keys():raise ValueError('Missing manifest fields: '+', '.join(sorted(required-metadata.keys())))
    if metadata['schema']!=1 or metadata['origin'] not in ('synthetic','user_declared_measurement'):
        raise ValueError('Unsupported schema or origin')
    if not all(isinstance(metadata[k],str) and metadata[k].strip() for k in ['run_id','provenance']):
        raise ValueError('Run ID and provenance must be explicit')
    if metadata['origin']=='user_declared_measurement' and not metadata.get('hardware_id','').strip():
        raise ValueError('A user-declared measurement requires a hardware_id')
    if metadata['current_convention']!='amplitude_invariant_dq_peak' or metadata['current_frame']!='rotor_electrical':
        raise ValueError('Only rotor-electrical amplitude-invariant dq peak current supported; no implicit RMS/line/abc conversion')
    units={'s':1.,'ms':1e-3,'us':1e-6};amps={'A':1.,'mA':1e-3}
    if metadata['time_unit'] not in units or metadata['current_unit'] not in amps:
        raise ValueError('Unsupported time/current unit')
    columns=metadata['columns']
    if not isinstance(columns,dict) or not {'time','id','iq'}<=columns.keys() or not columns.keys()<={'time','id','iq','position','bus'}:
        raise ValueError('Columns require time,id,iq and optional output position / bus')
    if not all(isinstance(v,str) and v for v in columns.values()) or len(set(columns.values()))!=len(columns):
        raise ValueError('Column mappings must be unique nonempty strings')
    position_scale=1.
    if 'position' in columns:
        if metadata.get('encoder_side')!='output':
            raise ValueError('A motor-side encoder cannot be substituted for output angle in a compliant drivetrain')
        unit=metadata.get('position_unit')
        if unit=='rad':position_scale=1.
        elif unit=='deg':position_scale=math.pi/180
        elif unit=='counts':
            counts=_finite(metadata.get('encoder_counts_per_turn',0),'encoder resolution')
            if counts<=0 or counts!=int(counts):raise ValueError('Positive integer encoder_counts_per_turn required')
            position_scale=2*math.pi/counts
        else:raise ValueError('Unsupported position unit')
    bus_scale={'V':1.,'mV':1e-3}.get(metadata.get('bus_unit'))
    if 'bus' in columns and bus_scale is None:raise ValueError('Explicit bus unit V or mV required')
    period=_finite(metadata['expected_sample_period_s'],'sample period')
    shift=_finite(metadata['time_shift_s'],'time shift')
    if period<=0:raise ValueError('Sample period must be positive')
    if shift and not str(metadata.get('time_shift_reason','')).strip():
        raise ValueError('Nonzero time shift requires a documented reason; no automatic best-fit alignment')
    normalized=[]
    with csv_path.open(newline='',encoding='utf-8-sig') as stream:
        reader=csv.DictReader(stream)
        if not reader.fieldnames or len(set(reader.fieldnames))!=len(reader.fieldnames) or not set(columns.values())<=set(reader.fieldnames):
            raise ValueError('CSV has missing or duplicate headers')
        for n,row in enumerate(reader,2):
            if None in row or any(v is None for v in row.values()):raise ValueError(f'Ragged CSV at line {n}')
            value=lambda name:_finite(row[columns[name]],f'{name} on line {n}')
            sample={'time_s':value('time')*units[metadata['time_unit']]+shift,
                    'id_A':value('id')*amps[metadata['current_unit']],
                    'iq_A':value('iq')*amps[metadata['current_unit']]}
            if 'position' in columns:sample['output_position_rad']=value('position')*position_scale
            if 'bus' in columns:sample['vbus_V']=value('bus')*bus_scale
            normalized.append(sample)
    if len(normalized)<3:raise ValueError('Need at least 3 samples')
    for a,b in zip(normalized,normalized[1:]):
        interval=b['time_s']-a['time_s']
        if interval<=0 or abs(interval/period-1)>.05+1e-10:
            raise ValueError('Non-monotonic, missing, or >5% jitter samples; resampling must be explicit and documented')
    metadata=dict(metadata,csv_sha256=hashlib.sha256(csv_path.read_bytes()).hexdigest(),
                  manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest())
    return normalized,metadata


def compare_measurement(reference: list[dict], measured: list[dict], metadata: dict) -> dict:
    if len(reference)<3 or len(measured)<3:raise ValueError('Insufficient reference or measurement')
    times=[r['time_s'] for r in reference]
    if any(not math.isfinite(t) for t in times) or any(b<=a for a,b in zip(times,times[1:])):
        raise ValueError('Invalid reference timestamps')
    # No extrapolation, automatic cropping, offset fitting, or time-shift fitting.
    if measured[0]['time_s']<times[0]-1e-12 or measured[-1]['time_s']>times[-1]+1e-12:
        raise ValueError('Measurement extends beyond reference coverage; no extrapolation or automatic cropping')
    signals={k:[] for k in measured[0] if k!='time_s'}
    for sample in measured:
        t=sample['time_s'];index=min(max(bisect.bisect_right(times,t)-1,0),len(times)-2)
        a,b=reference[index],reference[index+1];w=(t-times[index])/(times[index+1]-times[index])
        for signal,errors in signals.items():
            if signal not in a or signal not in b:raise ValueError('Reference is missing '+signal)
            expected=(1-w)*a[signal]+w*b[signal]
            error=sample[signal]-expected
            if not math.isfinite(error):raise ValueError('Nonfinite comparison data')
            errors.append(error)
    return {'comparison_status':'COMPUTED','sample_count':len(measured),'data_origin':metadata['origin'],
            'signals':{k:{'rmse':math.sqrt(sum(e*e for e in v)/len(v)),
                          'bias':sum(v)/len(v),'max_abs_error':max(abs(e) for e in v)} for k,v in signals.items()},
            'measurement_binding':metadata,
            'alignment':'Explicit manifest time_shift_s; linear interpolation of reference only',
            'numeric_acceptance':'NOT_SPECIFIED',
            'hardware_validation':'NOT_RUN' if metadata['origin']=='synthetic' else 'NOT_ESTABLISHED_BY_THIS_TOOL'}


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',required=True,type=Path,help='Native qdd_sim CSV or CSV.gz')
    parser.add_argument('--measurement',required=True,type=Path)
    parser.add_argument('--manifest',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--iq-rmse-max',type=float,help='Optional explicit numeric acceptance threshold in A')
    args=parser.parse_args()
    if args.iq_rmse_max is not None and (not math.isfinite(args.iq_rmse_max) or args.iq_rmse_max<=0):
        raise ValueError('RMSE threshold must be positive and finite')
    data,metadata=load_measurement(args.measurement,args.manifest)
    result=compare_measurement(read_trace(args.reference),data,metadata)
    result['reference_sha256']=hashlib.sha256(args.reference.read_bytes()).hexdigest()
    result['tool_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    failed=False
    if args.iq_rmse_max is not None:
        failed=result['signals']['iq_A']['rmse']>args.iq_rmse_max
        result['numeric_acceptance']='FAIL' if failed else 'PASS'
        result['iq_rmse_threshold_A']=args.iq_rmse_max
    args.output.parent.mkdir(parents=True,exist_ok=True)
    # Explicit no-overwrite behavior protects existing comparison evidence.
    with args.output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2,allow_nan=False);f.write('\n')
    print(json.dumps(result,indent=2,allow_nan=False))
    return 4 if failed else 0

if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,KeyError,TypeError) as exc:
        print(f'correlate: {exc}',file=sys.stderr);sys.exit(2)
