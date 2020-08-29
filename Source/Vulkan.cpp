#include "Vulkan.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <unordered_set>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace py {
void InitializeDefaultDispatcher() {
	static vk::DynamicLoader dl;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
		dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
}

vk::ApplicationInfo BuildApplicationInfo(uint32_t apiVersion) {
	return vk::ApplicationInfo { "Pyrite", 0, "Pyrite", 0, apiVersion };
}

vk::UniqueInstance InitializeVulkan(
	vk::ApplicationInfo const& appInfo,
	std::vector<std::string> const& extensions,
	std::vector<std::string> const& validationLayers,
	bool enableDebug
) {
	std::vector<char const*> extensionsPtrs;
	extensionsPtrs.reserve(extensions.size());
	std::transform(extensions.cbegin(), extensions.cend(), std::back_inserter(extensionsPtrs),
		[](std::string const& extension) { return extension.data(); }
	);

	std::vector<char const*> validationLayersPtrs;
	validationLayersPtrs.reserve(validationLayers.size());
	std::transform(validationLayers.cbegin(), validationLayers.cend(), std::back_inserter(validationLayersPtrs),
		[](std::string const& layer) { return layer.data(); }
	);

	vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> createInfoChain {
		vk::InstanceCreateInfo {
			{},
			&appInfo,
			static_cast<uint32_t>(validationLayersPtrs.size()), validationLayersPtrs.data(),
			static_cast<uint32_t>(extensionsPtrs.size()), extensionsPtrs.data()
		},
		vk::DebugUtilsMessengerCreateInfoEXT {}
	};

	if (enableDebug) {
		createInfoChain.get<vk::DebugUtilsMessengerCreateInfoEXT>() =
			BuildDebugMessengerCreateInfo();
	}
	else {
		createInfoChain.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
	}

	vk::UniqueInstance instance = vk::createInstanceUnique(createInfoChain.get<vk::InstanceCreateInfo>());
	VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

	return instance;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
	void* userData
) {
	std::cerr << "[Vulkan Debug] " << callbackData->pMessage << std::endl;
	return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT BuildDebugMessengerCreateInfo() {
	return vk::DebugUtilsMessengerCreateInfoEXT {
		{},
		vk::DebugUtilsMessageSeverityFlagsEXT {
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		},
		vk::DebugUtilsMessageTypeFlagsEXT {
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		},
		&DebugMessengerCallback,
		nullptr
	};
}

PhysicalDeviceDetails PhysicalDeviceDetails::Build(vk::PhysicalDevice const& physicalDevice, vk::SurfaceKHR const& surface) {
	PhysicalDeviceDetails details {
		physicalDevice,
		physicalDevice.getFeatures(),
		physicalDevice.getProperties(),
		physicalDevice.enumerateDeviceExtensionProperties(),
		physicalDevice.getQueueFamilyProperties(),
	};

	uint32_t index = 0;
	for (auto const& queueFamily : details.QueueFamilies) {
		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			details.GraphicsFamilyIndex = index;
		}

		if (physicalDevice.getSurfaceSupportKHR(index, surface)) {
			// Does the device support presenting to the surface through the current queue?
			details.PresentFamilyIndex = index;
		}

		index++;
	}

	details.Capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	details.Formats = physicalDevice.getSurfaceFormatsKHR(surface);
	details.PresentModes = physicalDevice.getSurfacePresentModesKHR(surface);
	return details;
}

static bool HasSwapchainExtension(vk::PhysicalDevice const& device) {
	std::vector<vk::ExtensionProperties> extensions = device.enumerateDeviceExtensionProperties();
	return std::any_of(extensions.cbegin(), extensions.cend(),
		[](vk::ExtensionProperties const& e) {
			return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
		}
	);
}

bool PhysicalDeviceDetails::IsSuitable() const {
	bool hasGraphicsQueue = GraphicsFamilyIndex.has_value();
	bool hasGeometryShader = Features.geometryShader;
	bool hasSwapchainExtension = std::any_of(Extensions.cbegin(), Extensions.cend(),
		[](vk::ExtensionProperties const& e) {
			return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
		}
	);
	bool swapchainAdequate = !Formats.empty() && !PresentModes.empty();
	return hasGraphicsQueue && hasGeometryShader && hasGraphicsQueue && swapchainAdequate;
}

vk::UniqueDevice BuildDevice(
	vk::PhysicalDevice const& device,
	std::unordered_set<uint32_t> const& queueFamilyIndexes,
	std::vector<std::string> const& extensions,
	std::vector<std::string> const& validationLayers,
	bool enableDebug
) {
	float queuePriority = 1.0;
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.reserve(queueFamilyIndexes.size());
	std::transform(queueFamilyIndexes.cbegin(), queueFamilyIndexes.cend(), std::back_inserter(queueCreateInfos),
		[&](uint32_t const& index) {
			return vk::DeviceQueueCreateInfo { {}, index, 1, &queuePriority };
		}
	);

	std::vector<char const*> extensionsPtrs;
	extensionsPtrs.reserve(extensions.size());
	std::transform(extensions.cbegin(), extensions.cend(), std::back_inserter(extensionsPtrs),
		[](std::string const& extension) { return extension.data(); }
	);

	std::vector<char const*> validationLayersPtrs;
	validationLayersPtrs.reserve(validationLayers.size());
	std::transform(validationLayers.cbegin(), validationLayers.cend(), std::back_inserter(validationLayersPtrs),
		[](std::string const& layer) { return layer.data(); }
	);

	vk::DeviceCreateInfo deviceCreateInfo {
		{},
		static_cast<uint32_t>(queueCreateInfos.size()), queueCreateInfos.data(),
		static_cast<uint32_t>(validationLayersPtrs.size()), validationLayersPtrs.data(),
		static_cast<uint32_t>(extensionsPtrs.size()), extensionsPtrs.data(),
	};

	vk::UniqueDevice logicalDevice = device.createDeviceUnique(deviceCreateInfo);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(*logicalDevice);
	return logicalDevice;
}

