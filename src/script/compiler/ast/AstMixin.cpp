#include <script/compiler/ast/AstMixin.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstAliasDeclaration.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

namespace hyperion::compiler {

AstMixin::AstMixin(const std::string &name,
    const std::string &mixin_expr,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_mixin_expr(mixin_expr)
{
}

void AstMixin::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AstIterator mixin_ast_iterator;
    std::string mixin_filepath = mod->GetLocation().GetFileName() + " <mixin>";

    // load stream into file buffer
    SourceFile mixin_source_file(mixin_filepath, m_mixin_expr.length() + 1);
    mixin_source_file.operator>>(m_mixin_expr);

    // use the lexer and parser on this file buffer
    TokenStream mixin_token_stream(TokenStreamInfo {
        mixin_filepath
    });

    Lexer mixin_lexer(SourceStream(&mixin_source_file), &mixin_token_stream, visitor->GetCompilationUnit());
    mixin_lexer.Analyze();

    Parser mixin_parser(&mixin_ast_iterator, &mixin_token_stream, visitor->GetCompilationUnit());
    mixin_parser.Parse(false);

    // open the new scope
    mod->m_scopes.Open(Scope());

    // create a temporary alias from this mixin's name to the prefixed version,
    // created by AstMixinDeclaration.
    // this is used to prevent circular/recursive mixins which will cause a
    // stack overflow.
    m_statements.push_back(std::shared_ptr<AstAliasDeclaration>(new AstAliasDeclaration(
        m_name,
        std::shared_ptr<AstVariable>(new AstVariable(
            std::string("$__") + m_name,
            m_location
        )),
        m_location
    )));

    while (mixin_ast_iterator.HasNext()) {
        auto stmt = mixin_ast_iterator.Next();
        AssertThrow(stmt != nullptr);
        m_statements.push_back(stmt);
    }

    // if last item is not an expression (or no expressions at all), add Null object (Void?)
    if (m_statements.empty() || dynamic_cast<AstExpression*>(m_statements.back().get()) == nullptr) {
        m_statements.push_back(std::shared_ptr<AstNil>(new AstNil(
            m_location
        )));
    }

    // temporarily declare another mixin assigning name to '$__name'.
    // this is created in the mixin declaration stage if there are any shadowed vars,
    // so that self-referencing this mixin within itself will route back to the original,
    // avoiding circular references (causes the compiler to get caught and eventually stack overflow)
    for (auto &stmt : m_statements) {
        AssertThrow(stmt != nullptr);
        // visit statement in this module
        stmt->Visit(visitor, mod);
    }

    // go down to previous scope
    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstMixin::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    for (auto &stmt : m_statements) {
        AssertThrow(stmt != nullptr);
        chunk->Append(stmt->Build(visitor, mod));
    }

    return std::move(chunk);
}

void AstMixin::Optimize(AstVisitor *visitor, Module *mod)
{
    for (auto &stmt : m_statements) {
        AssertThrow(stmt != nullptr);
        stmt->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstMixin::Clone() const
{
    return CloneImpl();
}

Tribool AstMixin::IsTrue() const
{
    AssertThrow(!m_statements.empty());

    const AstExpression *last_as_expr =
        dynamic_cast<AstExpression*>(m_statements.back().get());

    AssertThrow(last_as_expr != nullptr);

    return last_as_expr->IsTrue();
}

bool AstMixin::MayHaveSideEffects() const
{
    for (auto &stmt : m_statements) {
        AssertThrow(stmt != nullptr);

        if (AstExpression *expr = dynamic_cast<AstExpression*>(stmt.get())) {
            if (expr->MayHaveSideEffects()) {
                return true;
            }
        }
    }

    return false;
}

SymbolTypePtr_t AstMixin::GetExprType() const
{
    AssertThrow(!m_statements.empty());

    const AstExpression *last_as_expr =
        dynamic_cast<AstExpression*>(m_statements.back().get());

    AssertThrow(last_as_expr != nullptr);
    return last_as_expr->GetExprType();
}

} // namespace hyperion::compiler
