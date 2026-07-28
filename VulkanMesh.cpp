/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanMesh.h"

#include "../VKQuick/MemoryManager.h"
#include "../VKQuick/MeshBuilder.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

//These are both carefully arranged to match the MeshBuffer enum class!
vk::Format attributeFormats[] = { 
	vk::Format::eR32G32B32Sfloat,	//Positions have this format
	vk::Format::eR32G32B32A32Sfloat,//Colours
	vk::Format::eR32G32Sfloat,		//TexCoords
	vk::Format::eR32G32B32Sfloat,	//Normals
	vk::Format::eR32G32B32A32Sfloat,//Tangents are 4D!
	vk::Format::eR32G32B32A32Sfloat,//Skel Weights
	vk::Format::eR32G32B32A32Sint,	//Skel indices
	vk::Format::eR32G32B32A32Sfloat, //Generic Vec4s
	vk::Format::eR32Sint,			//Generic ints
};
//Attribute sizes for each of the above
size_t attributeSizes[] = {
	sizeof(Vector3),	//Positions 
	sizeof(Vector4),	//Colours
	sizeof(Vector2),	//TexCoords
	sizeof(Vector3),	//Normals
	sizeof(Vector4),	//Tangents are 4D!
	sizeof(Vector4),	//Skel Weights
	sizeof(Vector4),	//Skel indices
	sizeof(Vector4),	//Generic Vec4s
	sizeof(int),		//Generic ints
};

VulkanMesh::VulkanMesh() {

}

VulkanMesh::~VulkanMesh() {

}

void	VulkanMesh::UploadAttributes(vk::CommandBuffer  to) {
	void* allData = m_mesh->MapData();

	auto atrributeFunc = [&](VertexAttribute::Type attribute, size_t count, const char* data) {
		if (count == 0) {
			return;
		}
		uint32_t offset = 0;
		size_t	size = 0;
		vk::Format format;
		size_t stride = 0;
		if (!m_mesh->GeAttributeData((int)attribute, offset, size, format, stride)) {
			return;
		}
		char* gpuData = (char*)allData + offset;

		//TODO: check that returned size equals full attribute size!
		memcpy(gpuData, data, size);

		m_attributeMask |= (1 << attribute);
		};

	atrributeFunc(VertexAttribute::Positions, GetPositionData().size(), (const char*)GetPositionData().data());
	atrributeFunc(VertexAttribute::Colours, GetColourData().size(), (const char*)GetColourData().data());
	atrributeFunc(VertexAttribute::TextureCoords, GetTextureCoordData().size(), (const char*)GetTextureCoordData().data());
	atrributeFunc(VertexAttribute::Normals, GetNormalData().size(), (const char*)GetNormalData().data());
	atrributeFunc(VertexAttribute::Tangents, GetTangentData().size(), (const char*)GetTangentData().data());
	atrributeFunc(VertexAttribute::JointWeights, GetSkinWeightData().size(), (const char*)GetSkinWeightData().data());
	atrributeFunc(VertexAttribute::JointIndices, GetSkinIndexData().size(), (const char*)GetSkinIndexData().data());

	atrributeFunc(VertexAttribute::General_Vec4, GetGeneralVec4Data().size(), (const char*)GetGeneralVec4Data().data());
	atrributeFunc(VertexAttribute::General_Integer, GetGeneralIntegerData().size(), (const char*)GetGeneralIntegerData().data());

	if (GetIndexCount() > 0) {
		uint32_t	offset = 0;
		size_t		size = 0;
		vk::IndexType type;
		if (!m_mesh->GetIndexData(offset, size, type)) {
			return;
		}
		char* gpuData = (char*)allData + offset;

		memcpy(gpuData, GetIndexData().data(), size);
	}

	m_mesh->UnmapData(to);
}

