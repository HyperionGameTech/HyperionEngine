#include <script/compiler/ast/AstMixinDeclaration.hpp>
#include <script/compiler/ast/AstMixin.hpp>
#include <script/compiler/AstVisitor.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/debug.h>

#include <iostream>

namespace hyperion {
namespace compiler {

AstMixinDeclaration::AstMixinDeclaration(
    const std::string &name,
    const std::string &mixin_expr,
    const SourceLocation &location)
    : AstDeclaration(name, location),
      m_mixin_expr(mixin_expr),
      m_prevent_shadowing(true)
{
}

void AstMixinDeclaration::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    if (mod->LookUpIdentifier(m_name, true) != nullptr) {
        // a collision was found, add an error
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_redeclared_identifier,
            m_location,
            m_name
        ));
    } else {
        // create a declaration for closest shadowed object
        if (m_prevent_shadowing) {
            if (Identifier *ident = mod->LookUpIdentifier(m_name, false)) {
                if (!(ident->GetFlags() & FLAG_MIXIN)) {
                    m_shadowed_decl.reset(new AstVariableDeclaration(
                        std::string("$__") + m_name,
                        nullptr,
                        std::shared_ptr<AstVariable>(new AstVariable(
                            m_name,
                            m_location
                        )),
                        false,
                        m_location
                    ));

                    m_shadowed_decl->Visit(visitor, mod);
                }
            }
        }

        std::shared_ptr<AstMixin> mixin_expr(new AstMixin(
            m_name,
            m_mixin_expr,
            m_location
        ));

        Scope &scope = mod->m_scopes.Top();
        if ((m_identifier = scope.GetIdentifierTable().AddIdentifier(m_name, FLAG_MIXIN | FLAG_ALIAS))) {
            m_identifier->SetSymbolType(BuiltinTypes::ANY);
            m_identifier->SetCurrentValue(mixin_expr);
        }
    }
}

std::unique_ptr<Buildable> AstMixinDeclaration::Build(AstVisitor *visitor, Module *mod)
{
    if (m_shadowed_decl != nullptr) {
        return m_shadowed_decl->Build(visitor, mod);
    }
    
    return nullptr;
}

void AstMixinDeclaration::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_shadowed_decl != nullptr) {
        m_shadowed_decl->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstMixinDeclaration::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
