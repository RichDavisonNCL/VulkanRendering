/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "VulkanShaderBase.h"

namespace NCL::Rendering::Vulkan {
	/*
	VulkanCompute: Represents a single computer shader object
	*/
	class VulkanCompute : public VulkanShaderBase	{
	public:
		VulkanCompute(vk::Device m_sourceDevice, const std::string& filename, const std::string& entryPoint = "main");
		~VulkanCompute() {}

		Maths::Vector3i GetThreadCount() const { return m_localThreadSize; }

		void	FillShaderStageCreateInfo(vk::ComputePipelineCreateInfo& info) const;

	protected:
		Maths::Vector3i						m_localThreadSize;
		vk::PipelineShaderStageCreateInfo	m_createInfo;
		vk::UniqueShaderModule				m_computeModule;
	};

	using UniqueVulkanCompute = std::unique_ptr<VulkanCompute>;
	using SharedVulkanCompute = std::shared_ptr<VulkanCompute>;
}