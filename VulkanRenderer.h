/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../NCLCoreClasses/RendererBase.h"
#include "../NCLCoreClasses/Maths.h"

#include "VulkanPipeline.h"
#include "VulkanStagingBuffers.h"
#include "SmartTypes.h"
#include "vma/vk_mem_alloc.h"
using std::string;

namespace NCL::Rendering::Vulkan {
	class VulkanMesh;
	class VulkanShader;
	class VulkanCompute;
	class VulkanTexture;
	struct VulkanBuffer;

	namespace CommandType {
		enum Type : uint32_t {
			Graphics,
			AsyncCompute,
			Copy,
			Present,
			MAX_COMMAND_TYPES
		};
	};
	//Some auto-generated descriptor set layouts for quick prototyping
	struct DefaultSetLayouts {
		enum Type : uint32_t {
			Single_Texture,
			Single_UBO,
			Single_SSBO,
			Single_Storage_Image,
			Single_TLAS,
			MAX_SIZE
		};
	};

	struct FrameContext {
		vk::Device			device;
		vk::DescriptorPool  descriptorPool;
		vk::CommandBuffer	cmdBuffer;

		vk::UniqueSemaphore	workSempaphore;

		vk::Image			colourImage;
		vk::ImageView		colourView;
		vk::Format			colourFormat;

		vk::Image			depthImage;
		vk::ImageView		depthView;
		vk::Format			depthFormat;

		vk::Viewport		defaultViewport;
		vk::Rect2D			defaultScreenRect;

		vk::CommandPool			commandPools[CommandType::Type::MAX_COMMAND_TYPES];
		vk::Queue				queues[CommandType::Type::MAX_COMMAND_TYPES];
	};

	struct ChainState {
		vk::Framebuffer		frameBuffer;
		vk::Image			colourImage;
		vk::ImageView		colourView;
		vk::Format			colourFormat;
		vk::Semaphore		acquireSempaphore;
		vk::Fence			acquireFence;
	};

	struct VulkanInitialisation {
		vk::Format			depthStencilFormat	= vk::Format::eD32SfloatS8Uint;
		vk::PresentModeKHR  idealPresentMode	= vk::PresentModeKHR::eFifo;

		vk::PhysicalDeviceType idealGPU		= vk::PhysicalDeviceType::eDiscreteGpu;

		int majorVersion = 1;
		int minorVersion = 1;

		std::vector<void*> features;

		VmaAllocatorCreateFlags vmaFlags = {};

		std::vector<const char*>	instanceExtensions;
		std::vector<const char*>	instanceLayers;

		std::vector<const char*> deviceExtensions;
		std::vector<const char*> deviceLayers;

		bool				autoTransitionFrameBuffer = true;
		bool				autoBeginDynamicRendering = true;
		bool				useOpenGLCoordinates = false;
		bool				skipDynamicState = false;
	};

	class VulkanRenderer : public RendererBase {
		friend class VulkanMesh;
		friend class VulkanTexture;
	public:
		VulkanRenderer(Window& window, const VulkanInitialisation& vkInit);
		~VulkanRenderer();

		virtual bool HasInitialised() const { return device; }

		vk::Device GetDevice() const {
			return device;
		}

		vk::PhysicalDevice GetPhysicalDevice() const {
			return gpu;
		}

		vk::Instance	GetVulkanInstance() const {
			return instance;
		}

		VmaAllocator GetMemoryAllocator() const {
			return memoryAllocator;
		}

		vk::Queue GetQueue(CommandType::Type type) const {
			return queues[type];
		}

		uint32_t GetQueueFamily(CommandType::Type type) const {
			return queueFamilies[type];
		}

		vk::CommandPool GetCommandPool(CommandType::Type type) const {
			return commandPools[type];
		}

		vk::DescriptorPool GetDescriptorPool() {
			return defaultDescriptorPool;
		}

		FrameContext const& GetFrameContext() const {
			return frameContexts[currentFrameContext];
		}

		UniqueVulkanTexture const & GetDepthBuffer() const {
			return depthBuffer;
		}

		VulkanStagingBuffers& GetStagingBuffers() {
			return *stagingBuffers;
		}

		void	BeginDefaultRenderPass(vk::CommandBuffer cmds);
		void	BeginDefaultRendering(vk::CommandBuffer  cmds);

		void BeginFrame()		override;
		void RenderFrame()		override;
		void EndFrame()			override;
		void SwapBuffers()		override;

		void OnWindowResize(int w, int h)	override;
	protected:

		virtual void	CompleteResize();
		virtual void	InitDefaultRenderPass();
		virtual void	InitDefaultDescriptorPool(uint32_t maxSets = 128);

		virtual void WaitForSwapImage();

	protected:	
		vk::Viewport			defaultViewport;
		vk::Rect2D				defaultScissor;	
		vk::Rect2D				defaultScreenRect;			
		vk::RenderPass			defaultRenderPass;
		vk::RenderPassBeginInfo defaultBeginInfo;
		
		vk::DescriptorPool		defaultDescriptorPool;	//descriptor sets come from here!

		vk::CommandPool			commandPools[CommandType::Type::MAX_COMMAND_TYPES];
		vk::Queue				queues[CommandType::Type::MAX_COMMAND_TYPES];
		uint32_t				queueFamilies[CommandType::Type::MAX_COMMAND_TYPES];


		std::unique_ptr<VulkanStagingBuffers>  stagingBuffers;

		vk::CommandBuffer		frameCmds;

		UniqueVulkanTexture depthBuffer;

		VmaAllocatorCreateInfo	allocatorInfo;

		VulkanInitialisation vkInit;
	private: 
		void	InitCommandPools();
		bool	InitInstance();
		bool	InitPhysicalDevice();
		bool	InitGPUDevice();
		bool	InitSurface();
		void	InitMemoryAllocator();
		void	InitBufferChain(vk::CommandBuffer  cmdBuffer);

		void	InitFrameStates(uint32_t framesInFlight);

		static VkBool32 DebugCallbackFunction(
			vk::DebugUtilsMessageSeverityFlagBitsEXT			messageSeverity,
			vk::DebugUtilsMessageTypeFlagsEXT					messageTypes,
			const vk::DebugUtilsMessengerCallbackDataEXT*		pCallbackData,
			void*												pUserData);

		bool	InitDeviceQueueIndices();

		vk::Instance		instance;	//API Instance
		vk::PhysicalDevice	gpu;		//GPU in use

		vk::PhysicalDeviceProperties		deviceProperties;
		vk::PhysicalDeviceMemoryProperties	deviceMemoryProperties;

		vk::PipelineCache		pipelineCache;
		vk::Device				device;		//Device handle	

		vk::SurfaceKHR		surface;
		vk::Format			surfaceFormat;
		vk::ColorSpaceKHR	surfaceSpace;

		vk::DebugUtilsMessengerEXT debugMessenger;

		std::vector<FrameContext>	frameContexts;
		std::vector<ChainState>		swapStates;

		uint32_t				waitFrameContext	= 0; //We need to wait for this to complete at start of frame
		uint32_t				currentFrameContext	= 0; //Frame context for this frame
		uint32_t				currentSwap			= 0; //To index our swapchain 

		uint64_t				globalFrameID		= 0;

		vk::Fence			currentSwapFence;

		vk::SwapchainKHR	swapChain;
		VmaAllocator		memoryAllocator;

	
	};
}