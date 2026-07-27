#include "PieceSet.hxx"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <dirent.h>
#include <sys/stat.h>

#include "../mesh/loadObjFile.hxx"
#include "../constants.hxx"
#include "../get_share_path.hxx"
#include "../utils/utils.hxx"

namespace {

bool isDir(const std::string& p) {
  struct stat st {};
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool fileExists(const std::string& p) {
  std::ifstream f(p);
  return f.good();
}

// Minimal JSON string field: "key": "value"
std::string jsonStringField(const std::string& text, const std::string& key) {
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

std::string ncaShareRootFromToonchess() {
  std::string p = get_share_path(); // .../share/toonchess/
  auto pos = p.rfind("toonchess");
  if (pos != std::string::npos) p.replace(pos, 9, "nca");
  else p += "../nca/";
  if (!p.empty() && p.back() != '/') p.push_back('/');
  return p;
}

const char* pieceFile(int piece) {
  switch (piece) {
    case KING: return "king.obj";
    case QUEEN: return "queen.obj";
    case BISHOP: return "bishop.obj";
    case KNIGHT: return "knight.obj";
    case ROOK: return "rook.obj";
    case PAWN: return "pawn.obj";
    case BOARDCELL: return "boardCell.obj";
    default: return nullptr;
  }
}

const char* fragmentFile(int piece) {
  switch (piece) {
    case KING: return "king_fragments.cobj";
    case QUEEN: return "queen_fragments.cobj";
    case BISHOP: return "bishop_fragments.cobj";
    case KNIGHT: return "knight_fragments.cobj";
    case ROOK: return "rook_fragments.cobj";
    case PAWN: return "pawn_fragments.cobj";
    default: return nullptr;
  }
}

} // namespace

PieceSetManager::PieceSetManager() : active(0) {}

void PieceSetManager::scan(const std::string& pieceSetsRoot) {
  setList.clear();
  root = pieceSetsRoot;
  if (!root.empty() && root.back() != '/') root.push_back('/');

  DIR* dir = opendir(root.c_str());
  if (!dir) {
    // Fall back: default nca/piece_sets next to toonchess share
    root = ncaShareRootFromToonchess() + "piece_sets/";
    dir = opendir(root.c_str());
  }
  if (!dir) {
    std::cerr << "[PieceSet] No piece_sets directory at " << root << "\n";
    // Synthetic fallback using legacy starship/classic under toonchess
    PieceSetInfo legacy;
    legacy.id = "starship";
    legacy.name = "Starship Fleet";
    legacy.description = "Legacy path (assets/starship)";
    legacy.path = get_share_path() + "assets/starship/";
    setList.push_back(legacy);
    active = 0;
    return;
  }

  std::vector<std::string> names;
  while (dirent* ent = readdir(dir)) {
    if (ent->d_name[0] == '.') continue;
    std::string sub = root + ent->d_name;
    if (!isDir(sub)) continue;
    names.push_back(ent->d_name);
  }
  closedir(dir);
  std::sort(names.begin(), names.end());

  for (const auto& name : names) {
    std::string path = root + name + "/";
    PieceSetInfo info;
    info.id = name;
    info.name = name;
    info.description = "";
    info.path = path;
    std::string manifest = path + "set.json";
    if (fileExists(manifest)) {
      std::ifstream in(manifest);
      std::string text((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
      std::string id = jsonStringField(text, "id");
      std::string nm = jsonStringField(text, "name");
      std::string desc = jsonStringField(text, "description");
      if (!id.empty()) info.id = id;
      if (!nm.empty()) info.name = nm;
      if (!desc.empty()) info.description = desc;
    }
    // Must have at least one main piece mesh (or set.json alone is not enough)
    bool hasPiece = fileExists(path + "pawn.obj") || fileExists(path + "king.obj") ||
                    fileExists(path + "queen.obj") || fileExists(path + "rook.obj") ||
                    fileExists(path + "bishop.obj") || fileExists(path + "knight.obj");
    if (!hasPiece) {
      std::cerr << "[PieceSet] Skip incomplete set: " << path << "\n";
      continue;
    }
    setList.push_back(info);
    std::cout << "[PieceSet] Found " << info.id << " — " << info.name << "\n";
  }

  if (setList.empty()) {
    PieceSetInfo legacy;
    legacy.id = "starship";
    legacy.name = "Starship Fleet";
    legacy.description = "Legacy path";
    legacy.path = get_share_path() + "assets/starship/";
    setList.push_back(legacy);
  }

  // Prefer newest custom sets when present
  active = 0;
  auto prefer = [&](const char* id) {
    for (size_t i = 0; i < setList.size(); i++) {
      if (setList[i].id == id) { active = (int)i; return true; }
    }
    return false;
  };
  if (!prefer("vtuber_set_1"))
    if (!prefer("space_set_1"))
      prefer("starship");
  std::cout << "[PieceSet] Active: " << current().name << " (" << current().id << ")\n";
}

const PieceSetInfo& PieceSetManager::current() const {
  return setList.at((size_t)active);
}

const PieceSetInfo& PieceSetManager::cycle() {
  if (setList.empty()) return current();
  active = (active + 1) % (int)setList.size();
  std::cout << "[PieceSet] Switched to " << current().name << "\n";
  return current();
}

bool PieceSetManager::select(const std::string& id) {
  for (size_t i = 0; i < setList.size(); i++) {
    if (setList[i].id == id) {
      active = (int)i;
      return true;
    }
  }
  return false;
}

std::string PieceSetManager::resolveFile(const std::string& setPath,
                                         const std::string& fileName) const {
  std::string p = setPath + fileName;
  if (fileExists(p)) return p;

  // Fallbacks: starship set, then classic set, then legacy toonchess paths
  auto tryRoot = [&](const std::string& r) -> std::string {
    if (r.empty()) return "";
    std::string q = r;
    if (q.back() != '/') q.push_back('/');
    q += fileName;
    return fileExists(q) ? q : "";
  };

  for (const auto& s : setList) {
    if (s.id == "starship") {
      std::string q = tryRoot(s.path);
      if (!q.empty()) return q;
    }
  }
  for (const auto& s : setList) {
    if (s.id == "classic") {
      std::string q = tryRoot(s.path);
      if (!q.empty()) return q;
    }
  }

  std::string share = get_share_path();
  std::string ship = share + "assets/starship/" + fileName;
  if (fileExists(ship)) return ship;
  std::string base = share + "assets/" + fileName;
  if (fileExists(base)) return base;
  return "";
}

Mesh* PieceSetManager::loadOneObj(const std::string& path) const {
  if (path.empty()) throw std::runtime_error("empty mesh path");
  auto meshes = loadObjFile(path);
  if (meshes.empty()) throw std::runtime_error("no meshes in " + path);
  // loadObjFile may return multiple objects; use first for standing pieces
  Mesh* m = meshes.at(0);
  for (size_t i = 1; i < meshes.size(); i++) delete meshes[i];

  // Optional albedo: same basename as .obj with .png (e.g. pawn.obj → pawn.png)
  if (m->hasTexcoords()) {
    std::string png = path;
    auto dot = png.rfind('.');
    if (dot != std::string::npos) png = png.substr(0, dot) + ".png";
    else png += ".png";
    if (fileExists(png)) {
      try {
        m->diffuseTextureId = loadPNGTexture(png);
        glBindTexture(GL_TEXTURE_2D, m->diffuseTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        std::cout << "[PieceSet]   + diffuse " << png << "\n";
      } catch (const std::exception& e) {
        std::cerr << "[PieceSet] texture load failed " << png << ": "
                  << e.what() << "\n";
      }
    }
  }
  return m;
}

std::vector<Mesh*> PieceSetManager::loadFragmentObj(const std::string& path) const {
  if (path.empty()) return {};
  return loadObjFile(path);
}

std::map<int, Mesh*> PieceSetManager::loadPieces() const {
  const PieceSetInfo& info = current();
  std::map<int, Mesh*> meshes;
  const int kinds[] = {KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN, BOARDCELL};
  for (int k : kinds) {
    const char* fn = pieceFile(k);
    std::string path = resolveFile(info.path, fn);
    if (path.empty()) {
      throw std::runtime_error(std::string("Missing mesh ") + fn +
                               " for set " + info.id);
    }
    std::cout << "[PieceSet] " << fn << " ← " << path << "\n";
    meshes[k] = loadOneObj(path);
  }
  return meshes;
}

std::map<int, std::vector<Mesh*>> PieceSetManager::loadFragments() const {
  const PieceSetInfo& info = current();
  std::map<int, std::vector<Mesh*>> meshes;
  const int kinds[] = {KING, QUEEN, BISHOP, KNIGHT, ROOK, PAWN};
  for (int k : kinds) {
    const char* fn = fragmentFile(k);
    std::string path = resolveFile(info.path, fn);
    if (path.empty()) {
      std::cerr << "[PieceSet] No fragments for " << fn
                << " — capture VFX limited\n";
      meshes[k] = {};
      continue;
    }
    meshes[k] = loadFragmentObj(path);
    std::cout << "[PieceSet] fragments " << fn << " ← " << path
              << " (" << meshes[k].size() << " chunks)\n";

    // Share the standing piece albedo so shattered chunks stay recognisable
    if (!meshes[k].empty()) {
      bool anyUv = false;
      for (Mesh* m : meshes[k]) {
        if (m && m->hasTexcoords()) { anyUv = true; break; }
      }
      if (anyUv) {
        const char* pfn = pieceFile(k);
        std::string piecePath = resolveFile(info.path, pfn ? pfn : "");
        std::string png;
        if (!piecePath.empty()) {
          auto dot = piecePath.rfind('.');
          png = (dot != std::string::npos)
            ? piecePath.substr(0, dot) + ".png"
            : piecePath + ".png";
        }
        // Prefer set-local png even if fragment file was resolved from fallback
        std::string localPng = info.path + std::string(pfn ? pfn : "pawn.obj");
        {
          auto dot = localPng.rfind('.');
          if (dot != std::string::npos) localPng = localPng.substr(0, dot) + ".png";
          if (fileExists(localPng)) png = localPng;
        }
        if (!png.empty() && fileExists(png)) {
          try {
            GLuint tex = loadPNGTexture(png);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glBindTexture(GL_TEXTURE_2D, 0);
            for (Mesh* m : meshes[k]) {
              if (m) m->diffuseTextureId = tex; // shared id; freeFragments dedupes
            }
            std::cout << "[PieceSet]   + fragment diffuse " << png << "\n";
          } catch (const std::exception& e) {
            std::cerr << "[PieceSet] fragment texture failed " << png << ": "
                      << e.what() << "\n";
          }
        }
      }
    }
  }
  return meshes;
}

void PieceSetManager::freePieces(std::map<int, Mesh*>* pieces) {
  if (!pieces) return;
  for (auto& kv : *pieces) delete kv.second;
  pieces->clear();
}

void PieceSetManager::freeFragments(std::map<int, std::vector<Mesh*>>* fragments) {
  if (!fragments) return;
  // Chunks share one GL texture per piece type — delete each GL id once
  std::vector<GLuint> seenTex;
  auto already = [&](GLuint t) {
    for (GLuint s : seenTex) if (s == t) return true;
    return false;
  };
  for (auto& kv : *fragments) {
    for (Mesh* m : kv.second) {
      if (!m) continue;
      if (m->diffuseTextureId && already(m->diffuseTextureId)) {
        m->diffuseTextureId = 0; // Mesh dtor would double-free otherwise
      } else if (m->diffuseTextureId) {
        seenTex.push_back(m->diffuseTextureId);
      }
      delete m;
    }
    kv.second.clear();
  }
  fragments->clear();
}
