#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_tangent;
layout(location = 3) in vec2 a_texCoords0;
//layout(location = 4) in ivec4 a_bones;
//layout(location = 5) in vec4 a_weight;


layout (constant_id = 0) const uint USE_HIERARCHY = 0;
//layout (constant_id = 1) const uint USE_SKIN = 0;
layout (constant_id = 2) const uint HIERARCHY_SIZE = 1;
//layout (constant_id = 3) const uint SKIN_SIZE = 1;


layout(push_constant) uniform node_id
{
    uint id;
} u_node_id;


layout (set = 1, binding = 0)  uniform instance_data
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 mvp;
} u_instance_data;


layout (set = 1, binding = 1) uniform hierarchy
{
   mat4 nodes_transforms[HIERARCHY_SIZE];
} u_hierarchy;


//struct skin_data
//{
//    mat4 inv_bind_pose;
//    uint node;
//};


//layout (set = 1, binding = 2) uniform skin
//{
//    skin_data data[SKIN_SIZE];
//} u_skin;


layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_texCoords;


void main()
{
    mat4 skin_transform = mat4(1);

    if (USE_HIERARCHY == 1) {
//        if (USE_SKIN == 1) {
//        skin_transform =
//            (u_hierarchy.nodes_transforms[u_skin.data[a_bones.x].node] * u_skin.data[a_bones.x].inv_bind_pose) * a_weight.x +
//            (u_hierarchy.nodes_transforms[u_skin.data[a_bones.x].node] * u_skin.data[a_bones.y].inv_bind_pose) * a_weight.y +
//            (u_hierarchy.nodes_transforms[u_skin.data[a_bones.x].node] * u_skin.data[a_bones.z].inv_bind_pose) * a_weight.z +
//            (u_hierarchy.nodes_transforms[u_skin.data[a_bones.x].node] * u_skin.data[a_bones.w].inv_bind_pose) * a_weight.w;
//        } else {
            skin_transform = u_hierarchy.nodes_transforms[u_node_id.id];
//        }
    }

    gl_Position =
        u_instance_data.mvp * skin_transform * vec4(a_position, 1.0);

    gl_Position.y = 1. - gl_Position.y;
    v_normal = normalize(transpose(inverse(mat3(skin_transform))) * a_normal);
    v_texCoords = a_texCoords0;
}
