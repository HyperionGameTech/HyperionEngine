#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/Types.hpp>

#include <memory>
#include <vector>

namespace Hyperion {

class AstIterator
{
public:
    AstIterator();
    AstIterator(const AstIterator& other);

    void Prepend(AstIterator&& other, bool resetPosition = false);
    void Append(AstIterator&& other);

    HYP_FORCE_INLINE void Push(const Handle<AstStatement>& statement)
    {
        m_list.PushBack(statement);
    }

    HYP_FORCE_INLINE void Pop()
    {
        m_list.PopBack();
    }

    HYP_FORCE_INLINE size_t GetPosition() const
    {
        return m_position;
    }

    HYP_FORCE_INLINE void ResetPosition()
    {
        m_position = 0;
    }

    HYP_FORCE_INLINE void SetPosition(size_t position)
    {
        m_position = position;
    }

    HYP_FORCE_INLINE size_t GetSize() const
    {
        return m_list.Size();
    }

    HYP_FORCE_INLINE AstStatement* Peek() const
    {
        if (m_position >= m_list.Size())
        {
            return nullptr;
        }

        return m_list[m_position];
    }

    HYP_FORCE_INLINE AstStatement* Next()
    {
        if (m_position >= m_list.Size())
        {
            return nullptr;
        }

        return m_list[m_position++];
    }

    HYP_FORCE_INLINE bool HasNext() const
    {
        return m_position < m_list.Size();
    }

    HYP_FORCE_INLINE const SourceLocation& GetLocation() const
    {
        return m_list[m_position]->m_location;
    }

private:
    size_t m_position;
    Array<Handle<AstStatement>> m_list;
};

} // namespace Hyperion
