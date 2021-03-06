#version 330 core

uniform sampler2D srctex;
uniform vec4 res;

out vec4 frag;

void main() {
  vec2 uv = gl_FragCoord.xy * res.zw;
  vec2 st = vec2(0., res.w*3.);

  frag = texture2D(srctex, uv - 12.*st) * 0.15
       + texture2D(srctex, uv - 9.*st) * 0.25
       + texture2D(srctex, uv - st)    * 0.35
       + texture2D(srctex, uv)         * 0.50
       + texture2D(srctex, uv + st)    * 0.35
       + texture2D(srctex, uv + 9.*st) * 0.25
       + texture2D(srctex, uv + 12.*st) * 0.15
       ;
}
