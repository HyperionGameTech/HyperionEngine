/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LOADER_HPP
#define HYPERION_LOADER_HPP

#include <core/io/ByteReader.hpp>
#include <core/io/BufferedByteReader.hpp>

#include <core/utilities/Result.hpp>

#define HYP_LOADER_BUFFER_SIZE 2048

namespace hyperion {

class Engine;
class AssetManager;

struct LoaderState
{
    using Stream = BufferedReader;

    AssetManager    *asset_manager;
    String          filepath;
    Stream          stream;
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
          m_error_code(UNKNOWN)
    {
    }

    template <auto MessageString>
    AssetLoadError(const StaticMessage &current_function, ValueWrapper<MessageString>, ErrorCode error_code)
        : Error(current_function, ValueWrapper<MessageString>()),
          m_error_code(error_code)
    {
    }

    template <auto MessageString, class... Args>
    AssetLoadError(const StaticMessage &current_function, ValueWrapper<MessageString>, Args &&... args)
        : Error(current_function, ValueWrapper<MessageString>(), std::forward<Args>(args)...),
          m_error_code(UNKNOWN)
    {
    }

    virtual ~AssetLoadError() override = default;

    HYP_FORCE_INLINE ErrorCode GetErrorCode() const
        { return m_error_code; }

private:
    ErrorCode   m_error_code;
};

} // namespace hyperion

#endif
