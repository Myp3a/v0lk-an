#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>

#include <objects.h>

namespace fw {
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const int MAX_FRAMES_IN_FLIGHT = 2;
    const int MAX_FRAMERATE = 60;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    class Renderer {
        public:
            Renderer();
            void init();
            void addObject(fw::Object* obj);
            void delObject(fw::Object* obj);
            void run();
        
        private:
            const std::vector<const char*> validationLayers = {
                "VK_LAYER_KHRONOS_validation"
            };

            #if __APPLE__
            const std::vector<const char*> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            };
            #else
            const std::vector<const char*> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            #endif
            #ifdef NDEBUG
            const bool enableValidationLayers = false;
            #else
            const bool enableValidationLayers = true;
            #endif
            GLFWwindow* window;
        
            vk::raii::Context context;
            vk::raii::Instance instance = nullptr;
            vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
            vk::raii::SurfaceKHR surface = nullptr;
        
            vk::raii::PhysicalDevice physicalDevice = nullptr;
            vk::raii::Device device = nullptr;
        
            vk::raii::Queue graphicsQueue = nullptr;
            vk::raii::Queue presentQueue = nullptr;
        
            vk::raii::SwapchainKHR swapChain = nullptr;
            std::vector<vk::Image> swapChainImages;
            vk::Format swapChainImageFormat;
            vk::Extent2D swapChainExtent;
            std::vector<vk::raii::ImageView> swapChainImageViews;
            std::vector<vk::raii::Framebuffer> swapChainFramebuffers;
        
            vk::raii::RenderPass renderPass = nullptr;
            vk::raii::PipelineLayout pipelineLayout = nullptr;
            vk::raii::Pipeline graphicsPipeline = nullptr;
        
            vk::raii::CommandPool commandPool = nullptr;
            std::vector<vk::raii::CommandBuffer> commandBuffers;
        
            std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
            std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
            std::vector<vk::raii::Fence> inFlightFences;
            uint32_t currentFrame = 0;
            std::chrono::time_point<std::chrono::steady_clock> lastFrameTime = std::chrono::steady_clock::now();
        
            vk::raii::Buffer stagingBuffer = nullptr;
            vk::raii::DeviceMemory stagingBufferMemory = nullptr;
            void* stagingBufferMemoryData;

            vk::raii::Buffer vertexBuffer = nullptr;
            vk::raii::DeviceMemory vertexBufferMemory = nullptr;
            void* vertexBufferMemoryData;
        
            std::set<int> pressedKeys;
        
            std::vector<fw::Object*> objects {};
        
            bool framebufferResized = false;
        
            void initWindow();

            static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
            {
                Renderer* app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
                if (action == GLFW_PRESS) {
                    app->pressedKeys.insert(key);
                }
                else if (action == GLFW_RELEASE) {
                    app->pressedKeys.erase(key);
                }
            }

            static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
                Renderer* app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
                app->framebufferResized = true;
            }

            void initVulkan();
            void mainLoop();
            void cleanup();
            void createInstance();
            void setupDebugMessenger();
            void createSurface();
            void pickPhysicalDevice();
            void createLogicalDevice();
            void createSwapChain();
            void createImageViews();
            void createRenderPass();
            void createGraphicsPipeline();
            void createFramebuffers();
            void createCommandPool();
            void createStagingBuffer();
            void createVertexBuffer();
            void createCommandBuffers();
            void recordCommandBuffer(uint32_t imageIndex, uint32_t bufferIndex);
            void createSyncObjects();
            void drawFrame();
            void putObjectsToBuffer();
            void copyStagingToVertexBuffer();
            uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
            void recreateSwapChain();
            vk::raii::ShaderModule createShaderModule(const std::vector<uint32_t>& code);
            vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
            vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
            vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
            SwapChainSupportDetails querySwapChainSupport(vk::raii::PhysicalDevice device);
            bool isDeviceSuitable(vk::raii::PhysicalDevice device);
            bool checkDeviceExtensionSupport(vk::raii::PhysicalDevice device);
            QueueFamilyIndices findQueueFamilies(vk::raii::PhysicalDevice device);
            std::vector<const char*> getRequiredExtensions();
            bool checkValidationLayerSupport();

            static std::vector<uint32_t> readFile(const std::string& filename) {
                std::ifstream file(filename, std::ios::ate | std::ios::binary);
        
                if (!file.is_open()) {
                    throw std::runtime_error("failed to open file!");
                }
        
                size_t fileSize = (size_t) file.tellg();
                std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
        
                file.seekg(0);
                file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
        
                file.close();
        
                return buffer;
            }
        
            static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
                std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        
                return VK_FALSE;
            }
        };
}