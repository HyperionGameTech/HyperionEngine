#include <script/compiler/AstIterator.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion::compiler {

AstIterator::AstIterator()
    : m_position(0)
{
}

AstIterator::AstIterator(const AstIterator &other)
    : m_position(other.m_position),
      m_list(other.m_list)
{
}

void AstIterator::Prepend(AstIterator &&other, bool reset_position)
{
    if (reset_position) {
        m_position = 0;
    } else {
        m_position += other.m_list.Size();
    }

    Array<RC<AstStatement>> new_list = std::move(other.m_list);
    new_list.Concat(std::move(m_list));
    m_list = std::move(new_list);

    other.m_position = 0;
}

void AstIterator::Append(AstIterator &&other)
{
    for (auto &item : other.m_list) {
        m_list.PushBack(std::move(item));
    }

    other.m_list.Clear();
    other.m_position = 0;
}

} // namespace hyperion::compiler
