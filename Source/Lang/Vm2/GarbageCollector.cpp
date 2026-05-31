#include <Lang/vm/GarbageCollector.hpp>

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/GenericArrayWrapper.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/StaticField.hpp>

namespace Hyperion {

extern BoxedValue MakeValue(const ScriptObjectData& data);

static inline ScriptObjectData* GetVMData(BoxedValue& value)
{
    return reinterpret_cast<ScriptObjectData*>(value.TryGet<BoxedValue::InlineData>().TryGet());
}

GarbageCollector::GarbageCollector()
{
}

GarbageCollector::~GarbageCollector()
{
    for (auto it = m_trackedObjects.Begin(); it != m_trackedObjects.End(); ++it)
    {
        TrackedStorage& storage = *it;
        storage.Get().extData.gcIndex = INVALID_GC_INDEX;
        storage.Destruct();
    }

    m_trackedObjects.Clear();
    m_marks.Clear();
}

void GarbageCollector::MoveToTrackedMemory(BoxedValue& inOutRefValue)
{
    AssertDebug(inOutRefValue.extData.gcIndex == INVALID_GC_INDEX);
    AssertDebug(!IsRef(inOutRefValue));

    uint32 gcIndex = m_idGenerator.Next(); // starts at 1
    AssertDebug(gcIndex <= uint32(MAX_GC_INDEX), "Exceeded maximum number of tracked GC objects!");

    AssertDebug(!m_trackedObjects.HasIndex(gcIndex));

    TrackedStorage& storage = *m_trackedObjects.Emplace(gcIndex);
    BoxedValue* ptr = storage.Construct(std::move(inOutRefValue));
    ptr->extData.gcIndex = GCIndex(gcIndex);

    // set `inOutRefValue` to be a reference to the tracked value
    ScriptObjectData newRefData {};
    newRefData.type = ScriptObjectData::Type::Reference;
    newRefData.valueRef = ptr;

    inOutRefValue = MakeValue(newRefData);
}

void GarbageCollector::ClearMarks()
{
    m_marks.Clear();
}

void GarbageCollector::MarkReachable(Span<BoxedValue> values)
{
    for (BoxedValue& value : values)
    {
        MarkReachable(value);
    }
}

void GarbageCollector::Collect()
{
    // destruct in reverse order
    Array<ValueStorage<BoxedValue>, ScriptAllocator> toDestroy;

    auto it = m_trackedObjects.Begin();
    while (it != m_trackedObjects.End())
    {
        TrackedStorage& storage = *it;
        BoxedValue& bv = storage.Get();

        const uint32 gcIndex = uint32(bv.extData.gcIndex);

        if (m_marks.Test(gcIndex))
        {
            m_marks.Set(gcIndex, false); // reset for next collection
            ++it;

            continue;
        }

        if (!IsGarbage(storage.Get()))
        {
            toDestroy.PushBack(storage);
        }
        
        ++it;
    }

    for (size_t i = toDestroy.Size(); i != 0; i--)
    {
        TrackedStorage& storage = toDestroy[i - 1];
        BoxedValue& bv = storage.Get();

        const uint32 gcIndex = uint32(bv.extData.gcIndex);

        storage.Get().extData.gcIndex = INVALID_GC_INDEX;
        storage.Destruct();

        m_trackedObjects.EraseAt(gcIndex);

        m_idGenerator.ReleaseId(gcIndex);
    }
}

void GarbageCollector::Collect(Span<BoxedValue> roots)
{
    ClearMarks();
    MarkReachable(roots);
    Collect();
}

void GarbageCollector::MarkReachable(BoxedValue& value)
{
    if (!value.IsValid() || IsGarbage(value))
    {
        return;
    }

    BoxedValue* deref = Deref(value);
    if (!deref || deref->extData.gcIndex == INVALID_GC_INDEX)
    {
        return;
    }

    const uint32 gcIndex = uint32(deref->extData.gcIndex);

    if (m_marks.Test(gcIndex))
    {
        return; // already marked
    }

    m_marks.Set(gcIndex, true);

    TrackedStorage* storagePtr = m_trackedObjects.TryGet(gcIndex);
    if (storagePtr == nullptr)
    {
        return;
    }

    BoxedValue& tracked = storagePtr->Get();

    // Follow references from this tracked object
    if (ScriptObjectData* data = GetVMData(tracked))
    {
        if (data->type == ScriptObjectData::Type::Reference && data->valueRef != nullptr)
        {
            MarkReachable(*data->valueRef);
        }
    }

    // Mark object fields
    else if (const Class* cls = tracked.ToRef().GetClass())
    {
        for (const IMember& member : cls->GetMembers(MemberType::Field, /* deep */ true))
        {
            const Field& field = static_cast<const Field&>(member);

            BoxedValue fieldValue = field.Get(tracked);

            if (fieldValue.IsValid())
            {
                MarkReachable(fieldValue);
            }
        }
    }
    // Mark array elements
    else if (GenericArrayWrapper* array = tracked.TryGet<GenericArrayWrapper>().TryGet())
    {
        if (array->CanGetElementByIndex())
        {
            for (size_t j = 0; j < array->Size(); j++)
            {
                AnyRef element = array->GetElementAt(j);
                if (element.HasValue())
                {
                    if (element.GetTypeId() == TypeId::ForType<BoxedValue>())
                    {
                        MarkReachable(element.Get<BoxedValue>());
                    }
                }
            }
        }
    }
    // Handle ClassRef static fields
    else if (const ClassRef* classRef = tracked.TryGet<ClassRef>().TryGet())
    {
        for (const IMember& member : (*classRef)->GetMembers(MemberType::StaticField, /* deep */ true))
        {
            const StaticField& staticField = static_cast<const StaticField&>(member);

            BoxedValue value = staticField.Get();

            if (value.IsValid())
            {
                MarkReachable(value);
            }
        }
    }
}

} // namespace Hyperion
