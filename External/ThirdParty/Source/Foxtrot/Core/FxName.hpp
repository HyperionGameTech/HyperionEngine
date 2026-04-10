#pragma once

#include <Core/FxHash.hpp>


class FxName
{
public:
    FxName() = default;
    FxName(const char* name) : mName(name), mHash(FxHashStr64(name)) {}
    FxName(const std::string& name) : mName(name), mHash(FxHashStr64(name.c_str())) {}

    const std::string& Get() const { return mName; }
    FxHash64 GetHash() const { return mHash; }

    void Set(const std::string& name)
    {
        mName = name;
        mHash = FxHashStr64(name.c_str());
    }

    void Set(const char* name)
    {
        mName = name;
        mHash = FxHashStr64(name);
    }

    FxName& operator=(const char* name)
    {
        Set(name);
        return *this;
    }

    FxName& operator=(const std::string& name)
    {
        Set(name);
        return *this;
    }

    bool operator==(FxHash64 hash) const { return mHash == hash; }

    void Clear()
    {
        mName.clear();
        mHash = FxHashNull64;
    }

private:
    std::string mName = "";
    FxHash64 mHash = FxHashNull64;
};
