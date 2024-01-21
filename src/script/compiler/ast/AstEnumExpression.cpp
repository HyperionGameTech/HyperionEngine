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
#include <core/lib/String.hpp>

#include <script/Hasher.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstEnumExpression::AstEnumExpression(
    const String &name,
    const Array<EnumEntry> &entries,
    const RC<AstPrototypeSpecification> &underlying_type,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_name(name),
    m_entries(entries),
    m_underlying_type(underlying_type)
{
}

void AstEnumExpression::Visit(AstVisitor *visitor, Module *mod)
{
    Int64 enum_counter = 0;

    if (m_underlying_type == nullptr) {
        m_underlying_type.Reset(new AstPrototypeSpecification(
            RC<AstVariable>(new AstVariable(
                BuiltinTypes::INT->GetName(),
                m_location
            )),
            m_location
        ));
    }

    m_underlying_type->Visit(visitor, mod);

    Array<RC<AstVariableDeclaration>> enum_members;
    enum_members.Reserve(m_entries.Size());

    for (auto &entry : m_entries) {
        bool assignment_ok = false;

        if (entry.assignment != nullptr) {
            if (const auto *deep_value = entry.assignment->GetDeepValueOf()) {
                if (deep_value->IsLiteral()) {
                    if (const auto *as_constant = dynamic_cast<const AstConstant *>(deep_value)) {
                        enum_counter = as_constant->IntValue(); // for next item to use the incremented value from
                        assignment_ok = true;
                    }
                }
            }
        } else {
            entry.assignment.Reset(new AstInteger(
                enum_counter,
                entry.location
            ));
    
            assignment_ok = true;
        }

        if (assignment_ok) {
            enum_members.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
                entry.name,
                CloneAstNode(m_underlying_type),
                entry.assignment,
                IdentifierFlags::FLAG_CONST,
                entry.location
            )));
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_enum_assignment_not_constant,
                entry.location,
                entry.name
            ));
        }

        ++enum_counter;
    }

    SymbolTypePtr_t underlying_type = BuiltinTypes::INT;

    if (auto held_type = m_underlying_type->GetHeldType()) {
        underlying_type = held_type->GetUnaliased();
    }

    m_expr.Reset(new AstTypeExpression(
        m_name,
        nullptr,
        {},
        {},
        enum_members,
        underlying_type,
        false,
        m_location
    ));

    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstEnumExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstEnumExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
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
    AssertThrow(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstEnumExpression::GetExprType() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetExprType();
}

const AstExpression *AstEnumExpression::GetValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetValueOf();
}

const AstExpression *AstEnumExpression::GetDeepValueOf() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetDeepValueOf();
}

const String &AstEnumExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler