/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanStagingBuffers.h"
#include "VulkanBufferBuilder.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanStagingBuffers::VulkanStagingBuffers(vk::Device inDevice, VmaAllocator inAllocator, uint32_t inFramesInFlight) {
    m_device          = inDevice;
    m_allocator       = inAllocator;
    m_framesInFlight  = inFramesInFlight;
}

VulkanStagingBuffers::~VulkanStagingBuffers() {

}

const VulkanBuffer* VulkanStagingBuffers::GetStagingBuffer(size_t allocationSize) {
    m_activeBuffers.emplace_back(
        BufferBuilder(m_device, m_allocator)
        .WithBufferUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .WithHostVisibility()
        .Build(allocationSize, "Staging Buffer"),
        m_framesInFlight);

    return &m_activeBuffers.back().buffer;

    return nullptr;
}

void VulkanStagingBuffers::Update() {
    for (std::vector<StagingBuffer>::iterator i = m_activeBuffers.begin();
        i != m_activeBuffers.end(); )
    {
        (*i).framesCount--;

        if ((*i).framesCount == 0) {
            i = m_activeBuffers.erase(i);
        }
        else {
            ++i;
        }
    }
}