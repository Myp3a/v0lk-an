#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <chrono>
#include <thread>

#include <fw.h>
#include <objects.h>

namespace fw {
    Renderer::Renderer() {
        init();
    }

    void Renderer::init() {
        initWindow();
        initVulkan();
    }

    void Renderer::addObject(fw::Object* obj) {
        objects.push_back(obj);
        putObjectsToBuffer();
    }

    void Renderer::delObject(fw::Object* obj) {
        objects.erase(std::find(objects.begin(), objects.end(), obj));
        putObjectsToBuffer();
    }

    void Renderer::run() {
        mainLoop();
        cleanup();
    }

    void Renderer::initWindow() {
        glfwInit();
        

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwSetErrorCallback( []( int error, const char * msg ) { std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl; } );

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetKeyCallback(window, keyCallback);
    }

    void Renderer::initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    void Renderer::mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        device.waitIdle();
    }

    void Renderer::cleanup() {
        vertexBufferMemory.unmapMemory();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Renderer::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        auto extensions = getRequiredExtensions();
        const std::vector<const char *> empty;
        vk::ApplicationInfo applicationInfo("pipiska", vk::makeVersion(1, 0, 0), "zhopa", vk::makeVersion(1, 0, 0), vk::ApiVersion10);
        vk::InstanceCreateInfo createInfo(
            vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
            &applicationInfo,
            (enableValidationLayers) ? validationLayers : empty,
            extensions
        );

        instance = context.createInstance(createInfo);
    }

    void Renderer::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessengerCreateInfoEXT createInfo({},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            &debugCallback
        );
        debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
    }

    void Renderer::createSurface() {
        VkSurfaceKHR _surf;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surf) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surf);
    }

    void Renderer::pickPhysicalDevice() {
        vk::raii::PhysicalDevices devices(instance);

        for (const vk::raii::PhysicalDevice& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void Renderer::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        const std::vector<float_t> queuePriorities { 1.0f };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueCreateInfo(
                {},
                queueFamily,
                queuePriorities
            );
            queueCreateInfos.push_back(queueCreateInfo);
        }

        const std::vector<const char *> empty;
        vk::DeviceCreateInfo createInfo(
            {},
            queueCreateInfos,
            (enableValidationLayers) ? validationLayers : empty,
            deviceExtensions,
            nullptr  // deviceFeatures
        );

        device = physicalDevice.createDevice(createInfo);
        graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
        presentQueue = device.getQueue(indices.presentFamily.value(), 0);
    }

    void Renderer::createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::vector<uint32_t> queueIndices {indices.graphicsFamily.value(), indices.presentFamily.value()};
        vk::SwapchainCreateInfoKHR createInfo(
            {},
            surface,
            imageCount,
            surfaceFormat.format,
            surfaceFormat.colorSpace,
            extent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment,
            (indices.graphicsFamily != indices.presentFamily) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            queueIndices,
            swapChainSupport.capabilities.currentTransform,
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            presentMode,
            true,
            swapChain
        );
        swapChain = device.createSwapchainKHR(createInfo);

        swapChainImages = swapChain.getImages();
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void Renderer::createImageViews() {
        swapChainImageViews.clear();
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            vk::ImageViewCreateInfo createInfo(
                {},
                swapChainImages[i],
                vk::ImageViewType::e2D,
                swapChainImageFormat,
                {},
                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
            );
            swapChainImageViews.push_back(device.createImageView(createInfo));
        }
    }

    void Renderer::createRenderPass() {
        vk::AttachmentDescription colorAttachment(
            {},
            swapChainImageFormat,
            vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::ePresentSrcKHR
        );

        vk::AttachmentReference colorAttachmentRef(
            0,
            vk::ImageLayout::eColorAttachmentOptimal
        );

        std::vector<vk::AttachmentReference> attachmentRefs {colorAttachmentRef};
        vk::SubpassDescription subpass(
            {},
            vk::PipelineBindPoint::eGraphics,
            {},
            attachmentRefs
        );

        vk::SubpassDependency dependency(
            vk::SubpassExternal,
            0,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            {},
            vk::AccessFlagBits::eColorAttachmentWrite,
            {}
        );

        std::vector<vk::AttachmentDescription> colorAttachmentVec { colorAttachment };
        std::vector<vk::SubpassDescription> subpassVec { subpass };
        std::vector<vk::SubpassDependency> dependencyVec { dependency };
        vk::RenderPassCreateInfo renderPassInfo(
            {},
            colorAttachmentVec,
            subpassVec,
            dependencyVec
        );

        renderPass = device.createRenderPass(renderPassInfo);
    }

    void Renderer::createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/base.vert.spv");
        auto fragShaderCode = readFile("shaders/base.frag.spv");

        vk::raii::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::raii::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
            {},
            vk::ShaderStageFlagBits::eVertex,
            vertShaderModule,
            "main"
        );

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
            {},
            vk::ShaderStageFlagBits::eFragment,
            fragShaderModule,
            "main"
        );

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

        std::vector<vk::VertexInputBindingDescription> inputBindings = std::vector{Vertex::getBindingDescription()};
        std::vector<vk::VertexInputAttributeDescription> inputAttributes = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
            {},
            inputBindings,
            inputAttributes
        );

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
            {},
            vk::PrimitiveTopology::eTriangleList,
            false
        );

        vk::PipelineViewportStateCreateInfo viewportState(
            {},
            1,
            nullptr,
            1,
            nullptr
        );

        vk::PipelineRasterizationStateCreateInfo rasterizer(
            {},
            false,
            false,
            vk::PolygonMode::eFill,
            vk::CullModeFlagBits::eBack,
            vk::FrontFace::eClockwise,
            false,
            {},
            {},
            {},
            1.0f
        );

        vk::PipelineMultisampleStateCreateInfo multisampling(
            {},
            vk::SampleCountFlagBits::e1,
            false
        );

        vk::PipelineColorBlendAttachmentState colorBlendAttachment(
            false,
            vk::BlendFactor::eZero,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eZero,
            vk::BlendFactor::eZero,
            vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        );

        vk::PipelineColorBlendStateCreateInfo colorBlending(
            {},
            false,
            vk::LogicOp::eCopy,
            colorBlendAttachment
        );

        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState(
            {},
            dynamicStates
        );

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
            {},
            nullptr,
            nullptr
        );

        pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo(
            {},
            shaderStages,
            &vertexInputInfo,
            &inputAssembly,
            nullptr,
            &viewportState,
            &rasterizer,
            &multisampling,
            nullptr,
            &colorBlending,
            &dynamicState,
            pipelineLayout,
            renderPass
        );

        graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineInfo);
    }

    void Renderer::createFramebuffers() {
        swapChainFramebuffers.clear();
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vk::FramebufferCreateInfo framebufferInfo(
                {},
                renderPass,
                *swapChainImageViews[i],
                swapChainExtent.width,
                swapChainExtent.height,
                1
            );
            swapChainFramebuffers.push_back(device.createFramebuffer(framebufferInfo));
        }
    }

    void Renderer::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        vk::CommandPoolCreateInfo poolInfo(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueFamilyIndices.graphicsFamily.value()
        );

        commandPool = device.createCommandPool(poolInfo);
    }

    void Renderer::createVertexBuffer() {
        vk::BufferCreateInfo bufferInfo(
            {},
            sizeof(Vertex) * 1, //vertices.size(),
            vk::BufferUsageFlagBits::eVertexBuffer,
            vk::SharingMode::eExclusive
        );
        vertexBuffer = device.createBuffer(bufferInfo);

        vk::MemoryRequirements memRequirements(vertexBuffer.getMemoryRequirements());

        vk::MemoryAllocateInfo allocInfo(
            8388608, // 8MB // memRequirements.size,
            findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        );
        vertexBufferMemory = device.allocateMemory(allocInfo);
        vertexBuffer.bindMemory(vertexBufferMemory, 0);

        vertexBufferMemoryData = vertexBufferMemory.mapMemory(0, 8388608); // bufferInfo.size);
    }

    void Renderer::createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocInfo(
            commandPool,
            vk::CommandBufferLevel::ePrimary,
            MAX_FRAMES_IN_FLIGHT
        );

        commandBuffers = device.allocateCommandBuffers(allocInfo);
    }

    void Renderer::recordCommandBuffer(uint32_t imageIndex, uint32_t bufferIndex) {
        commandBuffers[bufferIndex].reset();

        vk::CommandBufferBeginInfo beginInfo{};

        commandBuffers[bufferIndex].begin(beginInfo);

        vk::Rect2D renderArea(
            {0, 0},
            swapChainExtent
        );
        vk::ClearValue clearValue({0.0f, 0.0f, 0.0f, 1.0f});
        vk::RenderPassBeginInfo renderPassInfo(
            renderPass,
            swapChainFramebuffers[imageIndex],
            renderArea,
            1,
            &clearValue
        );

        commandBuffers[bufferIndex].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commandBuffers[bufferIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
        commandBuffers[bufferIndex].bindVertexBuffers(
            0,
            {vertexBuffer},
            {0}
        );
        vk::Viewport viewport(
            0,
            0,
            swapChainExtent.width,
            swapChainExtent.height,
            0,
            1
        );
        commandBuffers[bufferIndex].setViewport(0, viewport);
        vk::Rect2D scissor(
            {0, 0},
            swapChainExtent
        );
        commandBuffers[bufferIndex].setScissor(0, scissor);
        uint32_t vertexCount = 0;
        for (fw::Object* obj : objects) {
            vertexCount += obj->vertices.size();
        }
        commandBuffers[bufferIndex].draw(vertexCount, 1, 0, 0);
        commandBuffers[bufferIndex].endRenderPass();

        commandBuffers[bufferIndex].end();
    }

    void Renderer::createSyncObjects() {
        vk::FenceCreateInfo fenceInfo(
            vk::FenceCreateFlagBits::eSignaled
        );
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores.push_back(device.createSemaphore({}));
            renderFinishedSemaphores.push_back(device.createSemaphore({}));
            inFlightFences.push_back(device.createFence(fenceInfo));
        }
    }

    void Renderer::drawFrame() {
        device.waitForFences({inFlightFences[currentFrame]}, true, UINT64_MAX);

        std::chrono::duration<float, std::ratio<1, MAX_FRAMERATE>> sinceLastFrame{std::chrono::steady_clock::now() - lastFrameTime};
        if (sinceLastFrame.count() < 1.0f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }

        //for (fw::Object* obj : objects) {  // breaks 'cause reallocation
        for (int i = 0; i < objects.size(); i++) {
            fw::Object* obj = objects[i];
            obj->runFrameCallbacks(
                std::chrono::duration_cast<std::chrono::microseconds>(sinceLastFrame).count() / 1000000.0f,
                pressedKeys
            );
        }
        
        std::pair<vk::Result, uint32_t> nextImagePair = swapChain.acquireNextImage(UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr);
        if (nextImagePair.first == vk::Result::eErrorOutOfDateKHR || nextImagePair.first == vk::Result::eSuboptimalKHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
            return;
        }

        lastFrameTime = std::chrono::steady_clock::now();

        uint32_t imageIndex = nextImagePair.second;

        device.resetFences({inFlightFences[currentFrame]});

        putObjectsToBuffer();
        
        recordCommandBuffer(imageIndex, currentFrame);

        vk::PipelineStageFlags waitStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submitInfo(
            *imageAvailableSemaphores[currentFrame],
            waitStageMask,
            *commandBuffers[currentFrame],
            *renderFinishedSemaphores[currentFrame]
        );
        graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

        vk::PresentInfoKHR presentInfo(
            *renderFinishedSemaphores[currentFrame],
            *swapChain,
            imageIndex
        );

        presentQueue.presentKHR(presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Renderer::putObjectsToBuffer() {
        std::vector<fw::Vertex> vertices;
        for (fw::Object* obj : objects) {
            vertices.insert(vertices.end(), obj->vertices.begin(), obj->vertices.end());
        }
        memcpy(vertexBufferMemoryData, vertices.data(), (size_t)(vertices.size() * sizeof(fw::Vertex)));
    }

    uint32_t Renderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties(physicalDevice.getMemoryProperties());

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void Renderer::recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();

        createSwapChain();
        createImageViews();
        createFramebuffers();
    }

    vk::raii::ShaderModule Renderer::createShaderModule(const std::vector<uint32_t>& code) {
        vk::ShaderModuleCreateInfo createInfo(
            {},
            code
        );

        return device.createShaderModule(createInfo);
    }

    vk::SurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    vk::PresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails Renderer::querySwapChainSupport(vk::raii::PhysicalDevice device) {
        SwapChainSupportDetails details;

        details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
        details.formats = device.getSurfaceFormatsKHR(surface);
        details.presentModes = device.getSurfacePresentModesKHR(surface);
        return details;
    }

    bool Renderer::isDeviceSuitable(vk::raii::PhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool Renderer::checkDeviceExtensionSupport(vk::raii::PhysicalDevice device) {
        std::vector<vk::ExtensionProperties> availableExtensions(device.enumerateDeviceExtensionProperties());
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices Renderer::findQueueFamilies(vk::raii::PhysicalDevice device) {
        QueueFamilyIndices indices {};
        std::vector<vk::QueueFamilyProperties> q = device.getQueueFamilyProperties();

        auto bothIter = std::find_if(q.begin(), q.end(), [&device, &surface = surface](vk::QueueFamilyProperties const &qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics && device.getSurfaceSupportKHR(0, surface); });
        if (bothIter != q.end()) {
            uint32_t ind = static_cast<uint32_t>(std::distance(q.begin(), bothIter));
            indices.graphicsFamily = ind;
            indices.presentFamily = ind;
            return indices;
        }

        auto graphicsIter = std::find_if(q.begin(), q.end(), [&device, &surface = surface](vk::QueueFamilyProperties const &qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
        if (graphicsIter != q.end()) {
            uint32_t ind = static_cast<uint32_t>(std::distance(q.begin(), graphicsIter));
            indices.graphicsFamily = ind;
        }
        auto presentIter = std::find_if(q.begin(), q.end(), [&device, &surface = surface](vk::QueueFamilyProperties const &qfp) { return device.getSurfaceSupportKHR(0, surface); });
        if (presentIter != q.end()) {
            uint32_t ind = static_cast<uint32_t>(std::distance(q.begin(), presentIter));
            indices.presentFamily = ind;
        }

        if (!indices.isComplete()) throw std::runtime_error("Suitable queues not found");
        return indices;
    }

    std::vector<const char*> Renderer::getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool Renderer::checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }
};


// int meow() {
//     fw::Renderer app;

//     try {
//         app.run();
//     } catch (const std::exception& e) {
//         std::cerr << e.what() << std::endl;
//         return EXIT_FAILURE;
//     }

//     return EXIT_SUCCESS;
// }
