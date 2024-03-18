#version 450

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 texcoord;
layout (location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D tex;

void main(){
    FragColor = color.bgra * texture(tex, texcoord);
}