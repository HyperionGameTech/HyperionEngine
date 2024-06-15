/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_PROPERTY_SERIALIZER_HPP
#define HYPERION_CORE_HYP_CLASS_PROPERTY_SERIALIZER_HPP

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/functional/Proc.hpp>
#include <core/utilities/TypeID.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/memory/Any.hpp>

#include <asset/serialization/Serialization.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class IHypClassPropertySerializer;

template <class T>
class HypClassPropertySerializer;

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
IHypClassPropertySerializer *GetClassPropertySerializer()
{
    static HypClassPropertySerializer<T> serializer { };

    return &serializer;
}

template <class T>
class HypClassPropertySerializer : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData operator<<(const T &value) const;
    T operator>>(const fbom::FBOMData &value) const;
};

template <>
class HypClassPropertySerializer<uint8> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData operator<<(uint8 value) const
    {
        return fbom::FBOMData::FromByte(value);
    }

    uint8 operator>>(const fbom::FBOMData &value) const
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
    fbom::FBOMData operator<<(uint32 value) const
    {
        return fbom::FBOMData::FromUnsignedInt(value);
    }

    uint32 operator>>(const fbom::FBOMData &value) const
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
    fbom::FBOMData operator<<(uint64 value) const
    {
        return fbom::FBOMData::FromUnsignedLong(value);
    }

    uint64 operator>>(const fbom::FBOMData &value) const
    {
        uint32 result;

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
    fbom::FBOMData operator<<(int32 value) const
    {
        return fbom::FBOMData::FromInt(value);
    }

    int32 operator>>(const fbom::FBOMData &value) const
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
    fbom::FBOMData operator<<(int64 value) const
    {
        return fbom::FBOMData::FromLong(value);
    }

    int64 operator>>(const fbom::FBOMData &value) const
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
    fbom::FBOMData operator<<(float value) const
    {
        return fbom::FBOMData::FromFloat(value);
    }

    float operator>>(const fbom::FBOMData &value) const
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
    fbom::FBOMData operator<<(bool value) const
    {
        return fbom::FBOMData::FromBool(value);
    }

    bool operator>>(const fbom::FBOMData &value) const
    {
        bool result;

        if (fbom::FBOMResult err = value.ReadBool(&result)) {
            return false;
        }

        return result;
    }
};

template <class T, typename = std::enable_if_t< IsPODType< T > > >
class HypClassPropertySerializer : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData operator<<(const T &value) const
    {
        return fbom::FBOMData::FromStruct<T>(value);
    }

    T operator>>(const fbom::FBOMData &value) const
    {
        T result { };

        if (fbom::FBOMResult err = value.ReadStruct(&result)) {
            return result;
        }

        return result;
    }
};

#define HYP_DEFINE_CLASS_PROPERTY_SERIALIZER(T, Serializer) \
    static ::hyperion::detail::HypClassPropertySerializerRegistration<T, Serializer> T##_ClassPropertySerializerRegistration { }

} // namespace hyperion

#endif