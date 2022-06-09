#ifndef INTERNAL_BYTE_STREAM_HPP
#define INTERNAL_BYTE_STREAM_HPP

#include <script/compiler/emit/Buildable.hpp>

#include <map>
#include <sstream>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

class InternalByteStream {
public:
    struct Fixup {
        LabelId label_id;
        size_t position;
        size_t offset;
    };

    inline size_t GetSize() const
    {
        return m_stream.size();
    }

    inline void Put(std::uint8_t byte)
    {
        m_stream.push_back(byte);
    }

    inline void Put(const std::uint8_t *bytes, size_t size)
    {
        for (size_t i = 0; i < size; i++) {
            m_stream.push_back(bytes[i]);
        }
    }

    void MarkLabel(LabelId label_id);
    void AddFixup(LabelId label_id, size_t offset = 0);

    std::vector<std::uint8_t> &Bake();

private:
    std::map<LabelId, LabelInfo> m_labels;
    std::vector<std::uint8_t> m_stream;
    std::vector<Fixup> m_fixups;
};

} // namespace hyperion::compiler

#endif
