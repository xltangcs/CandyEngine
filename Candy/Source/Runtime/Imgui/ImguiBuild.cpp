#include "CandyPCH.h"

// Include Vulkan headers up-front on Windows so that VULKAN_H_ is defined before
// imgui_impl_glfw.cpp is pulled in. imgui_impl_glfw.cpp otherwise emits its own
// minimal `enum VkResult` guard under #ifndef VULKAN_H_, which would collide
// with the real definition coming from imgui_impl_vulkan.cpp in this same
// compilation unit.
#ifdef CANDY_PLATFORM_WINDOWS
#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <backends/imgui_impl_vulkan.h>
#endif

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <backends/imgui_impl_opengl3.cpp>
#include <backends/imgui_impl_glfw.cpp>

#ifdef CANDY_PLATFORM_WINDOWS
// D3D12 renderer backend (Windows only). ImGuiLayer routes ImGui_ImplDX12_*
// here when RendererAPI::API::D3D12 is active.
#include <backends/imgui_impl_dx12.cpp>

// Vulkan renderer backend (Windows only). ImGuiLayer calls
// ImGui_ImplVulkan_LoadFunctions() to resolve entry points, so prototypes are
// disabled via IMGUI_IMPL_VULKAN_NO_PROTOTYPES (defined above before the
// Vulkan header is pulled in).
#include <backends/imgui_impl_vulkan.cpp>
#endif
