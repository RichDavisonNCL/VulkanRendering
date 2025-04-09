/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanTextureBuilder.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include "TextureLoader.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

TextureBuilder::TextureBuilder(vk::Device inDevice, VulkanMemoryManager& memManager) {
    m_sourceDevice   = inDevice;
    m_memManager     = &memManager;

    m_generateMips    = true;

    m_format      = vk::Format::eR8G8B8A8Unorm;
    m_layout      = vk::ImageLayout::eShaderReadOnlyOptimal;
    m_usages      = vk::ImageUsageFlagBits::eSampled;
    m_aspects     = vk::ImageAspectFlagBits::eColor;
    m_pipeFlags   = vk::PipelineStageFlagBits2::eFragmentShader;

    m_layerCount      = 1;
}

TextureBuilder& TextureBuilder::WithFormat(vk::Format inFormat) {
    m_format = inFormat;
    return *this;
}

TextureBuilder& TextureBuilder::WithLayout(vk::ImageLayout inLayout) {
    m_layout = inLayout;
    return *this;
}

TextureBuilder& TextureBuilder::WithAspects(vk::ImageAspectFlags inAspects) {
    m_aspects = inAspects;
    return *this;
}

TextureBuilder& TextureBuilder::WithUsages(vk::ImageUsageFlags inUsages) {
    m_usages = inUsages;
    return *this;
}

TextureBuilder& TextureBuilder::WithPipeFlags(vk::PipelineStageFlags2 inFlags) {
    m_pipeFlags = inFlags;
    return *this;
}

TextureBuilder& TextureBuilder::WithDimension(uint32_t inWidth, uint32_t inHeight, uint32_t inDepth) {
	m_requestedSize = { inWidth, inHeight, inDepth };
    return *this;
}

TextureBuilder& TextureBuilder::WithLayerCount(uint32_t layers) {
    m_layerCount = layers;
    return *this;
}

TextureBuilder& TextureBuilder::WithMips(bool inMips) {
    m_generateMips = inMips;
    return *this;
}

TextureBuilder& TextureBuilder::WithCommandBuffer(vk::CommandBuffer inBuffer) {
    assert(MessageAssert(m_queue == 0 && m_pool == 0, "Builder is either passed a command buffer OR uses a queue and pool!"));
    m_cmdBuffer = inBuffer;
    return *this;
}

TextureBuilder& TextureBuilder::UsingQueue(vk::Queue inQueue) {
    assert(MessageAssert(m_cmdBuffer == 0, "Builder is either passed a command buffer OR uses a queue and pool!"));
    m_queue = inQueue;
    return *this;
}

TextureBuilder& TextureBuilder::UsingPool(vk::CommandPool inPool) {
    assert(MessageAssert(m_cmdBuffer == 0, "Builder is either passed a command buffer OR uses a queue and pool!"));
    m_pool = inPool;
    return *this;
}

UniqueVulkanTexture TextureBuilder::Build(const std::string& debugName) {
    vk::UniqueCommandBuffer	uniqueBuffer;
    vk::CommandBuffer	    usingBuffer;
    BeginTexture(debugName, uniqueBuffer, usingBuffer);

    UniqueVulkanTexture tex = GenerateTexture(usingBuffer, m_requestedSize, false, debugName);

    //ImageTransitionBarrier(usingBuffer, tex->GetImage(), vk::ImageLayout::eUndefined, layout, aspects, vk::PipelineStageFlagBits::eTopOfPipe, pipeFlags);

    TextureJob job;
    job.image = tex->GetImage();
    EndTexture(debugName, uniqueBuffer, usingBuffer, job, tex);

    return tex;
}

UniqueVulkanTexture TextureBuilder::BuildFromData(void* dataSrc, size_t byteCount, const std::string& debugName) {
    vk::UniqueCommandBuffer	uniqueBuffer;
    vk::CommandBuffer	    usingBuffer;
    BeginTexture(debugName, uniqueBuffer, usingBuffer);

    UniqueVulkanTexture tex = GenerateTexture(usingBuffer, m_requestedSize, false, debugName);

    //ImageTransitionBarrier(usingBuffer, tex->GetImage(), vk::ImageLayout::eUndefined, layout, aspects, vk::PipelineStageFlagBits::eTopOfPipe, pipeFlags);

    TextureJob job;
    job.faceCount = 1;
    job.dataSrcs[0] = (char*)dataSrc;
    job.dataOwnership[0] = false;
    job.image = tex->GetImage();
    job.endLayout = m_layout;
    job.aspect = vk::ImageAspectFlagBits::eColor;
    job.faceByteCount = byteCount;
    job.dimensions = m_requestedSize;

    UploadTextureData(usingBuffer, job);

    EndTexture(debugName, uniqueBuffer, usingBuffer, job, tex);

    return tex;
}