PhysicalDeviceDetails ChoosePhysicalDevice(vk::Instance const& instance, vk::SurfaceKHR const& surface) {
	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
	if (devices.empty()) {
		throw std::runtime_error("failed to find any devices with Vulkan support");
	}

	std::optional<PhysicalDeviceDetails> bestAvailableDevice;
	for (auto const& device : devices) {
		PhysicalDeviceDetails details = PhysicalDeviceDetails::Build(device, surface);
		if (!details.IsSuitable()) {
			// We can't even use this device.
			continue;
		}

		bestAvailableDevice = details;
		if (details.Properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			// To keep it simple, take the first discrete device we see.
			return details;
		}
	}

	if (!bestAvailableDevice) {
		throw std::runtime_error("failed to find a suitable GPU");
	}

	return std::move(*bestAvailableDevice);
}

static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& formats) {
	for (auto const& format : formats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb &&
			format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return format;
		}
	}
	return formats.front();
}

static vk::PresentModeKHR ChooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& modes) {
	for (auto const& mode : modes) {
		if (mode == vk::PresentModeKHR::eMailbox) {
			return mode;
		}
	}
	return vk::PresentModeKHR::eFifo;
}

static vk::Extent2D ChooseSwapExtent(vk::Extent2D windowExtent, vk::SurfaceCapabilitiesKHR const& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}

	vk::Extent2D const& minExtent = capabilities.minImageExtent;
	vk::Extent2D const& maxExtent = capabilities.maxImageExtent;
	return vk::Extent2D {
		std::max(minExtent.width, std::min(maxExtent.width, windowExtent.width)),
		std::max(minExtent.height, std::min(maxExtent.height, windowExtent.height))
	};
}

void SwapchainDetails::Initialize(
	vk::Extent2D const& windowExtent,
	vk::SurfaceKHR const& surface,
	PhysicalDeviceDetails const& physicalDevice,
	vk::Device const& device
) {
	vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(physicalDevice.Formats);
	Format = surfaceFormat.format;
	vk::PresentModeKHR presentMode = ChooseSwapPresentMode(physicalDevice.PresentModes);
	Extent = ChooseSwapExtent(windowExtent, physicalDevice.Capabilities);

	uint32_t minImages = physicalDevice.Capabilities.minImageCount + 1;
	uint32_t maxImages = physicalDevice.Capabilities.maxImageCount;

	vk::SwapchainCreateInfoKHR createInfo {
		{},
		surface,
		std::min(maxImages, minImages),
		Format,
		surfaceFormat.colorSpace,
		Extent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment
	};

	uint32_t queueFamilyIndices[] = {
		physicalDevice.GraphicsFamilyIndex.value(),
		physicalDevice.PresentFamilyIndex.value()
	};
	if (physicalDevice.GraphicsFamilyIndex != physicalDevice.PresentFamilyIndex) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}

	// Don't transform the images in the swap chain.
	createInfo.preTransform = physicalDevice.Capabilities.currentTransform;

	// The window shouldn't be transparent.
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

	createInfo.presentMode = presentMode;
	createInfo.clipped = true;
	createInfo.oldSwapchain = *Swapchain;

	vk::UniqueSwapchainKHR swapchain = device.createSwapchainKHRUnique(createInfo);
	Swapchain = {};
	Images = device.getSwapchainImagesKHR(*swapchain);

	ImageViews.clear();
	ImageViews.reserve(Images.size());
	for (auto const& image : Images) {
		vk::ImageViewCreateInfo createInfo {
			{},
			image,
			vk::ImageViewType::e2D,
			Format,
			vk::ComponentMapping {
				vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity,
			},
			vk::ImageSubresourceRange {
				vk::ImageAspectFlagBits::eColor,
				0,
				1,
				0,
				1
			}
		};
		ImageViews.emplace_back(device.createImageViewUnique(createInfo));
	}

	Swapchain = std::move(swapchain);
}

std::vector<vk::UniqueFramebuffer> SwapchainDetails::BuildFramebuffers(
	vk::Device const& device,
	vk::RenderPass const& renderpass
) const {
	std::vector<vk::UniqueFramebuffer> framebuffers;
	framebuffers.reserve(ImageViews.size());
	for (auto const& view : ImageViews) {
		vk::FramebufferCreateInfo createInfo {
			{},
			renderpass,
			1, &*view,
			Extent.width,
			Extent.height,
			1
		};
		framebuffers.emplace_back(device.createFramebufferUnique(createInfo));
	}
	return framebuffers;
}

vk::UniqueShaderModule BuildShaderModule(vk::Device const& device, std::vector<uint32_t> const& il) {
	vk::ShaderModuleCreateInfo createInfo {
		{},
		il.size() * sizeof(uint32_t),
		il.data()
	};
	return device.createShaderModuleUnique(createInfo);
}
}
