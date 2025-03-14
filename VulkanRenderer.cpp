/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "Vulkanrenderer.h"
#include "VulkanMesh.h"
#include "VulkanTexture.h"
#include "VulkanTextureBuilder.h"
#include "VulkanDescriptorSetLayoutBuilder.h"

#include "VulkanUtils.h"

#ifdef _WIN32
#include "Win32Window.h"
using namespace NCL::Win32Code;
#endif

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanRenderer::VulkanRenderer(Window& window, const VulkanInitialisation& vkInitInfo) : RendererBase(window)
{
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = Vulkan::dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	vkInit = vkInitInfo;

	allocatorInfo		= {};

	for (uint32_t i = 0; i < CommandType::MAX_COMMAND_TYPES; ++i) {
		queueFamilies[i] = -1;
	}

	InitInstance();

	InitPhysicalDevice();

	InitGPUDevice();
	InitMemoryAllocator();

	stagingBuffers = std::make_unique<VulkanStagingBuffers>(GetDevice(), GetMemoryAllocator());

	InitCommandPools();
	InitDefaultDescriptorPool();

	OnWindowResize(window.GetScreenSize().x, window.GetScreenSize().y);
	InitFrameStates(vkInit.framesInFlight);

	pipelineCache = device.createPipelineCache(vk::PipelineCacheCreateInfo());

	frameCmds = frameContexts[currentSwap].cmdBuffer;

	globalFrameSemaphore = Vulkan::CreateTimelineSemaphore(GetDevice());
}

VulkanRenderer::~VulkanRenderer() {
	device.waitIdle();
	depthBuffer.reset();

	for (auto& i : frameContexts) {
		device.destroyImageView(i.colourView);
	};

	for(ChainState & c : swapStates) {
		device.destroyFramebuffer(c.frameBuffer);
		device.destroySemaphore(c.acquireSempaphore);
		device.destroyFence(c.acquireFence);
		device.destroyImageView(c.colourView);
	}
	vmaDestroyAllocator(memoryAllocator);
	device.destroyDescriptorPool(defaultDescriptorPool);
	device.destroySwapchainKHR(swapChain);

	device.destroyCommandPool(commandPools[CommandType::Graphics]);
	device.destroyCommandPool(commandPools[CommandType::Copy]);
	device.destroyCommandPool(commandPools[CommandType::AsyncCompute]);

	device.destroyRenderPass(defaultRenderPass);
	device.destroyPipelineCache(pipelineCache);
	device.destroy(); //Destroy everything except instance before this gets destroyed!

	instance.destroySurfaceKHR(surface);
	instance.destroy();
}

bool VulkanRenderer::InitInstance() {
	vk::ApplicationInfo appInfo = {
		.pApplicationName = this->hostWindow.GetTitle().c_str(),
		.apiVersion = VK_MAKE_VERSION(vkInit.majorVersion, vkInit.minorVersion, 0)
	};
		
	vk::InstanceCreateInfo instanceInfo = {
		.flags = {},
		.pApplicationInfo		 = &appInfo,
		.enabledLayerCount		 = (uint32_t)vkInit.instanceLayers.size(),
		.ppEnabledLayerNames	 = vkInit.instanceLayers.data(),
		.enabledExtensionCount	 = (uint32_t)vkInit.instanceExtensions.size(),
		.ppEnabledExtensionNames = vkInit.instanceExtensions.data()
	};
		
	instance = vk::createInstance(instanceInfo);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

	return true;
}

bool	VulkanRenderer::InitPhysicalDevice() {
	auto enumResult = instance.enumeratePhysicalDevices();

	if (enumResult.empty()) {
		return false; //Guess there's no Vulkan capable devices?!
	}

	gpu = enumResult[0];
	for (auto& i : enumResult) {
		if (i.getProperties().deviceType == vkInit.idealGPU) {
			gpu = i;
			break;
		}
	}

	std::cout << __FUNCTION__ << " Vulkan using physical device " << gpu.getProperties().deviceName << "\n";

	vk::PhysicalDeviceProperties2 props;
	gpu.getProperties2(&props);

	return true;
}

