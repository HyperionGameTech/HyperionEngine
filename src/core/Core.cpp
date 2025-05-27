/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/Core.hpp>

#include <core/object/HypClass.hpp>

namespace hyperion {

const HypClass* GetClass(TypeID type_id)
{
    return HypClassRegistry::GetInstance().GetClass(type_id);
}

const HypClass* GetClass(WeakName type_name)
{
    return HypClassRegistry::GetInstance().GetClass(type_name);
}

const HypEnum* GetEnum(TypeID type_id)
{
    return HypClassRegistry::GetInstance().GetEnum(type_id);
}

const HypEnum* GetEnum(WeakName type_name)
{
    return HypClassRegistry::GetInstance().GetEnum(type_name);
}

bool IsInstanceOfHypClass(const HypClass* hyp_class, const void* ptr, TypeID type_id)
{
    if (!hyp_class)
    {
        return false;
    }

    if (hyp_class->GetTypeID() == type_id)
    {
        return true;
    }

    const HypClass* other_hyp_class = GetClass(type_id);

    if (other_hyp_class != nullptr)
    {
        if (other_hyp_class->GetStaticIndex() != -1)
        {
            return uint32(other_hyp_class->GetStaticIndex() - hyp_class->GetStaticIndex()) <= hyp_class->GetNumDescendants();
        }

        // Try to get the initializer. If we can get it, use the instance class rather than just the class for the type ID.
        if (const IHypObjectInitializer* initializer = other_hyp_class->GetObjectInitializer(ptr))
        {
            other_hyp_class = initializer->GetClass();
        }
    }

    while (other_hyp_class != nullptr)
    {
        if (other_hyp_class == hyp_class)
        {
            return true;
        }

        other_hyp_class = other_hyp_class->GetParent();
    }

    return false;
}

bool IsInstanceOfHypClass(const HypClass* hyp_class, const HypClass* instance_hyp_class)
{
    if (!hyp_class || !instance_hyp_class)
    {
        return false;
    }

    if (instance_hyp_class->GetStaticIndex() != -1)
    {
        return uint32(instance_hyp_class->GetStaticIndex() - hyp_class->GetStaticIndex()) <= hyp_class->GetNumDescendants();
    }

    do
    {
        if (instance_hyp_class == hyp_class)
        {
            return true;
        }

        instance_hyp_class = instance_hyp_class->GetParent();
    }
    while (instance_hyp_class != nullptr);

    return false;
}

} // namespace hyperion