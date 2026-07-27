#ifndef NCA_PIECE_TRANSFORM_HXX_
#define NCA_PIECE_TRANSFORM_HXX_

#include <map>
#include <string>
#include <vector>
#include "../utils/math.hxx"

/* Per-piece placement tweak (model space, applied before team yaw / board pos). */
struct PieceTransform {
  float px = 0.f, py = 0.f, pz = 0.f; // position offset
  float rx = 0.f, ry = 0.f, rz = 0.f; // rotation degrees about X/Y/Z
  float scale = 1.f;                  // 1 = default mesh size
};

/* Loads/saves transforms.json next to each piece set (persists across sessions). */
class PieceTransformStore {
public:
  void clear();
  void loadForSet(const std::string& setId, const std::string& setPath);
  bool saveForSet(const std::string& setId, const std::string& setPath) const;

  PieceTransform get(const std::string& setId, const std::string& pieceKey) const;
  void set(const std::string& setId, const std::string& pieceKey, const PieceTransform& t);

  static const char* pieceKeyFromType(int pieceTypeAbs);
  static int pieceTypeFromKey(const std::string& key);
  static const std::vector<std::string>& allPieceKeys();

  /* Build model matrix: team yaw, then local transform, then board translation.
     Matches existing ToonChess multiply order (matrixProduct left*right). */
  static std::vector<GLfloat> buildPieceMatrix(
    int pieceSigned,
    float boardX, float boardY, float boardZ,
    float basePieceScale,
    const PieceTransform& t);

private:
  std::map<std::string, std::map<std::string, PieceTransform>> data;
};

#endif
