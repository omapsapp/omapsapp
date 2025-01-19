// Warning! Beware to use this shader. "discard" command may significally reduce performance.
// Unfortunately some CG algorithms cannot be implemented on OpenGL ES 2.0 without discarding
// fragments from depth buffer.

uniform sampler2D u_colorTex;
uniform float u_opacity;
uniform vec4 u_maskColor;

varying vec2 v_colorTexCoords;

void main()
{
  vec4 finalColor = texture2D(u_colorTex, v_colorTexCoords);
  if (finalColor.r == 1.0 && finalColor.g == 1.0 && finalColor.b == 1.0)
    finalColor=vec4(vec3(u_maskColor.rgb * finalColor.rgb), finalColor.a);
  else
    finalColor=vec4(1.0,1.0,1.0,finalColor.a);
  if (finalColor.a < 0.01)
    discard;
  finalColor = finalColor * u_opacity;
  gl_FragColor = finalColor;
}
