/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClassPropertySerializer.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypClassPropertySerializerRegistry

HypClassPropertySerializerRegistry &HypClassPropertySerializerRegistry::GetInstance()
{
    static HypClassPropertySerializerRegistry instance;

    return instance;
}

void HypClassPropertySerializerRegistry::RegisterSerializer(TypeID type_id, IHypClassPropertySerializer *serializer)
{
    AssertThrow(serializer != nullptr);

    const auto it = m_serializers.Find(type_id);
    AssertThrowMsg(it == m_serializers.End(), "Serializer already registered for type ID %u", type_id.Value());

    m_serializers.Set(type_id, serializer);
}

IHypClassPropertySerializer *HypClassPropertySerializerRegistry::GetSerializer(TypeID type_id)
{
    const auto it = m_serializers.Find(type_id);

    if (it == m_serializers.End()) {
        return nullptr;
    }

    return it->second;
}

#pragma endregion HypClassPropertySerializerRegistry

#pragma region Default serializers

#pragma endregion Default serializers

} // namespace hyperion