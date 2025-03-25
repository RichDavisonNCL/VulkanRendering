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

	m_vkInit = vkInitInfo;

	m_allocatorInfo		= {};

	for (uint32_t i = 0; i < CommandType::MAX_COMMAND_TYPES; ++i) {
		m_queueFamilies[i] = -1;
	}

	InitInstance();

	InitPhysicalDevice();

	InitGPUDevice();
	InitMemoryAllocator();

	m_stagingBuffers = std::make_unique<VulkanStagingBuffers>(GetDevice(), GetMemoryAllocator());

	InitCommandPools();
	InitDefaultDescriptorPool();

	OnWindowResize(window.GetScreenSize().x, window.GetScreenSize().y);
	InitFrameStates(m_vkInit.framesInFlight);

	m_pipelineCache = m_device.createPipelineCache(vk::PipelineCacheCreateInfo());

	m_frameCmds = m_frameContexts[m_currentSwap].cmdBuffer;
}

VulkanRenderer::~VulkanRenderer() {
	m_device.waitIdle();
	m_depthBuffer.reset();

	for(ChainState & c : m_swapStates) {
		m_device.destroyFramebuffer(c.frameBuffer);
		m_device.destroySemaphore(c.acquireSempaphore);
		m_device.destroyFence(c.acquireFence);
		m_device.destroyImageView(c.colourView);
	}
	m_frameContexts.clear();
	vmaDestroyAllocator(m_memoryAllocator);
	m_device.destroyDescriptorPool(m_defaultDescriptorPool);
	m_device.destroySwapchainKHR(m_swapChain);

	for (auto& c : m_commandPools) {
		if (c) {
			m_device.destroyCommandPool(c);
		}
	}
	
	m_device.destroyRenderPass(m_defaultRenderPass);
	m_device.destroyPipelineCache(m_pipelineCache);
	m_device.destroy(); //Destroy everything except instance before this gets destroyed!

	m_instance.destroySurfaceKHR(m_surface);
	m_instance.destroy();
}

bool VulkanRenderer::InitInstance() {
	vk::ApplicationInfo appInfo = {
		.pApplicationName = this->hostWindow.GetTitle().c_str(),
		.apiVersion = VK_MAKE_VERSION(m_vkInit.majorVersion, m_vkInit.minorVersion, 0)
	};
		
	vk::InstanceCreateInfo instanceInfo = {
		.flags = {},
		.pApplicationInfo		 = &appInfo,
		.enabledLayerCount		 = (uint32_t)m_vkInit.instanceLayers.size(),
		.ppEnabledLayerNames	 = m_vkInit.instanceLayers.data(),
		.enabledExtensionCount	 = (uint32_t)m_vkInit.instanceExtensions.size(),
		.ppEnabledExtensionNames = m_vkInit.instanceExtensions.data()
	};
		
	m_instance = vk::createInstance(instanceInfo);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

	return true;
}

bool	VulkanRenderer::InitPhysicalDevice() {
	auto enumResult = m_instance.enumeratePhysicalDevices();

	if (enumResult.empty()) {
		return false; //Guess there's no Vulkan capable devices?!
	}

	m_physicalDevice = enumResult[0];
	for (auto& i : enumResult) {
		if (i.getProperties().deviceType == m_vkInit.idealGPU) {
			m_physicalDevice = i;
			break;
		}
	}

	std::cout << __FUNCTION__ << " Vulkan using physical device " << m_physicalDevice.getProperties().deviceName << "\n";

	vk::PhysicalDeviceProperties2 props;
	m_physicalDevice.getProperties2(&props);

	return true;
}

