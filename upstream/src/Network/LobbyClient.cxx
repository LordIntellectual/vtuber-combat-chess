#include "LobbyClient.hxx"
#include "TcpSocket.hxx"

#include <cstdlib>
#include <sstream>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

std::string jsonGetString(const std::string& json, const std::string& key) {
  // naive "key":"value" extractor (good enough for our controlled API)
  std::string pat = "\"" + key + "\"";
  std::string::size_type k = json.find(pat);
  if (k == std::string::npos) return "";
  k = json.find(':', k + pat.size());
  if (k == std::string::npos) return "";
  k = json.find('"', k);
  if (k == std::string::npos) return "";
  std::string::size_type e = json.find('"', k + 1);
  if (e == std::string::npos) return "";
  return json.substr(k + 1, e - k - 1);
}

bool jsonGetBool(const std::string& json, const std::string& key, bool def = false) {
  std::string pat = "\"" + key + "\"";
  std::string::size_type k = json.find(pat);
  if (k == std::string::npos) return def;
  k = json.find(':', k + pat.size());
  if (k == std::string::npos) return def;
  while (k < json.size() && (json[k] == ':' || json[k] == ' ')) k++;
  if (json.compare(k, 4, "true") == 0) return true;
  if (json.compare(k, 5, "false") == 0) return false;
  return def;
}

int jsonGetInt(const std::string& json, const std::string& key, int def = 0) {
  std::string pat = "\"" + key + "\"";
  std::string::size_type k = json.find(pat);
  if (k == std::string::npos) return def;
  k = json.find(':', k + pat.size());
  if (k == std::string::npos) return def;
  while (k < json.size() && (json[k] == ':' || json[k] == ' ')) k++;
  try {
    return std::stoi(json.substr(k));
  } catch (...) {
    return def;
  }
}

std::string jsonEscape(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '"' || c == '\\') o.push_back('\\');
    if (c == '\n' || c == '\r') continue;
    o.push_back(c);
  }
  return o;
}

} // namespace

LobbyClient::LobbyClient() : host_("93.127.215.17"), port_(8080) {
  if (const char* e = std::getenv("VCC_LOBBY")) {
    // host:port
    std::string h;
    uint16_t p = 8080;
    if (TcpSocket::parseHostPort(e, h, p)) {
      host_ = h;
      port_ = p;
    } else if (e[0]) {
      host_ = e;
    }
  }
  if (const char* h = std::getenv("VCC_LOBBY_HOST")) {
    if (h[0]) host_ = h;
  }
  if (const char* p = std::getenv("VCC_LOBBY_PORT")) {
    int v = std::atoi(p);
    if (v > 0 && v < 65536) port_ = (uint16_t)v;
  }
}

void LobbyClient::setEndpoint(const std::string& host, uint16_t port) {
  host_ = host;
  port_ = port;
}

bool LobbyClient::httpJson(const std::string& method, const std::string& path,
                           const std::string& bodyJson, int& statusOut,
                           std::string& bodyOut, std::string* err) {
  statusOut = 0;
  bodyOut.clear();
  TcpSocket sock;
  std::string e;
  if (!sock.connectBegin(host_, port_, &e)) {
    if (err) *err = e.empty() ? "connect failed" : e;
    return false;
  }
  // Wait up to ~3s for connect
  for (int i = 0; i < 60; ++i) {
    TcpSocket::ConnectStatus st = sock.connectPoll(&e);
    if (st == TcpSocket::CONN_OK) break;
    if (st == TcpSocket::CONN_FAILED) {
      if (err) *err = e.empty() ? "connect failed" : e;
      return false;
    }
    // sleep 50ms
#ifdef _WIN32
    Sleep(50);
#else
    usleep(50 * 1000);
#endif
  }
  if (!sock.valid()) {
    if (err) *err = "connect timeout";
    return false;
  }

  std::ostringstream req;
  req << method << " " << path << " HTTP/1.1\r\n"
      << "Host: " << host_ << "\r\n"
      << "Connection: close\r\n"
      << "Accept: application/json\r\n";
  if (!bodyJson.empty()) {
    req << "Content-Type: application/json\r\n"
        << "Content-Length: " << bodyJson.size() << "\r\n";
  }
  req << "\r\n";
  if (!bodyJson.empty()) req << bodyJson;

  std::string pending;
  if (!sock.sendAll(req.str(), pending)) {
    if (err) *err = "send failed";
    return false;
  }
  // flush remaining
  for (int i = 0; i < 40 && !pending.empty(); ++i) {
#ifdef _WIN32
    Sleep(25);
#else
    usleep(25 * 1000);
#endif
    if (!sock.sendAll("", pending)) {
      if (err) *err = "send failed";
      return false;
    }
  }

  std::string raw;
  char buf[2048];
  for (int i = 0; i < 200; ++i) {
    int n = sock.recvRaw(buf, sizeof(buf));
    if (n < 0) break;
    if (n == 0) {
#ifdef _WIN32
      Sleep(20);
#else
      usleep(20 * 1000);
#endif
      continue;
    }
    raw.append(buf, buf + n);
    if (raw.size() > 256 * 1024) break;
  }
  sock.close();

  if (raw.empty()) {
    if (err) *err = "empty response";
    return false;
  }
  // status line
  std::string::size_type sp1 = raw.find(' ');
  std::string::size_type sp2 = raw.find(' ', sp1 + 1);
  if (sp1 != std::string::npos && sp2 != std::string::npos) {
    try {
      statusOut = std::stoi(raw.substr(sp1 + 1, sp2 - sp1 - 1));
    } catch (...) {
      statusOut = 0;
    }
  }
  std::string::size_type hdrEnd = raw.find("\r\n\r\n");
  if (hdrEnd == std::string::npos) {
    if (err) *err = "bad http";
    return false;
  }
  bodyOut = raw.substr(hdrEnd + 4);
  return true;
}

