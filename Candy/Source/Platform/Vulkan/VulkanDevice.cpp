#include <Windows.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanSwapChain.h"
#include "Platform/Vulkan/VulkanPipelineState.h"
#include "Runtime/Core/Log.h"

#include <algorithm>

namespace Candy {

	// =========================================================================
	// Vulkan function loader — dynamically loads vulkan-1.dll
	// =========================================================================
	class VulkanFunctionLoader
	{
	public:
		VulkanFunctionLoader()
		{
			m_VulkanDLL = LoadLibraryA("vulkan-1.dll");
			if (!m_VulkanDLL)
			{
				CANDY_CORE_ERROR("VulkanFunctionLoader: failed to load vulkan-1.dll");
				return;
			}

			m_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
				GetProcAddress(m_VulkanDLL, "vkGetInstanceProcAddr"));

			if (!m_vkGetInstanceProcAddr)
			{
				CANDY_CORE_ERROR("VulkanFunctionLoader: failed to load vkGetInstanceProcAddr");
				return;
			}

			m_Loaded = true;
			CANDY_CORE_INFO("VulkanFunctionLoader: vulkan-1.dll loaded successfully");
		}

		~VulkanFunctionLoader()
		{
			if (m_VulkanDLL)
				FreeLibrary(m_VulkanDLL);
		}

		[[nodiscard]] bool IsLoaded() const { return m_Loaded; }
		[[nodiscard]] PFN_vkGetInstanceProcAddr GetInstanceProcAddr() const { return m_vkGetInstanceProcAddr; }

	private:
		HMODULE m_VulkanDLL = nullptr;
		PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = nullptr;
		bool m_Loaded = false;
	};

	// =========================================================================
	// Command queue — wraps VkQueue, uses function pointers from VulkanDevice
	// =========================================================================
	class VulkanCommandQueue : public RHICommandQueue
	{
	public:
		VulkanCommandQueue(VkQueue queue, uint32_t queueFamilyIndex, PFN_vkQueueWaitIdle fnQueueWaitIdle)
			: m_Queue(queue), m_QueueFamilyIndex(queueFamilyIndex), m_vkQueueWaitIdle(fnQueueWaitIdle) {}

		Scope<RHICommandBuffer> CreateCommandBuffer() override
		{
			CANDY_CORE_WARN("TODO: VulkanCommandQueue::CreateCommandBuffer — not yet implemented");
			return Scope<VulkanCommandBuffer>(new VulkanCommandBuffer());
		}

		void Submit(const std::vector<RHICommandBuffer*>& commandBuffers) override
		{
			CANDY_CORE_WARN("TODO: VulkanCommandQueue::Submit — not yet implemented");
		}

		void Present(const Ref<RHISwapChain>& swapChain) override
		{
			CANDY_CORE_WARN("TODO: VulkanCommandQueue::Present — not yet implemented");
		}

		void WaitIdle() override
		{
			if (m_Queue && m_vkQueueWaitIdle)
				m_vkQueueWaitIdle(m_Queue);
		}

		[[nodiscard]] VkQueue  GetNativeQueue()       const { return m_Queue; }
		[[nodiscard]] uint32_t GetQueueFamilyIndex() const { return m_QueueFamilyIndex; }

	private:
		VkQueue             m_Queue            = VK_NULL_HANDLE;
		uint32_t            m_QueueFamilyIndex = 0;
		PFN_vkQueueWaitIdle m_vkQueueWaitIdle  = nullptr;
	};

	// =========================================================================
	// VulkanDevice
	// =========================================================================

	VulkanDevice::VulkanDevice()
	{
		CANDY_CORE_INFO("VulkanDevice: initializing...");

		m_FunctionLoader = std::make_unique<VulkanFunctionLoader>();
		if (!m_FunctionLoader->IsLoaded())
		{
			CANDY_CORE_ERROR("VulkanDevice: function loader failed");
			return;
		}

		// ---- Load vkGetInstanceProcAddr entry point -----------------------

		auto GetIPA = m_FunctionLoader->GetInstanceProcAddr();

		// ---- Create VkInstance ---------------------------------------------

		VkApplicationInfo appInfo  = {};
		appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName   = "CandyEngine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName        = "CandyEngine";
		appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion         = VK_API_VERSION_1_3;

		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME
		};

		std::vector<const char*> validationLayers;
