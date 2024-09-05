/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_PROPERTY_SERIALIZER_HPP
#define HYPERION_CORE_HYP_PROPERTY_SERIALIZER_HPP

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Handle.hpp>
#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/memory/Any.hpp>

#include <core/ID.hpp>

#include <asset/serialization/Serialization.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class IHypPropertySerializer
{
};

template <class T, class EnableIfResult = void>
class HypPropertySerializer
{
    static_assert(resolution_failure<T>, "No serializer found for type T");

private:
    HypPropertySerializer() = default;
};

class HYP_API HypPropertySerializerRegistry
{
public:
    static HypPropertySerializerRegistry &GetInstance();

    void RegisterSerializer(TypeID type_id, IHypPropertySerializer *serializer);
    IHypPropertySerializer *GetSerializer(TypeID type_id);

private:
    TypeMap<IHypPropertySerializer *>  m_serializers;
};

namespace detail {

template <class T, class Serializer>
struct HypPropertySerializerRegistration
{
    Serializer serializer { };
    
    HypPropertySerializerRegistration()
    {
        HypPropertySerializerRegistry::GetInstance().RegisterSerializer(
            TypeID::ForType<T>(),
            &serializer
        );
    }
};

} // namespace detail

template <class T>
HypPropertySerializer<T> &GetClassPropertySerializer()
{
    static HypPropertySerializer<T> serializer { };

    return serializer;
}

