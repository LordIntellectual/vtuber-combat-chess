#include "NetSession.hxx"

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace {
const double CONNECT_TIMEOUT_SEC = 8.0;
const double PING_INTERVAL_SEC = 10.0;
}

double NetSession::nowSec() {
#ifdef _WIN32
  return GetTickCount64() / 1000.0;
#else
  timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec + tv.tv_usec / 1e6;
#endif
}

std::string NetSession::randomSessionId() {
  static bool seeded = false;
  if (!seeded) {
    std::srand((unsigned)std::time(nullptr));
    seeded = true;
  }
  std::ostringstream os;
  os << std::hex << (std::rand() & 0xffff) << (std::rand() & 0xffff);
  return os.str();
}

NetSession::NetSession()
  : role_(ROLE_NONE),
    phase_(PHASE_IDLE),
    port_(7777),
    peerJustJoined_(false),
    lastPingTime_(0),
    connectStartTime_(0),
    relayMode_(false),
    relayPaired_(false),
    relayHelloSent_(false) {}

NetSession::~NetSession() { close("dtor"); }

void NetSession::setError(const std::string& e) {
  lastError_ = e;
  status_ = e;
  std::cerr << "[VCC-NET] " << e << std::endl;
}

bool NetSession::startHost(uint16_t port, std::string* err) {
  close("restart");
  relayMode_ = false;
  role_ = ROLE_HOST;
  port_ = port;
  localSide_ = "white";
  remoteSide_ = "black";
  sessionId_ = randomSessionId();
  std::string e;
  if (!listen_.listenOn(port, &e)) {
    setError(e);
    if (err) *err = e;
    role_ = ROLE_NONE;
    phase_ = PHASE_IDLE;
    return false;
  }
  phase_ = PHASE_LISTENING;
  status_ = "Hosting on port " + std::to_string(port) + " — waiting for guest";
  std::cout << "[VCC-NET] listening on 0.0.0.0:" << port << std::endl;
  return true;
}

bool NetSession::startClient(const std::string& host, uint16_t port,
                             const std::string& displayName, std::string* err) {
  close("restart");
  relayMode_ = false;
  role_ = ROLE_CLIENT;
  port_ = port;
  joinHost_ = host;
  displayName_ = displayName.empty() ? "Guest" : displayName;
  // provisional until WELCOME
  localSide_ = "black";
  remoteSide_ = "white";
  std::string e;
  if (!peer_.connectBegin(host, port, &e)) {
    setError(e);
    if (err) *err = e;
    role_ = ROLE_NONE;
    phase_ = PHASE_IDLE;
    return false;
  }
  phase_ = PHASE_CONNECTING;
  connectStartTime_ = nowSec();
  status_ = "Connecting to " + host + ":" + std::to_string(port);
  std::cout << "[VCC-NET] connecting to " << host << ":" << port << std::endl;
  return true;
}

bool NetSession::startRelay(const std::string& relayHost, uint16_t relayPort,
                            const std::string& token, const std::string& role,
                            std::string* err) {
  close("restart");
  relayMode_ = true;
  relayPaired_ = false;
  relayHelloSent_ = false;
  relayToken_ = token;
  relayRole_ = role;
  port_ = relayPort;
  joinHost_ = relayHost;
  sessionId_ = randomSessionId();
  if (role == "host") {
    role_ = ROLE_HOST;
    localSide_ = "white";
    remoteSide_ = "black";
  } else {
    role_ = ROLE_CLIENT;
    localSide_ = "black";
    remoteSide_ = "white";
  }
  displayName_ = role == "host" ? "Host" : "Guest";
  std::string e;
  if (!peer_.connectBegin(relayHost, relayPort, &e)) {
    setError(e);
    if (err) *err = e;
    role_ = ROLE_NONE;
    phase_ = PHASE_IDLE;
    relayMode_ = false;
    return false;
  }
  phase_ = PHASE_CONNECTING;
  connectStartTime_ = nowSec();
  status_ = "Relay connecting…";
  std::cout << "[VCC-NET] relay " << role << " → " << relayHost << ":" << relayPort
            << std::endl;
  return true;
}

