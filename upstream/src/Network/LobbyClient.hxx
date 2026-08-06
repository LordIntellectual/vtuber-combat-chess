#ifndef VCC_NETWORK_LOBBYCLIENT_HXX_
#define VCC_NETWORK_LOBBYCLIENT_HXX_

#include <string>
#include <vector>
#include <cstdint>

/** Room entry from GET /rooms */
struct LobbyRoom {
  std::string id;
  std::string name;
  int players;
  int maxPlayers;
  bool hasPassword;
  bool full;
};

struct LobbySession {
  bool ok;
  std::string error;
  std::string roomId;
  std::string roomName;
  std::string role;   // "host" | "guest"
  std::string token;
  uint16_t relayPort;
};

/**
 * Minimal blocking HTTP/1.1 client for the lobby API (stdlib sockets).
 * Call from main thread sparingly (create/join/list); keep timeouts short.
 */
class LobbyClient {
public:
  LobbyClient();

  /** Default public multiplayer endpoint (VPS). Override with VCC_LOBBY env. */
  void setEndpoint(const std::string& host, uint16_t port);
  const std::string& host() const { return host_; }
  uint16_t port() const { return port_; }

  bool health(std::string* err = nullptr);
  bool listRooms(std::vector<LobbyRoom>& out, std::string* err = nullptr);
  LobbySession createRoom(const std::string& name, const std::string& password,
                          std::string* err = nullptr);
  LobbySession joinRoom(const std::string& roomId, const std::string& password,
                        std::string* err = nullptr);

private:
  std::string host_;
  uint16_t port_;

  bool httpJson(const std::string& method, const std::string& path,
                const std::string& bodyJson, int& statusOut,
                std::string& bodyOut, std::string* err);
};

#endif
