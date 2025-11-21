#include <script/compiler/ast/AstName.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/Instruction.hpp>

#include <core/HashCode.hpp>

#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>

namespace hyperion {

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

} // namespace hyperion
