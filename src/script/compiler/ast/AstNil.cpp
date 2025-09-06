#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstInteger.hpp>
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
    : AstConstant(location)
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

hyperion::int32 AstNil::IntValue() const
{
    return 0;
}

float AstNil::FloatValue() const
{
    return 0.0f;
}

SymbolTypeRef AstNil::GetExprType() const
{
    return BuiltinTypes::g_nullType;
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

        return RC<AstInteger>(new AstInteger(
            right->IntValue(),
            m_location));

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
