/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#if 0

#include <core/object/HypPropertySerializer.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypPropertySerializerRegistry

HypPropertySerializerRegistry &HypPropertySerializerRegistry::GetInstance()
{
    static HypPropertySerializerRegistry instance;

    return instance;
}

void HypPropertySerializerRegistry::RegisterSerializer(TypeID type_id, IHypPropertySerializer *serializer)
{
    AssertThrow(serializer != nullptr);

    const auto it = m_serializers.Find(type_id);
    AssertThrowMsg(it == m_serializers.End(), "Serializer already registered for type ID %u", type_id.Value());

    m_serializers.Set(type_id, serializer);
}

IHypPropertySerializer *HypPropertySerializerRegistry::GetSerializer(TypeID type_id)
{
    const auto it = m_serializers.Find(type_id);

    if (it == m_serializers.End()) {
        return nullptr;
    }

    return it->second;
}

#pragma endregion HypPropertySerializerRegistry

#pragma region Default serializers

#pragma endregion Default serializers

} // namespace hyperion

#endif