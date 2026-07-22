#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/AstVisitor.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Utilities/Format.hpp>

#include <limits>
#include <cmath>

#include <AstFloat.generated.inl>

namespace Hyperion {

AstFloat::AstFloat(double value, ConstantBitSize bitSize, const SourceLocation& location)
    : AstConstant(ConstantValue(value, bitSize), location)
{
}

UniquePtr<Buildable> AstFloat::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    switch (m_constantValue.bitSize)
    {
    case CBS_32:
        return BytecodeUtil::Make<ConstF32>(rp, float32(m_constantValue.AsFloat()));
    case CBS_64:
        return BytecodeUtil::Make<ConstF64>(rp, m_constantValue.AsFloat());
    default:
        HYP_UNREACHABLE();
    }
}

Handle<AstStatement> AstFloat::Clone() const
{
    return CloneImpl();
}

Tribool AstFloat::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_constantValue.AsFloat() != 0.0f);
}

bool AstFloat::IsNumber() const
{
    return true;
}

const SymbolType* AstFloat::GetExprType() const
{
    switch (m_constantValue.bitSize)
    {
    case CBS_32:
        return BuiltinTypes::s_floatType;
    case CBS_64:
        return BuiltinTypes::s_doubleType;
    default:
        HYP_UNREACHABLE();
    }
}
} // namespace Hyperion
