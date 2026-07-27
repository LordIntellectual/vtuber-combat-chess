#include <GLFW/glfw3.h>

#include "loadObjFile.hxx"

#include <string>
#include <vector>
#include <fstream>

#include "Mesh.hxx"
#include "../utils/utils.hxx"


void extractFloatVec3(
    std::vector<std::string> *line, std::vector<GLfloat> *vector){
  for(int i = 1; i <= 3; i++){
    vector->push_back(
      std::stof(line->at(i))
    );
  }
}

void extractFloatVec2(
    std::vector<std::string> *line, std::vector<GLfloat> *vector){
  for(int i = 1; i <= 2; i++){
    vector->push_back(
      std::stof(line->at(i))
    );
  }
}

static std::vector<std::string> faceCorner(const std::string& token) {
  // "v", "v/vt", "v//vn", "v/vt/vn"
  return split(token, '/');
}

void extractVertices(
    std::vector<std::string> *line,
    std::vector<GLfloat> *unsortedVertices,
    std::vector<GLfloat> *vertices){
  for(int i = 1; i <= 3; i++){
    int vertexIndex = std::stoi(faceCorner(line->at(i)).at(0)) - 1;

    for(int j = 0; j <= 2; j++){
      vertices->push_back(unsortedVertices->at(3 * vertexIndex + j));
    }
  }
}

void extractNormals(
    std::vector<std::string> *line,
    std::vector<GLfloat> *unsortedNormals,
    std::vector<GLfloat> *normals){
  for(int i = 1; i <= 3; i++){
    auto parts = faceCorner(line->at(i));
    // Prefer vn index (3rd field); if missing, reuse vertex index
    int normalIndex = 0;
    if (parts.size() >= 3 && !parts[2].empty())
      normalIndex = std::stoi(parts[2]) - 1;
    else
      normalIndex = std::stoi(parts[0]) - 1;

    for(int j = 0; j <= 2; j++){
      normals->push_back(unsortedNormals->at(3 * normalIndex + j));
    }
  }
}

void extractTexcoords(
    std::vector<std::string> *line,
    std::vector<GLfloat> *unsortedTexcoords,
    std::vector<GLfloat> *texcoords){
  if (unsortedTexcoords->empty()) return;
  for(int i = 1; i <= 3; i++){
    auto parts = faceCorner(line->at(i));
    // Need vt index in second field
    if (parts.size() < 2 || parts[1].empty()) {
      texcoords->push_back(0.f);
      texcoords->push_back(0.f);
      continue;
    }
    int ti = std::stoi(parts[1]) - 1;
    texcoords->push_back(unsortedTexcoords->at(2 * ti + 0));
    texcoords->push_back(unsortedTexcoords->at(2 * ti + 1));
  }
}

std::vector<Mesh *> loadObjFile(const std::string& filePath){
  // Read obj file
  std::ifstream fobj(filePath);
  std::string line;

  std::vector<GLfloat> unsortedVertices;
  std::vector<GLfloat> unsortedNormals;
  std::vector<GLfloat> unsortedTexcoords;

  std::vector<Mesh*> meshes;
  Mesh* currentMesh = nullptr;

  while(std::getline(fobj, line)){
    if(line.size() == 0) continue;

    std::vector<std::string> splittedLine = split(line, ' ');
    if (splittedLine.empty()) continue;

    // The line starts with an "o", it's a new mesh definition
    if(splittedLine.at(0).compare("o") == 0){
      currentMesh = new Mesh();
      meshes.push_back(currentMesh);

      continue;
    }

    // The line starts with an "m", it's the mesh's mass
    if(splittedLine.at(0).compare("mass") == 0){
      if (currentMesh) currentMesh->mass = std::stof(splittedLine.at(1));

      continue;
    }

    // The line starts with "origin", it's the mesh's origin
    if(splittedLine.at(0).compare("origin") == 0){
      if (currentMesh) {
        currentMesh->origin = Vector3f(
          std::stof(splittedLine.at(1)),
          std::stof(splittedLine.at(2)),
          std::stof(splittedLine.at(3))
        );
      }

      continue;
    }

    // The line starts with a "v", it's a new vertex
    if(splittedLine.at(0).compare("v") == 0){
      extractFloatVec3(&splittedLine, &unsortedVertices);
      continue;
    }

    // Texture coordinates
    if(splittedLine.at(0).compare("vt") == 0){
      extractFloatVec2(&splittedLine, &unsortedTexcoords);
      continue;
    }

    // The line starts with a "vn", it's a vertex normal
    if(splittedLine.at(0).compare("vn") == 0){
      extractFloatVec3(&splittedLine, &unsortedNormals);
      continue;
    }

    // The line starts with an "f", it's a triangle definition
    if(splittedLine.at(0).compare("f") == 0){
      if (!currentMesh) {
        // OBJ with no "o" line — create a default mesh
        currentMesh = new Mesh();
        meshes.push_back(currentMesh);
      }
      extractVertices(&splittedLine, &unsortedVertices, &currentMesh->vertices);
      extractNormals(&splittedLine, &unsortedNormals, &currentMesh->normals);
      extractTexcoords(&splittedLine, &unsortedTexcoords, &currentMesh->texcoords);

      for(int i = 0; i <= 2; i++){
        currentMesh->indices.push_back(currentMesh->indices.size());
      }
      continue;
    }
  }

  for(unsigned int i = 0; i < meshes.size(); i++) {
    // If faces lacked vt, drop empty/partial UV buffer
    Mesh* m = meshes.at(i);
    if (!m->texcoords.empty() &&
        m->texcoords.size() != (m->vertices.size() / 3) * 2) {
      m->texcoords.clear();
    }
    m->initBuffers();
  }

  return meshes;
};