void NetSession::close(const std::string& reason) {
  if (peer_.valid() && phase_ != PHASE_IDLE && phase_ != PHASE_CLOSED) {
    if (!relayMode_ || relayPaired_) {
      NetProtocol::Message gb = NetProtocol::makeGoodbye(reason);
      std::string dummy;
      peer_.sendAll(NetProtocol::encode(gb), sendPending_);
    }
  }
  peer_.close();
  listen_.close();
  lines_.clear();
  sendPending_.clear();
  inbox_.clear();
  phase_ = (role_ == ROLE_NONE) ? PHASE_IDLE : PHASE_CLOSED;
  if (role_ != ROLE_NONE) {
    status_ = std::string("Disconnected: ") + reason;
    std::cout << "[VCC-NET] disconnect reason=" << reason << std::endl;
  }
  role_ = ROLE_NONE;
  peerJustJoined_ = false;
  relayMode_ = false;
  relayPaired_ = false;
  relayHelloSent_ = false;
  relayToken_.clear();
  relayRole_.clear();
}

void NetSession::send(const NetProtocol::Message& msg) {
  if (!peer_.valid()) return;
  if (!peer_.sendAll(NetProtocol::encode(msg), sendPending_)) {
    setError("send failed");
    close("send_error");
  }
}

bool NetSession::pollMessage(NetProtocol::Message& out) {
  if (inbox_.empty()) return false;
  out = inbox_.front();
  inbox_.erase(inbox_.begin());
  return true;
}

void NetSession::handleRelayLine(const std::string& line) {
  // VCCREL1 TYPE key=val...
  std::istringstream iss(line);
  std::string magic, type;
  iss >> magic >> type;
  if (magic != "VCCREL1") return;
  if (type == "WELCOME") {
    status_ = "Relay OK — waiting for opponent…";
    std::cout << "[VCC-NET] relay WELCOME " << line << std::endl;
    return;
  }
  if (type == "PAIRED") {
    relayPaired_ = true;
    std::cout << "[VCC-NET] relay PAIRED\n";
    if (role_ == ROLE_HOST) {
      // Wait for guest VCC1 HELLO over the pipe
      phase_ = PHASE_HANDSHAKE;
      status_ = "Opponent connected — starting…";
    } else {
      // Guest sends VCC1 HELLO to host through relay
      phase_ = PHASE_HANDSHAKE;
      status_ = "Paired — handshake";
      send(NetProtocol::makeHello(displayName_));
    }
    lastPingTime_ = nowSec();
    return;
  }
  if (type == "PEER_LEFT") {
    close("peer_left");
    return;
  }
  if (type == "ERROR") {
    setError("relay error: " + line);
    close("relay_error");
    return;
  }
}

void NetSession::handleLine(const std::string& line) {
  if (line.size() >= 7 && line.compare(0, 7, "VCCREL1") == 0) {
    handleRelayLine(line);
    return;
  }

  NetProtocol::Message msg;
  if (!NetProtocol::decode(line, msg)) {
    std::cerr << "[VCC-NET] bad line ignored\n";
    return;
  }

  if (msg.type == "PING") {
    long long t = 0;
    try { t = std::stoll(msg.get("t", "0")); } catch (...) {}
    send(NetProtocol::makePong(t));
    return;
  }
  if (msg.type == "PONG") {
    return;
  }
  if (msg.type == "GOODBYE") {
    close(msg.get("reason", "peer_goodbye"));
    return;
  }

  if (role_ == ROLE_HOST && phase_ == PHASE_HANDSHAKE && msg.type == "HELLO") {
    // Assign sides and start
    send(NetProtocol::makeWelcome(sessionId_, "black", "white"));
    peerJustJoined_ = true;
    phase_ = PHASE_INGAME;
    status_ = "In game (host, white) — guest joined";
    std::cout << "[VCC-NET] session start host=white guest=black session="
              << sessionId_ << std::endl;
    // START is sent by the app after reset/sync (has access to ChessGame FEN)
    inbox_.push_back(msg); // app may ignore HELLO
    return;
  }

  if (role_ == ROLE_CLIENT && phase_ == PHASE_HANDSHAKE && msg.type == "WELCOME") {
    sessionId_ = msg.get("session", sessionId_);
    localSide_ = msg.get("you", "black");
    remoteSide_ = msg.get("host", "white");
    phase_ = PHASE_INGAME;
    status_ = "In game (client, " + localSide_ + ")";
    std::cout << "[VCC-NET] session start you=" << localSide_
              << " host=" << remoteSide_ << " session=" << sessionId_
              << std::endl;
    inbox_.push_back(msg);
    return;
  }

  if (phase_ == PHASE_INGAME || phase_ == PHASE_HANDSHAKE) {
    inbox_.push_back(msg);
  }
}

