/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SAX_PARSER_HPP
#define HYPERION_SAX_PARSER_HPP

#include <asset/BufferedByteReader.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/Pair.hpp>
#include <util/fs/FsUtil.hpp>

#include <fstream>
#include <iostream>

namespace hyperion {
namespace xml {

using AttributeMap = HashMap<String, String>;

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
        enum SaxParserResult
        {
            SRT_OK  = 0,
            SRT_ERR = 1
        } result;

        String message;

        Result(decltype(result) result = SRT_OK, const String &message = String::empty)
            : result(result),
              message(message)
        {
        }

        Result(const Result &other)             = default;
        Result &operator=(const Result &other)  = default;

        [[nodiscard]]
        HYP_FORCE_INLINE
        operator bool() const
            { return result == SRT_OK; }
    };

    SAXParser(SAXHandler *handler);
    Result Parse(const FilePath &filepath);
    Result Parse(BufferedReader *reader);

private:
    SAXHandler *handler;
};

} // namespace xml
} // namespace hyperion

#endif
