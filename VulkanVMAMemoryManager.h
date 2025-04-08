/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once

#include "VulkanMemoryManager.h"
#include "VulkanBuffers.h"
#include "VulkanTexture.h"

#include "vma/vk_mem_alloc.h"

namespace NCL::Rendering::Vulkan {
	struct VulkanInitialisation;
	class VMABuffer;
	class VulkanVMAMemoryManager;

	struct VMABuffer : public VulkanBuffer {
		friend class VulkanVMAMemoryManager;

		void*	Map()		const override;
		void	Unmap()		const override;
		void*	Data()		const override;
		void	CopyData(void* data, size_t size) const override;
	protected:
		VMABuffer() {}
		VMABuffer(VulkanVMAMemoryManager& manager);
		VMABuffer(VMABuffer&& obj);
		~VMABuffer() {
			Destroy();
		}
		VMABuffer& operator=(VMABuffer&& obj);

		void Destroy() {
			if (m_allocator) {
				vmaDestroyBuffer(m_allocator, buffer, m_allocationHandle);

				m_allocator = 0;
				m_allocationHandle = 0;
				buffer = VK_NULL_HANDLE;
			}
		}

		VmaAllocation			m_allocationHandle = {};
		VmaAllocationInfo		m_allocationInfo = {};
		VmaAllocator			m_allocator = {};
	};

	class VulkanVMAMemoryManager : public VulkanMemoryManager {
	public:
		VulkanVMAMemoryManager(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Instance instance, const VulkanInitialisation& vkInit);
		virtual ~VulkanVMAMemoryManager();

		UniqueVulkanBuffer	CreateBuffer(const BufferCreationInfo& createInfo, const std::string& debugName = "")	override;
		UniqueVulkanBuffer	CreateStagingBuffer(size_t size, const std::string& debugName = "")						override;
		void				DiscardBuffer(VulkanBuffer& buffer, DiscardMode discard)								override;

		//void*			MapBuffer(VulkanBuffer& buffer)			override;
		//void			UnmapBuffer(VulkanBuffer& buffer)		override;

		vk::Image		CreateImage(vk::ImageCreateInfo& createInfo, const std::string& debugName = "")		override;
		void			DiscardImage(vk::Image& tex, DiscardMode discard)									override;

		void			Update() override;

	protected:
		struct DeferredBufferDeletion {
			VMABuffer		buffer;
			uint32_t		framesCount;
		};

		struct Allocation {
			VmaAllocation			m_allocationHandle = {};
			VmaAllocationInfo		m_allocationInfo = {};
		};

		std::map<vk::Image, Allocation> m_imageAllocations;

		std::vector<DeferredBufferDeletion> m_deferredDeleteBuffers;

		VmaAllocator			m_memoryAllocator;
		VmaAllocatorCreateInfo	m_allocatorInfo;

		uint32_t m_framesInFlight;
	};


}