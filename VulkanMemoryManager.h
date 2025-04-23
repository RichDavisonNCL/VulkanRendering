/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "VulkanBuffer.h"
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
		virtual VulkanBuffer	CreateBuffer(const BufferCreationInfo& createInfo, const std::string& debugName = "")	= 0;
		virtual VulkanBuffer	CreateStagingBuffer(size_t size, const std::string& debugName = "")						= 0;
		virtual void			DiscardBuffer(VulkanBuffer& buffer, DiscardMode discard = DiscardMode::Deferred)		= 0;

		virtual void*			MapBuffer(const VulkanBuffer& buffer)		= 0;
		virtual void			UnmapBuffer(const VulkanBuffer& buffer)	= 0;
		virtual void			CopyData(const VulkanBuffer& buffer, void* data, size_t size, size_t offset = 0) = 0;

		virtual void			Update() = 0;

		virtual vk::Image		CreateImage(vk::ImageCreateInfo& createInfo, const std::string& debugName = "")		= 0;
		virtual void			DiscardImage(vk::Image& img, DiscardMode discard = DiscardMode::Deferred)			= 0;
	
	protected:
		VulkanBuffer	AllocateBuffer() {
			VulkanBuffer b;
			b.sourceManager = this;
			return b;
		}
	};
}