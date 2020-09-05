#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#undef min
#undef max

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility>

#include "Vulkan.hpp"
#include "Iterator.hpp"

static std::vector<uint32_t> const VertexShaderIL {
	#include "Shaders/Triangle.vert.spv"
};

static std::vector<uint32_t> const FragmentShaderIL {
	#include "Shaders/Triangle.frag.spv"
};

static GLFWwindow* BuildWindow(vk::Extent2D const& extent) {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(extent.width, extent.height, "Pyrite", nullptr, nullptr);
	if (window == nullptr) {
		throw std::runtime_error("glfwCreateWindow() failed");
	}
	return window;
}

static vk::UniqueSurfaceKHR CreateWindowSurface(vk::Instance const& instance, GLFWwindow* window) {
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("glfwCreateWindowSurface() failed");
	}

	return vk::UniqueSurfaceKHR(
		vk::SurfaceKHR(surface),
		vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(instance)
	);
}

static std::vector<std::string> RequiredVulkanExtensionsForGlfw() {
	uint32_t numGlfwExtensions = 0;
	char const** extensions = glfwGetRequiredInstanceExtensions(&numGlfwExtensions);
	if (extensions == nullptr) {
		throw std::runtime_error("glfwGetRequiredInstanceExtensions() failed");
	}

	return std::vector<std::string>(extensions, extensions + numGlfwExtensions);
}

static vk::UniqueRenderPass BuildRenderPass(vk::Device const& device, vk::Format const& format) {
	vk::AttachmentDescription colorAttachment {
		{},
		format,
		vk::SampleCountFlagBits::e1,
		vk::AttachmentLoadOp::eClear,
		vk::AttachmentStoreOp::eStore,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR
	};
	vk::AttachmentReference colorAttachementRef {
		0,
		vk::ImageLayout::eColorAttachmentOptimal
	};

	vk::SubpassDescription subpass {
		{},
		vk::PipelineBindPoint::eGraphics,
		0, nullptr,
		1, &colorAttachementRef
	};

	vk::SubpassDependency dependency {
		VK_SUBPASS_EXTERNAL,
		0,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		{},
		vk::AccessFlagBits::eColorAttachmentWrite
	};

	vk::RenderPassCreateInfo renderPassInfo {
		{},
		1, &colorAttachment,
		1, &subpass,
		1, &dependency
	};
	return device.createRenderPassUnique(renderPassInfo);
}

