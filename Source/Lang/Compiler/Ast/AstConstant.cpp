#include <Lang/Compiler/Ast/AstConstant.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/AstVisitor.hpp>

#include <AstConstant.generated.inl>

namespace Hyperion {

AstConstant::AstConstant(const ConstantValue& constantValue, const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_constantValue(constantValue)
{
}

void AstConstant::Visit(AstVisitor* visitor, Module* mod)
{
    if (mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG, /* thisScopeOnly */ true))
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_cannot_create_reference,
            m_location));
    }
}

void AstConstant::Optimize(AstVisitor* visitor, Module* mod)
{
    // do nothing
}

bool AstConstant::MayHaveSideEffects() const
{
    // constants do not have side effects
    return false;
}

String AstConstant::ToString() const
{
    String str = "<invalid constant>";

    m_constantValue.value.Visit([&](auto&& arg)
        {
            str = HYP_FORMAT("{}", arg);
        });

    return str;
}

} // namespace Hyperion
