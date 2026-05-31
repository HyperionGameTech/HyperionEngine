#include <Lang/compiler/ast/AstString.hpp>
#include <Lang/compiler/ast/AstTrue.hpp>
#include <Lang/compiler/ast/AstFalse.hpp>
#include <Lang/compiler/AstVisitor.hpp>
#include <Lang/compiler/Configuration.hpp>

#include <Lang/compiler/type-system/BuiltinTypes.hpp>

#include <Lang/compiler/emit/BytecodeUtil.hpp>

namespace Hyperion {

AstString::AstString(const String& value, const SourceLocation& location)
    : AstConstant(ConstantValue(INVALID_CONSTANT_NUMBER), location),
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

const SymbolType* AstString::GetExprType() const
{
    return BuiltinTypes::s_stringType;
}

} // namespace Hyperion
