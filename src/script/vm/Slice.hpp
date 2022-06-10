#ifndef SLICE_HPP
#define SLICE_HPP

#include <script/vm/Array.hpp>
#include <script/vm/Value.hpp>

#include <cstdint>

namespace hyperion {
namespace vm {

class Slice {
public:
    Slice(Array *ary, std::size_t start, std::size_t end);
    Slice(const Slice &other);

    Slice &operator=(const Slice &other);
    inline bool operator==(const Slice &other) const { return this == &other; }

    inline std::size_t GetSize() const { return m_end - m_start; }
    inline Value &AtIndex(int index) { return m_ary->AtIndex(m_start + index); }
    inline const Value &AtIndex(int index) const { return m_ary->AtIndex(m_start + index); }

    void GetRepresentation(std::stringstream &ss, bool add_type_name = true) const;

private:
    Array *m_ary;
    std::size_t m_start;
    std::size_t m_end;
};

}
}

#endif
