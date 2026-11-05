// VN-100 calibration: prints each IMU's calibration and filter-tuning registers
// and lets you change them, from the USB serial monitor.
//
// Registers and commands are from the VN-100 ICD v3.1.0.0 (../nav), which
// matches our units' firmware (3.1.0.0).
//
// Changes are made in the IMU's RAM only: they take effect at once (except
// register 26, see below) but are lost at the next power cycle or `reset`.
// They become permanent only with `save` ($VNWNV), which writes ALL of the
// IMU's configuration registers to flash -- including anything other firmware
// changed in RAM since power-on, so power-cycle the IMU before a session you
// mean to save. Keep the IMU still while saving (ICD 1.3.3).
//
// Commands (type into the serial monitor, Enter to send):
//   help                  this list
//   imu <1|2>             pick the IMU (default 1)
//   show                  print every register below
//   show <reg>            print one register
//   set <reg> <values>    write a register (RAM only), values comma- or
//                         space-separated, all of them in field order; reads
//                         it back after
//   save                  write settings to flash (asks for YES)
//   reset                 reboot the IMU: unsaved changes are discarded
//
// Not offered: factory restore ($VNRFS) -- it also resets baud rate and
// output settings the nav firmware relies on. Use VectorNav's Control Center
// for that.
//
// ./arduino-cli.exe upload --fqbn teensy:avr:teensy41 -p "<port>" --input-dir 'C:\Users\melsj\AppData\Local\Temp\vn100_calibration_build'
// ./arduino-cli.exe monitor -p <COM port> --config baudrate=115200

#include <Arduino.h>

// Nav board Rev C: IMU 1 on Serial1, IMU 2 on Serial2, factory 115200 baud
// (../vn100_info finds the baud if that's wrong).
struct ImuPort {
    HardwareSerialIMXRT* serial;
    const char* name;
};
ImuPort ports[] = {
    {&Serial1, "IMU 1 (Serial1, RX 0 / TX 1)"},
    {&Serial2, "IMU 2 (Serial2, RX 7 / TX 8)"},
};
const size_t kPortCount = sizeof(ports) / sizeof(ports[0]);
const uint32_t kBaud = 115200;
uint8_t rxMemory[kPortCount][512];
size_t current = 0;

struct Register {
    int number;
    const char* name;
    const char* fields[12];
    const char* note;  // printed under the values, or nullptr
    bool writable;
};

const char* const kUserCalNote =
    "Applied on top of the factory calibration. ICD: user bias calibration only in certain\n"
    "  circumstances; changing the gain (scale/cross-axis) matrix is never recommended.";

