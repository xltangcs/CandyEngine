#pragma once

#include "Runtime/RHI/Shared/RHIDeviceBase.h"

#include <glad/glad.h>

namespace Candy {

	// =========================================================================
	// OpenGLRHIDevice — RHIDevice / RHIDeviceBase backend backed by OpenGL.
	// Provides direct-3D-style factory operations using modern OpenGL
	// (DSA, persistently mapped buffers, program pipeline objects).
	// =========================================================================
	class OpenGLRHIDevice : public RHIDeviceBase
	{
	public:
		OpenGLRHIDevice();
		~OpenGLRHIDevice() override;

		Ref<RHIBuffer>   CreateBuffer(const BufferDesc& desc) override;
		Ref<RHITexture>  CreateTexture(const TextureDesc& desc) override;
		Ref<RHISampler>  CreateSampler(const SamplerDesc& desc) override;

		// Stores GLSL source bytes inside an OpenGLRHIShaderModule.  byteSize
		// is the count of bytes in the string (not dwords).
		Ref<RHIShaderModule> CreateShaderModule(const void* sourceBytes, uint32_t byteSize,
		                                         const std::string& debugName) override;

		// GLSL modules carry the whole multi-stage file; `stage`/`entryPoint`
		// are ignored (CreateGraphicsPipeline parses #type markers itself).
		Ref<RHIShaderModule> CreateShaderModuleFromSource(const char* source, ShaderStage stage, const std::string& entryPoint, const std::string& debugName = "") override
		{
			(void)stage; (void)entryPoint;
			return CreateShaderModule(source, static_cast<uint32_t>(strlen(source)), debugName);
		}

		Ref<RHIGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc,
		                                                 const Ref<RHIShaderModule>& vs,
		                                                 const Ref<RHIShaderModule>& fs) override;

		Ref<RHISwapChain> CreateSwapChain(const SwapChainDesc& desc) override;
	Ref<RHIFramebuffer> CreateFramebuffer(const FramebufferDesc& desc) override;

		RHICommandQueue& GetCommandQueue() override;
		void WaitIdle() override;

		GLuint GetDefaultVAO() const { return m_DefaultVAO; }

	private:
		GLuint                   m_DefaultVAO = 0;
		Scope<RHICommandQueue>   m_Queue;
	};

} // namespace Candy
