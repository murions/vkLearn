#version 450

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aCol;
layout (location = 0) out vec4 color;

void main(){
    gl_Position = vec4(aPos, 0, 1);

    color = vec4(aCol, 1);
}