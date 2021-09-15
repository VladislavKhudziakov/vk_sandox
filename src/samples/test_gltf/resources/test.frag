#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_tex_coords;

layout(location = 0) out vec4 f_FragColor;

layout(set = 0, binding = 0) uniform sampler2D s_BaseColor;

const vec3 light_dir = vec3(1, 1, 1);

void main()
{
    vec3 light_dir_normalized = normalize(light_dir);
    float nDotL = max(dot(v_normal, light_dir_normalized), 0.);

    vec3 base_color = texture(s_BaseColor, v_tex_coords).rgb;
    f_FragColor = vec4(base_color * nDotL, 1.0);
}
