#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeUtil.hpp>

namespace hyperion {

AstString::AstString(const String& value, const SourceLocation& location)
    : AstConstant(CBS_INVALID, location),
      m_value(value)
{
}

UniquePtr<Buildable> AstString::Build(AstVisitor* visitor, Module* mod)
{
    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instrString = BytecodeUtil::Make<ConstString>();
    instrString->reg = rp;
    instrString->value = m_value;

    return instrString;
}

RC<AstStatement> AstString::Clone() const
{
    return CloneImpl();
}

Tribool AstString::IsTrue() const
{
    // strings evaluate to true
    return Tribool::True();
}

bool AstString::IsNumber() const
{
    return false;
}

hyperion::int64 AstString::IntValue() const
{
    // not valid
    return 0;
}

double AstString::FloatValue() const
{
    // not valid
    return 0.0;
}

SymbolTypeRef AstString::GetExprType() const
{
    return BuiltinTypes::s_stringType;
}

RC<AstConstant> AstString::HandleOperator(Operators opType, const AstConstant* right) const
{
    switch (opType)
    {
    case OP_logical_and:
        // literal strings evaluate to true.
        switch (right->IsTrue())
        {
        case TRI_TRUE:
            return RC<AstTrue>(new AstTrue(m_location));
        case TRI_FALSE:
            return RC<AstFalse>(new AstFalse(m_location));
        case TRI_INDETERMINATE:
            return nullptr;
        }

    case OP_logical_or:
        return RC<AstTrue>(new AstTrue(m_location));

    case OP_equals:
        if (const AstString* rightString = dynamic_cast<const AstString*>(right))
        {
            if (m_value == rightString->GetValue())
            {
                return RC<AstTrue>(new AstTrue(m_location));
            }
            else
            {
                return RC<AstFalse>(new AstFalse(m_location));
            }
        }

        return nullptr;

    default:
        return nullptr;
    }
}

} // namespace hyperion
