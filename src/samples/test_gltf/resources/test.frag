#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 v_normal;

layout(location = 0) out vec4 f_FragColor;

const vec3 light_dir = vec3(1, 1, 1);

void main()
{
    vec3 light_dir_normalized = normalize(light_dir);
    float nDotL = max(dot(v_normal, light_dir_normalized), 0.);

    f_FragColor = vec4(vec3(1., 1., 1.) * nDotL, 1.0);
}
