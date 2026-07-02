#include <Lang/Compiler/Ast/AstUnsignedInteger.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstUndefined.hpp>
#include <Lang/Compiler/AstVisitor.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <iostream>
#include <limits>
#include <cmath>

#include <AstUnsignedInteger.generated.inl>

namespace Hyperion {

AstUnsignedInteger::AstUnsignedInteger(uint64 value, ConstantBitSize bitSize, const SourceLocation& location)
    : AstConstant(ConstantValue(value, bitSize), location)
{
}

UniquePtr<Buildable> AstUnsignedInteger::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    switch (m_constantValue.bitSize)
    {
    case CBS_8:
        return BytecodeUtil::Make<ConstU32>(rp, uint8(m_constantValue.AsUInt())); // @TODO
    case CBS_16:
        return BytecodeUtil::Make<ConstU32>(rp, uint16(m_constantValue.AsUInt())); // @TODO
    case CBS_32:
        return BytecodeUtil::Make<ConstU32>(rp, uint32(m_constantValue.AsUInt()));
    case CBS_64:
        return BytecodeUtil::Make<ConstU64>(rp, m_constantValue.AsUInt());
    default:
        HYP_UNREACHABLE();
    }
}

SharedPtr<AstStatement> AstUnsignedInteger::Clone() const
{
    return CloneImpl();
}

Tribool AstUnsignedInteger::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_constantValue.AsUInt() != 0);
}

bool AstUnsignedInteger::IsNumber() const
{
    return true;
}

const SymbolType* AstUnsignedInteger::GetExprType() const
{
    switch (m_constantValue.bitSize)
    {
    case CBS_8:
        return BuiltinTypes::s_uint8Type;
    case CBS_16:
        return BuiltinTypes::s_uint16Type;
    case CBS_32:
        return BuiltinTypes::s_uint32Type;
    case CBS_64:
        return BuiltinTypes::s_uint64Type;
    default:
        HYP_UNREACHABLE();
    }
}
} // namespace Hyperion
