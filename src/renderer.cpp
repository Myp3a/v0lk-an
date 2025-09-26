#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.hpp>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <ratio>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <renderer.hpp>
#include <device_buffer_copy_handler.hpp>
#include <objects.hpp>
#include <raii_wrappers.hpp>
#include <resource_path.hpp>


namespace volchara {

    inline const std::filesystem::path& Renderer::getResourceDir() {
        static const std::filesystem::path p{resourceDirPath};
        return p;
    }

    Renderer::Renderer() : camera(*this) {
        init();
    }

    void Renderer::init() {
        initWindow();
        initVulkan();
    }

    void Renderer::run() {
        mainLoop();
        cleanup();
    }

    void Renderer::addObject(volchara::Object* obj) {
        objects.push_back(obj);
        putObjectsToBuffer();
    }

    void Renderer::delObject(volchara::Object* obj) {
        objects.erase(std::find(objects.begin(), objects.end(), obj));
        putObjectsToBuffer();
    }

    Plane Renderer::objPlaneFromWorldCoordinates(InitVerticesPlane vertices) {
        return Plane::fromWorldCoordinates(*this, vertices);
    }

    void Renderer::putObjectsToBuffer() {
        std::vector<volchara::Vertex> vertices;
        std::vector<uint32_t> indices;
        uint32_t offset = 0;
        for (volchara::Object* obj : objects) {
            vertices.insert(vertices.end(), obj->vertices.begin(), obj->vertices.end());
            std::transform(obj->indices.begin(), obj->indices.end(), std::back_inserter(indices), [offset](uint32_t index){return index + offset;});
            offset += obj->vertices.size();
        }
        stagingBuffer.copyFrom(vertices.data(), (size_t)(vertices.size() * sizeof(volchara::Vertex)));
        vertexBuffer.copyFrom(stagingBuffer.allocInfo().pMappedData, 8388608);
        stagingBuffer.copyFrom(indices.data(), (size_t)(indices.size() * sizeof(uint32_t)));
        indexBuffer.copyFrom(stagingBuffer.allocInfo().pMappedData, 8388608);
    }

