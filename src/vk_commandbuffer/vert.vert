#version 450

out gl_PerVertex{
    vec4 gl_Position;
};

layout (location = 0) out vec4 vertexColor;

vec2 position[3] = vec2[](
    vec2(0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 color[3] = vec3[](
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1)
);

void main(){
    gl_Position = vec4(position[gl_VertexIndex], 0, 1);

    vertexColor = vec4(color[gl_VertexIndex], 1);
}