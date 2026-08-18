#pragma once

#include "Runtime/RHI/Shared/RHISharedTypes.h"
#include "Runtime/RHI/Shared/RHIResourceManager.h"

namespace Candy {

	// =========================================================================
	// RHITrackable — optional resource-lifetime tracking mixin for the RHI
	// resource manager. Backends attach a manager+handle at creation; the
	// resource unregisters itself on destruction. Null-safe when unused.
	// =========================================================================
	class RHITrackable
	{
	public:
		virtual ~RHITrackable()
		{
			if (m_ResourceManager && m_TrackingHandle.Value != 0)
				m_ResourceManager->Unregister(m_TrackingHandle);
		}

		void SetRHITracking(RHIResourceManager* mgr, RHIHandle handle)
		{
			m_ResourceManager = mgr;
			m_TrackingHandle  = handle;
		}

		/// Detach from the resource manager. Called by RHIResourceManager on
		/// destruction so trackables that outlive the device never touch a
		/// destroyed manager (static caches etc.).
		void ClearRHITracking()
		{
			m_ResourceManager = nullptr;
			m_TrackingHandle  = {};
		}

	private:
		RHIResourceManager* m_ResourceManager = nullptr;
		RHIHandle           m_TrackingHandle{};
	};

} // namespace Candy
