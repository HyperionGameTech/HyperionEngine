#ifndef HYPERION_V2_LOADER_H
#define HYPERION_V2_LOADER_H

#include <asset/ByteReader.hpp>
#include <asset/BufferedByteReader.hpp>

#include <vector>

#define HYP_LOADER_BUFFER_SIZE 2048

namespace hyperion::v2 {

class Engine;

struct LoaderState {
    std::string                                filepath;
    BufferedReader<HYP_LOADER_BUFFER_SIZE>     stream;
    Engine                                    *engine;
};

struct LoaderResult {
    enum class Status {
        OK,
        ERR,
        ERR_NOT_FOUND,
        ERR_EOF
    };

    Status status = Status::OK;
    std::string message;

    operator bool() const { return status == Status::OK; }
    bool operator==(Status status) const { return this->status == status; }
};

template <class Object, class Handler>
class LoaderImpl {
    using Results = std::vector<std::pair<LoaderResult, Object>>;

public:
    LoaderImpl(const Handler &handler)
        : m_handler(handler) {}

    std::pair<LoaderResult, Object> Load(LoaderState &&state)
    {
        if (!state.stream.IsOpen()) {
            return std::make_pair(
                LoaderResult{LoaderResult::Status::ERR_NOT_FOUND, "Byte stream error -- failed to open file"},
                Object{}
            );
        }

        if (state.stream.Eof()) {
            return std::make_pair(
                LoaderResult{LoaderResult::Status::ERR_EOF, "Byte stream error -- eof"},
                Object{}
            );
        }

        Object object;
        LoaderResult result = m_handler.load_fn(&state, object);

        return std::make_pair(result, std::move(object));
    }

private:
    Handler m_handler;
};

template <class T, LoaderFormat Format>
class LoaderBase {
public:
    using FinalType = T;
    using Object  = LoaderObject<T, Format>;
    
    struct Handler {
        std::function<LoaderResult(LoaderState *, Object &)>     load_fn;
        std::function<std::unique_ptr<FinalType>(Engine *engine, const Object &)> build_fn;
    };

public:

    LoaderBase(Handler &&handler)
        : m_handler(std::move(handler))
    {
    }

    LoaderBase(const LoaderBase &other) = delete;
    LoaderBase &operator=(const LoaderBase &other) = delete;

    LoaderBase(LoaderBase &&other) noexcept
        : m_handler(std::move(other.m_handler))
    {
    }

    LoaderBase &operator=(LoaderBase &&other) noexcept
    {
        m_handler = std::move(other.m_handler);

        return *this;
    }

    ~LoaderBase() = default;

    LoaderImpl<Object, Handler> Instance() const
        { return LoaderImpl<Object, Handler>(m_handler); }

    std::unique_ptr<FinalType> Build(Engine *engine, const Object &object) const
    {
        AssertThrowMsg(m_handler.build_fn, "No object build function set");

        return std::move(m_handler.build_fn(engine, object));
    }

private:
    Handler m_handler;
};

} // namespace hyperion::v2

#endif
