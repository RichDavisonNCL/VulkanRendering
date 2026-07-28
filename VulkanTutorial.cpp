/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "VulkanTutorial.h"
#ifdef _WIN32
#include "../NCLCoreClasses/Win32Window.h"
#endif

#include "../VKQuick/Utils.h"
#include "../VKQuick/VMAMemoryManager.h"
#include "../VKQuick/TextureBuilder.h"
#include "../VKQuick/DescriptorSetLayoutBuilder.h"
#include "../VKQuick/Texture.h"

#include "MshLoader.h"

#include "../GLTFLoader/GLTFLoader.h"

#include "Shaders/VK/glslInterop.h"
#include "Shaders/VK/Camera.glslh"

using namespace NCL;
using namespace Rendering;
using namespace Vulkan;

VulkanTutorialEntry* VulkanTutorialEntry::s_listStartPtr = nullptr;

VulkanTutorial::VulkanTutorial(VKQuick::VKQuickInitialisation& vkInit) : m_controller(*Window::GetWindow()->GetKeyboard(), *Window::GetWindow()->GetMouse()) {
	m_runTime	= 0.0f;
	m_vkInit	= vkInit;

	VKQuick::TextureLoadFunction tlf = [](const std::string& filename) -> VKQuick::LoadedTexture {
		VKQuick::LoadedTexture lt;
		uint32_t flags = 0;
		TextureLoader::LoadTexture(filename, lt.texData, lt.dimensions.width, lt.dimensions.height, lt.channels, flags);
		return lt;
	};

	VKQuick::TextureLoadReleaseFunction trf = [](VKQuick::LoadedTexture& texture) -> void {
		TextureLoader::DeleteTextureData(texture.texData);
	};

	VKQuick::TextureBuilder::SetFileHandlingFunctions(tlf, trf);

	GLTFLoader::SetTextureConstructionFunction(
		[&](std::string& input) ->  SharedTexture {
			VulkanTexture* texture = new VulkanTexture(*(LoadTexture(input).release()));
			return std::shared_ptr<Texture>(texture);
		}
	);

	GLTFLoader::SetMeshConstructionFunction(
		[&]()-> std::shared_ptr<Mesh> {return std::shared_ptr<Mesh>(new VulkanMesh()); }
	);
}

VulkanTutorial::~VulkanTutorial() {
	m_vkQuick->GetDevice().waitIdle();
	for (auto& state : m_cameraStates) {
		state.descriptor.reset();
		m_memoryManager->DiscardBuffer(state.buffer, VKQuick::DiscardMode::Immediate);
	}
	m_cameraLayout.reset();
	m_defaultSampler.reset();

	m_triangleMesh.reset();
	m_quadMesh.reset();
	m_gridMesh.reset();
	m_cubeMesh.reset();
	m_sphereMesh.reset();

	delete m_memoryManager;
	delete m_vkQuick;
}

void VulkanTutorial::Finish() const {
	m_vkQuick->GetDevice().waitIdle();
}

void VulkanTutorial::Initialise() {
#ifdef _WIN32
	Win32Code::Win32Window* hostWindow = (Win32Code::Win32Window*)Window::GetWindow();
	m_vkInit.win32Handle	= hostWindow->GetHandle();
	m_vkInit.win32Instance = hostWindow->GetInstance();
#endif

	m_vkInit.initialWidth	= hostWindow->GetScreenSize().x;
	m_vkInit.initialHeight	= hostWindow->GetScreenSize().y;

	m_vkQuick		= new VKQuick::Instance(m_vkInit);
	m_memoryManager = new VKQuick::VMAMemoryManager(m_vkQuick->GetDevice(), m_vkQuick->GetPhysicalDevice(), m_vkQuick->GetVulkanInstance(), m_vkInit);
	BuildCamera();

	VKQuick::FrameContext const& context = m_vkQuick->GetFrameContext();

	vk::Device device = context.device;

	m_defaultSampler = device.createSamplerUnique(
		vk::SamplerCreateInfo()
		.setAnisotropyEnable(false)
		.setMaxAnisotropy(16)
		.setMinFilter(vk::Filter::eLinear)
		.setMagFilter(vk::Filter::eLinear)
		.setMipmapMode(vk::SamplerMipmapMode::eLinear)
		.setMaxLod(80.0f)
	);

	m_cameraLayout = VKQuick::DescriptorSetLayoutBuilder(device)
		.WithUniformBuffers(0, 1, vk::ShaderStageFlagBits::eAll)
		.Build("CameraMatrices"); //Get our m_camera matrices...

	m_cameraStates.resize(m_vkInit.framesInFlight);

	for (auto& state : m_cameraStates) {
		state.descriptor	= VKQuick::CreateDescriptorSet(device, context.descriptorPool, *m_cameraLayout);
		state.buffer		= m_memoryManager->CreateBuffer(
			{
				.size	= sizeof(ShaderCamera),
				.usage	= vk::BufferUsageFlagBits::eUniformBuffer,
			},
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent,
			"Camera Buffer"
		);

		WriteBufferDescriptor(device, *state.descriptor, 0, vk::DescriptorType::eUniformBuffer, state.buffer);
	}

	m_triangleMesh	= GenerateTriangle();
	m_quadMesh		= GenerateQuad();
	m_gridMesh		= GenerateGrid();
	m_cubeMesh		= LoadMesh("Cube.msh");
	m_sphereMesh	= LoadMesh("Sphere.msh");
}

