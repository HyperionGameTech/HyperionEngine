/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/TypeMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/util/UniqueId.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion {

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

enum class FBOMVersionCompareMode : uint32
{
    MAJOR = 0x1,
    MINOR = 0x2,
    PATCH = 0x4,

    DEFAULT = uint32(MAJOR) | uint32(MINOR)
};

HYP_MAKE_ENUM_FLAGS(FBOMVersionCompareMode)

namespace serialization {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;
class FBOMArray;
class FBOMData;
class FBOMMarshalerBase;

class ObjectMarshal;

struct FBOMVersion
{
    uint32 value;

    constexpr FBOMVersion()
        : value(0)
    {
    }

    constexpr FBOMVersion(uint32 value)
        : value(value)
    {
    }

    constexpr FBOMVersion(uint8 major, uint8 minor, uint8 patch)
        : value((uint32(major) << 16) | (uint32(minor) << 8) | (uint32(patch)))

    {
    }

    HYP_FORCE_INLINE uint32 GetMajor() const
    {
        return (value & (0xffu << 16)) >> 16;
    }

    HYP_FORCE_INLINE uint32 GetMinor() const
    {
        return (value & (0xffu << 8)) >> 8;
    }

    HYP_FORCE_INLINE uint32 GetPatch() const
    {
        return value & 0xffu;
    }

    /*! \brief Returns an integer indicating whether the two version are compatible or not.
     *  If the returned value is equal to zero, the two versions are compatible.
     *  If the returned value is less than zero, \ref{lhs} is incompatible, due to being outdated.
     *  If the returned value is greater than zero, \ref{lhs} is incompatible, due to being newer. */
    HYP_FORCE_INLINE static int TestCompatibility(const FBOMVersion& lhs, const FBOMVersion& rhs, EnumFlags<FBOMVersionCompareMode> compareMode = FBOMVersionCompareMode::DEFAULT)
    {
        if (compareMode & FBOMVersionCompareMode::MAJOR)
        {
            if (lhs.GetMajor() < rhs.GetMajor())
            {
                return -1;
            }
            else if (lhs.GetMajor() > rhs.GetMajor())
            {
                return 1;
            }
        }

        if (compareMode & FBOMVersionCompareMode::MINOR)
        {
            if (lhs.GetMinor() < rhs.GetMinor())
            {
                return -1;
            }
            else if (lhs.GetMinor() > rhs.GetMinor())
            {
                return 1;
            }
        }

        if (compareMode & FBOMVersionCompareMode::PATCH)
        {
            if (lhs.GetPatch() < rhs.GetPatch())
            {
                return -1;
            }
            else if (lhs.GetPatch() > rhs.GetPatch())
            {
                return 1;
            }
        }

        return 0;
    }
};

class HYP_API FBOM
{
public:
    static constexpr SizeType headerSize = 32;
    static constexpr char headerIdentifier[] = { 'H', 'Y', 'P', '\0' };
    static constexpr FBOMVersion version = FBOMVersion { 1, 9, 0 };

    static FBOM& GetInstance();

    FBOM();
    FBOM(const FBOM& other) = delete;
    FBOM& operator=(const FBOM& other) = delete;
    ~FBOM();

    /*! \brief Register a custom marshal class to be used for serializng and deserializing
     *  an object, based on its type Id. */
    void RegisterLoader(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal);

    /*! \brief Get the marshal to use for the given object type. If a custom marshal has been registered for \ref{T}'s type Id,
     *  that marshal will be used. Otherwise, the default marshal for the type will be used:
     *      For classes that have a Class associated, the ObjectMarshal will be used.
     *      Otherwise, no marshal will be used, and this function will return nullptr.
     *  \tparam T The type of the class to get the marshal for
     *  \param allowFallback If true (default), allows catch all marshal to be used for Class types
     *  \return A pointer to the marshal instance, or nullptr if no marshal will be used for the given type
     */
    template <class T>
    HYP_FORCE_INLINE FBOMMarshalerBase* GetMarshal(bool allowFallback = true) const
    {
        return GetMarshal(TypeId::ForType<T>(), allowFallback);
    }

    /*! \brief Get the marshal to use for the given object type. If a custom marshal has been registered for the type Id,
     *  that marshal will be used. Otherwise, the default marshal for the type will be used:
     *      For classes that have a Class associated, the ObjectMarshal will be used.
     *      Otherwise, no marshal will be used, and this function will return nullptr.
     *  \param typeId The type Id of the class to get the marshal for
     *  \param allowFallback If true (default), allows catch all marshal to be used for Class types
     *  \return A pointer to the marshal instance, or nullptr if no marshal will be used for the given type
     */
    FBOMMarshalerBase* GetMarshal(TypeId typeId, bool allowFallback = true) const;

    /*! \brief Get the marshal to use for the given object type. If a custom marshal has been registered for the type name,
     *  that marshal will be used. Otherwise, the default marshal for the type will be used:
     *      For classes that have a Class associated, the ObjectMarshal will be used.
     *      Otherwise, no marshal will be used, and this function will return nullptr.
     *  \param typeName The name of the class to get the marshal for
     *  \param allowFallback If true (default), allows catch all marshal to be used for Class types
     *  \return A pointer to the marshal instance, or nullptr if no marshal will be used for the given type (or if the type is a POD type)
     */
    FBOMMarshalerBase* GetMarshal(ANSIStringView typeName, bool allowFallback = true) const;

private:
    TypeMap<Pair<ANSIString, UniquePtr<FBOMMarshalerBase>>> m_marshals;
    UniquePtr<ObjectMarshal> m_objectMarshal;
};

} // namespace serialization

using namespace serialization;

} // namespace hyperion
