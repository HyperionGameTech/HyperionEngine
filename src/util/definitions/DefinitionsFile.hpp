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
    struct Element
    {
        static const Element empty;

        String          name;
        String          value;
        Array<String>   sub_elements;
    };

    struct Value
    {
        Array<Element> elements;

        const Element &GetValue() const
        {
            return elements.Any()
                ? elements.Front()
                : Element::empty;
        }

        const Element &GetValue(SizeType index) const
        {
            return index < elements.Size()
                ? elements[index]
                : Element::empty;
        }
    };

    using Section = HashMap<String, Value>;

    DefinitionsFile(const FilePath &path);
    ~DefinitionsFile() = default;

    Bool IsValid() const
        { return m_is_valid; }

    const FilePath &GetFilePath() const
        { return m_path; }

    const HashMap<String, Section> &GetSections() const
        { return m_sections; }

    Bool HasSection(const String &key) const
        { return m_sections.Contains(key); }

    Section &GetSection(const String &key)
        { return m_sections[key]; }

private:
    void Parse();

    Bool                        m_is_valid;
    FilePath                    m_path;

    HashMap<String, Section>    m_sections;
};

} // namespace hyperion::v2

#endif