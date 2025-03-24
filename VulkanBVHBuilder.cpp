/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanBVHBuilder.h"

#include "VulkanBufferBuilder.h"
#include "VulkanMesh.h"
#include "VulkanUtils.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanBVHBuilder::VulkanBVHBuilder() {
}

VulkanBVHBuilder::~VulkanBVHBuilder() {
}

VulkanBVHBuilder& VulkanBVHBuilder::WithObject(VulkanMesh* m, const Matrix4& transform, uint32_t mask, uint32_t hitID) {
	auto savedMesh = m_uniqueMeshes.find(m);

	uint32_t meshID = 0;

	if (savedMesh == m_uniqueMeshes.end()) {
		m_meshes.push_back(m);
		meshID = m_meshes.size() - 1;
		m_uniqueMeshes.insert({m, meshID});
	}
	else {
		meshID = savedMesh->second;
	}

	VulkanBVHEntry entry;
	entry.modelMat	= transform;
	entry.meshID	= meshID;
	entry.hitID		= hitID;
	entry.mask		= mask;

	m_entries.push_back(entry);

	return *this;
}

VulkanBVHBuilder& VulkanBVHBuilder::WithCommandQueue(vk::Queue inQueue) {
	m_queue = inQueue;
	return *this;
}

VulkanBVHBuilder& VulkanBVHBuilder::WithCommandPool(vk::CommandPool inPool) {
	m_pool = inPool;
	return *this;
}

VulkanBVHBuilder& VulkanBVHBuilder::WithDevice(vk::Device inDevice) {
	m_sourceDevice = inDevice;
	return *this;
}

VulkanBVHBuilder& VulkanBVHBuilder::WithAllocator(VmaAllocator inAllocator) {
	m_sourceAllocator = inAllocator;
	return *this;
}

vk::UniqueAccelerationStructureKHR VulkanBVHBuilder::Build(vk::BuildAccelerationStructureFlagsKHR inFlags, const std::string& debugName) {
	BuildBLAS(m_sourceDevice, m_sourceAllocator, inFlags);
	BuildTLAS(m_sourceDevice, m_sourceAllocator, inFlags);

	if (!debugName.empty()) {
		SetDebugName(m_sourceDevice, vk::ObjectType::eAccelerationStructureKHR, GetVulkanHandle(*m_tlas), debugName);
	}

	return std::move(m_tlas);
}

