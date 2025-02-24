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
    device          = inDevice;
    allocator       = inAllocator;
    framesInFlight  = inFramesInFlight;
}

VulkanStagingBuffers::~VulkanStagingBuffers() {

}

const VulkanBuffer* VulkanStagingBuffers::GetStagingBuffer(size_t allocationSize) {
    activeBuffers.emplace_back(
        BufferBuilder(device, allocator)
        .WithBufferUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .WithHostVisibility()
        .Build(allocationSize, "Staging Buffer"),
        framesInFlight);

    return &activeBuffers.back().buffer;

    return nullptr;
}

void VulkanStagingBuffers::Update() {
    for (std::vector<StagingBuffer>::iterator i = activeBuffers.begin();
        i != activeBuffers.end(); )
    {
        (*i).framesCount--;

        if ((*i).framesCount == 0) {
            i = activeBuffers.erase(i);
        }
        else {
            ++i;
        }
    }
}