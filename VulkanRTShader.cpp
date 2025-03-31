/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanRTShader.h"
#include "Assets.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanRTShader::VulkanRTShader(const std::string& filename, vk::Device m_device) {
	char* data;
	size_t dataSize = 0;
	Assets::ReadBinaryFile(Assets::SHADERDIR + "VK/" + filename, &data, dataSize);

	if (dataSize > 0) {
		m_shaderModule = m_device.createShaderModuleUnique(
			{
				.flags = {},
				.codeSize = dataSize,
				.pCode = (uint32_t*)data
			}
		);
	}
	else {
		std::cout << __FUNCTION__ << " Problem loading shader file " << filename << "!\n";
	}

	AddReflectionData(dataSize, data, vk::ShaderStageFlagBits::eCompute);
	BuildLayouts(m_device);
	delete data;
}