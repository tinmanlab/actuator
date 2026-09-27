#!/usr/bin/env python3
"""Scalar link identification and MIT-style torque-contact observer experiment.

This is a deliberately small bridge between the realistic QDD actuator model and
whole-body contact estimation.  The native drive owns current control, inverter,
motor and transmission torque. MuJoCo owns rotor/link inertia, gravity and
contact.  The estimator never uses MuJoCo contact forces as an input; those are
truth signals used only for evaluation.

The scalar observer is the constant-inertia special case of Bledt et al.,
"Contact Model Fusion for Event-Based Locomotion in Unstructured Terrains",
ICRA 2018, Eq. (10).  It estimates external generalized torque. Recovering a
Cartesian force additionally requires a known contact point/direction and a
non-singular contact Jacobian.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path
import argparse
import csv
import json
import math
import sys


def _sign(x: float, eps: float = 1e-9) -> float:
    return 1.0 if x > eps else (-1.0 if x < -eps else 0.0)


def _solve_square(a: list[list[float]], b: list[float]) -> list[float]:
    """Small dense Gaussian elimination with partial pivoting."""
    n = len(b)
    m = [list(a[r]) + [b[r]] for r in range(n)]
    for col in range(n):
        pivot = max(range(col, n), key=lambda r: abs(m[r][col]))
        if abs(m[pivot][col]) < 1e-12:
            raise ValueError("singular identification normal matrix")
        m[col], m[pivot] = m[pivot], m[col]
        div = m[col][col]
        for j in range(col, n + 1):
            m[col][j] /= div
        for r in range(n):
            if r == col:
                continue
            factor = m[r][col]
            for j in range(col, n + 1):
                m[r][j] -= factor * m[col][j]
    return [m[r][n] for r in range(n)]


@dataclass(frozen=True)
class ScalarLinkModel:
    """Lumped 1-DoF rigid-link model.

    tau_drive = I*qdd + gs*sin(q) + gc*cos(q)
                + b*qd + fc*sign(qd) + bias + tau_ext

    The sign convention matches MuJoCo's generalized force at the output hinge.
    """

    inertia: float
    gravity_sin: float
    gravity_cos: float
    viscous: float
    coulomb: float
    bias: float

    def internal_torque(self, q: float, qd: float) -> float:
        return (
            self.gravity_sin * math.sin(q)
            + self.gravity_cos * math.cos(q)
            + self.viscous * qd
            + self.coulomb * _sign(qd)
            + self.bias
        )


def fit_scalar_link(samples: list[tuple[float, float, float, float]]) -> ScalarLinkModel:
    """Least-squares fit from (q, qd, qdd, applied_tau) free-space samples."""
    if len(samples) < 12:
        raise ValueError("at least 12 excitation samples are required")
    normal = [[0.0] * 6 for _ in range(6)]
    rhs = [0.0] * 6
    for q, qd, qdd, tau in samples:
        row = [qdd, math.sin(q), math.cos(q), qd, _sign(qd), 1.0]
        for i in range(6):
            rhs[i] += row[i] * tau
            for j in range(6):
                normal[i][j] += row[i] * row[j]
    # Tiny scale-aware ridge regularization avoids accidental singularity around
    # zero speed without materially changing an excited dataset.
    trace = sum(normal[i][i] for i in range(6))
    ridge = max(1e-12, trace * 1e-12)
    for i in range(6):
        normal[i][i] += ridge
    x = _solve_square(normal, rhs)
    if not all(math.isfinite(v) for v in x) or x[0] <= 0:
        raise ValueError("identified model is non-physical")
    return ScalarLinkModel(*x)


class DiscreteMomentumObserver:
    """Bledt et al. Eq. (10), specialized to one constant-inertia DoF.

    Dynamics convention:
        I*qdd + h(q,qd) = tau_applied + tau_external

    For cutoff lambda and sample dt:
        gamma = exp(-lambda*dt)
        beta  = (1-gamma)/(gamma*dt)
        tau_hat = beta*p_k - LPF(beta*p + tau_applied - h)

    where p = I*qd and LPF(x)_k=(1-gamma)x_k+gamma*LPF(x)_{k-1}.
    """

    def __init__(self, inertia: float, cutoff_hz: float, dt: float):
        if not (math.isfinite(inertia) and inertia > 0):
            raise ValueError("inertia must be positive")
        if not (math.isfinite(cutoff_hz) and cutoff_hz > 0):
            raise ValueError("cutoff must be positive")
        if not (math.isfinite(dt) and dt > 0):
            raise ValueError("dt must be positive")
        self.inertia = inertia
        self.dt = dt
        self.lam = 2.0 * math.pi * cutoff_hz
        self.gamma = math.exp(-self.lam * dt)
        self.beta = (1.0 - self.gamma) / (self.gamma * dt)
        self.filtered: float | None = None

    def reset(self) -> None:
        self.filtered = None

    def update(self, qd: float, applied_tau: float, internal_tau: float) -> float:
        p = self.inertia * qd
        x = self.beta * p + applied_tau - internal_tau
        if self.filtered is None:
            # Start from a zero-residual state instead of creating an artificial
            # contact pulse when attaching the observer to a non-zero state.
            self.filtered = self.beta * p
            return 0.0
        self.filtered = (1.0 - self.gamma) * x + self.gamma * self.filtered
        return self.beta * p - self.filtered


class EulerMomentumObserver:
    """Classical continuous GM observer integrated with forward Euler.

    Included only as the comparison baseline used by the MIT paper; the fully
    discrete observer above is the implementation candidate.
    """

    def __init__(self, inertia: float, cutoff_hz: float, dt: float):
        if inertia <= 0 or cutoff_hz <= 0 or dt <= 0:
            raise ValueError("observer parameters must be positive")
        self.inertia = inertia
        self.dt = dt
        self.lam = 2.0 * math.pi * cutoff_hz
        self.integral = 0.0
        self.first = True

    def reset(self) -> None:
        self.integral = 0.0
        self.first = True

    def update(self, qd: float, applied_tau: float, internal_tau: float) -> float:
        p = self.inertia * qd
        if self.first:
            self.integral = self.lam * p
            self.first = False
            return 0.0
        estimate = self.lam * p - self.integral
        self.integral += self.dt * self.lam * (applied_tau - internal_tau + estimate)
        return estimate


def direct_inverse_dynamics(
    model: ScalarLinkModel, q: float, qd: float, qdd: float, applied_tau: float
) -> float:
    return model.inertia * qdd + model.internal_torque(q, qd) - applied_tau


def scalar_force_from_torque(tau_external: float, normal_jacobian: float) -> float:
    """Recover a 1-D normal force only when the contact normal Jacobian is known."""
    if abs(normal_jacobian) < 1e-6:
        raise ValueError("contact Jacobian is singular for scalar force recovery")
    return tau_external / normal_jacobian


def _rms(values: list[float]) -> float:
    return math.sqrt(sum(v * v for v in values) / max(1, len(values)))


def _correlation(a: list[float], b: list[float]) -> float:
    if len(a) != len(b) or len(a) < 2:
        return 0.0
    ma = sum(a) / len(a)
    mb = sum(b) / len(b)
    da = [x - ma for x in a]
    db = [x - mb for x in b]
    den = math.sqrt(sum(x * x for x in da) * sum(x * x for x in db))
    return 0.0 if den < 1e-15 else sum(x * y for x, y in zip(da, db)) / den


def _detection_metrics(truth: list[bool], predicted: list[bool], dt: float) -> dict:
    tp = sum(t and p for t, p in zip(truth, predicted))
    fp = sum((not t) and p for t, p in zip(truth, predicted))
    fn = sum(t and (not p) for t, p in zip(truth, predicted))
    tn = sum((not t) and (not p) for t, p in zip(truth, predicted))
    precision = tp / max(1, tp + fp)
    recall = tp / max(1, tp + fn)
    accuracy = (tp + tn) / max(1, len(truth))
    try:
        t0 = truth.index(True)
        p0 = next(i for i in range(t0, len(predicted)) if predicted[i])
        delay_ms = max(0.0, (p0 - t0) * dt * 1000.0)
    except (ValueError, StopIteration):
        delay_ms = None
    return dict(
        tp=tp, fp=fp, fn=fn, tn=tn, precision=precision, recall=recall,
        accuracy=accuracy, first_contact_delay_ms=delay_ms
    )


def _run_actual_engine(output: Path, library: Path | None, cutoff_hz: float, sample_hz: int) -> dict:
    """Run two actual MuJoCo lanes: free-space identification then contact."""
    try:
        import mujoco
        import numpy as np
    except ImportError as exc:
        return dict(status="NOT_RUN", accepted=False, reason=str(exc))

    # Import the existing native bridge only after engine dependencies exist.
    from bridge import Drive, Input

    root = Path(__file__).resolve().parents[2]
    xml = Path(__file__).with_name("bench.xml")
    if not xml.exists():
        return dict(status="NOT_RUN", accepted=False, reason="generated examples/mujoco/bench.xml is missing")

    output.mkdir(parents=True, exist_ok=False)
    period = 50e-6
    stride = max(1, round(1.0 / (sample_hz * period)))
    dt = stride * period

    def ids(model):
        def ident(kind, name):
            i = mujoco.mj_name2id(model, kind, name)
            if i < 0:
                raise ValueError("model is missing " + name)
            return i
        rotor = ident(mujoco.mjtObj.mjOBJ_JOINT, "rotor_joint")
        out_joint = ident(mujoco.mjtObj.mjOBJ_JOINT, "output_joint")
        return (
            int(model.jnt_qposadr[rotor]), int(model.jnt_dofadr[rotor]),
            int(model.jnt_qposadr[out_joint]), int(model.jnt_dofadr[out_joint]),
            ident(mujoco.mjtObj.mjOBJ_BODY, "output"),
        )

    def set_active(model, names, active):
        for name in names:
            g = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_GEOM, name)
            if g >= 0:
                model.geom_contype[g] = 1 if active else 0
                model.geom_conaffinity[g] = 1 if active else 0
                model.geom_rgba[g, 3] = 1.0 if active else 0.0

    def run_lane(contact: bool, duration: float):
        model = mujoco.MjModel.from_xml_path(str(xml))
        data = mujoco.MjData(model)
        rq, rd, oq, od, output_body = ids(model)
        set_active(model, ("stopper", "stop_foot", "stop_column"), contact)
        set_active(model, ("ball",), False)
        ball_body = mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_BODY, "impact_ball")
        if ball_body >= 0:
            model.body_gravcomp[ball_body] = 1.0
        mujoco.mj_forward(model, data)
        full_mass = np.zeros((model.nv, model.nv))
        mujoco.mj_fullM(model, full_mass, data.qM)
        inertia_truth = float(full_mass[od, od])
        rows = []
        trace = []
        with Drive(library) as drive:
            if abs(drive.period - period) > 1e-12:
                raise RuntimeError("unexpected native co-simulation period")
            ticks = round(duration / period)
            for tick in range(ticks):
                t = tick * period
                if contact:
                    target = 0.0 if t < 0.12 else 0.85
                else:
                    local = max(0.0, t - 0.12)
                    target = 0.0 if t < 0.12 else (
                        0.34 * math.sin(2 * math.pi * 0.75 * local)
                        + 0.16 * math.sin(2 * math.pi * 1.85 * local)
                    )
                inp = Input(
                    float(data.qpos[rq]), float(data.qvel[rd]),
                    float(data.qpos[oq]), float(data.qvel[od]),
                    target, 0.0, 0.0, 0.0, 28.0, 2.0, 4, 1, 0
                )
                drive_out = drive.tick(inp)
                data.qfrc_applied[rd] = drive_out.rotor_torque
                data.qfrc_applied[od] = drive_out.output_torque
                mujoco.mj_step(model, data)
                mujoco.mj_forward(model, data)

                if tick % stride:
                    continue
                intended = False
                normal_force = None
                normal_jacobian = None
                for j in range(data.ncon):
                    c = data.contact[j]
                    names = [
                        mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_GEOM, int(g))
                        for g in (c.geom1, c.geom2)
                    ]
                    if "stopper" not in names or not any(n in names for n in ("link", "tip")):
                        continue
                    intended = True
                    wrench = np.zeros(6)
                    mujoco.mj_contactForce(model, data, j, wrench)
                    normal_force = float(wrench[0])
                    jacp = np.zeros((3, model.nv))
                    jacr = np.zeros((3, model.nv))
                    pos = np.array(c.pos, dtype=float)
                    mujoco.mj_jac(model, data, jacp, jacr, pos, output_body)
                    normal = np.array(c.frame[:3], dtype=float)
                    normal_jacobian = float(normal @ jacp[:, od])
                    break
                row = dict(
                    t=float(data.time),
                    q=float(data.qpos[oq]),
                    qd=float(data.qvel[od]),
                    qdd_truth=float(data.qacc[od]),
                    tau=float(drive_out.output_torque),
                    tau_external_truth=float(data.qfrc_constraint[od]),
                    intended_contact=bool(intended),
                    normal_force_truth_N=normal_force,
                    normal_jacobian=normal_jacobian,
                )
                trace.append(row)
                if not contact and t > 0.28 and abs(row["qd"]) > 0.015:
                    rows.append((row["q"], row["qd"], row["qdd_truth"], row["tau"]))
        return inertia_truth, rows, trace

    inertia_truth, fit_rows, free_trace = run_lane(False, 3.2)
    model = fit_scalar_link(fit_rows)
    _, _, contact_trace = run_lane(True, 2.0)

    # Evaluate the identified model on the free-space lane.
    free_errors = [
        direct_inverse_dynamics(model, r["q"], r["qd"], r["qdd_truth"], r["tau"])
        for r in free_trace if r["t"] > 0.28
    ]

    discrete = DiscreteMomentumObserver(model.inertia, cutoff_hz, dt)
    classical = EulerMomentumObserver(model.inertia, cutoff_hz, dt)
    direct_prev_qd = None
    estimates = []
    truth = []
    classical_est = []
    direct_est = []
    contact_truth = []
    force_est = []
    force_truth = []

    for r in contact_trace:
        h = model.internal_torque(r["q"], r["qd"])
        de = discrete.update(r["qd"], r["tau"], h)
        ce = classical.update(r["qd"], r["tau"], h)
        if direct_prev_qd is None:
            ide = 0.0
        else:
            qdd_fd = (r["qd"] - direct_prev_qd) / dt
            ide = direct_inverse_dynamics(model, r["q"], r["qd"], qdd_fd, r["tau"])
        direct_prev_qd = r["qd"]
        estimates.append(de)
        classical_est.append(ce)
        direct_est.append(ide)
        truth.append(r["tau_external_truth"])
        contact_truth.append(r["intended_contact"])
        jn = r["normal_jacobian"]
        fn = r["normal_force_truth_N"]
        if r["intended_contact"] and jn is not None and fn is not None and abs(jn) > 1e-5:
            force_est.append(abs(scalar_force_from_torque(de, jn)))
            force_truth.append(abs(fn))

    # Estimate a contact threshold from the no-contact residual instead of
    # tuning it on the contact lane.
    noise_rms = _rms(free_errors)
    threshold = max(0.08, 5.0 * noise_rms)
    predicted_contact = [abs(x) >= threshold for x in estimates]
    detection = _detection_metrics(contact_truth, predicted_contact, dt)

    active = [i for i, c in enumerate(contact_truth) if c]
    contact_rmse = _rms([estimates[i] - truth[i] for i in active])
    classical_rmse = _rms([classical_est[i] - truth[i] for i in active])
    direct_rmse = _rms([direct_est[i] - truth[i] for i in active])
    force_rmse = _rms([a - b for a, b in zip(force_est, force_truth)]) if force_est else None
    inertia_rel = abs(model.inertia - inertia_truth) / inertia_truth

    with (output / "contact_trace.csv").open("w", newline="") as f:
        fields = list(contact_trace[0]) + [
            "tau_hat_discrete_Nm", "tau_hat_classical_Nm", "tau_hat_direct_Nm", "detected_contact"
        ]
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r, de, ce, ie, dc in zip(contact_trace, estimates, classical_est, direct_est, predicted_contact):
            row = dict(r)
            row.update(
                tau_hat_discrete_Nm=de,
                tau_hat_classical_Nm=ce,
                tau_hat_direct_Nm=ie,
                detected_contact=dc,
            )
            w.writerow(row)

    report = dict(
        status="EXECUTED",
        engine="MuJoCo",
        engine_version=mujoco.__version__,
        accepted=False,
        sample_hz=1.0 / dt,
        cutoff_hz=cutoff_hz,
        identified_model=asdict(model),
        truth_output_inertia_kg_m2=inertia_truth,
        inertia_relative_error=inertia_rel,
        free_space_residual_rms_Nm=noise_rms,
        contact_torque_rmse_Nm=contact_rmse,
        classical_observer_contact_rmse_Nm=classical_rmse,
        finite_difference_contact_rmse_Nm=direct_rmse,
        contact_torque_correlation=_correlation(estimates, truth),
        scalar_normal_force_rmse_N=force_rmse,
        scalar_force_samples=len(force_est),
        threshold_Nm=threshold,
        detection=detection,
        claim_boundary=(
            "Primary observable is generalized external joint torque. Scalar normal force is "
            "reported only when the intended contact point/direction gives a non-singular "
            "normal Jacobian; this is not a general 3-D wrench estimator. The lane reproduces "
            "the MIT force-estimation channel, not its gait/height/contact-probability fusion."
        ),
        identification_boundary=(
            "The first lane identifies a lumped one-DoF rigid-link model from contact-free "
            "excitation using MuJoCo acceleration truth. Contact-rich whole-body identification "
            "requires constraint-nullspace projection such as Khorshidi et al. ICRA 2025."
        ),
    )
    # Bounded simulation acceptance, intentionally not a hardware-performance claim.
    delay = detection["first_contact_delay_ms"]
    report["accepted"] = bool(
        inertia_rel < 0.08
        and noise_rms < 0.20
        and active
        and math.isfinite(contact_rmse)
        and _correlation(estimates, truth) > 0.75
        and detection["recall"] > 0.85
        and detection["precision"] > 0.85
        and delay is not None and delay < 35.0
    )
    (output / "summary.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    return report


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, default=Path("results/contact_estimation"))
    p.add_argument("--library", type=Path)
    p.add_argument("--cutoff-hz", type=float, default=15.0)
    p.add_argument("--sample-hz", type=int, default=1000)
    a = p.parse_args()
    if a.cutoff_hz <= 0 or a.sample_hz <= 0:
        p.error("cutoff and sample rate must be positive")
    report = _run_actual_engine(a.output, a.library, a.cutoff_hz, a.sample_hz)
    if report.get("status") != "EXECUTED":
        a.output.mkdir(parents=True, exist_ok=True)
        (a.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2, allow_nan=False))
    return 0 if report.get("accepted") else (2 if report.get("status") == "NOT_RUN" else 4)


if __name__ == "__main__":
    sys.exit(main())