void VulkanTutorial::BuildCamera() {
	m_camera.SetFieldOfVision(45.0f)
		.SetNearPlane(0.1f)
		.SetFarPlane(1000.0f);
	
	m_camera.SetController(m_controller);

	m_controller.MapAxis(0, "Sidestep");
	m_controller.MapAxis(1, "UpDown");
	m_controller.MapAxis(2, "Forward");

	m_controller.MapAxis(3, "XLook");
	m_controller.MapAxis(4, "YLook");
}

const CameraState& VulkanTutorial::GetCameraState(const VKQuick::FrameContext& context) {
	return m_cameraStates[context.cycleID];
}

void VulkanTutorial::UploadCameraUniform() {
	VKQuick::FrameContext const& context = m_vkQuick->GetFrameContext();

	CameraState& s = m_cameraStates[context.cycleID];

	ShaderCamera* shaderCam = s.buffer.Map<ShaderCamera>();

	shaderCam->viewMatrix	= m_camera.BuildViewMatrix();
	shaderCam->projMatrix	= m_camera.BuildProjectionMatrix(Window::GetWindow()->GetScreenAspect());
	shaderCam->position		= m_camera.GetPosition();

	s.buffer.Unmap();
}

void VulkanTutorial::UpdateCamera(float dt) {
	m_controller.Update(dt);
	m_camera.UpdateCamera(dt);
}

void VulkanTutorial::RunFrame(float dt) {
	if (Window::GetWindow()->IsMinimised()) {
		return;
	}	
	m_vkQuick->BeginFrame();

	Update(dt);

	m_memoryManager->Update();

	UploadCameraUniform();
	RenderFrame(dt);
	m_vkQuick->EndFrame();
	m_vkQuick->SwapBuffers();
};

void VulkanTutorial::WindowEventHandler(WindowEvent e, uint32_t w, uint32_t h) {
	if (e == WindowEvent::Minimize || e == WindowEvent::Maximize) {
		m_vkQuick->OnWindowMinimise(e == WindowEvent::Minimize ? true : false);
	}
	if (e == WindowEvent::Resize || e == WindowEvent::Maximize) {
		m_vkQuick->OnWindowResize(w, h);
		OnWindowResize(w, h);
	}
}

UniqueVulkanMesh VulkanTutorial::GenerateTriangle() {
	VulkanMesh* triMesh = new VulkanMesh();
	triMesh->SetVertexPositions({ Vector3(-1,-1,0), Vector3(1,-1,0), Vector3(0,1,0) });
	triMesh->SetVertexColours({ Vector4(1,0,0,1), Vector4(0,1,0,1), Vector4(0,0,1,1) });
	triMesh->SetVertexTextureCoords({ Vector2(0,0), Vector2(1,0), Vector2(0.5, 1) });
	triMesh->SetVertexIndices({ 0,1,2 });

	triMesh->SetDebugName("Triangle");
	triMesh->SetPrimitiveType(NCL::GeometryPrimitive::Triangles);

	UploadMeshWait(*triMesh);

	return UniqueVulkanMesh(triMesh);
}

UniqueVulkanMesh VulkanTutorial::GenerateQuad() {
	VulkanMesh* quadMesh = new VulkanMesh();
	quadMesh->SetVertexPositions({ Vector3(-1,-1,0), Vector3(1,-1,0), Vector3(1,1,0), Vector3(-1,1,0) });
	quadMesh->SetVertexTextureCoords({ Vector2(0,1), Vector2(1,1), Vector2(1, 0), Vector2(0, 0) });
	quadMesh->SetVertexIndices({ 0,1,3,2 });
	quadMesh->SetDebugName("Fullscreen Quad");
	quadMesh->SetPrimitiveType(NCL::GeometryPrimitive::TriangleStrip);

	UploadMeshWait(*quadMesh);

	return UniqueVulkanMesh(quadMesh);
}

