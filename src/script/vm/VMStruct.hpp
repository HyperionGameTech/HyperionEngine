#ifndef HYPERION_VM_STRUCT_HPP
#define HYPERION_VM_STRUCT_HPP

#include <core/lib/ByteBuffer.hpp>
#include <core/lib/Span.hpp>
#include <core/lib/Variant.hpp>
#include <script/vm/Value.hpp>

#include <Types.hpp>

namespace hyperion {
namespace vm {

enum VMStructType : UInt8
{
    VM_STRUCT_TYPE_NONE = 0,
    VM_STRUCT_TYPE_I8,
    VM_STRUCT_TYPE_I16,
    VM_STRUCT_TYPE_I32,
    VM_STRUCT_TYPE_I64,
    VM_STRUCT_TYPE_U8,
    VM_STRUCT_TYPE_U16,
    VM_STRUCT_TYPE_U32,
    VM_STRUCT_TYPE_U64,
    VM_STRUCT_TYPE_F32,
    VM_STRUCT_TYPE_F64,

    VM_STRUCT_TYPE_DYNAMIC,

    VM_STRUCT_TYPE_STRING = VM_STRUCT_TYPE_DYNAMIC,
    VM_STRUCT_TYPE_STRUCT
};

using VMStructMemory = ByteBuffer;
using VMStructMemoryView = Span<const UByte>;

struct VMStructMemberView
{
    UInt32 offset; // offset in binary object

    VMStructType type;
    VMStructMemoryView name_view;
    Variant<VMStructMemoryView, Value::ValueData> data_view;
};

struct VMStructView
{
    Array<VMStructMemberView> members;
};

struct VMStructMember
{
    VMStructType type;
    ANSIString name;
    Variant<ByteBuffer, Value::ValueData> data_buffer;
};

class VMStruct
{
public:
    VMStruct() = default;

    // construct from memory
    explicit VMStruct(const VMStructMemory &memory)
        : m_bytes(memory)
    {
    }

    // construct from memory
    explicit VMStruct(const VMStructMemoryView &memory_view)
        : m_bytes(memory_view)
    {
    }

    VMStruct(const VMStruct &other) = default;
    VMStruct &operator=(const VMStruct &other) = default;
    VMStruct(VMStruct &&other) noexcept = default;
    VMStruct &operator=(VMStruct &&other) noexcept = default;
    ~VMStruct() = default;

    const VMStructMemory &GetStructMemory() const
        { return m_bytes; }

    static bool Make(const Array<VMStructMember> &members, VMStruct &result)
    {
        MemoryByteWriter writer;

        for (auto &it : members) {
            if (it.type & (1 << 7)) {
                return false;
            }

            writer.Write<UInt8>(UInt8(it.type) | (1 << 7));

            if (it.name.Size() > MathUtil::MaxSafeValue<UInt16>()) {
                return false;
            }

            writer.Write<UInt16>(UInt16(it.name.Size()));
            writer.Write(it.name.Data(), it.name.Size());

            if (it.type >= VM_STRUCT_TYPE_DYNAMIC) {
                const ByteBuffer *byte_buffer = it.data_buffer.TryGet<ByteBuffer>();

                if (!byte_buffer) {
                    return false;
                }

                if (byte_buffer->Size() > MathUtil::MaxSafeValue<UInt32>()) {
                    return false;
                }

                writer.Write<UInt32>(UInt32(byte_buffer->Size()));
                writer.Write(byte_buffer->Data(), byte_buffer->Size());
            } else {
                const Value::ValueData *data = it.data_buffer.TryGet<Value::ValueData>();

                if (!data) {
                    return false;
                }

                // read next n bytes
                switch (it.type) {
                case VM_STRUCT_TYPE_I8:
                    writer.Write<Int8>(data->i8);
                    break;
                case VM_STRUCT_TYPE_U8:
                    writer.Write<UInt8>(data->u8);
                    break;
                case VM_STRUCT_TYPE_I16:
                    writer.Write<Int16>(data->i16);
                    break;
                case VM_STRUCT_TYPE_U16:
                    writer.Write<UInt8>(data->i16);
                    break;
                case VM_STRUCT_TYPE_I32:
                    writer.Write<Int32>(data->i32);
                    break;
                case VM_STRUCT_TYPE_U32:
                    writer.Write<UInt32>(data->u32);
                    break;
                case VM_STRUCT_TYPE_I64:
                    writer.Write<Int64>(data->i64);
                    break;
                case VM_STRUCT_TYPE_U64:
                    writer.Write<UInt64>(data->u64);
                    break;
                case VM_STRUCT_TYPE_F32:
                    writer.Write<Float32>(data->f);
                    break;
                case VM_STRUCT_TYPE_F64:
                    writer.Write<Float64>(data->d);
                    break;
                default:
                    AssertThrow(false);
                    break;
                }
            }
        }

        result.m_bytes.SetData(SizeType(writer.Position()), writer.GetData().Data());

        return true;
    }

