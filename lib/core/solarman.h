#pragma once
// Кадр Solarman V5 для читання регістрів Deye через Wi-Fi логер (TCP 8899).
// ЛИШЕ ЧИТАННЯ: тут є тільки Modbus-функція 0x03. Функцій запису (0x06/0x10) немає і не буде.
// Еталон — pysolarmanv5 (_v5_frame_encoder / _v5_frame_decoder), тестові вектори — test/test_core.
#include <stddef.h>
#include <stdint.h>

namespace solarman {

constexpr size_t REQUEST_LEN = 36;          // 11 заголовок + 15 payload + 8 Modbus + 2 хвіст
constexpr uint16_t MAX_REGISTERS = 125;     // ліміт Modbus на один запит

uint16_t crc16Modbus(const uint8_t* data, size_t len);

// Будує запит «прочитати count holding-регістрів, починаючи з reg». Повертає довжину (REQUEST_LEN).
size_t buildReadRequest(uint8_t* out, uint32_t loggerSerial, uint8_t sequence,
                        uint8_t slaveId, uint16_t reg, uint16_t count);

enum class Result {
  Ok,
  Incomplete,      // кадр ще не дочитано (менше байтів, ніж каже поле довжини)
  CounterFrame,    // службовий кадр логера 0x4710 — ігнорувати й читати далі
  BadFrame,        // старт/кінець/контрольна сума/серійник/код керування/тип кадру
  WrongSequence,   // відповідь на інший запит
  ModbusException, // інвертор відповів кодом помилки
  BadModbus,       // CRC16, адреса, функція або кількість байтів не збігаються
};

// Скільки байтів займає кадр, що починається в buf (0 — ще невідомо, менше 3 байтів).
size_t frameLength(const uint8_t* buf, size_t len);

// Розбирає відповідь на buildReadRequest(..., count). У values — count значень uint16.
Result parseReadResponse(const uint8_t* frame, size_t len, uint32_t loggerSerial, uint8_t sequence,
                         uint8_t slaveId, uint16_t count, uint16_t* values);

const char* resultName(Result r);

}  // namespace solarman
