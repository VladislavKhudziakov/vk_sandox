#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoords0;

//layout(location = 3) in ivec4 a_bones;
//layout(location = 4) in vec4 a_weight;

layout(push_constant) uniform instance_data
{
    mat4 model_view_proj;
}
u_instance_data;
//
//layout (set = 1, binding = 0) uniform animation_data
//{
//   mat4 bones[256];
//} u_animation_data;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_texCoords;


void main()
{
    //    mat4 anim_matrix =
    //        u_animation_data.bones[a_bones.x] * a_weight.x +
    //        u_animation_data.bones[a_bones.y] * a_weight.y +
    //        u_animation_data.bones[a_bones.z] * a_weight.z +
    //        u_animation_data.bones[a_bones.w] * a_weight.w;

    gl_Position =
        u_instance_data.model_view_proj *
        //        anim_matrix *
        vec4(a_position, 1.0);

    gl_Position.y = 1. - gl_Position.y;
    v_normal = normalize(a_normal);
    v_texCoords = a_texCoords0;
}
