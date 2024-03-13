#version 450

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aCol;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform UBO{
    mat4 model;
    mat4 view;
    mat4 proj;
}ubo;

void main(){
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(aPos, 0, 1);

    color = vec4(aCol, 1);
}