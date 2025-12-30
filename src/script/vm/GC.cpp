#include <script/vm/GC.hpp>

#include <core/reflection/BoxedValue.hpp>

namespace Hyperion {

extern BoxedValue MakeValue(const Script_VMData& data);

Script_GC::Script_GC()
    : m_allocator(sizeof(BoxedValue), alignof(BoxedValue))
{
}

Script_GC::~Script_GC()
{
    // clear all allocated memory
    //// \todo : Need to set Script_GC index of all Values to INVALID_GC_INDEX then destruct
}

void Script_GC::MoveToTrackedMemory(BoxedValue& inOutRefValue)
{
    AssertDebug(inOutRefValue.extData.gcIndex == INVALID_GC_INDEX);
    AssertDebug(!IsRef(inOutRefValue));

    BoxedValue* ptr = (BoxedValue*)m_allocator.Allocate();
    AssertDebug(ptr != nullptr, "Failed to allocate memory for tracked BoxedValue");

    uint32 gcIndex = m_idGenerator.Next(); // starts at 1
    AssertDebug(gcIndex <= uint32(MAX_GC_INDEX), "Exceeded maximum number of tracked GC objects!");

    new (ptr) BoxedValue(std::move(inOutRefValue));
    ptr->extData.gcIndex = GCIndex(gcIndex);

    // set `inOutRefValue` to be a reference to the tracked value
    Script_VMData newRefVmData;
    newRefVmData.type = Script_VMData::VALUE_REF;
    newRefVmData.valueRef = ptr;

    inOutRefValue = MakeValue(newRefVmData);
}

} // namespace Hyperion
