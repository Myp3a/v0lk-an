#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// #define SHOW_NORMALS

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
    layout(offset = 64)  // mat4
    uint textureId;
    mat4 lightModel;
    vec3 lightColor;
    float lightBrightness;
} pcs;

layout(location = 0) out vec4 outColor;

void main() {
    #ifdef SHOW_NORMALS
        outColor = vec4(fragNormal * 0.5 + 0.5, 1.0);
        return;
    #endif

    vec3 ambientColor;
    if (ambient.color.r != 0 || ambient.color.g != 0 || ambient.color.b != 0) {
        ambientColor = ambient.color * ambient.brightness;
    }
    else {
        ambientColor = vec3(1.0, 1.0, 1.0);
    }

    vec3 lightPos = vec3(directional.model * vec4(0,0,0,1));
    vec3 lightVec = lightPos - fragWorldPos;
    float lightDist = length(lightVec);
    // // range-based
    // float lightDistanceIntensity = min(max(directional.brightness - lightDist, 0.0), 1.0);
    // // physical
    float lightAttenuation = directional.brightness / max(lightDist*lightDist, 1e-4);
    float lightDistanceIntensity = min(max(lightAttenuation, 0.0), 1.0);

    float lightNormalizedIntensity = max(dot(fragNormal, normalize(lightVec)), 0.0);
    vec3 lightNormalizedColor = directional.color * lightDistanceIntensity * lightNormalizedIntensity + ambientColor;

    vec4 lightColorOut = vec4(lightNormalizedColor, 1.0);

    vec4 pixelColor;
    if (fragColor.r != 0 || fragColor.g != 0 || fragColor.b != 0) {
        pixelColor = vec4(fragColor, 1.0);
    }
    else {
        pixelColor = texture(sampler2D(textures[pcs.textureId], texSampler), fragTexCoord);
    }
    outColor = lightColorOut * pixelColor;
}
