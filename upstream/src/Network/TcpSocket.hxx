#ifndef VCC_NETWORK_TCPSOCKET_HXX_
#define VCC_NETWORK_TCPSOCKET_HXX_

#include <string>
#include <cstdint>

/**
 * Minimal cross-platform TCP helper (Linux BSD sockets / Windows Winsock).
 * Non-blocking by default for use in the game loop.
 */
class TcpSocket {
public:
  TcpSocket();
  ~TcpSocket();

  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;

  /** One-time process init (WSAStartup on Windows). Safe to call repeatedly. */
  static bool globalInit(std::string* err = nullptr);
  static void globalShutdown();

  void close();
  bool valid() const { return fd_ >= 0; }

  /** Server: bind 0.0.0.0:port and listen. */
  bool listenOn(uint16_t port, std::string* err = nullptr);

  /** Non-blocking accept. Returns false if no pending connection. */
  bool tryAccept(TcpSocket& out, std::string* peerAddr = nullptr);

  /** Non-blocking connect start; poll with connectPoll(). */
  bool connectBegin(const std::string& host, uint16_t port, std::string* err = nullptr);

  enum ConnectStatus { CONN_PENDING, CONN_OK, CONN_FAILED };
  ConnectStatus connectPoll(std::string* err = nullptr);

  /** Non-blocking send; returns bytes sent (>=0) or -1 on hard error. */
  int sendRaw(const char* data, int len);

  /** Non-blocking recv into buf; returns bytes, 0 would-block, -1 error/closed. */
  int recvRaw(char* buf, int capacity);

  /** Send all of a string, buffering remainder if needed (caller may retry). */
  bool sendAll(const std::string& data, std::string& pendingOut);

  static bool parseHostPort(const std::string& s, std::string& host, uint16_t& port);

private:
  int fd_;
  bool connecting_;
  bool listening_;

  void setNonBlocking();
  explicit TcpSocket(int fd);
};

#endif
