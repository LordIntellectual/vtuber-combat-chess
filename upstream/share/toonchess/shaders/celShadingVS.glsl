#version 130

varying float vLightIntensity;
varying vec3 vLightPosition;
varying vec2 vUV;
varying vec3 vWorldPos;
varying vec3 vWorldNormal;

uniform vec3 lightDirection;

// View/Movement/Projection matrices
uniform mat4 VMatrix;
uniform mat4 MMatrix;
uniform mat4 PMatrix;

// Normal matrix
uniform mat4 NMatrix;

// Light matrix and projection light matrix
uniform mat4 LMatrix;
uniform mat4 PLMatrix;

void main(void){
  vec3 normal = normalize((NMatrix * vec4(gl_Normal, 0.0)).xyz);
  vec3 lightDir = normalize(lightDirection);

  // Two-sided-ish fill so thin/hard-surface ships stay readable on stream
  float ndl = abs(dot(lightDir, normal));
  vLightIntensity = ndl;

  vUV = gl_MultiTexCoord0.xy;
  vWorldPos = (MMatrix * gl_Vertex).xyz;
  vWorldNormal = normal;

  // Compute position in the light coordinates system
  vec4 lightPosition = PLMatrix * LMatrix * MMatrix * gl_Vertex;
  vLightPosition = vec3(0.5, 0.5, 0.5) +
    lightPosition.xyz/lightPosition.w * 0.5;

  gl_Position = PMatrix * VMatrix * MMatrix * gl_Vertex;
}
