#include "CandyPCH.h"

#include "Platform/OpenGL/OpenGLRHIDevice.h"
#include "Platform/OpenGL/OpenGLRHIResources.h"
#include "Platform/OpenGL/OpenGLRHICommandQueue.h"
#include "Runtime/Core/Log.h"
#include "Runtime/RHI/RHIShaderSource.h"

#include <glad/glad.h>

namespace Candy {

	// =========================================================================
	// OpenGLRHIDevice
	// =========================================================================
	OpenGLRHIDevice::OpenGLRHIDevice()
	{
		glGenVertexArrays(1, &m_DefaultVAO);
		glBindVertexArray(m_DefaultVAO);
		m_Queue = CreateScope<OpenGLRHICommandQueue>(m_DefaultVAO);
		CANDY_CORE_INFO("OpenGLRHIDevice: initialized (default VAO {})", m_DefaultVAO);
	}

	OpenGLRHIDevice::~OpenGLRHIDevice()
	{
		m_Queue.reset();
		if (m_DefaultVAO)
			glDeleteVertexArrays(1, &m_DefaultVAO);
	}

	Ref<RHIBuffer> OpenGLRHIDevice::CreateBuffer(const BufferDesc& desc)
	{
		return CreateRef<OpenGLRHIBuffer>(desc);
	}

	Ref<RHITexture> OpenGLRHIDevice::CreateTexture(const TextureDesc& desc)
	{
		return CreateRef<OpenGLRHITexture2D>(desc);
	}

	Ref<RHISampler> OpenGLRHIDevice::CreateSampler(const SamplerDesc& desc)
	{
		return CreateRef<OpenGLRHISampler>(desc);
	}

	Ref<RHIShaderModule> OpenGLRHIDevice::CreateShaderModule(
		const void* sourceBytes, uint32_t byteSize, const std::string& debugName)
	{
		// Treat the RHI "bytecode" payload as an embedded GLSL source string
		// for the OpenGL path.  This is intentional: the same RHIDevice
		// interface is shared across backends; DXBC/SPIR-V bytecode travel the
		// same channel but we reinterpret as text here.
		if (!sourceBytes || byteSize == 0)
		{
			CANDY_CORE_ERROR("OpenGLRHIDevice::CreateShaderModule: null source for '{}'", debugName);
			return nullptr;
		}
		std::string source(static_cast<const char*>(sourceBytes), byteSize);
		return CreateRef<OpenGLRHIShaderModule>(ShaderStage::None, std::move(source), debugName);
	}

	Ref<RHIGraphicsPipeline> OpenGLRHIDevice::CreateGraphicsPipeline(
		const GraphicsPipelineDesc& desc,
		const Ref<RHIShaderModule>& vs,
		const Ref<RHIShaderModule>& fs)
	{
		auto extract = [](const Ref<RHIShaderModule>& mod) -> std::string {
			if (!mod) return {};
			return std::string(reinterpret_cast<const char*>(mod->GetBytecode()), mod->GetBytecodeSize());
		};

		std::string vsSource = extract(vs);
		std::string fsSource = extract(fs);

		// If only one module holds the merged "#type vertex/fragment" source
		// (Hazel-style multi-stage GLSL), parse it now.
		if (!fsSource.empty() && vsSource == fsSource)
		{
			auto parsed = RHIShaderSource::Parse(vsSource);
			vsSource = parsed.count(ShaderStage::Vertex)   ? parsed[ShaderStage::Vertex]   : std::string{};
			fsSource = parsed.count(ShaderStage::Fragment) ? parsed[ShaderStage::Fragment] : std::string{};
		}

		GLuint program = glCreateProgram();

		auto compileStage = [&](const std::string& source, GLenum stage, const char* tag) -> GLuint {
			if (source.empty()) return 0;
			GLuint shader = glCreateShader(stage);
			const char* code = source.c_str();
			glShaderSource(shader, 1, &code, nullptr);
			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if (isCompiled == GL_FALSE)
			{
				GLint logLen = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
				std::vector<GLchar> log(logLen > 0 ? logLen : 1);
				glGetShaderInfoLog(shader, logLen, &logLen, log.data());
				CANDY_CORE_ERROR("OpenGLRHIDevice: {} compile error:\n{}", tag, log.data());
				glDeleteShader(shader);
				return 0;
			}
			return shader;
		};

		GLuint vsShader = compileStage(vsSource, GL_VERTEX_SHADER,   "vertex shader");
		GLuint fsShader = compileStage(fsSource, GL_FRAGMENT_SHADER, "fragment shader");

		if (!vsShader || !fsShader)
		{
			CANDY_CORE_ERROR("OpenGLRHIDevice::CreateGraphicsPipeline: shader stage missing/failed");
			if (vsShader) glDeleteShader(vsShader);
			if (fsShader) glDeleteShader(fsShader);
			glDeleteProgram(program);
			return nullptr;
		}

		glAttachShader(program, vsShader);
		glAttachShader(program, fsShader);
		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint logLen = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
			std::vector<GLchar> log(logLen > 0 ? logLen : 1);
			glGetProgramInfoLog(program, logLen, &logLen, log.data());
			CANDY_CORE_ERROR("OpenGLRHIDevice: program link error:\n{}", log.data());
			glDeleteShader(vsShader);
			glDeleteShader(fsShader);
			glDeleteProgram(program);
			return nullptr;
		}

		glDetachShader(program, vsShader);
		glDetachShader(program, fsShader);
		glDeleteShader(vsShader);
		glDeleteShader(fsShader);

		auto pipeline = CreateRef<OpenGLRHIGraphicsPipeline>(desc);
		pipeline->SetProgram(program);
		return pipeline;
	}

	Ref<RHISwapChain> OpenGLRHIDevice::CreateSwapChain(const SwapChainDesc& desc)
	{
		return CreateRef<OpenGLRHISwapChain>(desc);
	}

	RHICommandQueue& OpenGLRHIDevice::GetCommandQueue()
	{
		return *m_Queue;
	}

	void OpenGLRHIDevice::WaitIdle()
	{
		glFinish();
	}

} // namespace Candy
