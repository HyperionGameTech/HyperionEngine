#pragma once

#include <Core/FxTypes.hpp>
#include <format>
#include <string>

class FxString
{
    static constexpr uint32 scStackAllocSize = 32;

public:
    static FxString NoCopy(char* ptr, uint32 length);

    FxString() = default;
    FxString(uint32 allocation_size);
    FxString(const char* str, uint32 length);
    FxString(const std::string& str);
    FxString(const char* str);

    FX_FORCE_INLINE bool IsHeapAllocated() const { return (mpHeapStr != nullptr); }

    FX_FORCE_INLINE uint32 GetLength() const { return Length; }

    const char* CStr() const
    {
        if (IsHeapAllocated()) {
            return mpHeapStr;
        }

        return mpStackStr;
    }

    FxString& operator=(const char* str);

    FxString operator+(const FxString& other) const;
    FxString operator+(const char* other) const;

    const char operator[](size_t index) const;
    char& operator[](size_t index);

    ~FxString();

private:
    FX_FORCE_INLINE char* GetInternalPtr()
    {
        if (IsHeapAllocated()) {
            return mpHeapStr;
        }

        return mpStackStr;
    }

public:
    uint32 Length = 0;

private:
    char mpStackStr[scStackAllocSize];
    char* mpHeapStr = nullptr;
};


template <>
struct std::formatter<FxString>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxString& str, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{}", str.CStr());
    }
};