void VulkanBVHBuilder::BuildBLAS(vk::Device device, VmaAllocator m_allocator, vk::BuildAccelerationStructureFlagsKHR inFlags) {
	//We need to first create the BLAS entries for the unique meshes
	for (const auto& i : m_meshes) {
		vk::Buffer	vBuffer;
		uint32_t	vOffset;
		uint32_t	vRange;
		vk::Format	vFormat;
		i->GetAttributeInformation(NCL::VertexAttribute::Positions, vBuffer, vOffset, vRange, vFormat);

		vk::Buffer		iBuffer;
		uint32_t		iOffset;
		uint32_t		iRange;
		vk::IndexType	iFormat;
		bool hasIndices = i->GetIndexInformation(iBuffer, iOffset, iRange, iFormat);

		//vk::AccelerationStructureGeometryTrianglesDataKHR triData;
		//triData.vertexFormat = vFormat;
		//triData.vertexData.deviceAddress = device.getBufferAddress(vk::BufferDeviceAddressInfo(vBuffer)) + vOffset;
		//triData.vertexStride = sizeof(Vector3);

		vk::AccelerationStructureGeometryTrianglesDataKHR triData;
		triData.vertexFormat = vFormat;
		triData.vertexData.deviceAddress = device.getBufferAddress({.buffer = vBuffer }) + vOffset;
		triData.vertexStride = sizeof(Vector3);

		if (hasIndices) {
			triData.indexType = iFormat;
			triData.indexData.deviceAddress = device.getBufferAddress({ .buffer = iBuffer} ) + iOffset;
		}

		triData.maxVertex = i->GetVertexCount();

		m_blasBuildInfo.resize(m_blasBuildInfo.size() + 1);

		BLASEntry& blasEntry = m_blasBuildInfo.back();

		size_t subMeshCount = i->GetSubMeshCount();

		blasEntry.buildInfo.geometryCount	= subMeshCount;

		blasEntry.geometries.resize(subMeshCount);
		blasEntry.ranges.resize(subMeshCount);
		blasEntry.maxPrims.resize(subMeshCount);

		for (int j = 0; j < subMeshCount; ++j) {
			const SubMesh* m = i->GetSubMesh(j);

			blasEntry.geometries[j].setGeometryType(vk::GeometryTypeKHR::eTriangles)
												.setFlags(vk::GeometryFlagBitsKHR::eOpaque)
												.geometry.setTriangles(triData);

			blasEntry.geometries[j].geometry.triangles.maxVertex = m->count;

			blasEntry.ranges[j].primitiveCount	= i->GetPrimitiveCount(j);
			blasEntry.ranges[j].firstVertex		= m->base;
			blasEntry.ranges[j].primitiveOffset = m->start *(iFormat == vk::IndexType::eUint32 ? 4 : 2);
			blasEntry.maxPrims[j] = i->GetPrimitiveCount(j); 
		}
	}

	vk::DeviceSize totalSize	= 0;
	vk::DeviceSize scratchSize	= 0;

	for (auto& i : m_blasBuildInfo) {	//Go through each of the added entries to build up data...
		i.buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		i.buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		i.buildInfo.geometryCount	= i.geometries.size(); //TODO
		i.buildInfo.pGeometries		= i.geometries.data();
		i.buildInfo.flags |= inFlags;

		device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
			&i.buildInfo, i.maxPrims.data(), &i.sizeInfo);

		totalSize	+= i.sizeInfo.accelerationStructureSize;
		scratchSize  = std::max(scratchSize, i.sizeInfo.buildScratchSize);
	}

	VulkanBuffer scratchBuff = BufferBuilder(device, m_allocator)
		.WithBufferUsage(	vk::BufferUsageFlagBits::eShaderDeviceAddress | 
							vk::BufferUsageFlagBits::eStorageBuffer)
		.Build(scratchSize, "Scratch Buffer");

	vk::DeviceAddress scratchAddr = device.getBufferAddress({ .buffer = scratchBuff.buffer });

	vk::UniqueCommandBuffer buffer = CmdBufferCreateBegin(device, m_pool, "Making BLAS");

	for (auto& i : m_blasBuildInfo) {		//Make the buffer for each blas entry...
		vk::AccelerationStructureCreateInfoKHR createInfo;
		createInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		createInfo.size = i.sizeInfo.accelerationStructureSize;

		i.buffer = BufferBuilder(device, m_allocator)
			.WithBufferUsage(	vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | 
								vk::BufferUsageFlagBits::eShaderDeviceAddress)
			.WithHostVisibility()
			.Build(createInfo.size);

		createInfo.buffer = i.buffer;

		i.accelStructure = device.createAccelerationStructureKHRUnique(createInfo);

		i.buildInfo.dstAccelerationStructure	= *i.accelStructure;
		i.buildInfo.scratchData.deviceAddress	= scratchAddr;

		const vk::AccelerationStructureBuildRangeInfoKHR* rangeInfo = i.ranges.data();

		buffer->buildAccelerationStructuresKHR(1, &i.buildInfo, &rangeInfo);
					
		buffer->pipelineBarrier(
			vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, //Source
			vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, //Dest
			{}, //Dependency Flags
			{//MemoryBarriers
				{
					.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
					.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR
				}
				},
			{}, //bufferMemoryBarriers
			{} //imageMemoryBarriers
		);
	}
	CmdBufferEndSubmitWait(*buffer, device, m_queue);
}

