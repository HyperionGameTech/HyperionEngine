#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/AstVisitor.hpp>

namespace hyperion {

AstConstant::AstConstant(const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_bitSize(CBS_INVALID)
{
}

AstConstant::AstConstant(ConstantBitSize bitSize, const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_bitSize(bitSize)
{
}

void AstConstant::Visit(AstVisitor* visitor, Module* mod)
{
    if (mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG, /* thisScopeOnly */ false))
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

Optional<uint64> AstConstant::UnsignedValue() const
{
    Optional<int64> intValue = IntValue();

    if (intValue.HasValue() && *intValue >= 0)
    {
        return uint64(*intValue);
    }

    return AstExpression::UnsignedValue();
}

Optional<double> AstConstant::FloatValue() const
{
    Optional<int64> intValue = IntValue();

    if (intValue.HasValue() && *intValue <= static_cast<int64>(DBL_MAX) && *intValue >= static_cast<int64>(-DBL_MAX))
    {
        return double(*intValue);
    }

    return AstExpression::FloatValue();
}

} // namespace hyperion
