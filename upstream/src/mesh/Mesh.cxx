#define GL_GLEXT_PROTOTYPES

#include <GLFW/glfw3.h>

#include <vector>

#include "Mesh.hxx"


Mesh::Mesh(){}

void Mesh::initBuffers(){
  // Vertex buffer
  glGenBuffers(1, &vertexBufferId);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
  glBufferData(
    GL_ARRAY_BUFFER,
    vertices.size()*sizeof(GLfloat),
    vertices.data(),
    GL_STATIC_DRAW);

  // Normal buffer
  glGenBuffers(1, &normalBufferId);
  glBindBuffer(GL_ARRAY_BUFFER, normalBufferId);
  glBufferData(
    GL_ARRAY_BUFFER,
    normals.size()*sizeof(GLfloat),
    normals.data(),
    GL_STATIC_DRAW);

  if (!texcoords.empty()) {
    glGenBuffers(1, &texcoordBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBufferId);
    glBufferData(
      GL_ARRAY_BUFFER,
      texcoords.size()*sizeof(GLfloat),
      texcoords.data(),
      GL_STATIC_DRAW);
  }

  // Index buffer
  glGenBuffers(1, &indexBufferId);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId);
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER,
    indices.size()*sizeof(GLuint),
    indices.data(),
    GL_STATIC_DRAW);

  // Unbind buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  return;
}

void Mesh::draw(){
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);

  // Send vertices
  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
  glVertexPointer(
    3,
    GL_FLOAT,
    0,
    (void*)0
  );

  // Send normals
  glBindBuffer(GL_ARRAY_BUFFER, normalBufferId);
  glNormalPointer(
    GL_FLOAT,
    0,
    (void*)0
  );

  if (texcoordBufferId != 0 && !texcoords.empty()) {
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBufferId);
    glTexCoordPointer(2, GL_FLOAT, 0, (void*)0);
  }

  // Draw triangles
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId);
  glDrawElements(
    GL_TRIANGLES,
    indices.size(),
    GL_UNSIGNED_INT,
    (void*)0
  );

  // Unbind buffers
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  if (texcoordBufferId != 0) {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
}

Mesh::~Mesh(){
  if (vertexBufferId) glDeleteBuffers(1, &vertexBufferId);
  if (normalBufferId) glDeleteBuffers(1, &normalBufferId);
  if (texcoordBufferId) glDeleteBuffers(1, &texcoordBufferId);
  if (indexBufferId) glDeleteBuffers(1, &indexBufferId);
  if (diffuseTextureId) glDeleteTextures(1, &diffuseTextureId);
}
