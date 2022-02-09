#ifndef FBOM_OBJECT_H
#define FBOM_OBJECT_H

#include "base_types.h"
#include "loadable.h"
#include "data.h"

#include <vector>
#include <string>
#include <map>

namespace hyperion {
namespace fbom {

class FBOMObject {
public:
    // std::string decl_type;
    FBOMType m_object_type;
    std::vector<std::shared_ptr<FBOMObject>> nodes;
    std::map<std::string, std::shared_ptr<FBOMData>> properties;
    FBOMDeserialized deserialized_object = nullptr;

    FBOMObject()
        : m_object_type(FBOMUnset())
    {
    }

    FBOMObject(const FBOMType &loader_type)
        : m_object_type(loader_type)
    {
    }

    FBOMObject(const FBOMObject &other)
        : m_object_type(other.m_object_type),
          nodes(other.nodes),
          properties(other.properties),
          deserialized_object(other.deserialized_object)
    {
    }

    FBOMObject &operator=(const FBOMObject &other)
    {
        m_object_type = other.m_object_type;
        nodes = other.nodes;
        properties = other.properties;
        deserialized_object = other.deserialized_object;

        return *this;
    }

    const FBOMData &GetProperty(const std::string &key)
    {
        auto it = properties.find(key);

        if (it == properties.end() || it->second == nullptr) {
            return FBOMData::UNSET;
        }

        return *it->second;
    }

    inline void SetProperty(const std::string &key, const std::shared_ptr<FBOMData> &data)
    {
        properties[key] = data;
    }

    inline void SetProperty(const std::string &key, const FBOMType &type, size_t size, const void *bytes)
    {
        std::shared_ptr<FBOMData> data(new FBOMData(type));
        data->SetBytes(size, reinterpret_cast<const unsigned char*>(bytes));

        SetProperty(key, data);
    }

    inline void SetProperty(const std::string &key, const FBOMType &type, const void *bytes)
    {
        ex_assert_msg(!type.IsUnbouned(), "Cannot determine size of an unbounded type, please manually specify size");

        SetProperty(key, type, type.size, bytes);
    }

    FBOMObject *AddChild(const FBOMType &loader_type)
    {
        std::shared_ptr<FBOMObject> child_node = std::make_shared<FBOMObject>(loader_type);

        nodes.push_back(child_node);

        return child_node.get();
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_object_type.GetHashCode());

        for (const auto &it : nodes) {
            soft_assert_continue(it != nullptr);

            hc.Add(it->GetHashCode());
        }

        for (const auto &it : properties) {
            soft_assert_continue(it.second != nullptr);

            hc.Add(it.first);
            hc.Add(it.second->GetHashCode());
        }

        return hc;
    }

    inline std::string ToString() const
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

        ss << nodes.size();
    
        ss << " ] ";

        ss << " } ";

        return ss.str();
    }
};

} // namespace fbom
} // namespace hyperion

#endif
