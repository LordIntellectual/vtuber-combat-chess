#version 130

varying float vLightIntensity;
varying vec3 vLightPosition;
varying vec2 vUV;
varying vec3 vWorldPos;
varying vec3 vWorldNormal;

uniform vec4 color;
uniform float emissiveBoost;
uniform float time;

uniform int shadowMapResolution;
uniform sampler2D shadowMap;
uniform sampler2D diffuseMap;
uniform int useDiffuseMap;

// Dynamic neon particle lights (space theme move trails)
uniform int numPointLights;
uniform vec3 pointLightPos[8];
uniform vec3 pointLightColor[8];
uniform float pointLightIntensity[8];

float getLightFactor(){
  float factor = 0.5;
  if(vLightIntensity > 0.8) factor = 1.0;
  else if (vLightIntensity > 0.3) factor = 0.9;
  else if (vLightIntensity > 0.0) factor = 0.7;
  return factor;
}

void main(void){
  float sum = 0.;
  vec2 dxy;
  for(float x = -1.5; x <= 1.5; x += 1.) {
    for(float y = -1.5; y <= 1.5; y += 1.) {
      dxy = vec2(x/float(shadowMapResolution), y/float(shadowMapResolution));
      sum += texture2D(shadowMap, vLightPosition.xy + dxy).r;
    }
  }
  sum /= 16.;
  float shadowCoeff = smoothstep(0.001, 0.04, vLightPosition.z - sum);

  float factor = getLightFactor();
  if(shadowCoeff > 0.5){
    factor = 0.5;
  }

  float pulse = 0.85 + 0.15 * sin(time * 2.5);
  float e = clamp(emissiveBoost, 0.0, 1.5) * pulse;

  vec3 albedo = color.rgb;
  if (useDiffuseMap != 0) {
    vec3 tex = texture2D(diffuseMap, vUV).rgb;
    albedo = mix(tex, tex * color.rgb, 0.22);
  }

  vec3 base = albedo * factor;
  vec3 lit = base + albedo * e * 0.55;

  // Neon point-light wash from nearby move-trail particles
  vec3 n = normalize(vWorldNormal);
  vec3 glow = vec3(0.0);
  int nL = numPointLights;
  if (nL > 8) nL = 8;
  for (int i = 0; i < 8; i++) {
    if (i >= nL) break;
    vec3 toL = pointLightPos[i] - vWorldPos;
    float dist = length(toL);
    float att = pointLightIntensity[i] / (1.0 + dist * dist * 0.55);
    float ndl = max(dot(n, toL / max(dist, 0.001)), 0.0);
    // Soft wrap so undersides pick up a little glow
    float wrap = ndl * 0.75 + 0.25;
    glow += pointLightColor[i] * att * wrap;
  }
  lit += glow * albedo;

  lit = min(lit, vec3(1.85));
  gl_FragColor = vec4(lit, color.a);
}
