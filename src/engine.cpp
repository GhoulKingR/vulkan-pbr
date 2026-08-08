#include "engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>
#include <stdexcept>
#include <utility>
#include <vector>
#include <format>
#include "glm/ext/vector_float3.hpp"
#include "glm/matrix.hpp"
#include "vulkan/vulkan_core.h"


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define checkVk(R, S) if(R != VK_SUCCESS) throw std::runtime_error(S"\n");

static GLFWwindow* window;
static VkInstance instance;
static VkSurfaceKHR surface;

// device
static VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
static VkDevice device;
static uint32_t queueIndex;
static VkQueue queue;
static VkCommandPool commandPool;
static std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers;

// swapchain
static VkSwapchainKHR swapchain;
static VkExtent2D swapchainExtent;
static VkFormat swapchainImageFormat;
static std::vector<VkImage> swapchainImages;
static std::vector<VkImageView> swapchainImageViews;
static VkImage depthImage;
static VkImageView depthImageView;
static VkDeviceMemory depthImageMemory;
static VkRenderPass renderPass;
static std::vector<VkFramebuffer> framebuffers;
static struct {
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences;
    uint32_t currentFrame = 0;
    uint32_t imageIndex;
} sync;

// pipeline stuffs
static VkDescriptorSetLayout descriptorSetLayout;
static VkPipelineLayout pipelineLayout;
static VkPipeline pipeline;

// helper functions
static std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = nullptr;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#   if __APPLE__
        requiredExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        requiredExtensions.push_back("VK_KHR_get_physical_device_properties2");
#   endif
#   if DEBUG
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#   endif

    return requiredExtensions;
}

#if DEBUG
    static VkDebugUtilsMessengerEXT debugMessenger;
    static const auto validationLayers = std::array{
        "VK_LAYER_KHRONOS_validation",
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        (void)messageType; // messageType unused
        (void)pUserData; // pUserData unused

        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            std::println(stderr, "[x] Validation layer: {}", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT* createInfo) {
        *createInfo = VkDebugUtilsMessengerCreateInfoEXT{
            .sType              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType        = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback    = debugCallback,
            .pUserData          = nullptr,
        };
    }

    static void setupDebugMessenger() {
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(&createInfo);
        checkVk(
            vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger),
            "Failed to set up debug messenger"
        );
    }

    VkResult vkCreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
            const VkAllocationCallbacks *pAllocator,
            VkDebugUtilsMessengerEXT *pMessenger
    ) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkCreateDebugUtilsMessengerEXT"
        );
        if (func == nullptr) {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        } else {
            return func(instance, pCreateInfo, pAllocator, pMessenger);
        }
    }

    void vkDestroyDebugUtilsMessengerEXT(
            VkInstance instance, VkDebugUtilsMessengerEXT messenger,
            const VkAllocationCallbacks *pAllocator
    ) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkDestroyDebugUtilsMessengerEXT"
        );
        if (func != nullptr) {
            func(instance, messenger, pAllocator);
        }
    }
#endif

static void createWindow() {
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, "PBR Test", nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error("Failed to create window object");
    }
}

static void createInstance() {
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    auto extensions = getRequiredExtensions();
#   if DEBUG
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        populateDebugMessengerCreateInfo(&debugCreateInfo);
#   endif

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = &debugCreateInfo;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#   if __APPLE__
        createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#   endif
#   if DEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
#   else
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
#   endif

    checkVk(
        vkCreateInstance(&createInfo, nullptr, &instance),
        "Failed to create instance"
    );
    checkVk(
        glfwCreateWindowSurface(instance, window, nullptr, &surface),
        "Failed to create window surface"
    );
}

static void pickPhysicalDevice() {
    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // First check if it's a dedicated card and pick that
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice = device;
            break;
        }
    }

    // Just pick the first one that's available if nothing there's no descrete GPU
    if (physicalDevice == VK_NULL_HANDLE) {
        physicalDevice = devices[0];
    }
}

static bool findQueueFamilies(
    VkPhysicalDevice device, VkSurfaceKHR surface,
    uint32_t& queueIndex
) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        bool graphicsFamily = queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (graphicsFamily && presentSupport) {
            queueIndex = i;
            return true;
        }
        i++;
    }

    return false;
}

