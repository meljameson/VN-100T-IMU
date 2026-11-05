// VN-100 info: asks each VectorNav IMU for its model, hardware revision, serial
// number and firmware version, and prints them to the USB serial monitor.
//
// Read-only: it sends register *reads* ($VNRRG) and changes no IMU settings.
// It finds each IMU's baud rate by trying the common ones, factory 115200 first.
//
// Flash, then open the serial monitor (any baud -- it's USB):
//   arduino-cli monitor -p COMx --config baudrate=115200
// Press any key in the monitor to run it again.
// 
// ./arduino-cli.exe board list    # find the Teensy port, e.g. Port_#0009.Hub_#0002
// ./arduino-cli.exe upload --fqbn teensy:avr:teensy41 -p "<port>" --input-file '/home/mel/projects/the_core/embedded/nav_board/vn100_info/vn100_info.ino'
// ./arduino-cli.exe monitor -p <COM port from board list> --config baudrate=115200

#include <Arduino.h>

// The ports the IMUs are on (nav board Rev C: IMU 1 on Serial1, IMU 2 on Serial2).
struct ImuPort {
    HardwareSerialIMXRT* serial;
    const char* name;
};
ImuPort ports[] = {
    {&Serial1, "IMU 1 (Serial1, RX 0 / TX 1)"},
    {&Serial2, "IMU 2 (Serial2, RX 7 / TX 8)"},
};

// Baud rates the VN-100 supports, most likely first.
const uint32_t kBauds[] = {115200, 921600, 460800, 230400, 128000, 57600, 38400, 19200, 9600};

struct Register {
    int number;
    const char* name;
};
const Register kRegisters[] = {
    {1, "Model"},
    {2, "Hardware revision"},
    {3, "Serial number"},
    {4, "Firmware version"},
};

const uint32_t kReplyTimeoutMs = 150;
uint8_t rxMemory[sizeof(ports) / sizeof(ports[0])][512];  // async output keeps arriving meanwhile

// What the port has heard during the current report, for telling "nothing on
// the wire" (power, wiring) apart from "something answers but not as expected".
uint32_t bytesHeard = 0;
char lastError[48] = "";

uint8_t xorChecksum(const char* s, size_t n) {
    uint8_t sum = 0;
    for (size_t i = 0; i < n; ++i) sum ^= static_cast<uint8_t>(s[i]);
    return sum;
}

// Sends $VNRRG,<reg> and waits for its reply. On success copies the value (the
// part after "$VNRRG,NN," up to '*') into `value` and returns true.
//
// The command uses the ICD's checksum bypass ("*XX", section 1.4.1), which the
// IMU accepts whichever checksum register 30 is set to. With a real 8-bit
// checksum, an IMU left in 16-bit CRC mode (nav_revA sets that) rejects every
// command with $VNERR,03 until it's power-cycled.
bool readRegister(HardwareSerialIMXRT& serial, int reg, char* value, size_t value_size) {
    char command[32];
    snprintf(command, sizeof command, "$VNRRG,%02d*XX\r\n", reg);

    while (serial.available()) serial.read();  // drop whatever was already buffered
    serial.print(command);

    char expected[16];
    snprintf(expected, sizeof expected, "$VNRRG,%02d,", reg);
    const size_t expected_len = strlen(expected);

    char line[160];
    size_t len = 0;
    const uint32_t start = millis();
    while (millis() - start < kReplyTimeoutMs) {
        if (!serial.available()) continue;
        const char c = static_cast<char>(serial.read());
        ++bytesHeard;
        if (c == '$') len = 0;  // every message starts with '$'
        if (c == '\n' || c == '\r') {
            line[len] = '\0';
            const char* star = strchr(line, '*');
            if (strncmp(line, "$VNERR,", 7) == 0) {
                snprintf(lastError, sizeof lastError, "%s", line);
            }
            if (len > 0 && star != nullptr && strncmp(line, expected, expected_len) == 0) {
                // Verify the 8-bit checksum when that's what the IMU is using
                // (2 hex digits); a 16-bit CRC (4 digits) is accepted as is.
                if (strlen(star + 1) == 2) {
                    const unsigned long given = strtoul(star + 1, nullptr, 16);
                    if (given != xorChecksum(line + 1, static_cast<size_t>(star - line - 1))) {
                        len = 0;
                        continue;
                    }
                }
                const size_t n = static_cast<size_t>(star - (line + expected_len));
                const size_t copy = n < value_size - 1 ? n : value_size - 1;
                memcpy(value, line + expected_len, copy);
                value[copy] = '\0';
                return true;
            }
            len = 0;
            continue;
        }
        if (len < sizeof line - 1) line[len++] = c;
    }
    return false;
}

