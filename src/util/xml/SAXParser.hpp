#ifndef SAXParser_H
#define SAXParser_H

#include <asset/BufferedByteReader.hpp>
#include <util/fs/FsUtil.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace hyperion {
namespace xml {

using AttributeMap = std::map<std::string, std::string>;

class SAXHandler
{
public:
    SAXHandler() {}
    virtual ~SAXHandler() {}

    virtual void Begin(const std::string &name, const AttributeMap &attributes) = 0;
    virtual void End(const std::string &name) = 0;
    virtual void Characters(const std::string &value) = 0;
    virtual void Comment(const std::string &comment) = 0;
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