void TextureBuilder::BeginTexture(const std::string& debugName, vk::UniqueCommandBuffer& uniqueBuffer, vk::CommandBuffer& usingBuffer) {
    //We're appending to an external command buffer
    if (m_cmdBuffer) {
        usingBuffer = m_cmdBuffer;
    }
    //We're in charge of our own command buffers
    else if (m_queue && m_pool) {
        uniqueBuffer = Vulkan::CmdBufferCreateBegin(m_sourceDevice, m_pool, debugName + " Creation");
        usingBuffer = *uniqueBuffer;
    }
    else {
        assert(MessageAssert(true, "Cannot build without either a command buffer OR a queue and pool"));
    }
}

void TextureBuilder::EndTexture(const std::string& debugName, vk::UniqueCommandBuffer& uniqueBuffer, vk::CommandBuffer& usingBuffer, TextureJob& job, UniqueVulkanTexture& t) {
    if (m_generateMips) {
        int mipCount = VulkanTexture::GetMaxMips(t->GetDimensions());
        if (mipCount > 1) {
            t->m_mipCount = mipCount;
            t->GenerateMipMaps(usingBuffer, job.endLayout);
        }
    }

    //If we're in charge of our own buffers, we just stop and wait for completion now
    if (uniqueBuffer) {
        CmdBufferEndSubmitWait(usingBuffer, m_sourceDevice, m_queue);
        for (int i = 0; i < job.faceCount; ++i) {
            if (job.dataOwnership[i]) {
                TextureLoader::DeleteTextureData(job.dataSrcs[i]);
            }
        }
    }
    //Otherwise, this is going to be handled external to the builder, and placed as a 'job'
    else {
        job.workFence   = m_sourceDevice.createFence({});
        job.jobName     = debugName;
        m_activeJobs.emplace_back(job);
    }
}

UniqueVulkanTexture TextureBuilder::BuildFromFile(const std::string& filename) {
    char* texData = nullptr;
    Vector3ui dimensions(0, 0, 1);
    uint32_t channels    = 0;
    uint32_t flags       = 0;
    TextureLoader::LoadTexture(filename, texData, dimensions.x, dimensions.y, channels, flags);

    vk::UniqueCommandBuffer	uniqueBuffer;
    vk::CommandBuffer	    usingBuffer;

    BeginTexture(filename, uniqueBuffer, usingBuffer);

    vk::ImageUsageFlags	realUsages = m_usages;

    m_usages |= vk::ImageUsageFlagBits::eTransferDst;

    UniqueVulkanTexture tex = GenerateTexture(usingBuffer, dimensions, false, filename);

    TextureJob job;
    job.faceCount = 1;
    job.dataSrcs[0] = texData;
    job.dataOwnership[0] = true;
    job.image = tex->GetImage();
    job.endLayout = m_layout;
    job.aspect = vk::ImageAspectFlagBits::eColor;
    job.dimensions = dimensions;
    job.faceByteCount = dimensions.x * dimensions.y * dimensions.z * channels * sizeof(char);

    UploadTextureData(usingBuffer, job);

    EndTexture(filename, uniqueBuffer, usingBuffer, job, tex);

    m_usages = realUsages;

    return tex;
}

UniqueVulkanTexture TextureBuilder::BuildCubemapFromFile(
    const std::string& negativeXFile, const std::string& positiveXFile,
    const std::string& negativeYFile, const std::string& positiveYFile,
    const std::string& negativeZFile, const std::string& positiveZFile,
    const std::string& debugName) {

    TextureJob job;
    job.faceCount = 6;
    job.endLayout = m_layout;
    job.aspect = vk::ImageAspectFlagBits::eColor;

    Vector3ui dimensions[6]{ Vector3ui(0, 0, 1) };
    uint32_t channels[6] = { 0 };
    uint32_t flags[6] = { 0 };

    const std::string* filenames[6] = {
        &negativeXFile,
        &positiveXFile,
        &negativeYFile,
        &positiveYFile,
        &negativeZFile,
        &positiveZFile
    };

    for (int i = 0; i < 6; ++i) {
        TextureLoader::LoadTexture(*filenames[i], job.dataSrcs[i], dimensions[i].x, dimensions[i].y, channels[i], flags[i]);
        job.dataOwnership[i] = true;
    }

    vk::UniqueCommandBuffer	uniqueBuffer;
    vk::CommandBuffer	    usingBuffer;
    BeginTexture(debugName, uniqueBuffer, usingBuffer);

    vk::ImageUsageFlags	realUsages = m_usages;

    m_usages |= vk::ImageUsageFlagBits::eTransferDst;

    uint32_t dataWidth = sizeof(char) * channels[0];

    job.faceByteCount = dimensions[0].x * dimensions[0].y * dimensions[0].z * channels[0] * sizeof(char);
    job.dimensions = dimensions[0];

    UniqueVulkanTexture tex = GenerateTexture(usingBuffer, dimensions[0], true, debugName);
    job.image = tex->GetImage();

    UploadTextureData(usingBuffer, job);
    EndTexture(debugName, uniqueBuffer, usingBuffer, job, tex);

    m_usages = realUsages;

    return tex;
}

