/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "BindlessManager.h"

#include "../VKQuick/DescriptorSetBuilder.h"
#include "../VKQuick/DescriptorSetLayoutBuilder.h"
#include "../VKQuick/MemoryManager.h"
#include "../VKQuick/Mesh.h"

#include "./Shaders/VK/GLSLInterop.h"

#define BINDLESS_SET 1
#include "./Shaders/VK/VKQuick/bindless.glslh"

using namespace VKQuick;

const int TEXTURE_SLOT = 4;

BindlessManager::BindlessManager(vk::Device device, vk::DescriptorPool pool, MemoryManager& memManager, uint32_t initialBufferSizes)
	: m_memoryManager(memManager), m_device(device)
{
	m_meshesBuffer = memManager.CreateBuffer(
		{
			.size = initialBufferSizes,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		},
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		"BindlessManager MeshEntry Buffer"
	);

	m_meshLayersBuffer = memManager.CreateBuffer(
		{
			.size = initialBufferSizes,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		},
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		"BindlessManager MeshLayerEntry Buffer"
	);

	m_materialsBuffer = memManager.CreateBuffer(
		{
			.size	= initialBufferSizes,
			.usage	= vk::BufferUsageFlagBits::eStorageBuffer
		},
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		"BindlessManager Materials Buffer"
	);

	m_allBuffers = memManager.CreateBuffer(
		{
			.size = sizeof(vk::DeviceAddress) * initialBufferSizes,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer
		},
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		"BindlessManager Buffer Pointer Buffer"
	);

	size_t _NumSamplers = 1024; //TODO!

	m_bindlessLayout = VKQuick::DescriptorSetLayoutBuilder(device)	
		.WithStorageBuffers(0, 1)	//Aliased buffers
		.WithStorageBuffers(1, 1)	//MeshesBuffer
		.WithStorageBuffers(2, 1)	//MeshLayersBuffer
		.WithStorageBuffers(3, 1)	//MaterialsBuffer
		.WithImageSamplers(TEXTURE_SLOT, _NumSamplers, vk::ShaderStageFlagBits::eAll, vk::DescriptorBindingFlagBits::eVariableDescriptorCount)		//allTextures

		.WithCreationFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
		.WithGlobalBindingFlags(vk::DescriptorBindingFlagBits::ePartiallyBound)
		.Build("Bindless Texture Data");

	m_bindlessSet = VKQuick::DescriptorSetBuilder(device, pool, *m_bindlessLayout, _NumSamplers)
		.WriteBuffer(0, m_allBuffers, vk::DescriptorType::eStorageBuffer)
		.WriteBuffer(1, m_meshesBuffer, vk::DescriptorType::eStorageBuffer)
		.WriteBuffer(2, m_meshLayersBuffer, vk::DescriptorType::eStorageBuffer)
		.WriteBuffer(3, m_materialsBuffer, vk::DescriptorType::eStorageBuffer)

	.Build();
}

uint32_t BindlessManager::AddMesh(const VKQuick::Mesh& mesh, std::vector< int32_t > materials) {
	auto entry = m_meshes.insert({ &mesh , (uint32_t)m_meshes.size() });

	if (entry.second) {
		//This was a new mesh!
		//We need a new entry for it
		//and then new entries for all of the submeshes

		const std::vector<MeshRange>& ranges = mesh.GetRanges();
		int32_t* matIndex = materials.data();

		MeshEntry& meshEntry = m_meshesBuffer.Map<MeshEntry>()[entry.first->second];

		uint32_t bufferIndex = AddBuffer(mesh.GetBuffer());
		AttributeData attributeData;
		IndexData indexData;

		mesh.GetIndexData(indexData);

		size_t attribIndex = 0;
		if (mesh.GetAttributeIndex(VKQuick::AttributeType::Position, attribIndex) && 
			mesh.GeAttributeData(attribIndex, attributeData)) {

			meshEntry.positionBufferIndex  = bufferIndex;
			meshEntry.positionBufferOffset = attributeData.offset;
		}

		if (mesh.GetAttributeIndex(VKQuick::AttributeType::Colour, attribIndex) &&
			mesh.GeAttributeData(attribIndex, attributeData)) {

			meshEntry.colourBufferIndex = bufferIndex;
			meshEntry.colourBufferOffset = attributeData.offset;
		}

		if (mesh.GetAttributeIndex(VKQuick::AttributeType::TexCoord, attribIndex) &&
			mesh.GeAttributeData(attribIndex, attributeData)) {

			meshEntry.texCoordBufferIndex = bufferIndex;
			meshEntry.texCoordBufferOffset = attributeData.offset;
		}

		if (mesh.GetAttributeIndex(VKQuick::AttributeType::Normals, attribIndex) &&
			mesh.GeAttributeData(attribIndex, attributeData)) {

			meshEntry.normalBufferIndex = bufferIndex;
			meshEntry.normalBufferOffset = attributeData.offset;
		}

		if (mesh.GetAttributeIndex(VKQuick::AttributeType::Tangents, attribIndex) &&
			mesh.GeAttributeData(attribIndex, attributeData)) {

			meshEntry.tangentBufferIndex = bufferIndex;
			meshEntry.tangentBufferOffset = attributeData.offset;
		}

		meshEntry.indexBufferIndex	= bufferIndex;
		meshEntry.indexBufferOffset = indexData.offset;

		//Now copy the info for each of the submeshes / sublayers / whatevers
		meshEntry.subMeshCount		= ranges.size();
		meshEntry.firstSubMeshIndex = m_subLayersUsed;
		m_subLayersUsed += meshEntry.subMeshCount;

		MeshLayerEntry* meshLayer = &m_meshLayersBuffer.Map<MeshLayerEntry>()[meshEntry.firstSubMeshIndex];

		for (auto& range : ranges) {
			meshLayer->firstElement		= range.start;
			meshLayer->elementCount		= range.count;
			meshLayer->base				= range.base;
			meshLayer->materialIndex	= *matIndex;

			meshLayer++;
			matIndex++;
		}

		m_meshLayersBuffer.Unmap();
		m_meshesBuffer.Unmap();
	}

	return entry.first->second;
}

uint32_t BindlessManager::AddTexture(const VKQuick::Texture& tex, const vk::Sampler sampler) {
	auto entry = m_textures.insert({ &tex , (uint32_t)m_textures.size() });

	if (entry.second) { //This was a new texture!
		WriteCombinedImageDescriptor(m_device, *m_bindlessSet, TEXTURE_SLOT, entry.first->second, tex, sampler);
	}

	return entry.first->second;
}

uint32_t BindlessManager::AddBuffer(const VKQuick::Buffer& buffer) {
	auto entry = m_buffers.insert({ &buffer , (uint32_t)m_buffers.size() });

	if (entry.second) { //This was a new buffer!
		vk::DeviceAddress& addresses = m_allBuffers.Map<vk::DeviceAddress>()[entry.first->second];
		addresses = buffer.GetDeviceAddress();
		m_allBuffers.Unmap();
	}

	return entry.first->second;
}