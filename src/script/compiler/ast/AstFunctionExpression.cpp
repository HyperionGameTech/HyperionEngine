#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Scope.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/math/MathUtil.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

#include <vector>
#include <iostream>

namespace hyperion::compiler {

AstFunctionExpression::AstFunctionExpression(
    const Array<RC<AstParameter>>& parameters,
    const RC<AstPrototypeSpecification>& returnTypeSpecification,
    const RC<AstBlock>& block,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_parameters(parameters),
      m_returnTypeSpecification(returnTypeSpecification),
      m_block(block),
      m_isClosure(false),
      m_isConstructorDefinition(false),
      m_returnType(BuiltinTypes::ANY),
      m_staticId(0)
{
}

void AstFunctionExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);
    Assert(m_block != nullptr);

    m_blockWithParameters = CloneAstNode(m_block);

    m_isClosure = true;
    m_isConstructorDefinition = GetExpressionFlags() & EXPR_FLAGS_CONSTRUCTOR_DEFINITION;

    int scopeFlags = 0;

    if (m_isClosure)
    {
        scopeFlags |= ScopeFunctionFlags::CLOSURE_FUNCTION_FLAG;

        // closures are objects with a method named '$invoke',
        // so we pass the '$functor' argument when it is called.
        m_closureSelfParam.Reset(new AstParameter(
            "$functor",
            nullptr,
            nullptr,
            false,
            false,
            false,
            m_location));
    }

    if (m_isConstructorDefinition)
    {
        scopeFlags |= CONSTRUCTOR_DEFINITION_FLAG;
    }

