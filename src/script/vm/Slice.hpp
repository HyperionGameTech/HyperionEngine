#ifndef SLICE_HPP
#define SLICE_HPP

#include <script/vm/Array.hpp>
#include <script/vm/Value.hpp>

#include <cstdint>

namespace hyperion {
namespace vm {

class Slice {
public:
    using SizeType = Array::SizeType;

    Slice(Array *ary, SizeType start, SizeType end);
    Slice(const Slice &other);

    Slice &operator=(const Slice &other);
    bool operator==(const Slice &other) const { return this == &other; }

    SizeType GetSize() const              { return m_end - m_start; }
    Value &AtIndex(int index)             { return m_ary->AtIndex(m_start + index); }
    const Value &AtIndex(int index) const { return m_ary->AtIndex(m_start + index); }

    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

private:
    Array *m_ary;
    SizeType m_start;
    SizeType m_end;
};

}
}

#endif
