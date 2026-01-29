#pragma once

#include <cstdint>
#include <Windows.h>

namespace Memory
{
	// Safe pointer dereference
	template<typename T>
	inline T Read(uintptr_t address)
	{
		if (!address) return T{};
		__try
		{
			return *reinterpret_cast<T*>(address);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return T{};
		}
	}

	// Safe pointer write
	template<typename T>
	inline bool Write(uintptr_t address, const T& value)
	{
		if (!address) return false;
		__try
		{
			*reinterpret_cast<T*>(address) = value;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	// Utility function for address calculation
	inline uintptr_t GetAddress(uintptr_t base)
	{
		return base;
	}
}
