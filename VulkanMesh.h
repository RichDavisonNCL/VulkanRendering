/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../NCLCoreClasses/Mesh.h"
#include "../VKQuick/Buffer.h"
#include "../VKQuick/Mesh.h"

namespace NCL::Rendering::Vulkan {
	class VulkanMemoryManager;

	class VulkanMesh : public Mesh {
	public:
		friend class VulkanRenderer;
		VulkanMesh();
		~VulkanMesh();

	/*	void Draw(vk::CommandBuffer  to, int instanceCount = 1);
		void DrawLayer(unsigned int layer, vk::CommandBuffer  to, int instanceCount = 1);
		void DrawAllLayers(vk::CommandBuffer  to, int instanceCount = 1);*/

		void UploadToGPU(RendererBase* renderer) override {}

		void	UploadAttributes(vk::CommandBuffer  to);
		void	InitialiseGPUState(vk::Device device, VKQuick::MemoryManager& memManager, vk::BufferUsageFlags extraFlags = {});

		uint32_t	GetAttributeMask() const;
		vk::PrimitiveTopology GetVulkanTopology() const;

		const VKQuick::UniqueMesh& GetMesh() const {
			return m_mesh;
		}

	protected:
		VKQuick::UniqueMesh m_mesh;

		uint32_t	m_attributeMask		= 0;

		std::vector< VertexAttribute::Type >	m_usedAttributes;		
	};

	using UniqueVulkanMesh = std::unique_ptr<VulkanMesh>;
	using SharedVulkanMesh = std::shared_ptr<VulkanMesh>;
}