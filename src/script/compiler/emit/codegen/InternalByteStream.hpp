#ifndef INTERNAL_BYTE_STREAM_HPP
#define INTERNAL_BYTE_STREAM_HPP

#include <script/compiler/emit/Buildable.hpp>
#include <Types.hpp>

#include <map>
#include <sstream>
#include <vector>
#include <cstdint>

namespace hyperion::compiler {

class InternalByteStream {
public:
    struct Fixup
    {
        LabelId label_id;
        SizeType position;
        SizeType offset;
    };

    SizeType GetSize() const
        { return m_stream.Size();  }

    void Put(UByte byte)
        { m_stream.PushBack(byte); }

    void Put(const UByte *bytes, SizeType size)
    {
        for (SizeType i = 0; i < size; i++) {
            m_stream.PushBack(bytes[i]);
        }
    }

    void MarkLabel(LabelId label_id);
    void AddFixup(LabelId label_id, SizeType offset = 0);

    Array<UByte> &Bake();

private:
    std::map<LabelId, LabelInfo> m_labels;
    Array<UByte> m_stream;
    Array<Fixup> m_fixups;
};

} // namespace hyperion::compiler

#endif
