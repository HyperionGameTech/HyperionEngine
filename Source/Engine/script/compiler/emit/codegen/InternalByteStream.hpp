#pragma once

#include <script/compiler/emit/Buildable.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct Fixup
{
    LabelId labelId = LabelId(-1);
    SizeType position = SizeType(-1);
    SizeType offset = SizeType(-1);
};

class InternalByteStream
{
public:
    SizeType GetPosition() const
    {
        return m_writer.Position();
    }

    const ByteBuffer& GetData() const
    {
        return m_writer.GetBuffer();
    }

    const Array<Fixup>& GetFixups() const
    {
        return m_fixups;
    }

    HYP_FORCE_INLINE void Put(ubyte byte)
    {
        m_writer.Write(ConstByteView(&byte, 1));
    }

    HYP_FORCE_INLINE void Put(const ubyte* bytes, SizeType size)
    {
        m_writer.Write(ConstByteView(bytes, size));
    }

    void MarkLabel(LabelId labelId);
    void AddFixup(LabelId labelId, SizeType position, SizeType offset);
    void AddFixup(LabelId labelId, SizeType offset);

    void Bake(const BuildParams& buildParams);

private:
    MemoryByteWriter m_writer;
    Array<Fixup> m_fixups;
};

} // namespace Hyperion
