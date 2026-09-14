# Electrical-mechanical co-simulation contract

The C++ drive owns sampled FOC, sensing, inverter switching/averaging, electrical motor state, DC-link and thermal state, and independent comparator/BREAK protection. An external mechanics engine owns rotor and load inertia, poses, velocities, gravity, and contacts. Never integrate the same mechanical degree of freedom in both engines.

At a fixed 50 microsecond boundary the mechanics engine supplies rotor/output angle and velocity. C++ substeps the electrical plant and returns mean rotor and output generalized torques. MuJoCo advances its two mechanical DOFs with those torques. Contact changes the next measured state and therefore affects gear reaction, back-EMF, current, and FOC. This is explicit co-simulation, not an ideal torque or position actuator.

A board visualization is a functional model of PWM, ADC, sensing and protection. It is not STM32 instruction-set emulation, a register-accurate BSP, SPICE, PCB verification, or a hardware safety claim. Rendered board geometry is illustrative, not a fabrication drawing.

Native numerical tests, native recorded trajectories, MuJoCo integration tests, actual MuJoCo execution, and hardware validation require distinct evidence. A successful native build or a rendered trajectory does not prove a MuJoCo run. Dependency or tool failures must remain visible.
