/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../NCLCoreClasses/Camera.h"
#include "../NCLCoreClasses/KeyboardMouseController.h"
#include "../NCLCoreClasses/TextureLoader.h"
#include "../NCLCoreClasses/Window.h"
#include "../VulkanRendering/VulkanMesh.h"
#include "../VulkanRendering/VulkanTexture.h"
#include "../VKQuick/Instance.h"

namespace NCL::Rendering::Vulkan {
	struct RenderObject {
		VulkanMesh*				mesh;
		Matrix4					transform;
		vk::UniqueDescriptorSet descriptorSet;
	};

	struct CameraState {
		vk::UniqueDescriptorSet descriptor;
		VKQuick::Buffer			buffer;
	};

	vk::TransformMatrixKHR ToVulkanMatrix(const NCL::Maths::Matrix4& mat4);

	class VulkanTutorial	{
	public:
		VulkanTutorial(VKQuick::VKQuickInitialisation& vkInit);
		virtual ~VulkanTutorial();

		virtual void Update(float dt) {
			m_runTime += dt;
			UpdateCamera(dt);
			UploadCameraUniform();
		}

		virtual void RunFrame(float dt);

		void Finish() const;

		void WindowEventHandler(NCL::WindowEvent e, uint32_t w, uint32_t h);

		static VulkanTutorial*		CreateTutorial(int& chainID, VKQuick::VKQuickInitialisation& vkInit);
		static VulkanTutorial*		CreateTutorial(const std::string& name, VKQuick::VKQuickInitialisation& vkInit);
		static VKQuick::VKQuickInitialisation DefaultInitialisation();

		const CameraState& GetCameraState(const VKQuick::FrameContext& context);

	protected:
		virtual void RenderFrame(float dt) = 0;
		virtual void OnWindowResize(uint32_t width, uint32_t height) {
			//Only tutorials that do off-screen rendering will care
		}
		void Initialise();

		void BuildCamera();
		void UpdateCamera(float dt);
		void UploadCameraUniform();

		void RenderSingleObject(RenderObject& o, vk::CommandBuffer  toBuffer, VKQuick::Pipeline& toPipeline, int descriptorSet = 0);

		UniqueVulkanMesh	LoadMesh(const std::string& filename, vk::BufferUsageFlags bufferUsage = {});

		void UploadMeshWait(VulkanMesh& m, vk::BufferUsageFlags bufferUsage = {});
		VKQuick::UniqueTexture LoadTexture(const std::string& filename);

		VKQuick::UniqueTexture LoadCubemap(
			const std::string& negativeXFile, const std::string& positiveXFile,
			const std::string& negativeYFile, const std::string& positiveYFile,
			const std::string& negativeZFile, const std::string& positiveZFile,
			const std::string& debugName = "CubeMap");

		UniqueVulkanMesh GenerateTriangle();
		UniqueVulkanMesh GenerateQuad();
		UniqueVulkanMesh GenerateGrid();

		VKQuick::VKQuickInitialisation	m_vkInit;
		VKQuick::Instance*				m_vkQuick;
		VKQuick::MemoryManager*			m_memoryManager = nullptr;

		KeyboardMouseController m_controller;
		PerspectiveCamera		m_camera;

		std::vector<CameraState>		m_cameraStates;

		vk::UniqueDescriptorSetLayout	m_cameraLayout;

		vk::UniqueSampler				m_defaultSampler;

		UniqueVulkanMesh	m_triangleMesh;
		UniqueVulkanMesh	m_quadMesh;
		UniqueVulkanMesh	m_gridMesh;
		UniqueVulkanMesh	m_cubeMesh;
		UniqueVulkanMesh	m_sphereMesh;

		float m_runTime;
	};



#define TUTORIAL_ENTRY(object) static VulkanTutorialEntry entry = VulkanTutorialEntry(#object, [](VKQuick::VKQuickInitialisation& vk) {return new object(vk); });

	using TutorialEntryFunc = std::function < VulkanTutorial*(VKQuick::VKQuickInitialisation&) >;
	class VulkanTutorialEntry {
	public:
		VulkanTutorialEntry(const std::string& s, TutorialEntryFunc f) {
			if (s_listStartPtr) {
				m_nodeChain		= s_listStartPtr;
				s_listStartPtr	= this;
			}
			else {
				s_listStartPtr	= this;
			}
			m_name			= s;
			m_creatorFunc	= f;
		}
		std::string				m_name;
		TutorialEntryFunc		m_creatorFunc;
		VulkanTutorialEntry*	m_nodeChain = nullptr;

		static VulkanTutorialEntry* s_listStartPtr;
	};
}