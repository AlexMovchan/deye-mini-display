#include "solarman.h"

#include <string.h>

namespace solarman {

namespace {
constexpr uint8_t START = 0xA5;
constexpr uint8_t END = 0x15;
constexpr uint16_t CTRL_REQUEST = 0x4510;
constexpr uint16_t CTRL_RESPONSE = 0x1510;
constexpr uint16_t CTRL_COUNTER = 0x4710;
constexpr uint8_t FN_READ_HOLDING = 0x03;
constexpr size_t HEADER_LEN = 11;     // start + length + control + sequence + serial
constexpr size_t MODBUS_OFFSET = 25;  // 11 + frame type + status/sensor + 3×4 часу

void putLe16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
void putLe32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF; }
uint16_t le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

uint8_t checksum(const uint8_t* frame, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 1; i + 2 < len; i++) sum += frame[i];
  return sum;
}
}  // namespace

uint16_t crc16Modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

size_t buildReadRequest(uint8_t* out, uint32_t loggerSerial, uint8_t sequence,
                        uint8_t slaveId, uint16_t reg, uint16_t count) {
  memset(out, 0, REQUEST_LEN);
  out[0] = START;
  putLe16(out + 1, 15 + 8);
  putLe16(out + 3, CTRL_REQUEST);
  putLe16(out + 5, sequence);
  putLe32(out + 7, loggerSerial);
  out[11] = 0x02;  // frame type; далі sensor type і три поля часу — нулі
  uint8_t* mb = out + 26;
  mb[0] = slaveId;
  mb[1] = FN_READ_HOLDING;
  mb[2] = reg >> 8;
  mb[3] = reg & 0xFF;
  mb[4] = count >> 8;
  mb[5] = count & 0xFF;
  uint16_t crc = crc16Modbus(mb, 6);
  mb[6] = crc & 0xFF;
  mb[7] = crc >> 8;
  out[REQUEST_LEN - 2] = checksum(out, REQUEST_LEN);
  out[REQUEST_LEN - 1] = END;
  return REQUEST_LEN;
}

size_t frameLength(const uint8_t* buf, size_t len) {
  if (len < 3) return 0;
  return 13 + le16(buf + 1);
}

Result parseReadResponse(const uint8_t* frame, size_t len, uint32_t loggerSerial, uint8_t sequence,
                         uint8_t slaveId, uint16_t count, uint16_t* values) {
  if (len < HEADER_LEN + 2) return Result::Incomplete;
  if (frame[0] != START) return Result::BadFrame;
  size_t flen = frameLength(frame, len);
  if (len < flen) return Result::Incomplete;
  if (le16(frame + 3) == CTRL_COUNTER) return Result::CounterFrame;
  if (frame[flen - 1] != END || frame[flen - 2] != checksum(frame, flen)) return Result::BadFrame;
  if (le16(frame + 3) != CTRL_RESPONSE || le32(frame + 7) != loggerSerial || frame[11] != 0x02)
    return Result::BadFrame;
  if (frame[5] != sequence) return Result::WrongSequence;
  if (flen < MODBUS_OFFSET + 2) return Result::BadModbus;

  const uint8_t* mb = frame + MODBUS_OFFSET;
  size_t mbLen = flen - 2 - MODBUS_OFFSET;
  if (mbLen >= 2 && mb[0] == slaveId && mb[1] == (FN_READ_HOLDING | 0x80)) return Result::ModbusException;
  if (mbLen < 5) return Result::BadModbus;
  size_t need = 3 + 2 * (size_t)count + 2;
  // Деякі логери додають другу CRC або нулі в кінці (pysolarmanv5 _handle_double_crc) — беремо рівно need байтів.
  if (mbLen < need || mb[0] != slaveId || mb[1] != FN_READ_HOLDING || mb[2] != 2 * count)
    return Result::BadModbus;
  uint16_t crc = crc16Modbus(mb, need - 2);
  if (mb[need - 2] != (crc & 0xFF) || mb[need - 1] != (crc >> 8)) return Result::BadModbus;
  for (uint16_t i = 0; i < count; i++) values[i] = (mb[3 + 2 * i] << 8) | mb[4 + 2 * i];
  return Result::Ok;
}

const char* resultName(Result r) {
  switch (r) {
    case Result::Ok: return "ok";
    case Result::Incomplete: return "неповний кадр";
    case Result::CounterFrame: return "службовий кадр";
    case Result::BadFrame: return "пошкоджений кадр V5";
    case Result::WrongSequence: return "чужий номер запиту";
    case Result::ModbusException: return "помилка Modbus";
    case Result::BadModbus: return "пошкоджена відповідь Modbus";
  }
  return "?";
}

}  // namespace solarman
