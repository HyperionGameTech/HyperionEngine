#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion::compiler {

AstConstant::AstConstant(const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD)
{
}

void AstConstant::Visit(AstVisitor *visitor, Module *mod)
{
    // do nothing
}

void AstConstant::Optimize(AstVisitor *visitor, Module *mod)
{
    // do nothing
}

bool AstConstant::MayHaveSideEffects() const
{
    // constants do not have side effects
    return false;
}

auint32 AstConstant::UnsignedValue() const
{
    return (auint32)IntValue();
}

} // namespace hyperion::compiler
