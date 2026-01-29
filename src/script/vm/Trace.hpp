#pragma once

#include <core/Core.hpp>

struct Script_Trace
{
    int callAddresses[10];

    Script_Trace()
    {
        Hyperion::Memory::Fill(callAddresses, 0, sizeof(callAddresses));
    }

    Script_Trace(const Script_Trace& other)
    {
        Hyperion::Memory::Copy(callAddresses, other.callAddresses, sizeof(callAddresses));
    }

    Script_Trace& operator=(const Script_Trace& other)
    {
        Hyperion::Memory::Copy(callAddresses, other.callAddresses, sizeof(callAddresses));

        return *this;
    }

    ~Script_Trace() = default;
};
