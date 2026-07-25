/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanTexture.h"
#include "../VKQuick/MemoryManager.h"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanTexture::VulkanTexture(VKQuick::Texture& t) : m_texture(t){

}

VulkanTexture::~VulkanTexture()	{

}