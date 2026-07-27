#include <map>
#include <vector>
#include <string>
#include <fstream>

#include "loadObjFile.hxx"
#include "Mesh.hxx"
#include "../constants.hxx"
#include "../get_share_path.hxx"

#include "meshes.hxx"

std::map<int, Mesh*> initPieces(){
  std::string share_path = get_share_path();
  // Prefer starship fleet meshes when present (vTuber Combat Chess)
  std::string ship = share_path + "assets/starship/";
  auto load1 = [&](const std::string& rel) -> Mesh* {
    std::string p = ship + rel;
    std::ifstream test(p);
    if (!test.good()) p = share_path + "assets/" + rel;
    return loadObjFile(p).at(0);
  };

  Mesh* king = load1("king.obj");
  Mesh* queen = load1("queen.obj");
  Mesh* bishop = load1("bishop.obj");
  Mesh* rook = load1("rook.obj");
  Mesh* knight = load1("knight.obj");
  Mesh* pawn = load1("pawn.obj");
  Mesh* boardCell = load1("boardCell.obj");

  std::map<int, Mesh*> meshes = {
    {KING, king},
    {QUEEN, queen},
    {BISHOP, bishop},
    {ROOK, rook},
    {KNIGHT, knight},
    {PAWN, pawn},
    {BOARDCELL, boardCell},
  };

  return meshes;
};

void deletePieces(std::map<int, Mesh*>* meshes){
  delete meshes->at(KING);
  delete meshes->at(QUEEN);
  delete meshes->at(BISHOP);
  delete meshes->at(ROOK);
  delete meshes->at(KNIGHT);
  delete meshes->at(PAWN);
  delete meshes->at(BOARDCELL);

  meshes->clear();
};

std::map<int, std::vector<Mesh*>> initFragmentMeshes(){
  std::string share_path = get_share_path();
  std::string ship = share_path + "assets/starship/";
  auto loadF = [&](const std::string& rel) -> std::vector<Mesh*> {
    std::string p = ship + rel;
    std::ifstream test(p);
    if (!test.good()) p = share_path + "assets/" + rel;
    return loadObjFile(p);
  };

  std::vector<Mesh*> king_fragments = loadF("king_fragments.cobj");
  std::vector<Mesh*> queen_fragments = loadF("queen_fragments.cobj");
  std::vector<Mesh*> bishop_fragments = loadF("bishop_fragments.cobj");
  std::vector<Mesh*> rook_fragments = loadF("rook_fragments.cobj");
  std::vector<Mesh*> knight_fragments = loadF("knight_fragments.cobj");
  std::vector<Mesh*> pawn_fragments = loadF("pawn_fragments.cobj");

  std::map<int, std::vector<Mesh*>> meshes = {
    {KING, king_fragments},
    {QUEEN, queen_fragments},
    {BISHOP, bishop_fragments},
    {ROOK, rook_fragments},
    {KNIGHT, knight_fragments},
    {PAWN, pawn_fragments},
  };

  return meshes;
};

void _deleteFragmentMeshes(std::vector<Mesh*>* fragments){
  for(unsigned int i = 0; i < fragments->size(); i++)
    delete fragments->at(i);
}

void deleteFragmentMeshes(std::map<int, std::vector<Mesh*>>* fragments){
  _deleteFragmentMeshes(&fragments->at(KING));
  _deleteFragmentMeshes(&fragments->at(QUEEN));
  _deleteFragmentMeshes(&fragments->at(BISHOP));
  _deleteFragmentMeshes(&fragments->at(ROOK));
  _deleteFragmentMeshes(&fragments->at(KNIGHT));
  _deleteFragmentMeshes(&fragments->at(PAWN));

  fragments->clear();
};
