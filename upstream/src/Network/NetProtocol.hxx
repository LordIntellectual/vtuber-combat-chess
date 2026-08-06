#ifndef VCC_NETWORK_NETPROTOCOL_HXX_
#define VCC_NETWORK_NETPROTOCOL_HXX_

#include <string>
#include <map>
#include <vector>

/** VCC1 line protocol — see docs/MULTIPLAYER_DESIGN.md */
namespace NetProtocol {

const char* const MAGIC = "VCC1";
const int PROTO_VERSION = 1;
const size_t MAX_LINE = 4096;

struct Message {
  std::string type;
  std::map<std::string, std::string> kv;

  std::string get(const std::string& key, const std::string& def = "") const {
    auto it = kv.find(key);
    return it == kv.end() ? def : it->second;
  }
  bool has(const std::string& key) const { return kv.find(key) != kv.end(); }
  void set(const std::string& key, const std::string& val) { kv[key] = val; }
};

/** Encode to a single line including trailing '\\n'. */
std::string encode(const Message& msg);

/**
 * Parse one complete line (no newline). Returns false if invalid.
 * Unknown keys are kept; unknown types still parse.
 */
bool decode(const std::string& line, Message& out);

/** Build helpers */
Message makeHello(const std::string& name);
Message makeWelcome(const std::string& session, const std::string& youSide,
                    const std::string& hostSide);
Message makeStart(const std::string& fen, const std::string& turn, int ply);
Message makeMoveReq(const std::string& uci, int ply);
Message makeMove(const std::string& uci, int ply, const std::string& by);
Message makeReject(const std::string& uci, const std::string& reason);
Message makeState(const std::string& fen, const std::string& turn, int ply,
                  const std::string& end, const std::string& winner);
Message makePing(long long tMs);
Message makePong(long long tMs);
Message makeGoodbye(const std::string& reason);
Message makeError(const std::string& code, const std::string& msg);

/** Incremental line buffer: feed bytes, pull complete lines. */
class LineBuffer {
public:
  void append(const char* data, int len);
  bool popLine(std::string& line);
  void clear() { buf_.clear(); }
  size_t size() const { return buf_.size(); }

private:
  std::string buf_;
};

} // namespace NetProtocol

#endif
