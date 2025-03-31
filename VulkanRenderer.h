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
		
		vk::CommandPool		commandPools[CommandType::Type::MAX_COMMAND_TYPES];
		vk::Queue			queues[CommandType::Type::MAX_COMMAND_TYPES];
		uint32_t			queueFamilies[CommandType::Type::MAX_COMMAND_TYPES];

		vk::UniqueSemaphore	workSempaphore;

		vk::Image			colourImage;
		vk::ImageView		colourView;
		vk::Format			colourFormat;

		vk::Image			depthImage;
		vk::ImageView		depthView;
		vk::Format			depthFormat;

		vk::Viewport		viewport;
		vk::Rect2D			screenRect;
	};

	struct ChainState {
		vk::Framebuffer		frameBuffer;
		vk::Image			colourImage;
		vk::ImageView		colourView;
		vk::Format			colourFormat;
		vk::Semaphore		acquireSempaphore;
		vk::Fence			acquireFence;

		vk::CommandBuffer	swapCmds;
	};

	struct VulkanInitialisation {
		vk::Format			depthStencilFormat	= vk::Format::eD32SfloatS8Uint;
		vk::PresentModeKHR  idealPresentMode	= vk::PresentModeKHR::eFifo;

		vk::PhysicalDeviceType idealGPU		= vk::PhysicalDeviceType::eDiscreteGpu;

		int majorVersion = 1;
		int minorVersion = 1;

		uint32_t	framesInFlight = 3;

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

		virtual bool HasInitialised() const { return m_device; }

		vk::Device GetDevice() const {
			return m_device;
		}

		vk::PhysicalDevice GetPhysicalDevice() const {
			return m_physicalDevice;
		}

		vk::Instance	GetVulkanInstance() const {
			return m_instance;
		}

		VmaAllocator GetMemoryAllocator() const {
			return m_memoryAllocator;
		}

		vk::Queue GetQueue(CommandType::Type type) const {
			return m_queues[type];
		}

		uint32_t GetQueueFamily(CommandType::Type type) const {
			return m_queueFamilies[type];
		}

		vk::CommandPool GetCommandPool(CommandType::Type type) const {
			return m_commandPools[type];
		}

		vk::DescriptorPool GetDescriptorPool() {
			return m_defaultDescriptorPool;
		}

		FrameContext const& GetFrameContext() const {
			return m_frameContexts[m_currentFrameContext];
		}

		UniqueVulkanTexture const & GetDepthBuffer() const {
			return m_depthBuffer;
		}

		void WaitForGPUIdle();

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
		vk::Viewport			m_defaultViewport;
		vk::Rect2D				m_defaultScissor;	
		vk::Rect2D				m_defaultScreenRect;			
		vk::RenderPass			m_defaultRenderPass;
		vk::RenderPassBeginInfo m_defaultBeginInfo;
		
		vk::DescriptorPool		m_defaultDescriptorPool;	//descriptor sets come from here!

		vk::CommandPool			m_commandPools[CommandType::Type::MAX_COMMAND_TYPES];
		vk::Queue				m_queues[CommandType::Type::MAX_COMMAND_TYPES];
		uint32_t				m_queueFamilies[CommandType::Type::MAX_COMMAND_TYPES];


		vk::CommandBuffer		m_frameCmds;
		//vk::CommandBuffer		swapCmds;

		UniqueVulkanTexture		m_depthBuffer;

		VmaAllocatorCreateInfo	m_allocatorInfo;

		VulkanInitialisation m_vkInit;
	private: 
		void	InitCommandPools();
		bool	InitInstance();
		bool	InitPhysicalDevice();
		bool	InitGPUDevice();
		bool	InitSurface();
		void	InitMemoryAllocator();
		void	InitBufferChain(vk::CommandBuffer  m_cmdBuffer);

		void	InitFrameStates(uint32_t framesInFlight);

		static VkBool32 DebugCallbackFunction(
			vk::DebugUtilsMessageSeverityFlagBitsEXT			messageSeverity,
			vk::DebugUtilsMessageTypeFlagsEXT					messageTypes,
			const vk::DebugUtilsMessengerCallbackDataEXT*		pCallbackData,
			void*												pUserData);

		bool	InitDeviceQueueIndices();

		vk::Instance		m_instance;			//API Instance
		vk::PhysicalDevice	m_physicalDevice;	//GPU in use

		vk::PhysicalDeviceProperties		m_deviceProperties;
		vk::PhysicalDeviceMemoryProperties	m_deviceMemoryProperties;

		vk::PipelineCache		m_pipelineCache;
		vk::Device				m_device;		//Device handle	

		vk::SurfaceKHR		m_surface;
		vk::Format			m_surfaceFormat;
		vk::ColorSpaceKHR	m_surfaceSpace;

		vk::DebugUtilsMessengerEXT m_debugMessenger;

		std::vector<FrameContext>	m_frameContexts;
		std::vector<ChainState>		m_swapStates;

		uint32_t				m_currentFrameContext	= 0; //Frame context for this frame
		uint32_t				m_currentSwap			= 0; //To index our swapchain 

		uint64_t				m_globalFrameID		= 0;

		vk::Fence			m_currentSwapFence;

		vk::SwapchainKHR	m_swapChain;


	//Buffer Management
		VulkanBufferManager* m_bufferManager;

		VmaAllocator		m_memoryAllocator;
	};
}