#version 330 core

layout(location=0) in vec3 inVert;
layout(location=1) in vec4 inColourSize;

uniform mat4 projection;
out vec3 colour;
void main()
{
  gl_Position = projection*vec4(inVert,1);
  colour.rgb=inColourSize.rgb;
  gl_PointSize=inColourSize.a;
}
