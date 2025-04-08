/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanVMAMemoryManager.h"
#include "VulkanTexture.h"
#include "Vulkanrenderer.h"
#include "VulkanUtils.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanVMAMemoryManager::VulkanVMAMemoryManager(vk::Device device, vk::PhysicalDevice physicalDevice, vk::Instance instance, const VulkanInitialisation& vkInit) {
	VmaVulkanFunctions funcs = {};
	m_allocatorInfo = {};
	funcs.vkGetInstanceProcAddr = ::vk::detail::defaultDispatchLoaderDynamic.vkGetInstanceProcAddr;
	funcs.vkGetDeviceProcAddr	= ::vk::detail::defaultDispatchLoaderDynamic.vkGetDeviceProcAddr;

	m_allocatorInfo.physicalDevice		= physicalDevice;
	m_allocatorInfo.device				= device;
	m_allocatorInfo.instance			= instance;
	m_allocatorInfo.vulkanApiVersion	= VK_MAKE_API_VERSION(0, vkInit.majorVersion, vkInit.minorVersion, 0);

	m_allocatorInfo.flags |= vkInit.vmaFlags;

	m_framesInFlight = vkInit.framesInFlight;

	m_allocatorInfo.pVulkanFunctions = &funcs;
	vmaCreateAllocator(&m_allocatorInfo, &m_memoryAllocator);
}

VulkanVMAMemoryManager::~VulkanVMAMemoryManager() {
	for (std::vector<DeferredBufferDeletion>::iterator i = m_deferredDeleteBuffers.begin();
		i != m_deferredDeleteBuffers.end(); ++i )
	{
		if (VMABuffer* b = dynamic_cast<VMABuffer*>(&i->buffer)) {
			b->Destroy();
		}
	}

	vmaDestroyAllocator(m_memoryAllocator);
}

UniqueVulkanBuffer VulkanVMAMemoryManager::CreateBuffer(const BufferCreationInfo& createInfo, const std::string& debugName) {
	VMABuffer* newBuffer = new VMABuffer(*this);

	newBuffer->m_allocator = m_memoryAllocator;
	
	size_t allocSize = createInfo.createInfo.size;
	
	newBuffer->size = allocSize;
		
	VmaAllocationCreateInfo alloCreateInfo = {};
	alloCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	
	alloCreateInfo.requiredFlags = (VkMemoryPropertyFlags)createInfo.memProperties;
	
	if (createInfo.memProperties & vk::MemoryPropertyFlagBits::eHostVisible) {
		alloCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
	}
	
	if (createInfo.memProperties & vk::MemoryPropertyFlagBits::eHostCoherent) {
		alloCreateInfo.flags |= (VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
	}
	
	vmaCreateBuffer(m_memoryAllocator, (VkBufferCreateInfo*)&createInfo, &alloCreateInfo, (VkBuffer*)&(newBuffer->buffer), &newBuffer->m_allocationHandle, &newBuffer->m_allocationInfo);
	
	if (createInfo.createInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {

		vk::Device d = m_allocatorInfo.device;

		newBuffer->deviceAddress = d.getBufferAddress(
			{
				.buffer = newBuffer->buffer
			}
		);
	}
	
	if (!debugName.empty()) {
		SetDebugName(m_allocatorInfo.device, vk::ObjectType::eBuffer, GetVulkanHandle(newBuffer->buffer), debugName);
	}
	
	return UniqueVulkanBuffer(newBuffer);
}

UniqueVulkanBuffer VulkanVMAMemoryManager::CreateStagingBuffer(size_t size, const std::string& debugName) {
	BufferCreationInfo createInfo = {
		.createInfo = {
			.size	= size,
			.usage	= vk::BufferUsageFlagBits::eTransferSrc
		},
		.memProperties	= vk::MemoryPropertyFlagBits::eHostVisible,
	};

	return CreateBuffer(createInfo, debugName);
}

void VulkanVMAMemoryManager::DiscardBuffer(VulkanBuffer& buffer, DiscardMode discard) {
	if (discard == DiscardMode::Deferred) {
		if (VMABuffer* b = dynamic_cast<VMABuffer*>(&buffer)) {
			m_deferredDeleteBuffers.push_back({ std::move(*b), m_framesInFlight });
		}
	}
	else {
		if (VMABuffer* b = dynamic_cast<VMABuffer*>(&buffer)) {
			b->Destroy();
		}
	}
}


vk::Image VulkanVMAMemoryManager::CreateImage(vk::ImageCreateInfo& createInfo, const std::string& debugName) {
	vk::Image image;
	Allocation imageAlloc;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	vmaCreateImage(m_memoryAllocator, (VkImageCreateInfo*)&createInfo, &vmaallocInfo, (VkImage*)&image, &imageAlloc.m_allocationHandle, &imageAlloc.m_allocationInfo);

	m_imageAllocations[image] = imageAlloc;

	return image;
}

void VulkanVMAMemoryManager::DiscardImage(vk::Image& image, DiscardMode discard)	{
	auto it = m_imageAllocations.find(image);
	if (it != m_imageAllocations.end()) {
		vmaDestroyImage(m_memoryAllocator, image, it->second.m_allocationHandle);
		image = VK_NULL_HANDLE;
	}
}

void	VulkanVMAMemoryManager::Update() {
	for (std::vector<DeferredBufferDeletion>::iterator i = m_deferredDeleteBuffers.begin();
		i != m_deferredDeleteBuffers.end(); )
	{
		(*i).framesCount--;

		if ((*i).framesCount == 0) {
			i = m_deferredDeleteBuffers.erase(i);
		}
		else {
			++i;
		}
	}
}

VMABuffer::VMABuffer(VulkanVMAMemoryManager& manager) {
	sourceManager = &manager;
}

VMABuffer::VMABuffer(VMABuffer&& obj) : VulkanBuffer(std::move(obj)) {
	if (this != &obj) {
		m_allocationHandle	= obj.m_allocationHandle;
		m_allocationInfo	= obj.m_allocationInfo;
		m_allocator			= obj.m_allocator;

		obj.m_allocator			= 0;
		obj.m_allocationHandle	= 0;
		obj.buffer				= VK_NULL_HANDLE;
	}
}

VMABuffer& VMABuffer::operator=(VMABuffer&& obj) {
	if (this != &obj) {
		m_allocationHandle	= obj.m_allocationHandle;
		m_allocationInfo	= obj.m_allocationInfo;
		m_allocator			= obj.m_allocator;

		obj.m_allocator			= 0;
		obj.m_allocationHandle	= 0;
		obj.buffer				= VK_NULL_HANDLE;
	}
	return *this;
}

void* VMABuffer::Map()		const {
	if (m_allocationInfo.pMappedData) {
		return m_allocationInfo.pMappedData;
	}
	void* mappedData = nullptr;
	vmaMapMemory(m_allocator, m_allocationHandle, &mappedData);
	return mappedData;
}

void VMABuffer::Unmap()		const {
	vmaUnmapMemory(m_allocator, m_allocationHandle);
};

void* VMABuffer::Data() const {
	return m_allocationInfo.pMappedData;
}

void VMABuffer::CopyData(void* data, size_t size) const {
	if (m_allocationInfo.pMappedData) {
		//we're already mapped, can just copy
		memcpy(m_allocationInfo.pMappedData, data, size);
	}
	else {
		//We should be able to safely map this?
		void* mappedData = nullptr;
		vmaMapMemory(m_allocator, m_allocationHandle, &mappedData);
		memcpy(mappedData, data, size);
		vmaUnmapMemory(m_allocator, m_allocationHandle);
	}
}
