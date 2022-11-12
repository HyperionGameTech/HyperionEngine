#include <script/vm/Value.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMMemoryBuffer.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMString.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/Hasher.hpp>

#include <system/Debug.hpp>

#include <stdio.h>
#include <cinttypes>
#include <iostream>

namespace hyperion {
namespace vm {

static const VMString NULL_STRING = VMString("null");
static const VMString BOOLEAN_STRINGS[2] = { VMString("false"), VMString("true") };

Value::Value(const Value &other)
    : m_type(other.m_type),
      m_value(other.m_value)
{
}

Value::Value(ValueType value_type, ValueData value_data)
    : m_type(value_type),
      m_value(value_data)
{
}

int Value::CompareAsPointers(
    Value *lhs,
    Value *rhs
)
{
    HeapValue *a = lhs->m_value.ptr;
    HeapValue *b = rhs->m_value.ptr;

    if (a == b) {
        // pointers equal, drop out early.
        return CompareFlags::EQUAL;
    } else if (a == nullptr || b == nullptr) {
        return CompareFlags::NONE;
    } else if (a->GetRawPointer() == b->GetRawPointer()) {
        // pointers equal
        return CompareFlags::EQUAL;
    } else if (a->GetTypeID() != b->GetTypeID()) {
        // type IDs not equal, drop out
        return CompareFlags::NONE;
    } else if (const VMObject *vm_object = a->GetPointer<VMObject>()) {
        return *vm_object == b->Get<VMObject>()
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    } else if (const VMString *vm_string = a->GetPointer<VMString>()) {
        return *vm_string == b->Get<VMString>()
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    } else if (const VMArray *vm_array = a->GetPointer<VMArray>()) {
        return *vm_array == b->Get<VMArray>()
            ? CompareFlags::EQUAL
            : CompareFlags::NONE;
    } else {
        return CompareFlags::NONE;
    }
}

void Value::Mark()
{
    switch (m_type) {
        case Value::VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            AssertThrowMsg(false, "Not supported");
            m_value.value_ref->Mark();
            break;
        
        case Value::HEAP_POINTER: {
            HeapValue *ptr = m_value.ptr;
            
            if (ptr != nullptr) {
                AssertThrowMsg(!(ptr->GetFlags() & GC_DESTROYED), "VM heap corruption! VMObject had flag GC_DESTROYED in Mark()");

                if (!(ptr->GetFlags() & GC_ALIVE)) {
                    ptr->Mark();
                }
            }
        }

        default:
            break;
    }
}

const char *Value::GetTypeString() const
{
    switch (m_type) {
        case NONE:
            return "<Uninitialized Data>";
        case I32: // fallthrough
            return "Int32";
        case I64:
            return "Int64";
        case U32: // fallthrough
            return "UInt32";
        case U64:
            return "UInt64";
        case F32: // fallthrough
            return "Float";
        case F64:
            return "Double";
        case BOOLEAN:
            return "Bool";
        case VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            return m_value.value_ref->GetTypeString();

        case HEAP_POINTER: 
            if (m_value.ptr == nullptr) {
                return "Null";
            } else if (m_value.ptr->GetPointer<VMString>()) {
                return "String";
            } else if (m_value.ptr->GetPointer<VMArray>() || m_value.ptr->GetPointer<VMArraySlice>()) {
                return "Array";
            } else if (m_value.ptr->GetPointer<VMMemoryBuffer>()) {
                return "MemoryBuffer";
            } else if (VMObject *object = m_value.ptr->GetPointer<VMObject>()) {
                return "Object"; // TODO prototype name
            }

            return "Pointer";

        case FUNCTION: return "Function";
        case NATIVE_FUNCTION: return "NativeFunction";
        case ADDRESS: return "Address";
        case FUNCTION_CALL: return "FunctionCallInfo";
        case TRY_CATCH_INFO: return "TryCatchInfo";
        case USER_DATA: return "UserData";
        default: return "??";
    }
}

VMString Value::ToString() const
{
    const size_t buf_size = 256;
    char buf[buf_size] = {0};

    const int depth = 3;

    switch (m_type) {
        case Value::I32: {
            int n = snprintf(buf, buf_size, "%d", m_value.i32);
            return VMString(buf, n);
        }

        case Value::I64: {
            int n = snprintf(
                buf,
                buf_size,
                "%" PRId64,
                m_value.i64
            );
            return VMString(buf, n);
        }

        case Value::U32: {
            int n = snprintf(buf, buf_size, "%u", m_value.u32);
            return VMString(buf, n);
        }

        case Value::U64: {
            int n = snprintf(
                buf,
                buf_size,
                "%" PRIu64,
                m_value.u64
            );
            return VMString(buf, n);
        }

        case Value::F32: {
            int n = snprintf(
                buf,
                buf_size,
                "%g",
                m_value.f
            );
            return VMString(buf, n);
        }

        case Value::F64: {
            int n = snprintf(
                buf,
                buf_size,
                "%g",
                m_value.d
            );
            return VMString(buf, n);
        }

        case Value::BOOLEAN:
            return BOOLEAN_STRINGS[m_value.b];

        case Value::VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            return m_value.value_ref->ToString();

        case Value::HEAP_POINTER: {
            if (m_value.ptr == nullptr) {
                return NULL_STRING;
            } else if (VMString *string = m_value.ptr->GetPointer<VMString>()) {
                return *string;
            } else if (VMArray *array = m_value.ptr->GetPointer<VMArray>()) {
                std::stringstream ss;
                array->GetRepresentation(ss, true, depth);
                const std::string &str = ss.str();
                return VMString(str.c_str());
            } else if (VMMemoryBuffer *memory_buffer = m_value.ptr->GetPointer<VMMemoryBuffer>()) {
                std::stringstream ss;
                memory_buffer->GetRepresentation(ss, true, depth);
                const std::string &str = ss.str();
                return VMString(str.c_str());
            } else if (VMArraySlice *slice = m_value.ptr->GetPointer<VMArraySlice>()) {
                std::stringstream ss;
                slice->GetRepresentation(ss, true, depth);
                const std::string &str = ss.str();
                return VMString(str.c_str());
            } else if (VMObject *object = m_value.ptr->GetPointer<VMObject>()) {
                std::stringstream ss;
                object->GetRepresentation(ss, true, depth);
                const std::string &str = ss.str();
                return VMString(str.c_str());
            } else {
                // return memory address as string
                int n = snprintf(buf, buf_size, "%p", (void*)m_value.ptr);
                return VMString(buf, n);
            }

            break;
        }

        default:
            return VMString(GetTypeString());
    }
}

void Value::ToRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    switch (m_type) {
        case Value::VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);

            m_value.value_ref->ToRepresentation(
                ss,
                add_type_name,
                depth - 1
            );

            return;

        case Value::HEAP_POINTER:
            if (m_value.ptr == nullptr) {
                ss << "null";
            } else if (VMString *string = m_value.ptr->GetPointer<VMString>()) {
                ss << '\"';
                ss << string->GetData();
                ss << '\"';
            } else if (VMArray *array = m_value.ptr->GetPointer<VMArray>()) {
                array->GetRepresentation(ss, add_type_name, depth - 1);
            } else if (VMArraySlice *slice = m_value.ptr->GetPointer<VMArraySlice>()) {
                slice->GetRepresentation(ss, add_type_name, depth - 1);
            } else if (VMObject *object = m_value.ptr->GetPointer<VMObject>()) {
                object->GetRepresentation(ss, add_type_name, depth - 1);
            } else {
                if (add_type_name) {
                    ss << GetTypeString();
                    ss << '(';
                }
                
                ss << ToString().GetData();

                if (add_type_name) {
                    ss << ')';
                }
            }

            break;
        default:
            ss << ToString().GetData();
    }
}

} // namespace vm
} // namespace hyperion
