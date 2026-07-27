#version 130
// Camera-facing billboard particles (same pattern as smoke — reliable on GL 3.0 compat)

attribute vec3 vertexPosition;   // quad corner in XZ: (-0.5,0,±0.5)
attribute vec4 centerSize;       // xyz = world center, w = world size
attribute vec4 colorAlpha;       // rgb + alpha

varying vec4 vColor;
varying vec2 vUV;

uniform mat4 VMatrix;
uniform mat4 PMatrix;

void main() {
  vec3 particleCenter = centerSize.xyz;
  float particleSize = centerSize.w;

  // Camera axes from view matrix (matches smokeVS)
  vec3 camera_right = vec3(VMatrix[0][0], VMatrix[1][0], VMatrix[2][0]);
  vec3 camera_up    = vec3(VMatrix[0][1], VMatrix[1][1], VMatrix[2][1]);

  vec3 position = particleCenter
    + camera_right * vertexPosition.x * particleSize
    + camera_up    * vertexPosition.z * particleSize;

  vUV = vertexPosition.xz + vec2(0.5, 0.5);
  vColor = colorAlpha;

  gl_Position = PMatrix * VMatrix * vec4(position, 1.0);
}