void VulkanBVHBuilder::BuildTLAS(vk::Device device, VmaAllocator m_allocator, vk::BuildAccelerationStructureFlagsKHR flags) {
	std::vector<vk::AccelerationStructureInstanceKHR> tlasEntries;

	const uint32_t instanceCount = m_entries.size();

	tlasEntries.resize(instanceCount);

	for (int i = 0; i < instanceCount; ++i) {
		//VkTransformMatrixKHR has 3 rows of 4 columns each. upper 3x3 remains rotation
		//Row major memory layout! Matrix class is column major...
		tlasEntries[i].transform.matrix[0][0] = m_entries[i].modelMat.array[0][0];
		tlasEntries[i].transform.matrix[0][1] = m_entries[i].modelMat.array[1][0];
		tlasEntries[i].transform.matrix[0][2] = m_entries[i].modelMat.array[2][0];
		tlasEntries[i].transform.matrix[0][3] = m_entries[i].modelMat.array[3][0];

		tlasEntries[i].transform.matrix[1][0] = m_entries[i].modelMat.array[0][1];
		tlasEntries[i].transform.matrix[1][1] = m_entries[i].modelMat.array[1][1];
		tlasEntries[i].transform.matrix[1][2] = m_entries[i].modelMat.array[2][1];
		tlasEntries[i].transform.matrix[1][3] = m_entries[i].modelMat.array[3][1];

		tlasEntries[i].transform.matrix[2][0] = m_entries[i].modelMat.array[0][2];
		tlasEntries[i].transform.matrix[2][1] = m_entries[i].modelMat.array[1][2];
		tlasEntries[i].transform.matrix[2][2] = m_entries[i].modelMat.array[2][2];
		tlasEntries[i].transform.matrix[2][3] = m_entries[i].modelMat.array[3][2];

		uint32_t meshID = m_entries[i].meshID;

		tlasEntries[i].instanceCustomIndex = meshID;

		tlasEntries[i].accelerationStructureReference = device.getBufferAddress({ .buffer = m_blasBuildInfo[meshID].buffer.buffer });


		tlasEntries[i].flags = (VkGeometryInstanceFlagBitsKHR)vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable;
		tlasEntries[i].mask = m_entries[i].mask;
		tlasEntries[i].instanceShaderBindingTableRecordOffset = m_entries[i].hitID;
	}

	size_t dataSize = instanceCount * sizeof(vk::AccelerationStructureInstanceKHR);

	VulkanBuffer instanceBuffer = BufferBuilder(device, m_allocator)
		.WithBufferUsage(	vk::BufferUsageFlagBits::eShaderDeviceAddress | 
							vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
		.WithHostVisibility()
		.Build(dataSize, "Instance Buffer");

	instanceBuffer.CopyData(tlasEntries.data(), dataSize);

	vk::AccelerationStructureGeometryKHR tlasGeometry;
	tlasGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
	tlasGeometry.geometry = vk::AccelerationStructureGeometryInstancesDataKHR();
	tlasGeometry.geometry.instances.data = device.getBufferAddress({ .buffer = instanceBuffer.buffer });

	vk::AccelerationStructureBuildGeometryInfoKHR geomInfo;
	geomInfo.flags			= flags;
	geomInfo.geometryCount	= 1;
	geomInfo.pGeometries	= &tlasGeometry;
	geomInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	geomInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	geomInfo.srcAccelerationStructure = nullptr; //??

	vk::AccelerationStructureBuildSizesInfoKHR sizesInfo;
	device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &geomInfo, &instanceCount, &sizesInfo);

	m_tlasBuffer = BufferBuilder(device, m_allocator)
		.WithDeviceAddress()
		.WithBufferUsage(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR)
		.Build(sizesInfo.accelerationStructureSize, "TLAS Buffer");

	vk::AccelerationStructureCreateInfoKHR tlasCreateInfo;
	tlasCreateInfo.buffer = m_tlasBuffer.buffer;
	tlasCreateInfo.size = sizesInfo.accelerationStructureSize;
	tlasCreateInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	
	m_tlas = device.createAccelerationStructureKHRUnique(tlasCreateInfo);

	VulkanBuffer scratchBuffer = BufferBuilder(device, m_allocator)
		.WithBufferUsage(	vk::BufferUsageFlagBits::eShaderDeviceAddress | 
							vk::BufferUsageFlagBits::eStorageBuffer)
		.WithHostVisibility()
		.WithDeviceAddress()
		.Build(sizesInfo.buildScratchSize, "Scratch Buffer");

	vk::DeviceAddress scratchAddr = device.getBufferAddress({ .buffer = scratchBuffer.buffer });

	geomInfo.srcAccelerationStructure = nullptr;
	geomInfo.dstAccelerationStructure = *m_tlas;
	geomInfo.scratchData.deviceAddress = scratchAddr;

	vk::AccelerationStructureBuildRangeInfoKHR rangeInfo;
	rangeInfo.primitiveCount = instanceCount;

	vk::AccelerationStructureBuildRangeInfoKHR* rangeInfoPtr = &rangeInfo;

	vk::UniqueCommandBuffer m_cmdBuffer = CmdBufferCreateBegin(device, m_pool, "Making TLAS");
	m_cmdBuffer->buildAccelerationStructuresKHR(1, &geomInfo, &rangeInfoPtr);
	CmdBufferEndSubmitWait(*m_cmdBuffer, device, m_queue);
}