const Register kRegisters[] = {
    {21, "Magnetic and gravity reference vectors",
     {"MagRefN G", "MagRefE G", "MagRefD G", "GravRefN m/s2", "GravRefE m/s2", "GravRefD m/s2"},
     "The field and gravity the attitude filter expects where the machine is.", true},
    {23, "Magnetometer calibration (user hard/soft iron)",
     {"C00", "C01", "C02", "C10", "C11", "C12", "C20", "C21", "C22", "BiasX G", "BiasY G", "BiasZ G"},
     "Offline hard/soft iron correction, on top of the factory calibration.", true},
    {25, "Accelerometer calibration (user)",
     {"C00", "C01", "C02", "C10", "C11", "C12", "C20", "C21", "C22", "BiasX m/s2", "BiasY m/s2", "BiasZ m/s2"},
     kUserCalNote, true},
    {84, "Gyro calibration (user)",
     {"C00", "C01", "C02", "C10", "C11", "C12", "C20", "C21", "C22", "BiasX rad/s", "BiasY rad/s", "BiasZ rad/s"},
     kUserCalNote, true},
    {26, "Reference frame rotation",
     {"R00", "R01", "R02", "R10", "R11", "R12", "R20", "R21", "R22"},
     "Mounting rotation (a rotation matrix). Static: takes effect only after save, then reset.", true},
    {35, "VPE basic control",
     {"Reserved", "HeadingMode", "FilteringMode", "TuningMode"},
     "Reserved must stay at its current value.", true},
    {36, "VPE magnetometer basic tuning",
     {"BaseX", "BaseY", "BaseZ", "AdaptTuneX", "AdaptTuneY", "AdaptTuneZ", "AdaptFiltX", "AdaptFiltY", "AdaptFiltZ"},
     nullptr, true},
    {38, "VPE accelerometer basic tuning",
     {"BaseX", "BaseY", "BaseZ", "AdaptTuneX", "AdaptTuneY", "AdaptTuneZ", "AdaptFiltX", "AdaptFiltY", "AdaptFiltZ"},
     nullptr, true},
    {43, "Filter startup gyro bias",
     {"X rad/s", "Y rad/s", "Z rad/s"},
     "Where the attitude filter's gyro bias estimate starts at power-on (needs save to matter).", true},
    {85, "IMU filtering configuration",
     {"MagWindow", "AccelWindow", "GyroWindow", "TempWindow", "PresWindow",
      "MagMode", "AccelMode", "GyroMode", "TempMode", "PresMode"},
     nullptr, true},
    {44, "Real-time HSI control",
     {"Mode", "ApplyComp", "ConvergeRate"},
     "Mode 0 off / 1 run / 2 reset the solution; ApplyComp 1 no / 3 yes.", true},
    {47, "Real-time HSI results (read-only)",
     {"C00", "C01", "C02", "C10", "C11", "C12", "C20", "C21", "C22", "B0", "B1", "B2"},
     "What the real-time HSI estimator has worked out; applied on top of register 23.", false},
};
const size_t kRegisterCount = sizeof(kRegisters) / sizeof(kRegisters[0]);

const Register* findRegister(int number) {
    for (const Register& r : kRegisters) {
        if (r.number == number) return &r;
    }
    return nullptr;
}

size_t fieldCount(const Register& r) {
    size_t n = 0;
    while (n < 12 && r.fields[n] != nullptr) ++n;
    return n;
}

const char* vnErrorText(int code) {
    switch (code) {
        case 1: return "hard fault";
        case 2: return "serial buffer overflow";
        case 3: return "invalid checksum";
        case 4: return "invalid command";
        case 5: return "not enough parameters";
        case 6: return "too many parameters";
        case 7: return "invalid parameter (out of range, or not a rotation matrix)";
        case 8: return "invalid register";
        case 9: return "unauthorized access (read-only register)";
        case 10: return "watchdog reset";
        case 11: return "output buffer overflow";
        case 12: return "baud rate too low for the output";
        default: return "unknown error";
    }
}

// ---- talking to the IMU -----------------------------------------------------------

// Sends "$<body>*XX" and waits for a reply line starting with "$<prefix>".
// "*XX" is the ICD's checksum bypass (1.4.1): accepted whatever checksum mode
// register 30 is in. On success copies what follows the prefix, up to '*',
// into `value`. A $VNERR reply returns false with its code in `error_code`.
bool transact(const char* body, const char* prefix, char* value, size_t value_size, uint32_t timeout_ms,
              int& error_code) {
    HardwareSerialIMXRT& serial = *ports[current].serial;
    error_code = 0;
    while (serial.available()) serial.read();
    serial.printf("$%s*XX\r\n", body);

    char expected[24];
    snprintf(expected, sizeof expected, "$%s", prefix);
    const size_t expected_len = strlen(expected);
    char line[256];
    size_t len = 0;
    const uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (!serial.available()) continue;
        const char c = static_cast<char>(serial.read());
        if (c == '$') len = 0;  // every message starts with '$'; also skips binary output
        if (c != '\n' && c != '\r') {
            if (len < sizeof line - 1) line[len++] = c;
            continue;
        }
        line[len] = '\0';
        len = 0;
        const char* star = strchr(line, '*');
        if (star == nullptr) continue;
        if (strncmp(line, "$VNERR,", 7) == 0) {
            error_code = atoi(line + 7);
            return false;
        }
        if (strncmp(line, expected, expected_len) == 0) {
            const size_t n = static_cast<size_t>(star - (line + expected_len));
            const size_t copy = n < value_size - 1 ? n : value_size - 1;
            memcpy(value, line + expected_len, copy);
            value[copy] = '\0';
            return true;
        }
    }
    return false;
}

