#include <script/compiler/ast/AstEnumExpression.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <core/containers/String.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstEnumExpression::AstEnumExpression(
    const String& name,
    const Array<EnumEntry>& entries,
    const RC<AstPrototypeSpecification>& underlyingType,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_entries(entries),
      m_underlyingType(underlyingType)
{
}

void AstEnumExpression::Visit(AstVisitor* visitor, Module* mod)
{
    int64 enumCounter = 0;

    if (m_underlyingType == nullptr)
    {
        m_underlyingType.Reset(new AstPrototypeSpecification(
            RC<AstVariable>(new AstVariable(
                BuiltinTypes::INT->GetName(),
                m_location)),
            m_location));
    }

    m_underlyingType->Visit(visitor, mod);

    Array<RC<AstVariableDeclaration>> enumMembers;
    enumMembers.Reserve(m_entries.Size());

    for (auto& entry : m_entries)
    {
        bool assignmentOk = false;

        if (entry.assignment != nullptr)
        {
            if (const auto* deepValue = entry.assignment->GetDeepValueOf())
            {
                if (deepValue->IsLiteral())
                {
                    if (const auto* asConstant = dynamic_cast<const AstConstant*>(deepValue))
                    {
                        enumCounter = asConstant->IntValue(); // for next item to use the incremented value from
                        assignmentOk = true;
                    }
                }
            }
        }
        else
        {
            entry.assignment.Reset(new AstInteger(
                enumCounter,
                entry.location));

            assignmentOk = true;
        }

        if (assignmentOk)
        {
            enumMembers.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
                entry.name,
                CloneAstNode(m_underlyingType),
                entry.assignment,
                IdentifierFlags::FLAG_CONST,
                entry.location)));
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_enum_assignment_not_constant,
                entry.location,
                entry.name));
        }

        ++enumCounter;
    }

    SymbolTypeRef underlyingType = BuiltinTypes::INT;

    if (auto heldType = m_underlyingType->GetHeldType())
    {
        underlyingType = heldType->GetUnaliased();
    }

    m_expr.Reset(new AstTypeExpression(
        m_name,
        nullptr,
        {},
        {},
        enumMembers,
        underlyingType,
        false,
        m_location));

    m_expr->Visit(visitor, mod);
}

UniquePtr<Buildable> AstEnumExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstEnumExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstEnumExpression::Clone() const
{
    return CloneImpl();
}

bool AstEnumExpression::IsLiteral() const
{
    return false;
}

Tribool AstEnumExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstEnumExpression::MayHaveSideEffects() const
{
    Assert(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypeRef AstEnumExpression::GetExprType() const
{
    Assert(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression* AstEnumExpression::GetValueOf() const
{
    Assert(m_expr != nullptr);
    return m_expr->GetValueOf();
}

const AstExpression* AstEnumExpression::GetDeepValueOf() const
{
    Assert(m_expr != nullptr);
    return m_expr->GetDeepValueOf();
}

const String& AstEnumExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler