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

#include <script/Hasher.hpp>
#include <system/debug.h>

namespace hyperion::compiler {

AstEnumExpression::AstEnumExpression(
    const std::string &name,
    const std::vector<EnumEntry> &entries,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_entries(entries)
{
}

void AstEnumExpression::Visit(AstVisitor *visitor, Module *mod)
{   
    AssertThrow(visitor != nullptr && mod != nullptr);

    mod->m_scopes.Open(Scope(SCOPE_TYPE_TYPE_DEFINITION, 0));

    SymbolTypePtr_t inner_type = BuiltinTypes::INT;

    std::vector<SymbolMember_t> members;

    std::cout << "m_entries.size() " << m_entries.size() << "\n" << std::endl;

    for (size_t i = 0; i < m_entries.size(); i++) {
        EnumEntry &entry = m_entries[i];

        if (entry.assignment == nullptr) {
            entry.assignment = std::shared_ptr<AstInteger>(new AstInteger(
                i, entry.location
            ));
        }

        entry.assignment->Visit(visitor, mod);

        const SymbolTypePtr_t entry_type = entry.assignment->GetExprType();

        SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
            visitor,
            mod,
            inner_type,
            entry.assignment->GetExprType(),
            entry.assignment->GetLocation()
        );

        members.push_back(std::make_tuple(
            entry.name,
            entry_type,
            entry.assignment
        ));
    }

    mod->m_scopes.Open(Scope(SCOPE_TYPE_TYPE_DEFINITION, 0));

    m_symbol_type = SymbolType::Extend(
        m_name,
        BuiltinTypes::ENUM_TYPE,
        members
    );

    m_expr.reset(new AstTypeObject(
        m_symbol_type,
        nullptr,
        m_location
    ));

    m_expr->Visit(visitor, mod);
}

std::unique_ptr<Buildable> AstEnumExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    auto buildable = m_expr->Build(visitor, mod);

    return std::move(buildable);
}

void AstEnumExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstEnumExpression::Clone() const
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
    return true;
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

} // namespace hyperion::compiler