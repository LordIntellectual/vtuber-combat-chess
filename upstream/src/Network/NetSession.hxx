#ifndef VCC_NETWORK_NETSESSION_HXX_
#define VCC_NETWORK_NETSESSION_HXX_

#include "TcpSocket.hxx"
#include "NetProtocol.hxx"

#include <string>
#include <vector>
#include <cstdint>

/**
 * Host or client multiplayer session (VCC1).
 * Call pump() once per frame from the main thread.
 */
class NetSession {
public:
  enum Role { ROLE_NONE, ROLE_HOST, ROLE_CLIENT };
  enum Phase {
    PHASE_IDLE,
    PHASE_LISTENING,
    PHASE_CONNECTING,
    PHASE_HANDSHAKE,
    PHASE_INGAME,
    PHASE_CLOSED
  };

  NetSession();
  ~NetSession();

  Role role() const { return role_; }
  Phase phase() const { return phase_; }
  bool active() const {
    return phase_ == PHASE_LISTENING || phase_ == PHASE_CONNECTING ||
           phase_ == PHASE_HANDSHAKE || phase_ == PHASE_INGAME;
  }
  bool inGame() const { return phase_ == PHASE_INGAME; }
  bool peerConnected() const { return peer_.valid() && phase_ >= PHASE_HANDSHAKE &&
                                      phase_ != PHASE_CLOSED; }

  /** Host listens; default host=white, guest=black. */
  bool startHost(uint16_t port, std::string* err = nullptr);

  /** Client connects to host:port. */
  bool startClient(const std::string& host, uint16_t port,
                   const std::string& displayName = "Guest",
                   std::string* err = nullptr);

  void close(const std::string& reason = "closed");

  /** Non-blocking I/O + accept + handshake. */
  void pump();

  /** Pop next application message (after internal PING handled). */
  bool pollMessage(NetProtocol::Message& out);

  void send(const NetProtocol::Message& msg);

  /** Local side: "white" or "black" once known. */
  const std::string& localSide() const { return localSide_; }
  const std::string& remoteSide() const { return remoteSide_; }
  const std::string& sessionId() const { return sessionId_; }
  uint16_t port() const { return port_; }
  const std::string& statusLine() const { return status_; }
  const std::string& lastError() const { return lastError_; }

  /** True if a new peer just completed handshake this pump (host). */
  bool consumePeerJustJoined() {
    bool v = peerJustJoined_;
    peerJustJoined_ = false;
    return v;
  }

private:
  Role role_;
  Phase phase_;
  uint16_t port_;
  TcpSocket listen_;
  TcpSocket peer_;
  NetProtocol::LineBuffer lines_;
  std::string sendPending_;
  std::string localSide_;
  std::string remoteSide_;
  std::string sessionId_;
  std::string displayName_;
  std::string status_;
  std::string lastError_;
  std::string joinHost_;
  std::vector<NetProtocol::Message> inbox_;
  bool peerJustJoined_;
  double lastPingTime_;
  double connectStartTime_;

  void handleLine(const std::string& line);
  void setError(const std::string& e);
  static double nowSec();
  static std::string randomSessionId();
};

#endif
