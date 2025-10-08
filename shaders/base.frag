#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;

layout(set = 1, binding = 0) uniform sampler texSampler;
layout(set = 1, binding = 1) uniform texture2D textures[];

layout(set = 3, binding = 0) uniform AmbientLightUniformBufferObject {
    vec3 color;
    float brightness;
} ambient;

layout(set = 4, binding = 0) uniform DirectionalLightUniformBufferObject {
    vec3 color;
    float brightness;
    mat4 model;
} directional;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint textureId;
    vec4 color;
    float brightness;
} pcs;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

void main() {
    vec4 pixelColor;
    if (fragColor.r != 0 || fragColor.g != 0 || fragColor.b != 0) {
        pixelColor = vec4(fragColor, 1.0);
    }
    else {
        pixelColor = texture(sampler2D(textures[pcs.textureId], texSampler), fragTexCoord);
    }
    outColor = pixelColor;
    outNormal = vec4(fragNormal, 1.0);
}
