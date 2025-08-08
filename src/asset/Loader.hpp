/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/io/ByteReader.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/utilities/Result.hpp>

#define HYP_LOADER_BUFFER_SIZE 2048

namespace hyperion {

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

    template <auto MessageString>
    AssetLoadError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, ErrorCode errorCode)
        : Error(currentFunction, ValueWrapper<MessageString>()),
          m_errorCode(errorCode)
    {
    }

    template <auto MessageString, class... Args>
    AssetLoadError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, Args&&... args)
        : Error(currentFunction, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_errorCode(UNKNOWN)
    {
    }

    virtual ~AssetLoadError() override = default;

    HYP_FORCE_INLINE ErrorCode GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    ErrorCode m_errorCode;
};

} // namespace hyperion
