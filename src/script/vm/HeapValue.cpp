#include <script/vm/HeapValue.hpp>
#include <script/vm/Object.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Slice.hpp>
#include <script/vm/ImmutableString.hpp>

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

    if (Object *object = GetPointer<Object>()) {
        HeapValue *proto = nullptr;

        do {
            const auto size = object->GetSize();

            for (SizeType i = 0; i < size; i++) {
                object->GetMember(i).value.Mark();
            }

            if ((proto = object->GetPrototype()) != nullptr) {
                proto->Mark();

                object = proto->GetPointer<Object>();

                // get object value of prototype
                // if ((object = proto->GetPointer<Object>()) == nullptr) {
                //     // if prototype is not an object (has been modified to another value), we're done.
                //     return;
                // }
            } else {
                object = nullptr;
            }
        } while (object != nullptr);
    } else if (Array *array = GetPointer<Array>()) {
        const auto size = array->GetSize();

        for (SizeType i = 0; i < size; i++) {
            array->AtIndex(i).Mark();
        }
    } else if (Slice *slice = GetPointer<Slice>()) {
        const auto size = slice->GetSize();

        for (SizeType i = 0; i < size; i++) {
            slice->AtIndex(i).Mark();
        }
    }
}

} // namespace vm
} // namespace hyperion
