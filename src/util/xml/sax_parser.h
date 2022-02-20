#ifndef SAXPARSER_H
#define SAXPARSER_H

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace hyperion {
namespace xml {

typedef std::map<std::string, std::string> AttributeMap;

class SaxHandler {
public:
    SaxHandler() {}
    virtual ~SaxHandler() {}

    virtual void Begin(const std::string &name, const AttributeMap &attributes) = 0;
    virtual void End(const std::string &name) = 0;
    virtual void Characters(const std::string &value) = 0;
    virtual void Comment(const std::string &comment) = 0;
};

class SaxParser {
public:
    struct Result {
        enum {
            SAX_OK = 0,
            SAX_ERR = 1
        } result;

        std::string message;

        Result(decltype(result) result = SAX_OK, std::string message = "") : result(result), message(message) {}
        Result(const Result &other) : result(other.result), message(other.message) {}
        inline Result &operator=(const Result &other)
        {
            result = other.result;
            message = other.message;

            return *this;
        }

        inline operator bool() const { return result == SAX_OK; }
    };

    SaxParser(SaxHandler *handler);
    Result Parse(const std::string &filepath);

private:
    std::ifstream file;
    SaxHandler *handler;
};

} // namespace xml
} // namespace hyperion

#endif
