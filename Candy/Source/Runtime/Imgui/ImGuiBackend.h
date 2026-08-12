#pragma once

#include "Runtime/Core/Base.h"

struct GLFWwindow;
struct ImDrawData;

namespace Candy {

	class Framebuffer;

	// =========================================================================
	// ImGuiBackend — graphics-API adapter for ImGui rendering
	//
	// Owned by ImGuiLayer; one instance per process, created via Create().
	// Both ImGui contexts (editor + game UI) share the instance — the backend
	// distinguishes per-context state via ImGui::GetCurrentContext().
	//
	// UI-layer code never touches D3D12/Vulkan/OpenGL types: command lists,
	// descriptor heaps, swap-chain present and friends live exclusively in the
	// Platform/* implementations.
	// =========================================================================
	class ImGuiBackend
	{
	public:
		virtual ~ImGuiBackend() = default;

		/// One-time init: GLFW platform init + shared renderer resources.
		/// Called with the EDITOR context current.
		virtual void Init(GLFWwindow* window) = 0;
		/// Per-context renderer init (ImGui_ImplXXX_Init binds the CURRENT context).
		virtual void InitContext() = 0;
		/// Per-context renderer shutdown (CURRENT context).
		virtual void ShutdownContext() = 0;
		/// One-time shutdown of shared resources + GLFW platform backend.
		virtual void Shutdown() = 0;

		/// Whether this backend supports ImGui multi-viewport platform windows
		/// (only the OpenGL/GLFW path wires UpdatePlatformWindowsDefault).
		virtual bool SupportsPlatformWindows() const = 0;

		/// Start a backend frame for the editor context (renderer + GLFW platform).
		virtual void NewFrame() = 0;
		/// Start a backend frame for the game UI context (renderer only — the game
		/// UI overlays the viewport and must not advance the GLFW platform frame).
		virtual void NewFrameGameUI() = 0;

		/// Render draw data of the CURRENT context. target == nullptr renders to
		/// the swap chain and presents (editor main pass); otherwise composites
		/// into the given framebuffer (game UI overlay; no present).
		virtual void RenderDrawData(ImDrawData* drawData, Framebuffer* target) = 0;

		static Scope<ImGuiBackend> Create();
	};
}
