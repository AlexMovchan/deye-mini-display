#include "https.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "event_log.h"
#include "root_ca.h"

#include <mbedtls/x509_crt.h>
#include <time.h>

namespace https {

namespace {
// Корінь під конкретне джерело: увесь набір разом не вміщався в пам'ять (-10368), див. root_ca.h
const char* rootFor(const char* url) {
  if (strstr(url, "yasno.ua/")) return ROOT_GLOBALSIGN;
  return ROOT_LETSENCRYPT;  // siren.pp.ua, *.github.io
}

// Діагностика збою перевірки сертифіката (-9984): з'єднатися без перевірки, взяти ланцюжок сервера й перевірити
// його нашими кореневими вручну — mbedTLS тоді скаже, що саме не так (дата, підпис, ім'я, невідомий корінь).
// Лише читає й пише в журнал; дані з цього з'єднання не використовуються.
void diagnoseVerify(const String& host, const char* roots) {
  static int runs = 0;
  if (runs++ >= 3) return;  // кілька разів за запуск досить
  WiFiClientSecure c;
  c.setInsecure();
  c.setTimeout(5);
  if (!c.connect(host.c_str(), 443)) {
    evlog::add("TLS діагностика %s: без перевірки теж не з'єднався", host.c_str());
    return;
  }
  const mbedtls_x509_crt* peer = c.getPeerCertificate();
  mbedtls_x509_crt ca;
  mbedtls_x509_crt_init(&ca);
  int parsed = mbedtls_x509_crt_parse(&ca, (const unsigned char*)roots, strlen(roots) + 1);
  int chain = 0;
  for (const mbedtls_x509_crt* p = peer; p; p = p->next) chain++;
  uint32_t flags = 0;
  int ret = peer ? mbedtls_x509_crt_verify(const_cast<mbedtls_x509_crt*>(peer), &ca, nullptr, host.c_str(), &flags,
                                           nullptr, nullptr)
                 : -1;
  char info[200] = "";
  if (flags) mbedtls_x509_crt_verify_info(info, sizeof info, "", flags);
  for (char* q = info; *q; q++)
    if (*q == '\n') *q = ';';
  char subj[80] = "", iss[80] = "";
  if (peer) {
    mbedtls_x509_dn_gets(subj, sizeof subj, &peer->subject);
    mbedtls_x509_dn_gets(iss, sizeof iss, &peer->issuer);
  }
  evlog::add("TLS діагностика %s: корені %d (розбір %d), ланцюжок %d, verify %d, flags 0x%lx %s; час %ld",
             host.c_str(), parsed == 0 ? 1 : -1, parsed, chain, ret, (unsigned long)flags, info, (long)time(nullptr));
  evlog::add("TLS діагностика: сертифікат %s, видав %s", subj, iss);
  mbedtls_x509_crt_free(&ca);
  c.stop();
}
}  // namespace

Response get(const Request& req, const std::function<bool(Stream&)>& onBody) {
  Response res;
  WiFiClientSecure client;
  const char* roots = rootFor(req.url);
  client.setCACert(roots);
  const int timeout = req.timeoutMs > 0 ? req.timeoutMs : HTTP_TIMEOUT_MS;
  client.setTimeout((timeout + 999) / 1000);
  client.setHandshakeTimeout((timeout + 999) / 1000);
  HTTPClient http;
  http.setTimeout(timeout);
  http.setConnectTimeout(timeout);
  http.useHTTP10(true);  // без chunked — тіло можна парсити прямо з потоку
  http.setReuse(false);
  if (!http.begin(client, req.url)) {
    res.status = -100;
    return res;
  }
  http.setUserAgent(HTTP_USER_AGENT);
  http.addHeader("Accept-Encoding", "identity");  // siren.pp.ua інколи шле gzip без запиту
  if (req.bearer) http.addHeader("Authorization", String("Bearer ") + req.bearer);
  if (req.ifModifiedSince && *req.ifModifiedSince) http.addHeader("If-Modified-Since", req.ifModifiedSince);
  const char* keys[] = {"Last-Modified"};
  http.collectHeaders(keys, 1);
  res.status = http.GET();
  if (res.status < 0) {
    char tls[80] = "";
    int code = client.lastError(tls, sizeof tls);
    IPAddress ip;
    String url = req.url;
    String host = url.substring(8, url.indexOf('/', 8));  // https://<host>/...
    bool dns = WiFi.hostByName(host.c_str(), ip);
    snprintf(res.error, sizeof res.error, "TLS %d %s; DNS %s", code, tls, dns ? ip.toString().c_str() : "не знайшов");
    if (code == -9984) {
      http.end();
      client.stop();
      diagnoseVerify(host, roots);
      return res;
    }
  }
  if (res.status == 200) {
    snprintf(res.lastModified, sizeof res.lastModified, "%s", http.header("Last-Modified").c_str());
    Stream* s = http.getStreamPtr();
    if (s) {
      s->setTimeout(timeout);
      res.bodyOk = onBody(*s);
    }
  }
  http.end();
  return res;
}

}  // namespace https