bool LobbyClient::health(std::string* err) {
  int st = 0;
  std::string body;
  if (!httpJson("GET", "/health", "", st, body, err)) return false;
  return st == 200 && body.find("\"ok\"") != std::string::npos;
}

bool LobbyClient::listRooms(std::vector<LobbyRoom>& out, std::string* err) {
  out.clear();
  int st = 0;
  std::string body;
  if (!httpJson("GET", "/rooms", "", st, body, err)) return false;
  if (st != 200) {
    if (err) *err = "list failed status=" + std::to_string(st);
    return false;
  }
  // Split on room objects roughly: find each "id"
  std::string::size_type pos = 0;
  while ((pos = body.find("\"id\"", pos)) != std::string::npos) {
    // take a window around this room
    std::string::size_type start = body.rfind('{', pos);
    std::string::size_type end = body.find('}', pos);
    if (start == std::string::npos || end == std::string::npos) {
      pos += 4;
      continue;
    }
    std::string obj = body.substr(start, end - start + 1);
    LobbyRoom r;
    r.id = jsonGetString(obj, "id");
    r.name = jsonGetString(obj, "name");
    r.players = jsonGetInt(obj, "players", 0);
    r.maxPlayers = jsonGetInt(obj, "max_players", 2);
    r.hasPassword = jsonGetBool(obj, "has_password", false);
    r.full = jsonGetBool(obj, "full", false);
    if (!r.id.empty()) out.push_back(r);
    pos = end + 1;
  }
  return true;
}

LobbySession LobbyClient::createRoom(const std::string& name,
                                     const std::string& password,
                                     std::string* err) {
  LobbySession s;
  s.ok = false;
  s.relayPort = 7777;
  std::ostringstream body;
  body << "{\"name\":\"" << jsonEscape(name) << "\",\"password\":\""
       << jsonEscape(password) << "\"}";
  int st = 0;
  std::string resp;
  if (!httpJson("POST", "/rooms", body.str(), st, resp, err)) return s;
  bool okBody = resp.find("\"ok\": true") != std::string::npos ||
                resp.find("\"ok\":true") != std::string::npos;
  if (st != 200 || !okBody) {
    s.error = jsonGetString(resp, "error");
    if (s.error.empty()) s.error = "create failed";
    if (err) *err = s.error;
    return s;
  }
  s.ok = true;
  s.roomId = jsonGetString(resp, "id");
  // id may be nested under room — try again from room blob
  if (s.roomId.empty()) {
    std::string::size_type r = resp.find("\"room\"");
    if (r != std::string::npos)
      s.roomId = jsonGetString(resp.substr(r), "id");
  }
  s.roomName = name;
  s.role = jsonGetString(resp, "role");
  if (s.role.empty()) s.role = "host";
  s.token = jsonGetString(resp, "token");
  s.relayPort = (uint16_t)jsonGetInt(resp, "relay_port", 7777);
  return s;
}

LobbySession LobbyClient::joinRoom(const std::string& roomId,
                                   const std::string& password,
                                   std::string* err) {
  LobbySession s;
  s.ok = false;
  s.relayPort = 7777;
  std::ostringstream body;
  body << "{\"password\":\"" << jsonEscape(password) << "\"}";
  std::string path = "/rooms/" + roomId + "/join";
  int st = 0;
  std::string resp;
  if (!httpJson("POST", path, body.str(), st, resp, err)) return s;
  if (st != 200) {
    s.error = jsonGetString(resp, "error");
    if (s.error.empty()) s.error = "join failed";
    if (err) *err = s.error;
    return s;
  }
  s.ok = true;
  s.roomId = roomId;
  s.role = jsonGetString(resp, "role");
  if (s.role.empty()) s.role = "guest";
  s.token = jsonGetString(resp, "token");
  s.relayPort = (uint16_t)jsonGetInt(resp, "relay_port", 7777);
  s.roomName = jsonGetString(resp, "name");
  return s;
}
