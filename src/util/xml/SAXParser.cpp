/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/xml/SAXParser.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/Util.hpp>

namespace hyperion {
namespace xml {

SAXParser::SAXParser(SAXHandler* handler)
    : m_handler(handler)
{
}

SAXParser::Result SAXParser::Parse(const FilePath& filepath)
{
    FileBufferedReaderSource source { filepath };
    BufferedReader reader { &source };

    return Parse(&reader);
}

SAXParser::Result SAXParser::Parse(BufferedReader* reader)
{
    if (reader == nullptr)
    {
        return Result(Result::SRT_ERR, "Reader was null");
    }

    if (!reader->IsOpen())
    {
        return Result(Result::SRT_ERR, "File could not be read.");
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

    utf::u32char lastChars[4] { utf::u32char(-1) };

    String elementStr, commentStr, valueStr;
    Array<Pair<String, String>> attribs;

    const auto checkLastChars = [&lastChars]<class T>(std::initializer_list<T> compareChars) -> bool
    {
        const SizeType minSize = compareChars.size() < ArraySize(lastChars) ? compareChars.size() : ArraySize(lastChars);

        for (SizeType i = 0; i < minSize; i++)
        {
            if (lastChars[i] != utf::u32char(*(compareChars.begin() + i)))
            {
                return false;
            }
        }

        return true;
    };

    reader->ReadChars([&](const char ch)
        {
            HYP_DEFER({
                // shift over last chars
                for (int i = int(ArraySize(lastChars)) - 1; i > 0; i--)
                {
                    lastChars[i] = lastChars[i - 1];
                }

                lastChars[0] = ch;
            });

            if (inComment)
            {
                if (lastChars[2] != -1)
                {
                    if (lastChars[3] != -1)
                    {
                        commentStr.Append(lastChars[3]);
                    }

                    commentStr.Append(lastChars[2]);
                }

                if (ch == '>' && checkLastChars({ '-', '-' }))
                {
                    inComment = false;
                    inElement = false;
                    m_handler->Comment(commentStr);
                }

                return;
            }

            if (ch == '\t' || ch == '\n')
            {
                return;
            }

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

                return;
            }

            if (ch == '-')
            {
                if (checkLastChars({ '-', '!', '<' }))
                {
                    inComment = true;
                    commentStr = "";
                }

                return;
            }

            if (ch == '?' && inElement)
            {
                inHeader = true;

                return;
            }

            if (ch == '/' && (inElement || (inAttributes && !inAttributeValue)))
            {
                isOpening = false;
                isClosing = true;

                return;
            }

            if (ch == '>')
            {

                inCharacters = true;

                if (!inHeader)
                {
                    if (isOpening || lastChars[0] == '/')
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

                inHeader = false;

                return;
            }

            if (inHeader)
            {
                return;
            }

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
                    else if (ch == '\"' && lastChars[0] != '\\')
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

                return;
            }

            if (inCharacters)
            {
                if (ch != ' ' || (lastChars[0] != ' ' && (lastChars[0] != '\n' && lastChars[0] != '<')))
                {
                    valueStr += ch;
                }
            }
        });

    return { Result::SRT_OK };
}

} // namespace xml
} // namespace hyperion
