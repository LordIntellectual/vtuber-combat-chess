#version 130
uniform sampler2D uTex;
varying vec2 vUV;
varying vec4 vColor;

void main(void) {
  vec4 t = texture2D(uTex, vUV);
  gl_FragColor = t * vColor;
}
