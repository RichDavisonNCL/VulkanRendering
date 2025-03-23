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
        VulkanStagingBuffers(vk::Device m_device, VmaAllocator m_allocator, uint32_t m_framesInFlight = 3);
        ~VulkanStagingBuffers();

        const VulkanBuffer* GetStagingBuffer(size_t allocationSize);

        void Update();

    protected:
        vk::Device m_device;
        VmaAllocator m_allocator;
 
        struct StagingBuffer {
            VulkanBuffer buffer;
            uint32_t framesCount;
        };

        std::vector<StagingBuffer> m_activeBuffers;

        uint32_t m_framesInFlight;
    };
}

