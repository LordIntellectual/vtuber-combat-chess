/* Headless host/client smoke test for VCC1 (no OpenGL).
 * Build from repo root:
 *   c++ -std=c++11 -I upstream/src \
 *     tools/net_smoke_test.cxx \
 *     upstream/src/Network/TcpSocket.cxx \
 *     upstream/src/Network/NetProtocol.cxx \
 *     upstream/src/Network/NetSession.cxx \
 *     -o /tmp/vcc_net_smoke && /tmp/vcc_net_smoke
 */
#include "Network/NetSession.hxx"
#include "Network/NetProtocol.hxx"
#include "Network/TcpSocket.hxx"

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>

int main() {
  const uint16_t port = 17991;
  std::string err;

  NetSession host;
  if (!host.startHost(port, &err)) {
    std::cerr << "host fail: " << err << "\n";
    return 1;
  }

  NetSession client;
  if (!client.startClient("127.0.0.1", port, "SmokeGuest", &err)) {
    std::cerr << "client fail: " << err << "\n";
    return 1;
  }

  bool hostInGame = false, clientInGame = false;
  bool gotHello = false, gotWelcome = false;
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5)) {
    host.pump();
    client.pump();
    if (host.consumePeerJustJoined()) {
      host.send(NetProtocol::makeStart(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", "w", 0));
      hostInGame = true;
    }
    NetProtocol::Message m;
    while (host.pollMessage(m)) {
      if (m.type == "HELLO") gotHello = true;
    }
    while (client.pollMessage(m)) {
      if (m.type == "WELCOME") {
        gotWelcome = true;
        clientInGame = true;
      }
      if (m.type == "START") {
        std::cout << "client got START fen=" << m.get("fen").substr(0, 20)
                  << "...\n";
      }
      if (m.type == "MOVE") {
        std::cout << "client got MOVE " << m.get("uci") << "\n";
      }
    }
    if (hostInGame && clientInGame && gotWelcome) {
      // Host broadcasts a sample move
      host.send(NetProtocol::makeMove("e2e4", 1, "w"));
      // drain
      for (int i = 0; i < 20; ++i) {
        host.pump();
        client.pump();
        while (client.pollMessage(m)) {
          if (m.type == "MOVE" && m.get("uci") == "e2e4") {
            std::cout << "PASS: host/client handshake + MOVE e2e4\n";
            host.close("done");
            client.close("done");
            TcpSocket::globalShutdown();
            return 0;
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::cerr << "FAIL hostInGame=" << hostInGame
            << " clientInGame=" << clientInGame
            << " hello=" << gotHello
            << " welcome=" << gotWelcome
            << " hostPhase=" << (int)host.phase()
            << " clientPhase=" << (int)client.phase()
            << "\n";
  host.close("fail");
  client.close("fail");
  return 1;
}
