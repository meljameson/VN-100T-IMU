# VN-100 calibration settings

What every value printed by `vn100_calibration.ino` means, and when to change it.
All of it comes from the VN-100 ICD v3.1.0.0 (`VN100 ICD v3_1_0_0 (ICD10005-R1).pdf`),
which matches the firmware on our units (3.1.0.0). Section numbers below are the ICD's.
The ICD lists the attitude-filter (VPE) options by name only and points to VectorNav's
User Manual for how they behave.

## Saving: RAM vs flash

- `set` writes the IMU's RAM copy. It applies right away (except register 26) and is
  lost at the next power cycle or `reset`.
- `save` (`$VNWNV`, 1.3.3) writes every configuration register to flash, not just what
  you changed. From then on those values load at every startup.
  - It also saves anything other firmware changed in RAM since power-on, e.g. `nav` turning
    off ASCII output or `nav_revA` switching on CRC checksums. Power-cycle the IMU before
    a session you mean to save.
  - Keep the IMU still while saving (~0.5–1 s). Otherwise the ICD says to reset it
    afterwards so its attitude filter doesn't diverge.
- `reset` (`$VNRST`, 1.3.5) reboots the IMU and discards unsaved changes. It's the undo.
- Factory restore (`$VNRFS`, 1.3.4) isn't offered by the sketch. It resets all saved settings,
  including baud rate and output setup, though not VectorNav's factory sensor calibration.

## Two kinds of calibration

1. Factory calibration. VectorNav's, stored in the IMU, not visible or changeable here.
   Nothing on this page undoes it.
2. User calibration (registers 23, 25, 84). Applied on top of the factory calibration.
   Identity matrix and zero bias (the defaults) means "no extra correction".

The 3×3 + bias registers (23, 25, 84, and 47) all use the same model, in the sensor frame:

```text
corrected = C * (measured - B)

C = | C00 C01 C02 |     B = | BiasX |
    | C10 C11 C12 |         | BiasY |
    | C20 C21 C22 |         | BiasZ |
```

`C` fixes scale and cross-axis coupling, `B` removes a constant offset.

## The registers

### 21: Magnetic and gravity reference vectors (3.3.1)

| Field | Unit | Default | Meaning |
| --- | --- | --- | --- |
| MagRefN / E / D | gauss | 0.234 / 0 / 0.4212 | Earth's magnetic field where the IMU is, in North-East-Down |
| GravRefN / E / D | m/s² | 0 / 0 / −9.79375 | Gravity where the IMU is, North-East-Down |

The attitude filter compares the magnetometer and accelerometer against these to work out
heading, pitch and roll. Our units still have VectorNav's defaults (VectorNav is in Dallas, TX).

When to change: for the magnetic field, set it to the local field at the dig site from a
magnetic model, e.g. NOAA's calculator, converting nT to gauss (÷100,000). This only matters
when heading comes from the magnetometer (register 35). Gravity barely varies so leave it.

These are the user values. If the IMU's built-in Earth models are turned on (register 83, not
shown by the sketch, so off by default), the IMU computes these itself and they become read-only.

### 23: Magnetometer calibration (3.4.1)

Hard- and soft-iron correction, in the model above (units: gauss for the bias).

- Hard iron (`Bias`): a constant extra field from magnetized steel or currents near the
  IMU, which shifts every reading by the same amount.
- Soft iron (`C`): nearby steel bending the Earth's field, which stretches readings
  differently per direction.

When to change: after the IMU is mounted in its final place, if the magnetometer is used
for heading. Inside a steel machine both effects will be large. The values come from a
calibration routine (rotating the mounted IMU through many orientations and fitting), not
from guessing. The real-time estimator (registers 44/47) does the same fit live.

### 25: Accelerometer calibration (3.4.2)

Same model, bias in m/s². The ICD warns: user bias calibration is "only recommended in
certain circumstances", and changing the gain matrix "is never recommended or required". The
factory calibration already covers this. Leave it at identity / zero unless a measured,
repeatable offset shows up after mounting.

### 84: Gyro calibration (3.4.5)

Same model, bias in rad/s, same ICD warning as register 25. For gyro drift, register 43 and
the filter's own bias estimate are the usual tools, not this.

### 26: Reference frame rotation (3.4.3)