    // // open the new scope for parameters
    mod->m_scopes.Open(Scope(
        SCOPE_TYPE_FUNCTION,
        scopeFlags));

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
        if (m_isConstructorDefinition)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_return_type_specification_invalid_on_constructor,
                m_returnTypeSpecification->GetLocation()));
        }
        else
        {
            m_blockWithParameters->PrependChild(m_returnTypeSpecification);
        }
    }

    if (m_isConstructorDefinition)
    {
        // add implicit 'return self' at the end
        m_blockWithParameters->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
            RC<AstVariable>(new AstVariable(
                "self",
                m_blockWithParameters->GetLocation())),
            m_blockWithParameters->GetLocation())));
    }

    // visit the function body
    m_blockWithParameters->Visit(visitor, mod);

    if (m_returnTypeSpecification != nullptr)
    {
        if (m_returnTypeSpecification->GetHeldType() != nullptr)
        {
            m_returnType = m_returnTypeSpecification->GetHeldType();
        }
        else
        {
            m_returnType = BuiltinTypes::UNDEFINED;
        }
    }

    // first item will be set to return type
    Array<GenericInstanceTypeInfo::Arg> paramSymbolTypes;

    for (auto& param : m_parameters)
    {
        if (!param || !param->GetIdentifier())
        {
            // skip, should have added an error
            continue;
        }

        // add to list of param types
        paramSymbolTypes.PushBack(GenericInstanceTypeInfo::Arg {
            .m_name = param->GetName(),
            .m_type = param->GetIdentifier()->GetSymbolType(),
            .m_defaultValue = param->GetDefaultValue(),
            .m_isRef = param->IsRef(),
            .m_isConst = param->IsConst() });
    }

    const Scope* functionScope = &mod->m_scopes.Top(); // m_blockWithParameters->GetScope();
    Assert(functionScope != nullptr);

    if (functionScope->GetReturnTypes().Any())
    {
        // search through return types for ambiguities
        for (const SymbolTypeRef& symbolType : functionScope->GetReturnTypes())
        {
            Assert(symbolType != nullptr);

            if (m_returnTypeSpecification != nullptr)
            {
                // strict mode, because user specifically stated the intended return type
                if (!m_returnType->TypeCompatible(*symbolType, true))
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
                if (m_returnType->IsAnyType() || m_returnType->IsPlaceholderType())
                {
                    m_returnType = symbolType;
                }
                else if (m_returnType->TypeCompatible(*symbolType, false))
                {
                    m_returnType = SymbolType::TypePromotion(m_returnType, symbolType);
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
        m_returnType = BuiltinTypes::VOID_TYPE;
    }

    // create data members to copy closure parameters
    Array<SymbolTypeMember> closureObjMembers;

    for (const auto& it : functionScope->GetClosureCaptures())
    {
        const String& name = it.first;
        const RC<Identifier>& identifier = it.second;

        Assert(identifier != nullptr);
        Assert(identifier->GetSymbolType() != nullptr);

        RC<AstExpression> currentValue(new AstVariable(
            name,
            m_location));

        closureObjMembers.PushBack(SymbolTypeMember {
            identifier->GetName(),
            identifier->GetSymbolType(),
            currentValue });
    }

    // close parameter scope
    mod->m_scopes.Close();

    // set object type to be an instance of function
    Array<GenericInstanceTypeInfo::Arg> genericParamTypes;
    genericParamTypes.Reserve(paramSymbolTypes.Size() + 1);
    genericParamTypes.PushBack({ "@return",
        m_returnType });

    // perform checking to see if it should still be considered a closure
    if (m_isClosure)
    {
        Assert(m_closureSelfParam != nullptr);
        Assert(m_closureSelfParam->GetIdentifier() != nullptr);

        if (closureObjMembers.Any() || m_closureSelfParam->GetIdentifier()->GetUseCount() > 0)
        {
            genericParamTypes.PushBack(GenericInstanceTypeInfo::Arg {
                m_closureSelfParam->GetName(),
                BuiltinTypes::ANY,
                nullptr });
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

    Array<RC<AstArgument>> genericParams;
    genericParams.Reserve(genericParamTypes.Size());

    for (auto& it : genericParamTypes)
    {
        genericParams.PushBack(RC<AstArgument>(new AstArgument(
            RC<AstTypeRef>(new AstTypeRef(
                it.m_type,
                m_location)),
            false,
            false,
            false,
            false,
            it.m_name,
            m_location)));
    }

    RC<AstPrototypeSpecification> functionTypeSpec(new AstPrototypeSpecification(
        RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            RC<AstVariable>(new AstVariable(
                "function",
                m_location)),
            genericParams,
            m_location)),
        m_location));

    functionTypeSpec->Visit(visitor, mod);

    SymbolTypeRef functionType = functionTypeSpec->GetHeldType();

    if (functionType == nullptr)
    {
        functionType = BuiltinTypes::UNDEFINED;
    }

    functionType = functionType->GetUnaliased();

    if (functionType != BuiltinTypes::UNDEFINED)
    {
        const SymbolTypeFlags currentFlags = functionType->GetFlags();

        functionType = SymbolType::GenericInstance(
            functionType,
            GenericInstanceTypeInfo {
                genericParamTypes });

        functionType->SetFlags(currentFlags);
    }

    if (m_isClosure)
    {
        String closureName = "$$closure";

        for (const auto& it : functionScope->GetClosureCaptures())
        {
            const String& name = it.first;
            const RC<Identifier>& identifier = it.second;

            Assert(identifier != nullptr);
            Assert(identifier->GetSymbolType() != nullptr);

            closureName += "$" + name;
        }

        // add $invoke to call this object
        m_closureTypeExpr.Reset(new AstTypeExpression(
            "__closure",
            nullptr,
            {},
            { RC<AstVariableDeclaration>(new AstVariableDeclaration(
                "$invoke",
                RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstTypeRef>(new AstTypeRef(
                        functionType,
                        m_location)),
                    m_location)),
                RC<AstNil>(new AstNil(m_location)), // placeholder; set to function type later
                IdentifierFlags::FLAG_CONST,
                m_location)) },
            {},
            false, // not proxy class
            m_location));

        for (const SymbolTypeMember& member : closureObjMembers)
        {
            m_closureTypeExpr->GetDataMembers().PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
                member.name,
                nullptr,
                member.expr,
                IdentifierFlags::FLAG_NONE,
                m_location)));
        }

        m_closureTypeExpr->Visit(visitor, mod);

        SymbolTypeRef closureHeldType = m_closureTypeExpr->GetHeldType();
        Assert(closureHeldType != nullptr);
        closureHeldType = closureHeldType->GetUnaliased();

        if (closureHeldType != BuiltinTypes::UNDEFINED)
        {
            //            Assert(closureHeldType->GetId() != -1);
            // Assert(closureHeldType->GetTypeObject().Lock() != nullptr);
        }

        m_functionTypeExpr.Reset(new AstPrototypeSpecification(
            RC<AstTypeRef>(new AstTypeRef(
                closureHeldType,
                m_location)),
            m_location));

        m_functionTypeExpr->Visit(visitor, mod);

        m_symbolType = std::move(closureHeldType);
    }
    else
    {
        m_symbolType = std::move(functionType);
        m_functionTypeExpr = std::move(functionTypeSpec);
    }

    // we do +1 to account for closure self var.
    const SizeType numArguments = m_isClosure
        ? m_parameters.Size() + 1
        : m_parameters.Size();

    if (numArguments > MathUtil::MaxSafeValue<uint8>())
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_maximum_number_of_arguments,
            m_location));
    }
}