bool VulkanRenderer::InitGPUDevice() {
	InitSurface();
	InitDeviceQueueIndices();

	float queuePriority = 0.0f;

	std::vector< vk::DeviceQueueCreateInfo> queueInfos;

	queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
		.setQueueCount(1)
		.setQueueFamilyIndex(queueFamilies[CommandType::Type::Graphics])
		.setPQueuePriorities(&queuePriority)
	);

	if (queueFamilies[CommandType::Type::AsyncCompute] != queueFamilies[CommandType::Type::Graphics]){
		queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(queueFamilies[CommandType::Type::AsyncCompute])
			.setPQueuePriorities(&queuePriority)
		);
	}
	if (queueFamilies[CommandType::Type::Copy] != queueFamilies[CommandType::Type::Graphics]) {
		queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(queueFamilies[CommandType::Copy])
			.setPQueuePriorities(&queuePriority)
		);
	}
	if (queueFamilies[CommandType::Type::Present] != queueFamilies[CommandType::Type::Graphics]) {
		queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(queueFamilies[CommandType::Present])
			.setPQueuePriorities(&queuePriority)
		);
	}

	vk::PhysicalDeviceFeatures2 deviceFeatures;
	gpu.getFeatures2(&deviceFeatures);

	if (!vkInit.features.empty()) {
		for (int i = 1; i < vkInit.features.size(); ++i) {
			vk::PhysicalDeviceFeatures2* prevStruct = (vk::PhysicalDeviceFeatures2*)vkInit.features[i-1];
			prevStruct->pNext = vkInit.features[i];
		}
		deviceFeatures.pNext = vkInit.features[0];
	}

	vk::DeviceCreateInfo createInfo = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount(queueInfos.size())
		.setPQueueCreateInfos(queueInfos.data());
	
	createInfo.setEnabledLayerCount((uint32_t)vkInit.deviceLayers.size())
		.setPpEnabledLayerNames(vkInit.deviceLayers.data())
		.setEnabledExtensionCount((uint32_t)vkInit.deviceExtensions.size())
		.setPpEnabledExtensionNames(vkInit.deviceExtensions.data());

	createInfo.pNext = &deviceFeatures;

	device = gpu.createDevice(createInfo);

	queues[CommandType::Graphics]		= device.getQueue(queueFamilies[CommandType::Type::Graphics], 0);
	queues[CommandType::AsyncCompute]	= device.getQueue(queueFamilies[CommandType::Type::AsyncCompute], 0);
	queues[CommandType::Copy]			= device.getQueue(queueFamilies[CommandType::Type::Copy], 0);
	queues[CommandType::Present]		= device.getQueue(queueFamilies[CommandType::Type::Present], 0);

	deviceMemoryProperties = gpu.getMemoryProperties();
	deviceProperties = gpu.getProperties();

	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

	//vk::DebugUtilsMessengerCreateInfoEXT debugInfo;
	//debugInfo.pfnUserCallback = DebugCallbackFunction;
	//debugInfo.messageSeverity = vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags;
	//debugInfo.messageType = vk::FlagTraits<vk::DebugUtilsMessageTypeFlagBitsEXT>::allFlags;

	//debugMessenger = instance.createDebugUtilsMessengerEXT(debugInfo);

	return true;
}

bool VulkanRenderer::InitSurface() {
#ifdef _WIN32
	Win32Window* window = (Win32Window*)&hostWindow;

	surface = instance.createWin32SurfaceKHR(
		{
			.flags = {},
			.hinstance = window->GetInstance(),
			.hwnd = window->GetHandle()
		}
	);
#endif

	auto formats = gpu.getSurfaceFormatsKHR(surface);

	if (formats.size() == 1 && formats[0].format == vk::Format::eUndefined) {
		surfaceFormat	= vk::Format::eB8G8R8A8Unorm;
		surfaceSpace	= formats[0].colorSpace;
	}
	else {
		surfaceFormat	= formats[0].format;
		surfaceSpace	= formats[0].colorSpace;
	}

	return formats.size() > 0;
}

