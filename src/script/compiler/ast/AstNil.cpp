#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <core/Types.hpp>

namespace hyperion {

AstNil::AstNil(const SourceLocation& location)
    : AstConstant(CBS_INVALID, location)
{
}

UniquePtr<Buildable> AstNil::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstNull>(rp);
}

RC<AstStatement> AstNil::Clone() const
{
    return CloneImpl();
}

Tribool AstNil::IsTrue() const
{
    return Tribool::False();
}

bool AstNil::IsNumber() const
{
    return false;
}

hyperion::int64 AstNil::IntValue() const
{
    return 0;
}

double AstNil::FloatValue() const
{
    return 0.0;
}

SymbolTypeRef AstNil::GetExprType() const
{
    return BuiltinTypes::s_nullType;
}

RC<AstConstant> AstNil::HandleOperator(Operators opType, const AstConstant* right) const
{
    switch (opType)
    {
    case OP_logical_and:
        // logical operations still work, so that we can do
        // things like testing for null in an if statement.
        return RC<AstFalse>(new AstFalse(m_location));

    case OP_logical_or:
        if (!right->IsNumber())
        {
            // this operator is valid to compare against null
            if (dynamic_cast<const AstNil*>(right) != nullptr)
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
            return nullptr;
        }

        // Return the right operand if it's truthy, otherwise return false
        if (right->IsTrue() == Tribool::True())
        {
            if (dynamic_cast<const AstInteger*>(right))
            {
                return RC<AstInteger>(new AstInteger(
                    right->IntValue(),
                    right->GetBitSize(),
                    m_location));
            }
            else if (dynamic_cast<const AstUnsignedInteger*>(right))
            {
                return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                    right->UnsignedValue(),
                    right->GetBitSize(),
                    m_location));
            }
            else if (dynamic_cast<const AstFloat*>(right))
            {
                return RC<AstFloat>(new AstFloat(
                    right->FloatValue(),
                    right->GetBitSize(),
                    m_location));
            }
        }

        return RC<AstFalse>(new AstFalse(m_location));

    case OP_equals:
        if (dynamic_cast<const AstNil*>(right) != nullptr)
        {
            // only another null value should be equal
            return RC<AstTrue>(new AstTrue(m_location));
        }
        // other values never equal to null
        return RC<AstFalse>(new AstFalse(m_location));

    case OP_logical_not:
        return RC<AstTrue>(new AstTrue(m_location));

    default:
        return nullptr;
    }
}

} // namespace hyperion
