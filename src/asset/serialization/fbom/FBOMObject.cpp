/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>

#include <core/HypClassRegistry.hpp>

namespace hyperion::fbom {

FBOMObject::FBOMObject()
    : m_object_type(FBOMBaseObjectType()),
      nodes(new FBOMNodeHolder())
{
}

FBOMObject::FBOMObject(const FBOMType &loader_type)
    : m_object_type(loader_type),
      nodes(new FBOMNodeHolder())
{
    AssertThrowMsg(loader_type.IsOrExtends(FBOMBaseObjectType()), "Expected type to be an object type, got %s", loader_type.ToString().Data());
}

FBOMObject::FBOMObject(const FBOMObject &other)
    : m_object_type(other.m_object_type),
      nodes(nullptr),
      properties(other.properties),
      m_deserialized_object(other.m_deserialized_object),
      m_external_info(other.m_external_info),
      m_unique_id(other.m_unique_id)
{
    if (other.nodes) {
        nodes = new FBOMNodeHolder(*other.nodes);
    } else {
        nodes = new FBOMNodeHolder();
    }
}

FBOMObject &FBOMObject::operator=(const FBOMObject &other)
{
    if (nodes) {
        if (other.nodes) {
            *nodes = *other.nodes;
        } else {
            nodes->Clear();
        }
    } else {
        nodes = new FBOMNodeHolder(*other.nodes);
    }

    m_object_type = other.m_object_type;
    properties = other.properties;
    m_deserialized_object = other.m_deserialized_object;
    m_external_info = other.m_external_info;
    m_unique_id = other.m_unique_id;

    return *this;
}

FBOMObject::FBOMObject(FBOMObject &&other) noexcept
    : m_object_type(std::move(other.m_object_type)),
      nodes(other.nodes),
      properties(std::move(other.properties)),
      m_deserialized_object(std::move(other.m_deserialized_object)),
      m_external_info(std::move(other.m_external_info)),
      m_unique_id(std::move(other.m_unique_id))
{
    other.nodes = nullptr;
}

FBOMObject &FBOMObject::operator=(FBOMObject &&other) noexcept
{
    if (nodes) {
        nodes->Clear();
    }

    std::swap(nodes, other.nodes);

    m_object_type = std::move(other.m_object_type);
    properties = std::move(other.properties);
    m_deserialized_object = std::move(other.m_deserialized_object);
    m_external_info = std::move(other.m_external_info);
    m_unique_id = std::move(other.m_unique_id);

    return *this;
}

FBOMObject::~FBOMObject()
{
    if (nodes) {
        delete nodes;
    }
}

bool FBOMObject::HasProperty(WeakName key) const
{
    auto it = properties.FindAs(key);

    if (it == properties.End()) {
        return false;
    }

    return true;
}

const FBOMData &FBOMObject::GetProperty(WeakName key) const
{
    auto it = properties.FindAs(key);

    if (it == properties.End()) {
        return FBOMData::UNSET;
    }

    return it->second;
}

FBOMObject &FBOMObject::SetProperty(Name key, const FBOMData &data)
{
    properties.Set(key, data);

    return *this;
}

FBOMObject &FBOMObject::SetProperty(Name key, FBOMData &&data)
{
    properties.Set(key, std::move(data));

    return *this;
}

FBOMObject &FBOMObject::SetProperty(Name key, const ByteBuffer &bytes)
{
    return SetProperty(key, FBOMData(FBOMByteBuffer(bytes.Size()), ByteBuffer(bytes)));
}

FBOMObject &FBOMObject::SetProperty(Name key, const FBOMType &type, ByteBuffer &&byte_buffer)
{
    FBOMData data(type);
    data.SetBytes(std::move(byte_buffer));

    return SetProperty(key, std::move(data));
}

FBOMObject &FBOMObject::SetProperty(Name key, const FBOMType &type, const ByteBuffer &byte_buffer)
{
    FBOMData data(type);
    data.SetBytes(byte_buffer);

    return SetProperty(key, std::move(data));
}

FBOMObject &FBOMObject::SetProperty(Name key, const FBOMType &type, SizeType size, const void *bytes)
{
    FBOMData data(type);
    data.SetBytes(size, bytes);

    return SetProperty(key, std::move(data));
}

FBOMObject &FBOMObject::SetProperty(Name key, const FBOMType &type, const void *bytes)
{
    // AssertThrowMsg(type.IsOrExtends(FBOMStruct()), "Type must be a struct to use this overload");

    return SetProperty(key, type, type.size, bytes);
}

const FBOMData &FBOMObject::operator[](WeakName key) const
{
    return GetProperty(key);
}

void FBOMObject::AddChild(FBOMObject &&object, const String &external_object_key)
{
    if (external_object_key.Length() != 0) {
        object.SetExternalObjectInfo(FBOMExternalObjectInfo { external_object_key });
    }

    // // debug sanity check
    // if (object.GetType().IsOrExtends("Node")) {
    //     FlatSet<UniqueID> subobject_ids;

    //     for (FBOMObject &subobject : *nodes) {
    //         auto insert_result = subobject_ids.Insert(subobject.GetUniqueID());
    //         AssertThrow(insert_result.second);
    //     }
    // }

    nodes->PushBack(std::move(object));
}

FBOMResult FBOMObject::Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const
{
    return writer->Write(out, *this, id, attributes);
}

FBOMResult FBOMObject::Deserialize(const TypeAttributes &type_attributes, const FBOMObject &in, FBOMDeserializedObject &out)
{
    FBOMMarshalerBase *marshal = GetMarshal(type_attributes);
    
    if (!marshal) {
        return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
    }

    Any any_value;

    const FBOMResult result = marshal->Deserialize(in, any_value);

    if (result.IsOK()) {
        AssertThrow(any_value.HasValue());
        
        out.m_value = std::move(any_value);
    }

    return result;
}

HashCode FBOMObject::GetHashCode() const
{
    HashCode hc;

    if (IsExternal()) {
        hc.Add(m_external_info.GetHashCode());
    } else {
        hc.Add(m_object_type.GetHashCode());

        for (const auto &it : properties) {
            hc.Add(it.first.GetHashCode());
            hc.Add(it.second.GetHashCode());
        }

        for (const FBOMObject &subobject : *nodes) {
            hc.Add(subobject.GetHashCode());
        }
    }

    return hc;
}

String FBOMObject::ToString(bool deep) const
{
    std::stringstream ss;

    ss << m_object_type.ToString();
    ss << " { properties: { ";
    for (auto &prop : properties) {
        ss << *prop.first;
        ss << ": ";
        if (deep) {
            ss << prop.second.ToString(deep);
        } else {
            ss << "...";
        }
        ss << ", ";
    }

    ss << " }, nodes: [ ";

    if (deep) {
        for (auto &subobject : *nodes) {
            ss << subobject.ToString(deep);
        }
    } else {
        ss << nodes->Size();
    }

    ss << " ] ";

    ss << " } ";

    return String(ss.str().data());
}

const HypClass *FBOMObject::GetHypClass() const
{
    if (const FBOMMarshalerBase *marshal = FBOM::GetInstance().GetMarshal(GetType().name)) {
        return GetClass(marshal->GetTypeID());
    }

    return nullptr;
}

FBOMMarshalerBase *FBOMObject::GetMarshal(const TypeAttributes &type_attributes)
{
    return FBOM::GetInstance().GetMarshal(type_attributes);
}

} // namespace hyperion::fbom