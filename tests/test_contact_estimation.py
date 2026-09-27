from pathlib import Path
import math
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "examples" / "mujoco"))
import contact_estimation as ce


class ContactEstimationContracts(unittest.TestCase):
    def test_scalar_link_identification_recovers_excited_model(self):
        truth = ce.ScalarLinkModel(
            inertia=0.031,
            gravity_sin=1.42,
            gravity_cos=-0.11,
            viscous=0.045,
            coulomb=0.028,
            bias=0.013,
        )
        samples = []
        dt = 0.002
        for k in range(1, 2500):
            t = k * dt
            q = 0.48 * math.sin(2.1 * t) + 0.17 * math.sin(5.3 * t)
            qd = 0.48 * 2.1 * math.cos(2.1 * t) + 0.17 * 5.3 * math.cos(5.3 * t)
            qdd = -0.48 * 2.1**2 * math.sin(2.1 * t) - 0.17 * 5.3**2 * math.sin(5.3 * t)
            tau = truth.inertia * qdd + truth.internal_torque(q, qd)
            samples.append((q, qd, qdd, tau))
        fit = ce.fit_scalar_link(samples)
        for field in ("inertia", "gravity_sin", "gravity_cos", "viscous", "coulomb", "bias"):
            self.assertAlmostEqual(getattr(fit, field), getattr(truth, field), places=6)

    def test_discrete_momentum_observer_tracks_external_torque_without_acceleration(self):
        inertia = 0.04
        dt = 0.001
        obs = ce.DiscreteMomentumObserver(inertia, cutoff_hz=15.0, dt=dt)
        qd = 0.0
        estimates = []
        for k in range(600):
            tau = 0.35 * math.sin(0.013 * k)
            external = 0.0 if k < 120 else 0.8
            estimates.append(obs.update(qd, tau, 0.0))
            qd += dt * (tau + external) / inertia
        self.assertLess(abs(estimates[80]), 0.03)
        self.assertLess(abs(estimates[-1] - 0.8), 0.02)

    def test_direct_inverse_dynamics_matches_scalar_convention(self):
        model = ce.ScalarLinkModel(0.04, 1.2, -0.1, 0.02, 0.03, 0.01)
        q, qd, qdd, external = 0.3, -0.7, 2.1, -0.45
        applied = model.inertia * qdd + model.internal_torque(q, qd) - external
        self.assertAlmostEqual(
            ce.direct_inverse_dynamics(model, q, qd, qdd, applied),
            external,
            places=12,
        )

    def test_scalar_force_requires_non_singular_contact_jacobian(self):
        self.assertAlmostEqual(ce.scalar_force_from_torque(3.0, 0.2), 15.0)
        with self.assertRaises(ValueError):
            ce.scalar_force_from_torque(1.0, 0.0)

    def test_detection_metrics_report_delay(self):
        truth = [False] * 5 + [True] * 5
        pred = [False] * 7 + [True] * 3
        m = ce._detection_metrics(truth, pred, 0.001)
        self.assertEqual(m["first_contact_delay_ms"], 2.0)
        self.assertEqual(m["fp"], 0)
        self.assertEqual(m["fn"], 2)


if __name__ == "__main__":
    unittest.main()
