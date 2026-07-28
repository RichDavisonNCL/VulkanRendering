/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../NCLCoreClasses/Mesh.h"
#include "../VKQuick/Mesh.h"

namespace NCL::Rendering::Vulkan {
	class VulkanMesh : public Mesh {
	public:
		VulkanMesh();
		~VulkanMesh();

		void	UploadAttributes(vk::CommandBuffer  to);
		void	InitialiseGPUState(vk::Device device, VKQuick::MemoryManager& memManager, vk::BufferUsageFlags extraFlags = {});

		vk::PrimitiveTopology GetPrimitiveTopology() const;

		const VKQuick::UniqueMesh& GetMesh() const {
			return m_mesh;
		}

		uint32_t	GetAttributeMask() const;

	protected:
		VKQuick::UniqueMesh m_mesh;

		uint32_t	m_attributeMask		= 0;

		std::vector< VertexAttribute::Type >	m_usedAttributes;		
	};

	using UniqueVulkanMesh = std::unique_ptr<VulkanMesh>;
	using SharedVulkanMesh = std::shared_ptr<VulkanMesh>;
}