    void Renderer::initWindow() {
        glfwInit();
        
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwSetErrorCallback( []( int error, const char * msg ) { std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl; } );

        window = glfwCreateWindow(WIDTH, HEIGHT, "v0l'A';", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCursorPosCallback(window, cursorPositionCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void Renderer::initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createBufferCopyHandler();
        createMemoryAllocator();
        createTextureSampler();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createStagingBuffer();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createSSBOBuffer();
        createDepthResources();
        createFramebuffers();
        uint32_t lisa = createTextureImage(getResourceDir() / "textures/lisa.jpg");
        createDescriptorPool();
        createDescriptorSets();
        loadTextureToDescriptors(lisa);
        createCommandBuffers();
        createSyncObjects();
    }

    void Renderer::mainLoop() {
        while (!glfwWindowShouldClose(window) && !shouldExit) {
            glfwPollEvents();
            drawFrame();
        }
        device.waitIdle();
    }

    void Renderer::cleanup() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    bool Renderer::checkValidationLayerSupport() {
        std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

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

    std::vector<const char*> Renderer::getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        extensions.insert(extensions.end(), instanceExtensions.begin(), instanceExtensions.end());

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void Renderer::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        auto extensions = getRequiredExtensions();
        const std::vector<const char *> empty;
        vk::ApplicationInfo applicationInfo{
            .pApplicationName = "v0l'A';",
            .applicationVersion = VK_API_VERSION_1_2,
            .pEngineName = "v0l'A';",
            .engineVersion = 42,
            .apiVersion = VK_API_VERSION_1_2,
        };
        vk::InstanceCreateInfo createInfo{
            .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
            .pApplicationInfo = &applicationInfo,
            .enabledLayerCount = static_cast<uint32_t>(enableValidationLayers ? 1 : 0),
            .ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        instance = context.createInstance(createInfo);
    }

    void Renderer::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessengerCreateInfoEXT createInfo{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = &debugCallback,
        };
        debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
    }

    void Renderer::createSurface() {
        VkSurfaceKHR _surf;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surf) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surf);
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

    bool Renderer::checkDeviceExtensionSupport(vk::raii::PhysicalDevice device) {
        std::vector<vk::ExtensionProperties> availableExtensions(device.enumerateDeviceExtensionProperties());
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
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

        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceDescriptorIndexingFeaturesEXT> supportedFeatures(device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>());

        return indices.isComplete() && extensionsSupported && swapChainAdequate && hasRequiredPhysicalDeviceFeatures(supportedFeatures.get<vk::PhysicalDeviceFeatures2>()) && hasRequiredPhysicalDeviceDescriptorFeatures(supportedFeatures.get<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>());
    }

    void Renderer::pickPhysicalDevice() {
        vk::raii::PhysicalDevices devices(instance);

        for (const vk::raii::PhysicalDevice& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                physicalDeviceProperties = device.getProperties();
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
            vk::DeviceQueueCreateInfo queueCreateInfo{
                .queueFamilyIndex = queueFamily,
                .queueCount = static_cast<uint32_t>(queuePriorities.size()),
                .pQueuePriorities = queuePriorities.data(),
            };
            queueCreateInfos.push_back(queueCreateInfo);
        }

        const std::vector<const char *> empty;
        vk::PhysicalDeviceFeatures reqDevFeatures{
            .samplerAnisotropy = true,
        };
        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT reqDevDescrFeatures{
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .descriptorBindingVariableDescriptorCount = true,
            .runtimeDescriptorArray = true,
        };
        vk::DeviceCreateInfo createInfo{
            .pNext = &reqDevDescrFeatures,
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
            .enabledLayerCount = static_cast<uint32_t>((enableValidationLayers) ? 1 : 0),
            .ppEnabledLayerNames = (enableValidationLayers) ? validationLayers.data() : nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &reqDevFeatures,
        };

        device = physicalDevice.createDevice(createInfo);
        graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
        presentQueue = device.getQueue(indices.presentFamily.value(), 0);
    }

    void Renderer::createBufferCopyHandler() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        deviceBufferCopyHandler = DeviceBufferCopyHandler(device, queueFamilyIndices.graphicsFamily.value());
    }

    void Renderer::createMemoryAllocator() {
        allocator = RAIIAllocator(instance, physicalDevice, device, deviceBufferCopyHandler);
    }

    void Renderer::createTextureSampler() {
        vk::SamplerCreateInfo samplerInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .mipLodBias = 0,
            .anisotropyEnable = true,
            .maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy,
        };
        textureSampler = device.createSampler(samplerInfo);
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
        vk::SwapchainCreateInfoKHR createInfo{
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = (indices.graphicsFamily != indices.presentFamily) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = static_cast<uint32_t>(queueIndices.size()),
            .pQueueFamilyIndices = queueIndices.data(),
            .preTransform = swapChainSupport.capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = true,
            .oldSwapchain = swapChain,
        };
        swapChain = device.createSwapchainKHR(createInfo);

        swapChainImages = swapChain.getImages();
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    vk::raii::ImageView Renderer::createImageView(const vk::Image& image, vk::Format format) {
        vk::ImageViewCreateInfo createInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        return device.createImageView(createInfo);
    }

