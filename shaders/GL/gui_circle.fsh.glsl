in vec2 v_position;
in vec3 v_color;
in vec3 v_outlineColor;
in float v_outlineWidthRatio;

void main()
{
  float R = 1.0;
  float R2 = R - v_outlineWidthRatio;
  float dist = sqrt(dot(v_position, v_position));
  if (dist >= R)
  {
    gl_FragColor.w = 0.0;
  }
  else if (dist <= R2)
  {
    gl_FragColor = vec4(v_color, 255.0);
  }
  else
  {
    float sm = smoothstep(R, R-0.01, dist);
    float sm2 = smoothstep(R2, R2+0.01, dist);
    float alpha = sm*sm2;
    gl_FragColor = vec4(v_outlineColor, alpha);
  }
}
