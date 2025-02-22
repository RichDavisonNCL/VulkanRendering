/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanTexture.h"
#include "Vulkanrenderer.h"
#include "TextureLoader.h"
#include "VulkanUtils.h"
#include "VulkanBuffers.h"
#include "VulkanBufferBuilder.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanTexture::VulkanTexture() {
	mipCount	= 0;
	layerCount	= 0;
	format		= vk::Format::eUndefined;
}

VulkanTexture::~VulkanTexture() {
	if (image) {
		vmaDestroyImage(allocator, image, allocationHandle);
	}
}

void VulkanTexture::GenerateMipMaps(vk::CommandBuffer  buffer, vk::ImageLayout endLayout, vk::PipelineStageFlags2 endFlags) {
	for (int layer = 0; layer < layerCount; ++layer) {	
		ImageTransitionBarrier(buffer, image, 
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal, 
			aspectType, 
			vk::PipelineStageFlagBits2::eAllCommands, vk::PipelineStageFlagBits2::eTransfer, 
			0, 1, layer, 1);

			vk::ImageBlit blitData;
			blitData.srcSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(0)
				.setBaseArrayLayer(layer)
				.setLayerCount(1);

			blitData.srcOffsets[0] = vk::Offset3D(0, 0, 0);
			blitData.srcOffsets[1].x = dimensions.x;
			blitData.srcOffsets[1].y = dimensions.y;
			blitData.srcOffsets[1].z = 1;
	
		for (uint32_t mip = 1; mip < mipCount; ++mip) {
			blitData.dstSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(mip)
				.setBaseArrayLayer(layer)
				.setLayerCount(1);

			blitData.dstOffsets[0] = vk::Offset3D(0, 0, 0);
			blitData.dstOffsets[1].x = std::max(dimensions.x >> mip, (uint32_t)1);
			blitData.dstOffsets[1].y = std::max(dimensions.y >> mip, (uint32_t)1);
			blitData.dstOffsets[1].z = 1;
			//Prepare the new mip level to receive the blitted data
			ImageTransitionBarrier(buffer, image, 
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 
				aspectType, 
				vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eTransfer,
				mip, 1, layer, 1);
			
			buffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blitData, vk::Filter::eLinear);
			//The new mip is then transitioned again to be able to act as a source
			//for the next level 
			ImageTransitionBarrier(buffer, image,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
				aspectType,
				vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eTransfer,
				mip, 1, layer, 1);

			blitData.srcOffsets		= blitData.dstOffsets;
			blitData.srcSubresource = blitData.dstSubresource;
		}
		//Now that this layer's mipchain is complete, transition the whole lot to final layout
		ImageTransitionBarrier(buffer, image, 
				vk::ImageLayout::eTransferSrcOptimal, endLayout,
				aspectType, 
				vk::PipelineStageFlagBits2::eBlit, endFlags, 
				0, mipCount, layer, 1);
	}
}
