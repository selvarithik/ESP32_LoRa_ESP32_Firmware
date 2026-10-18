#include "lora_transport.h"
#include "../core/logger.h"

LoRaTransport loraTransport;

#if defined(ENABLE_E220_DRIVER)

bool LoRaTransport::begin(const DeviceConfig& config) {
    _initialized=false;
    if(!config.lora_enabled||config.lora_uart!=2)return false;
    _rxPin = config.lora_rx_pin;
    _txPin = config.lora_tx_pin;
    _auxPin = config.lora_aux_pin;
    _m0Pin = config.lora_m0_pin;
    _m1Pin = config.lora_m1_pin;

    pinMode(_auxPin, INPUT_PULLUP);
    pinMode(_m0Pin, OUTPUT);
    pinMode(_m1Pin, OUTPUT);

    // Transparent UART mode: M0=LOW, M1=LOW.
    digitalWrite(_m0Pin, LOW);
    digitalWrite(_m1Pin, LOW);

    _serial.begin(config.lora_baud, SERIAL_8N1, _rxPin, _txPin);
    _initialized = false;

    if (waitAux(1500)) {
        logger.info("LORA", "E220 UART transport initialized");
        _initialized = true;
    } else {
        logger.warn("LORA", "E220 AUX not ready; transport unavailable");
        _serial.end();
    }

    return _initialized;
}

bool LoRaTransport::waitAux(uint32_t timeoutMs) const {
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        if (digitalRead(_auxPin) == HIGH) return true;
        delay(2);
    }
    return false;
}

bool LoRaTransport::auxReady() const {
    return digitalRead(_auxPin) == HIGH;
}

bool LoRaTransport::send(const Message& message) {
    if (!_initialized) return false;
    if (!auxReady()) return false;

    String serialized = message.toJson();
    if (serialized.length()>MAX_FRAME_BYTES || _serial.availableForWrite()<int(serialized.length()+2))return false;
    size_t written = _serial.println(serialized);

    return written == serialized.length()+2; // UART queue acceptance, never a radio ACK.
}

bool LoRaTransport::available() {
    return _serial.available() > 0;
}

bool LoRaTransport::receive(Message& message) {
    if(!_initialized)return false;
    for(unsigned budget=0;budget<128&&_serial.available();++budget) {
        char ch = static_cast<char>(_serial.read());

        if (ch == '\n') {
            if(_discardRx){_discardRx=false;_lineBuffer.clear();continue;}
            String line = _lineBuffer;
            _lineBuffer.clear();
            line.trim();

            if (line.length() == 0) continue;

            if (Message::fromJson(line, message)) {
                return true;
            }

            logger.warn("LORA", "Discarded invalid packet");
        } else if (ch != '\r'&&!_discardRx) {
            if (_lineBuffer.length() < MAX_FRAME_BYTES) {
                _lineBuffer += ch;
            } else {
                _lineBuffer.clear();
                _discardRx=true;
                logger.warn("LORA", "RX buffer overflow; packet discarded");
            }
        }
    }

    return false;
}

void LoRaTransport::loop() {
    // RX is drained by the application loop.
}

bool LoRaTransport::isInitialized() const {
    return _initialized;
}

#else
bool LoRaTransport::begin(const DeviceConfig&) { return false; }
bool LoRaTransport::send(const Message&) { return false; }
bool LoRaTransport::available() { return false; }
bool LoRaTransport::receive(Message&) { return false; }
void LoRaTransport::loop() {}
bool LoRaTransport::auxReady() const { return false; }
bool LoRaTransport::isInitialized() const { return false; }
bool LoRaTransport::waitAux(uint32_t) const { return false; }
#endif

