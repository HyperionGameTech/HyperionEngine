#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Scope.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <core/math/MathUtil.hpp>

#include <core/reflection/HypMethod.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <util/UTF8.hpp>

namespace hyperion {

AstFunctionExpression::AstFunctionExpression(
    const Array<RC<AstParameter>>& parameters,
    const RC<AstTypeSpecifier>& returnTypeSpecification,
    const RC<AstBlock>& block,
    const SourceLocation& location)
    : AstFunctionExpression(
        parameters,
        returnTypeSpecification,
        block,
        /* enableClosure */ true,
        location)
{
}

AstFunctionExpression::AstFunctionExpression(
    const Array<RC<AstParameter>>& parameters,
    const RC<AstTypeSpecifier>& returnTypeSpecification,
    const RC<AstBlock>& block,
    bool enableClosure,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_parameters(parameters),
      m_returnTypeSpecification(returnTypeSpecification),
      m_block(block),
      m_enableClosure(enableClosure),
      m_isClosure(false),
      m_isConstructorDefinition(false),
      m_symbolType(nullptr),
      m_returnType(BuiltinTypes::s_anyType),
      m_staticId(0)
{
}

void AstFunctionExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    m_blockWithParameters = CloneAstNode(m_block);

    m_isConstructorDefinition = GetExpressionFlags() & EXPR_FLAGS_CONSTRUCTOR_DEFINITION;

    int scopeFlags = 0;

    if (m_enableClosure)
    {
        m_isClosure = true;
        scopeFlags |= CLOSURE_FUNCTION_FLAG;

        // closures are objects with a method named '$invoke',
        // so we pass the '$functor' argument when it is called.
        m_closureSelfParam.Reset(new AstParameter(
            "$functor",
            nullptr,
            nullptr,
            false, /* variadic */
            IdentifierFlags::CONSTANT,
            m_location));
    }

    if (m_isConstructorDefinition)
    {
        scopeFlags |= CONSTRUCTOR_DEFINITION_FLAG;
    }

    // // open the new scope for parameters
    mod->scopeTree.Open(SCOPE_TYPE_FUNCTION, scopeFlags);

    if (m_isClosure)
    {
        m_closureSelfParam->Visit(visitor, mod);
    }

    for (SizeType index = 0; index < m_parameters.Size(); index++)
    {
        Assert(m_parameters[index] != nullptr);
        m_parameters[index]->Visit(visitor, mod);
    }

    if (m_returnTypeSpecification != nullptr)
    {
        m_returnTypeSpecification->Visit(visitor, mod);

        if (m_returnTypeSpecification->GetHeldType() != nullptr)
        {
            m_returnType = m_returnTypeSpecification->GetHeldType();
        }
        else
        {
            m_returnType = BuiltinTypes::s_errorType;
        }

        SymbolType* returnTypeAlias = SymbolType::Alias("ReturnType", { m_returnType });
        returnTypeAlias->Register(visitor->GetCompilationUnit());
        mod->scopeTree.Top().identifierTable.AddSymbolType(returnTypeAlias);
    }
    else
    {
        // add placeholder return type for AstReturnStatement nodes to use
        SymbolType* placeholderReturnType = SymbolType::Placeholder("ReturnType");
        placeholderReturnType->Register(visitor->GetCompilationUnit());

        mod->scopeTree.Top().identifierTable.AddSymbolType(placeholderReturnType);
    }

    if (m_blockWithParameters != nullptr)
    {
        if (m_isConstructorDefinition)
        {
            // add implicit 'return self' at the end
            m_blockWithParameters->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
                RC<AstVariable>(new AstVariable("self", m_blockWithParameters->GetLocation())),
                m_blockWithParameters->GetLocation())));
        }

        // visit the function body
        m_blockWithParameters->Visit(visitor, mod);
    }

    // first item will be set to return type
    Array<GenericInstanceTypeInfo::Arg> paramSymbolTypes;
    paramSymbolTypes.Reserve(m_parameters.Size());

    for (const RC<AstParameter>& param : m_parameters)
    {
        if (!param || !param->GetIdentifier())
        {
            // skip, should have added an error
            continue;
        }

        // add to list of param types
        paramSymbolTypes.PushBack(GenericInstanceTypeInfo::Arg(
            param->GetName(),
            param->GetIdentifier()->GetSymbolType(),
            CloneAstNode(param->GetDefaultValue()),
            param->IsRef(),
            param->IsConst()));
    }

    Scope* functionScope = &mod->scopeTree.Top();
    Assert(functionScope != nullptr);

    if (m_blockWithParameters != nullptr)
    {
        // - set when we need to insert casts for return statements to cast to the deduced return type
        //   or if the user specified a return type that differs from the deduced return type but can be promoted to it implicitly.
        // - we do this by providing an alias for `ReturnType` in a new scope and re-visiting the function body.
        // - AstReturnStatement nodes will insert cast expressions as needed.
        bool needsCast = false;

        if (functionScope->returnTypes.Any())
        {
            // search through return types for ambiguities
            for (const SymbolType* symbolType : functionScope->returnTypes)
            {
                Assert(symbolType != nullptr);

                if (m_returnTypeSpecification != nullptr)
                {
                    // strict mode, because user specifically stated the intended return type

                    if (m_returnType->TypeEqual(*symbolType))
                    {
                        // same type, do nothing
                    }
                    else if (m_returnType->TypeCompatible(
                                 *symbolType,
                                 /* strictNumbers */ true,
                                 /* strictAny */ false,
                                 /* strictEnum */ true))
                    {
                        needsCast = true;
                    }
                    else
                    {
                        // error; does not match what user specified
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_mismatched_return_type,
                            GetLocation(),
                            m_returnType->ToString(),
                            symbolType->ToString()));
                    }
                }
                else
                {
                    // deduce return type
                    if (m_returnType->TypeEqual(*symbolType))
                    {
                        // same type, do nothing
                    }
                    else if (m_returnType->IsAnyType())
                    {
                        // first potential return type found to deduce to, take it
                        m_returnType = symbolType;
                    }
                    else if (m_returnType->TypeCompatible(
                                 *symbolType,
                                 /* strictNumbers */ false,
                                 /* strictAny */ false,
                                 /* strictEnum */ false))
                    {
                        m_returnType = SymbolType::TypePromotion(m_returnType, symbolType);
                        needsCast = true;
                    }
                    else
                    {
                        // error; more than one possible deduced return type.
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_multiple_return_types,
                            GetLocation()));

                        break;
                    }
                }
            }
        }
        else
        {
            if (!m_returnTypeSpecification || !m_returnType->IsVoidType())
            {
                // check if last statement is an expression;
                // if it is, we use its type as the return type. otherwise, it is 'void'.

                if (m_blockWithParameters->IsLastStatementExpr())
                {
                    const SymbolType* lastExprType = m_blockWithParameters->GetLastExprType();

                    if (lastExprType != nullptr)
                    {
                        if (m_returnTypeSpecification != nullptr)
                        {
                            // strict mode, because user specifically stated the intended return type
                            if (m_returnType->TypeCompatible(
                                    *lastExprType,
                                    /* strictNumbers */ true,
                                    /* strictAny */ false,
                                    /* strictEnum */ true))
                            {
                                needsCast = true;
                            }
                            else
                            {
                                // error; does not match what user specified
                                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                                    LEVEL_ERROR,
                                    Msg_mismatched_return_type,
                                    GetLocation(),
                                    m_returnType->ToString(),
                                    lastExprType->ToString()));
                            }
                        }
                        else
                        {
                            m_returnType = lastExprType;
                        }
                    }
                }
                else
                {
                    // no expression at the end, so return type is void
                    if (m_returnTypeSpecification != nullptr)
                    {
                        if (!m_returnType->IsVoidType())
                        {
                            // error; does not match what user specified
                            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                                LEVEL_ERROR,
                                Msg_mismatched_return_type,
                                GetLocation(),
                                m_returnType->ToString(),
                                BuiltinTypes::s_voidType->ToString()));
                        }
                    }

                    m_returnType = BuiltinTypes::s_voidType;
                }
            }
            else
            {
                // void return type
                m_returnType = BuiltinTypes::s_voidType;
            }
        }

        if (needsCast)
        {
            // If return statements differ from the deduced return type, we need to insert a cast expression for each return statement
            // to ensure the correct type is returned.

            // we'll need to clone our block, open a new scope, provide replacement type via ReturnType.
            m_blockWithParameters = CloneAstNode(m_blockWithParameters);

            // @NOTE: do NOT use SCOPE_TYPE_FUNCTION, it'll treat it as a nested function and create closure captures.
            ScopeGuard newScope(mod, SCOPE_TYPE_NORMAL);

            SymbolType* returnTypeAlias = SymbolType::Alias("ReturnType", { m_returnType });
            returnTypeAlias->Register(visitor->GetCompilationUnit());
            newScope->identifierTable.AddSymbolType(returnTypeAlias);

            m_blockWithParameters->Visit(visitor, mod);
        }
    }
    else // function decl / extern (no block)
    {
        m_returnType = m_returnTypeSpecification != nullptr
            ? m_returnTypeSpecification->GetHeldType()
            : BuiltinTypes::s_anyType;

        if (m_returnType)
        {
            m_returnType = m_returnType->GetUnaliased();
        }
        else
        {
            m_returnType = BuiltinTypes::s_errorType;
        }
    }

    // create data members to copy closure parameters
    Array<SymbolTypeMember> closureObjMembers;

    for (const auto& it : functionScope->closureCaptures)
    {
        const String& name = it.first;
        const RC<Identifier>& identifier = it.second;

        Assert(identifier != nullptr);
        Assert(identifier->GetSymbolType() != nullptr);

        closureObjMembers.PushBack(SymbolTypeMember {
            identifier->GetName(),
            const_cast<SymbolType*>(identifier->GetSymbolType()),
            RC<AstVariable>(new AstVariable(name, m_location)) });
    }

    // close parameter scope
    mod->scopeTree.Close();

    SymbolType* functionType = nullptr;
    HYP_DEFER({
        if (functionType)
        {
            functionType->AssertRegistered();
        }
    });

    {
        // set object type to be an instance of function
        Array<GenericInstanceTypeInfo::Arg> genericParamTypes;
        genericParamTypes.Reserve(paramSymbolTypes.Size() + 1);
        genericParamTypes.EmplaceBack("@return", m_returnType, nullptr, false, false);

        // perform checking to see if it should still be considered a closure
        if (m_isClosure)
        {
            Assert(m_closureSelfParam != nullptr);
            Assert(m_closureSelfParam->GetIdentifier() != nullptr);

            if (closureObjMembers.Any() || m_closureSelfParam->GetIdentifier()->GetUseCount() > 0)
            {
                SymbolType* closureSelfTypePlaceholder = SymbolType::Placeholder("ClosureSelfType");
                closureSelfTypePlaceholder->Register(visitor->GetCompilationUnit());

                genericParamTypes.EmplaceBack(
                    m_closureSelfParam->GetName(),
                    closureSelfTypePlaceholder,
                    nullptr,
                    /* isRef */ false,
                    /* isConst */ false);
            }
            else
            {
                // unset m_isClosure, as closure 'self' param is unused.
                m_isClosure = false;
            }
        }

        for (auto& it : paramSymbolTypes)
        {
            genericParamTypes.PushBack(it);
        }

        functionType = SemanticAnalyzer::Helpers::SubstituteGenericParameters(
            visitor, mod,
            BuiltinTypes::s_functionType,
            genericParamTypes,
            m_location);

        Assert(functionType != nullptr);
        functionType->Register(visitor->GetCompilationUnit());
    }

    if (m_isClosure)
    {
        SymbolType* closureSelfType = SymbolType::Temp();
        closureSelfType->Register(visitor->GetCompilationUnit());

        ScopeGuard closureScope(mod, SCOPE_TYPE_NORMAL);

        SymbolType* closureSelfAliasType = SymbolType::Alias("ClosureSelfType", { closureSelfType });
        closureSelfAliasType->Register(visitor->GetCompilationUnit());
        closureScope->identifierTable.AddSymbolType(closureSelfAliasType);

        Array<RC<AstParameter>> closureParams;
        closureParams.Reserve(m_parameters.Size() + 1);
        closureParams.PushBack(CloneAstNode(m_closureSelfParam));

        for (const auto& it : m_parameters)
        {
            closureParams.PushBack(CloneAstNode(it));
        }

        // add $invoke to call this object
        RC<AstClass> closureClassDecl(new AstClass(
            visitor->GetCompilationUnit()->GetAnonClassName(),
            nullptr,
            {},
            { RC<AstVariableDeclaration>(new AstVariableDeclaration(
                "$invoke",
                RC<AstTypeSpecifier>(new AstTypeSpecifier(
                    RC<AstTypeRef>(new AstTypeRef(functionType, m_location)),
                    m_location)),
                RC<AstFunctionExpression>(new AstFunctionExpression(
                    closureParams,
                    CloneAstNode(m_returnTypeSpecification),
                    CloneAstNode(m_block),
                    false, // do not enable closure
                    m_location)),
                IdentifierFlags::PLACEHOLDER,
                m_location)) },
            {},
            CLASS_FLAG_ANONYMOUS,
            m_location));

        for (const SymbolTypeMember& member : closureObjMembers)
        {
            closureClassDecl->GetDataMembers().PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
                member.GetName(),
                RC<AstTypeSpecifier>(new AstTypeSpecifier(
                    RC<AstTypeRef>(new AstTypeRef(member.GetType(), m_location)),
                    m_location)),
                RC<AstNil>(new AstNil(m_location)),                  // placeholder; set later
                IdentifierFlags::PLACEHOLDER | IdentifierFlags::LAX, // don't emit errors for null assignment
                m_location)));
        }

        closureClassDecl->Visit(visitor, mod);

        Assert(closureClassDecl->GetHeldType() != nullptr);

        SymbolType* tmpAlias = SymbolType::Alias("ClosureSelfType", { closureClassDecl->GetHeldType() });
        closureSelfType->Assign(std::move(*tmpAlias));
        delete tmpAlias;
        tmpAlias = nullptr;

        m_closureBlock.Reset(new AstBlock(m_location));

        // create new instance of closure class
        m_closureBlock->AddChild(RC<AstVariableDeclaration>(new AstVariableDeclaration(
            "$__closure_instance",
            nullptr,
            RC<AstNewExpression>(new AstNewExpression(
                RC<AstTypeSpecifier>(new AstTypeSpecifier(
                    RC<AstTypeRef>(new AstTypeRef(closureSelfType, m_location)),
                    m_location)),
                nullptr, // no constructor args
                false,   // enable constructor call
                m_location)),
            IdentifierFlags::NONE,
            m_location)));

        // init each member of the closure object
        for (const SymbolTypeMember& member : closureObjMembers)
        {
            // $__closure_instance.<member> = <value>;
            m_closureBlock->AddChild(RC<AstBinaryExpression>(new AstBinaryExpression(
                RC<AstMember>(new AstMember(
                    member.GetName(),
                    RC<AstVariable>(new AstVariable("$__closure_instance", m_location)),
                    m_location)),
                CloneAstNode(member.GetExpr()),
                Operator::FindBinaryOperator(Operators::OP_assign),
                m_location)));
        }

        // return the closure instance as the value of this expression
        m_closureBlock->AddChild(RC<AstVariable>(new AstVariable("$__closure_instance", m_location)));
        m_closureBlock->Visit(visitor, mod);

        m_closureBlock->PrependChild(closureClassDecl);

        // HACK
        m_symbolType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
            visitor,
            mod,
            closureSelfType,
            m_location);

        m_symbolType->Register(visitor->GetCompilationUnit());

        return;
    }

    // HACK
    m_symbolType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(
        visitor,
        mod,
        functionType,
        m_location);

    m_symbolType->Register(visitor->GetCompilationUnit());

    if (m_parameters.Size() > MathUtil::MaxSafeValue<uint8>())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_maximum_number_of_arguments,
            m_location));
    }
}

