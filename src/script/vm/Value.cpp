#include <script/vm/Value.hpp>
#include <script/vm/Object.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Slice.hpp>
#include <script/vm/ImmutableString.hpp>
#include <script/vm/HeapValue.hpp>

#include <system/debug.h>

#include <stdio.h>
#include <cinttypes>
#include <iostream>

namespace hyperion {
namespace vm {

static const ImmutableString NULL_STRING = ImmutableString("null");
static const ImmutableString BOOLEAN_STRINGS[2] = {
    ImmutableString("false"),
    ImmutableString("true")
};

Value::Value(const Value &other)
    : m_type(other.m_type),
      m_value(other.m_value)
{
}

void Value::Mark()
{
    switch (m_type) {
        case Value::VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            m_value.value_ref->Mark();
            break;
        
        case Value::HEAP_POINTER: {
            HeapValue *ptr = m_value.ptr;

            if (ptr != nullptr && !(ptr->GetFlags() & GC_MARKED)) {
                ptr->Mark();
                ptr->GetFlags() |= GC_MARKED;
            }
        }

        default: break;
    }
}

const char *Value::GetTypeString() const
{
    switch (m_type) {
        case NONE:    return "<Uninitialized Data>";
        case I32:     // fallthrough
        case I64:     return "Int";
        case F32:     // fallthrough
        case F64:     return "Float";
        case BOOLEAN: return "Boolean";
        case VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            return m_value.value_ref->GetTypeString();

        case HEAP_POINTER: 
            if (m_value.ptr == nullptr) {
                return "Null";
            } else if (m_value.ptr->GetPointer<ImmutableString>()) {
                return "String";
            } else if (m_value.ptr->GetPointer<Array>() || m_value.ptr->GetPointer<Slice>()) {
                return "Array";
            } else if (Object *object = m_value.ptr->GetPointer<Object>()) {
                return "Object"; // TODO prototype name
            }

            return "Object";

        case FUNCTION:        return "Function";
        case NATIVE_FUNCTION: return "NativeFunction";
        case ADDRESS:         return "Address";
        case FUNCTION_CALL:   return "FunctionCallInfo";
        case TRY_CATCH_INFO:  return "TryCatchInfo";
        case USER_DATA:       return "UserData";
        default:              return "??";
    }
}

ImmutableString Value::ToString() const
{
    const size_t buf_size = 256;
    char buf[buf_size] = {0};

    switch (m_type) {
        case Value::I32: {
            int n = snprintf(buf, buf_size, "%d", m_value.i32);
            return ImmutableString(buf, n);
        }

        case Value::I64: {
            int n = snprintf(
                buf,
                buf_size,
                "%" PRId64,
                m_value.i64
            );
            return ImmutableString(buf, n);
        }

        case Value::F32: {
            int n = snprintf(
                buf,
                buf_size,
                "%g",
                m_value.f
            );
            return ImmutableString(buf, n);
        }

        case Value::F64: {
            int n = snprintf(
                buf,
                buf_size,
                "%g",
                m_value.d
            );
            return ImmutableString(buf, n);
        }

        case Value::BOOLEAN:
            return BOOLEAN_STRINGS[m_value.b];

        case Value::VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            return m_value.value_ref->ToString();

        case Value::HEAP_POINTER: {
            if (m_value.ptr == nullptr) {
                return NULL_STRING;
            } else if (ImmutableString *string = m_value.ptr->GetPointer<ImmutableString>()) {
                return *string;
            } else if (Array *array = m_value.ptr->GetPointer<Array>()) {
                std::stringstream ss;
                array->GetRepresentation(ss, true);
                const std::string &str = ss.str();
                return ImmutableString(str.c_str());
            } else if (Slice *slice = m_value.ptr->GetPointer<Slice>()) {
                std::stringstream ss;
                slice->GetRepresentation(ss, true);
                const std::string &str = ss.str();
                return ImmutableString(str.c_str());
            } /*else if (Object *object = m_value.ptr->GetPointer<Object>()) {
                std::stringstream ss;
                object->GetRepresentation(ss, true);
                const std::string &str = ss.str();
                return ImmutableString(str.c_str());
            } */ else {
                // return memory address as string
                int n = snprintf(buf, buf_size, "%p", (void*)m_value.ptr);
                return ImmutableString(buf, n);
            }

            break;
        }

        default: return ImmutableString(GetTypeString());
    }
}

void Value::ToRepresentation(std::stringstream &ss, bool add_type_name) const
{
    switch (m_type) {
        case Value::VALUE_REF:
            AssertThrow(m_value.value_ref != nullptr);
            m_value.value_ref->ToRepresentation(ss, add_type_name);
            return;

        case Value::HEAP_POINTER:
            if (m_value.ptr == nullptr) {
                ss << "null";
            } else if (ImmutableString *string = m_value.ptr->GetPointer<ImmutableString>()) {
                ss << '\"';
                ss << string->GetData();
                ss << '\"';
            } else if (Array *array = m_value.ptr->GetPointer<Array>()) {
                array->GetRepresentation(ss, add_type_name);
            } else if (Slice *slice = m_value.ptr->GetPointer<Slice>()) {
                slice->GetRepresentation(ss, add_type_name);
            } else if (Object *object = m_value.ptr->GetPointer<Object>()) {
                object->GetRepresentation(ss, add_type_name);
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