void NetSession::pump() {
  if (phase_ == PHASE_IDLE || phase_ == PHASE_CLOSED) return;

  // Client / relay connect progress
  if (phase_ == PHASE_CONNECTING) {
    std::string e;
    TcpSocket::ConnectStatus st = peer_.connectPoll(&e);
    if (st == TcpSocket::CONN_PENDING) {
      if (nowSec() - connectStartTime_ > CONNECT_TIMEOUT_SEC) {
        setError("connect timeout");
        close("timeout");
      }
      return;
    }
    if (st == TcpSocket::CONN_FAILED) {
      setError(e.empty() ? "connect failed" : e);
      close("connect_failed");
      return;
    }
    if (relayMode_) {
      // Send relay HELLO (not VCC1 yet)
      std::string hello = "VCCREL1 HELLO token=" + relayToken_ +
                          " role=" + relayRole_ + "\n";
      if (!peer_.sendAll(hello, sendPending_)) {
        setError("relay hello send failed");
        close("send_error");
        return;
      }
      relayHelloSent_ = true;
      phase_ = PHASE_HANDSHAKE;
      status_ = "Relay connected — waiting to pair…";
      lastPingTime_ = nowSec();
    } else {
      // Direct client — send VCC1 HELLO
      phase_ = PHASE_HANDSHAKE;
      status_ = "Connected — handshake";
      send(NetProtocol::makeHello(displayName_));
      lastPingTime_ = nowSec();
    }
  }

  // Host accept (only one peer) — direct mode only
  if (!relayMode_ && phase_ == PHASE_LISTENING && !peer_.valid()) {
    std::string peerAddr;
    if (listen_.tryAccept(peer_, &peerAddr)) {
      phase_ = PHASE_HANDSHAKE;
      status_ = "Peer connected from " + peerAddr + " — handshake";
      std::cout << "[VCC-NET] client connected from " << peerAddr << std::endl;
      lines_.clear();
      sendPending_.clear();
      lastPingTime_ = nowSec();
    }
  }

  if (!peer_.valid()) return;

  // Flush pending send
  if (!sendPending_.empty()) {
    if (!peer_.sendAll("", sendPending_)) {
      close("send_error");
      return;
    }
  }

  // Recv
  char buf[2048];
  for (;;) {
    int n = peer_.recvRaw(buf, sizeof(buf));
    if (n < 0) {
      close("peer_closed");
      return;
    }
    if (n == 0) break;
    lines_.append(buf, n);
  }

  std::string line;
  while (lines_.popLine(line)) {
    handleLine(line);
    if (phase_ == PHASE_CLOSED || phase_ == PHASE_IDLE) return;
  }

  // Keepalive
  if (phase_ == PHASE_INGAME && nowSec() - lastPingTime_ >= PING_INTERVAL_SEC) {
    long long t = (long long)(nowSec() * 1000.0);
    send(NetProtocol::makePing(t));
    lastPingTime_ = nowSec();
  }
}
