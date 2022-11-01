#include <util/definitions/DefinitionsFile.hpp>
#include <system/Debug.hpp>

namespace hyperion::v2 {

DefinitionsFile::DefinitionsFile(const FilePath &path)
    : m_path(path),
      m_is_valid(false)
{
    Parse();
}

void DefinitionsFile::Parse()
{
    m_is_valid = false;

    if (auto reader = m_path.Open()) {
        m_is_valid = true;

        Array<Pair<String, Section>> sections;

        auto lines = reader.ReadAllLines();

        for (const auto &line : lines) {
            const auto line_trimmed = line.TrimmedLeft();

            if (line_trimmed.Empty()) {
                continue;
            }

            // comment
            if (line_trimmed[0] == '#') {
                continue;
            }

            // section block
            if (line_trimmed[0] == '[') {
                std::string section_name;

                for (SizeType index = 1; index < line_trimmed.Size() && line_trimmed[index] != ']'; index++) {
                    section_name += line_trimmed[index];
                }

                if (section_name.empty()) {
                    DebugLog(
                        LogType::Warn,
                        "Empty section name\n"
                    );
                }

                sections.PushBack(Pair { String(section_name.c_str()), Section { } });

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
                sections.PushBack(Pair { String("default"), Section {  } });
            }

            const auto &key = split[0];

            // split value by commas
            Value value;

            for (auto &item : split[1].Split(',')) {
                value.elements.PushBack(item.Trimmed());
            }

            sections.Back().second[key] = std::move(value);
        }

        m_sections.Clear();

        for (auto &item : sections) {
            m_sections[item.first] = std::move(item.second);
        }
    }
}

} // namespace hyperion::v2