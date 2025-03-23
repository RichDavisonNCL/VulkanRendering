/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once

namespace NCL::Rendering::Vulkan {
	class VulkanTexture;

	extern vk::detail::DynamicLoader dynamicLoader;

	void SetNullDescriptor(vk::Device m_device, vk::DescriptorSetLayout m_layout);
	vk::DescriptorSetLayout GetNullDescriptor(vk::Device m_device);

	void SetDescriptorSizes(vk::Device, vk::PhysicalDeviceDescriptorBufferPropertiesEXT& props);

	template <typename T>
	uint64_t GetVulkanHandle(T const& cppHandle) {
		return uint64_t(static_cast<T::CType>(cppHandle));
	}
	bool MessageAssert(bool condition, const char* msg);
	void SetDebugName(vk::Device d, vk::ObjectType t, uint64_t handle, const std::string& debugName);

	void BeginDebugArea(vk::CommandBuffer buffer, const std::string& name);
	void EndDebugArea(vk::CommandBuffer buffer);

	class ScopedDebugArea {
	public:
		ScopedDebugArea(vk::CommandBuffer inBuffer, const std::string& inName) {
			Vulkan::BeginDebugArea(inBuffer, inName);
			m_cmdBuffer = inBuffer;
		}
		~ScopedDebugArea() {
			Vulkan::EndDebugArea(m_cmdBuffer);
		}
	private:
		vk::CommandBuffer m_cmdBuffer;
	};

	void ImageTransitionBarrier(vk::CommandBuffer  buffer, vk::Image i, vk::ImageMemoryBarrier2 barrier);
	void ImageTransitionBarrier(vk::CommandBuffer  buffer, vk::Image i, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect, vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage, uint32_t firstMip = 0, uint32_t mipCount = VK_REMAINING_MIP_LEVELS, uint32_t firstLayer = 0, uint32_t m_layerCount = VK_REMAINING_ARRAY_LAYERS);

	void TransitionUndefinedToColour(vk::CommandBuffer  buffer, vk::Image t);
	void TransitionColourToPresent(vk::CommandBuffer  buffer, vk::Image t);

	void TransitionColourToSampler(vk::CommandBuffer  buffer, vk::Image t);
	void TransitionDepthToSampler(vk::CommandBuffer  buffer, vk::Image t, bool doStencil = false);

	void TransitionSamplerToColour(vk::CommandBuffer  buffer, vk::Image t);
	void TransitionSamplerToDepth(vk::CommandBuffer  buffer, vk::Image t, bool doStencil = false);

	vk::AccessFlags	 DefaultAccessFlags(vk::ImageLayout forLayout);
	vk::AccessFlags2 DefaultAccessFlags2(vk::ImageLayout forLayout);

	vk::UniqueDescriptorSet CreateDescriptorSet(vk::Device m_device, vk::DescriptorPool m_pool, vk::DescriptorSetLayout  m_layout, uint32_t variableDescriptorCount = 0);

	vk::UniqueSemaphore		CreateTimelineSemaphore(vk::Device m_device, uint64_t initialValue = 0);
	vk::Result				TimelineSemaphoreHostWait(vk::Device device, vk::Semaphore semaphore, uint64_t waitVal, uint64_t waitTime);
	void					TimelineSemaphoreHostSignal(vk::Device device, vk::Semaphore semaphore, uint64_t signalVal);
	
	void					TimelineSemaphoreQueueWait(vk::Queue queue, vk::Semaphore semaphore, uint64_t waitVal, vk::PipelineStageFlags waitStage);
	void					TimelineSemaphoreQueueSignal(vk::Queue queue, vk::Semaphore semaphore, uint64_t signalVal);
	
	
	void	WriteDescriptor(vk::Device m_device, vk::WriteDescriptorSet setInfo, vk::DescriptorBufferInfo bufferInfo);
	void	WriteDescriptor(vk::Device m_device, vk::WriteDescriptorSet setInfo, vk::DescriptorImageInfo imageInfo);

	void	WriteBufferDescriptor(vk::Device m_device, vk::DescriptorSet set, uint32_t bindingSlot, vk::DescriptorType bufferType, vk::Buffer buff, size_t offset = 0, size_t range = VK_WHOLE_SIZE);
	void	WriteImageDescriptor(vk::Device m_device, vk::DescriptorSet set, uint32_t bindingSlot, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout m_layout = vk::ImageLayout::eShaderReadOnlyOptimal);
	void	WriteImageDescriptor(vk::Device m_device, vk::DescriptorSet set, uint32_t bindingSlot, uint32_t subIndex, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout m_layout = vk::ImageLayout::eShaderReadOnlyOptimal);
	void	WriteStorageImageDescriptor(vk::Device m_device, vk::DescriptorSet set, uint32_t bindingSlot, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout m_layout = vk::ImageLayout::eShaderReadOnlyOptimal);
	void	WriteTLASDescriptor(vk::Device m_device, vk::DescriptorSet set, uint32_t bindingSlot, vk::AccelerationStructureKHR tlas);

	vk::UniqueCommandBuffer	CmdBufferCreate(vk::Device m_device, vk::CommandPool fromPool, const std::string& debugName = "");
	vk::UniqueCommandBuffer	CmdBufferCreateBegin(vk::Device m_device, vk::CommandPool fromPool, const std::string& debugName = "");

	void	CmdBufferResetBegin(vk::CommandBuffer  buffer);
	void	CmdBufferResetBegin(const vk::UniqueCommandBuffer&  buffer);

	void	CmdBufferEndSubmit(vk::CommandBuffer  buffer, vk::Queue m_queue, vk::Fence fence = {}, vk::Semaphore waitSemaphore = {}, vk::Semaphore signalSempahore = {});
	void	CmdBufferEndSubmitWait(vk::CommandBuffer  buffer, vk::Device m_device, vk::Queue m_queue);
	void	CmdBufferEndSubmitWait(vk::CommandBuffer  buffer, vk::Device m_device, vk::Queue m_queue, vk::Fence fence);

	void WriteBufferDescriptor(vk::Device m_device,
		const vk::PhysicalDeviceDescriptorBufferPropertiesEXT& props,
		void* descriptorBufferMemory,
		vk::DescriptorSetLayout m_layout,
		size_t layoutIndex,
		vk::DeviceAddress bufferAddress,
		size_t bufferSize
	);

	size_t GetDescriptorSize(vk::DescriptorType type, const vk::PhysicalDeviceDescriptorBufferPropertiesEXT& props);

	void  UploadTextureData(vk::CommandBuffer  buffer, vk::Buffer tempBuffer, vk::Image image, vk::ImageLayout currentLyout, vk::ImageLayout endLayout, vk::BufferImageCopy copyInfo);
}