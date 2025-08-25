#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/utilities/Optional.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstTypeExpression::AstTypeExpression(
    const String& name,
    const RC<AstPrototypeSpecification>& baseSpecification,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    const SymbolTypeRef& enumUnderlyingType,
    bool isProxyClass,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_baseSpecification(baseSpecification),
      m_dataMembers(dataMembers),
      m_functionMembers(functionMembers),
      m_staticMembers(staticMembers),
      m_enumUnderlyingType(enumUnderlyingType),
      m_isProxyClass(isProxyClass),
      m_isUninstantiatedGeneric(false),
      m_isVisited(false)
{
}

AstTypeExpression::AstTypeExpression(
    const String& name,
    const RC<AstPrototypeSpecification>& baseSpecification,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    bool isProxyClass,
    const SourceLocation& location)
    : AstTypeExpression(
          name,
          baseSpecification,
          dataMembers,
          functionMembers,
          staticMembers,
          nullptr,
          isProxyClass,
          location)
{
}

void AstTypeExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr && mod != nullptr);
    Assert(!m_isVisited);

    m_isUninstantiatedGeneric = mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG);
    bool hasCustomProto = false;

    // Create scope
    ScopeGuard scope(mod, SCOPE_TYPE_NORMAL, IsEnum() ? ScopeFunctionFlags::ENUM_MEMBERS_FLAG : 0);

    SymbolTypeRef prototypeType = SymbolType::Object(
        "$$" + m_name + "Prototype",
        {},
        BuiltinTypes::OBJECT);

    SymbolTypeRef baseType = BuiltinTypes::OBJECT;

    if (m_baseSpecification != nullptr)
    {
        m_baseSpecification->Visit(visitor, mod);

        Assert(m_baseSpecification->GetExprType() != nullptr);

        if (auto baseTypeInner = m_baseSpecification->GetHeldType())
        {
            baseType = baseTypeInner;
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                m_location,
                m_baseSpecification->GetExprType()->ToString()));
        }
    }

    if (IsEnum())
    {
        // Create a generic instance of the enum type
        m_symbolType = SymbolType::GenericInstance(
            BuiltinTypes::ENUM_TYPE,
            GenericInstanceTypeInfo {
                { { "of", m_enumUnderlyingType } } });

        // m_typeObject.Reset(new AstTypeObject(
        //     m_symbolType,
        //     BuiltinTypes::CLASS_TYPE,
        //     m_enumUnderlyingType,
        //     m_isProxyClass,
        //     m_location));
    }
    else
    {
        m_symbolType = SymbolType::Extend(
            m_name,
            baseType,
            {});

        if (m_isProxyClass)
        {
            m_symbolType->GetFlags() |= SYMBOL_TYPE_FLAGS_PROXY;
        }

        if (m_isUninstantiatedGeneric)
        {
            m_symbolType->GetFlags() |= SYMBOL_TYPE_FLAGS_UNINSTANTIATED_GENERIC;
        }

        // m_typeObject.Reset(new AstTypeObject(
        //     m_symbolType,
        //     BuiltinTypes::CLASS_TYPE,
        //     m_enumUnderlyingType,
        //     m_isProxyClass,
        //     m_location));

        // special names
        bool protoFound = false;
        bool baseFound = false;
        bool nameFound = false;

        for (const auto& mem : m_staticMembers)
        {
            Assert(mem != nullptr);

            if (mem->GetName() == "$proto")
            {
                protoFound = true;
                hasCustomProto = true;
            }
            else if (mem->GetName() == "base")
            {
                baseFound = true;
            }
            else if (mem->GetName() == "name")
            {
                nameFound = true;
            }
        }

        if (!protoFound)
        { // no custom '$proto' member, add default.
            m_symbolType->AddMember(SymbolTypeMember {
                "$proto",
                prototypeType,
                RC<AstTypeRef>(new AstTypeRef(
                    prototypeType,
                    m_location)) });
        }

        if (!baseFound)
        { // no custom 'base' member, add default
            m_symbolType->AddMember(SymbolTypeMember {
                "base",
                BuiltinTypes::CLASS_TYPE,
                RC<AstTypeRef>(new AstTypeRef(
                    baseType,
                    m_location)) });
        }

        if (!nameFound)
        { // no custom 'name' member, add default
            m_symbolType->AddMember(SymbolTypeMember {
                "name",
                BuiltinTypes::STRING,
                RC<AstString>(new AstString(
                    m_name,
                    m_location)) });
        }

        if (hasCustomProto)
        {
            m_symbolType->GetFlags() |= SYMBOL_TYPE_FLAGS_PROXY;
        }
    }

    // m_symbolType->SetTypeObject(m_typeObject);

    // Add the symbol type to the identifier table so that it can be used within the type definition.
    // scope->GetIdentifierTable().AddSymbolType(m_symbolType);

    // if (mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG)) { // add symbol type to be usable within members
    //     SymbolTypeRef placeholderType = m_symbolType;

    //     // If the type is generic, we need to use a placeholder type
    //     // so that we can use the type within the type definition, without having to
    //     // instantiate it first.
    //     // if (m_symbolType->IsGeneric()) {
    //         placeholderType = SymbolType::Alias(
    //             m_symbolType->GetName(),
    //             { BuiltinTypes::PLACEHOLDER }
    //         );
    //     // }

    //     scope->GetIdentifierTable().AddSymbolType(placeholderType);
    // }

    { // add type aliases to be usable within members
        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            "SelfType",
            { m_symbolType }));

        scope->GetIdentifierTable().AddSymbolType(SymbolType::Alias(
            m_symbolType->GetName(),
            { m_symbolType }));
    }

    // ===== STATIC DATA MEMBERS ======
    {
        ScopeGuard staticDataMembers(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        // Add a static $invoke method which will be used to invoke the constructor.

        for (const auto& mem : m_staticMembers)
        {
            Assert(mem != nullptr);
            mem->Visit(visitor, mod);

            String memName = mem->GetName();

            Assert(mem->GetIdentifier() != nullptr);
            SymbolTypeRef memType = mem->GetIdentifier()->GetSymbolType();

            m_symbolType->AddMember(SymbolTypeMember {
                memName,
                memType,
                mem->GetRealAssignment() });
        }
    }

    // ===== INSTANCE DATA MEMBERS =====

    Optional<SymbolTypeMember> constructorMember;

    // open the scope for data members
    {
        ScopeGuard instanceDataMembers(mod, SCOPE_TYPE_TYPE_DEFINITION, 0);

        // Do data members first so we can use them all in functions.

        for (const RC<AstVariableDeclaration>& mem : m_dataMembers)
        {
            if (mem != nullptr)
            {
                mem->Visit(visitor, mod);

                Assert(mem->GetIdentifier() != nullptr);

                prototypeType->AddMember(SymbolTypeMember {
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment() });
            }
        }

        for (const RC<AstVariableDeclaration>& mem : m_functionMembers)
        {
            if (mem != nullptr)
            {
                // if name of the method matches that of the class, it is the constructor.
                const bool isConstructorDefinition = mem->GetName() == m_name;

                if (isConstructorDefinition)
                {
                    // ScopeGuard constructorDefinitionScope(mod, SCOPE_TYPE_FUNCTION, CONSTRUCTOR_DEFINITION_FLAG);

                    mem->ApplyIdentifierFlags(FLAG_CONSTRUCTOR);
                    mem->SetName("$construct");

                    mem->Visit(visitor, mod);
                }
                else
                {
                    mem->Visit(visitor, mod);
                }

                Assert(mem->GetIdentifier() != nullptr);

                SymbolTypeMember member {
                    mem->GetName(),
                    mem->GetIdentifier()->GetSymbolType(),
                    mem->GetRealAssignment()
                };

                if (isConstructorDefinition)
                {
                    constructorMember = member;
                }

                prototypeType->AddMember(std::move(member));
            }
        }
    }

#if HYP_SCRIPT_CALLABLE_CLASS_CONSTRUCTORS
    bool invokeFound = false;

    // Find the $invoke member on the class object (if it exists)
    for (const SymbolTypeMember& it : m_symbolType->GetMembers())
    {
        if (it.name == "$invoke")
        {
            invokeFound = true;
            break;
        }
    }

    if (!invokeFound && !IsProxyClass() && !IsEnum())
    { // Add an '$invoke' static member, if not already defined.
        Array<RC<AstParameter>> invokeParams;
        invokeParams.Reserve(1);

        // Add `self: typeof SelfType`
        invokeParams.PushBack(RC<AstParameter>(new AstParameter(
            "self", // self: typeof SelfType
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstTypeRef>(new AstTypeRef(
                    BuiltinTypes::CLASS_TYPE,
                    m_location)),
                m_location)),
            nullptr,
            false,
            false,
            false,
            m_location)));

        if (constructorMember.HasValue())
        {
            // We need to get the arguments for the constructor member, if possible

            const SymbolTypeMember& constructorMemberRef = constructorMember.Get();

            SymbolTypeRef constructorMemberType = constructorMemberRef.type;
            Assert(constructorMemberType != nullptr);
            constructorMemberType = constructorMemberType->GetUnaliased();

            // Rely on the fact that the constructor member type is a function type
            if (constructorMemberType->IsGenericInstanceType())
            {
                // Get params from generic expression type
                const Array<GenericInstanceTypeInfo::Arg>& params = constructorMemberType->GetGenericInstanceInfo().m_genericArgs;
                Assert(params.Size() >= 1, "Generic param list must have at least one parameter (return type should be first).");

                // `self` not guaranteed to be first parameter, so reserve with what we know we have
                invokeParams.Reserve(params.Size() - 1);

                // Start at 2 to skip the return type and `self` parameter - it will be supplied by `new SelfType()`
                for (SizeType i = 2; i < params.Size(); i++)
                {
                    const GenericInstanceTypeInfo::Arg& param = params[i];

                    SymbolTypeRef paramType = param.m_type;
                    Assert(paramType != nullptr);
                    paramType = paramType->GetUnaliased();

                    const bool isVariadic = paramType->IsVarArgsType() && i == params.Size() - 1;

                    RC<AstPrototypeSpecification> paramTypeSpec(new AstPrototypeSpecification(
                        RC<AstTypeRef>(new AstTypeRef(
                            paramType,
                            m_location)),
                        m_location));

                    invokeParams.PushBack(RC<AstParameter>(new AstParameter(
                        param.m_name,
                        paramTypeSpec,
                        CloneAstNode(param.m_defaultValue),
                        isVariadic,
                        param.m_isConst,
                        param.m_isRef,
                        m_location)));
                }
            }
        }

        // we don't provide `self` (the class) to the new expression

        Array<RC<AstArgument>> invokeArgs;
        invokeArgs.Reserve(invokeParams.Size() - 1);

        // Pass each parameter as an argument to the constructor
        for (SizeType index = 1; index < invokeParams.Size(); index++)
        {
            const RC<AstParameter>& param = invokeParams[index];
            Assert(param != nullptr);

            invokeArgs.PushBack(RC<AstArgument>(new AstArgument(
                RC<AstVariable>(new AstVariable(
                    param->GetName(),
                    m_location)),
                false,
                false,
                param->IsRef(),
                param->IsConst(),
                param->GetName(),
                m_location)));
        }

        RC<AstBlock> invokeBlock(new AstBlock(m_location));

        // Add AstNewExpression to the block
        // `new Self($invokeArgs...)`
        invokeBlock->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
            RC<AstNewExpression>(new AstNewExpression(
                RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                    RC<AstTypeRef>(new AstTypeRef(
                        m_symbolType,
                        m_location)),
                    m_location)),
                RC<AstArgumentList>(new AstArgumentList(
                    invokeArgs,
                    m_location)),
                true, // enable constructor call
                m_location)),
            m_location)));

        RC<AstFunctionExpression> invokeExpr(new AstFunctionExpression(
            invokeParams,
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstTypeRef>(new AstTypeRef(
                    m_symbolType,
                    m_location)),
                m_location)),
            invokeBlock,
            m_location));

        invokeExpr->Visit(visitor, mod);

        // // add it to the list of static members
        // m_staticMembers.PushBack(RC<AstVariableDeclaration>(new AstVariableDeclaration(
        //     "$invoke",
        //     nullptr,
        //     invokeExpr,
        //     IdentifierFlags::FLAG_CONST,
        //     m_location
        // )));

        // Add $invoke member to the symbol type
        m_symbolType->AddMember(SymbolTypeMember {
            "$invoke",
            invokeExpr->GetExprType(),
            CloneAstNode(invokeExpr) });
    }
