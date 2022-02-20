#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include "loadable.h"

#include <string>
#include <memory>

namespace hyperion {
class AssetLoader {
public:
    struct Result {
        enum {
            ASSET_OK = 0,
            ASSET_ERR = 1
        } result;

        std::shared_ptr<Loadable> loadable;

        std::string message;

        explicit Result(const std::shared_ptr<Loadable> &loadable)
            : result(ASSET_OK),
              loadable(loadable),
              message("")
        {
        }

        explicit Result(decltype(result) result, const std::shared_ptr<Loadable> &loadable, std::string message = "")
            : result(result),
              loadable(loadable),
              message(message)
        {
        }

        explicit Result(decltype(result) result, std::string message = "")
            : result(result),
              loadable(nullptr),
              message(message)
        {
        }

        Result(const Result &other)
            : result(other.result),
              loadable(other.loadable),
              message(other.message)
        {
        }

        inline Result &operator=(const Result &other)
        {
            result = other.result;
            loadable = other.loadable;
            message = other.message;

            return *this;
        }

        inline operator bool() const { return result == ASSET_OK; }

        inline std::shared_ptr<Loadable> &Value() { return loadable; }
        inline const std::shared_ptr<Loadable> &Value() const { return loadable; }
    };

    AssetLoader() = default;
    AssetLoader(const AssetLoader &other) = delete;
    AssetLoader &operator=(const AssetLoader &other) = delete;
    virtual ~AssetLoader() = default;

    virtual Result LoadFromFile(const std::string &) = 0;

    template <typename T>
    std::shared_ptr<T> LoadFromFile(const std::string &path)
    {
        return std::dynamic_pointer_cast<T>(LoadFromFile(path));
    }
};
}

#endif
