/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanShaderBase.h"
extern "C" {
#include "Spirv-reflect/Spirv_reflect.h"
}
using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

void	VulkanShaderBase::FillDescriptorSetLayouts(std::vector<vk::DescriptorSetLayout>& layouts) const {
	layouts.resize(std::max(m_allLayouts.size(), layouts.size()));
	for (int i = 0; i < m_allLayouts.size(); ++i) {
		layouts[i] = *m_allLayouts[i];
	}
}

void	VulkanShaderBase::FillPushConstants(std::vector<vk::PushConstantRange>& constants) const {
	constants.clear();
	constants = m_pushConstants;
}

vk::DescriptorSetLayout VulkanShaderBase::GetLayout(uint32_t index) const {
	assert(index < m_allLayouts.size());
	assert(!m_allLayouts.empty());
	return *m_allLayouts[index];
}

std::vector<vk::DescriptorSetLayoutBinding> VulkanShaderBase::GetLayoutBinding(uint32_t index) const {
	if (index >= m_allLayoutsBindings.size()) {
		return {};
	}
	return m_allLayoutsBindings[index];
}

void VulkanShaderBase::AddDescriptorSetLayoutState(std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& data, std::vector<vk::UniqueDescriptorSetLayout>& layouts) {
	m_allLayoutsBindings = std::move(data);
	m_allLayouts = std::move(layouts);
}

void VulkanShaderBase::AddPushConstantState(std::vector<vk::PushConstantRange>& data) {
	m_pushConstants = data;
}

void VulkanShaderBase::AddReflectionData(uint32_t dataSize, const void* data, vk::ShaderStageFlags stage) {
	SpvReflectShaderModule module;
	SpvReflectResult result = spvReflectCreateShaderModule(dataSize, data, &module);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	uint32_t descriptorCount = 0;
	result = spvReflectEnumerateDescriptorSets(&module, &descriptorCount, NULL);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	std::vector<SpvReflectDescriptorSet*> descriptorSetLayouts(descriptorCount);
	result = spvReflectEnumerateDescriptorSets(&module, &descriptorCount, descriptorSetLayouts.data());
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	for (auto& set : descriptorSetLayouts) {
		if (set->set >= m_allLayoutsBindings.size()) {
			m_allLayoutsBindings.resize(set->set + 1);
		}
		std::vector<vk::DescriptorSetLayoutBinding>& setLayout = m_allLayoutsBindings[set->set];
		setLayout.resize(set->binding_count);
		for (int i = 0; i < set->binding_count; ++i) {
			SpvReflectDescriptorBinding* binding = set->bindings[i];

			uint32_t index = binding->binding;
			uint32_t count = binding->count;

			if (index >= setLayout.size()) {
				setLayout.resize(index + 1);
			}

			vk::ShaderStageFlags stages;

			if (setLayout[index].stageFlags != vk::ShaderStageFlags()) {
				//Check that something hasn't gone wrong with the binding combo!
				if (setLayout[index].descriptorType != (vk::DescriptorType)binding->descriptor_type) {

				}
				if (setLayout[index].descriptorCount != binding->count) {

				}
			}
			setLayout[index].binding = index;
			setLayout[index].descriptorCount = binding->count;
			setLayout[index].descriptorType = (vk::DescriptorType)binding->descriptor_type;

			setLayout[index].stageFlags |= stage; //Combine sets across shader stages
		}
	}

	uint32_t pushConstantCount = 0;
	result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, NULL);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	std::vector<SpvReflectBlockVariable*> pushConstantLayouts(pushConstantCount);
	result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstantLayouts.data());
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	for (auto& constant : pushConstantLayouts) {
		bool found = false;
		for (int i = 0; i < m_pushConstants.size(); ++i) {
			if (m_pushConstants[i].offset == constant->offset &&
				m_pushConstants[i].size == constant->size) {
				m_pushConstants[i].stageFlags |= stage;
				found = true;
				break;
			}
		}
		vk::PushConstantRange range;
		range.offset = constant->offset;
		range.size = constant->size;
		range.stageFlags = stage;
		m_pushConstants.push_back(range);
	}

	spvReflectDestroyShaderModule(&module);
}

void VulkanShaderBase::BuildLayouts(vk::Device device) {
	for (const auto& i : m_allLayoutsBindings) {
		vk::DescriptorSetLayoutCreateInfo createInfo;
		createInfo.setBindings(i);
		m_allLayouts.push_back(device.createDescriptorSetLayoutUnique(createInfo));
	}
}