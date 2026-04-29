/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanComputePipelineBuilder.h"
#include "VulkanUtils.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

ComputePipelineBuilder::ComputePipelineBuilder(vk::Device device) : PipelineBuilderBase(device){
}

ComputePipelineBuilder& ComputePipelineBuilder::WithShaderBinary(const std::string& filename, const std::string& entrypoint) {
	m_loadedShaderModules.push_back(std::make_unique<VulkanShaderModule>(filename, vk::ShaderStageFlagBits::eCompute, m_sourceDevice));
	m_usedModules.push_back(m_loadedShaderModules.back().get());
	m_entryPoint	= entrypoint;
	return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::WithShaderModule(const VulkanShaderModule& module, const std::string& entrypoint) {
	m_usedModules.push_back(&module);
	m_entryPoint	= entrypoint;
	return *this;
}

VulkanPipeline	ComputePipelineBuilder::Build(const std::string& debugName, vk::PipelineCache cache) {
	VulkanPipeline output;
	assert(!m_usedModules.empty());

	FillShaderLayouts(output);

	vk::PipelineShaderStageCreateInfo	m_createInfo;
	m_createInfo.stage	= vk::ShaderStageFlagBits::eCompute;
	m_createInfo.module = *m_usedModules[0]->m_shaderModule;
	m_createInfo.pName	= m_entryPoint.c_str();

	m_pipelineCreate.setLayout(*output.layout);
	m_pipelineCreate.setStage(m_createInfo);

	vk::PipelineCreateFlags2CreateInfo pipeFlags;
	if (m_pipelineCreateBits) {
		pipeFlags.flags = m_pipelineCreateBits;
		m_pipelineCreate.pNext = &pipeFlags;
	}

	output.pipeline = m_sourceDevice.createComputePipelineUnique(cache, m_pipelineCreate).value;

	if (!debugName.empty()) {
		SetDebugName(m_sourceDevice, *output.pipeline, debugName);
	}

	return output;
}