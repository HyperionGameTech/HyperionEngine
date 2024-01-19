#ifndef HEAP_MEMORY_HPP
#define HEAP_MEMORY_HPP

#include <script/vm/HeapValue.hpp>
#include <Types.hpp>

#include <ostream>

namespace hyperion {
namespace vm {

struct HeapNode
{
    HeapValue   value;
    HeapNode    *before;
    HeapNode    *after;
};

class Heap
{
    friend std::ostream &operator<<(std::ostream &os, const Heap &heap);
public:
    Heap();

    Heap(const Heap &other)             = delete;
    Heap &operator=(const Heap &other)  = delete;
    
    Heap(Heap &&other)                  = delete;
    Heap &operator=(Heap &&other)       = delete;

    ~Heap();

    SizeType Size() const
        { return m_num_objects; }

    /** Destroy everything on the heap */
    void Purge();
    /** Allocate a new value on the heap. */
    HeapValue *Alloc();
    /** Delete all nodes that are not marked */
    void Sweep(UInt *out_num_collected = nullptr);

private:
    HeapNode *m_head;
    SizeType m_num_objects;
};

} // namespace vm
} // namespace hyperion

#endif
