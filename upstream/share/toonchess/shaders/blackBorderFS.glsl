#version 130

// Per-side outline colour (set per draw: white/red, black/blue, board black).
uniform vec4 outlineColor;

void main(void){
  gl_FragColor = outlineColor;
}
