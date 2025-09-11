#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/Types.hpp>

namespace hyperion {

AstFalse::AstFalse(const SourceLocation& location)
    : AstConstant(CBS_8, location)
{
}

UniquePtr<Buildable> AstFalse::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    const uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, false);
}

RC<AstStatement> AstFalse::Clone() const
{
    return CloneImpl();
}

Tribool AstFalse::IsTrue() const
{
    return Tribool::False();
}

bool AstFalse::IsNumber() const
{
    return false;
}

hyperion::int64 AstFalse::IntValue() const
{
    return 0;
}

double AstFalse::FloatValue() const
{
    return 0.0;
}

SymbolTypeRef AstFalse::GetExprType() const
{
    return BuiltinTypes::s_boolType;
}

RC<AstConstant> AstFalse::HandleOperator(Operators opType, const AstConstant* right) const
{
    switch (opType)
    {
    case OP_logical_and:
        return RC<AstFalse>(new AstFalse(m_location));

    case OP_logical_or:
        switch (right->IsTrue())
        {
        case TRI_TRUE:
            return RC<AstTrue>(new AstTrue(m_location));
        case TRI_FALSE:
            return RC<AstFalse>(new AstFalse(m_location));
        case TRI_INDETERMINATE:
            return nullptr;
        }

    case OP_equals:
        if (dynamic_cast<const AstFalse*>(right) != nullptr)
        {
            return RC<AstTrue>(new AstTrue(m_location));
        }
        return RC<AstFalse>(new AstFalse(m_location));

    case OP_logical_not:
        return RC<AstTrue>(new AstTrue(m_location));

    default:
        return nullptr;
    }
}

} // namespace hyperion