void	VulkanRenderer::InitFrameStates(uint32_t framesInFlight) {
	frameContexts.resize(framesInFlight);

	auto buffers = device.allocateCommandBuffers(
		{
			.commandPool		= commandPools[CommandType::Graphics],
			.level				= vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = framesInFlight
		}
	);

	uint32_t index = 0;
	for (FrameContext& context : frameContexts) {
		context.device = GetDevice();
		context.descriptorPool = GetDescriptorPool();

		context.cmdBuffer = buffers[index];

		//context.workSempaphore		= Vulkan::CreateTimelineSemaphore(GetDevice());

		context.defaultViewport		= defaultViewport;
		context.defaultScreenRect	= defaultScreenRect;

		context.colourFormat		= surfaceFormat;

		context.depthImage			= depthBuffer->GetImage();
		context.depthView			= depthBuffer->GetDefaultView();
		context.depthFormat			= depthBuffer->GetFormat();

		for (int i = 0; i < CommandType::Type::MAX_COMMAND_TYPES; ++i) {
			context.commandPools[i] = commandPools[i];
			context.queues[i]		= queues[i];
		}
		index++;
	}
}

void VulkanRenderer::InitBufferChain(vk::CommandBuffer  cmdBuffer) {
	vk::SwapchainKHR oldChain					= swapChain;

	vk::SurfaceCapabilitiesKHR surfaceCaps = gpu.getSurfaceCapabilitiesKHR(surface);

	vk::Extent2D swapExtents = vk::Extent2D((int)hostWindow.GetScreenSize().x, (int)hostWindow.GetScreenSize().y);

	auto presentModes = gpu.getSurfacePresentModesKHR(surface); //Type is of vector of PresentModeKHR

	vk::PresentModeKHR currentPresentMode	= vk::PresentModeKHR::eFifo;

	for (const auto& i : presentModes) {
		if (i == vkInit.idealPresentMode) {
			currentPresentMode = i;
			break;
		}
	}

	if (currentPresentMode != vkInit.idealPresentMode) {
		std::cout << __FUNCTION__ << " Vulkan cannot use chosen present mode! Defaulting to FIFO...\n";
	}

	vk::SurfaceTransformFlagBitsKHR idealTransform;

	if (surfaceCaps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
		idealTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else {
		idealTransform = surfaceCaps.currentTransform;
	}

	int idealImageCount = surfaceCaps.minImageCount + 1;
	if (surfaceCaps.maxImageCount > 0) {
		idealImageCount = std::min(idealImageCount, (int)surfaceCaps.maxImageCount);
	}

	vk::SwapchainCreateInfoKHR swapInfo;

	swapInfo.setPresentMode(currentPresentMode)
		.setPreTransform(idealTransform)
		.setSurface(surface)
		.setImageColorSpace(surfaceSpace)
		.setImageFormat(surfaceFormat)
		.setImageExtent(swapExtents)
		.setMinImageCount(idealImageCount)
		.setOldSwapchain(oldChain)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

	swapChain = device.createSwapchainKHR(swapInfo);

	if (!swapStates.empty()) {
		for (unsigned int i = 0; i < swapStates.size(); ++i) {
			device.destroyImageView(swapStates[i].colourView);
			device.destroyFramebuffer(swapStates[i].frameBuffer);
		}
	}
	if (oldChain) {
		device.destroySwapchainKHR(oldChain);
	}

	auto images = device.getSwapchainImagesKHR(swapChain);

	swapStates.resize(images.size());

	for(int i = 0; i < swapStates.size(); ++i) {
		ChainState& chain = swapStates[i];

		chain.acquireSempaphore = device.createSemaphore({});
		chain.acquireFence		= device.createFence({});

		chain.frameFinishedSemaphore = device.createSemaphore({});

		chain.swapCmds = CmdBufferCreate(device, GetCommandPool(CommandType::Graphics), "Window swap cmds").release();

		chain.colourImage = images[i];

		ImageTransitionBarrier(cmdBuffer, chain.colourImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput);

		chain.colourView = device.createImageView(
			vk::ImageViewCreateInfo()
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
			.setFormat(surfaceFormat)
			.setImage(chain.colourImage)
			.setViewType(vk::ImageViewType::e2D)	
		);

		vk::ImageView attachments[2];

		vk::FramebufferCreateInfo createInfo = vk::FramebufferCreateInfo()
			.setWidth(hostWindow.GetScreenSize().x)
			.setHeight(hostWindow.GetScreenSize().y)
			.setLayers(1)
			.setAttachmentCount(2)
			.setPAttachments(attachments)
			.setRenderPass(defaultRenderPass);

		attachments[0] = chain.colourView;
		attachments[1] = *depthBuffer->defaultView;
		chain.frameBuffer = device.createFramebuffer(createInfo);
	}
	currentSwap = 0; //??
}

void	VulkanRenderer::InitCommandPools() {	
	for (uint32_t i = 0; i < CommandType::MAX_COMMAND_TYPES; ++i) {
		commandPools[i] = device.createCommandPool(
			{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = queueFamilies[i]
			}
		);
	}
}

void	VulkanRenderer::InitMemoryAllocator() {
	VmaVulkanFunctions funcs = {};
	funcs.vkGetInstanceProcAddr = ::vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr;
	funcs.vkGetDeviceProcAddr   = ::vk::detail::defaultDispatchLoaderDynamic.vkGetDeviceProcAddr;

	allocatorInfo.physicalDevice = gpu;
	allocatorInfo.device	= device;
	allocatorInfo.instance	= instance;

	allocatorInfo.flags |= vkInit.vmaFlags;

	allocatorInfo.pVulkanFunctions = &funcs;
	vmaCreateAllocator(&allocatorInfo, &memoryAllocator);
}

bool VulkanRenderer::InitDeviceQueueIndices() {
	std::vector<vk::QueueFamilyProperties> deviceQueueProps = gpu.getQueueFamilyProperties();

	VkBool32 supportsPresent = false;

	int gfxBits		= INT_MAX;
	int computeBits = INT_MAX;
	int copyBits	= INT_MAX;

	for (unsigned int i = 0; i < deviceQueueProps.size(); ++i) {
		supportsPresent = gpu.getSurfaceSupportKHR(i, surface);

		int queueBitCount = std::popcount((uint32_t)deviceQueueProps[i].queueFlags);

		if (deviceQueueProps[i].queueFlags & vk::QueueFlagBits::eGraphics && queueBitCount < gfxBits) {
			queueFamilies[CommandType::Graphics] = i;
			gfxBits = queueBitCount;
			if (supportsPresent && queueFamilies[CommandType::Present] == -1) {
				queueFamilies[CommandType::Present] = i;
			}
		}

		if (deviceQueueProps[i].queueFlags & vk::QueueFlagBits::eCompute && queueBitCount < computeBits) {
			queueFamilies[CommandType::AsyncCompute] = i;
			computeBits = queueBitCount;
		}

		if (deviceQueueProps[i].queueFlags & vk::QueueFlagBits::eTransfer && queueBitCount < copyBits) {
			queueFamilies[CommandType::Copy] = i;
			copyBits = queueBitCount;
		}
	}

	if (queueFamilies[CommandType::Graphics] == -1) {
		return false;
	}

	if (queueFamilies[CommandType::AsyncCompute] == -1) {
		queueFamilies[CommandType::AsyncCompute] = queueFamilies[CommandType::Graphics];
	}
	else {
		std::cout << __FUNCTION__ << " Device supports async compute!\n";
	}

	if (queueFamilies[CommandType::Copy] == -1) {
		queueFamilies[CommandType::Copy] = queueFamilies[CommandType::Copy];
	}
	else {
		std::cout << __FUNCTION__ << " Device supports async copy!\n";
	}

	return true;
}

void VulkanRenderer::OnWindowResize(int width, int height) {
	if (!hostWindow.IsMinimised() && width == windowSize.x && height == windowSize.y) {
		return;
	}
	if (width == 0 && height == 0) {
		return;
	}
	windowSize = { width, height };

	defaultScreenRect = vk::Rect2D({ 0,0 }, { (uint32_t)windowSize.x, (uint32_t)windowSize.y });

	if (vkInit.useOpenGLCoordinates) {
		defaultViewport = vk::Viewport(0.0f, (float)windowSize.y, (float)windowSize.x, (float)windowSize.y, -1.0f, 1.0f);
	}
	else {
		defaultViewport = vk::Viewport(0.0f, (float)windowSize.y, (float)windowSize.x, -(float)windowSize.y, 0.0f, 1.0f);
	}

	defaultScissor	= vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(windowSize.x, windowSize.y));

	std::cout << __FUNCTION__ << " New dimensions: " << windowSize.x << " , " << windowSize.y << "\n";

	device.waitIdle();

	depthBuffer = TextureBuilder(GetDevice(), GetMemoryAllocator())
		.UsingPool(GetCommandPool(CommandType::Graphics))
		.UsingQueue(GetQueue(CommandType::Graphics))
		.WithDimension(hostWindow.GetScreenSize().x, hostWindow.GetScreenSize().y)
		.WithAspects(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
		.WithFormat(vkInit.depthStencilFormat)
		.WithLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
		.WithUsages(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
		.WithPipeFlags(vk::PipelineStageFlagBits2::eEarlyFragmentTests)
		.WithMips(false)
		.Build("Depth Buffer");

	InitDefaultRenderPass();

	vk::UniqueCommandBuffer cmds = CmdBufferCreateBegin(device, commandPools[CommandType::Graphics], "Window resize cmds");
	InitBufferChain(*cmds);

	device.waitIdle();

	CompleteResize();

	CmdBufferEndSubmitWait(*cmds, device, queues[CommandType::Graphics]);

	device.waitIdle();
}

void VulkanRenderer::CompleteResize() {

}

void VulkanRenderer::WaitForSwapImage() {
	if (!hostWindow.IsMinimised()) {
		vk::Result waitResult = device.waitForFences(currentSwapFence, true, ~0);
	}
	TransitionUndefinedToColour(frameCmds, swapStates[currentSwap].colourImage);
}

void	VulkanRenderer::BeginFrame() {
	currentFrameContext = (currentFrameContext + 1) % frameContexts.size();

	//block on the waiting frame's timeline semaphore
	if (globalFrameID >= frameContexts.size()+1) {
		uint64_t waitValue = globalFrameID - frameContexts.size();
		//The host has to wait for the frame n-framesInFlight to have been definitely completed!
		Vulkan::TimelineSemaphoreHostWait(GetDevice(), *globalFrameSemaphore, waitValue);
	}
	////And don't let any more queue work be submitted just yet...
	if (globalFrameID > 1) {
		uint64_t waitValue = globalFrameID - 1;
		Vulkan::TimelineSemaphoreQueueWait(GetQueue(CommandType::Graphics), *globalFrameSemaphore, waitValue);
	}

	//device.waitIdle();

	//Acquire our next swap image
	currentSwapFence = swapStates[currentSwap].acquireFence;
	device.resetFences(currentSwapFence);
	currentSwap = device.acquireNextImageKHR(swapChain, UINT64_MAX, VK_NULL_HANDLE, currentSwapFence).value;	//Get swap image

	//Now we know the swap image, we can fill out our current frame state...
	//Our current frame

	frameContexts[currentFrameContext].defaultViewport		= defaultViewport;
	frameContexts[currentFrameContext].defaultScreenRect	= defaultScreenRect;

	frameContexts[currentFrameContext].colourImage		= swapStates[currentSwap].colourImage;
	frameContexts[currentFrameContext].colourView		= swapStates[currentSwap].colourView;
	frameContexts[currentFrameContext].colourFormat		= surfaceFormat;

	frameContexts[currentFrameContext].depthFormat		= depthBuffer->GetFormat();
	frameCmds = frameContexts[currentFrameContext].cmdBuffer;
	frameCmds.reset({});

	frameCmds.begin(vk::CommandBufferBeginInfo());

	if (!vkInit.skipDynamicState) {
		frameCmds.setViewport(0, 1, &defaultViewport);
		frameCmds.setScissor(0, 1, &defaultScissor);
	}

	if (vkInit.autoTransitionFrameBuffer) {
		WaitForSwapImage();
	}
	if (vkInit.autoBeginDynamicRendering) {
		BeginDefaultRendering(frameCmds);
	}
}

void VulkanRenderer::RenderFrame() {

}

void	VulkanRenderer::EndFrame() {
	if (vkInit.autoBeginDynamicRendering) {
		frameCmds.endRendering();
	}
	TransitionColourToPresent(frameCmds, frameContexts[currentFrameContext].colourImage);
	if (hostWindow.IsMinimised()) {
		//TODO - signal frameFinishedSemaphore?
		CmdBufferEndSubmitWait(frameCmds, device, queues[CommandType::Graphics]);
	}
	else {
		CmdBufferEndSubmit(frameCmds, queues[CommandType::Graphics], {}, {}, swapStates[currentSwap].frameFinishedSemaphore);
	}

	Vulkan::TimelineSemaphoreQueueSignal(queues[CommandType::Graphics], *globalFrameSemaphore, globalFrameID);

	globalFrameID++;
}

void VulkanRenderer::SwapBuffers() {
	stagingBuffers->Update();
	if (!hostWindow.IsMinimised()) {
		vk::Queue		gfxQueue	= queues[CommandType::Graphics];

		vk::Result presentResult = gfxQueue.presentKHR(
			{
				.waitSemaphoreCount = 1,
				.pWaitSemaphores	= &swapStates[currentSwap].frameFinishedSemaphore,
				.swapchainCount		= 1,
				.pSwapchains		= &swapChain,
				.pImageIndices		= &currentSwap,				
			}
		);	
	}
}

void	VulkanRenderer::InitDefaultRenderPass() {
	if (defaultRenderPass) {
		device.destroyRenderPass(defaultRenderPass);
	}
	vk::AttachmentDescription attachments[] = {
		vk::AttachmentDescription()
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFormat(surfaceFormat)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
	,
		vk::AttachmentDescription()
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
			.setFormat(depthBuffer->GetFormat())
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
	};

	vk::AttachmentReference references[] = {
		vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
		vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal)
	};

	vk::SubpassDescription subPass = vk::SubpassDescription()
		.setColorAttachmentCount(1)
		.setPColorAttachments(&references[0])
		.setPDepthStencilAttachment(&references[1])
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

	vk::RenderPassCreateInfo renderPassInfo = vk::RenderPassCreateInfo()
		.setAttachmentCount(2)
		.setPAttachments(attachments)
		.setSubpassCount(1)
		.setPSubpasses(&subPass);

	defaultRenderPass = device.createRenderPass(renderPassInfo);
}

void	VulkanRenderer::InitDefaultDescriptorPool(uint32_t maxSets) {
	vk::DescriptorPoolSize poolSizes[] = {
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, maxSets),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, maxSets),
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, maxSets),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, maxSets),		
		
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, maxSets),
		vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, maxSets),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, maxSets),

	//	vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, maxSets),
	};

	uint32_t poolCount = sizeof(poolSizes) / sizeof(vk::DescriptorPoolSize);

	vk::DescriptorPoolCreateInfo poolCreate;
	poolCreate.setPoolSizeCount(sizeof(poolSizes) / sizeof(vk::DescriptorPoolSize));
	poolCreate.setPPoolSizes(poolSizes);
	poolCreate.setMaxSets(maxSets * poolCount);
	poolCreate.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

	defaultDescriptorPool = device.createDescriptorPool(poolCreate);
}

