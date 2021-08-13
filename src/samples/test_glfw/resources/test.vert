#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec3 v_color;

const vec2 vertices[] = vec2[](
    vec2(0.0, -0.5),
    vec2(-0.5, 0.5),
    vec2(0.5, 0.5)
);

const vec3 colors[] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main()
{
    gl_Position = vec4(vertices[gl_VertexIndex], 0., 1.);
    v_color = colors[gl_VertexIndex];
}