static void createLogicalDevice() {
    if (!findQueueFamilies(physicalDevice, surface, queueIndex)) {
        throw std::runtime_error("Couldn't find a suitable queue family");
    }

    const auto deviceExtensions = std::array {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#       if __APPLE__
            "VK_KHR_portability_subset",
#       endif
    };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    VkPhysicalDeviceFeatures deviceFeatures{
        .samplerAnisotropy = VK_TRUE,
    };
    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    checkVk(
        vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),
        "Failed to create logical device"
    );

    vkGetDeviceQueue(device, queueIndex, 0, &queue);
}

static void createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueIndex,
    };
    checkVk(
        vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool),
        "Failed to create command pool"
    );
}

static void createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
    };
    checkVk(
        vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),
        "Failed to allocate command buffers"
    );
}

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
static SwapChainSupportDetails querySwapChainSupport(
    VkPhysicalDevice device,
    VkSurfaceKHR surface
) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &presentModeCount,
            details.presentModes.data()
        );
    }
    return details;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats
) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    GLFWwindow* window
) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        };
        actualExtent.width = std::clamp(
            actualExtent.width, capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
        );
        actualExtent.height = std::clamp(
            actualExtent.height, capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        );
        return actualExtent;
    }
}

static void createSwapchain() {
    auto swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    swapchainExtent = chooseSwapExtent(swapChainSupport.capabilities, window);

    uint32_t imageCount = MAX_FRAMES_IN_FLIGHT + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, swapChainSupport.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = swapChainSupport.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    checkVk(
        vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain),
        "Failed to create swap chain"
    );

    swapchainImageFormat = surfaceFormat.format;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
}

static VkImageView createImageView(
    VkDevice device, VkImage image,
    VkFormat format, VkImageAspectFlags aspectFlags
) {
    VkImageViewCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = aspectFlags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkImageView imageView;
    checkVk(
        vkCreateImageView(device, &createInfo, nullptr, &imageView),
        "Failed to create texture image views"
    );
    return imageView;
}

static void createSwapchainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        swapchainImageViews[i] = createImageView(
            device, swapchainImages[i],
            swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT
        );
    }
}

static VkFormat findSupportedFormat(
    VkPhysicalDevice physicalDevice,
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features
) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (
                tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features
        ) {
            return format;
        } else if (
                tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features
        ) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format");
}

static uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice, uint32_t typeFilter,
    VkMemoryPropertyFlags propeties
) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; memProperties.memoryTypeCount; i++) {
        if (
                (typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & propeties) == propeties
        ) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}


static void createImage(
    uint32_t width, uint32_t height, VkFormat format, 
    VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties, VkImage& image,
    VkDeviceMemory& imageMemory
) {
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    checkVk(
        vkCreateImage(device, &imageInfo, nullptr, &image),
        "Failed to create image"
    );

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex =
            findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties),
    };
    checkVk(
        vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory),
        "Failed to allocate image memory"
    );

    vkBindImageMemory(device, image, imageMemory, 0);
}

static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice) {
    return findSupportedFormat(
        physicalDevice,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

static void createSwapchainDepthResources() {
    VkFormat depthFormat = findDepthFormat(physicalDevice);
    createImage(
        swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depthImage, depthImageMemory
    );
    depthImageView = createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

static void createRenderPass() {
    VkAttachmentDescription depthAttachment{
        .format = findDepthFormat(physicalDevice),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription colorAttachment{
        .format = swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef,
    };

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachments.size(),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    checkVk(
        vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass),
        "Failed to create render pass"
    );
}

static void createFramebuffers() {
    framebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {swapchainImageViews[i], depthImageView};
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .layers = 1,
        };
        checkVk(
            vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]),
            "Failed to create framebuffer"
        );
    }
}

