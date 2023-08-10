/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once

namespace NCL::Rendering::Vulkan {
	class VulkanTexture;
	template <typename T>
	uint64_t GetVulkanHandle(T const& cppHandle) {
		return uint64_t(static_cast<T::CType>(cppHandle));
	}

	void SetDebugName(vk::Device d, vk::ObjectType t, uint64_t handle, const std::string& debugName);

	void BeginDebugArea(vk::CommandBuffer buffer, const std::string& name);
	void EndDebugArea(vk::CommandBuffer buffer);

	void ImageTransitionBarrier(vk::CommandBuffer  buffer, vk::Image i, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, int mipLevel = 0, int layer = 0);

	void TransitionPresentToColour(vk::CommandBuffer  buffer, vk::Image t);
	void TransitionColourToPresent(vk::CommandBuffer  buffer, vk::Image t);

	void TransitionColourToSampler(vk::CommandBuffer  buffer, vk::Image t);
	void TransitionDepthToSampler(vk::CommandBuffer  buffer, vk::Image t, bool doStencil = false);

	void TransitionSamplerToColour(vk::CommandBuffer  buffer, vk::Image t);
	void TransitionSamplerToDepth(vk::CommandBuffer  buffer, vk::Image t, bool doStencil = false);

	void DispatchCompute(vk::CommandBuffer  buffer, unsigned int xCount, unsigned int yCount = 1, unsigned int zCount = 1);

	bool MessageAssert(bool condition, const char* msg);

	vk::UniqueDescriptorSet BuildUniqueDescriptorSet(vk::Device device, vk::DescriptorSetLayout  layout, vk::DescriptorPool pool, uint32_t variableDescriptorCount = 0);



	void	WriteBufferDescriptor(vk::Device device, vk::DescriptorSet set, int bindingSlot, vk::DescriptorType bufferType, vk::Buffer buff, size_t offset = 0, size_t range = 0);

	void	WriteImageDescriptor(vk::Device device, vk::DescriptorSet set, int bindingNum, int subIndex, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

	void	WriteStorageImageDescriptor(vk::Device device, vk::DescriptorSet set, int bindingNum, int subIndex, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
	void	WriteTLASDescriptor(vk::Device device, vk::DescriptorSet set, int bindingSlot, vk::AccelerationStructureKHR tlas);



	vk::UniqueCommandBuffer	CmdBufferCreate(vk::Device device, vk::CommandPool fromPool, const std::string& debugName = "");
	vk::UniqueCommandBuffer	CmdBufferBegin(vk::Device device, vk::CommandPool fromPool, const std::string& debugName = "");

	void	CmdBufferResetBegin(vk::CommandBuffer  buffer);
	void	CmdBufferResetBegin(const vk::UniqueCommandBuffer&  buffer);

	void	CmdBufferEndSubmit(vk::CommandBuffer  buffer, vk::Queue queue, vk::Fence fence = {}, vk::Semaphore waitSemaphore = {}, vk::Semaphore signalSempahore = {});
	void	CmdBufferEndSubmitWait(vk::CommandBuffer  buffer, vk::Device device, vk::Queue queue);

	extern vk::DynamicLoader dynamicLoader;
	void SetNullDescriptor(vk::Device device, vk::DescriptorSetLayout layout);
	extern std::map<vk::Device, vk::DescriptorSetLayout > nullDescriptors;
}