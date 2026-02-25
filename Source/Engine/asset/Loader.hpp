/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/io/ByteReader.hpp>
#include <Core/io/BufferedByteReader.hpp>

#include <Core/utilities/Result.hpp>

#define HYP_LOADER_BUFFER_SIZE 2048

namespace Hyperion {

class AssetManager;
class AssetRegistry;

struct LoaderState
{
    using Stream = BufferedReader;

    AssetManager* assetManager;
    FilePath filepath;
    String batchIdentifier;
    Stream stream;
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
