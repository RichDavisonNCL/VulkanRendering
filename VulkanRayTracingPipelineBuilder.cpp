/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanRayTracingPipelineBuilder.h"

#include "VulkanMesh.h"
#include "VulkanUtils.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanRayTracingPipelineBuilder::VulkanRayTracingPipelineBuilder(vk::Device device) : PipelineBuilderBase(device){
}

VulkanRayTracingPipelineBuilder::~VulkanRayTracingPipelineBuilder() {
}

VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithRecursionDepth(uint32_t count) {
	m_pipelineCreate.maxPipelineRayRecursionDepth = count;
	return *this;
}

VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithRayGenGroup(uint32_t shaderIndex) {
	vk::RayTracingShaderGroupCreateInfoKHR groupCreateInfo;
	groupCreateInfo.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	groupCreateInfo.generalShader = shaderIndex;
	m_genGroups.push_back(groupCreateInfo);
	return *this;
}

VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithMissGroup(uint32_t shaderIndex) {
	vk::RayTracingShaderGroupCreateInfoKHR groupCreateInfo;
	groupCreateInfo.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	groupCreateInfo.generalShader = shaderIndex;
	m_missGroups.push_back(groupCreateInfo);
	return *this;
}

//VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithShaderGroup(const vk::RayTracingShaderGroupCreateInfoKHR& groupCreateInfo) {
//	shaderGroups.push_back(groupCreateInfo);
//	return *this;
//}
//
//VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithGeneralGroup(uint32_t index) {
//	vk::RayTracingShaderGroupCreateInfoKHR groupCreateInfo;
//
//	groupCreateInfo.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
//	groupCreateInfo.intersectionShader	= VK_SHADER_UNUSED_KHR;
//	groupCreateInfo.generalShader		= index;
//	groupCreateInfo.closestHitShader	= VK_SHADER_UNUSED_KHR;
//	groupCreateInfo.anyHitShader		= VK_SHADER_UNUSED_KHR;
//
//	shaderGroups.push_back(groupCreateInfo);
//
//	return *this;
//}

VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithTriangleHitGroup(uint32_t closestHit, uint32_t anyHit) {
	vk::RayTracingShaderGroupCreateInfoKHR groupCreateInfo;

	groupCreateInfo.type				= vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
	groupCreateInfo.generalShader		= VK_SHADER_UNUSED_KHR;
	groupCreateInfo.intersectionShader	= VK_SHADER_UNUSED_KHR;
	groupCreateInfo.closestHitShader	= closestHit;
	groupCreateInfo.anyHitShader		= anyHit;

	m_hitGroups.push_back(groupCreateInfo);

	return *this;
}

VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithProceduralHitGroup(uint32_t intersection, uint32_t closestHit, uint32_t anyHit){
	vk::RayTracingShaderGroupCreateInfoKHR groupCreateInfo;

	groupCreateInfo.type				= vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup;
	groupCreateInfo.generalShader		= VK_SHADER_UNUSED_KHR;
	groupCreateInfo.intersectionShader	= intersection;
	groupCreateInfo.closestHitShader	= closestHit;
	groupCreateInfo.anyHitShader		= anyHit;

	m_hitGroups.push_back(groupCreateInfo);

	return *this;
}

VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithShaderBinary(const std::string& filename, vk::ShaderStageFlagBits stage, const std::string& entrypoint) {
	m_loadedShaderModules.push_back(std::make_unique<VulkanShaderModule>(filename, stage, m_sourceDevice));
	m_usedModules.push_back(m_loadedShaderModules.back().get());
	m_moduleEntryPoints.push_back(entrypoint);

	return *this;
}


VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithShaderModule(const VulkanShaderModule& module, const std::string& entrypoint) {
	m_usedModules.push_back(&module);
	m_moduleEntryPoints.push_back(entrypoint);

	return *this;
}

//VulkanRayTracingPipelineBuilder& VulkanRayTracingPipelineBuilder::WithShader(VulkanRTShader& shader, vk::ShaderStageFlagBits stage, const string& entry) {
//	ShaderEntry entryInfo;
//	entryInfo.entryPoint = entry;
//	entryInfo.shader = &shader;
//	entryInfo.stage = stage;
//
//	m_entries.push_back(entryInfo);
//
//	//shader.FillDescriptorSetLayouts(m_reflectionLayouts);
//	//shader.FillPushConstants(m_pushConstants);
//	
//	return *this;
//}

VulkanPipeline VulkanRayTracingPipelineBuilder::Build(const std::string& debugName, vk::PipelineCache cache) {
	VulkanPipeline output;	
	
	FillShaderState(m_pipelineCreate, output);
	
	for (int i = 0; i < m_usedModules.size(); ++i) {
		vk::PipelineShaderStageCreateInfo stageInfo;

		stageInfo.pName = m_moduleEntryPoints[i].c_str();
		stageInfo.stage = m_usedModules[i]->m_shaderStage;
		stageInfo.module = *m_usedModules[i]->m_shaderModule;

		m_shaderStages.push_back(stageInfo);
	}

	m_allGroups.clear();
	m_allGroups.insert(m_allGroups.end(), m_genGroups.begin() , m_genGroups.end());
	m_allGroups.insert(m_allGroups.end(), m_missGroups.begin(), m_missGroups.end());
	m_allGroups.insert(m_allGroups.end(), m_hitGroups.begin() , m_hitGroups.end());

	m_pipelineCreate.groupCount	= m_allGroups.size();
	m_pipelineCreate.pGroups	= m_allGroups.data();

	//m_pipelineCreate.stageCount	= m_shaderStages.size();
	//m_pipelineCreate.pStages	= m_shaderStages.data();

	//vk::PipelineLayoutCreateInfo pipeLayoutCreate = vk::PipelineLayoutCreateInfo()
	//	.setSetLayouts(m_allLayouts)
	//	.setPushConstantRanges(m_pushConstants);


	//output.layout = m_sourceDevice.createPipelineLayoutUnique(pipeLayoutCreate);

	//m_pipelineCreate.layout = *output.layout;
	m_pipelineCreate.setPDynamicState(&m_dynamicCreate);

	output.pipeline = m_sourceDevice.createRayTracingPipelineKHRUnique({}, cache, m_pipelineCreate).value;

	if (!debugName.empty()) {
		SetDebugName(m_sourceDevice, vk::ObjectType::ePipeline		, GetVulkanHandle(*output.pipeline)	, debugName);
		SetDebugName(m_sourceDevice, vk::ObjectType::ePipelineLayout, GetVulkanHandle(*output.layout)	, debugName);
	}

	return output;
}