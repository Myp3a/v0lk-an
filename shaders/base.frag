#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform sampler texSampler;
layout(set = 1, binding = 1) uniform texture2D textures[];

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint textureId;
} pcs;

layout(location = 0) out vec4 outColor;

void main() {
    // outColor = vec4(fragTexCoord, 0.0, 1.0);
    outColor = texture(sampler2D(textures[pcs.textureId], texSampler), fragTexCoord);
}
