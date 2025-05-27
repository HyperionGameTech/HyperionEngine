/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_INI_FILE_HPP
#define HYPERION_INI_FILE_HPP

#include <core/Defines.hpp>
#include <core/containers/HashMap.hpp>

#include <core/filesystem/FilePath.hpp>

namespace hyperion {

class HYP_API INIFile
{
public:
    struct Element
    {
        static const Element empty;

        String name;
        String value;
        Array<String> sub_elements;
    };

    struct Value
    {
        Array<Element> elements;

        const Element& GetValue() const
        {
            return elements.Any()
                ? elements.Front()
                : Element::empty;
        }

        const Element& GetValue(SizeType index) const
        {
            return index < elements.Size()
                ? elements[index]
                : Element::empty;
        }

        void SetValue(Element value)
        {
            elements.Clear();
            elements.PushBack(std::move(value));
        }

        void SetValue(SizeType index, Element value)
        {
            if (index >= elements.Size())
            {
                elements.Resize(index + 1);
            }

            elements[index] = std::move(value);
        }
    };

    using Section = HashMap<String, Value>;

    INIFile(const FilePath& path);
    ~INIFile() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_is_valid;
    }

    HYP_FORCE_INLINE const FilePath& GetFilePath() const
    {
        return m_path;
    }

    HYP_FORCE_INLINE const HashMap<String, Section>& GetSections() const
    {
        return m_sections;
    }

    HYP_FORCE_INLINE bool HasSection(UTF8StringView key) const
    {
        return m_sections.Contains(key);
    }

    HYP_FORCE_INLINE Section& GetSection(UTF8StringView key)
    {
        return m_sections[key];
    }

private:
    void Parse();

    bool m_is_valid;
    FilePath m_path;

    HashMap<String, Section> m_sections;
};

} // namespace hyperion

#endif