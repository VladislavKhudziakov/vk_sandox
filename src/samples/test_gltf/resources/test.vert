#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_tangent;
layout(location = 3) in vec2 a_texCoords0;
layout(location = 4) in vec2 a_texCoords1;
layout(location = 5) in vec3 a_color;
layout(location = 6) in uvec4 a_joints;
layout(location = 7) in vec4 a_weight;

layout(constant_id = 0) const uint USE_HIERARCHY = 0;
layout(constant_id = 1) const uint USE_SKIN = 0;
layout(constant_id = 2) const uint HIERARCHY_SIZE = 1;
layout(constant_id = 3) const uint SKIN_SIZE = 1;


layout(push_constant) uniform node_id
{
    uint id;
}
u_node_id;


layout(set = 0, binding = 0) uniform instance_data
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 mvp;
}
u_instance_data;


layout(set = 0, binding = 1) readonly buffer hierarchy
{
    mat4 nodes_transforms[HIERARCHY_SIZE];
}
u_hierarchy;


struct skin_joint
{
    mat4 inv_bind_pose;
    uint node;
};


layout(set = 0, binding = 2) uniform skin
{
    skin_joint joints[SKIN_SIZE];
}
u_skin;


layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_tangent;
layout(location = 2) out vec2 v_tex_coords;
layout(location = 3) out vec3 v_vert_color;

void main()
{
    mat4 skin_transform = mat4(1);

    if (USE_HIERARCHY == 1) {
        if (USE_SKIN == 1) {
            skin_transform =
                (u_hierarchy.nodes_transforms[u_skin.joints[a_joints.x].node] * u_skin.joints[a_joints.x].inv_bind_pose) * a_weight.x + (u_hierarchy.nodes_transforms[u_skin.joints[a_joints.y].node] * u_skin.joints[a_joints.y].inv_bind_pose) * a_weight.y + (u_hierarchy.nodes_transforms[u_skin.joints[a_joints.z].node] * u_skin.joints[a_joints.z].inv_bind_pose) * a_weight.z + (u_hierarchy.nodes_transforms[u_skin.joints[a_joints.w].node] * u_skin.joints[a_joints.w].inv_bind_pose) * a_weight.w;
        } else {
            skin_transform = u_hierarchy.nodes_transforms[u_node_id.id];
        }
    }

    gl_Position =
        u_instance_data.mvp * skin_transform * vec4(a_position, 1.0);

    gl_Position.y = 1. - gl_Position.y;

    v_normal = normalize(transpose(inverse(mat3(skin_transform))) * a_normal);
    v_tangent = normalize(transpose(inverse(mat3(skin_transform))) * a_tangent);

    v_tex_coords = a_texCoords0;
    v_vert_color = a_color;
}
