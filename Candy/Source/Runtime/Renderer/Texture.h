#pragma once

#include <string>
#include <Runtime/Core/Base.h>

namespace Candy {

	class RHITexture;

	class Texture : public std::enable_shared_from_this<Texture>
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		/// Returns 64-bit renderer/GPU handle for ImGui::Image().
		/// In OpenGL: zero-extended GLuint. In D3D12: D3D12_GPU_DESCRIPTOR_HANDLE.ptr.
		virtual uint64_t GetRendererID64() const = 0;

		virtual void SetData(void* data, uint32_t size) = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual bool IsLoaded() const = 0;
		virtual bool operator==(const Texture& other) const = 0;

		/// RHI view of this texture, for command-buffer binding
		/// (RHICommandBuffer::SetTexture). Backends either return themselves
		/// (when the texture class already is-a RHITexture, e.g. OpenGL) or
		/// the RHI texture they own (D3D12). May return nullptr on backends
		/// without RHI texture support (frozen Vulkan path).
		virtual Ref<RHITexture> GetRHITexture() = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(const std::string& path);
	};
}
