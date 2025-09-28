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
    if (fragColor.r != 0 || fragColor.g != 0 || fragColor.b != 0) {
        outColor = vec4(fragColor, 1.0);
    }
    else {
        outColor = texture(sampler2D(textures[pcs.textureId], texSampler), fragTexCoord);
    }
}
