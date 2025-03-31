/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanBasicBufferManager.h"
#include "VulkanTexture.h"
#include "Vulkanrenderer.h"
#include "VulkanUtils.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanBasicBufferManager::VulkanBasicBufferManager() {

}

VulkanBasicBufferManager::~VulkanBasicBufferManager() {

}

void VulkanBasicBufferManager::CreateBuffer(BufferCreationInfo& createInfo, const std::string& debugName) {

}

void VulkanBasicBufferManager::CreateStagingBuffer(size_t size, const std::string& debugName) {

}

void VulkanBasicBufferManager::DiscardBuffer(VulkanBuffer& buffer) {

}

///*
//
//Buffer Management
//
//*/
//
//VulkanBuffer	VulkanRenderer::CreateBuffer(BufferCreationInfo& createInfo, const std::string& debugName) {
//	VulkanBuffer	outputBuffer;
//
//	size_t allocSize = createInfo.createInfo.size;
//
//	outputBuffer.size = allocSize;
//
//	outputBuffer.allocator = m_memoryAllocator;
//
//	VmaAllocationCreateInfo alloCreateInfo;
//	alloCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
//
//	alloCreateInfo.requiredFlags = (VkMemoryPropertyFlags)createInfo.memProperties;
//
//	if (createInfo.memProperties & vk::MemoryPropertyFlagBits::eHostVisible) {
//		alloCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
//	}
//
//	if (createInfo.memProperties & vk::MemoryPropertyFlagBits::eHostCoherent) {
//		alloCreateInfo.flags |= (VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
//	}
//
//	vmaCreateBuffer(m_memoryAllocator, (VkBufferCreateInfo*)&createInfo, &alloCreateInfo, (VkBuffer*)&(outputBuffer.buffer), &outputBuffer.allocationHandle, &outputBuffer.allocationInfo);
//
//	if (createInfo.createInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
//		outputBuffer.deviceAddress = m_device.getBufferAddress(
//			{
//				.buffer = outputBuffer.buffer
//			}
//		);
//	}
//
//	if (!debugName.empty()) {
//		SetDebugName(m_device, vk::ObjectType::eBuffer, GetVulkanHandle(outputBuffer.buffer), debugName);
//	}
//
//	return outputBuffer;
//}
//
//void VulkanRenderer::DestroyBufferImmediate(VulkanBuffer& buffer) {
//	buffer = VulkanBuffer();
//}
//
//void VulkanRenderer::DestroyBufferDeferred(VulkanBuffer& buffer) {
//	m_deferredDeleteBuffers.emplace_back(std::move(buffer), m_vkInit.framesInFlight);
//}
//
//void VulkanRenderer::UpdateDeferredBufferDeletion() {
//	for (std::vector<DeferredBufferDeletion>::iterator i = m_deferredDeleteBuffers.begin();
//		i != m_deferredDeleteBuffers.end(); )
//	{
//		(*i).framesCount--;
//
//		if ((*i).framesCount == 0) {
//			i = m_deferredDeleteBuffers.erase(i);
//		}
//		else {
//			++i;
//		}
//	}
//}