bool VulkanRenderer::InitGPUDevice() {
	InitSurface();
	InitDeviceQueueIndices();

	float queuePriority = 0.0f;

	std::vector< vk::DeviceQueueCreateInfo> queueInfos;

	queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
		.setQueueCount(1)
		.setQueueFamilyIndex(m_queueFamilies[CommandType::Type::Graphics])
		.setPQueuePriorities(&queuePriority)
	);

	if (m_queueFamilies[CommandType::Type::AsyncCompute] != m_queueFamilies[CommandType::Type::Graphics]){
		queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(m_queueFamilies[CommandType::Type::AsyncCompute])
			.setPQueuePriorities(&queuePriority)
		);
	}
	if (m_queueFamilies[CommandType::Type::Copy] != m_queueFamilies[CommandType::Type::Graphics]) {
		queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(m_queueFamilies[CommandType::Copy])
			.setPQueuePriorities(&queuePriority)
		);
	}
	if (m_queueFamilies[CommandType::Type::Present] != m_queueFamilies[CommandType::Type::Graphics]) {
		queueInfos.emplace_back(vk::DeviceQueueCreateInfo()
			.setQueueCount(1)
			.setQueueFamilyIndex(m_queueFamilies[CommandType::Present])
			.setPQueuePriorities(&queuePriority)
		);
	}

	vk::PhysicalDeviceFeatures2 deviceFeatures;
	m_physicalDevice.getFeatures2(&deviceFeatures);

	if (!m_vkInit.features.empty()) {
		for (int i = 1; i < m_vkInit.features.size(); ++i) {
			vk::PhysicalDeviceFeatures2* prevStruct = (vk::PhysicalDeviceFeatures2*)m_vkInit.features[i-1];
			prevStruct->pNext = m_vkInit.features[i];
		}
		deviceFeatures.pNext = m_vkInit.features[0];
	}

	vk::DeviceCreateInfo createInfo = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount(queueInfos.size())
		.setPQueueCreateInfos(queueInfos.data());
	
	createInfo.setEnabledLayerCount((uint32_t)m_vkInit.deviceLayers.size())
		.setPpEnabledLayerNames(m_vkInit.deviceLayers.data())
		.setEnabledExtensionCount((uint32_t)m_vkInit.deviceExtensions.size())
		.setPpEnabledExtensionNames(m_vkInit.deviceExtensions.data());

	createInfo.pNext = &deviceFeatures;

	m_device = m_physicalDevice.createDevice(createInfo);

	m_queues[CommandType::Graphics]		= m_device.getQueue(m_queueFamilies[CommandType::Type::Graphics], 0);
	m_queues[CommandType::AsyncCompute]	= m_device.getQueue(m_queueFamilies[CommandType::Type::AsyncCompute], 0);
	m_queues[CommandType::Copy]			= m_device.getQueue(m_queueFamilies[CommandType::Type::Copy], 0);
	m_queues[CommandType::Present]		= m_device.getQueue(m_queueFamilies[CommandType::Type::Present], 0);

	m_deviceMemoryProperties = m_physicalDevice.getMemoryProperties();
	m_deviceProperties = m_physicalDevice.getProperties();

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

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

	m_surface = m_instance.createWin32SurfaceKHR(
		{
			.flags = {},
			.hinstance = window->GetInstance(),
			.hwnd = window->GetHandle()
		}
	);
#endif

	auto formats = m_physicalDevice.getSurfaceFormatsKHR(m_surface);

	if (formats.size() == 1 && formats[0].format == vk::Format::eUndefined) {
		m_surfaceFormat	= vk::Format::eB8G8R8A8Unorm;
		m_surfaceSpace	= formats[0].colorSpace;
	}
	else {
		m_surfaceFormat	= formats[0].format;
		m_surfaceSpace	= formats[0].colorSpace;
	}

	return formats.size() > 0;
}

