/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class AssetManager;
class AssetRegistry;

enum class AssetLoadHint : uint32
{
    NoHint = 0,

    TextureLoader_LoadAsSRGB
};

struct LoaderState
{
    using Stream = FileByteReader;
    
    Stream stream;
    AssetManager* assetManager = nullptr;
    FilePath filepath;
    String batchIdentifier;
    AssetLoadHint hint = AssetLoadHint::NoHint;
};

class AssetLoadError final : public Error
{
public:
    enum ErrorCode : int
    {
        UNKNOWN = -1,
        ERR_NOT_FOUND = 1,
        ERR_NO_LOADER,
        ERR_EOF
    };

    AssetLoadError()
        : Error(),
          m_errorCode(UNKNOWN)
    {
    }

    template <auto CurrentFunctionString, auto MessageString>
    AssetLoadError(ValueWrapper<CurrentFunctionString>, ValueWrapper<MessageString>, ErrorCode errorCode)
        : Error(ValueWrapper<CurrentFunctionString>(), ValueWrapper<MessageString>()),
          m_errorCode(errorCode)
    {
    }

    template <auto CurrentFunctionString, auto MessageString, class... Args>
    AssetLoadError(ValueWrapper<CurrentFunctionString>, ValueWrapper<MessageString>, Args&&... args)
        : Error(ValueWrapper<CurrentFunctionString>(), ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_errorCode(UNKNOWN)
    {
    }

    ~AssetLoadError() = default;

    HYP_FORCE_INLINE ErrorCode GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    ErrorCode m_errorCode;
};

} // namespace Hyperion