    void Renderer::createImageViews() {
        swapChainImageViews.clear();
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews.push_back(createImageView(swapChainImages[i], swapChainImageFormat));
        }
    }

    vk::Format Renderer::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
        for (vk::Format format : candidates) {
            vk::FormatProperties props(physicalDevice.getFormatProperties(format));
            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported depth format!");
    }

    vk::Format Renderer::findDepthFormat() {
        return findSupportedFormat(
            {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
    }

    void Renderer::createRenderPass() {
        vk::AttachmentDescription colorAttachment{
            .format = swapChainImageFormat,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
        };
        vk::AttachmentReference colorAttachmentRef{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
        };

        vk::AttachmentDescription depthAttachment{
            .format = findDepthFormat(),
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        };
        vk::AttachmentReference depthAttachmentRef{
            .attachment = 1,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        };

        vk::SubpassDescription subpass{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef,
            .pDepthStencilAttachment = &depthAttachmentRef,
        };

        vk::SubpassDependency dependency{
            .srcSubpass = vk::SubpassExternal,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        };

        std::vector<vk::AttachmentDescription> attachmentDescriptions { colorAttachment, depthAttachment };
        std::vector<vk::SubpassDescription> subpassVec { subpass };
        std::vector<vk::SubpassDependency> dependencyVec { dependency };
        vk::RenderPassCreateInfo renderPassInfo{
            .attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()),
            .pAttachments = attachmentDescriptions.data(),
            .subpassCount = static_cast<uint32_t>(subpassVec.size()),
            .pSubpasses = subpassVec.data(),
            .dependencyCount = static_cast<uint32_t>(dependencyVec.size()),
            .pDependencies = dependencyVec.data(),
        };

        renderPass = device.createRenderPass(renderPassInfo);
    }

    void Renderer::createDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding uboLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };
        std::vector<vk::DescriptorSetLayoutBinding> uboBindings{uboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo ubolayoutInfo{
            .bindingCount = static_cast<uint32_t>(uboBindings.size()),
            .pBindings = uboBindings.data(),
        };
        descriptorSetLayoutUBO = device.createDescriptorSetLayout(ubolayoutInfo);

        vk::DescriptorSetLayoutBinding samplerLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };
        vk::DescriptorSetLayoutBinding textureLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .descriptorCount = maxTextures,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };
        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {samplerLayoutBinding, textureLayoutBinding};
        std::array<vk::DescriptorBindingFlags, 2> flags{vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind, vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind};
        vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
            .bindingCount = flags.size(),
            .pBindingFlags = flags.data(),
        };
        vk::DescriptorSetLayoutCreateInfo texturelayoutInfo{
            .pNext = &flagsInfo,
            .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data(),            
        };
        descriptorSetLayoutTextures = device.createDescriptorSetLayout(texturelayoutInfo);

        vk::DescriptorSetLayoutBinding ssboLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        };
        std::vector<vk::DescriptorSetLayoutBinding> ssboBindings{ssboLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo ssbolayoutInfo{
            .bindingCount = static_cast<uint32_t>(ssboBindings.size()),
            .pBindings = ssboBindings.data(),
        };
        descriptorSetLayoutSSBO = device.createDescriptorSetLayout(ssbolayoutInfo);
    }

    vk::raii::ShaderModule Renderer::createShaderModule(const std::vector<char *>& code) {
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t *>(code.data()),
        };

        return device.createShaderModule(createInfo);
    }

    void Renderer::createGraphicsPipeline() {
        auto vertShaderCode = readFile(getResourceDir() / "shaders/base.vert.spv");
        auto fragShaderCode = readFile(getResourceDir() / "shaders/base.frag.spv");

        vk::raii::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::raii::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule,
            .pName = "main",
        };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShaderModule,
            .pName = "main",
        };

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

        std::vector<vk::VertexInputBindingDescription> inputBindings = std::vector{Vertex::getBindingDescription()};
        std::vector<vk::VertexInputAttributeDescription> inputAttributes = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = static_cast<uint32_t>(inputBindings.size()),
            .pVertexBindingDescriptions = inputBindings.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(inputAttributes.size()),
            .pVertexAttributeDescriptions = inputAttributes.data(),
        };

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
        };

        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount = 1,
        };

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .lineWidth = 1,
        };

        vk::PipelineMultisampleStateCreateInfo multisampling{
            .sampleShadingEnable = false,
        };

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
        };

        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };

        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = vk::CompareOp::eGreater,
        };

        vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            .size = sizeof(PushConstants),
        };

        std::vector<vk::DescriptorSetLayout> descriptorSets = {*descriptorSetLayoutUBO, *descriptorSetLayoutTextures, *descriptorSetLayoutSSBO};
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = static_cast<uint32_t>(descriptorSets.size()),
            .pSetLayouts = descriptorSets.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        };

        pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo{
            .stageCount = static_cast<uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = pipelineLayout,
            .renderPass = renderPass,
        };

        graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineInfo);
    }

    void Renderer::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
        };

        commandPool = device.createCommandPool(poolInfo);
    }

    void Renderer::createStagingBuffer() {
        vk::BufferCreateInfo bufferInfo{
            .size = 8388608, // 8MB //vertices.size() * sizeof(Vertex) //indices.size() * sizeof(uint32_t),
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        vma::AllocationCreateInfo allocInfo{
            .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
            .usage = vma::MemoryUsage::eAuto,
        };
        stagingBuffer = allocator.createBuffer(bufferInfo, allocInfo);
    }

    void Renderer::createVertexBuffer() {
        vk::BufferCreateInfo bufferInfo{
            .size = 8388608, // 8MB //vertices.size() * sizeof(Vertex)
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        // vertexBuffer = device.createBuffer(bufferInfo);
        vma::AllocationCreateInfo allocInfo{
            .usage = vma::MemoryUsage::eAuto,
        };
        vertexBuffer = allocator.createBuffer(bufferInfo, allocInfo);
    }

    void Renderer::createIndexBuffer() {
        vk::BufferCreateInfo bufferInfo{
            .size = 8388608, // 8MB //indices.size() * sizeof(uint32_t),
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        vma::AllocationCreateInfo allocInfo{
            .usage = vma::MemoryUsage::eAuto,
        };
        indexBuffer = allocator.createBuffer(bufferInfo, allocInfo);
    }

    void Renderer::createUniformBuffers() {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::BufferCreateInfo bufferInfo{
                .size = 8388608, // 8MB //indices.size() * sizeof(uint32_t),
                .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                .sharingMode = vk::SharingMode::eExclusive,
            };
            vma::AllocationCreateInfo allocInfo{
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto,
            };
            uniformBuffers.push_back(allocator.createBuffer(bufferInfo, allocInfo));
        }
    }

    void Renderer::createSSBOBuffer() {
        vk::BufferCreateInfo bufferInfo{
            .size = 8388608 * maxTextures, // 8MB * x
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        vma::AllocationCreateInfo allocInfo{
            .usage = vma::MemoryUsage::eAuto,
        };
        ssboBuffer = allocator.createBuffer(bufferInfo, allocInfo);
    }

    RAIIvmaImage Renderer::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlags aspectFlags) {
        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        vma::AllocationCreateInfo allocInfo{
            .usage = vma::MemoryUsage::eAuto,
        };
        RAIIvmaImage image = allocator.createImage(imageInfo, allocInfo, aspectFlags);
        return image;
    }

    vk::raii::CommandBuffer Renderer::beginSingleTimeCommands() {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };

        std::vector<vk::raii::CommandBuffer> buffers = device.allocateCommandBuffers(allocInfo);
        
        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
        buffers[0].begin(beginInfo);
        return std::move(buffers[0]);
    }

    void Renderer::endSingleTimeCommands(vk::raii::CommandBuffer& buffer) {
        buffer.end();
        vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*buffer,
        };
        graphicsQueue.submit(submitInfo);
        graphicsQueue.waitIdle();
    }

    void Renderer::transitionImageLayout(const vk::Image& image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
        vk::PipelineStageFlags srcStage;
        vk::PipelineStageFlags dstStage;
        vk::AccessFlags srcMask;
        vk::AccessFlags dstMask;
        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eTransfer;
            srcMask = vk::AccessFlagBits::eNone;
            dstMask = vk::AccessFlagBits::eTransferWrite;
        } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            srcStage = vk::PipelineStageFlagBits::eTransfer;
            dstStage = vk::PipelineStageFlagBits::eFragmentShader;
            srcMask = vk::AccessFlagBits::eTransferWrite;
            dstMask = vk::AccessFlagBits::eShaderRead;
        } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            srcMask = vk::AccessFlagBits::eNone;
            dstMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        } else {
            throw std::runtime_error("unsupported transition");
        }

        vk::ImageAspectFlags aspectMask;
        if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        }
        else {
            aspectMask = vk::ImageAspectFlagBits::eColor;
        }
        vk::raii::CommandBuffer buffer(beginSingleTimeCommands());
        vk::ImageMemoryBarrier barrier{
            .srcAccessMask = srcMask,
            .dstAccessMask = dstMask,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange = {.aspectMask = aspectMask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
        };
        buffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
        endSingleTimeCommands(buffer);
    }

    void Renderer::createDepthResources() {
        vk::Format depthFormat = findDepthFormat();
        depthBuffer = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
        transitionImageLayout(depthBuffer, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }

    void Renderer::createFramebuffers() {
        swapChainFramebuffers.clear();
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<const vk::ImageView, 2> attachments{*swapChainImageViews[i], depthBuffer.imageView()};
            vk::FramebufferCreateInfo framebufferInfo{
                .renderPass = renderPass,
                .attachmentCount = attachments.size(),
                .pAttachments = attachments.data(),
                .width = swapChainExtent.width,
                .height = swapChainExtent.height,
                .layers = 1,
            };
            swapChainFramebuffers.push_back(device.createFramebuffer(framebufferInfo));
        }
    }

    uint32_t Renderer::createTextureImage(const std::filesystem::path path) {
        // TODO: deduplication
        int width, height, channels;
        std::vector<char *> texture = readFile(path);
        stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture.data()), texture.size(), &width, &height, &channels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = width * height * STBI_rgb_alpha;

        if (!pixels) {
            throw std::runtime_error("couldn't load texture image");
        };

        vk::BufferCreateInfo createInfo{
            .size = imageSize,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        vk::raii::Buffer stagingBuffer = device.createBuffer(createInfo);
        vma::AllocationCreateInfo allocInfo{
            .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
            .usage = vma::MemoryUsage::eAuto,
        };
        RAIIvmaBuffer textureBuffer = allocator.createBuffer(createInfo, allocInfo);
        
        textureBuffer.copyFrom(pixels, imageSize);
        stbi_image_free(pixels);

        RAIIvmaImage image = createImage(width, height, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

        transitionImageLayout(image, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        image.copyFrom(textureBuffer.allocInfo().pMappedData, imageSize);
        transitionImageLayout(image, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        textures.push_back(std::move(image));
        return textures.size() - 1;
    }

    void Renderer::createDescriptorPool() {
        vk::DescriptorPoolSize uboSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        };
        vk::DescriptorPoolSize ssboSize{
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
        };
        vk::DescriptorPoolSize imageSize{
            .type = vk::DescriptorType::eSampledImage,
            .descriptorCount = maxTextures,
        };
        vk::DescriptorPoolSize samplerSize{
            .type = vk::DescriptorType::eSampler,
            .descriptorCount = 1,
        };
        std::array<vk::DescriptorPoolSize, 4> poolSizes = {uboSize, ssboSize, imageSize, samplerSize};
        vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1024,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes = poolSizes.data(),
        };
        descriptorPool = device.createDescriptorPool(poolInfo);
    }

    void Renderer::createDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutUBO);
        vk::DescriptorSetAllocateInfo uboallocInfo{
            .descriptorPool = descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        };
        descriptorSetsUBO = device.allocateDescriptorSets(uboallocInfo);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::DescriptorBufferInfo ubobufferInfo{
                .buffer = uniformBuffers[i],
                .offset = 0,
                .range = sizeof(UniformBufferObject),
            };
            vk::WriteDescriptorSet ubodescriptorWrite{
                .dstSet = descriptorSetsUBO[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &ubobufferInfo,
            };
            device.updateDescriptorSets(ubodescriptorWrite, nullptr);
        }

        vk::DescriptorSetVariableDescriptorCountAllocateInfo texturecountInfo{
            .descriptorSetCount = 1,
            .pDescriptorCounts = &maxTextures,
        };
        vk::DescriptorSetAllocateInfo textureallocInfo{
            .pNext = &texturecountInfo,
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*descriptorSetLayoutTextures,
        };
        descriptorSetsTextures = device.allocateDescriptorSets(textureallocInfo);
        std::vector<vk::DescriptorImageInfo> imgs;
        for (uint32_t i = 0; i < textures.size(); i++) {
            imgs.push_back({
                .imageView = textures[i].imageView(),
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            });
        }
        vk::WriteDescriptorSet texturedescriptorWrite{
            .dstSet = descriptorSetsTextures[0],
            .dstBinding = 1,
            .descriptorCount = static_cast<uint32_t>(imgs.size()),
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = imgs.data(),
        };
        device.updateDescriptorSets(texturedescriptorWrite, nullptr);

        vk::DescriptorImageInfo samplerInfo{textureSampler};
        vk::WriteDescriptorSet samplerdescriptorWrite{
            .dstSet = descriptorSetsTextures[0],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .pImageInfo = &samplerInfo,
        };
        device.updateDescriptorSets(samplerdescriptorWrite, nullptr);
        
        vk::DescriptorSetAllocateInfo ssboallocInfo{
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*descriptorSetLayoutSSBO,
        };
        descriptorSetsSSBO = device.allocateDescriptorSets(ssboallocInfo);
        vk::DescriptorBufferInfo ssbobufferInfo{
            .buffer = ssboBuffer,
            .range = vk::WholeSize,
        };
        vk::WriteDescriptorSet ssbodescriptorWrite{
            .dstSet = descriptorSetsSSBO[0],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &ssbobufferInfo,
        };
        device.updateDescriptorSets(ssbodescriptorWrite, nullptr);
    }

    uint32_t Renderer::loadTextureToDescriptors(uint32_t textureIndex) {
        vk::DescriptorImageInfo imgInfo{
            .sampler = textureSampler,
            .imageView = textures[textureIndex].imageView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::WriteDescriptorSet setUpdate{
            .dstSet = descriptorSetsTextures[0],
            .dstBinding = 1,
            .dstArrayElement = textureIndex,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &imgInfo,
        };
        device.updateDescriptorSets(setUpdate, nullptr);
        return textureIndex;
    }

    void Renderer::createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };

        commandBuffers = device.allocateCommandBuffers(allocInfo);
    }

    void Renderer::createSyncObjects() {
        vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled,
        };
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores.push_back(device.createSemaphore({}));
            renderFinishedSemaphores.push_back(device.createSemaphore({}));
            inFlightFences.push_back(device.createFence(fenceInfo));
        }
    }

    void Renderer::updateCameraPosition(float passedSeconds) {
        if (pressedKeys.contains(GLFW_KEY_W)) {
            camera.transform.position.forward(passedSeconds * cameraSpeed);
        }
        if (pressedKeys.contains(GLFW_KEY_S)) {
            camera.transform.position.backward(passedSeconds * cameraSpeed);
        }
        if (pressedKeys.contains(GLFW_KEY_A)) {
            camera.transform.position.left(passedSeconds * cameraSpeed);
        }
        if (pressedKeys.contains(GLFW_KEY_D)) {
            camera.transform.position.right(passedSeconds * cameraSpeed);
        }
        if (pressedKeys.contains(GLFW_KEY_Q)) {
            camera.transform.position.down(passedSeconds * cameraSpeed, true);
        }
        if (pressedKeys.contains(GLFW_KEY_E)) {
            camera.transform.position.up(passedSeconds * cameraSpeed, true);
        }
        camera.transform.rotation.up(-cursorOffset.y * mouseSensitivity * 0.0001f);
        camera.transform.rotation.right(cursorOffset.x * mouseSensitivity * 0.0001f, true);
        cursorOffset.x = 0;
        cursorOffset.y = 0;
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
        createDepthResources();
        createFramebuffers();
    }

    void Renderer::recordCommandBuffer(uint32_t imageIndex, uint32_t bufferIndex) {
        commandBuffers[bufferIndex].reset();

        vk::CommandBufferBeginInfo beginInfo{};

        commandBuffers[bufferIndex].begin(beginInfo);

        vk::Rect2D renderArea{
            .extent = swapChainExtent,
        };
        vk::ClearValue clearValueColor({0.0f, 0.0f, 0.0f, 1.0f});
        vk::ClearValue clearValueDepth({0.0f, 0});
        std::array<vk::ClearValue, 2> clearValues{clearValueColor, clearValueDepth};
        vk::RenderPassBeginInfo renderPassInfo{
            .renderPass = renderPass,
            .framebuffer = swapChainFramebuffers[imageIndex],
            .renderArea = renderArea,
            .clearValueCount = clearValues.size(),
            .pClearValues = clearValues.data(),
        };

        commandBuffers[bufferIndex].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commandBuffers[bufferIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
        commandBuffers[bufferIndex].bindVertexBuffers(
            0,
            {vertexBuffer},
            {0}
        );
        vk::Viewport viewport{
            .x = 0,
            .y = static_cast<float>(swapChainExtent.height),
            .width = static_cast<float>(swapChainExtent.width),
            .height = -static_cast<float>(swapChainExtent.height),
            .maxDepth = 1,
        };
        commandBuffers[bufferIndex].bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
        commandBuffers[bufferIndex].setViewport(0, viewport);
        vk::Rect2D scissor{
            .extent = swapChainExtent,
        };
        commandBuffers[bufferIndex].setScissor(0, scissor);
        commandBuffers[bufferIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSetsUBO[bufferIndex], nullptr);
        commandBuffers[bufferIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, *descriptorSetsTextures[0], nullptr);
        commandBuffers[bufferIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, *descriptorSetsSSBO[0], nullptr);
        uint32_t alreadyDrawn = 0;
        for (int i = 0; i < objects.size(); i++) {
            PushConstants cnst;
            cnst.model = objects[i]->transform.modelMatrix();
            cnst.textureIndex = objects[i]->textureIndex;
            commandBuffers[bufferIndex].pushConstants<PushConstants>(pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, {cnst});
            commandBuffers[bufferIndex].drawIndexed(objects[i]->indices.size(), 1, alreadyDrawn, 0, 0);
            alreadyDrawn += objects[i]->indices.size();
        }
        commandBuffers[bufferIndex].endRenderPass();

        commandBuffers[bufferIndex].end();
    }

    void Renderer::updateUniformBuffer(uint32_t imageIndex) {
        UniformBufferObject ubo{};
        ubo.view = glm::inverse(camera.transform.modelMatrix());
        float const fovMult = 1.0f / tan(glm::radians(45.0f) / 2.0f);
        float const aspect = swapChainExtent.width / (float)swapChainExtent.height;
        ubo.proj = glm::mat4(
            fovMult / aspect,    0.0f,  0.0f,  0.0f,
                        0.0f, fovMult,  0.0f,  0.0f,
                        0.0f,    0.0f,  0.0f, -1.0f,
                        0.0f,    0.0f, 0.01f,  0.0f
        );
        uniformBuffers[imageIndex].copyFrom(&ubo, sizeof(ubo));
    }

    void Renderer::drawFrame() {
        device.waitForFences({inFlightFences[currentFrame]}, true, UINT64_MAX);

        std::chrono::duration<float, std::ratio<1, MAX_FRAMERATE>> sinceLastFrame{std::chrono::steady_clock::now() - lastFrameTime};
        if (sinceLastFrame.count() < 1.0f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }

        float passedSeconds = std::chrono::duration_cast<std::chrono::microseconds>(sinceLastFrame).count() / 1000000.0f;

        //for (fw::Object* obj : objects) {  // breaks 'cause reallocation
        for (int i = 0; i < objects.size(); i++) {
            volchara::Object* obj = objects[i];
            obj->runFrameCallbacks(
                passedSeconds,
                pressedKeys
            );
        }

        updateCameraPosition(passedSeconds);
        
        std::pair<vk::Result, uint32_t> nextImagePair = swapChain.acquireNextImage(UINT64_MAX, imageAvailableSemaphores[currentFrame], nullptr);
        if (nextImagePair.first == vk::Result::eErrorOutOfDateKHR || nextImagePair.first == vk::Result::eSuboptimalKHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
            return;
        }

        lastFrameTime = std::chrono::steady_clock::now();

        uint32_t imageIndex = nextImagePair.second;

        device.resetFences({inFlightFences[currentFrame]});
        
        recordCommandBuffer(imageIndex, currentFrame);

        updateUniformBuffer(currentFrame);

        vk::PipelineStageFlags waitStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*imageAvailableSemaphores[currentFrame],
            .pWaitDstStageMask = &waitStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*renderFinishedSemaphores[currentFrame],
        };
        graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*renderFinishedSemaphores[currentFrame],
            .swapchainCount = 1,
            .pSwapchains = &*swapChain,
            .pImageIndices = &imageIndex,
        };

        presentQueue.presentKHR(presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        if (pressedKeys.contains(GLFW_KEY_ESCAPE)) shouldExit = true;
    }
};