UniquePtr<Buildable> AstFunctionExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_blockWithParameters != nullptr);

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_closureTypeExpr != nullptr)
    {
        chunk->Append(m_closureTypeExpr->Build(visitor, mod));
    }

    if (m_functionTypeExpr != nullptr && !m_isClosure)
    {
        chunk->Append(m_functionTypeExpr->Build(visitor, mod));
    }

    if (m_isClosure && m_closureSelfParam != nullptr)
    {
        chunk->Append(m_closureSelfParam->Build(visitor, mod));
    }

    for (SizeType index = 0; index < m_parameters.Size(); index++)
    {
        Assert(m_parameters[index] != nullptr);
        chunk->Append(m_parameters[index]->Build(visitor, mod));
    }

    uint8 rp;

    Assert(m_parameters.Size() + (m_isClosure ? 1 : 0) <= MathUtil::MaxSafeValue<uint8>());

    // the properties of this function
    uint8 nargs = uint8(m_parameters.Size());

    if (m_isClosure)
    {
        nargs++; // make room for the closure self object
    }

    uint8 flags = FunctionFlags::NONE;

    if (m_parameters.Any())
    {
        const RC<AstParameter>& last = m_parameters.Back();
        Assert(last != nullptr);

        if (last->IsVariadic())
        {
            flags |= FunctionFlags::VARIADIC;
        }
    }

    if (m_isClosure)
    {
        flags |= FunctionFlags::CLOSURE;
    }

    // the label to jump to the very end
    LabelId endLabel = contextGuard->NewLabel();
    chunk->TakeOwnershipOfLabel(endLabel);

    LabelId funcAddr = contextGuard->NewLabel();
    chunk->TakeOwnershipOfLabel(funcAddr);

    // jump to end as to not execute the function body
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, endLabel));

    // store the function address before the function body
    chunk->Append(BytecodeUtil::Make<LabelMarker>(funcAddr));

    // TODO add optimization to avoid duplicating the function body
    // Build the function
    chunk->Append(BuildFunctionBody(visitor, mod));

    // set the label's position to after the block
    chunk->Append(BytecodeUtil::Make<LabelMarker>(endLabel));

    // store local variable
    // get register index
    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    auto func = BytecodeUtil::Make<BuildableFunction>();
    func->labelId = funcAddr;
    func->reg = rp;
    func->nargs = nargs;
    func->flags = flags;
    chunk->Append(std::move(func));

    if (m_isClosure)
    {
        Assert(m_functionTypeExpr != nullptr);

        const uint8 funcExprReg = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // increase reg usage for closure object to hold it while we move this function expr as a member
        visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        const uint8 closureObjReg = rp;

        // load __closure into register
        chunk->Append(BytecodeUtil::Make<Comment>("Load __closure object"));
        chunk->Append(m_functionTypeExpr->Build(visitor, mod));

        // set $proto.$invoke to the function object

        // load $proto
        chunk->Append(BytecodeUtil::Make<Comment>("Load $proto"));
        const HashCode::ValueType protoHash = HashCode::GetHashCode("$proto").Value();
        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, protoHash));

        // store into $invoke
        chunk->Append(BytecodeUtil::Make<Comment>("Store $invoke"));
        const HashCode::ValueType invokeHash = HashCode::GetHashCode("$invoke").Value();
        chunk->Append(Compiler::StoreMemberFromHash(visitor, mod, invokeHash));

        visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // ASSERT_MSG(rp == 0, "Register position should be 0 to return closure object");

        // swap regs, so the closure object returned (put on register zero)
        auto instrMovReg = BytecodeUtil::Make<RawOperation<>>();
        instrMovReg->opcode = MOV_REG;
        instrMovReg->Accept<uint8>(0);             // dst
        instrMovReg->Accept<uint8>(closureObjReg); // src
        chunk->Append(std::move(instrMovReg));
    }

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
    if (m_closureTypeExpr != nullptr)
    {
        m_closureTypeExpr->Optimize(visitor, mod);
    }

    if (m_functionTypeExpr != nullptr)
    {
        m_functionTypeExpr->Optimize(visitor, mod);
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

SymbolTypeRef AstFunctionExpression::GetExprType() const
{
    if (m_isClosure && m_closureTypeExpr != nullptr)
    {
        SymbolTypeRef heldType = m_closureTypeExpr->GetHeldType();
        Assert(heldType != nullptr);

        return heldType->GetUnaliased();
    }

    return m_symbolType;
}

} // namespace hyperion::compiler
