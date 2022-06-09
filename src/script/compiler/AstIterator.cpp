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

} // namespace hyperion::compiler
