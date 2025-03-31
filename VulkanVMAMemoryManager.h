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

	class VulkanVMAMemoryManager : public VulkanMemoryManager {
	public:
		VulkanVMAMemoryManager();
		~VulkanVMAMemoryManager();

		void CreateBuffer(BufferCreationInfo& createInfo, const std::string& debugName = "")	override;
		void CreateStagingBuffer(size_t size, const std::string& debugName = "")				override;
		void DiscardBuffer(VulkanBuffer& buffer)												override;

		void CreateTexture(vk::ImageCreateInfo& createInfo, const std::string& debugName = "")	override;
		void DiscardTexture(VulkanTexture& tex)													override;

	protected:
		struct DeferredBufferDeletion {
			VulkanBuffer	buffer;
			uint32_t		framesCount;
		};
		std::vector<DeferredBufferDeletion> m_deferredDeleteBuffers;

		VmaAllocator		m_memoryAllocator;
	};
}