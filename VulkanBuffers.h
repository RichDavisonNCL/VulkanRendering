/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Buffer.h"
namespace NCL::Rendering::Vulkan {
	class VulkanMemoryManager;
	//A buffer, backed by memory we have allocated elsewhere
	class VulkanBuffer : Buffer {
	public:
		VulkanBuffer(VulkanBuffer&& obj);

		VulkanBuffer& operator=(VulkanBuffer&& obj);

		virtual ~VulkanBuffer();

		vk::Buffer	buffer;
		size_t		size = 0;

		void* mappedPtr = nullptr;

		vk::DeviceAddress	deviceAddress;

		//A convenience func to help get around vma holding various
		//mapped pointers etc, so us calling mapBuffer can cause
		//validation errors
		virtual void	CopyData(void* data, size_t size) const;

		virtual void*	Data()		const;

		virtual void*	Map()		const;
		virtual void	Unmap()		const;

		template<typename T>
		T* Map() const {
			void* data = Map();
			return static_cast<T*>(data);
		}

		//Convenience function so we can use this struct in place of a vkBuffer when necessary
		operator vk::Buffer() const {
			return buffer;
		}

	protected:
		VulkanBuffer();
		VulkanMemoryManager* sourceManager = nullptr;
	};
};