/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LOADER_HPP
#define HYPERION_LOADER_HPP

#include <asset/ByteReader.hpp>
#include <asset/BufferedByteReader.hpp>

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

struct LoaderResult
{
    enum class Status
    {
        OK,
        ERR,
        ERR_NOT_FOUND,
        ERR_NO_LOADER,
        ERR_EOF
    };

    Status status = Status::OK;
    String message;

    operator bool() const { return status == Status::OK; }
    bool operator==(Status status) const { return this->status == status; }
    bool operator!=(Status status) const { return this->status != status; }
};

} // namespace hyperion

#endif
