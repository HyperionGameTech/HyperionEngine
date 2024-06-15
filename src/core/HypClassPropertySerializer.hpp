/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_PROPERTY_SERIALIZER_HPP
#define HYPERION_CORE_HYP_CLASS_PROPERTY_SERIALIZER_HPP

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/functional/Proc.hpp>
#include <core/utilities/TypeID.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/memory/Any.hpp>
#include <core/ID.hpp>

#include <asset/serialization/Serialization.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class IHypClassPropertySerializer
{

};

template <class T, class EnableIfResult = void>
class HypClassPropertySerializer
{
    static_assert(resolution_failure<T>, "No serializer found for type T");

private:
    HypClassPropertySerializer() = default;
};

class HYP_API HypClassPropertySerializerRegistry
{
public:
    static HypClassPropertySerializerRegistry &GetInstance();

    void RegisterSerializer(TypeID type_id, IHypClassPropertySerializer *serializer);
    IHypClassPropertySerializer *GetSerializer(TypeID type_id);

private:
    TypeMap<IHypClassPropertySerializer *>  m_serializers;
};

namespace detail {

template <class T, class Serializer>
struct HypClassPropertySerializerRegistration
{
    Serializer serializer { };
    
    HypClassPropertySerializerRegistration()
    {
        HypClassPropertySerializerRegistry::GetInstance().RegisterSerializer(
            TypeID::ForType<T>(),
            &serializer
        );
    }
};

} // namespace detail

template <class T>
HypClassPropertySerializer<T> &GetClassPropertySerializer()
{
    static HypClassPropertySerializer<T> serializer { };

    return serializer;
}

// template <class T, class EnableIfResult>
// class HypClassPropertySerializer : public IHypClassPropertySerializer
// {
// public:
//     fbom::FBOMData Serialize(const T &value) const;
//     T Deserialize(const fbom::FBOMData &value) const;
// };

template <>
class HypClassPropertySerializer<uint8> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint8 value) const
    {
        return fbom::FBOMData::FromByte(value);
    }

    uint8 Deserialize(const fbom::FBOMData &value) const
    {
        uint8 result;

        if (fbom::FBOMResult err = value.ReadByte(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypClassPropertySerializer<uint32> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint32 value) const
    {
        return fbom::FBOMData::FromUnsignedInt(value);
    }

    uint32 Deserialize(const fbom::FBOMData &value) const
    {
        uint32 result;

        if (fbom::FBOMResult err = value.ReadUnsignedInt(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypClassPropertySerializer<uint64> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint64 value) const
    {
        return fbom::FBOMData::FromUnsignedLong(value);
    }

    uint64 Deserialize(const fbom::FBOMData &value) const
    {
        uint64 result;

        if (fbom::FBOMResult err = value.ReadUnsignedLong(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypClassPropertySerializer<int32> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(int32 value) const
    {
        return fbom::FBOMData::FromInt(value);
    }

    int32 Deserialize(const fbom::FBOMData &value) const
    {
        int32 result;

        if (fbom::FBOMResult err = value.ReadInt(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypClassPropertySerializer<int64> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(int64 value) const
    {
        return fbom::FBOMData::FromLong(value);
    }

    int64 Deserialize(const fbom::FBOMData &value) const
    {
        int64 result;

        if (fbom::FBOMResult err = value.ReadLong(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypClassPropertySerializer<float> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(float value) const
    {
        return fbom::FBOMData::FromFloat(value);
    }

    float Deserialize(const fbom::FBOMData &value) const
    {
        float result;

        if (fbom::FBOMResult err = value.ReadFloat(&result)) {
            return 0.0f;
        }

        return result;
    }
};

template <>
class HypClassPropertySerializer<bool> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(bool value) const
    {
        return fbom::FBOMData::FromBool(value);
    }

    bool Deserialize(const fbom::FBOMData &value) const
    {
        bool result;

        if (fbom::FBOMResult err = value.ReadBool(&result)) {
            return false;
        }

        return result;
    }
};

template <class T>
class HypClassPropertySerializer<T, std::enable_if_t< fbom::FBOMStruct::is_valid_struct_type< NormalizedType<T> > > > : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const NormalizedType<T> &value) const
    {
        return fbom::FBOMData::FromStruct< NormalizedType<T> >(value);
    }

    NormalizedType<T> Deserialize(const fbom::FBOMData &value) const
    {
        NormalizedType<T> result { };

        if (fbom::FBOMResult err = value.ReadStruct(&result)) {
            return result;
        }

        return result;
    }
};

template <class T>
class HypClassPropertySerializer< ID<T> > : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const ID<T> &id) const
    {
        return fbom::FBOMData::FromUnsignedInt(id.value);
    }

    ID<T> Deserialize(const fbom::FBOMData &value) const
    {
        ID<T> id;

        if (fbom::FBOMResult err = value.ReadUnsignedInt(&id.value)) {
            return id;
        }

        return id;
    }
};

#define HYP_DEFINE_CLASS_PROPERTY_SERIALIZER(T, Serializer) \
    static ::hyperion::detail::HypClassPropertySerializerRegistration<T, Serializer> T##_ClassPropertySerializerRegistration { }

} // namespace hyperion

#endif