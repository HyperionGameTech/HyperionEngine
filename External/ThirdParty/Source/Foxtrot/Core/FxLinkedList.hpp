#pragma once

#include "FxPagedArray.hpp"
#include "FxTypes.hpp"

#include <cstdlib>

// #define FX_LINKED_LIST_NO_REUSE_NODES 1

template <typename ElementType>
class FxLinkedList
{
public:
    struct Node
    {
    public:
        Node* Next = nullptr;
        Node* Prev = nullptr;
        ElementType Data;

    private:
        friend class FxLinkedList;
        inline ElementType& InsertAfterNode(Node* other)
        {
            Prev = other;
            Next = other->Next;

            if (other->Next) {
                other->Next->Prev = this;
            }
            other->Next = this;

            return Data;
        }

        inline ElementType& InsertBeforeNode(Node* other)
        {
            Prev = other->Prev;
            Next = other;

            if (other->Prev) {
                other->Prev->Next = this;
            }
            other->Prev = this;

            return Data;
        }
    };

public:
    FxLinkedList() = default;

    FxLinkedList(uint32 initial_pool_size) { Create(initial_pool_size); }

    void Create(uint32 initial_pool_size = 32)
    {
        mNodePool.Create(initial_pool_size);
        mFreedNodes.Create(initial_pool_size);
    }

    Node* NewNode(const ElementType& data)
    {
        Node* node = nullptr;

#ifdef FX_LINKED_LIST_NO_REUSE_NODES
        // Check if there are available spaces that have been freed recently
        if (!mFreedNodes.IsEmpty()) {
            // Set the node to use as the last node that was freed
            node = mFreedNodes.GetLast();

            // Remove the node from the freed list
            mFreedNodes.RemoveLast();
        }
        // No nodes can be recycled, insert a new one
        if (node == nullptr) {
            node = mNodePool.Insert();
        }

#else
        node = mNodePool.Insert();
#endif


        node->Data = data;
        node->Prev = nullptr;
        node->Next = nullptr;

        return node;
    }

    void DeleteNode(Node* node)
    {
        // Node has previously been removed or not integrated, ignore.
        if (node->Next == nullptr && node->Prev == nullptr) {
            return;
        }

#ifdef FX_LINKED_LIST_NO_REUSE_NODES
        // Insert the node into the freed list. This allocated node will be reused when needed.
        {
            auto* element = mFreedNodes.Insert();

            (*element) = node;
        }
#endif

        // If the current node is the head of the list, set the head to be the next node.
        // This means that if there is no node next, the head will be set to null.
        if (node == Head) {
            Head = node->Next;
        }

        // If the current node is the tail, set the tail to be the node behind the current one.
        if (node == Tail) {
            Tail = node->Prev;
        }

        // Since the node will be removed, make the previous and next nodes point to each other
        if (node->Prev != nullptr) {
            node->Prev->Next = node->Next;
        }
        if (node->Next != nullptr) {
            node->Next->Prev = node->Prev;
        }

        node->Next = nullptr;
        node->Prev = nullptr;
    }

    /**
     * @brief Inserts a value at the end of a list.
     */
    Node* InsertLast(const ElementType& element)
    {
        Node* node = NewNode(element);

        // If there is a tail, insert the element after
        if (Tail != nullptr) {
            node->InsertAfterNode(Tail);
            Tail = node;
        }
        // If there is no tail, insert this element as the tail.
        else {
            Tail = node;
        }

        // This the first node in the list, set the head and tail to be equal
        if (Head == nullptr) {
            Head = Tail;
        }

        return node;
    }

    /**
     * Inserts a value at the start of the list.
     */
    Node* InsertFirst(const ElementType& element)
    {
        Node* node = NewNode(element);

        if (Head != nullptr) {
            node->InsertBeforeNode(Head);
            Head = node;
        }
        else {
            Head = node;
        }

        if (Tail == nullptr) {
            Tail = Head;
        }
        return node;
    }

    /**
     * Inserts a value after a particular node.
     */
    Node* InsertAfterNode(const ElementType& element, Node* other)
    {
        // If there is no tail or the node to insert after is the tail, insert last
        if (Tail == nullptr || other == Tail) {
            return InsertLast(element);
        }

        Node* node = NewNode(element);
        node->InsertAfterNode(other);

        return node;
    }

    /**
     * Inserts a value before a particular node.
     */
    Node* InsertBeforeNode(const ElementType& element, Node* other)
    {
        // If there is no head or the node to insert before is the head, insert first
        if (Head == nullptr || other == Head) {
            return InsertFirst(element);
        }

        Node* node = NewNode(element);
        node->InsertBeforeNode(other);

        return node;
    }


    ~FxLinkedList()
    {
        // Since the linked list is allocated by the NodeBuffers, they are destroyed automatically at
        // the time of this destructor call.
    }

public:
    Node* Head = nullptr;
    Node* Tail = nullptr;

private:
    FxPagedArray<Node> mNodePool;
    FxPagedArray<Node*> mFreedNodes;
};