void report() {
    Serial.println();
    Serial.println("=== VN-100 info ===");
    for (size_t p = 0; p < sizeof(ports) / sizeof(ports[0]); ++p) {
        HardwareSerialIMXRT& serial = *ports[p].serial;
        Serial.printf("\n%s\n", ports[p].name);

        uint32_t found_baud = 0;
        char value[96];
        bytesHeard = 0;
        lastError[0] = '\0';
        uint32_t bytes_at_115200 = 0;
        for (uint32_t baud : kBauds) {
            serial.begin(baud);
            serial.addMemoryForRead(rxMemory[p], sizeof rxMemory[p]);
            delay(20);
            // Try twice: the first command can be garbled by leftover bytes.
            if (readRegister(serial, 1, value, sizeof value) || readRegister(serial, 1, value, sizeof value)) {
                found_baud = baud;
                break;
            }
            if (baud == kBauds[0]) bytes_at_115200 = bytesHeard;
            serial.end();
        }

        if (found_baud == 0) {
            Serial.println("  no reply at any baud rate");
            if (lastError[0] != '\0') {
                Serial.printf("  but the IMU answered with an error: %s\n", lastError);
                Serial.println("  (VNERR codes: 3 = bad checksum, 4 = bad command, 12 = baud too low)");
            } else if (bytesHeard == 0) {
                Serial.println("  and not a single byte came back: check the IMU has power, and the wiring");
                Serial.println("  (VN TX -> Teensy RX, VN RX -> Teensy TX, common ground)");
            } else {
                Serial.printf("  but %lu bytes arrived (%lu of them at 115200) that weren't a reply:\n",
                              static_cast<unsigned long>(bytesHeard), static_cast<unsigned long>(bytes_at_115200));
                Serial.println("  the IMU is talking, maybe streaming output at another baud, or the line is noisy");
            }
            Serial.println("  An IMU left in a bad state by other firmware recovers when its power is cycled");
            Serial.println("  (reflashing the Teensy alone doesn't do it).");
            continue;
        }

        Serial.printf("  %-18s %lu\n", "Baud rate", static_cast<unsigned long>(found_baud));
        for (const Register& r : kRegisters) {
            if (readRegister(serial, r.number, value, sizeof value)) {
                Serial.printf("  %-18s %s\n", r.name, value);
            } else {
                Serial.printf("  %-18s (no reply)\n", r.name);
            }
        }
    }
    Serial.println("\nPress any key to run again.");
}

void setup() {
    Serial.begin(115200);
}

// Runs the report whenever a serial monitor connects (Serial goes true when
// one opens the port), so it's never printed before anyone is listening --
// e.g. right after an upload, before the IDE's monitor reattaches -- and
// again whenever a key is sent.
bool monitorWasConnected = false;

void loop() {
    const bool connected = static_cast<bool>(Serial);
    if (connected && !monitorWasConnected) {
        delay(100);  // let the monitor finish opening
        report();
    }
    monitorWasConnected = connected;

    if (Serial.available()) {
        while (Serial.available()) Serial.read();
        report();
    }
}
