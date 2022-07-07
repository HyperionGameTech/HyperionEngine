#ifndef TYPE_INFO_HPP
#define TYPE_INFO_HPP

#include <system/Debug.hpp>

namespace hyperion {
namespace vm {

class TypeInfo {
public:
    TypeInfo(const char *name, size_t size, char **names);
    TypeInfo(const TypeInfo &other);
    TypeInfo &operator=(const TypeInfo &other);
    ~TypeInfo();

    bool operator==(const TypeInfo &other) const;

    inline char *const GetName() const { return m_name; }
    inline size_t GetSize() const { return m_size; }
    inline char **const GetNames() const { return m_names; }
    inline const char *GetMemberName(size_t index) const
        { AssertThrow(index < m_size); return m_names[index]; }

private:
    char *m_name;
    size_t m_size;
    char **m_names;
};

} // namespace vm
} // namespace hyperion

#endif
