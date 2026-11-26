#include "https.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "root_ca.h"

namespace https {

Response get(const Request& req, const std::function<bool(Stream&)>& onBody) {
  Response res;
  WiFiClientSecure client;
  client.setCACert(ROOT_CA_PEM);
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
