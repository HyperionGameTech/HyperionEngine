#include <script/compiler/emit/codegen/InternalByteStream.hpp>

#include <system/Debug.hpp>
#include <iostream>

namespace hyperion::compiler {

void InternalByteStream::MarkLabel(LabelId label_id)
{
    m_labels[label_id] = LabelInfo {
        LabelPosition(m_stream.Size())
    };
}

void InternalByteStream::AddFixup(LabelId label_id, SizeType offset)
{
    Fixup fixup;
    fixup.label_id = label_id;
    fixup.position = m_stream.Size();
    fixup.offset = offset;

    m_fixups.PushBack(fixup);

    // advance position by adding placeholder bytes
    for (SizeType i = 0; i < sizeof(LabelPosition); i++) {
        m_stream.PushBack(UByte(-1));
    }
}

Array<UByte> &InternalByteStream::Bake()
{
    for (const Fixup &fixup : m_fixups) {
        auto it = m_labels.find(fixup.label_id);
        AssertThrowMsg(it != m_labels.end(), "No label with fixup ID was found");

        LabelPosition label_position = it->second.position;
        label_position += fixup.offset;

        const SizeType fixup_position = fixup.position;
        
        // first, make sure there is enough space in the stream at that position
        AssertThrowMsg(m_stream.Size() >= fixup_position + sizeof(LabelPosition), "Not enough space allotted for the LabelPosition");
        
        // now, make sure that each item in the buffer has been set to -1
        // if this is not the case, it is likely the wrong position
        for (SizeType i = 0; i < sizeof(LabelPosition); i++) {
            AssertThrowMsg(m_stream[fixup_position + i] == UByte(-1), "Placeholder value in buffer should be set to -1");
            m_stream[fixup_position + i] = (label_position >> (i << 3)) & 0xFF;
        }
    }

    // clear fixups vector, no more work to do
    m_fixups.Clear();

    // return the modified stream
    return m_stream;
}

} // namespace hyperion::compiler
