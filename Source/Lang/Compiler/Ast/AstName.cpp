#include <Lang/Compiler/Ast/AstName.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Configuration.hpp>
#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/Instruction.hpp>

#include <Core/HashCode.hpp>

#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstString.hpp>
#include <Lang/Compiler/Ast/AstArgument.hpp>
#include <Lang/Compiler/Ast/AstArgumentList.hpp>

namespace Hyperion {

AstName::AstName(const String& value, const SourceLocation& location)
    : AstConstant(ConstantValue(INVALID_CONSTANT_NUMBER), location),
      m_value(value)
{
}

void AstName::Visit(AstVisitor* visitor, Module* mod)
{
    m_callExpr.Reset(new AstCallExpression(
        RC<AstMember>(new AstMember(
            "FromString",
            RC<AstTypeRef>(new AstTypeRef(BuiltinTypes::s_nameType, m_location)),
            m_location)),
        { RC<AstArgument>(new AstArgument(
            RC<AstString>(new AstString(m_value, m_location)),
            false, /* isSplat */
            false, /* isNamed */
            false, /* isPassByRef */
            false, /* isPassConst */
            "",    /* name */
            m_location)) },
        false, /* insertSelf */
        m_location));

    m_callExpr->Visit(visitor, mod);
}

UniquePtr<Buildable> AstName::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_callExpr != nullptr);

    return m_callExpr->Build(visitor, mod);
}

RC<AstStatement> AstName::Clone() const
{
    return CloneImpl();
}

Tribool AstName::IsTrue() const
{
    // names evaluate to true
    return Tribool::True();
}

bool AstName::IsNumber() const
{
    return false;
}

const SymbolType* AstName::GetExprType() const
{
    return BuiltinTypes::s_nameType;
}

} // namespace Hyperion