void	VulkanRenderer::InitFrameStates(uint32_t m_framesInFlight) {
	m_frameContexts.resize(m_framesInFlight);

	auto buffers = m_device.allocateCommandBuffers(
		{
			.commandPool = m_commandPools[CommandType::Graphics],
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = m_framesInFlight
		}
	);

	uint32_t index = 0;
	for (FrameContext& context : m_frameContexts) {
		context.device				= m_device;
		context.descriptorPool		= m_defaultDescriptorPool;

		context.cmdBuffer			= buffers[index];

		Vulkan::SetDebugName(m_device, vk::ObjectType::eCommandBuffer, Vulkan::GetVulkanHandle(context.cmdBuffer), "Context Cmds " + std::to_string(index));

		context.workSempaphore		= Vulkan::CreateTimelineSemaphore(GetDevice());

		context.viewport			= m_defaultViewport;
		context.screenRect			= m_defaultScreenRect;

		context.colourFormat		= m_surfaceFormat;

		context.depthImage			= m_depthBuffer->GetImage();
		context.depthView			= m_depthBuffer->GetDefaultView();
		context.depthFormat			= m_depthBuffer->GetFormat();

		for (int i = 0; i < CommandType::Type::MAX_COMMAND_TYPES; ++i) {
			context.commandPools[i]		= m_commandPools[i];
			context.queues[i]			= m_queues[i];
			context.queueFamilies[i]	= m_queueFamilies[i];
		}
		index++;
	}
}

