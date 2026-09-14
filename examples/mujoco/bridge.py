"""Thin ctypes binding to the native electrical/drive co-simulation boundary."""
from __future__ import annotations
import ctypes as C
from pathlib import Path

class Input(C.Structure):
    _fields_ = [(n,C.c_double) for n in (
        'rotor_angle','rotor_speed','output_angle','output_speed','position_ref',
        'velocity_ref','torque_ref','iq_ref','kp','kd')] + [
        (n,C.c_int32) for n in ('mode','enable','inject_driver_fault')]

class Output(C.Structure):
    _fields_ = [(n,C.c_double) for n in (
        'time_s','rotor_torque','output_torque','id','iq','iq_ref','vbus',
        'winding_c','fet_c','duty_a','duty_b','duty_c','phase_peak','current_age','encoder_age')]+[
        (n,C.c_int32) for n in ('state','fault','gate_enabled','break_latched')]

class Drive:
    """One context per motor. Allocate once, tick at 20 kHz, close explicitly."""
    def __init__(self, library: Path | None = None, *, switched=False, predictive=False):
        root=Path(__file__).resolve().parents[2]
        if library is None:
            names=['libqdd_external.so','libqdd_external.dylib','qdd_external.dll']
            library=next((p for name in names for p in [root/'build'/name,root/'build'/'Release'/name] if p.is_file()),None)
        if library is None:
            raise FileNotFoundError('Build the qdd_external shared library first; see README.md')
        self.path=Path(library).resolve();self.lib=C.CDLL(str(self.path))
        self.lib.qdd_external_abi_version.restype=C.c_uint32
        if self.lib.qdd_external_abi_version()!=1:raise RuntimeError('Unsupported QDD external ABI')
        self.lib.qdd_external_create.argtypes=[C.c_int32,C.c_int32];self.lib.qdd_external_create.restype=C.c_void_p
        self.lib.qdd_external_tick.argtypes=[C.c_void_p,C.POINTER(Input),C.POINTER(Output)]
        self.lib.qdd_external_tick.restype=C.c_int
        self.lib.qdd_external_destroy.argtypes=[C.c_void_p];self.lib.qdd_external_destroy.restype=None
        self.lib.qdd_external_period.restype=C.c_double
        self.period=self.lib.qdd_external_period()
        self.handle=self.lib.qdd_external_create(int(switched),int(predictive))
        if not self.handle:raise RuntimeError('Invalid configuration or native allocation failure')
    def tick(self, inp: Input) -> Output:
        if not self.handle:raise RuntimeError('Drive is closed')
        out=Output()
        if self.lib.qdd_external_tick(self.handle,C.byref(inp),C.byref(out))!=0:
            raise RuntimeError('Native co-simulation input rejected; context remains inhibited')
        return out
    def close(self):
        if self.handle:self.lib.qdd_external_destroy(self.handle);self.handle=None
    def __enter__(self):return self
    def __exit__(self,*args):self.close()
