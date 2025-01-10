attribute vec2 a_position;
attribute vec2 a_texCoords;
attribute vec4 a_color;

varying vec2 v_texCoords;
varying vec4 v_color;

uniform mat4 u_projection;

void main()
{
  v_texCoords = a_texCoords;
  v_color = a_color;
  gl_Position = vec4(a_position, 0, 1) * u_projection;
#ifdef VULKAN
  gl_Position.y = -gl_Position.y;
  gl_Position.z = (gl_Position.z  + gl_Position.w) * 0.5;
#endif
}