UniqueVulkanMesh VulkanTutorial::GenerateGrid() {
	VulkanMesh* gridMesh = new VulkanMesh();
	gridMesh->SetVertexPositions({ Vector3(-1,-1,0), Vector3(1,-1,0), Vector3(1,1,0), Vector3(-1,1,0) });
	gridMesh->SetVertexTextureCoords({ Vector2(0,0), Vector2(1,0), Vector2(1, 1), Vector2(0, 1) });
	gridMesh->SetVertexIndices({ 0,1,3,2 });
	gridMesh->SetDebugName("Test Grid");
	gridMesh->SetPrimitiveType(NCL::GeometryPrimitive::TriangleStrip);

	UploadMeshWait(*gridMesh);

	return UniqueVulkanMesh(gridMesh);
}

UniqueVulkanMesh VulkanTutorial::LoadMesh(const std::string& filename, vk::BufferUsageFlags flags) {
	VulkanMesh* newMesh = new VulkanMesh();

	MshLoader::LoadMesh(filename, *newMesh);
	UploadMeshWait(*newMesh, flags);
	return UniqueVulkanMesh(newMesh);
}

void VulkanTutorial::UploadMeshWait(VulkanMesh& m, vk::BufferUsageFlags flags) {
	VKQuick::FrameContext const& context = m_vkQuick->GetFrameContext();

	vk::UniqueCommandBuffer cmdBuffer = VKQuick::CmdBufferCreateBegin(context.device, context.commandPools[VKQuick::CommandType::Graphics], "VulkanMesh upload");

	m.InitialiseGPUState(context.device, *m_memoryManager, flags);

	m.UploadAttributes(*cmdBuffer);

	VKQuick::CmdBufferSubmit(
		{
			.buffer = *cmdBuffer,
			.queue = context.queues[VKQuick::CommandType::Graphics],
			.device = context.device,
			.wait = true
		}
	);
}

VKQuick::UniqueTexture VulkanTutorial::LoadTexture(const std::string& filename) {
	VKQuick::FrameContext const& context = m_vkQuick->GetFrameContext();
	vk::UniqueCommandBuffer cmdBuffer = VKQuick::CmdBufferCreateBegin(context.device, context.commandPools[VKQuick::CommandType::Graphics], "VulkanTexture upload");
	
	VKQuick::UniqueTexture tex = VKQuick::TextureBuilder(context.device, *m_memoryManager)
	.WithCommandBuffer(*cmdBuffer)
	.BuildFromFile(filename);

	VKQuick::CmdBufferSubmit(
		{
			.buffer = *cmdBuffer,
			.queue	= context.queues[VKQuick::CommandType::Graphics],
			.device = context.device,
			.wait	= true
		}
	);

	return tex;
}

VKQuick::UniqueTexture VulkanTutorial::LoadCubemap(
	const std::string& negativeXFile, const std::string& positiveXFile,
	const std::string& negativeYFile, const std::string& positiveYFile,
	const std::string& negativeZFile, const std::string& positiveZFile,
	const std::string& debugName) {

	VKQuick::FrameContext const& context = m_vkQuick->GetFrameContext();
	vk::UniqueCommandBuffer cmdBuffer = VKQuick::CmdBufferCreateBegin(context.device, context.commandPools[VKQuick::CommandType::Graphics], "VulkanTexture upload");

	VKQuick::UniqueTexture tex = VKQuick::TextureBuilder(context.device, *m_memoryManager)
		.WithCommandBuffer(*cmdBuffer)
		.BuildCubemapFromFile(negativeXFile, positiveXFile,
			negativeYFile, positiveYFile,
			negativeZFile, positiveZFile,
			debugName
	);

	VKQuick::CmdBufferSubmit(
		{
			.buffer = *cmdBuffer,
			.queue	= context.queues[VKQuick::CommandType::Graphics],
			.device = context.device,
			.wait	= true
		}
	);

	return tex;
}

void VulkanTutorial::RenderSingleObject(RenderObject& o, vk::CommandBuffer  toBuffer, VKQuick::Pipeline& toPipeline, int descriptorSet) {
	toBuffer.pushConstants(*toPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(Matrix4), (void*)&o.transform);
	toBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *toPipeline.layout, descriptorSet, 1, &*o.descriptorSet, 0, nullptr);
	
	const VKQuick::UniqueMesh& m = o.mesh->GetMesh();

	m->BindToCommandBuffer(toBuffer);
	m->Draw(toBuffer);
}

