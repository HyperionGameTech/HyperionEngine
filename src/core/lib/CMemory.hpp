#ifndef HYPERION_V2_LIB_C_MEMORY_HPP
#define HYPERION_V2_LIB_C_MEMORY_HPP

#include <cstring>

namespace hyperion
{

class Memory
{
public:
	static char *CopyString(char *dest, const char *src, size_t length=0)
	{
		if (length)
			return strncpy(dest, src, length);
		return std::strcpy(dest, src);
	}

	static inline void *Set(void *dest, int ch, size_t length)
	{
		return std::memset(dest, ch, length);
	}

	static inline void *Copy(void *dest, const void *src, size_t size) 
	{
		return std::memcpy(dest, src, size);
	}
};

}



#endif