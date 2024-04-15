/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMObject.hpp>

namespace hyperion::v2::fbom {

FBOMObject::FBOMObject()
    : m_object_type(FBOMUnset()),
      nodes(new FBOMNodeHolder())
{
}

FBOMObject::FBOMObject(const FBOMType &loader_type)
    : m_object_type(loader_type),
      nodes(new FBOMNodeHolder())
{
}

FBOMObject::FBOMObject(const FBOMObject &other)
    : m_object_type(other.m_object_type),
      nodes(nullptr),
      properties(other.properties),
      deserialized(other.deserialized),
      m_external_info(other.m_external_info),
      m_unique_id(other.m_unique_id)
{
    if (other.nodes) {
        nodes = new FBOMNodeHolder(*other.nodes);
    } else {
        nodes = new FBOMNodeHolder();
    }
}

auto FBOMObject::operator=(const FBOMObject &other) -> FBOMObject&
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
    deserialized = other.deserialized;
    m_external_info = other.m_external_info;
    m_unique_id = other.m_unique_id;

    return *this;
}

FBOMObject::FBOMObject(FBOMObject &&other) noexcept
    : m_object_type(std::move(other.m_object_type)),
      nodes(other.nodes),
      properties(std::move(other.properties)),
      deserialized(std::move(other.deserialized)),
      m_external_info(std::move(other.m_external_info)),
      m_unique_id(std::move(other.m_unique_id))
{
    other.nodes = nullptr;
}

auto FBOMObject::operator=(FBOMObject &&other) noexcept -> FBOMObject&
{
    if (nodes) {
        nodes->Clear();
    }

    std::swap(nodes, other.nodes);

    m_object_type = std::move(other.m_object_type);
    properties = std::move(other.properties);
    deserialized = std::move(other.deserialized);
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

bool FBOMObject::HasProperty(const String &key) const
{
    auto it = properties.Find(key);

    if (it == properties.End()) {
        return false;
    }

    return true;
}

const FBOMData &FBOMObject::GetProperty(const String &key) const
{
    auto it = properties.Find(key);

    if (it == properties.End()) {
        return FBOMData::UNSET;
    }

    return it->second;
}

void FBOMObject::SetProperty(const String &key, const FBOMData &data)
{
    properties.Set(key, data);
}

void FBOMObject::SetProperty(const String &key, FBOMData &&data)
{
    properties.Set(key, std::move(data));
}

void FBOMObject::SetProperty(const String &key, const FBOMType &type, SizeType size, const void *bytes)
{
    FBOMData data(type);
    data.SetBytes(size, bytes);

    SetProperty(key, data);
}

void FBOMObject::SetProperty(const String &key, const FBOMType &type, const void *bytes)
{
    AssertThrowMsg(!type.IsUnbouned(), "Cannot determine size of an unbounded type, please manually specify size");

    SetProperty(key, type, type.size, bytes);
}

void FBOMObject::SetProperty(const String &key, const ByteBuffer &bytes)
{
    SetProperty(key, FBOMByteBuffer(bytes.Size()), bytes.Data());
}

void FBOMObject::AddChild(FBOMObject &&object, const String &external_object_key)
{
    object.SetExternalObjectInfo(FBOMExternalObjectInfo { external_object_key });

    nodes->PushBack(std::move(object));

    if (object.GetType().IsOrExtends("Node")) {

        FlatSet<uint64> sub_node_ids;
        for (auto &node : *nodes) {
            // debug sanity check
            auto insert_result = sub_node_ids.Insert(node.GetUniqueID());
            AssertThrow(insert_result.second);
        }
    }
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

        for (const auto &it : *nodes) {
            hc.Add(it.GetHashCode());
        }
    }

    return hc;
}

String FBOMObject::ToString() const
{
    std::stringstream ss;

    ss << m_object_type.ToString();
    ss << " { properties: { ";
    for (auto &prop : properties) {
        ss << prop.first;// << ": ";
        //ss << prop.second.ToString();
        ss << ", ";
    }

    ss << " }, nodes: [ ";

    ss << nodes->Size();

    ss << " ] ";

    ss << " } ";

    return String(ss.str().data());
}

} // namespace hyperion::v2::fbom