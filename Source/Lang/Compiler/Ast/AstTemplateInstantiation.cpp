#include <Lang/Compiler/Ast/AstTemplateInstantiation.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Unicode.hpp>

#include <AstTemplateInstantiation.generated.inl>

namespace Hyperion {

AstTemplateInstantiation::AstTemplateInstantiation(
    const SharedPtr<AstExpression>& expr,
    const Array<SharedPtr<AstTypeSpecifier>>& genericArgs,
    const SharedPtr<AstTypeSpecifier>& functionReturnType,
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

    for (size_t i = 0; i < m_genericArgs.Size(); i++)
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

SharedPtr<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
