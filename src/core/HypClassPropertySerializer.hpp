/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_PROPERTY_SERIALIZER_HPP
#define HYPERION_CORE_HYP_CLASS_PROPERTY_SERIALIZER_HPP

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Handle.hpp>
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

template <>
class HypClassPropertySerializer<uint8> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(uint8 value) const
    {
        return fbom::FBOMData(value);
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
        return fbom::FBOMData(value);
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
        return fbom::FBOMData(value);
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
        return fbom::FBOMData(value);
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
        return fbom::FBOMData(value);
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
        return fbom::FBOMData(value);
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
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec2i> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec2i &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec3i> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec3i &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec4i> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec4i &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec2u> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec2u &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec3u> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec3u &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec4u> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec4u &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec2f> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec2f &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec3f> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec3f &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Vec4f> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Vec4f &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Matrix3> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Matrix3 &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Matrix4> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Matrix4 &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<Quaternion> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Quaternion &value) const
    {
        return fbom::FBOMData(value);
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
class HypClassPropertySerializer<containers::detail::String<StringType>> : public IHypClassPropertySerializer
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
class HypClassPropertySerializer<T> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const NormalizedType<T> &value) const
    {
        // if constexpr (fbom::FBOMStruct::is_valid_struct_type< NormalizedType<T> >) {
        //     return fbom::FBOMData::FromStruct< NormalizedType<T> >(value);
        // } else {
            fbom::FBOMObject object = fbom::FBOMObject::Serialize(value);

            object.m_deserialized_object.Reset(new fbom::FBOMDeserializedObject());
            object.m_deserialized_object->any_value.Set<T>(value);

            return fbom::FBOMData::FromObject(std::move(object), /* keep_native_object */ true);
        // }
    }

    decltype(auto) Deserialize(const fbom::FBOMData &value) const
    {
        // if constexpr (fbom::FBOMStruct::is_valid_struct_type< NormalizedType<T> >) {
        //     NormalizedType<T> result { };

        //     if (fbom::FBOMResult err = value.ReadStruct(&result)) {
        //         return result;
        //     }

        //     return result;
        // } else {
            Optional<const fbom::FBOMDeserializedObject &> deserialized_object_opt = value.GetDeserializedObject();
            AssertThrow(deserialized_object_opt.HasValue());

            return deserialized_object_opt->Get< NormalizedType<T> >();

#if 0
            // Check if object is already stored in memory
            if (Optional<const fbom::FBOMDeserializedObject &> deserialized_object_opt = value.GetDeserializedObject(); deserialized_object_opt.HasValue()) {
                return deserialized_object_opt->Get< NormalizedType<T> >();
            } else {
                fbom::FBOMResult result;

                fbom::FBOMDeserializedObject deserialized_object;

                fbom::FBOMObject object;
                result = value.ReadObject(object);
                AssertThrowMsg(result.IsOK(), "Failed to read object: %s", *result.message);

                result = fbom::FBOMObject::Deserialize< NormalizedType<T> >(object, deserialized_object);
                AssertThrowMsg(result.IsOK(), "Failed to deserialize object: %s", *result.message);

                return deserialized_object.Get< NormalizedType<T> >();
            }
#endif
        // }
    }
};

template <class T>
class HypClassPropertySerializer< Handle<T> > : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const Handle<T> &value) const
    {
        if (!value.IsValid()) {
            return fbom::FBOMData { };
        }

        fbom::FBOMObject object = fbom::FBOMObject::Serialize(*value);

        object.m_deserialized_object.Reset(new fbom::FBOMDeserializedObject());
        object.m_deserialized_object->Set<T>(value);

        return fbom::FBOMData::FromObject(object, /* keep_native_object */ true);
    }

    auto Deserialize(const fbom::FBOMData &value) const
    {
        // Check if object is already stored in memory
        if (Optional<const fbom::FBOMDeserializedObject &> deserialized_object_opt = value.GetDeserializedObject(); deserialized_object_opt.HasValue()) {
            return deserialized_object_opt->Get<T>();
        } else {
            fbom::FBOMDeserializedObject deserialized_object;

            fbom::FBOMObject object;

            fbom::FBOMResult result = value.ReadObject(object);
            AssertThrowMsg(result.IsOK(), "Failed to read object: %s", *result.message);

            result = fbom::FBOMObject::Deserialize< NormalizedType<T> >(object, deserialized_object);
            AssertThrowMsg(result.IsOK(), "Failed to deserialize object: %s", *result.message);

            return deserialized_object.Get<T>();
        }
    }
};

// template <class T>
// class HypClassPropertySerializer< RC<T> > : public IHypClassPropertySerializer
// {
// public:
//     fbom::FBOMData Serialize(const RC<T> &value) const
//     {
//         if (!value) {
//             return fbom::FBOMData { };
//         }

//         return HypClassPropertySerializer<T>().Serialize(*value);
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

//             return RC<T>(std::move(*HypClassPropertySerializer<T>().Deserialize(value)));
//         }
//     }
// };

template <class T>
class HypClassPropertySerializer< ID<T> > : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const ID<T> &id) const
    {
        return fbom::FBOMData(uint32(id.value));
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

template <class T, SizeType Sz>
class HypClassPropertySerializer<FixedArray<T, Sz>> : public IHypClassPropertySerializer
{
public:
    fbom::FBOMData Serialize(const FixedArray<T, Sz> &value) const
    {
        fbom::FBOMArray array;

        for (SizeType index = 0; index < Sz; index++) {
            array.AddElement(HypClassPropertySerializer<T>().Serialize(value[index]));
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
            result[index] = HypClassPropertySerializer<T>().Deserialize(array.GetElement(index));
        }

        return result;
    }
};

#define HYP_DEFINE_CLASS_PROPERTY_SERIALIZER(T, Serializer) \
    static ::hyperion::detail::HypClassPropertySerializerRegistration<T, Serializer> T##_ClassPropertySerializerRegistration { }

} // namespace hyperion

#endif