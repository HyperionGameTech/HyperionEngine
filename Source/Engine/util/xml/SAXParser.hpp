/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Map.hpp>
#include <Core/containers/String.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/filesystem/FilePath.hpp>

namespace Hyperion {

class ByteReader;

namespace xml {

using AttributeMap = TMap<String, String>;

class SAXHandler
{
public:
    SAXHandler()
    {
    }

    virtual ~SAXHandler()
    {
    }

    virtual void Begin(const String& name, const AttributeMap& attributes) = 0;
    virtual void End(const String& name) = 0;
    virtual void Characters(const String& value) = 0;
    virtual void Comment(const String& comment) = 0;
};

class SAXParser
{
public:
    SAXParser(SAXHandler* handler);
    Result Parse(const FilePath& filepath);
    Result Parse(ByteReader& stream);

private:
    SAXHandler* m_handler;
};

} // namespace xml
} // namespace Hyperion