static void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        checkVk(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sync.imageAvailableSemaphores[i]),
            "Failed to create availability semaphores"
        );
        checkVk(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sync.renderFinishedSemaphores[i]),
            "Failed to create completion semaphores"
        );
        checkVk(
            vkCreateFence(device, &fenceInfo, nullptr, &sync.inFlightFences[i]),
            "Failed to create fences"
        );
    }
}

static void cleanupSwapchain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

static void recreateSwapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);
    cleanupSwapchain();

    createSwapchain();
    createSwapchainImageViews();
    createSwapchainDepthResources();
    createFramebuffers();
}

static VkVertexInputBindingDescription getVertexBindingDescription() {
    return VkVertexInputBindingDescription{
        .binding = 0,
        .stride = sizeof(Engine::Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
}

static auto getVertexAttributeDescriptions() {
    return std::array{
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Engine::Vertex, pos),
        },
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Engine::Vertex, normal),
        },
    };
}

static void createDescriptorSetLayout() {
    auto bindings = std::array{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = bindings.size(),
        .pBindings = bindings.data(),
    };
    checkVk(
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout),
        "Failed to create descriptor set layout"
    );
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::format("Failed to open '{}'", filename));
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static VkShaderModule createShaderModule(
    const std::vector<char>& code,
    VkDevice device
) {
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };

    VkShaderModule shaderModule;
    checkVk(
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
        "Failed to create shader module"
    );
    return shaderModule;
}

static void createPipeline() {
    // Shader modules
    auto vertShaderCode = readFile("vert.spv");
    auto fragShaderCode = readFile("frag.spv");
    auto vertShaderModule = createShaderModule(vertShaderCode, device);
    auto fragShaderModule = createShaderModule(fragShaderCode, device);

    auto shaderStages = std::array{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
        },
    };

    // Vertex input
    auto bindingDescription = getVertexBindingDescription();
    auto attributeDescriptions = getVertexAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = attributeDescriptions.size(),
        .pVertexAttributeDescriptions = attributeDescriptions.data(),
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Dynamic state
    auto dynamicStates = std::array{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    // Viewport state
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    // Depth-Stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };
    checkVk(
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout),
        "Failed to create pipeline layout"
    );

    // Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shaderStages.size(),
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
        .subpass = 0,
    };
    checkVk(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
        "Failed to create graphics pipeline"
    );
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

// main functions
Engine::Engine() {
    createWindow();
    createInstance();

#   if DEBUG
        setupDebugMessenger();
#   endif

    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createCommandBuffers();

    // create swapchain and other related stuff
    createSwapchain();
    createSwapchainImageViews();
    createSwapchainDepthResources();
    createRenderPass();
    createFramebuffers();
    createSyncObjects();

    // create pipeline stuffs
    createDescriptorSetLayout();
    createPipeline();
}

Engine::~Engine() {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);

    // cleanup swapchain and syncobjects
    cleanupSwapchain();

    vkDestroyRenderPass(device, renderPass, nullptr);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, sync.imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, sync.renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, sync.inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);

#   if DEBUG
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#   endif
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwTerminate();
}

bool Engine::running() {
    return !glfwWindowShouldClose(window);
}

void Engine::wait() {
    vkDeviceWaitIdle(device);
}

bool Engine::beginFrame() {
    // make sure the current image isn't still being worked on
    vkWaitForFences(device, 1, &sync.inFlightFences[sync.currentFrame], VK_TRUE, UINT64_MAX);

    // acquire image from the swapchain
    auto result = vkAcquireNextImageKHR(
        device, swapchain, UINT64_MAX,
        sync.imageAvailableSemaphores[sync.currentFrame],
        VK_NULL_HANDLE, &sync.imageIndex
    );
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    vkResetFences(device, 1, &sync.inFlightFences[sync.currentFrame]);
    vkResetCommandBuffer(commandBuffers[sync.currentFrame], 0);
    auto commandBuffer = commandBuffers[sync.currentFrame];

    // recnrd First sets of commands needed to draw pixels to the screen
    VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    checkVk(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "Failed to begin recording command buffer"
    );

    // Start a render pass
    auto clearValues = std::array{
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}},
    };
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffers[sync.imageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = swapchainExtent,
        },
        .clearValueCount = clearValues.size(),
        .pClearValues = clearValues.data(),
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // set viewport and scissor for the render pass
    VkViewport viewport{
        .x = .0f,
        .y = .0f,
        .width = static_cast<float>(swapchainExtent.width),
        .height = static_cast<float>(swapchainExtent.height),
        .minDepth = .0f,
        .maxDepth = 1.f,
    };
    VkRect2D scissor{
        .offset = {0, 0},
        .extent = swapchainExtent,
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    return true;
}

