#include <script/compiler/ast/AstModuleProperty.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <util/fs/FsUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <script/Hasher.hpp>

#include <iostream>

namespace hyperion::compiler {

AstModuleProperty::AstModuleProperty(
    const std::string &field_name,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_field_name(field_name),
      m_expr_type(BuiltinTypes::UNDEFINED)
{
}

void AstModuleProperty::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    if (m_field_name == "name") {
        m_expr_value = std::shared_ptr<AstString>(new AstString(
            mod->GetName(),
            m_location
        ));
    } else if (m_field_name == "path") {
        m_expr_value = std::shared_ptr<AstString>(new AstString(
            mod->GetLocation().GetFileName(),
            m_location
        ));
    } else if (m_field_name == "directory") {
        m_expr_value = std::shared_ptr<AstString>(new AstString(
            FilePath(mod->GetLocation().GetFileName().c_str()).BasePath().Data(),
            m_location
        ));
    } else if (m_field_name == "basename") {
        m_expr_value = std::shared_ptr<AstString>(new AstString(
            FilePath(mod->GetLocation().GetFileName().c_str()).Basename().Data(),
            m_location
        ));
    }
    
    if (m_expr_value != nullptr) {
      m_expr_value->Visit(visitor, mod);

      AssertThrow(m_expr_value->GetExprType() != nullptr);
      m_expr_type = m_expr_value->GetExprType();
    } else {
      visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
          LEVEL_ERROR,
          Msg_not_a_data_member,
          m_location,
          m_field_name,
          BuiltinTypes::MODULE_INFO->GetName()
      ));
    }
}

std::unique_ptr<Buildable> AstModuleProperty::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_expr_value != nullptr);
    return m_expr_value->Build(visitor, mod);
}

void AstModuleProperty::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_expr_value != nullptr);
    m_expr_value->Optimize(visitor, mod);
}

Pointer<AstStatement> AstModuleProperty::Clone() const
{
    return CloneImpl();
}

Tribool AstModuleProperty::IsTrue() const
{
    AssertThrow(m_expr_value != nullptr);
    return m_expr_value->IsTrue();
}

bool AstModuleProperty::MayHaveSideEffects() const
{
    if (m_expr_value != nullptr) {
        return m_expr_value->MayHaveSideEffects();
    } else {
        return false;
    }
}

SymbolTypePtr_t AstModuleProperty::GetExprType() const
{
    AssertThrow(m_expr_type != nullptr);
    return m_expr_type;
}

} // namespace hyperion::compiler
