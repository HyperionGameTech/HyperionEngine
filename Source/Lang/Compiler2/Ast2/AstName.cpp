#include <Lang/compiler/ast/AstName.hpp>
#include <Lang/compiler/AstVisitor.hpp>
#include <Lang/compiler/Configuration.hpp>
#include <Lang/compiler/type-system/BuiltinTypes.hpp>
#include <Lang/compiler/emit/BytecodeUtil.hpp>
#include <Lang/compiler/emit/Instruction.hpp>

#include <Core/HashCode.hpp>

#include <Lang/compiler/ast/AstCallExpression.hpp>
#include <Lang/compiler/ast/AstMember.hpp>
#include <Lang/compiler/ast/AstIdentifier.hpp>
#include <Lang/compiler/ast/AstTypeRef.hpp>
#include <Lang/compiler/ast/AstString.hpp>
#include <Lang/compiler/ast/AstArgument.hpp>
#include <Lang/compiler/ast/AstArgumentList.hpp>

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
