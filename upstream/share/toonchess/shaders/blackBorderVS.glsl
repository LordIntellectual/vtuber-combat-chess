#version 130

uniform mat4 VMatrix;
uniform mat4 MMatrix;
uniform mat4 PMatrix;
// Outline thickness in model units (settings Video slider). Default ~0.08.
uniform float outlineFactor;

void main(void){
  // Extrude along surface normal so complex / off-centre meshes (characters)
  // get a stable silhouette. Fall back to radial expand if normal is unusable.
  float f = outlineFactor;
  if (f < 0.0) f = 0.0;
  if (f > 0.5) f = 0.5;

  vec3 n = normalize(gl_Normal);
  float nlen = length(gl_Normal);
  if (nlen < 1e-4) {
    // Radial fallback (original ToonChess behaviour)
    n = normalize(gl_Vertex.xyz);
  }

  vec4 deformedPosition = gl_Vertex + vec4(n * f, 0.0);
  gl_Position = PMatrix * VMatrix * MMatrix * deformedPosition;
}
