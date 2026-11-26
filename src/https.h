#pragma once
// HTTPS GET з перевіркою сертифіката (root_ca.h). Запити строго по черзі — один клієнт за раз.
#include <Arduino.h>

#include <functional>

namespace https {

struct Request {
  const char* url;
  const char* bearer = nullptr;           // Authorization: Bearer …
  const char* ifModifiedSince = nullptr;  // для 304
};

struct Response {
  int status = 0;            // HTTP-код або < 0 — помилка з'єднання/TLS
  char lastModified[40] = "";
  bool bodyOk = false;       // onBody повернув true
};

// onBody отримує потік тіла (HTTP/1.0, без chunked) лише для відповіді 200.
Response get(const Request& req, const std::function<bool(Stream&)>& onBody);

}  // namespace https
