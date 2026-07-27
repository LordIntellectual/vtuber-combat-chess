#ifndef MESH_HXX_
#define MESH_HXX_

#include "../gl_compat.hxx"

#include <vector>

#include "../utils/math.hxx"

class Mesh {
  public:
    /* Vector of vertices defining the mesh */
    std::vector<GLfloat> vertices;
    /* Vector of normals defining the mesh */
    std::vector<GLfloat> normals;
    /* Optional UVs (u,v pairs), same expansion count as vertices when present */
    std::vector<GLfloat> texcoords;
    /* Vector of indices defining the faces of the mesh */
    std::vector<GLuint> indices;

    /* ID of the vertex buffer */
    GLuint vertexBufferId = 0;
    /* ID of the normal buffer */
    GLuint normalBufferId = 0;
    /* ID of the texcoord buffer (0 if unused) */
    GLuint texcoordBufferId = 0;
    /* ID of the indices buffer */
    GLuint indexBufferId = 0;

    /* Optional diffuse texture (0 = solid cel colour only) */
    GLuint diffuseTextureId = 0;

    /* Mass of the mesh */
    GLfloat mass = 1;

    /* Origin of the mesh */
    Vector3f origin = {0.0, 0.0, 0.0};

    /* Constructor */
    explicit Mesh();

    /* Initialization of the buffer objects, must be called after filling
      vectors of vertices, normals and indices */
    void initBuffers();

    bool hasDiffuseTexture() const { return diffuseTextureId != 0; }
    bool hasTexcoords() const { return !texcoords.empty(); }

    /* Draw the mesh in the 3D scene */
    void draw();

    /* Destructor, this will remove the buffers from memory */
    ~Mesh();
};

#endif
