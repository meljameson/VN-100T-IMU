# VN-100T measurement characterization: test plan

## Purpose

Measure how the VN-100T readings behave over time: noise, bias stability, drift,
warm-up, sensitivity to temperature, and how much worse things get on the machine
compared to the bench. The results set realistic expectations for heading and
attitude accuracy underground and show whether any user calibration is worth doing.

## Units under test

| IMU | Model | Serial | Register 26 (rotation) |
|---|---|---|---|
| 1 | VN-100T-CR | 0100110348 | −90° about Y (non-default) |
| 2 | | | |

Before each session, record each unit's configuration with `vn100_info` and
`show` in `vn100_calibration`, and store the output with the data (see Data).

## Equipment

- VN-100T unit(s) and their usual power supply
- PC running the Python logger
- Serial link from the IMU to the PC (direct USB-serial adapter, or Arduino
  pass-through: to be decided)
- Rigid mount: a heavy block or bench away from foot traffic, fans, motors and
  steel that might move
- Spirit level
- Temperature reference if possible (a room thermometer is enough for the bench tests)

## Data collection setup

| Item | Setting |
|---|---|
| Output | Binary, one packet per sample, with CRC |
| Fields | Time since startup, uncompensated mag / accel / gyro, temperature, pressure, yaw / pitch / roll |
| Rate | 200 Hz (100 Hz if the link can't keep up) |
| ASCII async output | Off (register 6 = 0) during logging |
| Baud rate | High enough for the rate with margin: check against the ICD |

Uncompensated data is used so that the attitude filter's corrections don't hide the
sensor behaviour. Yaw, pitch and roll are logged alongside it to show what the
filter makes of that behaviour, which is what the application actually uses. Note
that yaw/pitch/roll are in the body frame set by register 26, while the
uncompensated data is in the sensor (case) frame. Configure the output in RAM only: do not `save` the logging setup
to flash unless the team decides to.

Check the register numbers and binary field selections against the ICD before the
first run.

## Data handling

1. The logger writes the raw bytes from the IMU to `<date>_<imu>_<test>.bin`. This
   survives crashes and power loss; at worst the last packet is lost.
2. The converter checks each packet's CRC, counts dropped and corrupt packets, and
   writes `<same name>.h5` with one dataset per channel.
3. The `.h5` file also stores, as attributes: IMU serial, firmware version, register
   dump, output rate, test ID, start time, operator, location, mounting notes.
4. Keep the `.bin` files. They are the original record and can be reconverted.

## Tests

### T1: Long static run (noise and bias stability)

- Mount the IMU rigidly, level, in a quiet spot. Power on and wait 15 min.
- Log for at least 8 h, ideally 12 h or more (overnight). Nobody touches the setup.
- Note the start and end room temperature.
- Analysis: Allan deviation for each gyro and accel axis, which gives
  - angle random walk (gyro) and velocity random walk (accel), read at τ = 1 s
  - bias instability, from the minimum of the curve
  - rate random walk / drift, from the rising tail
- Also: mean and standard deviation of the magnetometer on each axis.
- Attitude: yaw, pitch and roll against time. The IMU isn't moving, so any change is
  drift or noise. Report the standard deviation of each and the total change in yaw
  over the run (heading drift is usually the largest).

### T2: Warm-up

- Start with the IMU powered off for at least 1 h so it's at room temperature.
- Power on and start logging immediately. Log for 60 min.
- Analysis: bias against time and against internal temperature; time until the bias
  settles within its T1 noise band. Also yaw/pitch/roll against time, and how long
  the attitude takes to settle after power-on.
- Result: how long to wait after power-on before measurements are trusted.

### T3: Turn-on to turn-on repeatability

- Five or more cycles: power on, wait the T2 settle time, log 10 min, power off,
  wait at least 30 min.
- Do not move the IMU between cycles.
- Analysis: mean bias for each cycle; spread of the means across cycles. Same for
  the settled yaw, pitch and roll: the IMU hasn't moved, so they should agree.
- Result: how much the bias changes between startups. Allan variance can't show this,
  and it decides whether a startup gyro bias (register 43) would help.

### T4: Temperature sensitivity

- Repeat a 1–2 h static run at the coldest and warmest temperatures the machine
  sees, or do one long run while the temperature changes slowly.
- Analysis: bias against internal temperature for each axis; slope per °C. Also
  yaw/pitch/roll against temperature.
- Result: how well the factory temperature compensation holds over the operating range.

### T5: Orientation check (register 26)

- With the IMU mounted on the machine as it will be used, put the machine level.
- Log attitude for 2 min; compare pitch and roll to the spirit level.
- Turn the machine (or a mock-up) roughly 90° in yaw and repeat.
- Pass: pitch and roll within a few degrees of level, and the yaw change matches the
  real turn. A reading near ±90° pitch means register 26 doesn't match the mounting.
- Do this for every IMU on the machine.

### T6: On the machine (installed conditions)

- IMU in its final mount on the machine.
- Log 30 min with the machine off, then 30 min with it running but stationary, then
  through a typical working period if possible.
- Analysis: compare noise to T1; spectrum (FFT) of the accel and gyro to find
  vibration frequencies; yaw/pitch/roll noise and heading stability with the machine
  off and running; magnetometer field strength and direction compared to the
  bench values to see the steel's effect.
- Result: whether IMU filtering (register 85) or VPE tuning (registers 36, 38) needs
  changing, and whether hard/soft iron calibration (register 23 or real-time HSI) is
  needed.

## Order and time estimate

| Order | Test | Duration |
|---|---|---|
| 1 | T2 warm-up (gives the settle time the others need) | 1 h |
| 2 | T1 long static | overnight |
| 3 | T3 repeatability | about half a day |
| 4 | T4 temperature | depends on access to hot and cold conditions |
| 5 | T5 orientation check | 1 h per IMU |
| 6 | T6 on the machine | half a day |

Run T1–T4 on each IMU. Results that differ a lot between units are worth flagging.

## Acceptance and comparison

Compare T1 results to the VN-100 datasheet values for gyro and accel noise density
and bias instability (fill in from the datasheet in use):

| Parameter | Datasheet | IMU 1 | IMU 2 |
|---|---|---|---|
| Gyro angle random walk | | | |
| Gyro bias instability | | | |
| Accel velocity random walk | | | |
| Accel bias instability | | | |

A unit much worse than the datasheet on the bench points to a mounting, power or
logging problem before it points to a bad sensor. Check those first.

## Data quality checks for every run

- No dropped or corrupt packets, or fewer than 0.01%. Record the count.
- Timestamps evenly spaced at the set rate.
- Temperature channel present and sensible.
- Configuration recorded and matching what was intended.

## Deliverables

- Raw `.bin` and converted `.h5` file for every run
- Allan deviation plots for each axis and IMU, with extracted values
- Warm-up time, turn-on spread and temperature slopes for each IMU
- Yaw/pitch/roll stability for each IMU: noise, drift over the long run, and
  spread between startups
- Register 26 check result for each IMU
- Short summary with recommendations: warm-up time, any calibration to apply,
  filter settings for the machine

## Open questions

- How does the IMU connect to the PC for logging: direct serial or through the Arduino?
- What temperature range does the machine see underground?
- How many IMUs are on the machine, and where is each mounted?
- What accuracy does the application need for heading, pitch and roll? This sets
  what counts as good enough.
