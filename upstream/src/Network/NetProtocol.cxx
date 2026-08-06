#include "NetProtocol.hxx"

#include <sstream>
#include <cctype>

namespace NetProtocol {

std::string encode(const Message& msg) {
  std::ostringstream os;
  os << MAGIC << ' ' << msg.type;
  for (std::map<std::string, std::string>::const_iterator it = msg.kv.begin();
       it != msg.kv.end(); ++it) {
    os << ' ' << it->first << '=' << it->second;
  }
  os << '\n';
  return os.str();
}

bool decode(const std::string& lineIn, Message& out) {
  out.type.clear();
  out.kv.clear();
  std::string line = lineIn;
  // strip CR
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
    line.pop_back();
  if (line.empty() || line.size() > MAX_LINE) return false;

  std::vector<std::string> tokens;
  std::string cur;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        tokens.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) tokens.push_back(cur);
  if (tokens.size() < 2) return false;
  if (tokens[0] != MAGIC) return false;

  out.type = tokens[1];
  for (size_t i = 2; i < tokens.size(); ++i) {
    const std::string& t = tokens[i];
    std::string::size_type eq = t.find('=');
    if (eq == std::string::npos || eq == 0) continue;
    std::string k = t.substr(0, eq);
    std::string v = t.substr(eq + 1);
    out.kv[k] = v;
  }
  return true;
}

Message makeHello(const std::string& name) {
  Message m;
  m.type = "HELLO";
  m.set("proto", "1");
  m.set("name", name.empty() ? "Player" : name);
  return m;
}

Message makeWelcome(const std::string& session, const std::string& youSide,
                    const std::string& hostSide) {
  Message m;
  m.type = "WELCOME";
  m.set("proto", "1");
  m.set("session", session);
  m.set("you", youSide);
  m.set("host", hostSide);
  return m;
}

Message makeStart(const std::string& fen, const std::string& turn, int ply) {
  Message m;
  m.type = "START";
  m.set("fen", fen);
  m.set("turn", turn);
  m.set("ply", std::to_string(ply));
  return m;
}

Message makeMoveReq(const std::string& uci, int ply) {
  Message m;
  m.type = "MOVE_REQ";
  m.set("uci", uci);
  m.set("ply", std::to_string(ply));
  return m;
}

Message makeMove(const std::string& uci, int ply, const std::string& by) {
  Message m;
  m.type = "MOVE";
  m.set("uci", uci);
  m.set("ply", std::to_string(ply));
  m.set("by", by);
  return m;
}

Message makeReject(const std::string& uci, const std::string& reason) {
  Message m;
  m.type = "REJECT";
  m.set("uci", uci);
  m.set("reason", reason);
  return m;
}

Message makeState(const std::string& fen, const std::string& turn, int ply,
                  const std::string& end, const std::string& winner) {
  Message m;
  m.type = "STATE";
  m.set("fen", fen);
  m.set("turn", turn);
  m.set("ply", std::to_string(ply));
  m.set("end", end);
  m.set("winner", winner);
  return m;
}

Message makePing(long long tMs) {
  Message m;
  m.type = "PING";
  m.set("t", std::to_string(tMs));
  return m;
}

Message makePong(long long tMs) {
  Message m;
  m.type = "PONG";
  m.set("t", std::to_string(tMs));
  return m;
}

Message makeGoodbye(const std::string& reason) {
  Message m;
  m.type = "GOODBYE";
  m.set("reason", reason);
  return m;
}

Message makeError(const std::string& code, const std::string& msg) {
  Message m;
  m.type = "ERROR";
  m.set("code", code);
  m.set("msg", msg);
  return m;
}

void LineBuffer::append(const char* data, int len) {
  if (len <= 0) return;
  buf_.append(data, data + len);
  if (buf_.size() > MAX_LINE * 4)
    buf_.erase(0, buf_.size() - MAX_LINE * 2);
}

bool LineBuffer::popLine(std::string& line) {
  std::string::size_type nl = buf_.find('\n');
  if (nl == std::string::npos) {
    if (buf_.size() > MAX_LINE) buf_.clear(); // desync — drop
    return false;
  }
  line = buf_.substr(0, nl);
  buf_.erase(0, nl + 1);
  return true;
}

} // namespace NetProtocol
