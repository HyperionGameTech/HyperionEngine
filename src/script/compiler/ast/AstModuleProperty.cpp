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

#include <core/filesystem/FsUtil.hpp>
#include <core/filesystem/FilePath.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <iostream>

namespace hyperion {

AstModuleProperty::AstModuleProperty(
    const String& fieldName,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_fieldName(fieldName),
      m_exprType(BuiltinTypes::UNDEFINED)
{
}

void AstModuleProperty::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    if (m_fieldName == "name")
    {
        m_exprValue = RC<AstString>(new AstString(
            mod->GetName(),
            m_location));
    }
    else if (m_fieldName == "path")
    {
        m_exprValue = RC<AstString>(new AstString(
            mod->GetLocation().GetFileName().Data(),
            m_location));
    }
    else if (m_fieldName == "directory")
    {
        m_exprValue = RC<AstString>(new AstString(
            FilePath(mod->GetLocation().GetFileName()).BasePath().Data(),
            m_location));
    }
    else if (m_fieldName == "basename")
    {
        m_exprValue = RC<AstString>(new AstString(
            FilePath(mod->GetLocation().GetFileName()).Basename().Data(),
            m_location));
    }

    if (m_exprValue != nullptr)
    {
        m_exprValue->Visit(visitor, mod);

        Assert(m_exprValue->GetExprType() != nullptr);
        m_exprType = m_exprValue->GetExprType();
    }
    else
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_data_member,
            m_location,
            m_fieldName,
            BuiltinTypes::MODULE_INFO->ToString()));
    }
}

UniquePtr<Buildable> AstModuleProperty::Build(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    Assert(m_exprValue != nullptr);
    return m_exprValue->Build(visitor, mod);
}

void AstModuleProperty::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_exprValue != nullptr);
    m_exprValue->Optimize(visitor, mod);
}

RC<AstStatement> AstModuleProperty::Clone() const
{
    return CloneImpl();
}

Tribool AstModuleProperty::IsTrue() const
{
    Assert(m_exprValue != nullptr);
    return m_exprValue->IsTrue();
}

bool AstModuleProperty::MayHaveSideEffects() const
{
    if (m_exprValue != nullptr)
    {
        return m_exprValue->MayHaveSideEffects();
    }
    else
    {
        return false;
    }
}

SymbolTypeRef AstModuleProperty::GetExprType() const
{
    Assert(m_exprType != nullptr);
    return m_exprType;
}

} // namespace hyperion