void Engine::endFrame() {
    auto commandBuffer = commandBuffers[sync.currentFrame];
    vkCmdEndRenderPass(commandBuffer);
    checkVk(
        vkEndCommandBuffer(commandBuffer),
        "Failed to record command buffer"
    );

    // Submit the command buffer with all the recorded instructions for
    // rendering
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sync.imageAvailableSemaphores[sync.currentFrame],
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[sync.currentFrame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &sync.renderFinishedSemaphores[sync.currentFrame],
    };
    checkVk(
        vkQueueSubmit(queue, 1, &submitInfo, sync.inFlightFences[sync.currentFrame]),
        "Failed to submit draw command buffer"
    );

    // Present the rendered image to the screen
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sync.renderFinishedSemaphores[sync.currentFrame],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &sync.imageIndex,
    };

    auto result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    sync.currentFrame = (sync.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}


// Mesh helper functions
static void createDescriptorPool(VkDescriptorPool &descriptorPool) {
    auto poolSizes = std::array{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        },
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes = poolSizes.data(),
    };
    checkVk(
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool),
        "Failed to create descriptor pool"
    );
}

static void createBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer &buffer,
    VkDeviceMemory &bufferMemory
) {
    // create buffer
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    checkVk(
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
        "Failed to create buffer"
    );

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    // allocate buffer memory
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties),
    };
    checkVk(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate buffer memory");

    // bind memory
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

static VkCommandBuffer beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(
    VkBuffer srcBuffer, VkBuffer dstBuffer,
    VkDeviceSize bufferSize
) {
    VkBufferCopy region{.size = bufferSize};
    auto commandBuffer = beginSingleTimeCommands();
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &region);
    endSingleTimeCommands(commandBuffer);
}

static void createVertexBuffer(
    const std::vector<Engine::Vertex> &vertices,
    VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory
) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory
    );
    void *dataStaging;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &dataStaging);
    memcpy(dataStaging, vertices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // actual buffer
    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory
    );

    // copy data
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

static void createIndexBuffer(
    const std::vector<uint32_t> &indices,
    VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory
) {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
        stagingBufferMemory
    );

    // copy data from indices to staging buffer
    void *dataStaging;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &dataStaging);
    memcpy(dataStaging, indices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // actual buffer
    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer, indexBufferMemory
    );

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

static void createUniformBuffers(
    VkDeviceSize bufferSize,
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> &uniformBuffers,
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> &uniformBufferMemories,
    std::array<void*, MAX_FRAMES_IN_FLIGHT> &mappedUniformBufferMemories
) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(
            bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i],
            uniformBufferMemories[i]
        );
        vkMapMemory(
            device, uniformBufferMemories[i], 0,
            bufferSize, 0,
            &mappedUniformBufferMemories[i]
        );
    }
}

static void createDescriptorSets(
    const VkDescriptorPool &descriptorPool,
    const std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> &uniformBuffers,
    VkDeviceSize bufferSize,
    const std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> &projectionUniformBuffers,
    VkDeviceSize projectionBufferSize,
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> &descriptorSets
) {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data(),
    };
    checkVk(
        vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()),
        "Failed to allocate descriptor sets"
    );

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo {
            .buffer = uniformBuffers[i],
            .offset = 0,
            .range = bufferSize,
        };
        VkDescriptorBufferInfo projectionBufferInfo {
            .buffer = projectionUniformBuffers[i],
            .offset = 0,
            .range = projectionBufferSize,
        };
        auto descriptorWrites = std::array{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &bufferInfo,
            },
            VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &projectionBufferInfo,
            },
        };
        vkUpdateDescriptorSets(
            device, descriptorWrites.size(),
            descriptorWrites.data(),
            0, nullptr
        );
    }
}

