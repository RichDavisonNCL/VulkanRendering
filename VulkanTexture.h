/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../NCLCoreClasses/Texture.h"
#include "VulkanTextureBuilder.h"
#include "SmartTypes.h"

namespace NCL::Rendering::Vulkan {
	class VulkanRenderer;

	class VulkanTexture : public Texture	{
		friend class VulkanRenderer;
		friend class TextureBuilder;
	public:
		~VulkanTexture();

		vk::ImageView GetDefaultView() const {
			return *defaultView;
		}

		vk::Format GetFormat() const {
			return format;
		}

		vk::Image GetImage() const {
			return image;
		}

		//Allows us to pass a texture as vk type to various functions
		operator vk::Image() const {
			return image;
		}
		operator vk::ImageView() const {
			return *defaultView;
		}		
		operator vk::Format() const {
			return format;
		}

		void GenerateMipMaps(	vk::CommandBuffer  buffer, 
								vk::ImageLayout startLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
								vk::ImageLayout endLayout	= vk::ImageLayout::eShaderReadOnlyOptimal, 
								vk::PipelineStageFlags2 endFlags = vk::PipelineStageFlagBits2::eFragmentShader);

		template <typename T, uint32_t n>
		static constexpr size_t GetMaxMips(const VectorTemplate<T, n>& texDims) {
			T m = Vector::GetMaxElement(texDims);
			return (size_t)std::floor(log2((float(m)))) + 1;
		}

	protected:
		VulkanTexture();

		vk::UniqueImageView		defaultView;
		vk::Image				image; //Don't use 'Unique', uses VMA
		vk::Format				format = vk::Format::eUndefined;
		vk::ImageAspectFlags	aspectType;

		vk::UniqueSemaphore		workSemaphore;

		VmaAllocation			allocationHandle	= {};
		VmaAllocationInfo		allocationInfo		= {};
		VmaAllocator			allocator			= {};

		uint32_t mipCount	= 0;
		uint32_t layerCount = 0;
	};
}