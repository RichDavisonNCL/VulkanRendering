/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once

namespace NCL::Rendering::Vulkan {
	/*
	DescriptorBufferWriter: A helper class for writing descriptors to a 
	descriptor buffer. We MUST call Finish to ensure that the writes to the
	buffer are synchronised, in case the buffer is not persistently mapped.
	*/
	class DescriptorBufferWriter {
	public:
		DescriptorBufferWriter(vk::Device inDevice, vk::DescriptorSetLayout inLayout, VulkanBuffer& inBuffer)
		 : m_destBuffer(inBuffer) {
			m_device = inDevice;
			m_layout = inLayout;
			m_descriptorBufferMemory = m_destBuffer.Map();
		}

		~DescriptorBufferWriter() {
			if (m_descriptorBufferMemory) {
				Finish();
			}
		}

		DescriptorBufferWriter& SetProperties(vk::PhysicalDeviceDescriptorBufferPropertiesEXT* inProps) {
			m_props = inProps;
			return *this;
		}

		DescriptorBufferWriter& WriteBuffer(uint32_t binding, vk::DescriptorType type, const VulkanBuffer& buffer, uint32_t arrayIndex = 0) {
			vk::DescriptorAddressInfoEXT descriptorAddress = {
				.address	= buffer.deviceAddress,
				.range		= buffer.size
			};

			vk::DescriptorGetInfoEXT getInfo = {
				.type = type,
				.data = &descriptorAddress
			};

			vk::DeviceSize		offset = m_device.getDescriptorSetLayoutBindingOffsetEXT(m_layout, binding);

			m_device.getDescriptorEXT(&getInfo, Vulkan::GetDescriptorSize(type, *m_props), ((char*)m_descriptorBufferMemory) + offset);

			return *this;
		}

		//DescriptorBufferWriter& WriteStorageBuffer(uint32_t binding, const VulkanBuffer& buffer) {
		//	vk::DescriptorAddressInfoEXT descriptorAddress = {
		//		.address = uniformBuffer.deviceAddress,
		//		.range = uniformBuffer.size
		//	};

		//	vk::DescriptorGetInfoEXT getInfo = {
		//		.type = vk::DescriptorType::eUniformBuffer,
		//		.data = &descriptorAddress
		//	};

		//	vk::DeviceSize		offset = device.getDescriptorSetLayoutBindingOffsetEXT(layout, binding);

		//	device.getDescriptorEXT(&getInfo, props->uniformBufferDescriptorSize, ((char*)descriptorBufferMemory) + offset);

		//	return *this;
		//}

		void Finish() {
			m_destBuffer.Unmap();
			m_descriptorBufferMemory = nullptr;
		}

	protected:
		vk::Device				m_device;
		VulkanBuffer&			m_destBuffer;
		void*					m_descriptorBufferMemory;
		vk::DescriptorSetLayout m_layout;
		vk::PhysicalDeviceDescriptorBufferPropertiesEXT* m_props;
	};
};