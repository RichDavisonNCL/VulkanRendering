/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanCompute.h"
#include "Assets.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanCompute::VulkanCompute(vk::Device m_device, const std::string& filename, const std::string& entryPoint) : m_localThreadSize{ 0,0,0 } {
	char* data;
	size_t dataSize = 0;
	Assets::ReadBinaryFile(Assets::SHADERDIR + "VK/" + filename, &data, dataSize);

	if (dataSize > 0) {
		m_computeModule = m_device.createShaderModuleUnique(
			{
				.flags = {},
				.codeSize = dataSize,
				.pCode = (uint32_t*)data

			}	
		);
	}
	else {
		std::cout << __FUNCTION__ << " Problem loading shader file " << filename << "!\n";
		return;
	}
	m_entryPoint = entryPoint;

	m_createInfo.stage	= vk::ShaderStageFlagBits::eCompute;
	m_createInfo.module = *m_computeModule;
	m_createInfo.pName	= m_entryPoint.c_str();

	AddReflectionData(dataSize, data, vk::ShaderStageFlagBits::eCompute);
	BuildLayouts(m_device);

	delete data;
}

void	VulkanCompute::FillShaderStageCreateInfo(vk::ComputePipelineCreateInfo& pipeInfo) const {
	pipeInfo.setStage(m_createInfo);
}