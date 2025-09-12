#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace fw {
    class DeviceBufferCopyHandler {
        vk::raii::Device* device = nullptr;
        vk::raii::Queue queue = nullptr;
        vk::raii::CommandPool commandPool = nullptr;
        // vk::raii::Fence fence = nullptr;

        public:
            DeviceBufferCopyHandler(vk::raii::Device& dev, uint32_t queueFamilyIndex);
            void submit(vk::Buffer& from, vk::Buffer& to, uint32_t size);
            void submit(vk::Buffer& from, vk::Image& to, vk::Extent3D extent);
            DeviceBufferCopyHandler(nullptr_t) {}
            ~DeviceBufferCopyHandler() {}
            DeviceBufferCopyHandler(DeviceBufferCopyHandler&) = delete;
            DeviceBufferCopyHandler& operator=(DeviceBufferCopyHandler&) = delete;
            DeviceBufferCopyHandler(DeviceBufferCopyHandler&& other);
            const DeviceBufferCopyHandler& operator=(DeviceBufferCopyHandler&& other);
    };
}