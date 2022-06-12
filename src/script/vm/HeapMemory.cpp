#include <script/vm/HeapMemory.hpp>

#include <script/vm/Object.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Slice.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/ImmutableString.hpp>
#include <script/vm/TypeInfo.hpp>

#include <iostream>
#include <iomanip>
#include <bitset>
#include <sstream>

namespace hyperion {
namespace vm {

std::ostream &operator<<(std::ostream &os, const Heap &heap)
{
    // print table header
    os << std::left;
    os << std::setw(16) << "Address" << "| ";
    os << std::setw(8) << "Flags" << "| ";
    os << std::setw(10) << "Type" << "| ";
    os << std::setw(16) << "Value";
    os << std::endl;

    HeapNode *tmp_head = heap.m_head;
    while (tmp_head != nullptr) {
        os << std::setw(16) << (void*)tmp_head << "| ";
        os << std::setw(8) << std::bitset<sizeof(tmp_head->value.GetFlags())>(tmp_head->value.GetFlags()) << "| ";
        os << std::setw(10);

        {
            union {
                ImmutableString *str_ptr;
                Array *array_ptr;
                Slice *slice_ptr;
                Object *obj_ptr;
                TypeInfo *type_info_ptr;
            } data;
            
            if (!tmp_head->value.GetId()) {
                os << "NullType" << "| ";

                os << std::setw(16);
                os << "null";
            } else if ((data.str_ptr = tmp_head->value.GetPointer<ImmutableString>()) != nullptr) {
                os << "String" << "| ";

                os << "\"" << data.str_ptr->GetData() << "\"" << std::setw(16);
            } else if ((data.array_ptr = tmp_head->value.GetPointer<Array>()) != nullptr) {
                os << "Array" << "| ";

                os << std::setw(16);
                
                std::stringstream ss;
                data.array_ptr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            } else if ((data.slice_ptr = tmp_head->value.GetPointer<Slice>()) != nullptr) {
                os << "ArraySlice" << "| ";
                os << std::setw(16);

                std::stringstream ss;
                data.slice_ptr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            } else if ((data.obj_ptr = tmp_head->value.GetPointer<Object>()) != nullptr) {
                os << "Object" << "| ";

                os << std::setw(16);
                std::stringstream ss;
                data.obj_ptr->GetRepresentation(ss, false);
                os << ss.rdbuf();
            } else if ((data.type_info_ptr = tmp_head->value.GetPointer<TypeInfo>()) != nullptr) {
                os << "TypeInfo" << "| ";

                os << std::setw(16);
                os << " ";
            } else {
                os << "Pointer" << "| ";

                os << std::setw(16);
                os << " ";
            }
        }

        os << std::endl;

        tmp_head = tmp_head->before;
    }

    return os;
}

Heap::Heap()
    : m_head(nullptr),
      m_num_objects(0)
{
}

Heap::~Heap()
{
    // purge the heap on destructor
    Purge();
}

void Heap::Purge()
{
    // clean up all allocated objects
    while (m_head) {
        HeapNode *tmp = m_head;
        m_head = tmp->before;
        delete tmp;

        m_num_objects--;
    }
}

HeapValue *Heap::Alloc()
{
    HeapNode *node = new HeapNode;

    node->after = nullptr;
    
    if (m_head != nullptr) {
        m_head->after = node;
    }
    
    node->before = m_head;
    m_head = node;

    m_num_objects++;

    return &m_head->value;
}

void Heap::Sweep()
{
    HeapNode *last = m_head;
    while (last) {
        if (!(last->value.GetFlags() & GC_MARKED)) {
            // unmarked object, so delete it

            HeapNode *after = last->after;
            HeapNode *before = last->before;

            if (before) {
                before->after = after;
            }

            if (after) {
                // removing an item from the middle, so
                // make the nodes to the other sides now
                // point to each other
                after->before = before;
            } else {
                // since there are no nodes after this,
                // set the head to be this node here
                m_head = before;
            }

            AssertThrow(!(last->value.GetFlags() & GC_DESTROYED));
            last->value.GetFlags() |= GC_DESTROYED;

            // delete the middle node
            delete last;
            last = before;

            // decrement number of currently allocated
            // objects
            m_num_objects--;

        } else {
            // the object is currently marked, so
            // we unmark it for the next time
            last->value.GetFlags() &= ~GC_MARKED;
            last = last->before;
        }
    }
}

} // namespace vm
} // namespace hyperion
