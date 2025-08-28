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

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstTemplateInstantiation::AstTemplateInstantiation(
    const RC<AstExpression>& expr,
    const Array<RC<AstTypeSpecifier>>& genericArgs,
    const SourceLocation& location)
    : AstTypeSpecifier(expr, location),
      m_genericArgs(genericArgs)
{
}

void AstTemplateInstantiation::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    ScopeGuard scope(mod, SCOPE_TYPE_GENERIC_INSTANTIATION, 0);

    for (auto& arg : m_genericArgs)
    {
        Assert(arg != nullptr);
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());

        auto argType = arg->GetExprType();
        Assert(argType != nullptr);
    }

    AstTypeSpecifier::Visit(visitor, mod);

    if (!m_symbolType || !m_symbolType->IsGenericBaseType())
    {
        m_symbolType = BuiltinTypes::UNDEFINED;

        // not a generic if it doesnt resolve
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_type_not_generic,
            m_location,
            m_symbolType->ToString()));

        return;
    }

    Array<GenericInstanceTypeInfo::Arg> genericParamTypes;
    genericParamTypes.Reserve(m_genericArgs.Size());

    for (SizeType i = 0; i < m_genericArgs.Size(); i++)
    {
        SymbolTypeRef argType = m_genericArgs[i]->GetHeldType();
        Assert(argType != nullptr);

        genericParamTypes.PushBack({ HYP_FORMAT("@arg{}", i), argType });
    }

    SymbolTypeRef genericInstanceType = SymbolType::GenericInstance(
        m_symbolType,
        GenericInstanceTypeInfo { genericParamTypes });

    if (!genericInstanceType)
    {
        m_symbolType = BuiltinTypes::UNDEFINED;

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));

        return;
    }

    m_symbolType = SymbolType::SubstituteGenericParams(
        m_symbolType,
        BuiltinTypes::PLACEHOLDER,
        genericInstanceType);

    if (!m_symbolType)
    {
        m_symbolType = BuiltinTypes::UNDEFINED;

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_internal_error,
            m_location));

        return;
    }
}

UniquePtr<Buildable> AstTemplateInstantiation::Build(AstVisitor* visitor, Module* mod)
{

    return nullptr;

    // UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // // Build the arguments
    // chunk->Append(Compiler::BuildArgumentsStart(
    //     visitor,
    //     mod,
    //     m_substitutedArgs
    // ));

    // const int stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // // Build the original expression.
    // // Usage of arguments in the expression will be replaced with the substituted arguments.
    // chunk->Append(m_expr->Build(visitor, mod));

    // const int stackSizeNow = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    // Assert(
    //     stackSizeNow == stackSizeBefore,
    //     "Stack size mismatch detected! Internal record of stack does not match. (%d != %d)",
    //     stackSizeNow,
    //     stackSizeBefore
    // );

    // chunk->Append(Compiler::BuildArgumentsEnd(
    //     visitor,
    //     mod,
    //     m_substitutedArgs.Size()
    // ));

    // return chunk;
}

void AstTemplateInstantiation::Optimize(AstVisitor* visitor, Module* mod)
{
    // Assert(visitor != nullptr);
    // Assert(mod != nullptr);

    // Assert(m_block != nullptr);
    // m_block->Optimize(visitor, mod);
}

RC<AstStatement> AstTemplateInstantiation::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
