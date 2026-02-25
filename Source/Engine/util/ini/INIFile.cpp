/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <util/ini/INIFile.hpp>

#include <Core/io/BufferedByteReader.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

namespace Hyperion {

const INIFile::Element INIFile::Element::empty = {};

INIFile::INIFile(const FilePath& path)
    : m_path(path),
      m_isValid(false)
{
    Parse();
}

void INIFile::Parse()
{
    m_isValid = false;

    if (!m_path.Exists())
    {
        return;
    }

    FileBufferedReaderSource source { m_path };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        HYP_LOG(Core, Error, "Failed to open INI file: {}", m_path);

        return;
    }

    m_isValid = true;

    Array<Pair<String, Section>> sections;

    auto lines = reader.ReadAllLines();

    for (const auto& line : lines)
    {
        String lineTrimmed = line.TrimmedLeft();

        FixedArray<SizeType, 2> commentIndices {
            lineTrimmed.FindFirstIndex(";"),
            lineTrimmed.FindFirstIndex("#")
        };

        if (commentIndices[1] < commentIndices[0])
        {
            commentIndices[0] = commentIndices[1];
        }

        if (commentIndices[0] != String::NotFound)
        {
            lineTrimmed = lineTrimmed.Substr(0, commentIndices[0]);
        }

        if (lineTrimmed.Empty())
        {
            continue;
        }

        // section block
        if (lineTrimmed.GetChar(0) == '[')
        {
            String sectionName;

            for (SizeType index = 1; index < lineTrimmed.Length() && lineTrimmed.GetChar(index) != ']'; index++)
            {
                sectionName += lineTrimmed.GetChar(index);
            }

            if (sectionName.Empty())
            {
                HYP_LOG(Core, Warning, "Empty section name in INI");
            }

            sections.PushBack(Pair<String, Section> { std::move(sectionName), {} });

            continue;
        }

        // key, value pair in block
        auto split = lineTrimmed.Split('=');

        for (auto& part : split)
        {
            part = part.Trimmed();
        }

        if (split.Size() < 2)
        {
            HYP_LOG(Core, Warning, "Line is not in required format (key = value): {}", lineTrimmed);

            continue;
        }

        if (sections.Empty())
        {
            // no section defined; add a default one
            sections.PushBack(Pair<String, Section> { "default", {} });
        }

        const String& key = split[0];

        // split value by commas
        Value value;

        for (String& item : split[1].Split(','))
        {
            String itemTrimmed = item.Trimmed();

            Element element;

            // read sub-elements
            int parenthesesDepth = 0;
            String subElementName;

            for (SizeType index = 0; index < itemTrimmed.Size(); index++)
            {
                if (std::isspace(itemTrimmed[index]) || itemTrimmed[index] == ',')
                {
                    if (subElementName.Any())
                    {
                        element.subElements.PushBack(std::move(subElementName));
                        subElementName.Clear(); // NOLINT
                    }

                    continue;
                }

                if (itemTrimmed[index] == '(')
                {
                    ++parenthesesDepth;
                }
                else if (itemTrimmed[index] == ')')
                {
                    --parenthesesDepth;

                    if (parenthesesDepth == 0 && subElementName.Any())
                    {
                        element.subElements.PushBack(std::move(subElementName));
                        subElementName.Clear(); // NOLINT
                    }
                }
                else if (parenthesesDepth > 0)
                {
                    subElementName += itemTrimmed[index];
                }
                else if (itemTrimmed[index] == '=')
                {
                    ++index;

                    // Skip spaces
                    while (index < itemTrimmed.Size())
                    {
                        if (std::isspace(itemTrimmed[index]))
                        {
                            ++index;
                        }
                        else
                        {
                            break;
                        }
                    }

                    String valueStr;

                    while (index < itemTrimmed.Size())
                    {
                        valueStr += itemTrimmed[index];

                        ++index;
                    }

                    if (valueStr.Any())
                    {
                        element.value = valueStr;
                    }
                }
                else
                {
                    element.name += itemTrimmed[index];
                }
            }

            value.elements.PushBack(std::move(element));
        }

        auto& lastSection = sections.Back();

        lastSection.second[key] = std::move(value);
    }

    m_sections.Clear();

    for (auto& item : sections)
    {
        m_sections[item.first] = std::move(item.second);
    }
}

} // namespace Hyperion
