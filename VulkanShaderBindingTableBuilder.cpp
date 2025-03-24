/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanShaderBindingTableBuilder.h"
#include "VulkanBufferBuilder.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanShaderBindingTableBuilder::VulkanShaderBindingTableBuilder(const std::string& inDebugName) {
	m_debugName = inDebugName;
}

VulkanShaderBindingTableBuilder& VulkanShaderBindingTableBuilder::WithProperties(vk::PhysicalDeviceRayTracingPipelinePropertiesKHR deviceProps) {
	m_properties = deviceProps;
	return *this;
}

VulkanShaderBindingTableBuilder& VulkanShaderBindingTableBuilder::WithPipeline(vk::Pipeline inPipe, const vk::RayTracingPipelineCreateInfoKHR& inPipeCreateInfo) {
	m_pipeline = inPipe;
	m_pipeCreateInfo = &inPipeCreateInfo;
	return *this;
}

VulkanShaderBindingTableBuilder& VulkanShaderBindingTableBuilder::WithLibrary(const vk::RayTracingPipelineCreateInfoKHR& createInfo) {
	m_libraries.push_back(&createInfo);
	return *this;
}

void VulkanShaderBindingTableBuilder::FillCounts(const vk::RayTracingPipelineCreateInfoKHR* fromInfo) {
	for (int i = 0; i < fromInfo->groupCount; ++i) {
		if (fromInfo->pGroups[i].type == vk::RayTracingShaderGroupTypeKHR::eGeneral) {
			int shaderType = fromInfo->pGroups[i].generalShader;

			if (fromInfo->pStages[shaderType].stage == vk::ShaderStageFlagBits::eRaygenKHR) {
				m_handleCounts[BindingTableOrder::RayGen]++;
			}
			else if (fromInfo->pStages[shaderType].stage == vk::ShaderStageFlagBits::eMissKHR) {
				m_handleCounts[BindingTableOrder::Miss]++;
			}
			else if(fromInfo->pStages[shaderType].stage == vk::ShaderStageFlagBits::eCallableKHR) {
				m_handleCounts[BindingTableOrder::Call]++;
			}
		}
		else { //Must be a hit group
			m_handleCounts[BindingTableOrder::Hit]++;
		}
	}
}

int MakeMultipleOf(int input, int multiple) {
	int count = input / multiple;
	int r = input % multiple;
	
	if (r != 0) {
		count += 1;
	}

	return count * multiple;
}

ShaderBindingTable VulkanShaderBindingTableBuilder::Build(vk::Device device, VmaAllocator m_allocator) {
	assert(m_pipeCreateInfo);
	assert(m_pipeline);

	ShaderBindingTable table;

	FillCounts(m_pipeCreateInfo); //Fills the handleIndices vectors

	uint32_t numShaderGroups = m_pipeCreateInfo->groupCount;
	for (auto& i : m_libraries) {
		numShaderGroups += i->groupCount;
		FillCounts(i);
	}

	uint32_t handleSize			= m_properties.shaderGroupHandleSize;
	uint32_t alignedHandleSize	= MakeMultipleOf(handleSize, m_properties.shaderGroupHandleAlignment);
	uint32_t totalHandleSize	= numShaderGroups * handleSize;

	std::vector<uint8_t> handles(totalHandleSize);
	auto result = device.getRayTracingShaderGroupHandlesKHR(m_pipeline, 0, numShaderGroups, totalHandleSize, handles.data());

	uint32_t bufferSize = 0;

	for (int i = 0; i < BindingTableOrder::MAX_SIZE; ++i) {
		table.regions[i].size	= MakeMultipleOf(alignedHandleSize * m_handleCounts[i], m_properties.shaderGroupBaseAlignment);
		table.regions[i].stride = alignedHandleSize;
		bufferSize += table.regions[i].size;
	}
	table.regions[0].stride = table.regions[0].size;

	table.tableBuffer = BufferBuilder(device, m_allocator)
		.WithBufferUsage(vk::BufferUsageFlagBits::eTransferSrc | 
						 vk::BufferUsageFlagBits::eShaderDeviceAddressKHR | 
						 vk::BufferUsageFlagBits::eShaderBindingTableKHR)
		.WithDeviceAddress()
		.WithHostVisibility()
		.Build(bufferSize, m_debugName + " SBT Buffer");

	vk::DeviceAddress bufferAddress = device.getBufferAddress({ .buffer = table.tableBuffer.buffer });

	char* bufferData = (char*)table.tableBuffer.Map();
	int dataOffset = 0;
	int currentHandleIndex = 0;
	for (int i = 0; i < BindingTableOrder::MAX_SIZE; ++i) { //For each group type
		int dataOffsetStart = dataOffset;
		table.regions[i].deviceAddress = bufferAddress + dataOffsetStart;
	
		for (int j = 0; j < m_handleCounts[i]; ++j) { //For entries in that group
			memcpy(bufferData + dataOffset, handles.data() + (currentHandleIndex++ * handleSize), handleSize);
			dataOffset += alignedHandleSize;
		}
		dataOffset = dataOffsetStart + table.regions[i].size;
	}

	table.tableBuffer.Unmap();

	return table;
}