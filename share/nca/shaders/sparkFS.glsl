#version 130
// Soft glowing disc (neon trails + hard sparks)

varying vec4 vColor;
varying vec2 vUV;

void main() {
  vec2 c = vUV - vec2(0.5);
  float d = length(c) * 2.0; // 0 at center, 1 at edge of unit circle
  if (d > 1.0) discard;

  // Soft falloff + hot core for neon energy look
  float soft = smoothstep(1.0, 0.0, d);
  float core = smoothstep(0.45, 0.0, d);
  float a = soft * vColor.a;
  if (a < 0.01) discard;

  vec3 rgb = vColor.rgb * (1.15 + core * 2.4);
  gl_FragColor = vec4(rgb, a);
}
