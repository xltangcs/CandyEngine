#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Vulkan/VulkanPipelineState.h"
#include "Platform/Vulkan/VulkanSPIRV.h"
#include "Runtime/Core/Log.h"

#include <algorithm>

namespace Candy {

	class VulkanFunctionLoader
	{
	public:
		VulkanFunctionLoader()
		{
			m_DLL = LoadLibraryA("vulkan-1.dll");
			if (!m_DLL) { CANDY_CORE_ERROR("Vulkan: failed to load vulkan-1.dll"); return; }
			m_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(m_DLL, "vkGetInstanceProcAddr"));
			if (!m_vkGetInstanceProcAddr) { CANDY_CORE_ERROR("Vulkan: vkGetInstanceProcAddr not found"); return; }
			m_Loaded = true;
		}
		~VulkanFunctionLoader() { if (m_DLL) FreeLibrary(m_DLL); }
		bool IsLoaded() const { return m_Loaded; }
		PFN_vkGetInstanceProcAddr GetIPA() const { return m_vkGetInstanceProcAddr; }
	private:
		HMODULE m_DLL = nullptr;
		PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = nullptr;
		bool m_Loaded = false;
	};

	// =========================================================================
	// VulkanCommandQueue
	// =========================================================================
	class VulkanCommandQueue : public RHICommandQueue
	{
	public:
		VulkanCommandQueue(VulkanDevice* dev, VkQueue queue, uint32_t qfi, VkCommandPool pool)
			: m_Dev(dev), m_Queue(queue), m_QueueFamilyIndex(qfi), m_Pool(pool) {}

		Scope<RHICommandBuffer> CreateCommandBuffer() override
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool        = m_Pool;
			allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer cb;
			if (m_Dev->fnAllocateCommandBuffers(m_Dev->GetVkDevice(), &allocInfo, &cb) != VK_SUCCESS)
			{
				CANDY_CORE_ERROR("VulkanCommandQueue: vkAllocateCommandBuffers failed");
				return nullptr;
			}
			return Candy::CreateScope<VulkanCommandBuffer>(m_Dev, m_Pool, cb);
		}

		void Submit(const std::vector<RHICommandBuffer*>& cbs) override
		{
			std::vector<VkCommandBuffer> vkCbs;
			for (auto* cb : cbs)
				if (auto* vkc = dynamic_cast<VulkanCommandBuffer*>(cb))
					vkCbs.push_back(vkc->GetVkCommandBuffer());

			VkSubmitInfo si = {};
			si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			si.commandBufferCount = static_cast<uint32_t>(vkCbs.size());
			si.pCommandBuffers    = vkCbs.data();

			VkFence fence = VK_NULL_HANDLE;
			m_Dev->fnCreateFence(m_Dev->GetVkDevice(), &VkFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}, nullptr, &fence);
			vkQueueSubmit(m_Queue, 1, &si, fence);
			m_Dev->fnWaitForFences(m_Dev->GetVkDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
			m_Dev->fnDestroyFence(m_Dev->GetVkDevice(), fence, nullptr);
		}

		void Present(const Ref<RHISwapChain>& sc) override
		{
			auto* vksc = dynamic_cast<VulkanSwapChain*>(sc.get());
			if (!vksc) return;

			uint32_t imgIndex = vksc->AcquireNextImage(VK_NULL_HANDLE, VK_NULL_HANDLE);
			VkPresentInfoKHR pi = {};
			pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			pi.swapchainCount     = 1;
			pi.pSwapchains        = &vksc->GetVkSwapchain();
			pi.pImageIndices      = &imgIndex;
			m_Dev->fnQueuePresentKHR(m_Queue, &pi);
		}

		void WaitIdle() override { if (m_Queue) vkQueueWaitIdle(m_Queue); }

		[[nodiscard]] VkQueue           GetQueue()           const { return m_Queue; }
		[[nodiscard]] VkCommandPool     GetPool()            const { return m_Pool; }
		[[nodiscard]] uint32_t          GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }

	private:
		VulkanDevice* m_Dev = nullptr;
		VkQueue       m_Queue = VK_NULL_HANDLE;
		uint32_t      m_QueueFamilyIndex = 0;
		VkCommandPool m_Pool = VK_NULL_HANDLE;
	};

	// =========================================================================
	// VulkanDevice
	// =========================================================================