void printError(int error_code) {
    if (error_code != 0) {
        Serial.printf("  IMU error %d: %s\n", error_code, vnErrorText(error_code));
    } else {
        Serial.println("  no reply -- is the IMU powered and wired? (../vn100_info checks)");
    }
}

// ---- show / set ------------------------------------------------------------------------

void printValues(const Register& r, char* values) {
    const size_t n = fieldCount(r);
    size_t i = 0;
    for (char* token = strtok(values, ","); token != nullptr; token = strtok(nullptr, ","), ++i) {
        const char* name = i < n ? r.fields[i] : "?";
        Serial.printf("  %-14s %12s", name, token);
        if (i % 3 == 2) Serial.println();
    }
    if (i % 3 != 0) Serial.println();
    if (i != n) Serial.printf("  (expected %u values, got %u)\n", static_cast<unsigned>(n), static_cast<unsigned>(i));
}

void showRegister(const Register& r) {
    char body[16], prefix[16], value[256];
    snprintf(body, sizeof body, "VNRRG,%02d", r.number);
    snprintf(prefix, sizeof prefix, "VNRRG,%02d,", r.number);
    Serial.printf("\n%s\n", r.name);
    int error_code;
    if (!transact(body, prefix, value, sizeof value, 200, error_code)) {
        printError(error_code);
        return;
    }
    printValues(r, value);
    if (r.note != nullptr) Serial.printf("  %s\n", r.note);
}

void showAll() {
    Serial.printf("\n=== %s calibration ===\n", ports[current].name);
    for (const Register& r : kRegisters) showRegister(r);
    Serial.println("\nChanges: set <reg> <values>   then save to keep them.  help for more.");
}

// "set 23 1,0,0 ..." -- `args` is everything after "set ".
void setRegister(char* args) {
    char* end = nullptr;
    const long number = strtol(args, &end, 10);
    const Register* r = (end != args) ? findRegister(static_cast<int>(number)) : nullptr;
    if (r == nullptr) {
        Serial.println("  usage: set <reg> <values>  (show lists the registers)");
        return;
    }
    if (!r->writable) {
        Serial.printf("  register %d is read-only\n", r->number);
        return;
    }

    // Values as one comma-separated list, each checked to be a plain number.
    char values[200] = "";
    size_t count = 0;
    for (char* token = strtok(end, " ,\t"); token != nullptr; token = strtok(nullptr, " ,\t")) {
        char* number_end = nullptr;
        strtod(token, &number_end);
        if (number_end == token || *number_end != '\0') {
            Serial.printf("  '%s' isn't a number\n", token);
            return;
        }
        if (strlen(values) + strlen(token) + 2 > sizeof values) {
            Serial.println("  too many values");
            return;
        }
        if (count++ > 0) strcat(values, ",");
        strcat(values, token);
    }
    const size_t needed = fieldCount(*r);
    if (count != needed) {
        Serial.printf("  register %d takes %u values, in this order:\n   ", r->number, static_cast<unsigned>(needed));
        for (size_t i = 0; i < needed; ++i) Serial.printf(" %s", r->fields[i]);
        Serial.println();
        return;
    }

    char body[240], prefix[16], reply[256];
    snprintf(body, sizeof body, "VNWRG,%02d,%s", r->number, values);
    snprintf(prefix, sizeof prefix, "VNWRG,%02d", r->number);
    int error_code;
    if (!transact(body, prefix, reply, sizeof reply, 300, error_code)) {
        printError(error_code);
        return;
    }
    Serial.printf("  written to RAM. Now reads back as:");
    showRegister(*r);
    Serial.println("  (lost at power-off unless you save)");
}

