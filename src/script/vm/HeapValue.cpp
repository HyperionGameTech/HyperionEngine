#include <script/vm/HeapValue.hpp>
#include <script/vm/VMObject.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/VMArraySlice.hpp>
#include <script/vm/VMString.hpp>

#include <iostream>

namespace hyperion {
namespace vm {

HeapValue::HeapValue()
    : m_flags(0)
{
}

HeapValue::~HeapValue() = default;

void HeapValue::Mark()
{
    AssertThrow(!(m_flags & GC_DESTROYED));

    m_flags |= GC_MARKED;

    if (VMObject *object = GetPointer<VMObject>()) {
        const auto size = object->GetSize();

        for (SizeType i = 0; i < size; i++) {
            object->GetMember(i).value.Mark();
        }

        if (HeapValue *class_pointer = object->GetClassPointer()) {
            class_pointer->Mark();
        }
    } else if (VMArray *array = GetPointer<VMArray>()) {
        const auto size = array->GetSize();

        for (SizeType i = 0; i < size; i++) {
            array->AtIndex(i).Mark();
        }
    } else if (VMArraySlice *slice = GetPointer<VMArraySlice>()) {
        const auto size = slice->GetSize();

        for (SizeType i = 0; i < size; i++) {
            slice->AtIndex(i).Mark();
        }
    }
}

} // namespace vm
} // namespace hyperion
