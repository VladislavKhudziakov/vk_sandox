#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

layout(push_constant) uniform instance_data
{
    mat4 model_view_proj;
}
u_instance_data;

layout(location = 0) out vec3 v_normal;

void main()
{
    gl_Position = u_instance_data.model_view_proj * vec4(a_position, 1.0);
    gl_Position.y = 1. - gl_Position.y;
    v_normal = normalize(a_normal);
}
