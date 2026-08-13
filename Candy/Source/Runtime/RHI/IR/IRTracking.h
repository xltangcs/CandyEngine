#pragma once

#include "Runtime/RHI/IR/IRTypes.h"
#include "Runtime/RHI/IR/IRResourceManager.h"

namespace Candy {

	// =========================================================================
	// IRTrackable — optional resource-lifetime tracking mixin for the IR
	// resource manager. Backends attach a manager+handle at creation; the
	// resource unregisters itself on destruction. Null-safe when unused.
	// =========================================================================
	class IRTrackable
	{
	public:
		virtual ~IRTrackable()
		{
			if (m_IRManager && m_IRHandle.Value != 0)
				m_IRManager->Unregister(m_IRHandle);
		}

		void SetIRTracking(IR::IRResourceManager* mgr, RHIHandle handle)
		{
			m_IRManager = mgr;
			m_IRHandle  = handle;
		}

	private:
		IR::IRResourceManager* m_IRManager = nullptr;
		RHIHandle              m_IRHandle{};
	};

} // namespace Candy