A rotation matrix from the sensor frame (the axes printed on the IMU case) to the
body frame (the machine's axes):

```text
| X |          | R00 R01 R02 |   | X |
| Y |        = | R10 R11 R12 | * | Y |
| Z | body     | R20 R21 R22 |   | Z | sensor
```

It lets the IMU be mounted in any orientation and still report the machine's yaw, pitch and
roll, and its gyro and accel along the machine's axes. It must be a proper rotation
(orthonormal, right-handed, every entry between −1 and 1). The IMU rejects anything else.

Static register: a change only takes effect after `save` and then `reset` or a power
cycle.

Our IMU 1 (VN-100T-CR, serial 0100110348) has a non-default rotation saved:

```text
R = |  0  0 -1 |
    |  0  1  0 |      a −90° rotation about Y
    |  1  0  0 |
```

So body X = −sensor Z, body Y = sensor Y, body Z = sensor X: the IMU is configured as mounted
on its side, with its X axis pointing along the machine's down axis. Someone set this on
purpose, or it came from VectorNav that way. Either way, every IMU on the machine needs the
rotation that matches how it is actually mounted, or their readings won't agree. Check the
other units with `imu 2`, then `show 26`. A unit with the default identity matrix reports
along its own case axes.

### 35: VPE basic control (3.3.2)

The VPE (VectorNav's attitude filter) settings.

| Field | Values | Ours |
| --- | --- | --- |
| Reserved | must stay 1 | 1 |
| HeadingMode | 0 Absolute, 1 Relative, 2 Indoor/Auto | 1 Relative (default) |
| FilteringMode | 0 Unfiltered, 1 Adaptively filtered | 1 (default) |
| TuningMode | 0 Static tuning, 1 Adaptive tuning | 1 (default) |

- HeadingMode decides how much heading relies on the magnetometer. Absolute trusts it
  for true magnetic heading. Relative gives a stable heading that isn't tied to magnetic
  north. Indoor is meant for magnetically disturbed places. Underground in a steel machine
  the field is badly disturbed, so absolute magnetic heading is unlikely to be trustworthy.
  The ICD defers the details of each mode to the User Manual.
- FilteringMode / TuningMode let the filter adapt to disturbances on its own. Leave them on.

### 36: VPE magnetometer basic tuning (3.3.3)

| Field | Default | Meaning |
| --- | --- | --- |
| BaseX/Y/Z | 4 | How much the filter trusts the magnetometer when undisturbed. Higher = better heading accuracy but more sensitive to magnetic interference. |
| AdaptTuneX/Y/Z | 5 | Level of adaptive tuning |
| AdaptFiltX/Y/Z | 5.5 | Level of adaptive filtering |

### 38: VPE accelerometer basic tuning (3.3.4)

Same fields for the accelerometer (defaults 6 / 3 / 5). A higher base number gives better
pitch/roll accuracy but more sensitivity to motion, vibration and shocks. A boring machine
vibrates, so this is the one to experiment with if pitch/roll are noisy while cutting, and
only in small steps with before/after data.

### 43: Filter startup gyro bias (3.3.5)

Where the filter's gyro-bias estimate starts at power-on (rad/s, body frame). All zero on
ours, meaning it starts from nothing and learns the bias in the first moments after boot.

To fill it: let the IMU sit still and warm up until the bias estimate settles, send
`$VNSGB` (Set Gyro Bias, 1.3.10: copies the current estimate here), then `save`. The sketch
doesn't send `$VNSGB`. You can `set 43 x,y,z` if you have the numbers. This only helps
startup convergence.

### 85: IMU filtering configuration (3.4.6)

A moving-average (boxcar) filter on the output measurements only. It does not change what
the attitude filter uses.

| Field | Ours | Meaning |
| --- | --- | --- |
| MagWindow / AccelWindow / GyroWindow / TempWindow / PresWindow | 0 / 4 / 4 / 4 / 0 | How many of the latest samples (at the IMU's internal rate, nominally 800 Hz) are averaged. 4 samples = 5 ms. 0 = off. |
| MagMode / AccelMode / GyroMode / TempMode / PresMode | 0 / 3 / 3 / 3 / 0 | Which outputs the averaging applies to, as bits: 1 = uncompensated outputs, 2 = compensated, 3 = both, 0 = neither. |

These are the defaults. Our 50 Hz output already samples far slower than this window.

### 44: Real-time HSI control (3.5.1)

The IMU's built-in, live hard/soft-iron estimator (same job as register 23, done
continuously).

| Field | Values | Ours |
| --- | --- | --- |
| Mode | 0 Off, 1 Run, 2 Reset (clears the solution) | 0 Off |
| ApplyComp | 1 Don't apply, 3 Apply the solution to the magnetometer | 1 Don't apply |
| ConvergeRate | 1 (slow, ~60–90 s, more accurate) to 5 (fast, ~15–20 s) | 5 |

Turning `Mode` off doesn't clear the solution, only `2` (Reset) does.

When to use: `set 44 1,1,5` to let it learn while you slowly rotate the mounted IMU
through as many orientations as you can, watch register 47, then `set 44 0,3,5` to freeze
and apply it, and `save` to keep it. Only worth doing if the magnetometer is used for
heading (register 35).

### 47: Real-time HSI results (4.5.1, read-only)

The estimator's current solution, in the same `C * (measured − B)` model as register 23,
applied on top of register 23 when register 44's ApplyComp is 3. Identity / zero (ours)
means it hasn't learned anything.