void save() {
    Serial.println("  This writes ALL of the IMU's current settings to flash, used at every startup from now on.");
    Serial.println("  Keep the IMU still. Type YES to save, anything else to cancel:");
    char answer[16];
    size_t len = 0;
    for (;;) {
        if (!Serial.available()) continue;
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (len == 0) continue;
            break;
        }
        if (len < sizeof answer - 1) answer[len++] = c;
    }
    answer[len] = '\0';
    if (strcmp(answer, "YES") != 0) {
        Serial.println("  cancelled, nothing saved");
        return;
    }
    char reply[32];
    int error_code;
    // Flash writes take ~500 ms, up to 1 s (ICD 1.3.3).
    if (transact("VNWNV", "VNWNV", reply, sizeof reply, 2000, error_code)) {
        Serial.println("  saved to flash");
    } else {
        printError(error_code);
    }
}

void resetImu() {
    char reply[32];
    int error_code;
    if (!transact("VNRST", "VNRST", reply, sizeof reply, 300, error_code)) {
        printError(error_code);
        return;
    }
    Serial.println("  IMU resetting: unsaved changes discarded. Waiting 2 s for it to restart...");
    delay(2000);  // ICD 1.3.5
    Serial.println("  done");
}

void help() {
    Serial.println("\nCommands:");
    Serial.println("  imu <1|2>            pick the IMU");
    Serial.println("  show                 print all calibration registers");
    Serial.println("  show <reg>           print one register");
    Serial.println("  set <reg> <values>   write a register in RAM (all its values, in order), then read back");
    Serial.println("  save                 write settings to flash so they survive power-off (asks for YES)");
    Serial.println("  reset                reboot the IMU, discarding unsaved changes");
    Serial.printf("Current: %s\n", ports[current].name);
}

// ---- command line --------------------------------------------------------------------

void handle(char* line) {
    while (*line == ' ') ++line;
    char* args = strchr(line, ' ');
    if (args != nullptr) *args++ = '\0';
    else args = line + strlen(line);

    if (strcmp(line, "") == 0) return;
    if (strcmp(line, "help") == 0) {
        help();
    } else if (strcmp(line, "imu") == 0) {
        const int n = atoi(args);
        if (n >= 1 && static_cast<size_t>(n) <= kPortCount) {
            current = static_cast<size_t>(n - 1);
            Serial.printf("  now using %s\n", ports[current].name);
        } else {
            Serial.printf("  imu takes 1..%u\n", static_cast<unsigned>(kPortCount));
        }
    } else if (strcmp(line, "show") == 0) {
        if (*args == '\0') {
            showAll();
        } else if (const Register* r = findRegister(atoi(args))) {
            showRegister(*r);
        } else {
            Serial.println("  not one of the registers this sketch knows (show lists them)");
        }
    } else if (strcmp(line, "set") == 0) {
        setRegister(args);
    } else if (strcmp(line, "save") == 0) {
        save();
    } else if (strcmp(line, "reset") == 0) {
        resetImu();
    } else {
        Serial.printf("  unknown command '%s' -- try help\n", line);
    }
    Serial.print("> ");
}

void setup() {
    Serial.begin(115200);
    for (size_t i = 0; i < kPortCount; ++i) {
        ports[i].serial->begin(kBaud);
        ports[i].serial->addMemoryForRead(rxMemory[i], sizeof rxMemory[i]);
    }
}

bool monitorWasConnected = false;
char input[256];
size_t inputLength = 0;

void loop() {
    // Print the current settings whenever a serial monitor connects.
    const bool connected = static_cast<bool>(Serial);
    if (connected && !monitorWasConnected) {
        delay(100);  // let the monitor finish opening
        showAll();
        help();
        Serial.print("> ");
    }
    monitorWasConnected = connected;

    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (inputLength == 0) continue;
            input[inputLength] = '\0';
            Serial.println(input);  // echo: most monitors don't
            inputLength = 0;
            handle(input);
        } else if (inputLength < sizeof input - 1) {
            input[inputLength++] = c;
        }
    }
}
