#version 330 core

layout(location=0)in vec2 position;
layout(location=1)in vec2 Texcoord;
layout(location=2)in vec4 col;
out vec2 texc;
out vec4 color;

uniform mat4 ProjectM;
void main()
{
     texc=vec2(Texcoord.x,Texcoord.y) ;
     color=col;
    gl_Position =ProjectM*vec4(position,0.0f,1.0f);
   
}
