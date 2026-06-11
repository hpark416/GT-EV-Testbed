#ifndef VESC_UART_H
#define VESC_UART_H

#include <Arduino.h>

// VESC / FT85BD UART packet commands (subset)
#define COMM_GET_VALUES_SELECTIVE 50
#define COMM_SET_RPM              8
#define COMM_FORWARD_CAN          34
#define COMM_ALIVE                30
#define COMM_FW_VERSION           0

class VescUart {
public:
  void begin(HardwareSerial &serial, unsigned long baud = 115200) {
    port_ = &serial;
    port_->begin(baud);
    resetRxState();
  }

  void poll() {
    while (port_->available()) {
      processByte(port_->read());
    }
  }

  bool sendFwVersion() { return sendPacket(COMM_FW_VERSION, nullptr, 0); }

  bool sendAlive() { return sendPacket(COMM_ALIVE, nullptr, 0); }

  bool requestTelemetry(uint8_t canId = 0) {
  // Mask bits: 2=motor current, 7=erpm, 8=v_in, 15=fault, 17=controller id
    const uint8_t payload[] = {
      COMM_GET_VALUES_SELECTIVE,
      0x00, 0x02, 0x88, 0x84
    };
    if (canId == 0) {
      return sendPacket(payload, sizeof(payload));
    }
    return sendForwardCan(canId, payload, sizeof(payload));
  }

  bool setRpm(int32_t erpm, uint8_t canId = 0) {
    uint8_t payload[5];
    payload[0] = COMM_SET_RPM;
    writeInt32(&payload[1], erpm);
    if (canId == 0) {
      return sendPacket(payload, sizeof(payload));
    }
    return sendForwardCan(canId, payload, sizeof(payload));
  }

  bool hasTelemetry() const { return telemetryValid_; }
  void clearTelemetryFlag() { telemetryValid_ = false; }

  int32_t erpm() const { return erpm_; }
  float inputVoltage() const { return inputVoltage_; }
  float motorCurrent() const { return motorCurrent_; }
  uint8_t faultCode() const { return faultCode_; }
  uint8_t controllerId() const { return controllerId_; }

  bool hasFwVersion() const { return fwValid_; }
  uint8_t fwMajor() const { return fwMajor_; }
  uint8_t fwMinor() const { return fwMinor_; }

private:
  HardwareSerial *port_ = nullptr;

  bool telemetryValid_ = false;
  int32_t erpm_ = 0;
  float inputVoltage_ = 0.0f;
  float motorCurrent_ = 0.0f;
  uint8_t faultCode_ = 0;
  uint8_t controllerId_ = 0;

  bool fwValid_ = false;
  uint8_t fwMajor_ = 0;
  uint8_t fwMinor_ = 0;

  static const uint8_t RX_BUF_SIZE = 128;
  uint8_t rxBuf_[RX_BUF_SIZE];
  uint8_t rxLen_ = 0;
  uint8_t rxExpected_ = 0;
  bool rxActive_ = false;

  static uint16_t crc16(const uint8_t *data, uint8_t len) {
    uint16_t crc = 0;
    while (len--) {
      uint8_t x = (crc >> 8) ^ *data++;
      x ^= x >> 4;
      crc = (crc << 8) ^ (uint16_t(x << 12)) ^ (uint16_t(x << 5)) ^ x;
    }
    return crc;
  }

  static void writeInt32(uint8_t *dst, int32_t value) {
    dst[0] = (value >> 24) & 0xFF;
    dst[1] = (value >> 16) & 0xFF;
    dst[2] = (value >> 8) & 0xFF;
    dst[3] = value & 0xFF;
  }

  static int32_t readInt32(const uint8_t *&p) {
    int32_t v = (int32_t)p[0] << 24;
    v |= (int32_t)p[1] << 16;
    v |= (int32_t)p[2] << 8;
    v |= p[3];
    p += 4;
    return v;
  }

  static uint32_t readUint32(const uint8_t *&p) {
    uint32_t v = (uint32_t)p[0] << 24;
    v |= (uint32_t)p[1] << 16;
    v |= (uint32_t)p[2] << 8;
    v |= p[3];
    p += 4;
    return v;
  }

  static float readFloat16(const uint8_t *&p, float scale) {
    int16_t raw = (int16_t)((p[0] << 8) | p[1]);
    p += 2;
    return raw / scale;
  }

  static double readDouble32(const uint8_t *&p, double scale) {
    int32_t raw = readInt32(p);
    return raw / scale;
  }

  bool sendPacket(uint8_t command, const uint8_t *data, uint8_t dataLen) {
    uint8_t payload[64];
    payload[0] = command;
    if (dataLen > 0 && data != nullptr) {
      memcpy(&payload[1], data, dataLen);
    }
    return sendPacket(payload, 1 + dataLen);
  }

  bool sendPacket(const uint8_t *payload, uint8_t len) {
    if (port_ == nullptr || len == 0) return false;

    uint16_t crc = crc16(payload, len);
    port_->write(0x02);
    port_->write(len);
    port_->write(payload, len);
    port_->write((uint8_t)(crc >> 8));
    port_->write((uint8_t)(crc & 0xFF));
    port_->write(0x03);
    return true;
  }

  bool sendForwardCan(uint8_t canId, const uint8_t *inner, uint8_t innerLen) {
    uint8_t payload[64];
    if (innerLen + 5 > sizeof(payload)) return false;

    payload[0] = COMM_FORWARD_CAN;
    writeInt32(&payload[1], canId);
    memcpy(&payload[5], inner, innerLen);
    return sendPacket(payload, 5 + innerLen);
  }

  void resetRxState() {
    rxActive_ = false;
    rxLen_ = 0;
    rxExpected_ = 0;
  }

  void processByte(uint8_t b) {
    if (!rxActive_) {
      if (b == 0x02) {
        rxActive_ = true;
        rxLen_ = 0;
        rxExpected_ = 0;
      }
      return;
    }

    if (rxExpected_ == 0) {
      rxExpected_ = b + 3;
      return;
    }

    if (rxLen_ >= RX_BUF_SIZE) {
      resetRxState();
      return;
    }

    rxBuf_[rxLen_++] = b;
    rxExpected_--;

    if (rxExpected_ == 0) {
      if (rxBuf_[rxLen_ - 1] == 0x03) {
        handlePacket(rxBuf_, rxLen_ - 1);
      }
      resetRxState();
    }
  }

  void handlePacket(const uint8_t *data, uint8_t len) {
    if (len < 3) return;

    uint8_t payloadLen = len - 2;
    uint16_t rxCrc = ((uint16_t)data[payloadLen] << 8) | data[payloadLen + 1];
    if (crc16(data, payloadLen) != rxCrc) return;

    const uint8_t *p = data;
    uint8_t cmd = *p++;

    switch (cmd) {
      case COMM_FW_VERSION:
        if (payloadLen >= 3) {
          fwMajor_ = p[0];
          fwMinor_ = p[1];
          fwValid_ = true;
        }
        break;

      case COMM_GET_VALUES_SELECTIVE: {
        if (payloadLen < 5) break;
        uint32_t mask = readUint32(p);
        if (mask & (1UL << 2)) motorCurrent_ = (float)readDouble32(p, 100.0);
        if (mask & (1UL << 7)) erpm_ = readInt32(p);
        if (mask & (1UL << 8)) inputVoltage_ = readFloat16(p, 10.0f);
        if (mask & (1UL << 15)) faultCode_ = *p++;
        if (mask & (1UL << 17)) controllerId_ = *p++;
        telemetryValid_ = true;
        break;
      }

      default:
        break;
    }
  }
};

#endif
