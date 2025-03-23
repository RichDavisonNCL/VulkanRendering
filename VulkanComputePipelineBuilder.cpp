/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanComputePipelineBuilder.h"
#include "VulkanCompute.h"
#include "VulkanUtils.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

ComputePipelineBuilder::ComputePipelineBuilder(vk::Device m_device) : PipelineBuilderBase(m_device){

}

ComputePipelineBuilder& ComputePipelineBuilder::WithShader(const UniqueVulkanCompute& compute) {
	compute->FillShaderStageCreateInfo(m_pipelineCreate);
	compute->FillDescriptorSetLayouts(m_reflectionLayouts);
	compute->FillPushConstants(m_allPushConstants);
	return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::WithShader(const VulkanCompute& compute) {
	compute.FillShaderStageCreateInfo(m_pipelineCreate);
	compute.FillDescriptorSetLayouts(m_reflectionLayouts);
	compute.FillPushConstants(m_allPushConstants);
	return *this;
}

VulkanPipeline	ComputePipelineBuilder::Build(const std::string& debugName, vk::PipelineCache cache) {
	VulkanPipeline output;

	FinaliseDescriptorLayouts();

	vk::PipelineLayoutCreateInfo pipeLayoutCreate = vk::PipelineLayoutCreateInfo()
		.setSetLayoutCount((uint32_t)m_allLayouts.size())
		.setPSetLayouts(m_allLayouts.data())
		.setPPushConstantRanges(m_allPushConstants.data())
		.setPushConstantRangeCount((uint32_t)m_allPushConstants.size());


	output.m_layout = m_sourceDevice.createPipelineLayoutUnique(pipeLayoutCreate);

	m_pipelineCreate.setLayout(*output.m_layout);


	output.pipeline = m_sourceDevice.createComputePipelineUnique(cache, m_pipelineCreate).value;

	if (!debugName.empty()) {
		SetDebugName(m_sourceDevice, vk::ObjectType::ePipeline, GetVulkanHandle(*output.pipeline), debugName);
	}

	return output;
}