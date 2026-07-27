#version 130
/* Immediate-mode compatible: gl_Vertex / gl_MultiTexCoord0 / gl_Color
   + matrix stack (gl_ModelViewProjectionMatrix). Avoids fixed-function
   multitexture unit pollution from smoke/sparks. */
varying vec2 vUV;
varying vec4 vColor;

void main(void) {
  vUV = gl_MultiTexCoord0.xy;
  vColor = gl_Color;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
