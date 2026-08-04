#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLRHICommandBuffer.h"
#include "Platform/OpenGL/OpenGLRHIResources.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Runtime/Core/Log.h"

#include <glad/glad.h>

namespace Candy {

	void OpenGLRHICommandBuffer::Begin()
	{
		// Modern OpenGL needs a bound VAO before any vertex attribute / draw.
		glBindVertexArray(m_DefaultVAO);
	}

	void OpenGLRHICommandBuffer::End()
	{
		glBindVertexArray(0);
	}

	void OpenGLRHICommandBuffer::BeginRenderPass(const RenderPassDesc& desc)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_Framebuffer);

		GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
		uint32_t count = std::min<uint32_t>(4u, static_cast<uint32_t>(desc.ColorAttachments.size()));
		if (count > 0)
			glDrawBuffers(count, buffers);

		// Clear attachments
		for (uint32_t i = 0; i < desc.ColorAttachments.size(); ++i)
		{
			if (desc.ColorAttachments[i].LoadOp == LoadOp::Clear)
			{
				const float* cc = desc.ColorAttachments[i].ClearColor;
				GLbitfield mask = (i == 0) ? GL_COLOR_BUFFER_BIT : 0;
				if (i == 0)
				{
					glClearColor(cc ? cc[0] : 0, cc ? cc[1] : 0,
					             cc ? cc[2] : 0, cc ? cc[3] : 1);
					glClear(mask);
				}
			}
		}
		if (desc.HasDepthStencil && desc.DepthStencilAttachment.DepthLoadOp == LoadOp::Clear)
		{
			GLbitfield mask = GL_DEPTH_BUFFER_BIT;
			if (desc.DepthStencilAttachment.StencilLoadOp == LoadOp::Clear)
				mask |= GL_STENCIL_BUFFER_BIT;
			glClearDepth(desc.DepthStencilAttachment.ClearDepth);
			glClearStencil(desc.DepthStencilAttachment.ClearStencil);
			glClear(mask);
		}
	}

	void OpenGLRHICommandBuffer::EndRenderPass()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLRHICommandBuffer::SetPipeline(const Ref<RHIGraphicsPipeline>& pipeline)
	{
		m_Pipeline = pipeline;
		auto* gl = static_cast<OpenGLRHIGraphicsPipeline*>(pipeline.get());
		if (gl)
		{
			m_Program = gl->GetProgram();
			glUseProgram(m_Program);
			gl->ApplyState();
		}
		else
		{
			m_Program = 0;
		}
	}

	void OpenGLRHICommandBuffer::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
	{
		glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
		           static_cast<GLsizei>(width), static_cast<GLsizei>(height));
		// depth range: modern GL uses glDepthRangef, but glDepthRange ok
		(void)minDepth; (void)maxDepth;
	}

	void OpenGLRHICommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(static_cast<GLint>(x), static_cast<GLint>(y),
		          static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	}

	void OpenGLRHICommandBuffer::SetVertexBuffer(const Ref<RHIBuffer>& buffer, uint32_t slot, uint64_t offset)
	{
		(void)slot;
		auto* gl = dynamic_cast<OpenGLRHIBuffer*>(buffer.get());
		if (!gl) return;
		m_VBID     = gl->GetID();
		m_VBStride = gl->GetStride();
		m_VBOffset = static_cast<GLuint>(offset);
		glBindBuffer(GL_ARRAY_BUFFER, m_VBID);
		if (m_Pipeline)
		{
			const GraphicsPipelineDesc& desc = static_cast<OpenGLRHIGraphicsPipeline*>(m_Pipeline.get())->GetDesc();
			for (const auto& attr : desc.VertexInput.Attributes)
			{
				GLenum type; GLint size; GLboolean normalized;
				ToGLVertexAttrib(attr.Format, type, size, normalized);
				glEnableVertexAttribArray(attr.Location);
				// Use classic glVertexAttribPointer; offset within stride only.
				glVertexAttribPointer(attr.Location, size, type, normalized,
				                       static_cast<GLsizei>(m_VBStride),
				                       reinterpret_cast<void*>(static_cast<uintptr_t>(attr.Offset + m_VBOffset)));
			}
		}
	}

	void OpenGLRHICommandBuffer::SetIndexBuffer(const Ref<RHIBuffer>& buffer, IndexFormat format, uint64_t offset)
	{
		(void)offset;
		auto* gl = dynamic_cast<OpenGLRHIBuffer*>(buffer.get());
		if (!gl) return;
		m_IBID   = gl->GetID();
		m_IBType = (format == IndexFormat::UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBID);
	}

	void OpenGLRHICommandBuffer::SetConstantBuffer(uint32_t slot, uint32_t binding, const Ref<RHIBuffer>& buffer)
	{
		(void)slot; // D3D12 root param; not needed in OpenGL
		auto* gl = dynamic_cast<OpenGLRHIBuffer*>(buffer.get());
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, gl ? gl->GetID() : 0);
	}

	void OpenGLRHICommandBuffer::SetTexture(uint32_t slot, uint32_t binding, const Ref<RHITexture>& texture)
	{
		(void)slot;
		auto* gl = dynamic_cast<OpenGLRHITexture2D*>(texture.get());
		GLenum unit = static_cast<GLenum>(GL_TEXTURE0 + binding);
		glActiveTexture(unit);
		glBindTexture(GL_TEXTURE_2D, gl ? gl->GetID() : 0);
	}

	void OpenGLRHICommandBuffer::SetSampler(uint32_t slot, uint32_t binding, const Ref<RHISampler>& sampler)
	{
		(void)slot;
		auto* gl = dynamic_cast<OpenGLRHISampler*>(sampler.get());
		glBindSampler(static_cast<GLuint>(binding), gl ? gl->GetID() : 0);
	}

	void OpenGLRHICommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
	                                  uint32_t firstVertex, uint32_t firstInstance)
	{
		(void)firstInstance;
		if (instanceCount <= 1)
			glDrawArrays(GL_TRIANGLES, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
		else
			glDrawArraysInstanced(GL_TRIANGLES, static_cast<GLint>(firstVertex),
			                      static_cast<GLsizei>(vertexCount), static_cast<GLsizei>(instanceCount));
	}

	void OpenGLRHICommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
	                                          uint32_t firstIndex, int32_t vertexOffset,
	                                          uint32_t firstInstance)
	{
		(void)firstInstance;
		GLuint topology = GL_TRIANGLES;
		if (m_Pipeline)
		{
			const GraphicsPipelineDesc& desc = static_cast<OpenGLRHIGraphicsPipeline*>(m_Pipeline.get())->GetDesc();
			switch (desc.Topology)
			{
			case PrimitiveTopology::Triangles:      topology = GL_TRIANGLES;      break;
			case PrimitiveTopology::Lines:          topology = GL_LINES;          break;
			case PrimitiveTopology::Points:         topology = GL_POINTS;         break;
			case PrimitiveTopology::TriangleStrip:  topology = GL_TRIANGLE_STRIP; break;
			case PrimitiveTopology::LineStrip:       topology = GL_LINE_STRIP;     break;
			default:                                topology = GL_TRIANGLES;      break;
			}
		}

		GLvoid* indices = reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(firstIndex * (m_IBType == GL_UNSIGNED_SHORT ? 2 : 4)));
		if (instanceCount <= 1)
		{
			glDrawElements(topology, static_cast<GLsizei>(indexCount), m_IBType, indices);
		}
		else
		{
			glDrawElementsInstanced(topology, static_cast<GLsizei>(indexCount), m_IBType,
			                        indices, static_cast<GLsizei>(instanceCount));
		}
		(void)vertexOffset;
	}

	void OpenGLRHICommandBuffer::SetSwapChainRenderTarget(RHISwapChain* /*swapChain*/)
	{
		m_Framebuffer = 0; // default framebuffer
	}

	void OpenGLRHICommandBuffer::SetFramebufferRenderTarget(const Ref<RHIFramebuffer>& framebuffer)
	{
		// Legacy-bridge path: EditorLayer hands in an OpenGLFramebuffer that
		// multi-inherits RHIFramebuffer; expose its native FBO id.
		if (auto* oglFb = dynamic_cast<OpenGLFramebuffer*>(framebuffer.get()))
		{
			m_Framebuffer = oglFb->GetNativeFBO();
			return;
		}
		auto* gl = dynamic_cast<OpenGLRHIFramebuffer*>(framebuffer.get());
		m_Framebuffer = gl ? gl->GetFBO() : 0;
	}

} // namespace Candy
