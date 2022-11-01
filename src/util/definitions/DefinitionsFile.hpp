#ifndef HYPERION_V2_DEFINITIONS_FILE_HPP
#define HYPERION_V2_DEFINITIONS_FILE_HPP

#include <core/Containers.hpp>
#include <util/Defines.hpp>
#include <util/StringUtil.hpp>
#include <util/fs/FsUtil.hpp>
#include <asset/BufferedByteReader.hpp>
#include <system/Debug.hpp>
#include <Config.hpp>

namespace hyperion::v2 {

class DefinitionsFile
{
public:
    struct Value
    {
        Array<String> elements;

        const String &GetValue() const
        {
            return elements.Any()
                ? elements.Front()
                : String::empty;
        }

        const String &GetValue(SizeType index) const
        {
            return index < elements.Size()
                ? elements[index]
                : String::empty;
        }
    };

    struct Section : FlatMap<String, Value>
    {
    };

    DefinitionsFile(const FilePath &path);
    ~DefinitionsFile() = default;

    bool IsValid() const
        { return m_is_valid; }

    const FilePath &GetFilePath() const
        { return m_path; }

    const FlatMap<String, Section> &GetSections() const
        { return m_sections; }

    bool HasSection(const String &key) const
        { return m_sections.Contains(key); }

    Section &GetSection(const String &key)
        { return m_sections[key]; }

    const Section &GetSection(const String &key) const
        { return m_sections.At(key); }

private:
    void Parse();

    bool m_is_valid;
    FilePath m_path;

    FlatMap<String, Section> m_sections;
};

} // namespace hyperion::v2

#endif