#define LOAD(fn) fn = reinterpret_cast<decltype(fn)>(GetIPA(#fn))
#define LOAD_DEV(fn) fn = reinterpret_cast<decltype(fn)>(GetDevProc(#fn))

	VulkanDevice::VulkanDevice()
	{
		CANDY_CORE_INFO("VulkanDevice: initializing...");

		m_FunctionLoader = std::make_unique<VulkanFunctionLoader>();
		if (!m_FunctionLoader->IsLoaded()) return;

		auto GetIPA = m_FunctionLoader->GetIPA();

		// ---- VkInstance ---------------------------------------------------

		VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.pApplicationName = "CandyEngine";
		appInfo.apiVersion       = VK_API_VERSION_1_3;

		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME
		};

		VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		ici.pApplicationInfo       = &appInfo;
		ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		ici.ppEnabledExtensionNames = extensions.data();

		auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(GetIPA("vkCreateInstance"));
		if (!vkCreateInstance || vkCreateInstance(&ici, nullptr, &m_Instance) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanDevice: vkCreateInstance failed");
			return;
		}

		// ---- Physical device ------------------------------------------------

		uint32_t count = 0;
		vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());
		m_PhysicalDevice = devices.empty() ? VK_NULL_HANDLE : devices[0];

		// Prefer discrete
		for (auto d : devices) {
			VkPhysicalDeviceProperties p;
			vkGetPhysicalDeviceProperties(d, &p);
			if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { m_PhysicalDevice = d; break; }
		}

		// ---- Queue family ------------------------------------------------

		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &count, nullptr);
		std::vector<VkQueueFamilyProperties> qfProps(count);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &count, qfProps.data());
		for (uint32_t i = 0; i < count; ++i)
			if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { m_GraphicsQueueFamilyIndex = i; break; }

		// ---- Logical device ----------------------------------------------

		float prio = 1.0f;
		VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		qci.queueFamilyIndex = m_GraphicsQueueFamilyIndex;
		qci.queueCount = 1; qci.pQueuePriorities = &prio;

		const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
		dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = devExts;

		auto vkCreateDeviceFn = reinterpret_cast<PFN_vkCreateDevice>(GetIPA("vkCreateDevice"));
		if (!vkCreateDeviceFn || vkCreateDeviceFn(m_PhysicalDevice, &dci, nullptr, &m_Device) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanDevice: vkCreateDevice failed");
			return;
		}

		// ---- Load all functions ------------------------------------------

		auto GetDevProc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetIPA("vkGetDeviceProcAddr"));

		// Core 1.0
		LOAD_DEV(fnCreateBuffer);   LOAD_DEV(fnDestroyBuffer);
		LOAD_DEV(fnAllocateMemory); LOAD_DEV(fnFreeMemory);
		LOAD_DEV(fnBindBufferMemory);
		LOAD_DEV(fnMapMemory);      LOAD_DEV(fnUnmapMemory);
		LOAD_DEV(fnCreateCommandPool); LOAD_DEV(fnDestroyCommandPool);
		LOAD_DEV(fnAllocateCommandBuffers); LOAD_DEV(fnFreeCommandBuffers);
		LOAD_DEV(fnCreateShaderModule); LOAD_DEV(fnDestroyShaderModule);
		LOAD_DEV(fnCreatePipelineLayout); LOAD_DEV(fnDestroyPipelineLayout);
		LOAD_DEV(fnCreateGraphicsPipelines); LOAD_DEV(fnDestroyPipeline);
		LOAD_DEV(fnCreateRenderPass); LOAD_DEV(fnDestroyRenderPass);
		LOAD_DEV(fnCreateFramebuffer); LOAD_DEV(fnDestroyFramebuffer);
		LOAD_DEV(fnCreateImageView); LOAD_DEV(fnDestroyImageView);
		LOAD_DEV(fnCreateFence); LOAD_DEV(fnDestroyFence);
		LOAD_DEV(fnWaitForFences); LOAD_DEV(fnResetFences);

		// Additional functions for Texture/Sampler/Descriptor/Renderer2D
		LOAD_DEV(fnCreateImage);               LOAD_DEV(fnDestroyImage);
		LOAD_DEV(fnGetImageMemoryRequirements); LOAD_DEV(fnBindImageMemory);
		LOAD_DEV(fnCreateSampler);             LOAD_DEV(fnDestroySampler);
		LOAD_DEV(fnCreateDescriptorSetLayout);  LOAD_DEV(fnDestroyDescriptorSetLayout);
		LOAD_DEV(fnCreateDescriptorPool);      LOAD_DEV(fnDestroyDescriptorPool);
		LOAD_DEV(fnAllocateDescriptorSets);    LOAD_DEV(fnUpdateDescriptorSets);
		LOAD_DEV(fnCmdBindDescriptorSets);     LOAD_DEV(fnCmdCopyBufferToImage);
		LOAD_DEV(fnCmdPipelineBarrier);        LOAD_DEV(fnCreateSemaphore);
		LOAD_DEV(fnDestroySemaphore);          LOAD_DEV(fnGetBufMemReqs);
		LOAD_DEV(fnBeginCommandBuffer);        LOAD_DEV(fnEndCommandBuffer);
		LOAD_DEV(fnQueueSubmit);              LOAD_DEV(fnCmdDraw);
		LOAD_DEV(fnCmdDrawIndexed);           LOAD_DEV(fnCmdBindVertexBuffers);
		LOAD_DEV(fnCmdBindIndexBuffer);       LOAD_DEV(fnCmdSetViewport);
		LOAD_DEV(fnCmdSetScissor);

		// Instance extensions
		LOAD(fnCreateSwapchainKHR);   LOAD_DEV(fnDestroySwapchainKHR);
		LOAD_DEV(fnGetSwapchainImagesKHR);
		LOAD_DEV(fnAcquireNextImageKHR);
		LOAD_DEV(fnQueuePresentKHR);
		LOAD(fnCreateWin32SurfaceKHR); LOAD(fnDestroySurfaceKHR);

		// ---- Command pool + queue ----------------------------------------

		VkCommandPoolCreateInfo poolCI = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolCI.queueFamilyIndex = m_GraphicsQueueFamilyIndex;
		poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		fnCreateCommandPool(m_Device, &poolCI, nullptr, &m_CommandPool);

		vkGetDeviceQueue(m_Device, m_GraphicsQueueFamilyIndex, 0, &m_Queue);

		m_CommandQueue = CreateScope<VulkanCommandQueue>(this, m_Queue, m_GraphicsQueueFamilyIndex, m_CommandPool);

		m_Initialized = true;
		CANDY_CORE_INFO("VulkanDevice: ready");
	}

