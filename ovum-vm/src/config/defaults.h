#pragma once

#include "../vm.h"

namespace ovum
{

namespace config
{
	class Defaults
	{
	public:
		// The size of the (pre-allocated) generation 0 chunk.
		// (1.5 MB)
		static const size_t GEN0_SIZE = 1536 * 1024;

		// The maximum amount of garbage allowed in generation 1 before the GC
		// forces it to be collected.
		// (768 kB)
		static const size_t GEN1_DEAD_OBJECT_THRESHOLD = 768 * 1024;

		// The size of the managed call stack.
		// (4 MB)
		static const size_t CALL_STACK_SIZE = 4096 * 1024;
	};
} // namespace ovum::config

} // namespace ovum
