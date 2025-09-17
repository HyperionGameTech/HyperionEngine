#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/utilities/Format.hpp>

#include <limits>
#include <cmath>

namespace hyperion {

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

RC<AstStatement> AstFloat::Clone() const
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
} // namespace hyperion