void	VulkanMesh::InitialiseGPUState(vk::Device device, VKQuick::MemoryManager& memManager, vk::BufferUsageFlags extraFlags) {
	VKQuick::MeshBuilder builder = VKQuick::MeshBuilder(device, memManager)
		.WithVertexCount(GetVertexCount())
		.WithIndexCount(GetIndexCount(), vk::IndexType::eUint32)
		.WithBufferUsageFlags(extraFlags)
		.WithHostVisibleBuffers();

	auto atrributeFunc = [&](VertexAttribute::Type attribute, size_t count, const char* data) {
		if (count > 0) {
			builder.WithVertexAttribute((int)attribute, attributeFormats[attribute], attributeSizes[attribute]);
		}
	};

	atrributeFunc(VertexAttribute::Positions, GetPositionData().size(), (const char*)GetPositionData().data());
	atrributeFunc(VertexAttribute::Colours, GetColourData().size(), (const char*)GetColourData().data());
	atrributeFunc(VertexAttribute::TextureCoords, GetTextureCoordData().size(), (const char*)GetTextureCoordData().data());
	atrributeFunc(VertexAttribute::Normals, GetNormalData().size(), (const char*)GetNormalData().data());
	atrributeFunc(VertexAttribute::Tangents, GetTangentData().size(), (const char*)GetTangentData().data());
	atrributeFunc(VertexAttribute::JointWeights, GetSkinWeightData().size(), (const char*)GetSkinWeightData().data());
	atrributeFunc(VertexAttribute::JointIndices, GetSkinIndexData().size(), (const char*)GetSkinIndexData().data());

	atrributeFunc(VertexAttribute::General_Vec4, GetGeneralVec4Data().size(), (const char*)GetGeneralVec4Data().data());
	atrributeFunc(VertexAttribute::General_Integer, GetGeneralIntegerData().size(), (const char*)GetGeneralIntegerData().data());

	for(const SubMesh& sm : subMeshes) {
		builder.WithMeshRange(sm.start, sm.count, sm.base);
	}

	m_mesh = builder.Build();
}

//void VulkanMesh::DrawLayer(unsigned int layer, vk::CommandBuffer  to, int instanceCount) {
//	const SubMesh* sm = GetSubMesh(layer);
//	if (GetIndexCount() > 0) {
//		to.drawIndexed(sm->count, instanceCount, sm->start, sm->base, 0);
//	}
//	else {
//		to.draw(sm->count, instanceCount, sm->start, 0);
//	}
//}
//
//void VulkanMesh::Draw(vk::CommandBuffer  to, int instanceCount) {
//	if (GetIndexCount() > 0) {
//		to.drawIndexed(GetIndexCount(), instanceCount, 0, 0, 0);
//	}
//	else {
//		to.draw(GetVertexCount(), instanceCount, 0, 0);
//	}
//}
//
//void VulkanMesh::DrawAllLayers(vk::CommandBuffer  to, int instanceCount) {
//	if (GetIndexCount() > 0) {
//		for (int i = 0; i < subMeshes.size(); ++i) {
//			const SubMesh* sm = GetSubMesh(i);
//			to.drawIndexed(sm->count, instanceCount, sm->start, sm->base, 0);
//		}
//	}
//	else {
//		for (int i = 0; i < subMeshes.size(); ++i) {
//			const SubMesh* sm = GetSubMesh(i);
//			to.draw(sm->count, instanceCount, sm->start, 0);
//		}
//	}
//}

vk::PrimitiveTopology VulkanMesh::GetVulkanTopology() const {
	assert((uint32_t)primType < GeometryPrimitive::MAX_PRIM);

	const vk::PrimitiveTopology table[] = {
		vk::PrimitiveTopology::ePointList,
		vk::PrimitiveTopology::eLineList,
		vk::PrimitiveTopology::eTriangleList,
		vk::PrimitiveTopology::eTriangleFan,
		vk::PrimitiveTopology::eTriangleStrip,
		vk::PrimitiveTopology::ePatchList
	};
	return table[(uint32_t)primType];
}

uint32_t VulkanMesh::GetAttributeMask() const {
	return m_attributeMask;
}