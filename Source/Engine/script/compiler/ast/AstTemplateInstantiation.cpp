#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/utilities/Format.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/Unicode.hpp>

namespace Hyperion {

AstTemplateInstantiation::AstTemplateInstantiation(
    const RC<AstExpression>& expr,
    const Array<RC<AstTypeSpecifier>>& genericArgs,
    const RC<AstTypeSpecifier>& functionReturnType,
    const SourceLocation& location)
    : AstTypeSpecifier(expr, location),
      m_functionReturnType(functionReturnType),
      m_genericArgs(genericArgs)
{
}

void AstTemplateInstantiation::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    for (auto& arg : m_genericArgs)
    {
        Assert(arg != nullptr);
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());

        auto argType = arg->GetExprType();
        Assert(argType != nullptr);
    }

    AstTypeSpecifier::Visit(visitor, mod);

    if (!m_symbolType || !m_symbolType->IsGenericInstanceType())
    {
        m_symbolType = BuiltinTypes::s_errorType;

        // not a generic if it doesnt resolve
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_type_not_generic,
            m_location,
            m_symbolType->ToString()));

        return;
    }

    ScopeGuard scopeGuard(mod, SCOPE_TYPE_NORMAL);

    // supplant "SelfType" placeholder type with the actual target type
    SymbolType* newType = SymbolType::Temp();
    newType->Register(visitor->GetCompilationUnit());

    SymbolType* selfTypeAlias = SymbolType::Alias("SelfType", { newType });
    selfTypeAlias->Register(visitor->GetCompilationUnit());
    scopeGuard->identifierTable.AddSymbolType(selfTypeAlias);

    Array<GenericInstanceTypeInfo::Arg> genericParamTypes;
    genericParamTypes.Reserve(m_genericArgs.Size() + (m_functionReturnType ? 1 : 0));

    if (m_functionReturnType)
    {
        m_functionReturnType->Visit(visitor, mod);

        const SymbolType* returnType = m_functionReturnType->GetHeldType();
        Assert(returnType != nullptr);

        genericParamTypes.EmplaceBack("@return", returnType, nullptr, false, false);
    }

    for (SizeType i = 0; i < m_genericArgs.Size(); i++)
    {
        const SymbolType* argType = m_genericArgs[i]->GetHeldType();
        Assert(argType != nullptr);

        genericParamTypes.EmplaceBack(HYP_FORMAT("Arg{}", i), argType, nullptr, false, false);
    }

    SymbolType* genericInstanceType = SemanticAnalyzer::Helpers::SubstituteGenericParameters(
        visitor,
        mod,
        m_symbolType,
        genericParamTypes,
        m_location);

    Assert(genericInstanceType != nullptr);
    genericInstanceType->Register(visitor->GetCompilationUnit());

    const SymbolType* resolvedType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
        visitor,
        mod,
        genericInstanceType,
        m_location);

    Assert(resolvedType != nullptr);
    resolvedType->Register(visitor->GetCompilationUnit());

    newType->Assign(*resolvedType);

    m_symbolType = newType;
}

UniquePtr<Buildable> AstTemplateInstantiation::Build(AstVisitor* visitor, Module* mod)
{
    // no bytecode to generate
    return nullptr;
}

void AstTemplateInstantiation::Optimize(AstVisitor* visitor, Module* mod)
{
}

RC<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
