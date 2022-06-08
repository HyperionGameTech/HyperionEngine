#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <system/debug.h>

#define HYP_SCRIPT_RUNTIME_TYPEOF 0

namespace hyperion {
namespace compiler {

AstTypeOfExpression::AstTypeOfExpression(
    const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_static_id(0)
{
}

void AstTypeOfExpression::Visit(AstVisitor *visitor, Module *mod)
{
     AssertThrow(m_expr != nullptr);
     m_expr->Visit(visitor, mod);
    
     SymbolTypePtr_t expr_type = m_expr->GetSymbolType();
     AssertThrow(expr_type != nullptr);

#if HYP_SCRIPT_RUNTIME_TYPEOF
     if (expr_type == BuiltinTypes::ANY) {
        // add runtime::typeof call
        m_runtime_typeof_call = visitor->GetCompilationUnit()->GetAstNodeBuilder()
            .Module("runtime")
            .Function("typeof")
            .Call({
                std::shared_ptr<AstArgument>((new AstArgument(
                    m_expr,
                    false,
                    "",
                    SourceLocation::eof
                )))
            });

        AssertThrow(m_runtime_typeof_call != nullptr);
        m_runtime_typeof_call->Visit(visitor, mod);
    }
#endif
}

std::unique_ptr<Buildable> AstTypeOfExpression::Build(AstVisitor *visitor, Module *mod)
{
#if HYP_SCRIPT_RUNTIME_TYPEOF
    if (m_runtime_typeof_call != nullptr) {
        return m_runtime_typeof_call->Build(visitor, mod);
    }
#endif
    AssertThrow(m_expr != nullptr);

    SymbolTypePtr_t expr_type = m_expr->GetSymbolType();
    AssertThrow(expr_type != nullptr);

    // simply add a string representing the type

    // get active register
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto instr_string = BytecodeUtil::Make<BuildableString>();
    instr_string->reg = rp;
    instr_string->value = expr_type->GetName();
    return std::move(instr_string);
}

void AstTypeOfExpression::Optimize(AstVisitor *visitor, Module *mod)
{
#if HYP_SCRIPT_RUNTIME_TYPEOF
    if (m_runtime_typeof_call != nullptr) {
        m_runtime_typeof_call->Optimize(visitor, mod);

        return;
    }
#endif

    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

Pointer<AstStatement> AstTypeOfExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTypeOfExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeOfExpression::MayHaveSideEffects() const
{
#if HYP_SCRIPT_RUNTIME_TYPEOF
    if (m_runtime_typeof_call != nullptr) {
        return m_runtime_typeof_call->MayHaveSideEffects();
    }
#endif

    AssertThrow(m_expr != nullptr);

    return m_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstTypeOfExpression::GetSymbolType() const
{
    AssertThrow(m_expr != nullptr);
    
    return BuiltinTypes::STRING;
}

} // namespace compiler
} // namespace hyperion