#undef LOAD
#undef LOAD_DEV

	VulkanDevice::~VulkanDevice()
	{
		WaitIdle();
		if (m_Device)   { vkDestroyDevice(m_Device, nullptr);   m_Device   = VK_NULL_HANDLE; }
		if (m_Instance) { vkDestroyInstance(m_Instance, nullptr); m_Instance = VK_NULL_HANDLE; }
	}

	// ---- Built-in SPIR-V ---------------------------------------------------

	const std::vector<uint32_t>& VulkanDevice::GetTriangleVSSPIRV() { return VulkanSPIRV::GetTriangleVS(); }
	const std::vector<uint32_t>& VulkanDevice::GetTrianglePSSPIRV() { return VulkanSPIRV::GetTrianglePS(); }

	// ---- Resource creation ---------------------------------------------------

	Ref<RHIBuffer> VulkanDevice::CreateBuffer(const BufferDesc& desc)
	{
		return CreateRef<VulkanBuffer>(this, desc);
	}

	Ref<RHITexture> VulkanDevice::CreateTexture(const TextureDesc&)
	{
		CANDY_CORE_WARN("TODO: Vulkan CreateTexture");
		return nullptr;
	}

	Ref<RHISampler> VulkanDevice::CreateSampler(const SamplerDesc&)
	{
		CANDY_CORE_WARN("TODO: Vulkan CreateSampler");
		return nullptr;
	}

	Ref<RHIShaderModule> VulkanDevice::CreateShaderModule(const void* spirv, uint32_t size, const std::string&)
	{
		if (!spirv || size == 0) return nullptr;

		VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		ci.codeSize = size;
		ci.pCode    = static_cast<const uint32_t*>(spirv);

		VkShaderModule sm;
		if (fnCreateShaderModule(m_Device, &ci, nullptr, &sm) != VK_SUCCESS) return nullptr;

		// Wrap in simple holder (same pattern as DX12)
		struct VkShaderHolder : RHIShaderModule {
			VkDevice dev; VkShaderModule mod; ShaderStage stage; std::string name;
			PFN_vkDestroyShaderModule destroyFn;
			VkShaderHolder(VkDevice d, VkShaderModule m, ShaderStage s, std::string n, PFN_vkDestroyShaderModule f)
				: dev(d), mod(m), stage(s), name(n), destroyFn(f) {}
			~VkShaderHolder() override { if (mod && destroyFn) destroyFn(dev, mod, nullptr); }
			ShaderStage GetStage() const override { return stage; }
			const uint32_t* GetBytecode() const override { return nullptr; }
			uint32_t GetBytecodeSize() const override { return 0; }
			const std::string& GetDebugName() const override { return name; }
			VkShaderModule GetModule() const { return mod; }
		};

		return CreateRef<VkShaderHolder>(m_Device, sm, ShaderStage::None, std::string(), fnDestroyShaderModule);
	}

	Ref<RHIGraphicsPipeline> VulkanDevice::CreateGraphicsPipeline(
		const GraphicsPipelineDesc& desc,
		const Ref<RHIShaderModule>& vs,
		const Ref<RHIShaderModule>& fs)
	{
		if (auto cached = GetPipelineCache().Find(desc))
			return cached;

		// Shader stages
		auto* vsHolder = dynamic_cast<struct VkShaderHolder*>(vs.get());
		auto* fsHolder = dynamic_cast<struct VkShaderHolder*>(fs.get());

		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vsHolder ? vsHolder->GetModule() : VK_NULL_HANDLE;
		stages[0].pName  = "main";
		stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fsHolder ? fsHolder->GetModule() : VK_NULL_HANDLE;
		stages[1].pName  = "main";

		// Vertex input
		VkVertexInputBindingDescription bindings[1] = {};
		if (!desc.VertexInput.Bindings.empty())
		{
			bindings[0].binding   = desc.VertexInput.Bindings[0].Binding;
			bindings[0].stride    = desc.VertexInput.Bindings[0].Stride;
			bindings[0].inputRate = desc.VertexInput.Bindings[0].PerInstance
			                        ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
		}
		else
		{
			bindings[0].binding = 0; bindings[0].stride = 7 * sizeof(float);
			bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		std::vector<VkVertexInputAttributeDescription> attrs;
		for (const auto& a : desc.VertexInput.Attributes)
		{
			VkFormat fmt = VK_FORMAT_R32G32B32_SFLOAT;
			switch (a.Format) {
			case RHIFormat::R32G32Float:       fmt = VK_FORMAT_R32G32_SFLOAT; break;
			case RHIFormat::R32G32B32Float:    fmt = VK_FORMAT_R32G32B32_SFLOAT; break;
			case RHIFormat::R32G32B32A32Float: fmt = VK_FORMAT_R32G32B32A32_SFLOAT; break;
			case RHIFormat::R8G8B8A8Unorm:     fmt = VK_FORMAT_R8G8B8A8_UNORM; break;
			default: break;
			}
			attrs.push_back({ a.Location, a.Binding, fmt, a.Offset });
		}
		if (attrs.empty())
		{
			attrs.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 });
			attrs.push_back({ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12 });
		}

		VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		vi.vertexBindingDescriptionCount   = 1;
		vi.pVertexBindingDescriptions      = bindings;
		vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
		vi.pVertexAttributeDescriptions    = attrs.data();

		VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineLayoutCreateInfo plCI = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		VkPipelineLayout layout;
		fnCreatePipelineLayout(m_Device, &plCI, nullptr, &layout);

		VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		vpState.viewportCount = 1; vpState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		rs.polygonMode = (desc.Rasterizer.Fill == FillMode::Wireframe)
		                 ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
		rs.cullMode    = (desc.Rasterizer.Cull == CullMode::None) ? VK_CULL_MODE_NONE
		               : (desc.Rasterizer.Cull == CullMode::Front) ? VK_CULL_MODE_FRONT_BIT
		                                                           : VK_CULL_MODE_BACK_BIT;
		rs.frontFace   = VK_FRONT_FACE_CLOCKWISE;
		rs.lineWidth   = 1.0f;

		VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState blendAttach = {};
		blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		                           | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		blend.attachmentCount = 1; blend.pAttachments = &blendAttach;

		VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dyn = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dyn.dynamicStateCount = 2; dyn.pDynamicStates = dynStates;

		VkGraphicsPipelineCreateInfo pci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pci.stageCount = 2; pci.pStages = stages;
		pci.pVertexInputState   = &vi;
		pci.pInputAssemblyState = &ia;
		pci.pViewportState      = &vpState;
		pci.pRasterizationState = &rs;
		pci.pMultisampleState   = &ms;
		pci.pColorBlendState    = &blend;
		pci.pDynamicState       = &dyn;
		pci.layout = layout;
		pci.renderPass = VK_NULL_HANDLE; // Will be set when bound to swapchain
		pci.subpass = 0;

		VkPipeline pipeline;
		if (fnCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanDevice: CreateGraphicsPipeline failed");
			fnDestroyPipelineLayout(m_Device, layout, nullptr);
			return nullptr;
		}

		auto vkp = CreateRef<VulkanGraphicsPipeline>(desc);
		vkp->SetVkPipeline(pipeline, layout);
		GetPipelineCache().Insert(desc, vkp);
		return vkp;
	}

	Ref<RHISwapChain> VulkanDevice::CreateSwapChain(const SwapChainDesc& desc)
	{
		return CreateRef<VulkanSwapChain>(this, desc);
	}

	RHICommandQueue& VulkanDevice::GetCommandQueue() { return *m_CommandQueue; }

	void VulkanDevice::WaitIdle() { if (m_Device) vkDeviceWaitIdle(m_Device); }

} // namespace Candy
