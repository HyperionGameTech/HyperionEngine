#pragma once

#include <Lang/Compiler/Emit/Buildable.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct Fixup
{
    LabelId labelId = LabelId(-1);
    size_t position = size_t(-1);
    size_t offset = size_t(-1);
};

class InternalByteStream
{
public:
    size_t GetPosition() const
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

    HYP_FORCE_INLINE void Put(const ubyte* bytes, size_t size)
    {
        m_writer.Write(ConstByteView(bytes, size));
    }

    void MarkLabel(LabelId labelId);
    void AddFixup(LabelId labelId, size_t position, size_t offset);
    void AddFixup(LabelId labelId, size_t offset);

    void Bake(const BuildParams& buildParams);

private:
    MemoryByteWriter<DynamicAllocator> m_writer;
    Array<Fixup> m_fixups;
};

} // namespace Hyperion