static vk::UniquePipeline BuildGraphicsPipeline(
	vk::Device const& device,
	vk::Extent2D const& extent,
	vk::PipelineLayout const& pipelineLayout,
	vk::RenderPass const& renderPass
) {
	vk::UniqueShaderModule vertexShaderModule = py::BuildShaderModule(device, VertexShaderIL);
	vk::UniqueShaderModule fragmentShaderModule = py::BuildShaderModule(device, FragmentShaderIL);
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages {
		vk::PipelineShaderStageCreateInfo {
			{},
			vk::ShaderStageFlagBits::eVertex,
			*vertexShaderModule,
			"main"
		},
		vk::PipelineShaderStageCreateInfo {
			{},
			vk::ShaderStageFlagBits::eFragment,
			*fragmentShaderModule,
			"main"
		}
	};

	// TODO: For now, the defaults are fine as the vertex data is coming from the shader itself.
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo {};

	vk::PipelineInputAssemblyStateCreateInfo assemblyInputInfo {
		{},
		vk::PrimitiveTopology::eTriangleList,
		false
	};

	vk::Viewport viewport {
		0.0f,
		0.0f,
		static_cast<float>(extent.width),
		static_cast<float>(extent.height),
		0.0f,
		1.0f
	};
	vk::Rect2D scissor {
		{ 0, 0 },
		extent
	};
	vk::PipelineViewportStateCreateInfo viewportStateInfo {
		{},
		1, &viewport,
		1, &scissor
	};

	vk::PipelineRasterizationStateCreateInfo rasterizerInfo {
		{},
		false,
		false,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		vk::FrontFace::eClockwise,
		false,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	vk::PipelineMultisampleStateCreateInfo multisamplingInfo {
		{},
		vk::SampleCountFlagBits::e1,
		false,
		1.0f,
		nullptr,
		false,
		false
	};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment {
		false,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};
	vk::PipelineColorBlendStateCreateInfo colorBlendInfo {
		{},
		false,
		vk::LogicOp::eCopy,
		1, &colorBlendAttachment
	};

	vk::GraphicsPipelineCreateInfo pipelineInfo {
		{},
		static_cast<uint32_t>(shaderStages.size()), shaderStages.data(),
		&vertexInputInfo,
		&assemblyInputInfo,
		nullptr,
		&viewportStateInfo,
		&rasterizerInfo,
		&multisamplingInfo,
		nullptr,
		&colorBlendInfo,
		nullptr,
		pipelineLayout,
		renderPass,
		0
	};
	return device.createGraphicsPipelineUnique({}, pipelineInfo);
}

using namespace py;

int main(int argc, char** argv) {
    std::vector<std::string> v { "foo", "bar", "baz", "qux" };
    std::vector<std::string> u { "ed", "edd", "eddy" };
    std::vector<std::string> w = v | Map([](std::string& s) { return s + "!"; }) | Vectorize;
    std::vector<std::string> x = v | Map([](std::string& s) { return s + "?"; }) | AppendTo(std::move(u));

    for (std::string& s : w) {
        std::cout << "w\t" << s << std::endl;
    }

    for (std::string& s : x) {
        std::cout << "x\t" << s << std::endl;
    }

	int result = EXIT_SUCCESS;
	try {
		if (!glfwInit()) {
			throw std::runtime_error("glfwInit() failed");
		}

		if (!glfwVulkanSupported()) {
			throw std::runtime_error("Vulkan not supported by GLFW");
		}

		vk::Extent2D windowExtent = { 1280 , 720 };
		GLFWwindow* window = BuildWindow(windowExtent);

		InitializeDefaultDispatcher();
		vk::ApplicationInfo appInfo = BuildApplicationInfo(VK_API_VERSION_1_1);
		std::vector<std::string> instanceExtensions = RequiredVulkanExtensionsForGlfw();
#ifdef NDEBUG
		vk::UniqueInstance instance = BuildVulkanInstance(appInfo, extensions, {}, false);
		vk::UniqueDebugUtilsMessengerEXT debugMessenger;
#else
		instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		vk::UniqueInstance instance = InitializeVulkan(appInfo, instanceExtensions, { "VK_LAYER_KHRONOS_validation" }, true);
		vk::UniqueDebugUtilsMessengerEXT debugMessenger =
			instance->createDebugUtilsMessengerEXTUnique(BuildDebugMessengerCreateInfo());
#endif

		vk::UniqueSurfaceKHR surface = CreateWindowSurface(*instance, window);
		PhysicalDeviceDetails physicalDeviceDetails = ChoosePhysicalDevice(*instance, *surface);

		std::unordered_set<uint32_t> queueFamilyIndexes =
			{ physicalDeviceDetails.GraphicsFamilyIndex.value(), physicalDeviceDetails.PresentFamilyIndex.value() };
		std::vector<std::string> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#ifdef NDEBUG
		vk::UniqueDevice device = BuildDevice(physicalDeviceDetails.Device, queueFamilyIndexes, deviceExtensions, {}, false);
#else
		vk::UniqueDevice device = BuildDevice(
			physicalDeviceDetails.Device,
			queueFamilyIndexes,
			deviceExtensions,
			{ "VK_LAYER_KHRONOS_validation" },
			true
		);
#endif

		vk::Queue graphicsQueue = device->getQueue(physicalDeviceDetails.GraphicsFamilyIndex.value(), 0);
		vk::Queue presentQueue = device->getQueue(physicalDeviceDetails.PresentFamilyIndex.value(), 0);
		vk::UniqueCommandPool commandPool = device->createCommandPoolUnique(
			{ {}, physicalDeviceDetails.GraphicsFamilyIndex.value() }
		);

		// The previous swapchain is used when initializing the next one, which is why it exists
		// outside of the loop.
		SwapchainDetails swapchainDetails;
		while (!glfwWindowShouldClose(window)) {
			// Setup the swapchain based upon the current window state.
			device->waitIdle();

			int windowWidth, windowHeight;
			glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
			while (windowWidth == 0 && windowHeight == 0) {
				// Wait until the window is brought back into the foreground.
				glfwWaitEvents();
				glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
			}

			windowExtent = vk::Extent2D {
				static_cast<uint32_t>(windowWidth),
				static_cast<uint32_t>(windowHeight)
			};

			swapchainDetails.Initialize(windowExtent, *surface, physicalDeviceDetails, *device);

			vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique({});
			vk::UniqueRenderPass renderPass = BuildRenderPass(*device, swapchainDetails.Format);
			vk::UniquePipeline graphicsPipeline =
				BuildGraphicsPipeline(*device, swapchainDetails.Extent, *pipelineLayout, *renderPass);

			std::vector<vk::UniqueFramebuffer> framebuffers = swapchainDetails.BuildFramebuffers(*device, *renderPass);
			std::vector<vk::UniqueCommandBuffer> commandBuffers = device->allocateCommandBuffersUnique(
				{ *commandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(framebuffers.size()) }
			);

			size_t numBuffers = std::min(framebuffers.size(), commandBuffers.size());
			for (size_t bufferIndex = 0; bufferIndex < numBuffers; ++bufferIndex) {
				vk::Framebuffer& framebuffer = *framebuffers[bufferIndex];
				vk::CommandBuffer& commandBuffer = *commandBuffers[bufferIndex];

				commandBuffer.begin(vk::CommandBufferBeginInfo {});

				vk::ClearValue clearValue = vk::ClearColorValue(
					std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f }
				);
				vk::RenderPassBeginInfo renderPassBegin {
					*renderPass,
					framebuffer,
					vk::Rect2D { { 0, 0 }, swapchainDetails.Extent },
					1, &clearValue
				};
				commandBuffer.beginRenderPass(renderPassBegin, vk::SubpassContents::eInline);

				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
				commandBuffer.draw(3, 1, 0, 0);
				commandBuffer.endRenderPass();
				commandBuffer.end();
			}

			// TODO: Might be able to break this out into a separate object.
			std::vector<vk::UniqueSemaphore> availableImageSemaphores;
			std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
			std::vector<vk::UniqueFence> inFlightFences;
			std::vector<vk::Fence> imagesInFlight(swapchainDetails.Images.size());

			size_t maxInFlightImages = swapchainDetails.Images.size() - 1;
			availableImageSemaphores.reserve(maxInFlightImages);
			renderFinishedSemaphores.reserve(maxInFlightImages);
			inFlightFences.reserve(maxInFlightImages);

			for (size_t i = 0; i < maxInFlightImages; ++i) {
				availableImageSemaphores.emplace_back(device->createSemaphoreUnique({}));
				renderFinishedSemaphores.emplace_back(device->createSemaphoreUnique({}));
				inFlightFences.emplace_back(device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled }));
			}

			size_t syncObjectIndex = 0;
			bool validSwapchain = true;
			while (!glfwWindowShouldClose(window) && validSwapchain) {
				// Draw the next frame.
				glfwPollEvents();

				syncObjectIndex = ++syncObjectIndex < maxInFlightImages ? syncObjectIndex : 0;
				vk::Semaphore& availableImageSemaphore = *availableImageSemaphores[syncObjectIndex];
				vk::Semaphore& renderFinishedSemaphore = *renderFinishedSemaphores[syncObjectIndex];
				vk::Fence& inFlightFence = *inFlightFences[syncObjectIndex];

				device->waitForFences(inFlightFence, true, std::numeric_limits<uint64_t>::max());

				vk::ResultValue<uint32_t> imageIndexResult = device->acquireNextImageKHR(
					*swapchainDetails.Swapchain,
					std::numeric_limits<uint64_t>::max(),
					availableImageSemaphore, {}
				);

				switch (imageIndexResult.result) {
				case vk::Result::eSuccess:
					break;
				case vk::Result::eErrorOutOfDateKHR:
				case vk::Result::eSuboptimalKHR:
					validSwapchain = false;
					continue;
				default:
					throw std::runtime_error("Failed to acquire next image from swapchain");
				}

				syncObjectIndex++;
				uint32_t imageIndex = imageIndexResult.value;

				vk::Fence& imageInFlight = imagesInFlight[imageIndex];
				if (imageInFlight) {
					device->waitForFences(imageInFlight, true, std::numeric_limits<uint64_t>::max());
				}
				imageInFlight = inFlightFence;

				vk::PipelineStageFlags waitStages { vk::PipelineStageFlagBits::eColorAttachmentOutput };
				vk::SubmitInfo submitInfo {
					1, &availableImageSemaphore, &waitStages,
					1, &*commandBuffers[imageIndex],
					1, &renderFinishedSemaphore
				};

				device->resetFences(inFlightFence);
				graphicsQueue.submit(submitInfo, inFlightFence);

				vk::PresentInfoKHR presentInfo {
					1, &renderFinishedSemaphore,
					1, &*swapchainDetails.Swapchain, &imageIndex
				};

				try {
					presentQueue.presentKHR(presentInfo);
				} catch (vk::OutOfDateKHRError const&) {
					validSwapchain = false;
					continue;
				}
			}

			// Wait before destroying anything.
			device->waitIdle();
		}
	} catch (vk::SystemError const& e) {
		std::cerr << "[Vulkan Fatal] " << e.what() << std::endl;
		result = EXIT_FAILURE;
	} catch (std::exception const& e) {
		std::cerr << "[Fatal] " <<  e.what() << std::endl;
		result = EXIT_FAILURE;
	} catch (...) {
		std::cerr << "[Fatal] Unhandled exception" << std::endl;
		result = EXIT_FAILURE;
	}

	glfwTerminate();
	return result;
}
