/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "VulkanPipeline.h"
#include "VulkanUtils.h"


namespace NCL::Rendering::Vulkan {
	class VulkanRenderer;
	class VulkanShader;

	struct VulkanVertexSpecification;

	template <class T, class P>
	class PipelineBuilderBase	{
	public:

		T& WithLayout(vk::PipelineLayout pipeLayout) {
			m_layout = pipeLayout;
			m_pipelineCreate.setLayout(pipeLayout);
			return (T&)*this;
		}

		T& WithDescriptorSetLayout(uint32_t setIndex, vk::DescriptorSetLayout m_layout) {
			assert(setIndex < 32);
			if (setIndex >= m_userLayouts.size()) {
				vk::DescriptorSetLayout nullLayout = Vulkan::GetNullDescriptor(m_sourceDevice);
				while (m_userLayouts.size() <= setIndex) {
					m_userLayouts.push_back(nullLayout);
				}
			}
			m_userLayouts[setIndex] = m_layout;
			return (T&)*this;
		}

		T& WithDescriptorSetLayout(uint32_t setIndex, const vk::UniqueDescriptorSetLayout& m_layout) {
			return WithDescriptorSetLayout(setIndex, *m_layout);
		}

		T& WithDescriptorBuffers() {
			m_pipelineCreate.flags |= vk::PipelineCreateFlagBits::eDescriptorBufferEXT;
			return (T&)*this;
		}

		P& GetCreateInfo() {
			return m_pipelineCreate;
		}
	protected:
		PipelineBuilderBase(vk::Device m_device) {
			m_sourceDevice = m_device;
		}
		~PipelineBuilderBase() {}

		void FinaliseDescriptorLayouts() {
			m_allLayouts.clear();
			for (int i = 0; i < m_reflectionLayouts.size(); ++i) {
				if (m_userLayouts.size() > i && m_userLayouts[i] != Vulkan::GetNullDescriptor(m_sourceDevice)) {
					m_allLayouts.push_back(m_userLayouts[i]);
				}
				else {
					m_allLayouts.push_back(m_reflectionLayouts[i]);
				}
			}
		}

	protected:
		P m_pipelineCreate;
		vk::PipelineLayout	m_layout;
		vk::Device			m_sourceDevice;

		std::vector< vk::DescriptorSetLayout>	m_allLayouts;

		std::vector< vk::DescriptorSetLayout>	m_reflectionLayouts;
		std::vector< vk::DescriptorSetLayout>	m_userLayouts;

		std::vector< vk::PushConstantRange>		m_allPushConstants;
	};
}