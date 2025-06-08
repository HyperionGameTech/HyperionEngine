/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOM.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypData.hpp>

namespace hyperion::serialization {

FBOMObject::FBOMObject()
    : m_object_type(FBOMBaseObjectType())
{
}

FBOMObject::FBOMObject(const FBOMType& loader_type)
    : m_object_type(loader_type)
{
    AssertThrowMsg(loader_type.IsOrExtends(FBOMBaseObjectType()), "Expected type to be an object type, got %s", loader_type.ToString().Data());
}

FBOMObject::FBOMObject(const FBOMObject& other)
    : m_object_type(other.m_object_type),
      m_children(other.m_children),
      properties(other.properties),
      m_deserialized_object(other.m_deserialized_object),
      m_external_info(other.m_external_info),
      m_unique_id(other.m_unique_id)
{
}

FBOMObject& FBOMObject::operator=(const FBOMObject& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_object_type = other.m_object_type;
    m_children = other.m_children;
    properties = other.properties;
    m_deserialized_object = other.m_deserialized_object;
    m_external_info = other.m_external_info;
    m_unique_id = other.m_unique_id;

    return *this;
}

FBOMObject::FBOMObject(FBOMObject&& other) noexcept
    : m_object_type(std::move(other.m_object_type)),
      m_children(std::move(other.m_children)),
      properties(std::move(other.properties)),
      m_deserialized_object(std::move(other.m_deserialized_object)),
      m_external_info(std::move(other.m_external_info)),
      m_unique_id(std::move(other.m_unique_id))
{
}

FBOMObject& FBOMObject::operator=(FBOMObject&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    m_object_type = std::move(other.m_object_type);
    m_children = std::move(other.m_children);
    properties = std::move(other.properties);
    m_deserialized_object = std::move(other.m_deserialized_object);
    m_external_info = std::move(other.m_external_info);
    m_unique_id = std::move(other.m_unique_id);

    return *this;
}

FBOMObject::~FBOMObject() = default;

bool FBOMObject::HasProperty(ANSIStringView key) const
{
    const auto it = properties.FindAs(key);

    if (it == properties.End())
    {
        return false;
    }

    return true;
}

const FBOMData& FBOMObject::GetProperty(ANSIStringView key) const
{
    static const FBOMData invalid_property_data {};

    auto it = properties.FindAs(key);

    if (it == properties.End())
    {
        return invalid_property_data;
    }

    return it->second;
}

FBOMObject& FBOMObject::SetProperty(ANSIStringView key, const FBOMData& data)
{
    properties.Set(key, data);

    return *this;
}

FBOMObject& FBOMObject::SetProperty(ANSIStringView key, FBOMData&& data)
{
    // sanity check
    // ANSIString str = key.LookupString();
    // AssertThrowMsg(key.hash_code == str.GetHashCode().Value(),
    //     "Expected hash for %s (len: %u) (%llu) to equal hash of %s (len: %u) (%llu)",
    //     key.LookupString(),
    //     std::strlen(key.LookupString()),
    //     key.hash_code,
    //     str.Data(),
    //     str.Size(),
    //     str.GetHashCode().Value());
    // AssertThrow(key.hash_code == CreateNameFromDynamicString(str).hash_code);

    properties.Set(key, std::move(data));

    return *this;
}

FBOMObject& FBOMObject::SetProperty(ANSIStringView key, const FBOMType& type, SizeType size, const void* bytes)
{
    FBOMData data(type);
    data.SetBytes(size, bytes);

    if (!type.IsUnbounded())
    {
        AssertThrowMsg(data.TotalSize() == type.size, "Expected byte count to match type size");
    }

    return SetProperty(key, std::move(data));
}

const FBOMData& FBOMObject::operator[](ANSIStringView key) const
{
    return GetProperty(key);
}

void FBOMObject::AddChild(FBOMObject&& object)
{
    m_children.PushBack(std::move(object));
}

FBOMResult FBOMObject::Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes) const
{
    if (IsExternal())
    {
        if (GetExternalObjectInfo()->IsLinked())
        {
            // make sure the EXT_REF_PLACEHOLDER bit is not set if the external data properties is already set.
            // we don't want to end up having it set to empty.
            attributes &= ~FBOMDataAttributes::EXT_REF_PLACEHOLDER;
        }
        else
        {
            // Set the EXT_REF_PLACEHOLDER flag so when we iterate properties and children, we can update them
            attributes |= FBOMDataAttributes::EXT_REF_PLACEHOLDER;
        }
    }

    return writer->Write(out, *this, id, attributes);
}

FBOMResult FBOMObject::Deserialize(FBOMLoadContext& context, TypeID type_id, const FBOMObject& in, HypData& out)
{
    FBOMMarshalerBase* marshal = GetMarshal(type_id);

    if (!marshal)
    {
        return {
            FBOMResult::FBOM_ERR,
            "No registered marshal class for type"
        };
    }

    return marshal->Deserialize(context, in, out);
}

HashCode FBOMObject::GetHashCode() const
{
    HashCode hc;

    hc.Add(m_object_type.GetHashCode());

    for (const auto& it : properties)
    {
        hc.Add(it.first.GetHashCode());
        hc.Add(it.second.GetHashCode());
    }

    hc.Add(m_children.Size());

    for (const FBOMObject& child : m_children)
    {
        hc.Add(child.GetHashCode());
    }

    return hc;
}

String FBOMObject::ToString(bool deep) const
{
    std::stringstream ss;

    ss << m_object_type.ToString();
    ss << " { properties: { ";
    for (auto& prop : properties)
    {
        ss << *prop.first;
        ss << ": ";
        if (deep)
        {
            ss << prop.second.ToString(deep);
        }
        else
        {
            ss << "...";
        }
        ss << ", ";
    }

    ss << " }, children: [ ";

    if (deep)
    {
        for (const FBOMObject& child : m_children)
        {
            ss << child.ToString(deep);
        }
    }
    else
    {
        ss << m_children.Size();
    }

    ss << " ] ";

    ss << " } ";

    return String(ss.str().data());
}

FBOMMarshalerBase* FBOMObject::GetMarshal(TypeID type_id)
{
    return FBOM::GetInstance().GetMarshal(type_id);
}

} // namespace hyperion::serialization