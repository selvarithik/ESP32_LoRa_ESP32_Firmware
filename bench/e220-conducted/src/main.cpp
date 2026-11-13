#include <Arduino.h>
#include <esp_mac.h>
#include "../../../src/config/hardware_profile.h"

namespace {
constexpr char Version[] = "E220 conducted bench 1.0.0";
constexpr uint32_t SendGap = 300000;
constexpr uint32_t BurstGap = 25;
constexpr size_t MaxPayload = 16;
constexpr unsigned MaxSends = 3; // Two boards, six attempts per session.
constexpr uint8_t ReadRegisters[] = {0xc1, 0, 6};
constexpr uint8_t KeyZero[] = {0xc2, 6, 2, 0, 0};
constexpr uint8_t KeyReply[] = {0xc1, 6, 2, 0, 0};

HardwareSerial radio(2);
bool recognized = false, normal = false, approvedProfile = false, keyConfirmed = false;
bool capture = true, rssiEnabled = false;
unsigned sends = 0;
uint32_t lastSend, lastByteTime;
bool pendingByte = false;
uint8_t lastByte;
unsigned burstSize = 0;
char line[48];
size_t used = 0;
bool badLine = false;

bool waitAux(uint32_t settleMs) {
    uint32_t started = millis(), highSince = started;
    while (millis() - started < 3000) {
        if (digitalRead(HardwareProfile::RadioAux) == LOW) highSince = millis();
        else if (millis() - highSince >= settleMs) return true;
        delay(1);
    }
    Serial.println("ERR AUX_TIMEOUT");
    return false;
}

bool mode(bool config) {
    normal = false;
    if (!waitAux(3)) return false;
    digitalWrite(HardwareProfile::RadioM0, config ? HIGH : LOW);
    digitalWrite(HardwareProfile::RadioM1, config ? HIGH : LOW);
    if (!waitAux(100)) return false;
    normal = !config;
    return true;
}

void printHex(const char* label, const uint8_t* bytes, size_t size) {
    Serial.print(label);
    for (size_t i = 0; i < size; ++i) Serial.printf(" %02X", bytes[i]);
    Serial.println();
}

template<size_t Request, size_t Reply>
size_t exchange(const uint8_t (&request)[Request], uint8_t (&reply)[Reply]) {
    while (radio.available()) radio.read();
    if (radio.write(request, Request) != Request) return 0;
    radio.flush();
    size_t size = 0;
    const uint32_t started = millis();
    while (millis() - started < 1200 && size < Reply) {
        if (radio.available()) reply[size++] = radio.read();
        else delay(1);
    }
    return size;
}

bool readRegisters(const char* label, uint8_t (&reply)[9]) {
    const size_t size = exchange(ReadRegisters, reply);
    printHex(label, reply, size);
    return size == sizeof(reply) && memcmp(reply, ReadRegisters, sizeof(ReadRegisters)) == 0;
}

bool profileMatches(const uint8_t (&reply)[9]) {
    // Reject any unreviewed framing, UART rate, RF setting or mode.
    const uint8_t expected[] = {0, 0, 0x62, 0x03, 0x10, 0x83};
    return memcmp(reply + 3, expected, sizeof(expected)) == 0;
}

void configure(bool setKey) {
    approvedProfile = false;
    if (setKey) keyConfirmed = false;
    if (!recognized || !mode(true)) return;
    uint8_t before[9]{}, after[9]{};
    const bool read = readRegisters("CFG BEFORE", before);
    bool valid = read && profileMatches(before);
    rssiEnabled = read && (before[8] & 0x80);
    if (setKey && valid && waitAux(3)) {
        uint8_t reply[5]{};
        const size_t size = exchange(KeyZero, reply);
        printHex("C2 ECHO", reply, size);
        const bool echo = size == sizeof(reply) && memcmp(reply, KeyReply, sizeof(reply)) == 0;
        const bool reread = waitAux(10) && readRegisters("CFG AFTER", after);
        valid = echo && reread && memcmp(before, after, sizeof(before)) == 0;
        keyConfirmed = valid;
    }
    const bool restored = mode(false);
    approvedProfile = valid && restored;
    Serial.printf("CFG %s NORMAL=%d KEY0_ACK=%d\n", approvedProfile ? "OK" : "REFUSED", restored, keyConfirmed);
}

void finishBurst() {
    if (!pendingByte) return;
    if (rssiEnabled && burstSize >= 2) {
        Serial.printf("RX %lu %02X RSSI %d dBm\n", (unsigned long)lastByteTime, lastByte, int(lastByte) - 256);
    } else {
        Serial.printf("RX %lu %02X RSSI_UNAVAILABLE\n", (unsigned long)lastByteTime, lastByte);
    }
    Serial.printf("RX_BURST %u bytes\n", burstSize);
    pendingByte = false;
    burstSize = 0;
}

void readRadio() {
    if (!normal) return;
    if (pendingByte && millis() - lastByteTime >= BurstGap) finishBurst();
    for (unsigned i = 0; i < 64 && radio.available(); ++i) {
        const uint8_t byte = radio.read();
        if (!capture) continue;
        if (pendingByte) Serial.printf("RX %lu %02X\n", (unsigned long)lastByteTime, lastByte);
        lastByte = byte;
        lastByteTime = millis();
        pendingByte = true;
        ++burstSize;
    }
}

void sendOnce(const char* payload) {
    const size_t size = strlen(payload);
    if (size == 0 || size > MaxPayload) {
        Serial.println("ERR MAX_16_BYTES");
        return;
    }
    if (!normal || !approvedProfile || !keyConfirmed) {
        Serial.println("ERR RUN_CFG_KEY0_WITH_MATCHING_PROFILE");
        return;
    }
    if (sends >= MaxSends) {
        Serial.println("ERR SESSION_TX_LIMIT");
        return;
    }
    const uint32_t elapsed = millis() - lastSend;
    if (elapsed < SendGap) {
        Serial.printf("ERR WAIT_MS %lu\n", (unsigned long)(SendGap - elapsed));
        return;
    }
    if (!waitAux(5)) return;
    lastSend = millis(); // Charge even if UART accepts only part of the request.
    ++sends;
    const size_t written = radio.write(reinterpret_cast<const uint8_t*>(payload), size);
    radio.flush();
    Serial.printf("TX %u/%u UART_BYTES %u/%u\n", sends, MaxSends, unsigned(written), unsigned(size));
}

void command() {
    if (strcmp(line, "rx") == 0) {
        capture = true;
        Serial.println("OK RX");
    } else if (strcmp(line, "cfg read") == 0) {
        configure(false);
    } else if (strcmp(line, "cfg key0") == 0) {
        configure(true);
    } else if (strncmp(line, "send ", 5) == 0) {
        sendOnce(line + 5);
    } else {
        Serial.println("ERR COMMAND");
    }
}
}

