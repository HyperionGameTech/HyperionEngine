#include <script/vm/GC.hpp>

#include <core/reflection/HypData.hpp>

namespace hyperion {

extern HypData ScriptApi_MakeValue(const Script_VMData& data);

Script_GC::Script_GC()
    : m_allocator(sizeof(HypData), alignof(HypData))
{
}

Script_GC::~Script_GC()
{
    // clear all allocated memory
    //// \todo : Need to set Script_GC index of all Values to INVALID_GC_INDEX then destruct
}

void Script_GC::MoveToTrackedMemory(HypData& inOutRefValue)
{
    Assert(inOutRefValue.extData.scriptGcIndex == INVALID_GC_INDEX);
    Assert(!IsRef(inOutRefValue));

    HypData* ptr = (HypData*)m_allocator.Allocate();
    uint32 gcIndex = m_idGenerator.Next(); // starts at 1

    new (ptr) HypData(std::move(inOutRefValue));
    ptr->extData.scriptGcIndex = GCIndex(gcIndex);

    // set `inOutRefValue` to be a reference to the tracked value
    Script_VMData newRefVmData;
    newRefVmData.type = Script_VMData::VALUE_REF;
    newRefVmData.valueRef = ptr;

    inOutRefValue = ScriptApi_MakeValue(newRefVmData);
}

} // namespace hyperion