#endif

    // { // create a type object for the prototype type
    //     m_prototypeExpr.Reset(new AstTypeObject(
    //         prototypeType,
    //         BuiltinTypes::CLASS_TYPE,
    //         m_location));

    //     prototypeType->SetTypeObject(m_prototypeExpr);
    //     m_prototypeExpr->Visit(visitor, mod); // will register the type. it will be built later.
    // }

    // { // Finally we visit the newly created AstTypeObject, this will Register our SymbolType
    //     m_typeObject->Visit(visitor, mod);
    // }

    { // create a type ref for the symbol type
        m_typeRef.Reset(new AstTypeRef(
            m_symbolType,
            m_location));

        m_typeRef->Visit(visitor, mod);
    }

    m_isVisited = true;
}

UniquePtr<Buildable> AstTypeExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_symbolType != nullptr);
//    Assert(m_symbolType->GetId() != -1);

    Assert(m_isVisited);

    // Assert(!m_isUninstantiatedGeneric, "Cannot build an uninstantiated generic type.");

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_prototypeExpr != nullptr)
    {
        chunk->Append(m_prototypeExpr->Build(visitor, mod));
    }

    // Assert(m_typeObject != nullptr);
    // chunk->Append(m_typeObject->Build(visitor, mod));

    Assert(m_typeRef != nullptr);
    chunk->Append(m_typeRef->Build(visitor, mod));

    return chunk;
}

void AstTypeExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_isVisited);

    // Assert(m_typeObject != nullptr);
    // m_typeObject->Optimize(visitor, mod);

//    Assert(m_prototypeExpr != nullptr);
//    m_prototypeExpr->Optimize(visitor, mod);

    Assert(m_typeRef != nullptr);
    m_typeRef->Optimize(visitor, mod);
}

RC<AstStatement> AstTypeExpression::Clone() const
{
    return CloneImpl();
}

bool AstTypeExpression::IsLiteral() const
{
    return false;
}

Tribool AstTypeExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeExpression::MayHaveSideEffects() const
{
    return true;
}

SymbolTypeRef AstTypeExpression::GetExprType() const
{
    return BuiltinTypes::CLASS_TYPE;
}

SymbolTypeRef AstTypeExpression::GetHeldType() const
{
    Assert(m_symbolType != nullptr);

    return m_symbolType;
}

const AstExpression* AstTypeExpression::GetValueOf() const
{
    Assert(m_isVisited);
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetValueOf();
}

const AstExpression* AstTypeExpression::GetDeepValueOf() const
{
    Assert(m_isVisited);
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetDeepValueOf();
}

const String& AstTypeExpression::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
