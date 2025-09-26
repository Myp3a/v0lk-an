#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>

#include <objects.hpp>
#include <renderer.hpp>


namespace volchara {
    bool Vertex::operator==(const Vertex& other) const {
        return ((pos == other.pos) && (color == other.color));
    }
    bool Vertex::operator<(const Vertex& other) const {
        if (pos.x < other.pos.x) return true;
        if (pos.x > other.pos.x) return false;
        if (pos.x == other.pos.x) {
            if (pos.y < other.pos.y) return true;
            if (pos.y > other.pos.y) return false;
            if (pos.y == other.pos.y) {
                if (color.r < other.color.r) return true;
                if (color.r > other.color.r) return false;
                if (color.r == other.color.r) {
                    if (color.g < other.color.g) return true;
                    if (color.g > other.color.g) return false;
                    if (color.g == other.color.g) {
                        if (color.b < other.color.b) return true;
                        if (color.b > other.color.b) return false;
                        if (color.b == other.color.b) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    vk::VertexInputBindingDescription Vertex::getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex,
        };

        return bindingDescription;
    }

    std::vector<vk::VertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

        vk::VertexInputAttributeDescription positionDescription{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, pos),
        };
        attributeDescriptions.push_back(positionDescription);

        vk::VertexInputAttributeDescription colorDescription{
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, color),
        };
        attributeDescriptions.push_back(colorDescription);

        vk::VertexInputAttributeDescription textureCoordinatesDescription{
            .location = 2,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, texCoord),
        };
        attributeDescriptions.push_back(textureCoordinatesDescription);
    
        return attributeDescriptions;
    }

    void Transform::Position::forward(float distance, bool world) {
        if (world) {
            parent->translation += glm::vec3(0, 0, -distance);
        } else {
            parent->translation += parent->rotationQuat * glm::vec3(0, 0, -distance);
        }
    }
    void Transform::Position::backward(float distance, bool world) {
        forward(-distance, world);
    }
    void Transform::Position::left(float distance, bool world) {
        if (world) {
            parent->translation += glm::vec3(-distance, 0, 0);
        } else {
            parent->translation += parent->rotationQuat * glm::vec3(-distance, 0, 0);
        }
    }
    void Transform::Position::right(float distance, bool world) {
        left(-distance, world);
    }
    void Transform::Position::up(float distance, bool world) {
        if (world) {
            parent->translation += glm::vec3(0, distance, 0);
        } else {
            parent->translation += parent->rotationQuat * glm::vec3(0, distance, 0);
        }
    }
    void Transform::Position::down(float distance, bool world) {
        up(-distance, world);
    }

    void Transform::Rotation::up(float degrees, bool world) {
        if (world) {
            parent->rotationQuat = glm::normalize(glm::quat({degrees, 0, 0}) * parent->rotationQuat);
        } else {
            parent->rotationQuat = glm::normalize(parent->rotationQuat * glm::quat({degrees, 0, 0}));
        }
    }
    void Transform::Rotation::down(float degrees, bool world) {
        up(-degrees, world);
    }
    void Transform::Rotation::left(float degrees, bool world) {
        if (world) {
            parent->rotationQuat = glm::normalize(glm::quat({0, degrees, 0}) * parent->rotationQuat);
        } else {
            parent->rotationQuat = glm::normalize(parent->rotationQuat * glm::quat({0, degrees, 0}));
        }
    }
    void Transform::Rotation::right(float degrees, bool world) {
        left(-degrees, world);
    }
    void Transform::Rotation::cw(float degrees, bool world) {
        if (world) {
            parent->rotationQuat = glm::normalize(glm::quat({0, 0, -degrees}) * parent->rotationQuat);
        } else {
            parent->rotationQuat = glm::normalize(parent->rotationQuat * glm::quat({0, 0, -degrees}));
        }
    }
    void Transform::Rotation::ccw(float degrees, bool world) {
        cw(-degrees, world);
    }

    glm::mat4 Transform::modelMatrix() {
        glm::mat4 translationMatrix = glm::translate(translation);
        glm::mat4 scaleMatrix = glm::scale(scaling);
        glm::mat4 rotationMatrix = glm::toMat4(rotationQuat);
        glm::mat4 result = translationMatrix * rotationMatrix * scaleMatrix;
        return result;
    }

    Object::Object(Renderer& renderer, std::vector<Vertex> initVertices, glm::vec3 translation, glm::vec3 scaling, glm::quat rotation) {
        this->renderer = &renderer;
        std::map<Vertex, int32_t> indexMap;
        for (Vertex v : initVertices) {
            auto pos = indexMap.find(v);
            if (pos == indexMap.end()) {
                indices.push_back(vertices.size());
                indexMap.insert({v, vertices.size()});
                vertices.push_back(v);
            } else {
                indices.push_back(pos->second);
            }
        }
        transform.translation = translation;
        transform.scaling = scaling;
        transform.rotationQuat = rotation;
    }
    void Object::runFrameCallbacks(float passedSeconds, std::set<int> pressedKeys) {
        for (auto callback : frameCallbacks) {
            callback(this, passedSeconds, pressedKeys);
        }
        return;
    }
    void Object::setColor(std::array<float, 3> color) {
        for (Vertex& v : vertices) {
            v.color.r = color[0];
            v.color.g = color[1];
            v.color.b = color[2];
        }
    }
    void Object::loadTexture(const std::filesystem::path path) {
        textureIndex = renderer->createTextureImage(path);
        renderer->loadTextureToDescriptors(textureIndex);
    }

    Plane Plane::fromWorldCoordinates(Renderer& renderer, InitVerticesPlane initVertices) {
        std::vector<Vertex> vertices;
        glm::vec3 topLeft = {initVertices.topLeft[0], initVertices.topLeft[1], initVertices.topLeft[2]};
        glm::vec3 topRight = {initVertices.topRight[0], initVertices.topRight[1], initVertices.topRight[2]};
        glm::vec3 botRight = {initVertices.botRight[0], initVertices.botRight[1], initVertices.botRight[2]};
        glm::vec3 botLeft = topLeft - (topRight - botRight);
        glm::vec3 x = topRight - topLeft;
        glm::vec3 y = topLeft - botLeft;
        glm::vec3 z = glm::cross(x, y);
        glm::vec3 center = botLeft + x / 2.0f + y / 2.0f;
        float width = glm::length(x);
        float height = glm::length(y);
        vertices.push_back({.pos = {-width / 2.0f, height / 2.0f, 0}, .texCoord = {1, 0}});
        vertices.push_back({.pos = {width / 2.0f, -height / 2.0f, 0}, .texCoord = {0, 1}});
        vertices.push_back({.pos = {width / 2.0f, height / 2.0f, 0}, .texCoord = {0, 0}});
        vertices.push_back({.pos = {-width / 2.0f, height / 2.0f, 0}, .texCoord = {1, 0}});
        vertices.push_back({.pos = {-width / 2.0f, -height / 2.0f, 0}, .texCoord = {1, 1}});
        vertices.push_back({.pos = {width / 2.0f, -height / 2.0f, 0}, .texCoord = {0, 1}});
        return Plane(renderer, vertices, center, {1, 1, 1}, glm::quatLookAtRH(glm::normalize(z), y));
    }
}