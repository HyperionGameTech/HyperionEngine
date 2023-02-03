#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstArgument::AstArgument(
    const RC<AstExpression> &expr,
    bool is_splat,
    bool is_named,
    const std::string &name,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_expr(expr),
    m_is_splat(is_splat),
    m_is_named(is_named),
    m_is_pass_by_ref(false),
    m_is_pass_const(false),
    m_name(name)
{
}

void AstArgument::Visit(AstVisitor *visitor, Module *mod)
{
    if (m_is_splat) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_implemented,
            m_location,
            "splat-expressions"
        ));
    }

    AssertThrow(m_expr != nullptr);

    bool pass_by_ref_scope = false;
    bool pass_const_scope = false;

    if (IsPassConst()) {
        mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, CONST_VARIABLE_FLAG));

        pass_const_scope = true;
    }

    if (IsPassByRef()) {
        if (m_expr->GetAccessOptions() & AccessMode::ACCESS_MODE_STORE) {
            mod->m_scopes.Open(Scope(SCOPE_TYPE_NORMAL, REF_VARIABLE_FLAG));

            pass_by_ref_scope = true;
        } else {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_cannot_create_reference,
                m_location
            ));
        }
    }

    m_expr->Visit(visitor, mod);

    if (pass_by_ref_scope) {
        mod->m_scopes.Close();
    }

    if (pass_const_scope) {
        mod->m_scopes.Close();
    }
}

std::unique_ptr<Buildable> AstArgument::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(m_expr->Build(visitor, mod));

    // if (IsPassByRef()) {
    //     // TODO! Ensure in the analyzing stage that we are passing in something we can take the reference of.
    //     UInt8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    //     chunk->Append(BytecodeUtil::Make<LoadRef>(rp, rp));
    // }

    return std::move(chunk);
}

void AstArgument::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstArgument::Clone() const
{
    return CloneImpl();
}

bool AstArgument::IsLiteral() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->IsLiteral();
}

Tribool AstArgument::IsTrue() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->IsTrue();
}

bool AstArgument::MayHaveSideEffects() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

SymbolTypePtr_t AstArgument::GetExprType() const
{
    AssertThrow(m_expr != nullptr);
    return m_expr->GetExprType();
}

const std::string &AstArgument::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