template <>
class HypPropertySerializer<uint8> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint8 value) const
    {
        return fbom::FBOMData::FromUInt8(value);
    }

    uint8 Deserialize(const fbom::FBOMData &value) const
    {
        uint8 result;

        if (fbom::FBOMResult err = value.ReadUInt8(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<uint16> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint16 value) const
    {
        return fbom::FBOMData::FromUInt16(value);
    }

    uint16 Deserialize(const fbom::FBOMData &value) const
    {
        uint16 result;

        if (fbom::FBOMResult err = value.ReadUInt16(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<uint32> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint32 value) const
    {
        return fbom::FBOMData::FromUInt32(value);
    }

    uint32 Deserialize(const fbom::FBOMData &value) const
    {
        uint32 result;

        if (fbom::FBOMResult err = value.ReadUInt32(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<uint64> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint64 value) const
    {
        return fbom::FBOMData::FromUInt64(value);
    }

    uint64 Deserialize(const fbom::FBOMData &value) const
    {
        uint64 result;

        if (fbom::FBOMResult err = value.ReadUInt64(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<int8> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(int8 value) const
    {
        return fbom::FBOMData::FromInt8(value);
    }

    int8 Deserialize(const fbom::FBOMData &value) const
    {
        int8 result;

        if (fbom::FBOMResult err = value.ReadInt8(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<int16> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(int16 value) const
    {
        return fbom::FBOMData::FromInt16(value);
    }

    int16 Deserialize(const fbom::FBOMData &value) const
    {
        int16 result;

        if (fbom::FBOMResult err = value.ReadInt16(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<int32> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(int32 value) const
    {
        return fbom::FBOMData::FromInt32(value);
    }

    int32 Deserialize(const fbom::FBOMData &value) const
    {
        int32 result;

        if (fbom::FBOMResult err = value.ReadInt32(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<int64> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(int64 value) const
    {
        return fbom::FBOMData::FromInt64(value);
    }

    int64 Deserialize(const fbom::FBOMData &value) const
    {
        int64 result;

        if (fbom::FBOMResult err = value.ReadInt64(&result)) {
            return 0;
        }

        return result;
    }
};

template <>
class HypPropertySerializer<float> : public IHypPropertySerializer
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
class HypPropertySerializer<bool> : public IHypPropertySerializer
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

template <>
class HypPropertySerializer<Vec2i> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec2i &value) const
    {
        return fbom::FBOMData::FromVec2i(value);
    }

    Vec2i Deserialize(const fbom::FBOMData &value) const
    {
        Vec2i result;

        if (fbom::FBOMResult err = value.ReadVec2i(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec3i> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec3i &value) const
    {
        return fbom::FBOMData::FromVec3i(value);
    }

    Vec3i Deserialize(const fbom::FBOMData &value) const
    {
        Vec3i result;

        if (fbom::FBOMResult err = value.ReadVec3i(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec4i> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec4i &value) const
    {
        return fbom::FBOMData::FromVec4i(value);
    }

    Vec4i Deserialize(const fbom::FBOMData &value) const
    {
        Vec4i result;

        if (fbom::FBOMResult err = value.ReadVec4i(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec2u> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec2u &value) const
    {
        return fbom::FBOMData::FromVec2u(value);
    }

    Vec2u Deserialize(const fbom::FBOMData &value) const
    {
        Vec2u result;

        if (fbom::FBOMResult err = value.ReadVec2u(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec3u> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec3u &value) const
    {
        return fbom::FBOMData::FromVec3u(value);
    }

    Vec3u Deserialize(const fbom::FBOMData &value) const
    {
        Vec3u result;

        if (fbom::FBOMResult err = value.ReadVec3u(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec4u> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec4u &value) const
    {
        return fbom::FBOMData::FromVec4u(value);
    }

    Vec4u Deserialize(const fbom::FBOMData &value) const
    {
        Vec4u result;

        if (fbom::FBOMResult err = value.ReadVec4u(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec2f> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec2f &value) const
    {
        return fbom::FBOMData::FromVec2f(value);
    }

    Vec2f Deserialize(const fbom::FBOMData &value) const
    {
        Vec2f result;

        if (fbom::FBOMResult err = value.ReadVec2f(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec3f> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec3f &value) const
    {
        return fbom::FBOMData::FromVec3f(value);
    }

    Vec3f Deserialize(const fbom::FBOMData &value) const
    {
        Vec3f result;

        if (fbom::FBOMResult err = value.ReadVec3f(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Vec4f> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec4f &value) const
    {
        return fbom::FBOMData::FromVec4f(value);
    }

    Vec4f Deserialize(const fbom::FBOMData &value) const
    {
        Vec4f result;

        if (fbom::FBOMResult err = value.ReadVec4f(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Matrix3> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Matrix3 &value) const
    {
        return fbom::FBOMData::FromMat3f(value);
    }

    Matrix3 Deserialize(const fbom::FBOMData &value) const
    {
        Matrix3 result;

        if (fbom::FBOMResult err = value.ReadMat3f(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Matrix4> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Matrix4 &value) const
    {
        return fbom::FBOMData::FromMat4f(value);
    }

    Matrix4 Deserialize(const fbom::FBOMData &value) const
    {
        Matrix4 result;

        if (fbom::FBOMResult err = value.ReadMat4f(&result)) {
            return { };
        }

        return result;
    }
};

template <>
class HypPropertySerializer<Quaternion> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Quaternion &value) const
    {
        return fbom::FBOMData::FromQuat4f(value);
    }

    Quaternion Deserialize(const fbom::FBOMData &value) const
    {
        Quaternion result;

        if (fbom::FBOMResult err = value.ReadQuat4f(&result)) {
            return { };
        }

        return result;
    }
};

template <int StringType>
class HypPropertySerializer<containers::detail::String<StringType>> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const containers::detail::String<StringType> &value) const
    {
        return fbom::FBOMData::FromString(value);
    }

    containers::detail::String<StringType> Deserialize(const fbom::FBOMData &value) const
    {
        containers::detail::String<StringType> result;

        if (fbom::FBOMResult err = value.ReadString(result)) {
            return { };
        }

        return result;
    }
};

template <class T>
class HypPropertySerializer<Handle<T>> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Handle<T> &value) const
    {
        if (!value.IsValid()) {
            return fbom::FBOMData { };
        }

        fbom::FBOMObject object = fbom::FBOMObject::Serialize(*value);
        object.m_deserialized_object.Reset(new HypData(value));

        return fbom::FBOMData::FromObject(object, /* keep_native_object */ true);
    }

    auto Deserialize(const fbom::FBOMData &value) const
    {
        // Check if object is already stored in memory
        if (const RC<HypData> &deserialized_object_ptr = value.GetDeserializedObject()) {
            return deserialized_object_ptr->Get<Handle<T>>();
        } else {
            HypData deserialized_object;

            fbom::FBOMObject object;

            fbom::FBOMResult result = value.ReadObject(object);
            AssertThrowMsg(result.IsOK(), "Failed to read object: %s", *result.message);

            result = fbom::FBOMObject::Deserialize<NormalizedType<T>>(object, deserialized_object);
            AssertThrowMsg(result.IsOK(), "Failed to deserialize object: %s", *result.message);

            return deserialized_object.Get<Handle<T>>();
        }
    }
};

// template <class T>
// class HypPropertySerializer< RC<T> > : public IHypPropertySerializer
// {
// public:
//     fbom::FBOMData Serialize(const RC<T> &value) const
//     {
//         if (!value) {
//             return fbom::FBOMData { };
//         }

//         return HypPropertySerializer<T>().Serialize(*value);
//     }

//     RC<T> Deserialize(const fbom::FBOMData &value) const
//     {
//         if (!value) {
//             return { };
//         }

//         // Check if object is already stored in memory
//         if (Optional<const fbom::FBOMDeserializedObject &> deserialized_object_opt = value.GetDeserializedObject(); deserialized_object_opt.HasValue()) {
//             return deserialized_object_opt->Get<RC<T>>();
//         } else {

//             return RC<T>(std::move(*HypPropertySerializer<T>().Deserialize(value)));
//         }
//     }
// };

template <class T>
class HypPropertySerializer< ID<T> > : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const ID<T> &id) const
    {
        return fbom::FBOMData::FromUInt32(id.value);
    }

    ID<T> Deserialize(const fbom::FBOMData &value) const
    {
        ID<T> id;

        if (fbom::FBOMResult err = value.ReadUInt32(&id.value)) {
            return id;
        }

        return id;
    }
};

// template <class T>
// class HypPropertySerializer<T, std::enable_if_t< std::is_enum_v< T > > > : public HypPropertySerializer< std::underlying_type_t< T > >
// {
// public:
//     fbom::FBOMData Serialize(const T &value) const
//     {
//         return HypPropertySerializer< std::underlying_type_t< T > >::Serialize(static_cast<std::underlying_type_t< T >>(value));
//     }

//     T Deserialize(const fbom::FBOMData &value) const
//     {
//         return static_cast<T>(HypPropertySerializer< std::underlying_type_t< T > >::Deserialize(value));
//     }
// };

template <class T, SizeType Sz>
class HypPropertySerializer<FixedArray<T, Sz>> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const FixedArray<T, Sz> &value) const
    {
        fbom::FBOMArray array;

        for (SizeType index = 0; index < Sz; index++) {
            array.AddElement(HypPropertySerializer<T>().Serialize(value[index]));
        }

        return fbom::FBOMData::FromArray(array);
    }

    FixedArray<T, Sz> Deserialize(const fbom::FBOMData &value) const
    {
        FixedArray<T, Sz> result;

        fbom::FBOMArray array;

        fbom::FBOMResult err = value.ReadArray(array);
        AssertThrowMsg(err.IsOK(), "Failed to read array: %s", *err.message);

        for (SizeType index = 0; index < Sz; index++) {
            result[index] = HypPropertySerializer<T>().Deserialize(array.GetElement(index));
        }

        return result;
    }
};

template <class T>
class HypPropertySerializer<T> : public IHypPropertySerializer
{
public:
    fbom::FBOMData Serialize(const NormalizedType<T> &value) const
    {
        if constexpr (std::is_class_v<T>) {
            fbom::FBOMObject object = fbom::FBOMObject::Serialize(value);
            object.m_deserialized_object.Reset(new HypData(value));

            return fbom::FBOMData::FromObject(std::move(object), /* keep_native_object */ true);
        } else if constexpr (std::is_enum_v<T>) {
            return HypPropertySerializer<std::underlying_type_t<T>>().Serialize(static_cast<std::underlying_type_t<T>>(value));
        } else {
            static_assert(resolution_failure<T>, "No serializer found for type T");
        }
    }

    decltype(auto) Deserialize(const fbom::FBOMData &value) const
    {
        if constexpr (std::is_class_v<T>) {
            // Use marshal class or default HypClass instance marshal

            const RC<HypData> &deserialized_object_ptr = value.GetDeserializedObject();
            AssertThrowMsg(deserialized_object_ptr != nullptr,
                "No deserialized object for data with type: %s",
                value.GetType().name.Data());
            
            return deserialized_object_ptr->Get<NormalizedType<T>>();
        } else if constexpr (std::is_enum_v<T>) {
            return static_cast<T>(HypPropertySerializer<std::underlying_type_t<T>>().Deserialize(value));
        } else {
            static_assert(resolution_failure<T>, "No serializer found for type T");
        }
    }
};

#define HYP_DEFINE_CLASS_PROPERTY_SERIALIZER(T, Serializer) \
    static ::hyperion::detail::HypPropertySerializerRegistration<T, Serializer> T##_ClassPropertySerializerRegistration { }

} // namespace hyperion

#endif