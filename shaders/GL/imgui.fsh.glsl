varying vec2 v_texCoords;
varying vec4 v_color;

uniform sampler2D u_colorTex;

void main()
{
  LOW_P vec4 color = texture2D(u_colorTex, v_texCoords);
  gl_FragColor = color * v_color;
}
