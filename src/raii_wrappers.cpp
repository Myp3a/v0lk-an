#include <algorithm>
#include <utility>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.hpp>

#include <raii_wrappers.hpp>
#include <device_buffer_copy_handler.hpp>

namespace volchara {
    RAIIvmaBuffer::RAIIvmaBuffer(vk::raii::Device& dev, vma::Allocator& fromAllocator, vk::BufferCreateInfo bufferInfo, vma::AllocationCreateInfo allocInfo, DeviceBufferCopyHandler& handler) {
        this->dev = &dev;
        allocator = &fromAllocator;
        std::pair<vk::Buffer, vma::Allocation> p = allocator->createBuffer(bufferInfo, allocInfo);
        buf = p.first;
        alloc = p.second;
        if (allocInfo.flags & vma::AllocationCreateFlagBits::eHostAccessSequentialWrite) mappable = true;
        copyHandler = &handler;
    }
    RAIIvmaBuffer::~RAIIvmaBuffer() {
        if (buf)
            allocator->destroyBuffer(buf, alloc);
        buf = nullptr;
        alloc = nullptr;
    }
    RAIIvmaBuffer::RAIIvmaBuffer(RAIIvmaBuffer&& other) {
        swap(*this, other);
    }
    const RAIIvmaBuffer& RAIIvmaBuffer::operator=(RAIIvmaBuffer&& other) {
        RAIIvmaBuffer t(std::move(other));
        swap(*this, t);
        return *this;
    }
    RAIIvmaBuffer::operator vk::Buffer() const {
        return buf;
    }
    RAIIvmaBuffer::operator vma::Allocation() const {
        return alloc;
    }
    void RAIIvmaBuffer::copyFrom(void* buffer, uint32_t size) {
        if (mappable) {
            allocator->copyMemoryToAllocation(buffer, alloc, 0, size);
        }
        else {
            vk::BufferCreateInfo bufInfo{
                .size = size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive,
            };
            vma::AllocationCreateInfo allocInfo{
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto,
            };
            std::pair<vk::Buffer, vma::Allocation> p = allocator->createBuffer(bufInfo, allocInfo);
            allocator->copyMemoryToAllocation(buffer, p.second, 0, size);
            copyHandler->submit(p.first, buf, size);
            allocator->destroyBuffer(p.first, p.second);
        }
    }
    vma::AllocationInfo RAIIvmaBuffer::allocInfo() {
        return allocator->getAllocationInfo(alloc);
    }
    void RAIIvmaBuffer::swap(RAIIvmaBuffer& lhs, RAIIvmaBuffer& rhs) {
        std::swap(lhs.dev, rhs.dev);
        std::swap(lhs.allocator, rhs.allocator);
        std::swap(lhs.buf, rhs.buf);
        std::swap(lhs.alloc, rhs.alloc);
        std::swap(lhs.mappable, rhs.mappable);
        std::swap(lhs.copyHandler, rhs.copyHandler);
    }

    RAIIvmaImage::RAIIvmaImage(vk::raii::Device& dev, vma::Allocator& fromAllocator, vk::ImageCreateInfo imageInfo, vma::AllocationCreateInfo allocInfo, DeviceBufferCopyHandler& handler) {
        this->dev = &dev;
        allocator = &fromAllocator;
        std::pair<vk::Image, vma::Allocation> p = allocator->createImage(imageInfo, allocInfo);
        img = p.first;
        alloc = p.second;
        if (allocInfo.flags & vma::AllocationCreateFlagBits::eHostAccessSequentialWrite) mappable = true;
        vk::ImageViewCreateInfo viewInfo{
            .image = img,
            .viewType = vk::ImageViewType::e2D,
            .format = imageInfo.format,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        imgView = this->dev->createImageView(viewInfo);
        copyHandler = &handler;
        imageExtent = imageInfo.extent;
    }
    RAIIvmaImage::~RAIIvmaImage() {
        if (img)
            allocator->destroyImage(img, alloc);
        img = nullptr;
        alloc = nullptr;
    }
    RAIIvmaImage::RAIIvmaImage(RAIIvmaImage&& other) {
        swap(*this, other);
    }
    const RAIIvmaImage& RAIIvmaImage::operator=(RAIIvmaImage&& other) {
        RAIIvmaImage t(std::move(other));
        swap(*this, t);
        return *this;
    }
    RAIIvmaImage::operator vk::Image() const {
        return img;
    }
    RAIIvmaImage::operator vma::Allocation() const {
        return alloc;
    }
    void RAIIvmaImage::copyFrom(void* buffer, uint32_t size) {
        if (mappable) {
            allocator->copyMemoryToAllocation(buffer, alloc, 0, size);
        }
        else {
            vk::BufferCreateInfo bufInfo{
                .size = size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive,
            };
            vma::AllocationCreateInfo allocInfo{
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto,
            };
            std::pair<vk::Buffer, vma::Allocation> p = allocator->createBuffer(bufInfo, allocInfo);
            allocator->copyMemoryToAllocation(buffer, p.second, 0, size);
            copyHandler->submit(p.first, img, imageExtent);
            allocator->destroyBuffer(p.first, p.second);
        }
    }
    const vk::ImageView RAIIvmaImage::imageView() {
        return *imgView;
    }
    void RAIIvmaImage::swap(RAIIvmaImage& lhs, RAIIvmaImage& rhs) {
        std::swap(lhs.dev, rhs.dev);
        std::swap(lhs.allocator, rhs.allocator);
        std::swap(lhs.img, rhs.img);
        std::swap(lhs.imgView, rhs.imgView);
        std::swap(lhs.alloc, rhs.alloc);
        std::swap(lhs.mappable, rhs.mappable);
        std::swap(lhs.copyHandler, rhs.copyHandler);
    }

    RAIIAllocator::RAIIAllocator(vk::raii::Instance& inst, vk::raii::PhysicalDevice& physDev, vk::raii::Device& device, DeviceBufferCopyHandler& handler) {
        dev = &device;
        vma::AllocatorCreateInfo allocInfo{
            .physicalDevice = physDev,
            .device = *dev,
            .instance = inst,
            .vulkanApiVersion = VK_API_VERSION_1_4,
        };
        vmaAlloc = vma::createAllocator(allocInfo);
        copyHandler = &handler;
    }
    RAIIAllocator::~RAIIAllocator() {
        vmaAlloc.destroy();
    }
    RAIIAllocator::RAIIAllocator(RAIIAllocator&& other) {
        std::swap(vmaAlloc, other.vmaAlloc);
        std::swap(dev, other.dev);
        std::swap(copyHandler, other.copyHandler);
    }
    const RAIIAllocator& RAIIAllocator::operator=(RAIIAllocator&& other) {
        RAIIAllocator t(std::move(other));
        std::swap(vmaAlloc, t.vmaAlloc);
        std::swap(dev, t.dev);
        std::swap(copyHandler, t.copyHandler);
        return *this;
    }

    RAIIvmaBuffer RAIIAllocator::createBuffer(vk::BufferCreateInfo bufferInfo, vma::AllocationCreateInfo allocInfo) {
        return RAIIvmaBuffer(*dev, vmaAlloc, bufferInfo, allocInfo, *copyHandler);
    }
    RAIIvmaImage RAIIAllocator::createImage(vk::ImageCreateInfo imageInfo, vma::AllocationCreateInfo allocInfo) {
        return RAIIvmaImage(*dev, vmaAlloc, imageInfo, allocInfo, *copyHandler);
    }
}