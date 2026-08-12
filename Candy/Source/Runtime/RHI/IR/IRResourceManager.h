#pragma once

#include "Runtime/RHI/IR/IRTypes.h"

#include <unordered_map>
#include <string>

namespace Candy::IR {

	// =========================================================================
	// IRResourceManager — centralized handle → resource registry
	//
	// All GPU resources created through IRDevice are registered here with
	// metadata (type, name, state).  This enables:
	//  - Resource lifetime tracking and leak detection
	//  - Barrier generation (know current state → compute target state)
	//  - Debug naming across all backends
	// =========================================================================
	class IRResourceManager
	{
	public:
		struct ResourceEntry
		{
			ResourceType  Type   = ResourceType::Unknown;
			ResourceState State  = ResourceState::Undefined;
			std::string   Name;
			void*         RawPtr = nullptr; ///< opaque pointer to backend resource
		};

		IRResourceManager() = default;
		~IRResourceManager();

		// ---- Registration --------------------------------------------------

		/// Register a new resource; returns its handle.
		Candy::RHIHandle Register(ResourceType type, void* rawPtr, std::string_view name = "");

		/// Unregister a resource by handle.  Asserts if handle not found.
		void Unregister(Candy::RHIHandle handle);

		// ---- Lookup --------------------------------------------------------

		[[nodiscard]] const ResourceEntry* Find(Candy::RHIHandle handle) const;
		[[nodiscard]] ResourceEntry*       Find(Candy::RHIHandle handle);

		[[nodiscard]] ResourceType  GetType(Candy::RHIHandle handle) const;
		[[nodiscard]] ResourceState GetState(Candy::RHIHandle handle) const;

		// ---- Templated backend-pointer accessor ----------------------------

		template<typename T>
		[[nodiscard]] T* GetAs(Candy::RHIHandle handle)
		{
			if (auto* entry = Find(handle))
				return static_cast<T*>(entry->RawPtr);
			return nullptr;
		}

		template<typename T>
		[[nodiscard]] const T* GetAs(Candy::RHIHandle handle) const
		{
			if (const auto* entry = Find(handle))
				return static_cast<const T*>(entry->RawPtr);
			return nullptr;
		}

		// ---- State tracking ------------------------------------------------

		void SetState(Candy::RHIHandle handle, ResourceState newState);

		// ---- Query ---------------------------------------------------------

		[[nodiscard]] size_t GetResourceCount() const { return m_Resources.size(); }
		[[nodiscard]] bool   IsRegistered(Candy::RHIHandle handle) const;

	private:
		std::unordered_map<Candy::RHIHandle, ResourceEntry> m_Resources;
		uint32_t m_NextHandle = 1;
	};

} // namespace Candy::IR
