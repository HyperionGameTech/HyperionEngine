#pragma once

#include <Core/FxDynArray.hpp>
#include <Core/FxSlice.hpp>
#include <Core/FxTypes.hpp>
#include <Renderer/Backend/RxShader.hpp>
#include <vector>
#include <array>


namespace FxShaderPreproc {


using DataBuffer = FxDynArray<char, FxGrowthFunctions::InPages>;

enum EntryType : uint16
{
    eStructuredBuffer,
    eUniformBuffer,
    eSampler2D,
};

struct Entry
{
    Entry() = delete;
    Entry(EntryType type, uint8 set, uint8 binding) : Type(type), Set(set), Binding(binding) {}

    EntryType Type;
    uint8 Set;
    uint8 Binding;
};


struct Result
{
public:
    Result() = default;

    DataBuffer& GetBuffer() { return ProgramData[static_cast<uint32>(CurrentType)]; }
    DataBuffer& GetBuffer(RxShaderType type) { return ProgramData[static_cast<uint32>(type)]; }

    void InsertString(const std::string& str)
    {
        DataBuffer& buffer = GetBuffer();
        for (const char ch : str) {
            buffer.Insert(ch);
        }
    }

    void SetCurrentShader(RxShaderType type)
    {
        bBroadcastToAllPrograms = false;
        CurrentType = type;
    }

public:
    std::array<DataBuffer, RxShaderUtil::scNumShaderTypes> ProgramData;

    RxShaderType CurrentType = RxShaderType::eVertex;

    // Before a program definition, broadcast to all
    bool bBroadcastToAllPrograms = true;

    std::vector<Entry> Reflection;
};


/**
 * @brief Processes a shader file and retrieves each program and associated reflection data.
 */
Result Process(const FxSlice<char>& data, const FxSizedArray<FxShaderMacro>& macros);
void DebugSaveToDisk(const char* name, const Result& result);

}; // namespace FxShaderPreproc
