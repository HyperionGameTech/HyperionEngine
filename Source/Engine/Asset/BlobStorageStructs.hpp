/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/Name/Name.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

namespace Hyperion {

struct BlobHeader
{
    uint8 magic[4];
    uint32 version : 8;
    uint32 payloadOffset : 24;
    uint64 payloadSize;
};

constexpr uint64 InvalidBufferOffset = uint64(-1);

HYP_STRUCT()
struct BlobDataReference
{
    HYP_STRUCT_BODY(BlobDataReference)

    HYP_FIELD()
    Name key;
    
    HYP_FIELD()
    uint64 size = 0;
    
    HYP_FIELD(Transient)
    void* raw = nullptr;
    
    HYP_FIELD(Transient)
    bool readOnly = false;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(key)
            .Combine(size);
    }
};

} // namespace Hyperion
