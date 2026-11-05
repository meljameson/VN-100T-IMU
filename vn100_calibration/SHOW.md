> show

=== IMU 1 (Serial1, RX 0 / TX 1) calibration ===

Magnetic and gravity reference vectors
  MagRefN G             0.234  MagRefE G                 0  MagRefD G            0.4212
  GravRefN m/s2             0  GravRefE m/s2             0  GravRefD m/s2      -9.79375
  The field and gravity the attitude filter expects where the machine is.

Magnetometer calibration (user hard/soft iron)
  C00                       1  C01                       0  C02                       0
  C10                       0  C11                       1  C12                       0
  C20                       0  C21                       0  C22                       1
  BiasX G                   0  BiasY G                   0  BiasZ G                   0
  Offline hard/soft iron correction, on top of the factory calibration.

Accelerometer calibration (user)
  C00                       1  C01                       0  C02                       0
  C10                       0  C11                       1  C12                       0
  C20                       0  C21                       0  C22                       1
  BiasX m/s2                0  BiasY m/s2                0  BiasZ m/s2                0
  Applied on top of the factory calibration. ICD: user bias calibration only in certain
  circumstances; changing the gain (scale/cross-axis) matrix is never recommended.

Gyro calibration (user)
  C00                       1  C01                       0  C02                       0
  C10                       0  C11                       1  C12                       0
  C20                       0  C21                       0  C22                       1
  BiasX rad/s               0  BiasY rad/s               0  BiasZ rad/s               0
  Applied on top of the factory calibration. ICD: user bias calibration only in certain
  circumstances; changing the gain (scale/cross-axis) matrix is never recommended.

Reference frame rotation
  R00                       0  R01                       0  R02                      -1
  R10                       0  R11                       1  R12                       0
  R20                       1  R21                       0  R22                       0
  Mounting rotation (a rotation matrix). Static: takes effect only after save, then reset.

VPE basic control
  Reserved                  1  HeadingMode               1  FilteringMode             1
  TuningMode                1
  Reserved must stay at its current value.

VPE magnetometer basic tuning
  BaseX                     4  BaseY                     4  BaseZ                     4
  AdaptTuneX                5  AdaptTuneY                5  AdaptTuneZ                5
  AdaptFiltX              5.5  AdaptFiltY              5.5  AdaptFiltZ              5.5

VPE accelerometer basic tuning
  BaseX                     6  BaseY                     6  BaseZ                     6
  AdaptTuneX                3  AdaptTuneY                3  AdaptTuneZ                3
  AdaptFiltX                5  AdaptFiltY                5  AdaptFiltZ                5

Filter startup gyro bias
  X rad/s          +00.000000  Y rad/s          +00.000000  Z rad/s          +00.000000
  Where the attitude filter's gyro bias estimate starts at power-on (needs save to matter).

IMU filtering configuration
  MagWindow                 0  AccelWindow               4  GyroWindow                4
  TempWindow                4  PresWindow                0  MagMode                   0
  AccelMode                 3  GyroMode                  3  TempMode                  3
  PresMode                  0

Real-time HSI control
  Mode                      0  ApplyComp                 1  ConvergeRate              5
  Mode 0 off / 1 run / 2 reset the solution; ApplyComp 1 no / 3 yes.

Real-time HSI results (read-only)
  C00                       1  C01                       0  C02                       0
  C10                       0  C11                       1  C12                       0
  C20                       0  C21                       0  C22                       1
  B0                        0  B1                        0  B2                        0
  What the real-time HSI estimator has worked out; applied on top of register 23.

Changes: set <reg> <values>   then save to keep them.  help for more.
> 