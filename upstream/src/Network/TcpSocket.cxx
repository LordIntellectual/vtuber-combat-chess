#include "TcpSocket.hxx"

#include <cstring>
#include <sstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define VCC_SOCK_ERR SOCKET_ERROR
#define VCC_INVALID INVALID_SOCKET
static int vccLastErr() { return WSAGetLastError(); }
static bool vccWouldBlock(int e) {
  return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
}
static void vccCloseFd(int fd) { closesocket(fd); }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define VCC_SOCK_ERR (-1)
#define VCC_INVALID (-1)
static int vccLastErr() { return errno; }
static bool vccWouldBlock(int e) {
  return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS || e == EALREADY;
}
// Must use ::close — TcpSocket::close would otherwise hide it
static void vccCloseFd(int fd) { ::close(fd); }
#endif

namespace {
int g_initCount = 0;
}

bool TcpSocket::globalInit(std::string* err) {
#ifdef _WIN32
  if (g_initCount == 0) {
    WSADATA wsa;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) {
      if (err) *err = "WSAStartup failed";
      return false;
    }
  }
#endif
  ++g_initCount;
  return true;
}

void TcpSocket::globalShutdown() {
  if (g_initCount <= 0) return;
  --g_initCount;
#ifdef _WIN32
  if (g_initCount == 0) WSACleanup();
#endif
}

TcpSocket::TcpSocket() : fd_(-1), connecting_(false), listening_(false) {}

TcpSocket::TcpSocket(int fd) : fd_(fd), connecting_(false), listening_(false) {
  if (fd_ >= 0) setNonBlocking();
}

TcpSocket::~TcpSocket() { close(); }

void TcpSocket::close() {
  if (fd_ >= 0) {
    vccCloseFd(fd_);
    fd_ = -1;
  }
  connecting_ = false;
  listening_ = false;
}

void TcpSocket::setNonBlocking() {
  if (fd_ < 0) return;
#ifdef _WIN32
  u_long mode = 1;
  ioctlsocket(fd_, FIONBIO, &mode);
#else
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags >= 0) fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool TcpSocket::listenOn(uint16_t port, std::string* err) {
  close();
  if (!globalInit(err)) return false;

  fd_ = (int)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd_ < 0) {
    if (err) *err = "socket() failed";
    return false;
  }

  int yes = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#ifdef SO_REUSEPORT
  setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, (const char*)&yes, sizeof(yes));
#endif

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  if (bind(fd_, (sockaddr*)&addr, sizeof(addr)) == VCC_SOCK_ERR) {
    if (err) *err = "bind() failed (port in use?)";
    close();
    return false;
  }
  if (listen(fd_, 2) == VCC_SOCK_ERR) {
    if (err) *err = "listen() failed";
    close();
    return false;
  }
  setNonBlocking();
  listening_ = true;
  return true;
}

bool TcpSocket::tryAccept(TcpSocket& out, std::string* peerAddr) {
  if (fd_ < 0 || !listening_) return false;
  sockaddr_in peer;
  socklen_t plen = sizeof(peer);
  int cfd = (int)::accept(fd_, (sockaddr*)&peer, &plen);
  if (cfd < 0) {
    return false; // would-block or transient
  }
  out.close();
  out.fd_ = cfd;
  out.connecting_ = false;
  out.listening_ = false;
  out.setNonBlocking();
  int one = 1;
  setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
  if (peerAddr) {
    char buf[64];
    const char* p = inet_ntop(AF_INET, &peer.sin_addr, buf, sizeof(buf));
    *peerAddr = p ? p : "?";
  }
  return true;
}

bool TcpSocket::connectBegin(const std::string& host, uint16_t port, std::string* err) {
  close();
  if (!globalInit(err)) return false;

  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  std::ostringstream ps;
  ps << port;
  addrinfo* res = nullptr;
  int gr = getaddrinfo(host.c_str(), ps.str().c_str(), &hints, &res);
  if (gr != 0 || !res) {
    if (err) *err = std::string("resolve failed: ") + host;
    return false;
  }

  fd_ = (int)::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd_ < 0) {
    freeaddrinfo(res);
    if (err) *err = "socket() failed";
    return false;
  }
  setNonBlocking();
  int one = 1;
  setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));

  int r = ::connect(fd_, res->ai_addr, (socklen_t)res->ai_addrlen);
  freeaddrinfo(res);
  if (r == 0) {
    connecting_ = false;
    return true;
  }
  int e = vccLastErr();
  if (vccWouldBlock(e)
#ifdef _WIN32
      || e == WSAEINVAL
#endif
  ) {
    connecting_ = true;
    return true;
  }
  if (err) *err = "connect() failed";
  close();
  return false;
}

TcpSocket::ConnectStatus TcpSocket::connectPoll(std::string* err) {
  if (fd_ < 0) {
    if (err) *err = "no socket";
    return CONN_FAILED;
  }
  if (!connecting_) return CONN_OK;

  fd_set wset, eset;
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  FD_SET((unsigned)fd_, &wset);
  FD_SET((unsigned)fd_, &eset);
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  int sel = select(fd_ + 1, nullptr, &wset, &eset, &tv);
  if (sel < 0) {
    if (err) *err = "select() failed";
    return CONN_FAILED;
  }
  if (sel == 0) return CONN_PENDING;

  int soerr = 0;
  socklen_t sl = sizeof(soerr);
  if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, (char*)&soerr, &sl) != 0) {
    if (err) *err = "getsockopt failed";
    return CONN_FAILED;
  }
  if (soerr != 0) {
    if (err) *err = "connection refused or timed out";
    close();
    return CONN_FAILED;
  }
  connecting_ = false;
  return CONN_OK;
}

int TcpSocket::sendRaw(const char* data, int len) {
  if (fd_ < 0 || len <= 0) return -1;
#ifdef _WIN32
  int n = ::send(fd_, data, len, 0);
#else
  int n = (int)::send(fd_, data, (size_t)len, MSG_NOSIGNAL);
#endif
  if (n < 0) {
    int e = vccLastErr();
    if (vccWouldBlock(e)) return 0;
    return -1;
  }
  return n;
}

int TcpSocket::recvRaw(char* buf, int capacity) {
  if (fd_ < 0 || capacity <= 0) return -1;
#ifdef _WIN32
  int n = ::recv(fd_, buf, capacity, 0);
#else
  int n = (int)::recv(fd_, buf, (size_t)capacity, 0);
#endif
  if (n < 0) {
    int e = vccLastErr();
    if (vccWouldBlock(e)) return 0;
    return -1;
  }
  if (n == 0) return -1; // peer closed
  return n;
}

bool TcpSocket::sendAll(const std::string& data, std::string& pendingOut) {
  std::string blob = pendingOut + data;
  pendingOut.clear();
  int off = 0;
  while (off < (int)blob.size()) {
    int n = sendRaw(blob.data() + off, (int)blob.size() - off);
    if (n < 0) return false;
    if (n == 0) {
      pendingOut.assign(blob.begin() + off, blob.end());
      return true;
    }
    off += n;
  }
  return true;
}

bool TcpSocket::parseHostPort(const std::string& s, std::string& host, uint16_t& port) {
  if (s.empty()) return false;
  // [ipv6]:port not supported in MVP; host:port only
  std::string::size_type colon = s.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 >= s.size())
    return false;
  host = s.substr(0, colon);
  int p = 0;
  try {
    p = std::stoi(s.substr(colon + 1));
  } catch (...) {
    return false;
  }
  if (p <= 0 || p > 65535) return false;
  port = (uint16_t)p;
  return true;
}
