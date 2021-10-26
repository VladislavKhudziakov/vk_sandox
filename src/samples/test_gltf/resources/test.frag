#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_tangent;
layout(location = 2) in vec2 v_tex_coords;
layout(location = 3) in vec3 v_vert_color;

layout(location = 0) out vec4 f_FragColor;

layout(set = 1, binding = 0) uniform sampler2D s_BaseColor;
layout(set = 1, binding = 1) uniform sampler2D s_Normal;
layout(set = 1, binding = 2) uniform sampler2D s_MetallicRoughness;
layout(set = 1, binding = 3) uniform sampler2D s_Occlusion;
layout(set = 1, binding = 4) uniform sampler2D s_Emissive;

const vec3 light_dir = vec3(1, 1, 1);

void main()
{
    vec3 bitangent = normalize(cross(v_normal, v_tangent));

    mat3 n_mat = transpose(mat3(v_normal, v_tangent, bitangent));

    vec3 N = texture(s_Normal, v_tex_coords).rgb;
    vec3 MR = texture(s_MetallicRoughness, v_tex_coords).rgb;
    vec3 O = texture(s_Occlusion, v_tex_coords).rgb;
    vec3 E = texture(s_Emissive, v_tex_coords).rgb;

    vec3 light_dir_normalized = normalize(light_dir);

    float nDotL = max(dot(v_normal, light_dir_normalized), 0.);

    vec3 base_color = texture(s_BaseColor, v_tex_coords).rgb;

    f_FragColor = vec4(base_color * N * MR * O + E, 1.0);
}