UniquePtr<Buildable> AstFunctionExpression::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (!m_blockWithParameters)
    {
        return nullptr;
    }

    if (m_isClosure)
    {
        Assert(m_closureBlock != nullptr);

        // load closure object into register
        chunk->Append(BytecodeUtil::Make<Comment>("Begin closure object initialization"));
        chunk->Append(m_closureBlock->Build(visitor, mod));
        chunk->Append(BytecodeUtil::Make<Comment>("End closure object initialization"));

        return chunk;
    }

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    AstParameter* variadicParam = nullptr;

    // Find variadic parameter, reserve stack location for the array that will be created:
    for (SizeType index = m_parameters.Size(); index > 0; index--)
    {
        const RC<AstParameter>& param = m_parameters[index - 1];
        Assert(param != nullptr);

        if (param->IsVariadic())
        {
            Assert(variadicParam == nullptr);
            Assert(index == m_parameters.Size());

            variadicParam = param.Get();
        }
    }

    uint16 numArgs = 0;

    if (m_isClosure && m_closureSelfParam != nullptr)
    {
        chunk->Append(m_closureSelfParam->Build(visitor, mod));

        numArgs++;
    }

    for (SizeType index = 0; index < m_parameters.Size(); index++, numArgs++)
    {
        const RC<AstParameter>& param = m_parameters[index];
        Assert(param != nullptr);

        chunk->Append(param->Build(visitor, mod));
    }

    uint8 rp;

    Assert(m_parameters.Size() + (m_isClosure ? 1 : 0) <= MathUtil::MaxSafeValue<uint8>());

    EnumFlags<HypMethodFlags> methodFlags = HypMethodFlags::NONE;

    if (variadicParam)
    {
        const RC<AstParameter>& last = m_parameters.Back();
        Assert(last != nullptr);

        methodFlags |= HypMethodFlags::VARIADIC;

        // chunk->Append(variadicParam->Build(visitor, mod));
    }

    // the label to jump to the very end
    LabelId endLabel = contextGuard->NewLabel(NAME("AfterFunctionBody"));
    chunk->TakeOwnershipOfLabel(endLabel);

    LabelId funcAddr = contextGuard->NewLabel(NAME("FunctionBody"));
    chunk->TakeOwnershipOfLabel(funcAddr);

    // jump to end as to not execute the function body
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, endLabel));

    // store the function address before the function body
    chunk->Append(BytecodeUtil::Make<LabelMarker>(funcAddr));

    // Build the function
    chunk->Append(BuildFunctionBody(visitor, mod));

    // set the label's position to after the block
    chunk->Append(BytecodeUtil::Make<LabelMarker>(endLabel));

    // store local variable
    // get register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    Assert(numArgs <= UINT8_MAX);

    auto func = BytecodeUtil::Make<ScriptFunction>();
    func->labelId = funcAddr;
    func->reg = rp;
    func->nargs = (uint8)numArgs;
    func->flags = (uint8)methodFlags;
    chunk->Append(std::move(func));

    return chunk;
}

