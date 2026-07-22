#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstUnsignedInteger.hpp>
#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstUndefined.hpp>
#include <Lang/Compiler/AstVisitor.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <AstInteger.generated.inl>

namespace Hyperion {

AstInteger::AstInteger(int64 value, ConstantBitSize bitSize, const SourceLocation& location)
    : AstConstant(ConstantValue(value, bitSize), location)
{
}

UniquePtr<Buildable> AstInteger::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    switch (m_constantValue.bitSize)
    {
    case CBS_8:
        return BytecodeUtil::Make<ConstI32>(rp, int8(m_constantValue.AsInt())); // @TODO
    case CBS_16:
        return BytecodeUtil::Make<ConstI32>(rp, int16(m_constantValue.AsInt())); // @TODO
    case CBS_32:
        return BytecodeUtil::Make<ConstI32>(rp, int32(m_constantValue.AsInt()));
    case CBS_64:
        return BytecodeUtil::Make<ConstI64>(rp, m_constantValue.AsInt());
    default:
        HYP_UNREACHABLE();
    }
}

Handle<AstStatement> AstInteger::Clone() const
{
    return CloneImpl();
}

Tribool AstInteger::IsTrue() const
{
    // any non-zero value is considered true
    return Tribool(m_constantValue.AsInt() != 0);
}

bool AstInteger::IsNumber() const
{
    return true;
}

const SymbolType* AstInteger::GetExprType() const
{
    switch (m_constantValue.bitSize)
    {
    case CBS_8:
        return BuiltinTypes::s_int8Type;
    case CBS_16:
        return BuiltinTypes::s_int16Type;
    case CBS_32:
        return BuiltinTypes::s_int32Type;
    case CBS_64:
        return BuiltinTypes::s_int64Type;
    default:
        HYP_UNREACHABLE();
    }
}

} // namespace Hyperion
