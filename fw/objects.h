#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <vector>
#include <set>

#define GLM_ENABLE_EXPERIMENTAL

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace fw {

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};

struct PushConstants {
    glm::mat4 model;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    bool operator==(const Vertex& other) const {
        return ((pos == other.pos) && (color == other.color));
    }
    bool operator<(const Vertex& other) const {
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

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription(
            0,
            sizeof(Vertex),
            vk::VertexInputRate::eVertex
        );

        return bindingDescription;
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

        vk::VertexInputAttributeDescription positionDescription(
            0,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, pos)
        );
        attributeDescriptions.push_back(positionDescription);

        vk::VertexInputAttributeDescription colorDescription(
            1,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, color)
        );
        attributeDescriptions.push_back(colorDescription);
    
        return attributeDescriptions;
    }
};

class Transform {
    public:
    glm::vec3 translation{0,0,0};
    glm::vec3 scaling{1,1,1};
    glm::quat rotationQuat{1,0,0,0};

    class Position {
        private:
        Transform* parent;
        public:
        Position(Transform* loc): parent(loc) {}
        void forward(float distance, bool world = false) {
            if (world) {
                parent->translation -= glm::vec3(0, 0, distance);
            } else {
                parent->translation -= parent->rotationQuat * glm::vec3(0, 0, distance);
            }
        }
        void backward(float distance, bool world = false) {
            forward(-distance, world);
        }
        void left(float distance, bool world = false) {
            if (world) {
                parent->translation -= parent->rotationQuat * glm::vec3(distance, 0, 0);
            } else {
                parent->translation -= glm::vec3(distance, 0, 0);
            }
        }
        void right(float distance, bool world = false) {
            left(-distance, world);
        }
        void up(float distance, bool world = false) {
            if (world) {
                parent->translation -= glm::vec3(0, distance, 0);
            } else {
                parent->translation -= parent->rotationQuat * glm::vec3(0, distance, 0);
            }
        }
        void down(float distance, bool world = false) {
            up(-distance, world);
        }
    };
    Position position = nullptr;

    class Rotation {
        private:
        Transform* parent;
        public:
        Rotation(Transform* loc): parent(loc) {}
        void up(float degrees, bool world = false) {
            if (world) {
                parent->rotationQuat = glm::normalize(glm::quat({degrees, 0, 0}) * parent->rotationQuat);
            } else {
                parent->rotationQuat = glm::normalize(parent->rotationQuat * glm::quat({degrees, 0, 0}));
            }
        }
        void down(float degrees, bool world = false) {
            up(-degrees, world);
        }
        void left(float degrees, bool world = false) {
            if (world) {
                parent->rotationQuat = glm::normalize(glm::quat({0, degrees, 0}) * parent->rotationQuat);
            } else {
                parent->rotationQuat = glm::normalize(parent->rotationQuat * glm::quat({0, degrees, 0}));
            }
        }
        void right(float degrees, bool world = false) {
            left(-degrees, world);
        }
        void ccw(float degrees, bool world = false) {
            if (world) {
                parent->rotationQuat = glm::normalize(glm::quat({0, 0, degrees}) * parent->rotationQuat);
            } else {
                parent->rotationQuat = glm::normalize(parent->rotationQuat * glm::quat({0, 0, degrees}));
            }
        }
        void cw(float degrees, bool world = false) {
            ccw(-degrees, world);
        }
    };
    Rotation rotation = nullptr;

    Transform() {
        position = Position(this);
        rotation = Rotation(this);
    }

    glm::mat4 modelMatrix() {
        glm::mat4 translationMatrix = glm::translate(translation);
        glm::mat4 scaleMatrix = glm::scale(scaling);
        glm::mat4 rotationMatrix = glm::toMat4(rotationQuat);
        glm::mat4 result = translationMatrix * rotationMatrix * scaleMatrix;
        return result;
    }
    
};

class Object {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<std::function<void(Object*, float, std::set<int>)>> frameCallbacks{};
    Transform transform;

    Object(std::vector<Vertex> initVertices) {
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
    }
    virtual ~Object() = default;  // for RTTI and callback polymorphism
    void runFrameCallbacks(float passedSeconds, std::set<int> pressedKeys) {
        for (auto callback : frameCallbacks) {
            callback(this, passedSeconds, pressedKeys);
        }
        return;
    }
    void setColor(std::array<float, 3> color) {
        for (Vertex& v : vertices) {
            v.color.r = color[0];
            v.color.g = color[1];
            v.color.b = color[2];
        }
    }
};
// class Triangle2D : public Object {};
class Square2D : public Object {
public:
    static std::vector<Vertex> calcVertices(std::array<float, 2> topLeft, std::array<float, 2> botRight) {
        std::vector<Vertex> vertices;
        float top, bot, left, right;
        top = std::min(topLeft[1], botRight[1]);
        bot = std::max(topLeft[1], botRight[1]);
        left = std::min(topLeft[0], botRight[0]);
        right = std::max(topLeft[0], botRight[0]);
        vertices.push_back({{left, top, 0}, {1.0f,1.0f,1.0f}});
        vertices.push_back({{right, top, 0}, {1.0f,1.0f,1.0f}});
        vertices.push_back({{left, bot, 0}, {1.0f,1.0f,1.0f}});
        vertices.push_back({{right, top, 0}, {1.0f,1.0f,1.0f}});
        vertices.push_back({{right, bot, 0}, {1.0f,1.0f,1.0f}});
        vertices.push_back({{left, bot, 0}, {1.0f,1.0f,1.0f}});
        return vertices;
    }

    Square2D(std::array<float, 2> topLeft, std::array<float, 2> botRight, std::array<float, 3> color) : Object(calcVertices(topLeft, botRight)) {
        setColor(color);
    }
};
}