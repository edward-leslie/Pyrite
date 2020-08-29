#pragma once

#define NOMINMAX
#include <vulkan/vulkan.hpp>

#include <optional>
#include <string>
#include <vector>
#include <unordered_set>

namespace py {
// Initializes the default dispatcher for extension methods.
void InitializeDefaultDispatcher();

vk::ApplicationInfo BuildApplicationInfo(uint32_t apiVersion);

vk::UniqueInstance InitializeVulkan(
    vk::ApplicationInfo const &appInfo,
    std::vector<std::string> const &extensions,
    std::vector<std::string> const &validationLayers,
    bool enableDebug
);

// Invoked by the debug messenger to report on an event.
VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    VkDebugUtilsMessengerCallbackDataEXT const *callbackData,
    void *userData
);

vk::DebugUtilsMessengerCreateInfoEXT BuildDebugMessengerCreateInfo();

// Details regarding a physical device in-relation to a surface.
struct PhysicalDeviceDetails {
    vk::PhysicalDevice Device;
    vk::PhysicalDeviceFeatures Features;
    vk::PhysicalDeviceProperties Properties;
    std::vector<vk::ExtensionProperties> Extensions;

    // Queue Details
    std::vector<vk::QueueFamilyProperties> QueueFamilies;
    std::optional<uint32_t> GraphicsFamilyIndex;
    std::optional<uint32_t> PresentFamilyIndex;

    // Swapchain Details
    vk::SurfaceCapabilitiesKHR Capabilities;
    std::vector<vk::SurfaceFormatKHR> Formats;
    std::vector<vk::PresentModeKHR> PresentModes;

    static PhysicalDeviceDetails Build(vk::PhysicalDevice const &device, vk::SurfaceKHR const &surface);

    bool IsSuitable() const;
};

vk::UniqueDevice BuildDevice(
    vk::PhysicalDevice const &device,
    std::unordered_set<uint32_t> const &queueFamilyIndexes,
    std::vector<std::string> const &extensions,
    std::vector<std::string> const &validationLayers,
    bool enableDebug
);

// Chooses the best physical device for the given instance and surface.
PhysicalDeviceDetails ChoosePhysicalDevice(vk::Instance const &instance, vk::SurfaceKHR const &surface);

struct SwapchainDetails {
    vk::UniqueSwapchainKHR Swapchain;
    vk::Format Format;
    vk::Extent2D Extent;
    std::vector<vk::Image> Images;
    std::vector<vk::UniqueImageView> ImageViews;

    // Initializes the swapchain with the given arguments and, if available, the previous swapchain.
    void Initialize(
            vk::Extent2D const &windowExtent,
            vk::SurfaceKHR const &surface,
            PhysicalDeviceDetails const &physicalDevice,
            vk::Device const &device
    );

    std::vector<vk::UniqueFramebuffer>
    BuildFramebuffers(vk::Device const &device, vk::RenderPass const &renderpass) const;
};

vk::UniqueShaderModule BuildShaderModule(vk::Device const &device, std::vector<uint32_t> const &il);
}
