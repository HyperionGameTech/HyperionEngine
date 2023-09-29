#ifndef HYPERION_V2_LOADER_H
#define HYPERION_V2_LOADER_H

#include <asset/ByteReader.hpp>
#include <asset/BufferedByteReader.hpp>

#include "LoaderObject.hpp"

#include <vector>

#define HYP_LOADER_BUFFER_SIZE 2048

namespace hyperion::v2 {

class Engine;
class AssetManager;

struct LoaderState
{
    using Stream = BufferedReader<HYP_LOADER_BUFFER_SIZE>;

    AssetManager *asset_manager;
    String filepath;
    Stream stream;
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

} // namespace hyperion::v2

#endif