VKQuick::VKQuickInitialisation VulkanTutorial::DefaultInitialisation() {
	VKQuick::VKQuickInitialisation m_vkInit;

	m_vkInit.depthStencilFormat = vk::Format::eD32SfloatS8Uint;

	m_vkInit.majorVersion = 1;
	m_vkInit.minorVersion = 3;

	m_vkInit.deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	//vkInit.deviceExtensions.push_back("VK_KHR_dynamic_rendering");		//Now in core 1.3
	//vkInit.deviceExtensions.push_back("VK_KHR_maintenance4");			//Now in core 1.3
	//vkInit.deviceExtensions.push_back("VK_KHR_depth_stencil_resolve");	//Now in core 1.2
	//vkInit.deviceExtensions.push_back("VK_KHR_create_renderpass2");		//Now in core 1.2
	//vkInit.deviceExtensions.push_back("VK_KHR_synchronization2");		//Now in core 1.2
	m_vkInit.deviceExtensions.push_back("VK_EXT_robustness2");
	m_vkInit.deviceExtensions.emplace_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

	m_vkInit.instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	m_vkInit.instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	m_vkInit.instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#ifdef WIN32
	m_vkInit.instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

	m_vkInit.deviceLayers.push_back("VK_LAYER_LUNARG_standard_validation");

	m_vkInit.instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

	static vk::PhysicalDeviceRobustness2FeaturesEXT robustness{
		.nullDescriptor = true
	};
	
	static vk::PhysicalDeviceSynchronization2Features syncFeatures{
		.synchronization2 = true
	};

	static vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRendering{
		.dynamicRendering = true
	};

	static vk::PhysicalDeviceTimelineSemaphoreFeatures timelineSemaphores{
		.timelineSemaphore = true
	};

	static vk::PhysicalDeviceScalarBlockLayoutFeatures scalarFeatures{
		.scalarBlockLayout = true
	};
	
	m_vkInit.features.push_back((void*)&robustness);
	m_vkInit.features.push_back((void*)&syncFeatures);
	m_vkInit.features.push_back((void*)&dynamicRendering);
	m_vkInit.features.push_back((void*)&timelineSemaphores);
	m_vkInit.features.push_back((void*)&scalarFeatures);

	m_vkInit.framesInFlight = 1;

	m_vkInit.shaderRoot = Assets::SHADERDIR + "VK/";

	return m_vkInit;
}

VulkanTutorial* VulkanTutorial::CreateTutorial(const std::string& name, VKQuick::VKQuickInitialisation& vkInit) {
	VulkanTutorialEntry* e = VulkanTutorialEntry::s_listStartPtr;

	while (e) {
		if (e->m_name == name) {
			std::cout << "Running tutorial " << e->m_name << "\n";
			return e->m_creatorFunc(vkInit);
		}
		e = e->m_nodeChain;
	}
	return nullptr;
}

VulkanTutorial* VulkanTutorial::CreateTutorial(int& chainID, VKQuick::VKQuickInitialisation& vkInit) {
	VulkanTutorialEntry* e = VulkanTutorialEntry::s_listStartPtr;
	int index = 0;
	while (e && index != chainID) {
		e = e->m_nodeChain;
		index++;
	}
	chainID++;
	if (e) {
		std::cout << "Running tutorial " << e->m_name << "\n";
		return e->m_creatorFunc(vkInit);
	}

	return nullptr;
}

vk::TransformMatrixKHR NCL::Rendering::Vulkan::ToVulkanMatrix(const NCL::Maths::Matrix4& mat4) {
	vk::TransformMatrixKHR vkMat;

	vkMat.matrix[0][0] = mat4.array[0][0];
	vkMat.matrix[0][1] = mat4.array[1][0];
	vkMat.matrix[0][2] = mat4.array[2][0];
	vkMat.matrix[0][3] = mat4.array[3][0];
						 
	vkMat.matrix[1][0] = mat4.array[0][1];
	vkMat.matrix[1][1] = mat4.array[1][1];
	vkMat.matrix[1][2] = mat4.array[2][1];
	vkMat.matrix[1][3] = mat4.array[3][1];
						 
	vkMat.matrix[2][0] = mat4.array[0][2];
	vkMat.matrix[2][1] = mat4.array[1][2];
	vkMat.matrix[2][2] = mat4.array[2][2];
	vkMat.matrix[2][3] = mat4.array[3][2];

	return vkMat;
}