UniquePtr<Buildable> AstFunctionExpression::BuildFunctionBody(AstVisitor* visitor, Module* mod)
{
    Assert(m_blockWithParameters != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // increase stack size by the number of parameters
    const SizeType paramStackSize = m_parameters.Size() + ((m_isClosure && m_closureSelfParam != nullptr) ? 1 : 0);

    // increase stack size for call stack info
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    // build the function body
    chunk->Append(m_blockWithParameters->Build(visitor, mod));

    if (!m_blockWithParameters->IsLastStatementReturn())
    {
        // add RET instruction
        chunk->Append(BytecodeUtil::Make<Return>());
    }

    for (SizeType i = 0; i < paramStackSize; i++)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    // decrease stack size for call stack info
    visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();

    return chunk;
}

void AstFunctionExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_closureBlock != nullptr)
    {
        m_closureBlock->Optimize(visitor, mod);

        return;
    }

    for (auto& param : m_parameters)
    {
        if (param != nullptr)
        {
            param->Optimize(visitor, mod);
        }
    }

    if (m_blockWithParameters != nullptr)
    {
        m_blockWithParameters->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstFunctionExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstFunctionExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstFunctionExpression::MayHaveSideEffects() const
{
    // changed to true because it affects registers
    return true;
}

const SymbolType* AstFunctionExpression::GetExprType() const
{
    return m_symbolType;
}

} // namespace hyperion
