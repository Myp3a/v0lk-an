#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <device_buffer_copy_handler.hpp>

namespace volchara {
    DeviceBufferCopyHandler::DeviceBufferCopyHandler(vk::raii::Device& dev, uint32_t queueFamilyIndex) {
        device = &dev;
        queue = device->getQueue(queueFamilyIndex, 0);
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilyIndex,
        };
        commandPool = device->createCommandPool(poolInfo);
        // fence = device->createFence({});
    }
    void DeviceBufferCopyHandler::submit(vk::Buffer& from, vk::Buffer& to, uint32_t size) {
        vk::CommandBufferAllocateInfo bufInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        vk::raii::CommandBuffer cmdBuf = std::move(device->allocateCommandBuffers(bufInfo).front());
        cmdBuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        vk::BufferCopy copyCmd{
            .size = size,
        };
        cmdBuf.copyBuffer(from, to, copyCmd);
        cmdBuf.end();
        vk::SubmitInfo sub{
            .commandBufferCount = 1,
            .pCommandBuffers = &*cmdBuf,
        };
        queue.submit(sub);
        queue.waitIdle();
    }
    void DeviceBufferCopyHandler::submit(vk::Buffer& from, vk::Image& to, vk::Extent3D extent) {
        vk::CommandBufferAllocateInfo bufInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        vk::raii::CommandBuffer cmdBuf = std::move(device->allocateCommandBuffers(bufInfo).front());
        cmdBuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        vk::BufferImageCopy copyCmd{
            .imageExtent = extent,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        cmdBuf.copyBufferToImage(from, to, vk::ImageLayout::eTransferDstOptimal, copyCmd);
        cmdBuf.end();
        vk::SubmitInfo sub{
            .commandBufferCount = 1,
            .pCommandBuffers = &*cmdBuf,
        };
        queue.submit(sub);
        queue.waitIdle();
    }
    DeviceBufferCopyHandler::DeviceBufferCopyHandler(DeviceBufferCopyHandler&& other) {
        std::swap(device, other.device);
        std::swap(queue, other.queue);
        std::swap(commandPool, other.commandPool);
    }
    const DeviceBufferCopyHandler& DeviceBufferCopyHandler::operator=(DeviceBufferCopyHandler&& other) {
        DeviceBufferCopyHandler t(std::move(other));
        std::swap(device, t.device);
        std::swap(queue, t.queue);
        std::swap(commandPool, t.commandPool);
        return *this;
    }
}