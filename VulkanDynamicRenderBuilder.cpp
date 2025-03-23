/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanDynamicRenderBuilder.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

DynamicRenderBuilder::DynamicRenderBuilder() {
	usingStencil	= false;
	renderInfo.setLayerCount(1);
}

DynamicRenderBuilder& DynamicRenderBuilder::WithColourAttachment(vk::RenderingAttachmentInfoKHR const&  info) {
	colourAttachments.push_back(info);
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithDepthAttachment(vk::RenderingAttachmentInfoKHR const&  info) {
	depthAttachment = info;
	//TODO check stencil state, maybe in Build...
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithColourAttachment(
	vk::ImageView	texture, vk::ImageLayout m_layout, bool clear, vk::ClearValue clearValue
)
{
	vk::RenderingAttachmentInfoKHR colourAttachment;
	colourAttachment.setImageView(texture)
		.setImageLayout(m_layout)
		.setLoadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(clearValue);

	colourAttachments.push_back(colourAttachment);

	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithDepthAttachment(
	vk::ImageView	texture, vk::ImageLayout m_layout, bool clear, vk::ClearValue clearValue, bool withStencil
)
{
	depthAttachment.setImageView(texture)
		.setImageLayout(m_layout)
		.setLoadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setClearValue(clearValue);

	usingStencil = withStencil;

	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithRenderArea(vk::Rect2D area, bool useAutoViewstate) {
	renderInfo.setRenderArea(area);
	usingAutoViewstate = useAutoViewstate;
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithRenderArea(int32_t offsetX, int32_t offsetY, uint32_t extentX, uint32_t extentY, bool useAutoViewstate) {
	vk::Rect2D area = {
		.offset{offsetX, offsetY},
		.extent{extentX, extentY}
	};
	
	renderInfo.setRenderArea(area);
	usingAutoViewstate = useAutoViewstate;
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithRenderArea(vk::Offset2D offset, vk::Extent2D extent, bool useAutoViewstate) {
	vk::Rect2D area = { offset, extent };

	renderInfo.setRenderArea(area);
	usingAutoViewstate = useAutoViewstate;
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithLayerCount(int count) {
	renderInfo.setLayerCount(count);
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithRenderingFlags(vk::RenderingFlags flags) {
	renderInfo.setFlags(flags);
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithViewMask(uint32_t viewMask) {
	renderInfo.setViewMask(viewMask);
	return *this;
}

DynamicRenderBuilder& DynamicRenderBuilder::WithRenderInfo(vk::RenderingInfoKHR const& info) {
	renderInfo = info;
	return *this;
}

const vk::RenderingInfoKHR& DynamicRenderBuilder::Build() {
	renderInfo
		.setColorAttachments(colourAttachments)
		.setPDepthAttachment(&depthAttachment);

	if (usingStencil) {
		renderInfo.setPStencilAttachment(&depthAttachment);
	}
	return renderInfo;
}

void DynamicRenderBuilder::BeginRendering(vk::CommandBuffer m_cmdBuffer) {
	m_cmdBuffer.beginRendering(Build());
	if (usingAutoViewstate) {

		vk::Extent2D	extent		= renderInfo.renderArea.extent;
		vk::Viewport	viewport	= vk::Viewport(0.0f, (float)extent.height, (float)extent.width, -(float)extent.height, 0.0f, 1.0f);
		vk::Rect2D		scissor		= vk::Rect2D(vk::Offset2D(0, 0), extent);

		m_cmdBuffer.setViewport(0, 1, &viewport);
		m_cmdBuffer.setScissor( 0, 1, &scissor);
	}
}