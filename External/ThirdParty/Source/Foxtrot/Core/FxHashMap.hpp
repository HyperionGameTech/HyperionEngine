#pragma once

#include "FxHash.hpp"

#include <cstdlib>

template <typename TKeyType, typename TValueType>
class FxHashMap
{
    static constexpr uint32 scInitialCapacity = 16;

    using KeyHashType = FxHash64;

    struct Entry
    {
        KeyHashType Key;
        TValueType Value;
    };

public:
    FxHashMap() { mpEntries = std::malloc(sizeof(Entry) * scInitialCapacity); }

    void Set(TKeyType key) {}


private:
    Entry* mpEntries = nullptr;
};
