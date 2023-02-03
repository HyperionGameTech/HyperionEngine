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
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstEnumExpression::AstEnumExpression(
    const std::string &name,
    const std::vector<EnumEntry> &entries,
    const std::shared_ptr<AstPrototypeSpecification> &underlying_type,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_name(name),
    m_entries(entries),
    m_underlying_type(underlying_type)
{
}

void AstEnumExpression::Visit(AstVisitor *visitor, Module *mod)
{
    int64_t enum_counter = 0;

    if (m_underlying_type == nullptr) {
        m_underlying_type.reset(new AstPrototypeSpecification(
            std::shared_ptr<AstVariable>(new AstVariable(
                BuiltinTypes::INT->GetName(), // m_name
                m_location
            )),
            m_location
        ));
    }

    m_underlying_type->Visit(visitor, mod);

    std::vector<std::shared_ptr<AstVariableDeclaration>> enum_members;
    enum_members.reserve(m_entries.size());

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
            entry.assignment.reset(new AstInteger(
                enum_counter,
                entry.location
            ));
    
            assignment_ok = true;
        }

        if (assignment_ok) {
            enum_members.emplace_back(new AstVariableDeclaration(
                entry.name,
                CloneAstNode(m_underlying_type),
                entry.assignment,
                {},
                IdentifierFlags::FLAG_CONST,
                entry.location
            ));
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

    m_expr.reset(new AstTypeExpression(
        m_name,
        nullptr,
        {},
        {},
        enum_members,
        m_underlying_type->GetExprType() ? m_underlying_type->GetExprType() : BuiltinTypes::INT,
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

const std::string &AstEnumExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler