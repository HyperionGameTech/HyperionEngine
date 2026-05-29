/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/containers/Map.hpp>

#include <Core/filesystem/FilePath.hpp>

namespace Hyperion {

class ENGINE_API INIFile
{
public:
    struct Element
    {
        static const Element empty;

        String name;
        String value;
        Array<String> subElements;
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

        const Element& GetValue(size_t index) const
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

        void SetValue(size_t index, Element value)
        {
            if (index >= elements.Size())
            {
                elements.Resize(index + 1);
            }

            elements[index] = std::move(value);
        }
    };

    using Section = TMap<String, Value>;

    INIFile(const FilePath& path);
    ~INIFile() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_isValid;
    }

    HYP_FORCE_INLINE const FilePath& GetFilePath() const
    {
        return m_path;
    }

    HYP_FORCE_INLINE const TMap<String, Section>& GetSections() const
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

    bool m_isValid;
    FilePath m_path;

    TMap<String, Section> m_sections;
};

} // namespace Hyperion
