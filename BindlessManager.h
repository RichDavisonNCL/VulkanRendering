/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Buffer.h"

namespace VKQuick {
	class MemoryManager;
	class Mesh;
	class Texture;
	class Buffer;

	class BindlessManager {
	public:
		BindlessManager(vk::Device device, vk::DescriptorPool pool,  MemoryManager& memManager, uint32_t initialBufferSizes = 1024 * 1024);

		~BindlessManager() {

		} 

		uint32_t AddMesh(const VKQuick::Mesh& mesh, std::vector< int32_t > materials);
		uint32_t AddTexture(const VKQuick::Texture& tex, const vk::Sampler sampler);
		uint32_t AddBuffer(const VKQuick::Buffer& buffer);

		template<typename T>
		uint32_t AddMaterial(const T& mat) {
			uint32_t index = m_materialsAdded;

			T& material = m_materialsBuffer.Map<T>()[index];
			material = mat;
			m_materialsBuffer.Unmap();

			m_materialsAdded++;

			return index;
		}

		vk::DescriptorSet GetDescriptorSet() const {
			return *m_bindlessSet;
		}

		vk::DescriptorSetLayout GetDescriptorSetLayout() const {
			return *m_bindlessLayout;
		}

	protected:
		vk::Device			m_device;
		MemoryManager&		m_memoryManager;

		vk::UniqueDescriptorSet			m_bindlessSet;
		vk::UniqueDescriptorSetLayout	m_bindlessLayout;

		std::unordered_map<const VKQuick::Mesh*		, uint32_t>	m_meshes;
		std::unordered_map<const VKQuick::Texture*	, uint32_t> m_textures;
		std::unordered_map<const VKQuick::Buffer*	, uint32_t>	m_buffers;

		VKQuick::Buffer	m_allBuffers;

		VKQuick::Buffer	m_materialsBuffer;
		VKQuick::Buffer	m_meshesBuffer;
		VKQuick::Buffer	m_meshLayersBuffer;

		uint32_t m_subLayersUsed = 0;

		uint32_t m_materialsAdded = 0;
	};
}