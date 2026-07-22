#include <Lang/Compiler/AstIterator.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

namespace Hyperion {

AstIterator::AstIterator()
    : m_position(0)
{
}

AstIterator::AstIterator(const AstIterator& other)
    : m_position(other.m_position),
      m_list(other.m_list)
{
}

void AstIterator::Prepend(AstIterator&& other, bool resetPosition)
{
    if (resetPosition)
    {
        m_position = 0;
    }
    else
    {
        m_position += other.m_list.Size();
    }

    Array<Handle<AstStatement>> newList = std::move(other.m_list);
    newList.Concat(m_list);
    m_list = std::move(newList);

    other.m_position = 0;
}

void AstIterator::Append(AstIterator&& other)
{
    for (auto& item : other.m_list)
    {
        m_list.PushBack(std::move(item));
    }

    other.m_list.Clear();
    other.m_position = 0;
}

} // namespace Hyperion
