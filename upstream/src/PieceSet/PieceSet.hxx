#ifndef NCA_PIECE_SET_HXX_
#define NCA_PIECE_SET_HXX_

#include <map>
#include <string>
#include <vector>

#include "../mesh/Mesh.hxx"

struct PieceSetInfo {
  std::string id;
  std::string name;
  std::string description;
  std::string path; // absolute/resolved directory of the set
};

/* Discovers and loads chess piece mesh sets from share/nca/piece_sets/. */
class PieceSetManager {
public:
  PieceSetManager();

  /* Scan piece_sets root (usually .../share/nca/piece_sets). */
  void scan(const std::string& pieceSetsRoot);

  const std::vector<PieceSetInfo>& sets() const { return setList; }
  int activeIndex() const { return active; }
  const PieceSetInfo& current() const;

  /* Cycle to next set; returns new info. */
  const PieceSetInfo& cycle();

  /* Select by id; returns false if unknown. */
  bool select(const std::string& id);

  /* Load standing piece meshes for the active set (with fallbacks). */
  std::map<int, Mesh*> loadPieces() const;

  /* Load capture fragment meshes for the active set (with fallbacks). */
  std::map<int, std::vector<Mesh*>> loadFragments() const;

  static void freePieces(std::map<int, Mesh*>* pieces);
  static void freeFragments(std::map<int, std::vector<Mesh*>>* fragments);

private:
  std::vector<PieceSetInfo> setList;
  int active;
  std::string root;

  std::string resolveFile(const std::string& setPath, const std::string& fileName) const;
  Mesh* loadOneObj(const std::string& path) const;
  std::vector<Mesh*> loadFragmentObj(const std::string& path) const;
};

#endif
