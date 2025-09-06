#include <script/vm/GC.hpp>

#include <core/object/HypData.hpp>

namespace hyperion {

extern Script_Value ScriptApi_MakeValue(const Script_VMData& data);

Script_GC::Script_GC()
    : m_pool(NAME("GCMemoryPool"))
{
}

Script_GC::~Script_GC()
{
    // clear all allocated memory
    /// @TODO: Need to set Script_GC index of all Values to INVALID_GC_INDEX then destruct
}

void Script_GC::MoveToTrackedMemory(Script_Value& inOutRefValue)
{
    Assert(inOutRefValue.m_gcIndex == INVALID_GC_INDEX);
    Assert(!inOutRefValue.IsRef());

    Script_Value* ptr;
    uint32 gcIndex = m_pool.AcquireIndex(&ptr) + 1; // reserve 0 for INVALID_GC_INDEX

    *ptr->GetHypData() = std::move(*inOutRefValue.GetHypData());
    ptr->m_gcIndex = GCIndex(gcIndex);
    
    // set `inOutRefValue` to be a reference to the tracked value
    Script_VMData newRefVmData;
    newRefVmData.type = Script_VMData::VALUE_REF;
    newRefVmData.valueRef = ptr;
    
    inOutRefValue = ScriptApi_MakeValue(newRefVmData);
}

} // namespace hyperion