void setup() {
    lastSend = millis(); // Reset cannot shorten the 300 s holdoff.
    Serial.begin(115200);
    uint8_t mac[6]{};
    const uint8_t master[] = {0x78, 0x21, 0x84, 0x7d, 0x77, 0x10};
    const uint8_t slave[] = {0x84, 0x1f, 0xe8, 0x2a, 0x2f, 0x84};
    const bool read = esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK;
    const bool isMaster = read && memcmp(mac, master, sizeof(mac)) == 0;
    recognized = isMaster || (read && memcmp(mac, slave, sizeof(mac)) == 0);
    Serial.println(Version);
    printHex("MAC", mac, sizeof(mac));
    if (!recognized) {
        Serial.println("ERR UNKNOWN_BOARD");
        return;
    }
    const int rx = isMaster ? 32 : HardwareProfile::RadioRx;
    const int tx = isMaster ? 33 : HardwareProfile::RadioTx;
    Serial.printf("LISTEN ONLY RX=%d TX=%d M0=%d M1=%d AUX=%d\n", rx, tx,
                  HardwareProfile::RadioM0, HardwareProfile::RadioM1, HardwareProfile::RadioAux);
    pinMode(HardwareProfile::RadioAux, INPUT);
    digitalWrite(HardwareProfile::RadioM0, HIGH);
    digitalWrite(HardwareProfile::RadioM1, HIGH);
    pinMode(HardwareProfile::RadioM0, OUTPUT);
    pinMode(HardwareProfile::RadioM1, OUTPUT);
    radio.setRxBufferSize(512);
    radio.begin(9600, SERIAL_8N1, rx, tx);
    configure(false);
}

void loop() {
    if (!recognized) { delay(20); return; }
    readRadio();
    while (Serial.available()) {
        const int byte = Serial.read();
        if (byte == '\r') continue;
        if (byte == '\n') {
            line[used] = 0;
            if (badLine) Serial.println("ERR INVALID_COMMAND_BYTES");
            else if (used) command();
            used = 0;
            badLine = false;
        } else if (byte < 0x20 || byte > 0x7e || used == sizeof(line) - 1) {
            badLine = true;
        } else if (!badLine) {
            line[used++] = char(byte);
        }
    }
    delay(1);
}
