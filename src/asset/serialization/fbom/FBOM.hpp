/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_HPP
#define HYPERION_FBOM_HPP

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/TypeAttributes.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <math/MathUtil.hpp>

#include <asset/ByteWriter.hpp>

#include <asset/serialization/fbom/FBOMResult.hpp>

#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion {

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

enum class FBOMVersionCompareMode : uint32
{
    MAJOR   = 0x1,
    MINOR   = 0x2,
    PATCH   = 0x4,
    
    DEFAULT = uint32(MAJOR) | uint32(MINOR)
};

HYP_MAKE_ENUM_FLAGS(FBOMVersionCompareMode)

namespace fbom {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;
class FBOMArray;
class FBOMData;
class FBOMMarshalerBase;

class HypClassInstanceMarshal;

struct FBOMVersion
{
    uint32  value;

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
        { return (value & (0xffu << 16)) >> 16; }
    
    HYP_FORCE_INLINE uint32 GetMinor() const
        { return (value & (0xffu << 8)) >> 8; }
    
    HYP_FORCE_INLINE uint32 GetPatch() const
        { return value & 0xffu; }

    /*! \brief Returns an integer indicating whether the two version are compatible or not.
     *  If the returned value is equal to zero, the two versions are compatible.
     *  If the returned value is less than zero, \ref{lhs} is incompatible, due to being outdated.
     *  If the returned value is less than zero, \ref{lhs} is incompatible, due to being newer. */
    HYP_FORCE_INLINE static int TestCompatibility(const FBOMVersion &lhs, const FBOMVersion &rhs, EnumFlags<FBOMVersionCompareMode> compare_mode = FBOMVersionCompareMode::DEFAULT)
    {
        if (compare_mode & FBOMVersionCompareMode::MAJOR) {
            if (lhs.GetMajor() < rhs.GetMajor()) {
                return -1;
            } else if (lhs.GetMajor() > rhs.GetMajor()) {
                return 1;
            }
        }

        if (compare_mode & FBOMVersionCompareMode::MINOR) {
            if (lhs.GetMinor() < rhs.GetMinor()) {
                return -1;
            } else if (lhs.GetMinor() > rhs.GetMinor()) {
                return 1;
            }
        }

        if (compare_mode & FBOMVersionCompareMode::PATCH) {
            if (lhs.GetPatch() < rhs.GetPatch()) {
                return -1;
            } else if (lhs.GetPatch() > rhs.GetPatch()) {
                return 1;
            }
        }

        return 0;
    }
};

class HYP_API FBOM
{
public:
    static constexpr SizeType header_size = 32;
    static constexpr char header_identifier[] = { 'H', 'Y', 'P', '\0' };
    static constexpr FBOMVersion version = FBOMVersion { 1, 9, 0 };

    static FBOM &GetInstance();

    FBOM();
    FBOM(const FBOM &other)             = delete;
    FBOM &operator=(const FBOM &other)  = delete;
    ~FBOM();
    
    /*! \brief Register a custom marshal class to be used for serializng and deserializing
     *  an object, based on its type ID. */
    void RegisterLoader(TypeID type_id, ANSIStringView name, UniquePtr<FBOMMarshalerBase> &&marshal);

    /*! \brief Get the marshal to use for the given object type. If a custom marshal has been registered for \ref{T}'s type ID,
     *  that marshal will be used. Otherwise, the default marshal for the type will be used:
     *      For classes that have a HypClass associated, the HypClassInstanceMarshal will be used.
     *      Otherwise, no marshal will be used, and this function will return nullptr.
     *  \tparam T The type of the class to get the marshal for
     *  \param allow_fallback If true (default), allows catch all marshal to be used for HypClass types
     *  \return A pointer to the marshal instance, or nullptr if no marshal will be used for the given type
     */
    template <class T>
    HYP_FORCE_INLINE FBOMMarshalerBase *GetMarshal(bool allow_fallback = true) const
        { return GetMarshal(TypeID::ForType<T>(), allow_fallback); }

    /*! \brief Get the marshal to use for the given object type. If a custom marshal has been registered for the type ID,
     *  that marshal will be used. Otherwise, the default marshal for the type will be used:
     *      For classes that have a HypClass associated, the HypClassInstanceMarshal will be used.
     *      Otherwise, no marshal will be used, and this function will return nullptr.
     *  \param type_id The type ID of the class to get the marshal for
     *  \param allow_fallback If true (default), allows catch all marshal to be used for HypClass types
     *  \return A pointer to the marshal instance, or nullptr if no marshal will be used for the given type
     */
    FBOMMarshalerBase *GetMarshal(TypeID type_id, bool allow_fallback = true) const;

    /*! \brief Get the marshal to use for the given object type. If a custom marshal has been registered for the type name,
     *  that marshal will be used. Otherwise, the default marshal for the type will be used:
     *      For classes that have a HypClass associated, the HypClassInstanceMarshal will be used.
     *      Otherwise, no marshal will be used, and this function will return nullptr.
     *  \param type_name The name of the class to get the marshal for
     *  \param allow_fallback If true (default), allows catch all marshal to be used for HypClass types
     *  \return A pointer to the marshal instance, or nullptr if no marshal will be used for the given type (or if the type is a POD type)
     */
    FBOMMarshalerBase *GetMarshal(ANSIStringView type_name, bool allow_fallback = true) const;

private:
    TypeMap<Pair<ANSIString, UniquePtr<FBOMMarshalerBase>>> m_marshals;
    UniquePtr<HypClassInstanceMarshal>                      m_hyp_class_instance_marshal;
};

} // namespace fbom
} // namespace hyperion

#endif
