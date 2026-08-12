#pragma once

#include <cstdint>

namespace Candy {

	// =========================================================================
	// RHIFence — CPU-GPU synchronization primitive (binary signaled state)
	// =========================================================================
	class RHIFence
	{
	public:
		virtual ~RHIFence() = default;

		/// Blocks the CPU until the fence is signaled by the GPU.
		/// `timeoutNanoseconds` = 0 means "poll", UINT64_MAX means "forever".
		virtual void Wait(uint64_t timeoutNanoseconds = UINT64_MAX) = 0;

		/// Resets the fence back to the unsignaled state (CPU-side).
		virtual void Reset() = 0;

		/// Queries whether the fence is currently signaled (non-blocking).
		virtual bool IsSignaled() const = 0;
	};

	// =========================================================================
	// RHISemaphore — GPU-GPU synchronization primitive (binary / timeline)
	// =========================================================================
	class RHISemaphore
	{
	public:
		virtual ~RHISemaphore() = default;
	};

} // namespace Candy
