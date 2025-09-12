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

Optional<ConstantUInt> AstConstant::UnsignedValue() const
{
    Optional<ConstantInt> constantValue = IntValue();

    if (constantValue.HasValue() && constantValue->value >= 0)
    {
        return ConstantUInt(uint64(constantValue->value), constantValue->bitSize);
    }

    return AstExpression::UnsignedValue();
}

Optional<ConstantFloat> AstConstant::FloatValue() const
{
    Optional<ConstantInt> constantValue = IntValue();

    if (constantValue.HasValue())
    {
        return ConstantFloat(float64(constantValue->value), constantValue->bitSize);
    }

    return AstExpression::FloatValue();
}

} // namespace hyperion
