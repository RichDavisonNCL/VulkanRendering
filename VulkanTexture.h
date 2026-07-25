/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../NCLCoreClasses/Texture.h"
#include "../VKQuick/Texture.h"

namespace NCL::Rendering::Vulkan {
	class VulkanMemoryManager;

	class VulkanTexture : public Texture {
	public:
		friend class VulkanRenderer;

		VulkanTexture(VKQuick::Texture& t);
		~VulkanTexture();

		const VKQuick::Texture& GetTex() const {
			return m_texture;
		}

	protected:
		VKQuick::Texture m_texture;
	};

	using UniqueVulkanTexture = std::unique_ptr<VulkanTexture>;
	using SharedVulkanTexture = std::shared_ptr<VulkanTexture>;
}