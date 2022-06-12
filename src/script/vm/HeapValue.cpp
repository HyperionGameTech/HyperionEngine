#include <script/vm/HeapValue.hpp>
#include <script/vm/Object.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Slice.hpp>
#include <script/vm/ImmutableString.hpp>

#include <iostream>

namespace hyperion {
namespace vm {

HeapValue::HeapValue()
    : m_holder(nullptr),
      m_ptr(nullptr),
      m_flags(0)
{
}

HeapValue::~HeapValue()
{
    if (m_holder != nullptr) {
        delete m_holder;
        m_holder = nullptr;
    }
}

void HeapValue::Mark()
{
    m_flags |= GC_MARKED;

    if (Object *object = GetPointer<Object>()) {
        HeapValue *proto = nullptr;

        //do {
            const size_t size = object->GetSize();

            for (size_t i = 0; i < size; i++) {
                object->GetMember(i).value.Mark();
            }

            if ((proto = object->GetPrototype()) != nullptr) {
                proto->Mark();

                // get object value of prototype
                // if ((object = proto->GetPointer<Object>()) == nullptr) {
                //     // if prototype is not an object (has been modified to another value), we're done.
                //     return;
                // }
            }
        //} while (proto != nullptr);
    } else if (Array *array = GetPointer<Array>()) {
        const size_t size = array->GetSize();

        for (int i = 0; i < size; i++) {
            array->AtIndex(i).Mark();
        }
    } else if (Slice *slice = GetPointer<Slice>()) {
        const size_t size = slice->GetSize();

        for (int i = 0; i < size; i++) {
            slice->AtIndex(i).Mark();
        }
    }
}

} // namespace vm
} // namespace hyperion
