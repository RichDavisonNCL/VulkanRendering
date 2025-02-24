/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "VulkanBuffers.h"

namespace NCL::Rendering::Vulkan {
    class VulkanStagingBuffers
    {
    public:
        VulkanStagingBuffers() {};
        VulkanStagingBuffers(vk::Device device, VmaAllocator allocator, uint32_t framesInFlight = 3);
        ~VulkanStagingBuffers();

        const VulkanBuffer* GetStagingBuffer(size_t allocationSize);

        void Update();

    protected:
        vk::Device device;
        VmaAllocator allocator;
 
        struct StagingBuffer {
            VulkanBuffer buffer;
            uint32_t framesCount;
        };

        std::vector<StagingBuffer> activeBuffers;

        uint32_t framesInFlight;
    };
}

