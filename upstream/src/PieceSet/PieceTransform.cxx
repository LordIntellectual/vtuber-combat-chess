#include "PieceTransform.hxx"
#include "../constants.hxx"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static std::string jsonStringField(const std::string& text, const std::string& key) {
  std::string pat = "\"" + key + "\"";
  auto k = text.find(pat);
  if (k == std::string::npos) return "";
  auto colon = text.find(':', k + pat.size());
  if (colon == std::string::npos) return "";
  auto q1 = text.find('"', colon + 1);
  if (q1 == std::string::npos) return "";
  auto q2 = text.find('"', q1 + 1);
  if (q2 == std::string::npos) return "";
  return text.substr(q1 + 1, q2 - q1 - 1);
}

static float jsonNumberAfter(const std::string& text, size_t from, const char* key) {
  std::string pat = std::string("\"") + key + "\"";
  auto k = text.find(pat, from);
  if (k == std::string::npos) return 0.f;
  auto colon = text.find(':', k + pat.size());
  if (colon == std::string::npos) return 0.f;
  size_t i = colon + 1;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) i++;
  try {
    return std::stof(text.substr(i));
  } catch (...) {
    return 0.f;
  }
}

void PieceTransformStore::clear() { data.clear(); }

const std::vector<std::string>& PieceTransformStore::allPieceKeys() {
  static const std::vector<std::string> keys = {
    "king", "queen", "bishop", "knight", "rook", "pawn"
  };
  return keys;
}

const char* PieceTransformStore::pieceKeyFromType(int pieceTypeAbs) {
  switch (std::abs(pieceTypeAbs)) {
    case KING: return "king";
    case QUEEN: return "queen";
    case BISHOP: return "bishop";
    case KNIGHT: return "knight";
    case ROOK: return "rook";
    case PAWN: return "pawn";
    default: return "pawn";
  }
}

int PieceTransformStore::pieceTypeFromKey(const std::string& key) {
  if (key == "king") return KING;
  if (key == "queen") return QUEEN;
  if (key == "bishop") return BISHOP;
  if (key == "knight") return KNIGHT;
  if (key == "rook") return ROOK;
  if (key == "pawn") return PAWN;
  return PAWN;
}

/** Built-in defaults when transforms.json is missing or omits a piece. */
static void applyBuiltinTransformDefaults(
    const std::string& setId, std::map<std::string, PieceTransform>& map) {
  // vtuber_set_1 pawn mesh is authored large; default display scale 0.75
  // (does not change the .obj — only the transform applied at render time).
  if (setId == "vtuber_set_1") {
    auto it = map.find("pawn");
    if (it == map.end()) {
      PieceTransform t;
      t.scale = 0.75f;
      map["pawn"] = t;
    }
  }
}

void PieceTransformStore::loadForSet(const std::string& setId, const std::string& setPath) {
  std::string path = setPath;
  if (!path.empty() && path.back() != '/') path.push_back('/');
  path += "transforms.json";

  std::map<std::string, PieceTransform> map;
  std::ifstream in(path);
  if (!in) {
    applyBuiltinTransformDefaults(setId, map);
    data[setId] = map;
    if (!map.empty())
      std::cout << "[Transforms] No file at " << path
                << " — applied built-in defaults for " << setId << "\n";
    return;
  }
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  for (const auto& key : allPieceKeys()) {
    std::string needle = "\"" + key + "\"";
    auto pos = text.find(needle);
    if (pos == std::string::npos) continue;
    auto brace = text.find('{', pos);
    if (brace == std::string::npos) continue;
    auto end = text.find('}', brace);
    if (end == std::string::npos) continue;
    std::string block = text.substr(brace, end - brace + 1);
    PieceTransform t;
    t.px = jsonNumberAfter(block, 0, "px");
    t.py = jsonNumberAfter(block, 0, "py");
    t.pz = jsonNumberAfter(block, 0, "pz");
    t.rx = jsonNumberAfter(block, 0, "rx");
    t.ry = jsonNumberAfter(block, 0, "ry");
    t.rz = jsonNumberAfter(block, 0, "rz");
    float sc = jsonNumberAfter(block, 0, "scale");
    t.scale = (sc > 1e-4f) ? sc : 1.f;
    map[key] = t;
  }
  applyBuiltinTransformDefaults(setId, map);
  data[setId] = map;
  std::cout << "[Transforms] Loaded " << map.size() << " entries from " << path << "\n";
}

bool PieceTransformStore::saveForSet(const std::string& setId, const std::string& setPath) const {
  std::string path = setPath;
  if (!path.empty() && path.back() != '/') path.push_back('/');
  path += "transforms.json";

  auto it = data.find(setId);
  const std::map<std::string, PieceTransform>* map =
    (it != data.end()) ? &it->second : nullptr;

  std::ofstream out(path);
  if (!out) {
    std::cerr << "[Transforms] Failed to write " << path << "\n";
    return false;
  }
  out << "{\n";
  bool first = true;
  for (const auto& key : allPieceKeys()) {
    PieceTransform t;
    if (map) {
      auto jt = map->find(key);
      if (jt != map->end()) t = jt->second;
    }
    if (!first) out << ",\n";
    first = false;
    out << "  \"" << key << "\": {"
        << "\"px\": " << t.px << ", \"py\": " << t.py << ", \"pz\": " << t.pz
        << ", \"rx\": " << t.rx << ", \"ry\": " << t.ry << ", \"rz\": " << t.rz
        << ", \"scale\": " << t.scale << "}";
  }
  out << "\n}\n";
  std::cout << "[Transforms] Saved " << path << "\n";
  return true;
}

PieceTransform PieceTransformStore::get(const std::string& setId, const std::string& pieceKey) const {
  auto it = data.find(setId);
  if (it == data.end()) return PieceTransform{};
  auto jt = it->second.find(pieceKey);
  if (jt == it->second.end()) return PieceTransform{};
  return jt->second;
}

void PieceTransformStore::set(const std::string& setId, const std::string& pieceKey,
                              const PieceTransform& t) {
  data[setId][pieceKey] = t;
}

std::vector<GLfloat> PieceTransformStore::buildPieceMatrix(
    int pieceSigned,
    float boardX, float boardY, float boardZ,
    float basePieceScale,
    const PieceTransform& t) {
  Vector3f zAxis = {0, 0, 1};
  Vector3f xAxis = {1, 0, 0};
  Vector3f yAxis = {0, 1, 0};

  std::vector<GLfloat> m = getIdentityMatrix();

  // Local mesh adjust (scale → rotate X/Y/Z → offset)
  float s = basePieceScale * (t.scale > 1e-4f ? t.scale : 1.f);
  m = scale(&m, s);
  if (std::fabs(t.rx) > 1e-5f) m = rotate(&m, t.rx, xAxis);
  if (std::fabs(t.ry) > 1e-5f) m = rotate(&m, t.ry, yAxis);
  if (std::fabs(t.rz) > 1e-5f) m = rotate(&m, t.rz, zAxis);
  if (std::fabs(t.px) > 1e-5f || std::fabs(t.py) > 1e-5f || std::fabs(t.pz) > 1e-5f)
    m = translate(&m, {t.px, t.py, t.pz});

  // Team facing (same as original game)
  m = rotate(&m, pieceSigned > 0 ? -90.f : 90.f, zAxis);

  // Board cell position
  m = translate(&m, {boardX, boardY, boardZ});
  return m;
}
