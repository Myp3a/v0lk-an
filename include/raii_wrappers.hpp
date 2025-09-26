#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.hpp>

#include <device_buffer_copy_handler.hpp>

namespace volchara {
    class RAIIvmaBuffer {
        private:
        vk::raii::Device* dev = nullptr;
        vma::Allocator* allocator;
        vk::Buffer buf = nullptr;
        vma::Allocation alloc = nullptr;
        bool mappable = false;
        DeviceBufferCopyHandler* copyHandler = nullptr;
        public:
        RAIIvmaBuffer(vk::raii::Device& dev, vma::Allocator& fromAllocator, vk::BufferCreateInfo bufferInfo, vma::AllocationCreateInfo allocInfo, DeviceBufferCopyHandler& handler);
        RAIIvmaBuffer(nullptr_t) {}
        ~RAIIvmaBuffer();
        RAIIvmaBuffer(RAIIvmaBuffer&) = delete;
        RAIIvmaBuffer& operator=(RAIIvmaBuffer&) = delete;
        RAIIvmaBuffer(RAIIvmaBuffer&& other);
        const RAIIvmaBuffer& operator=(RAIIvmaBuffer&& other);
        operator vk::Buffer() const;
        operator vma::Allocation() const;
        void copyFrom(void* buffer, uint32_t size);
        vma::AllocationInfo allocInfo();
        static void swap(RAIIvmaBuffer& lhs, RAIIvmaBuffer& rhs);
    };

    class RAIIvmaImage {
        private:
        vk::raii::Device* dev = nullptr;
        vma::Allocator* allocator;
        vk::Image img = nullptr;
        vk::raii::ImageView imgView = nullptr;
        vma::Allocation alloc = nullptr;
        bool mappable = false;
        DeviceBufferCopyHandler* copyHandler = nullptr;
        vk::Extent3D imageExtent;
        public:
        RAIIvmaImage(vk::raii::Device& dev, vma::Allocator& fromAllocator, vk::ImageCreateInfo imageInfo, vma::AllocationCreateInfo allocInfo, DeviceBufferCopyHandler& handler, vk::ImageAspectFlags aspectFlags);
        RAIIvmaImage(nullptr_t) {}
        ~RAIIvmaImage();
        RAIIvmaImage(RAIIvmaImage&) = delete;
        RAIIvmaImage& operator=(RAIIvmaImage&) = delete;
        RAIIvmaImage(RAIIvmaImage&& other);
        const RAIIvmaImage& operator=(RAIIvmaImage&& other);
        operator vk::Image() const;
        operator vma::Allocation() const;
        void copyFrom(void* buffer, uint32_t size);
        const vk::ImageView imageView();
        static void swap(RAIIvmaImage& lhs, RAIIvmaImage& rhs);
    };

    class RAIIAllocator {
        private:
        vma::Allocator vmaAlloc;
        vk::raii::Device* dev = nullptr;
        DeviceBufferCopyHandler* copyHandler = nullptr;
        public:
        RAIIAllocator(vk::raii::Instance& inst, vk::raii::PhysicalDevice& physDev, vk::raii::Device& dev, DeviceBufferCopyHandler& handler);
        RAIIAllocator( nullptr_t ) {}
        ~RAIIAllocator();
        RAIIAllocator(RAIIAllocator&) = delete;
        RAIIAllocator& operator=(RAIIAllocator&) = delete;
        RAIIAllocator(RAIIAllocator&& other);
        const RAIIAllocator& operator=(RAIIAllocator&& other);

        RAIIvmaBuffer createBuffer(vk::BufferCreateInfo bufferInfo, vma::AllocationCreateInfo allocInfo);
        RAIIvmaImage createImage(vk::ImageCreateInfo imageInfo, vma::AllocationCreateInfo allocInfo, vk::ImageAspectFlags aspectFlags);
    };
}