void	VulkanRenderer::BeginDefaultRenderPass(vk::CommandBuffer  cmds) {
	cmds.beginRenderPass(defaultBeginInfo, vk::SubpassContents::eInline);
}

void	VulkanRenderer::BeginDefaultRendering(vk::CommandBuffer  cmds) {
	vk::RenderingInfoKHR renderInfo;
	renderInfo.layerCount = 1;

	vk::RenderingAttachmentInfoKHR colourAttachment;
	colourAttachment.setImageView(frameContexts[currentFrameContext].colourView)
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(vk::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f));

	vk::RenderingAttachmentInfoKHR depthAttachment;
	depthAttachment.setImageView(depthBuffer->GetDefaultView())
		.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.clearValue.setDepthStencil({ 1.0f, ~0U });

	renderInfo.setColorAttachments(colourAttachment)
		.setPDepthAttachment(&depthAttachment);
		//.setPStencilAttachment(&depthAttachment);

	renderInfo.setRenderArea(defaultScreenRect);

	cmds.beginRendering(renderInfo);
	cmds.setViewport(0, 1, &defaultViewport);
	cmds.setScissor(0, 1, &defaultScissor);
}

VkBool32 VulkanRenderer::DebugCallbackFunction(
	vk::DebugUtilsMessageSeverityFlagBitsEXT			messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT					messageTypes,
	const vk::DebugUtilsMessengerCallbackDataEXT*		pCallbackData,
	void*												pUserData) {

	return false;
}