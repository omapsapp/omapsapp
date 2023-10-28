in vec2 a_position;
in vec3 a_color;
in vec3 a_outlineColor;
in float a_radius;
in float a_outlineWidthRatio;

uniform mat4 u_modelView;
uniform mat4 u_projection;

out vec2 v_position;
out vec3 v_color;
out vec3 v_outlineColor;
out float v_outlineWidthRatio;

void main()
{
  v_position = a_position;
  v_color = a_color;
  v_outlineColor = a_outlineColor;
  v_outlineWidthRatio = a_outlineWidthRatio;

  gl_Position = vec4(a_position * a_radius, 0, 1) * u_modelView * u_projection;
#ifdef VULKAN
  gl_Position.y = -gl_Position.y;
  gl_Position.z = (gl_Position.z  + gl_Position.w) * 0.5;
#endif
}
