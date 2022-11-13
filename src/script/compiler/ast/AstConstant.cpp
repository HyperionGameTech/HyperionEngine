#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/AstVisitor.hpp>

namespace hyperion::compiler {

AstConstant::AstConstant(const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD)
{
}

void AstConstant::Visit(AstVisitor *visitor, Module *mod)
{
    if (mod->IsInScopeOfType(ScopeType::SCOPE_TYPE_NORMAL, ScopeFunctionFlags::REF_VARIABLE_FLAG)) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_cannot_create_reference,
            m_location
        ));
    }
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

UInt32 AstConstant::UnsignedValue() const
{
    return (UInt32)IntValue();
}

} // namespace hyperion::compiler
