/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "VulkanBuffers.h"
#include "VulkanTexture.h"

namespace NCL::Rendering::Vulkan {
	struct BufferCreationInfo {
		vk::BufferCreateInfo	createInfo = {
			.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer
		};
		vk::MemoryPropertyFlags memProperties	= vk::MemoryPropertyFlagBits::eDeviceLocal;
	};

	enum class DiscardMode {
		Immediate,
		Deferred
	};

	class VulkanMemoryManager {
	public:
		virtual ~VulkanMemoryManager() {};
		virtual UniqueVulkanBuffer	CreateBuffer(const BufferCreationInfo& createInfo, const std::string& debugName = "")	= 0;
		virtual UniqueVulkanBuffer	CreateStagingBuffer(size_t size, const std::string& debugName = "")						= 0;
		virtual void				DiscardBuffer(VulkanBuffer& buffer, DiscardMode discard = DiscardMode::Deferred)		= 0;

		//virtual void*			MapBuffer(VulkanBuffer& buffer)		= 0;
		//virtual void			UnmapBuffer(VulkanBuffer& buffer)	= 0;

		virtual void			Update() = 0;

		virtual vk::Image		CreateImage(vk::ImageCreateInfo& createInfo, const std::string& debugName = "")		= 0;
		virtual void			DiscardImage(vk::Image& img, DiscardMode discard = DiscardMode::Deferred)			= 0;
	};
}