    static bool ReadMemberAtOffset(const VMStructMemory &memory, SizeType &offset, VMStructMemberView &out)
    {
        if (offset >= memory.Size()) {
            return false;
        }

        out.offset = UInt32(offset);

        UInt8 type;
        { // read type; also marked with a bit to show it is a valid member

            if (!memory.Read<UInt8>(offset, &type)) {
                return false;
            }

            if (!(type & (1 << 7))) {
                DebugLog(LogType::Error, "invalid struct member header\n");

                return false;
            }

            offset += sizeof(UInt8);

            type &= ~(1 << 7);
            out.type = VMStructType(type);
        }

        UInt16 name_length;
        {
            if (!memory.Read<UInt16>(offset, &name_length)) {
                return false;
            }

            offset += sizeof(UInt16);
        }

        out.name_view = VMStructMemoryView(memory.Data() + offset, SizeType(name_length));
        offset += name_length;
    
        if (type >= VM_STRUCT_TYPE_DYNAMIC) {
            UInt32 data_length;
            {
                if (!memory.Read<UInt32>(offset, &data_length)) {
                    return false;
                }

                offset += sizeof(UInt32);
            }

            out.data_view.Set(VMStructMemoryView(memory.Data() + offset, SizeType(data_length)));

            offset += data_length;
        } else {
            UInt8 byte_size = 0;

            Value::ValueData data;

            // read next n bytes
            switch (VMStructType(type)) {
            case VM_STRUCT_TYPE_I8:
                byte_size = 1;
                memory.Read(offset, &data.i8);
                break;
            case VM_STRUCT_TYPE_U8:
                byte_size = 1;
                memory.Read(offset, &data.u8);
                break;
            case VM_STRUCT_TYPE_I16:
                byte_size = 2;
                memory.Read(offset, &data.i16);
                break;
            case VM_STRUCT_TYPE_U16:
                byte_size = 2;
                memory.Read(offset, &data.u16);
                break;
            case VM_STRUCT_TYPE_I32:
                byte_size = 4;
                memory.Read(offset, &data.i32);
                break;
            case VM_STRUCT_TYPE_U32:
                byte_size = 4;
                memory.Read(offset, &data.u32);
                break;
            case VM_STRUCT_TYPE_I64:
                byte_size = 8;
                memory.Read(offset, &data.i64);
                break;
            case VM_STRUCT_TYPE_U64:
                byte_size = 8;
                memory.Read(offset, &data.u64);
                break;
            case VM_STRUCT_TYPE_F32:
                byte_size = 4;
                memory.Read(offset, &data.f);
                break;
            case VM_STRUCT_TYPE_F64:
                byte_size = 8;
                memory.Read(offset, &data.d);
                break;
            default:
                AssertThrow(false);
                break;
            }

            out.data_view.Set(data);

            offset += byte_size;
        }

        return true;
    }

    // Todo: Use VMStructMemoryView instead
    static bool Decompose(const VMStructMemory &memory, VMStructView &out)
    {
        const SizeType memory_size = memory.Size();

        for (SizeType memory_offset = 0; memory_offset < memory_size;) {
            vm::VMStructMemberView member_view;

            if (!ReadMemberAtOffset(memory, memory_offset, member_view)) {
                return false;
            }

            out.members.PushBack(std::move(member_view));
        }

        return true;
    }

private:
    VMStructMemory m_bytes;
};

} // namespace vm
} // namespace hyperion

#endif