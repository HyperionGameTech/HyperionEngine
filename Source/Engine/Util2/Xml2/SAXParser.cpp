/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Util/xml/SAXParser.hpp>

#include <Core/io/ByteReader.hpp>

namespace Hyperion {
namespace xml {

SAXParser::SAXParser(SAXHandler* handler)
    : m_handler(handler)
{
}

Result SAXParser::Parse(const FilePath& filepath)
{
    FileByteReader stream { filepath };

    return Parse(stream);
}

Result SAXParser::Parse(ByteReader& stream)
{
    if (stream.Eof())
    {
        return HYP_MAKE_ERROR(Error, "File could not be read.");
    }

    bool isReading = false,
         isOpening = false,
         isClosing = false,
         inElement = false,
         inComment = false,
         inCharacters = false,
         inHeader = false,
         inAttributes = false,
         inAttributeValue = false,
         inAttributeName = false;

    utf::Char32 lastChar = utf::Char32(-1);
    String elementStr, commentStr, valueStr;
    Array<Pair<String, String>> attribs;

    ByteBuffer bytes = stream.Read();
    const char* chars = reinterpret_cast<const char*>(bytes.Data());

    for (size_t index = 0; index < bytes.Size(); index++)
    {
        char ch = chars[index];

        if (ch != '\t' && ch != '\n')
        {
            if (ch == '<')
            {
                elementStr.Clear();
                inCharacters = false;

                if (!valueStr.Empty())
                {
                    m_handler->Characters(valueStr);
                }

                isOpening = true;
                isReading = true;
                inElement = true;
                inAttributes = false;
                isClosing = false;
                valueStr.Clear();
                attribs.Clear();
            }
            else if (ch == '!' && inElement)
            {
                inComment = true;
                commentStr = "";
            }
            else if (ch == '?' && inElement)
            {
                inHeader = true;
            }
            else if (ch == '/' && (inElement || (inAttributes && !inAttributeValue)))
            {
                isOpening = false;
                isClosing = true;
            }
            else if (ch == '>')
            {
                inCharacters = true;
                if (inComment)
                {
                    inComment = false;
                    m_handler->Comment(commentStr);
                }
                else if (inHeader)
                {
                    inHeader = false;
                }
                else
                {
                    if (isOpening || lastChar == '/')
                    {
                        AttributeMap locals;

                        for (auto& attr : attribs)
                        {
                            if (!attr.first.Empty())
                            {
                                locals[attr.first.ToLower()] = attr.second;
                            }
                        }

                        m_handler->Begin(elementStr, locals);
                        isOpening = false;
                    }

                    if (isClosing)
                    {
                        m_handler->End(elementStr);
                    }

                    inAttributes = false;
                    inElement = false;
                    isClosing = false;
                    isReading = false;

                    attribs.Clear();
                }
            }
            else
            {
                if (!inComment && !inHeader)
                {
                    if (isReading)
                    {
                        if (inElement)
                        {
                            if (ch == ' ')
                            {
                                inElement = false;
                                inAttributes = true;
                                attribs.PushBack({ "", "" });
                            }
                            else
                            {
                                elementStr += ch;
                            }
                        }
                        else if (inAttributes && isOpening)
                        {
                            if (!inAttributeValue && ch == ' ')
                            {
                                attribs.PushBack({ "", "" });
                            }
                            else if (ch == '\"' && lastChar != '\\')
                            {
                                inAttributeValue = !inAttributeValue;
                            }
                            else if (ch != '\\')
                            {
                                auto& last = attribs.Back();
                                if (!inAttributeValue && ch != '=')
                                {
                                    last.first += ch;
                                }
                                else if (inAttributeValue)
                                {
                                    last.second += ch;
                                }
                            }
                        }
                    }
                    else if (inCharacters)
                    {
                        if (ch != ' ' || (lastChar != ' ' && (lastChar != '\n' && lastChar != '<')))
                        {
                            valueStr += ch;
                        }
                    }
                }
                else if (inComment && ch != '-')
                {
                    commentStr += ch;
                }
            }
        }

        lastChar = ch;
    }

    // ok
    return {};
}

} // namespace xml
} // namespace Hyperion