UniqueVulkanTexture TextureBuilder::BuildCubemap(const std::string& debugName) {
    vk::UniqueCommandBuffer	uniqueBuffer;
    vk::CommandBuffer	    usingBuffer;
    BeginTexture(debugName, uniqueBuffer, usingBuffer);

    UniqueVulkanTexture tex = GenerateTexture(usingBuffer, m_requestedSize, true, debugName);

    TextureJob job;
    job.image = tex->GetImage();
    job.endLayout = m_layout;
    EndTexture(debugName, uniqueBuffer, usingBuffer, job, tex);

    return tex;
}

UniqueVulkanTexture	TextureBuilder::GenerateTexture(vk::CommandBuffer m_cmdBuffer, Vector3ui dimensions, bool isCube, const std::string& debugName) {
    VulkanTexture* t = new VulkanTexture();

    uint32_t mipCount = VulkanTexture::GetMaxMips(dimensions);

    uint32_t genLayerCount = isCube ? m_layerCount * 6: m_layerCount;

    vk::ImageUsageFlags creationUsages = m_usages;

    if (m_generateMips) {
        creationUsages |= vk::ImageUsageFlagBits::eTransferSrc;
        creationUsages |= vk::ImageUsageFlagBits::eTransferDst;
    }

    auto createInfo = vk::ImageCreateInfo()
        .setImageType(dimensions.z > 1 ? vk::ImageType::e3D : vk::ImageType::e2D)
        .setExtent(vk::Extent3D(dimensions.x, dimensions.y, dimensions.z))
        .setFormat(m_format)
        .setUsage(creationUsages)
        .setMipLevels(m_generateMips ? mipCount : 1)
        //.setInitialLayout(layout)
        .setArrayLayers(genLayerCount);

	if (isCube) {
		createInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
	}

    t->m_image = m_memManager->CreateImage(createInfo);

    vk::ImageViewType viewType = m_layerCount > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
    if (isCube) {
        viewType = m_layerCount > 1 ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
    }
    else if (dimensions.z > 1) {
        viewType = vk::ImageViewType::e3D;
    }

	vk::ImageViewCreateInfo viewInfo = vk::ImageViewCreateInfo()
        .setViewType(viewType)
		.setFormat(m_format)
        .setSubresourceRange(vk::ImageSubresourceRange(m_aspects, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS))
		.setImage(t->m_image);

	t->m_defaultView    = m_sourceDevice.createImageViewUnique(viewInfo);
    t->m_memManager     = m_memManager;
    t->m_layerCount     = genLayerCount;
    t->m_aspectType     = m_aspects;
    t->m_format         = m_format;
    t->dimensions       = { dimensions.x, dimensions.y };

	SetDebugName(m_sourceDevice, vk::ObjectType::eImage    , GetVulkanHandle(t->m_image)       , debugName);
	SetDebugName(m_sourceDevice, vk::ObjectType::eImageView, GetVulkanHandle(*t->m_defaultView), debugName);

	ImageTransitionBarrier(m_cmdBuffer, t->m_image, vk::ImageLayout::eUndefined, m_layout, m_aspects, vk::PipelineStageFlagBits2::eTopOfPipe, m_pipeFlags);

    return UniqueVulkanTexture(t);
}

void TextureBuilder::UploadTextureData(vk::CommandBuffer m_cmdBuffer, TextureJob& job) {
    int allocationSize = job.faceByteCount * job.faceCount;

    job.stagingBuffer = m_memManager->CreateStagingBuffer(allocationSize, "Staging Buffer");

    //our buffer now has memory! Copy some texture date to it...
    char* gpuPtr = (char*)job.stagingBuffer.Map();
    for (int i = 0; i < job.faceCount; ++i) {
        memcpy(gpuPtr, job.dataSrcs[i], job.faceByteCount);
        gpuPtr += job.faceByteCount;
    }
    job.stagingBuffer.Unmap();

    Vulkan::UploadTextureData(m_cmdBuffer, job.stagingBuffer.buffer, job.image, vk::ImageLayout::eUndefined, job.endLayout,
        vk::BufferImageCopy{
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel   = 0,
                .layerCount = job.faceCount
            },
            .imageExtent{job.dimensions.x, job.dimensions.y, job.dimensions.z},          
        }
    );
}

bool	TextureBuilder::IsProcessing() const {
    return !m_activeJobs.empty();
}

bool	TextureBuilder::IsProcessing(const std::string& debugName) const {
    if (m_activeJobs.empty()) {
        return false;
    }
    return false;
}

void	TextureBuilder::WaitForProcessing() {
    for (const auto& i : m_activeJobs) {
        if (m_sourceDevice.waitForFences(1, &i.workFence, true, UINT64_MAX) != vk::Result::eSuccess) {
        };
        m_sourceDevice.destroyFence(i.workFence);

        for (int j = 0; j < 6; ++j) {
            TextureLoader::DeleteTextureData(i.dataSrcs[j]);
        }
    }
    m_activeJobs.clear();
}