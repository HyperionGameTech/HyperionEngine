#include <script/compiler/AstIterator.hpp>

namespace hyperion {
namespace compiler {

AstIterator::AstIterator()
    : m_position(0)
{
}

AstIterator::AstIterator(const AstIterator &other)
    : m_position(other.m_position),
      m_list(other.m_list)
{
}

} // namespace compiler
} // namespace hyperion
