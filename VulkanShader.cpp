/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanShader.h"
#include "VulkanDescriptorSetLayoutBuilder.h"
#include "Assets.h"
extern "C" {
#include "Spirv-reflect/Spirv_reflect.h"
}

using std::ifstream;

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;



VulkanShader::VulkanShader()	{
	m_stageCount	= 0;
}

VulkanShader::~VulkanShader()	{

}

void VulkanShader::ReloadShader() {

}

void VulkanShader::AddBinaryShaderModule(const std::string& fromFile, ShaderStages::Type stage, vk::Device m_device, const std::string& m_entryPoint) {
	char* data;
	size_t dataSize = 0;
	Assets::ReadBinaryFile(Assets::SHADERDIR + "VK/" + fromFile, &data, dataSize);

	if (dataSize > 0) {
		m_shaderModules[stage] = m_device.createShaderModuleUnique(
			{
				.flags = {},
				.codeSize = dataSize,
				.pCode = (uint32_t*)data
			}
			//vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), dataSize, (uint32_t*)data)
		);
	}
	else {
		std::cout << __FUNCTION__ << " Problem loading shader file " << fromFile << "!\n";
	}

	m_shaderFiles[stage] = fromFile;
	m_entryPoints[stage] = m_entryPoint;

	delete data;
}

void VulkanShader::AddBinaryShaderModule(ShaderStages::Type stage, vk::UniqueShaderModule& m_shaderModule, const std::string& m_entryPoint) {
	m_shaderModule.swap(m_shaderModules[stage]);
	m_entryPoints[stage]		= m_entryPoint;
}

void VulkanShader::Init() {
	m_stageCount = 0;
	for (int i = 0; i < ShaderStages::MAX_SIZE; ++i) {
		if (m_shaderModules[i]) {
			m_stageCount++;
		}
	}
	uint32_t doneCount = 0;
	for (int i = 0; i < ShaderStages::MAX_SIZE; ++i) {
		if (m_shaderModules[i]) {
			m_infos[doneCount].stage	= rasterStages[i];
			m_infos[doneCount].module = *m_shaderModules[i];
			m_infos[doneCount].pName	= m_entryPoints[i].c_str();

			doneCount++;
			if (doneCount >= m_stageCount) {
				break;
			}
		}
	}
}

void	VulkanShader::FillShaderStageCreateInfo(vk::GraphicsPipelineCreateInfo &info) const {
	info.setStageCount(m_stageCount); 
	info.setPStages(m_infos);
}