#if defined(CANDY_DEBUG)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		validationLayers.push_back("VK_LAYER_KHRONOS_validation");

		// Check if validation layers are available (use the loaded entry point)
		{
			auto vkEnumerateInstanceLayerPropertiesFn = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
				GetIPA(nullptr, "vkEnumerateInstanceLayerProperties"));

			uint32_t layerCount = 0;
			if (vkEnumerateInstanceLayerPropertiesFn)
			{
				vkEnumerateInstanceLayerPropertiesFn(&layerCount, nullptr);
				std::vector<VkLayerProperties> availableLayers(layerCount);
				vkEnumerateInstanceLayerPropertiesFn(&layerCount, availableLayers.data());

				bool validationFound = false;
				for (const auto& layer : availableLayers)
				{
					if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
					{
						validationFound = true;
						break;
					}
				}

				if (!validationFound)
				{
					CANDY_CORE_WARN("VulkanDevice: VK_LAYER_KHRONOS_validation not available, proceeding without validation layers");
					validationLayers.clear();
					extensions.erase(std::remove(extensions.begin(), extensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME), extensions.end());
				}
			}
		}
#endif

		VkInstanceCreateInfo instanceCI = {};
		instanceCI.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCI.pApplicationInfo        = &appInfo;
		instanceCI.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
		instanceCI.ppEnabledExtensionNames = extensions.data();
		instanceCI.enabledLayerCount       = static_cast<uint32_t>(validationLayers.size());
		instanceCI.ppEnabledLayerNames     = validationLayers.data();

		auto vkCreateInstanceFn = reinterpret_cast<PFN_vkCreateInstance>(
			GetIPA(nullptr, "vkCreateInstance"));

		if (!vkCreateInstanceFn)
		{
			CANDY_CORE_ERROR("VulkanDevice: failed to load vkCreateInstance");
			return;
		}

		VkResult result = vkCreateInstanceFn(&instanceCI, nullptr, &m_Instance);
		if (result != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanDevice: vkCreateInstance failed ({})", static_cast<int>(result));
			return;
		}

		// Load instance-level functions
		LoadInstanceFunctions();

		CANDY_CORE_INFO("VulkanDevice: VkInstance created");

		// ---- Select physical device ----------------------------------------

		uint32_t deviceCount = 0;
		m_Funcs.vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			CANDY_CORE_ERROR("VulkanDevice: no Vulkan-capable physical devices found");
			return;
		}

		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		m_Funcs.vkEnumeratePhysicalDevices(m_Instance, &deviceCount, physicalDevices.data());

		m_PhysicalDevice = VK_NULL_HANDLE;
		for (auto device : physicalDevices)
		{
			VkPhysicalDeviceProperties props;
			m_Funcs.vkGetPhysicalDeviceProperties(device, &props);

			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				m_PhysicalDevice = device;
				CANDY_CORE_INFO("VulkanDevice: selected discrete GPU: {}", props.deviceName);
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			m_PhysicalDevice = physicalDevices[0];
			VkPhysicalDeviceProperties props;
			m_Funcs.vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
			CANDY_CORE_INFO("VulkanDevice: no discrete GPU found, using: {}", props.deviceName);
		}

		// ---- Find graphics queue family ------------------------------------

		uint32_t queueFamilyCount = 0;
		m_Funcs.vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		m_Funcs.vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

		m_GraphicsQueueFamilyIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				m_GraphicsQueueFamilyIndex = i;
				break;
			}
		}

		if (m_GraphicsQueueFamilyIndex == UINT32_MAX)
		{
			CANDY_CORE_ERROR("VulkanDevice: no graphics queue family found");
			return;
		}

		// ---- Create logical device -----------------------------------------

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCI = {};
		queueCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCI.queueFamilyIndex = m_GraphicsQueueFamilyIndex;
		queueCI.queueCount       = 1;
		queueCI.pQueuePriorities = &queuePriority;

		std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo deviceCI      = {};
		deviceCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCI.queueCreateInfoCount    = 1;
		deviceCI.pQueueCreateInfos       = &queueCI;
		deviceCI.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
		deviceCI.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCI.pEnabledFeatures        = &deviceFeatures;

		result = m_Funcs.vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_Device);
		if (result != VK_SUCCESS)
		{
			CANDY_CORE_ERROR("VulkanDevice: vkCreateDevice failed ({})", static_cast<int>(result));
			return;
		}

		// Load device-level functions
		LoadDeviceFunctions();

		// Get the graphics queue
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		m_Funcs.vkGetDeviceQueue(m_Device, m_GraphicsQueueFamilyIndex, 0, &graphicsQueue);

		m_CommandQueue = CreateScope<VulkanCommandQueue>(graphicsQueue, m_GraphicsQueueFamilyIndex, m_Funcs.vkQueueWaitIdle);

		m_Initialized = true;
		CANDY_CORE_INFO("VulkanDevice: initialization complete");
	}

	VulkanDevice::~VulkanDevice()
	{
		WaitIdle();

		if (m_Device)
		{
			m_Funcs.vkDestroyDevice(m_Device, nullptr);
			m_Device = VK_NULL_HANDLE;
		}

		if (m_Instance)
		{
			m_Funcs.vkDestroyInstance(m_Instance, nullptr);
			m_Instance = VK_NULL_HANDLE;
		}

		CANDY_CORE_INFO("VulkanDevice: shutdown complete");
	}

	void VulkanDevice::LoadInstanceFunctions()
	{
		auto load = [this](const char* name) {
			return m_FunctionLoader->GetInstanceProcAddr()(m_Instance, name);
		};

		m_Funcs.vkGetPhysicalDeviceProperties           = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(load("vkGetPhysicalDeviceProperties"));
		m_Funcs.vkEnumeratePhysicalDevices              = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(load("vkEnumeratePhysicalDevices"));
		m_Funcs.vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(load("vkGetPhysicalDeviceQueueFamilyProperties"));
		m_Funcs.vkCreateDevice                          = reinterpret_cast<PFN_vkCreateDevice>(load("vkCreateDevice"));
		m_Funcs.vkGetDeviceQueue                        = reinterpret_cast<PFN_vkGetDeviceQueue>(load("vkGetDeviceQueue"));
		m_Funcs.vkDestroyInstance                       = reinterpret_cast<PFN_vkDestroyInstance>(load("vkDestroyInstance"));
	}

	void VulkanDevice::LoadDeviceFunctions()
	{
		// Load vkGetDeviceProcAddr first — it's the gateway to device functions
		auto loadInstance = [this](const char* name) {
			return m_FunctionLoader->GetInstanceProcAddr()(m_Instance, name);
		};

		PFN_vkGetDeviceProcAddr getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
			loadInstance("vkGetDeviceProcAddr"));

		if (!getDeviceProcAddr)
		{
			CANDY_CORE_ERROR("VulkanDevice: failed to load vkGetDeviceProcAddr");
			return;
		}

		auto loadDevice = [this, getDeviceProcAddr](const char* name) {
			return getDeviceProcAddr(m_Device, name);
		};

		m_Funcs.vkDestroyDevice   = reinterpret_cast<PFN_vkDestroyDevice>(loadDevice("vkDestroyDevice"));
		m_Funcs.vkQueueWaitIdle   = reinterpret_cast<PFN_vkQueueWaitIdle>(loadDevice("vkQueueWaitIdle"));
		m_Funcs.vkDeviceWaitIdle  = reinterpret_cast<PFN_vkDeviceWaitIdle>(loadDevice("vkDeviceWaitIdle"));
		m_Funcs.vkDevGetProcAddr  = getDeviceProcAddr;
	}

	// ---- Forwarding wrappers -----------------------------------------------

	void VulkanDevice::vkGetPhysicalDeviceProperties(VkPhysicalDevice physDevice, VkPhysicalDeviceProperties* props)
	{
		m_Funcs.vkGetPhysicalDeviceProperties(physDevice, props);
	}

	VkResult VulkanDevice::vkEnumeratePhysicalDevices(VkInstance inst, uint32_t* count, VkPhysicalDevice* devices)
	{
		return m_Funcs.vkEnumeratePhysicalDevices(inst, count, devices);
	}

	void VulkanDevice::vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physDevice, uint32_t* count, VkQueueFamilyProperties* props)
	{
		m_Funcs.vkGetPhysicalDeviceQueueFamilyProperties(physDevice, count, props);
	}

	VkResult VulkanDevice::vkCreateDevice(VkPhysicalDevice physDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
	{
		return m_Funcs.vkCreateDevice(physDevice, pCreateInfo, pAllocator, pDevice);
	}

	void VulkanDevice::vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
	{
		m_Funcs.vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
	}

	void VulkanDevice::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
	{
		m_Funcs.vkDestroyInstance(instance, pAllocator);
	}

	void VulkanDevice::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
	{
		m_Funcs.vkDestroyDevice(device, pAllocator);
	}

	VkResult VulkanDevice::vkQueueWaitIdle(VkQueue queue)
	{
		return m_Funcs.vkQueueWaitIdle(queue);
	}

	VkResult VulkanDevice::vkDeviceWaitIdle(VkDevice device)
	{
		return m_Funcs.vkDeviceWaitIdle(device);
	}

	// ---- Resource creation ---------------------------------------------------

	Ref<RHIBuffer> VulkanDevice::CreateBuffer(const BufferDesc& desc)
	{
		CANDY_CORE_WARN("TODO: VulkanDevice::CreateBuffer — not yet implemented");
		return nullptr;
	}

	Ref<RHITexture> VulkanDevice::CreateTexture(const TextureDesc& desc)
	{
		CANDY_CORE_WARN("TODO: VulkanDevice::CreateTexture — not yet implemented");
		return nullptr;
	}

	Ref<RHISampler> VulkanDevice::CreateSampler(const SamplerDesc& desc)
	{
		CANDY_CORE_WARN("TODO: VulkanDevice::CreateSampler — not yet implemented");
		return nullptr;
	}

	Ref<RHIShaderModule> VulkanDevice::CreateShaderModule(const void* spirvBytecode, uint32_t byteSize, const std::string& debugName)
	{
		if (!spirvBytecode || byteSize == 0)
		{
			CANDY_CORE_ERROR("VulkanDevice::CreateShaderModule: null or empty bytecode");
			return nullptr;
		}

		// Load vkCreateShaderModule dynamically
		auto vkCreateShaderModuleFn = reinterpret_cast<PFN_vkCreateShaderModule>(
			m_Funcs.vkDevGetProcAddr(m_Device, "vkCreateShaderModule"));

		if (!vkCreateShaderModuleFn)
		{
			CANDY_CORE_ERROR("VulkanDevice::CreateShaderModule: vkCreateShaderModule not found");
			return nullptr;
		}

		VkShaderModuleCreateInfo shaderCI = {};
		shaderCI.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderCI.codeSize = byteSize;
		shaderCI.pCode    = static_cast<const uint32_t*>(spirvBytecode);

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VkResult result = vkCreateShaderModuleFn(m_Device, &shaderCI, nullptr, &shaderModule);
		if (result == VK_SUCCESS)
		{
			CANDY_CORE_INFO("VulkanDevice::CreateShaderModule: '{}' created", debugName);
			// TODO: wrap in RHIShaderModule
			return nullptr; // placeholder
		}

		CANDY_CORE_ERROR("VulkanDevice::CreateShaderModule: vkCreateShaderModule failed ({})", static_cast<int>(result));
		return nullptr;
	}

	Ref<RHIGraphicsPipeline> VulkanDevice::CreateGraphicsPipeline(
		const GraphicsPipelineDesc& desc,
		const Ref<RHIShaderModule>& vs,
		const Ref<RHIShaderModule>& fs)
	{
		if (auto cached = GetPipelineCache().Find(desc))
			return cached;

		CANDY_CORE_WARN("TODO: VulkanDevice::CreateGraphicsPipeline — not yet implemented");

		Ref<VulkanGraphicsPipeline> pipeline = CreateRef<VulkanGraphicsPipeline>(desc);
		GetPipelineCache().Insert(desc, pipeline);
		return pipeline;
	}

	Ref<RHISwapChain> VulkanDevice::CreateSwapChain(const SwapChainDesc& desc)
	{
		CANDY_CORE_INFO("VulkanDevice::CreateSwapChain {}x{}", desc.Width, desc.Height);
		return CreateRef<VulkanSwapChain>(desc);
	}

	// ---- Command submission --------------------------------------------------

	RHICommandQueue& VulkanDevice::GetCommandQueue()
	{
		return *m_CommandQueue;
	}

	void VulkanDevice::WaitIdle()
	{
		if (m_Funcs.vkDeviceWaitIdle)
			m_Funcs.vkDeviceWaitIdle(m_Device);
	}

} // namespace Candy
