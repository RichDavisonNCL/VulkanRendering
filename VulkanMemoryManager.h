/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "VulkanBuffers.h"
#include "VulkanTexture.h"

#include "vma/vk_mem_alloc.h"

namespace NCL::Rendering::Vulkan {
	struct BufferCreationInfo {
		vk::BufferCreateInfo	createInfo;
		vk::MemoryPropertyFlags memProperties;
		vk::BufferUsageFlags	bufferUsage;
	};

	class VulkanMemoryManager {
	public:
		virtual void CreateBuffer(BufferCreationInfo& createInfo, const std::string& debugName = "")	= 0;
		virtual void CreateStagingBuffer(size_t size, const std::string& debugName = "")				= 0;
		virtual void DiscardBuffer(VulkanBuffer& buffer)												= 0;

		virtual void CreateTexture(vk::ImageCreateInfo& createInfo, const std::string& debugName = "")	= 0;
		virtual void DiscardTexture(VulkanTexture& tex)													= 0;
	};
}