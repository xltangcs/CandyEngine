#pragma once

#include "Runtime/RHI/RHICommandBuffer.h"

#include <glad/glad.h>

namespace Candy {

	class RHIFramebuffer;
	class RHISwapChain;

	// =========================================================================
	// OpenGLRHICommandBuffer — immediate-mode GL implementation of the
	// command-recording RHI interface.  Calls translate straight to GL.
	// =========================================================================
	class OpenGLRHICommandBuffer : public RHICommandBuffer
	{
	public:
		explicit OpenGLRHICommandBuffer(GLuint defaultVAO) : m_DefaultVAO(defaultVAO) {}
		~OpenGLRHICommandBuffer() override = default;

		void Begin() override;
		void End()   override;

		void BeginRenderPass(const RenderPassDesc& desc) override;
		void EndRenderPass() override;

		void SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline) override;
		void SetViewport(float x, float y, float width, float height,
		                 float minDepth = 0.0f, float maxDepth = 1.0f) override;
		void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

		void SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot = 0, uint64_t offset = 0) override;
		void SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format = IndexFormat::UInt32, uint64_t offset = 0) override;

		void SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer) override;
		void SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture) override;
		void SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler) override;

		void Draw(uint32_t vertexCount,
		          uint32_t instanceCount = 1,
		          uint32_t firstVertex   = 0,
		          uint32_t firstInstance = 0) override;

		void DrawIndexed(uint32_t indexCount,
		                 uint32_t instanceCount = 1,
		                 uint32_t firstIndex    = 0,
		                 int32_t  vertexOffset  = 0,
		                 uint32_t firstInstance = 0) override;

		/// D3D12 path's banded API for parity; in OpenGL this just records
		/// the target for the subsequent BeginRenderPass.
		void SetSwapChainRenderTarget(RHISwapChain* swapChain);
		void SetFramebufferRenderTarget(const Ref<RHIFramebuffer>& framebuffer);

	private:
		GLuint m_DefaultVAO = 0;

		Ref<RHIGraphicsPipeline> m_Pipeline;       // shared-by-Ref to keep alive
		GLuint                   m_Program = 0;

		GLuint m_VBID     = 0;
		GLuint m_VBStride = 0;
		GLuint m_VBOffset = 0;
		GLuint m_IBID     = 0;
		GLenum m_IBType   = GL_UNSIGNED_INT;

		GLuint                  m_Framebuffer = 0; ///< 0 = default framebuffer
	};

} // namespace Candy
