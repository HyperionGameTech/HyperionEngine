#include <util/definitions/DefinitionsFile.hpp>
#include <asset/BufferedByteReader.hpp>
#include <system/Debug.hpp>

namespace hyperion::v2 {

const DefinitionsFile::Element DefinitionsFile::Element::empty = { };

DefinitionsFile::DefinitionsFile(const FilePath &path)
    : m_path(path),
      m_is_valid(false)
{ 
    Parse();
}

void DefinitionsFile::Parse()
{
    m_is_valid = false;

    Reader reader;

    if (!m_path.Open(reader)) {
        return;
    }
    
    m_is_valid = true;

    Array<Pair<String, Section>> sections;

    auto lines = reader.ReadAllLines();

    for (const auto &line : lines) {
        const auto line_trimmed = line.TrimmedLeft();

        if (line_trimmed.Empty()) {
            continue;
        }

        // comment
        if (line_trimmed.GetChar(0) == '#') {
            continue;
        }

        // section block
        if (line_trimmed.GetChar(0) == '[') {
            String section_name;

            for (SizeType index = 1; index < line_trimmed.Length() && line_trimmed.GetChar(index) != ']'; index++) {
                section_name += line_trimmed.GetChar(index);
            }

            if (section_name.Empty()) {
                DebugLog(
                    LogType::Warn,
                    "Empty section name\n"
                );
            }

            sections.PushBack(Pair { std::move(section_name), Section { } });

            continue;
        }

        // key, value pair in block
        auto split = line_trimmed.Split('=');

        for (auto &part : split) {
            part = part.Trimmed();
        }

        if (split.Size() < 2) {
            DebugLog(
                LogType::Warn,
                "Line is not in required format (key = value):\n\t%s\n",
                line_trimmed.Data()
            );

            continue;
        }

        if (sections.Empty()) {
            // no section defined; add a default one
            sections.PushBack(Pair { String("default"), Section { } });
        }

        const String &key = split[0];

        // split value by commas
        Value value;

        for (auto &item : split[1].Split(',')) {
            String item_trimmed = item.Trimmed();

            Element element;

            for (SizeType index = 0; index < item_trimmed.Size(); index++) {
                if (std::isspace(item_trimmed[index])) {
                    continue;
                }

                // read sub-elements
                if (item_trimmed[index] == '(') {
                    ++index;

                    String working_name;

                    while (index < item_trimmed.Size()) {
                        if (std::isspace(item_trimmed[index])) {
                            if (working_name.Any()) {
                                element.sub_elements.PushBack(std::move(working_name));
                            }

                            ++index;
                            continue;
                        }

                        if (item_trimmed[index] == ')') {
                            ++index;
                            break;
                        }

                        working_name += item_trimmed[index];

                        ++index;
                    }

                    if (working_name.Any()) {
                        element.sub_elements.PushBack(std::move(working_name));
                    }
                } else if (item_trimmed[index] == '=') {
                    ++index;

                    // Skip spaces
                    while (index < item_trimmed.Size()) {
                        if (std::isspace(item_trimmed[index])) {
                            ++index;
                        } else {
                            break;
                        }
                    }

                    String value_str;

                    while (index < item_trimmed.Size()) {
                        value_str += item_trimmed[index];

                        ++index;
                    }

                    if (value_str.Any()) {
                        element.value = value_str;
                    }
                } else {
                    element.name += item_trimmed[index];
                }
            }

            value.elements.PushBack(std::move(element));
        }

        auto &last_section = sections.Back();
        
        last_section.second[key] = std::move(value);
    }

    m_sections.Clear();

    for (auto &item : sections) {
        m_sections[item.first] = std::move(item.second);
    }
}

} // namespace hyperion::v2