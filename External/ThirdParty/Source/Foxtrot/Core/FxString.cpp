#include "FxString.hpp"

#include <Core/FxLog.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>

FxString FxString::NoCopy(char* ptr, uint32 length)
{
    FxString result;
    result.mpHeapStr = ptr;
    result.Length = length;
    return result;
}

FxString::FxString(const char* str, uint32 length)
{
    Length = length;

    char* dst = mpStackStr;


    // If the size cannot fit into the stack string, allocate a buffer for it
    if (length >= scStackAllocSize) {
        mpHeapStr = gEnginePool->Alloc<char>(length + 1);
        dst = mpHeapStr;
    }

    memcpy(dst, str, length);
    dst[length] = 0;
}

FxString::FxString(const char* str) : FxString(str, strlen(str)) {}

FxString::FxString(const std::string& str) : FxString(str.data(), str.length()) {}

FxString::FxString(uint32 allocation_size)
{
    Length = allocation_size;

    // If the size cannot fit into the stack string, allocate a buffer for it
    if (Length >= scStackAllocSize) {
        mpHeapStr = gEnginePool->Alloc<char>(Length + 1);
    }
}


FxString FxString::operator+(const FxString& other) const
{
    // +1 for null terminator
    const uint32 result_len = Length + other.Length;
    FxString result(result_len);

    char* dst = result.GetInternalPtr();
    memcpy(dst, CStr(), Length);
    memcpy(dst + Length, other.CStr(), other.Length);
    dst[result_len] = '\0';

    return result;
}

FxString FxString::operator+(const char* other) const
{
    const uint32 other_len = strlen(other);

    // +1 for null terminator
    const uint32 result_len = Length + other_len;
    FxString result(result_len);

    char* dst = result.GetInternalPtr();
    memcpy(dst, CStr(), Length);
    memcpy(dst + Length, other, other_len);
    dst[result_len] = '\0';

    return result;
}


FxString& FxString::operator=(const char* str)
{
    const uint32 new_len = strlen(str);

    char* dst = mpStackStr;

    if (new_len >= scStackAllocSize && new_len > Length) {
        if (mpHeapStr == nullptr) {
            mpHeapStr = gEnginePool->Alloc<char>(new_len);
        }
        else {
            mpHeapStr = gEnginePool->Realloc(mpHeapStr, new_len);
        }
        dst = mpHeapStr;
    }

    memcpy(dst, str, new_len);


    Length = new_len;
    dst[Length] = 0;

    return *this;
}


const char FxString::operator[](size_t index) const { return CStr()[index]; }
char& FxString::operator[](size_t index) { return GetInternalPtr()[index]; }

FxString::~FxString()
{
    if (mpHeapStr) {
        gEnginePool->Free<char>(mpHeapStr);
    }

    mpHeapStr = nullptr;
    Length = 0;
}
