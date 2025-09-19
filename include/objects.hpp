#pragma once

#include <array>
#include <functional>
#include <vector>
#include <set>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace volchara {
    class Renderer;

    struct InitVerticesPlane {
        std::array<float, 3> topLeft;
        std::array<float, 3> topRight;
        std::array<float, 3> botRight;
    };

    struct UniformBufferObject {
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct PushConstants {
        glm::mat4 model;
        uint32_t textureIndex;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        bool operator==(const Vertex& other) const;
        bool operator<(const Vertex& other) const;
        static vk::VertexInputBindingDescription getBindingDescription();
        static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
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
            void forward(float distance, bool world = false);
            void backward(float distance, bool world = false);
            void left(float distance, bool world = false);
            void right(float distance, bool world = false);
            void up(float distance, bool world = false);
            void down(float distance, bool world = false);
        };
        Position position = nullptr;

        class Rotation {
            private:
            Transform* parent;
            public:
            Rotation(Transform* loc): parent(loc) {}
            void up(float degrees, bool world = false);
            void down(float degrees, bool world = false);
            void left(float degrees, bool world = false);
            void right(float degrees, bool world = false);
            void ccw(float degrees, bool world = false);
            void cw(float degrees, bool world = false);
        };
        Rotation rotation = nullptr;

        Transform() {
            position = Position(this);
            rotation = Rotation(this);
        }

        glm::mat4 modelMatrix();
    };

    class Object {
    public:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<std::function<void(Object*, float, std::set<int>)>> frameCallbacks{};
        Transform transform;
        Renderer* renderer;
        uint32_t textureIndex;

        Object(Renderer &renderer, std::vector<Vertex> initVertices);
        virtual ~Object() = default;  // for RTTI and callback polymorphism
        void runFrameCallbacks(float passedSeconds, std::set<int> pressedKeys);
        void setColor(std::array<float, 3> color);
        void loadTexture(const std::string& path);
    };

    class Plane : public Object {
        private:
            static std::vector<Vertex> calcVertices(InitVerticesPlane initVertices);
        public:
            Plane(Renderer& renderer, InitVerticesPlane initVertices);
    };
}