void VulkanRenderer::InitBufferChain(vk::CommandBuffer  cmdBuffer) {
	vk::SwapchainKHR oldChain					= m_swapChain;

	vk::SurfaceCapabilitiesKHR surfaceCaps = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);

	vk::Extent2D swapExtents = vk::Extent2D((int)hostWindow.GetScreenSize().x, (int)hostWindow.GetScreenSize().y);

	auto presentModes = m_physicalDevice.getSurfacePresentModesKHR(m_surface); //Type is of vector of PresentModeKHR

	vk::PresentModeKHR currentPresentMode	= vk::PresentModeKHR::eFifo;

	for (const auto& i : presentModes) {
		if (i == m_vkInit.idealPresentMode) {
			currentPresentMode = i;
			break;
		}
	}

	if (currentPresentMode != m_vkInit.idealPresentMode) {
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
		.setSurface(m_surface)
		.setImageColorSpace(m_surfaceSpace)
		.setImageFormat(m_surfaceFormat)
		.setImageExtent(swapExtents)
		.setMinImageCount(idealImageCount)
		.setOldSwapchain(oldChain)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

	m_swapChain = m_device.createSwapchainKHR(swapInfo);

	if (!m_swapStates.empty()) {
		for (unsigned int i = 0; i < m_swapStates.size(); ++i) {
			m_device.destroyImageView(m_swapStates[i].colourView);
			m_device.destroyFramebuffer(m_swapStates[i].frameBuffer);
		}
	}
	if (oldChain) {
		m_device.destroySwapchainKHR(oldChain);
	}

	auto images = m_device.getSwapchainImagesKHR(m_swapChain);

	m_swapStates.resize(images.size());

	for(int i = 0; i < m_swapStates.size(); ++i) {
		ChainState& chain = m_swapStates[i];

		chain.acquireSempaphore = m_device.createSemaphore({});
		chain.acquireFence		= m_device.createFence({});

		chain.swapCmds = CmdBufferCreate(m_device, GetCommandPool(CommandType::Graphics), "Window swap cmds").release();

		chain.colourImage = images[i];

		ImageTransitionBarrier(cmdBuffer, chain.colourImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput);

		chain.colourView = m_device.createImageView(
			vk::ImageViewCreateInfo()
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
			.setFormat(m_surfaceFormat)
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
			.setRenderPass(m_defaultRenderPass);

		attachments[0] = chain.colourView;
		attachments[1] = *m_depthBuffer->m_defaultView;
		chain.frameBuffer = m_device.createFramebuffer(createInfo);
	}
	m_currentSwap = 0; //??
}

void	VulkanRenderer::InitCommandPools() {	
	for (uint32_t i = 0; i < CommandType::MAX_COMMAND_TYPES; ++i) {
		m_commandPools[i] = m_device.createCommandPool(
			{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = m_queueFamilies[i]
			}
		);
	}
}

void	VulkanRenderer::InitMemoryAllocator() {
	VmaVulkanFunctions funcs = {};
	funcs.vkGetInstanceProcAddr = ::vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr;
	funcs.vkGetDeviceProcAddr   = ::vk::detail::defaultDispatchLoaderDynamic.vkGetDeviceProcAddr;

	m_allocatorInfo.physicalDevice = m_physicalDevice;
	m_allocatorInfo.device	= m_device;
	m_allocatorInfo.instance	= m_instance;

	m_allocatorInfo.flags |= m_vkInit.vmaFlags;

	m_allocatorInfo.pVulkanFunctions = &funcs;
	vmaCreateAllocator(&m_allocatorInfo, &m_memoryAllocator);
}

bool VulkanRenderer::InitDeviceQueueIndices() {
	std::vector<vk::QueueFamilyProperties> deviceQueueProps = m_physicalDevice.getQueueFamilyProperties();

	VkBool32 supportsPresent = false;

	int gfxBits		= INT_MAX;
	int computeBits = INT_MAX;
	int copyBits	= INT_MAX;

	for (unsigned int i = 0; i < deviceQueueProps.size(); ++i) {
		supportsPresent = m_physicalDevice.getSurfaceSupportKHR(i, m_surface);

		int queueBitCount = std::popcount((uint32_t)deviceQueueProps[i].queueFlags);

		if (deviceQueueProps[i].queueFlags & vk::QueueFlagBits::eGraphics && queueBitCount < gfxBits) {
			m_queueFamilies[CommandType::Graphics] = i;
			gfxBits = queueBitCount;
			if (supportsPresent && m_queueFamilies[CommandType::Present] == -1) {
				m_queueFamilies[CommandType::Present] = i;
			}
		}

		if (deviceQueueProps[i].queueFlags & vk::QueueFlagBits::eCompute && queueBitCount < computeBits) {
			m_queueFamilies[CommandType::AsyncCompute] = i;
			computeBits = queueBitCount;
		}

		if (deviceQueueProps[i].queueFlags & vk::QueueFlagBits::eTransfer && queueBitCount < copyBits) {
			m_queueFamilies[CommandType::Copy] = i;
			copyBits = queueBitCount;
		}
	}

	if (m_queueFamilies[CommandType::Graphics] == -1) {
		return false;
	}

	if (m_queueFamilies[CommandType::AsyncCompute] == -1) {
		m_queueFamilies[CommandType::AsyncCompute] = m_queueFamilies[CommandType::Graphics];
	}
	else if(m_queueFamilies[CommandType::AsyncCompute] != m_queueFamilies[CommandType::Graphics]) {
		std::cout << __FUNCTION__ << " Device supports async compute!\n";
	}
	else {
		std::cout << __FUNCTION__ << " Device does NOT support async compute!\n";
	}

	if (m_queueFamilies[CommandType::Copy] == -1) {
		m_queueFamilies[CommandType::Copy] = m_queueFamilies[CommandType::Copy];
	}
	else if(m_queueFamilies[CommandType::Copy] != m_queueFamilies[CommandType::Graphics]){
		std::cout << __FUNCTION__ << " Device supports async copy!\n";
	}
	else {
		std::cout << __FUNCTION__ << " Device does NOT support async copy!\n";
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

	m_defaultScreenRect = vk::Rect2D({ 0,0 }, { (uint32_t)windowSize.x, (uint32_t)windowSize.y });

	if (m_vkInit.useOpenGLCoordinates) {
		m_defaultViewport = vk::Viewport(0.0f, (float)windowSize.y, (float)windowSize.x, (float)windowSize.y, -1.0f, 1.0f);
	}
	else {
		m_defaultViewport = vk::Viewport(0.0f, (float)windowSize.y, (float)windowSize.x, -(float)windowSize.y, 0.0f, 1.0f);
	}

	m_defaultScissor	= vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(windowSize.x, windowSize.y));

	std::cout << __FUNCTION__ << " New dimensions: " << windowSize.x << " , " << windowSize.y << "\n";

	m_device.waitIdle();

	m_depthBuffer = TextureBuilder(GetDevice(), GetMemoryAllocator())
		.UsingPool(GetCommandPool(CommandType::Graphics))
		.UsingQueue(GetQueue(CommandType::Graphics))
		.WithDimension(hostWindow.GetScreenSize().x, hostWindow.GetScreenSize().y)
		.WithAspects(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
		.WithFormat(m_vkInit.depthStencilFormat)
		.WithLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
		.WithUsages(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
		.WithPipeFlags(vk::PipelineStageFlagBits2::eEarlyFragmentTests)
		.WithMips(false)
		.Build("Depth Buffer");

	InitDefaultRenderPass();

	vk::UniqueCommandBuffer cmds = CmdBufferCreateBegin(m_device, m_commandPools[CommandType::Graphics], "Window resize cmds");
	InitBufferChain(*cmds);

	m_device.waitIdle();

	CompleteResize();

	CmdBufferEndSubmitWait(*cmds, m_device, m_queues[CommandType::Graphics]);

	m_device.waitIdle();
}

void VulkanRenderer::CompleteResize() {

}

void VulkanRenderer::WaitForSwapImage() {
	if (!hostWindow.IsMinimised()) {
		vk::Result waitResult = m_device.waitForFences(m_currentSwapFence, true, ~0);
	}
	TransitionUndefinedToColour(m_frameCmds, m_swapStates[m_currentSwap].colourImage);
}

void	VulkanRenderer::BeginFrame() {
	//First we need to prevent the m_renderer from going too far ahead of the frames in flight max
	m_currentFrameContext = (m_currentFrameContext + 1) % m_frameContexts.size();

	//block on the waiting frame's timeline semaphore
	if (m_globalFrameID >= m_frameContexts.size()) {
		uint64_t waitValue = m_globalFrameID;

		vk::SemaphoreWaitInfo waitInfo;
		waitInfo.pSemaphores = &*m_frameContexts[m_currentFrameContext].workSempaphore;
		waitInfo.semaphoreCount = 1;
		waitInfo.pValues = &waitValue;

		m_device.waitSemaphores(waitInfo, UINT64_MAX);
	}

	//Acquire our next swap image
	m_currentSwapFence = m_swapStates[m_currentSwap].acquireFence;
	m_device.resetFences(m_currentSwapFence);
	m_currentSwap = m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, VK_NULL_HANDLE, m_currentSwapFence).value;	//Get swap image

	//Now we know the swap image, we can fill out our current frame state...
	//Our current frame

	m_frameContexts[m_currentFrameContext].frameID = m_globalFrameID;
	m_frameContexts[m_currentFrameContext].cycleID = m_currentFrameContext;


	m_frameContexts[m_currentFrameContext].viewport			= m_defaultViewport;
	m_frameContexts[m_currentFrameContext].screenRect		= m_defaultScreenRect;

	m_frameContexts[m_currentFrameContext].colourImage		= m_swapStates[m_currentSwap].colourImage;
	m_frameContexts[m_currentFrameContext].colourView		= m_swapStates[m_currentSwap].colourView;
	m_frameContexts[m_currentFrameContext].colourFormat		= m_surfaceFormat;

	m_frameContexts[m_currentFrameContext].depthFormat		= m_depthBuffer->GetFormat();
	m_frameCmds = m_frameContexts[m_currentFrameContext].cmdBuffer;
	m_frameCmds.reset({});

	m_frameCmds.begin(vk::CommandBufferBeginInfo());

	if (!m_vkInit.skipDynamicState) {
		m_frameCmds.setViewport(0, 1, &m_defaultViewport);
		m_frameCmds.setScissor( 0, 1, &m_defaultScissor);
	}

	if (m_vkInit.autoTransitionFrameBuffer) {
		WaitForSwapImage();
	}
	if (m_vkInit.autoBeginDynamicRendering) {
		BeginDefaultRendering(m_frameCmds);
	}
}

void VulkanRenderer::RenderFrame() {

}

void	VulkanRenderer::EndFrame() {
	if (m_vkInit.autoBeginDynamicRendering) {
		m_frameCmds.endRendering();
	}
	TransitionColourToPresent(m_frameCmds, m_frameContexts[m_currentFrameContext].colourImage);
	if (hostWindow.IsMinimised()) {
		CmdBufferEndSubmitWait(m_frameCmds, m_device, m_queues[CommandType::Graphics]);
	}
	else {
		CmdBufferEndSubmit(m_frameCmds, m_queues[CommandType::Graphics]);
	}

	uint64_t signalValue = m_globalFrameID + (m_frameContexts.size());
	vk::TimelineSemaphoreSubmitInfo tlSubmit;
	tlSubmit.signalSemaphoreValueCount = 1;
	tlSubmit.pSignalSemaphoreValues = &signalValue;

	vk::SubmitInfo queueSubmit;
	queueSubmit.pNext = &tlSubmit;
	queueSubmit.signalSemaphoreCount = 1;
	queueSubmit.pSignalSemaphores = &*m_frameContexts[m_currentFrameContext].workSempaphore;

	m_queues[CommandType::Graphics].submit(queueSubmit);

	m_globalFrameID++;
}

void VulkanRenderer::SwapBuffers() {
	m_stagingBuffers->Update();
	if (!hostWindow.IsMinimised()) {
		vk::Queue	gfxQueue		= m_queues[CommandType::Graphics];
		vk::Result	presentResult	= gfxQueue.presentKHR(
			{
				.swapchainCount = 1,
				.pSwapchains	= &m_swapChain,
				.pImageIndices	= &m_currentSwap
			}
		);	
	}
}

void	VulkanRenderer::InitDefaultRenderPass() {
	if (m_defaultRenderPass) {
		m_device.destroyRenderPass(m_defaultRenderPass);
	}
	vk::AttachmentDescription attachments[] = {
		vk::AttachmentDescription()
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFormat(m_surfaceFormat)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
	,
		vk::AttachmentDescription()
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
			.setFormat(m_depthBuffer->GetFormat())
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
	};

	vk::AttachmentReference references[] = {
		vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
		vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal)
	};

	vk::SubpassDescription m_subPass = vk::SubpassDescription()
		.setColorAttachmentCount(1)
		.setPColorAttachments(&references[0])
		.setPDepthStencilAttachment(&references[1])
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

	vk::RenderPassCreateInfo renderPassInfo = vk::RenderPassCreateInfo()
		.setAttachmentCount(2)
		.setPAttachments(attachments)
		.setSubpassCount(1)
		.setPSubpasses(&m_subPass);

	m_defaultRenderPass = m_device.createRenderPass(renderPassInfo);
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

	m_defaultDescriptorPool = m_device.createDescriptorPool(poolCreate);
}

void	VulkanRenderer::BeginDefaultRenderPass(vk::CommandBuffer  cmds) {
	cmds.beginRenderPass(m_defaultBeginInfo, vk::SubpassContents::eInline);
}

void	VulkanRenderer::BeginDefaultRendering(vk::CommandBuffer  cmds) {
	vk::RenderingInfoKHR renderInfo;
	renderInfo.layerCount = 1;

	vk::RenderingAttachmentInfoKHR colourAttachment;
	colourAttachment.setImageView(m_frameContexts[m_currentFrameContext].colourView)
		.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(vk::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f));

	vk::RenderingAttachmentInfoKHR depthAttachment;
	depthAttachment.setImageView(m_depthBuffer->GetDefaultView())
		.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.clearValue.setDepthStencil({ 1.0f, ~0U });

	renderInfo.setColorAttachments(colourAttachment)
		.setPDepthAttachment(&depthAttachment);
		//.setPStencilAttachment(&depthAttachment);

	renderInfo.setRenderArea(m_defaultScreenRect);

	cmds.beginRendering(renderInfo);
	cmds.setViewport(0, 1, &m_defaultViewport);
	cmds.setScissor( 0, 1, &m_defaultScissor);
}

void VulkanRenderer::WaitForGPUIdle() {
	m_device.waitIdle();
}

VkBool32 VulkanRenderer::DebugCallbackFunction(
	vk::DebugUtilsMessageSeverityFlagBitsEXT			messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT					messageTypes,
	const vk::DebugUtilsMessengerCallbackDataEXT*		pCallbackData,
	void*												pUserData) {

	return false;
}