static void updateUniformBuffer(
    std::array<void*, MAX_FRAMES_IN_FLIGHT> &mappedProjectionUniformBufferMemories,
    VkDeviceSize projectionBufferSize, void* srcProjectionUBO,
    std::array<void*, MAX_FRAMES_IN_FLIGHT> &mappedUniformBufferMemories,
    VkDeviceSize bufferSize, void* srcUniformUBO
) {
    memcpy(
        mappedProjectionUniformBufferMemories[sync.currentFrame],
        srcProjectionUBO, projectionBufferSize
    );
    memcpy(
        mappedUniformBufferMemories[sync.currentFrame],
        srcUniformUBO, bufferSize
    );
}

// Mesh methods and stuff
Engine::Mesh::Mesh(
    const std::vector<Vertex> &vertices,
    const std::vector<uint32_t> &indices
) :
    indexCount(indices.size())
{
    createDescriptorPool(descriptorPool);
    createVertexBuffer(vertices, vertexBuffer, vertexBufferMemory);
    createIndexBuffer(indices, indexBuffer, indexBufferMemory);
    createUniformBuffers(
        sizeof(UBO), uniformBuffers,
        uniformBufferMemories, mappedUniformBufferMemories
    );
    createUniformBuffers(
        sizeof(ProjectionUBO),
        projectionUniformBuffers,
        projectionUniformBufferMemories,
        mappedProjectionUniformBufferMemories
    );
    createDescriptorSets(
        descriptorPool,
        uniformBuffers, sizeof(UBO),
        projectionUniformBuffers, sizeof(ProjectionUBO),
        descriptorSets
    );

    setTransform(
        {0.0, 0.0, 0.0},
        {1.0, 1.0, 1.0},
        {0.0, 0.0, 0.0}
    );
}

Engine::Mesh::~Mesh() {
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBufferMemories[i], nullptr);
        vkDestroyBuffer(device, projectionUniformBuffers[i], nullptr);
        vkFreeMemory(device, projectionUniformBufferMemories[i], nullptr);
    }

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
}

void Engine::Mesh::draw() {
    VkBuffer buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    auto commandBuffer = commandBuffers[sync.currentFrame];

    vkCmdBindVertexBuffers(
        commandBuffer, 0,
        1, buffers, offsets
    );
    vkCmdBindIndexBuffer(
        commandBuffer, indexBuffer, 0,
        VK_INDEX_TYPE_UINT32
    );
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1,
        &descriptorSets[sync.currentFrame],
        0, nullptr
    );
    vkCmdDrawIndexed(
        commandBuffer, indexCount, 1,
        0, 0, 0
    );

    projectionData.proj = glm::perspective(
        glm::radians(45.0f),
        swapchainExtent.width / (float)swapchainExtent.height,
        0.1f, 10.0f
    );
    // projectionData.proj[1][1] *= -1;
    
    updateUniformBuffer(
        mappedProjectionUniformBufferMemories,
        sizeof(projectionData), &projectionData,
        mappedUniformBufferMemories,
        sizeof(fragmentData), &fragmentData
    );
}

void Engine::Mesh::setCamera(
    glm::mat4 view
) {
    projectionData.view = std::move(view);
}

void Engine::Mesh::setTransform(
    glm::vec3 pos, glm::vec3 scale, glm::vec3 rotate
) {
    auto model = glm::identity<glm::mat4>();
    model = glm::translate(model, pos);
    model = glm::scale(model, scale);
    model = glm::rotate(model, rotate.x, glm::vec3(1.0, 0.0, 0.0));
    model = glm::rotate(model, rotate.y, glm::vec3(0.0, 1.0, 0.0));
    model = glm::rotate(model, rotate.z, glm::vec3(0.0, 0.0, 1.0));
    projectionData.model = model;
    projectionData.model_normal = glm::transpose(glm::inverse(model));
}

void Engine::Mesh::setColor(glm::vec3 color) {
    fragmentData.color = std::move(color);
}

void Engine::Mesh::setLight(glm::vec3 light) {
    projectionData.lightPos = std::move(light);
}
