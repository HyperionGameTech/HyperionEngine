#ifndef SAXParser_H
#define SAXParser_H

#include <asset/BufferedByteReader.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/String.hpp>
#include <core/lib/Pair.hpp>
#include <util/fs/FsUtil.hpp>

#include <fstream>
#include <iostream>

namespace hyperion {
namespace xml {

using AttributeMap = FlatMap<String, String>;

class SAXHandler
{
public:
    SAXHandler() {}
    virtual ~SAXHandler() {}

    virtual void Begin(const String &name, const AttributeMap &attributes) = 0;
    virtual void End(const String &name) = 0;
    virtual void Characters(const String &value) = 0;
    virtual void Comment(const String &comment) = 0;
};

class SAXParser
{
public:
    struct Result
    {
        enum {
            SAX_OK = 0,
            SAX_ERR = 1
        } result;

        String message;

        Result(decltype(result) result = SAX_OK, const String &message = String::empty)
            : result(result),
              message(message)
        {
        }

        Result(const Result &other) = default;
        Result &operator=(const Result &other) = default;

        operator bool() const { return result == SAX_OK; }
    };

    SAXParser(SAXHandler *handler);
    Result Parse(const FilePath &filepath);
    Result Parse(BufferedReader<2048> *reader);

private:
    SAXHandler *handler;
};

} // namespace xml
} // namespace hyperion

#endif
