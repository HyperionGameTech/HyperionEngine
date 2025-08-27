#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypConstant.hpp>
#include <core/object/HypProperty.hpp>

#include <core/utilities/Optional.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

AstClass::AstClass(
    const String& name,
    const RC<AstTypeSpecifier>& baseSpec,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    const SymbolTypeRef& enumUnderlyingType,
    bool isProxyClass,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_baseSpec(baseSpec),
      m_dataMembers(dataMembers),
      m_functionMembers(functionMembers),
      m_staticMembers(staticMembers),
      m_enumUnderlyingType(enumUnderlyingType),
      m_isProxyClass(isProxyClass),
      m_isUninstantiatedGeneric(false),
      m_isVisited(false)
{
}

AstClass::AstClass(
    const String& name,
    const SymbolTypeRef& baseType,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    bool isProxyClass,
    const SourceLocation& location)
    : AstClass(
          name,
          nullptr,
          dataMembers,
          functionMembers,
          staticMembers,
          nullptr,
          isProxyClass,
          location)
{
    m_baseType = baseType;
}

AstClass::AstClass(
    const String& name,
    const RC<AstTypeSpecifier>& baseSpec,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    bool isProxyClass,
    const SourceLocation& location)
    : AstClass(
          name,
          baseSpec,
          dataMembers,
          functionMembers,
          staticMembers,
          nullptr,
          isProxyClass,
          location)
{
}

void AstClass::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr && mod != nullptr);
    Assert(!m_isVisited);

    m_isUninstantiatedGeneric = mod->IsInScopeOfType(SCOPE_TYPE_NORMAL, UNINSTANTIATED_GENERIC_FLAG);
    bool hasCustomProto = false;

    // Create scope
    ScopeGuard scope(mod, SCOPE_TYPE_NORMAL, IsEnum() ? ScopeFunctionFlags::ENUM_MEMBERS_FLAG : 0);

    if (m_baseSpec != nullptr)
    {
        m_baseSpec->Visit(visitor, mod);

        Assert(m_baseSpec->GetExprType() != nullptr);

        if (SymbolTypeRef baseTypeInner = m_baseSpec->GetHeldType())
        {
            m_baseType = baseTypeInner;
        }
        else
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_not_a_type,
                m_location,
                m_baseSpec->GetExprType()->ToString()));
        }
    }

    if (IsEnum())
    {
        // Create a generic instance of the enum type
        m_symbolType = SymbolType::GenericInstance(
            BuiltinTypes::ENUM_TYPE,
            GenericInstanceTypeInfo {
                { { "of", m_enumUnderlyingType } } });
    }
    else
    {
        if (m_baseType != nullptr)
        {
            m_symbolType = SymbolType::Extend(
                m_name,
                m_baseType,
                {}, {});
        }
        else
        {
            m_symbolType = SymbolType::Object(
                m_name,
                BuiltinTypes::OBJECT,
                {}, {});
        }

        if (m_isProxyClass)
        {
            m_symbolType->GetFlags() |= SYMBOL_TYPE_FLAGS_PROXY;
        }

        if (m_isUninstantiatedGeneric)
        {
            m_symbolType->GetFlags() |= SYMBOL_TYPE_FLAGS_UNINSTANTIATED_GENERIC;
        }
    }

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

                m_symbolType->AddMember(SymbolTypeMember {
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

                m_symbolType->AddMember(std::move(member));
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
            RC<AstTypeSpecifier>(new AstTypeSpecifier(
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

                    RC<AstTypeSpecifier> paramTypeSpec(new AstTypeSpecifier(
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
                RC<AstTypeSpecifier>(new AstTypeSpecifier(
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
            RC<AstTypeSpecifier>(new AstTypeSpecifier(
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

    { // create a type ref for the symbol type
        m_typeRef.Reset(new AstTypeRef(
            m_symbolType,
            m_location));

        m_typeRef->Visit(visitor, mod);
    }

    m_isVisited = true;
}

UniquePtr<Buildable> AstClass::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_symbolType != nullptr);
    //    Assert(m_symbolType->GetId() != -1);

    Assert(m_isVisited);

    if (m_isProxyClass)
    {
        return nullptr; // nothing to build for proxy classes
    }

    // Assert(!m_isUninstantiatedGeneric, "Cannot build an uninstantiated generic type.");

    Array<HypField*> fields;
    Array<HypMethod*> methods;
    Array<HypConstant*> constants;
    Array<HypProperty*> properties;

    SizeType fieldOffset = sizeof(HypObjectBase); // start after base object
    const SizeType fieldAlignment = alignof(HypData);

    // iterate through parent types and add up their fields to get offset
    SymbolTypeRef baseTypeRef = m_symbolType->GetBaseType();

    while (baseTypeRef != nullptr && baseTypeRef != BuiltinTypes::OBJECT)
    {
        for (const SymbolTypeMember& member : baseTypeRef->GetMembers())
        {
            if (!member.type->IsOrHasBase(*BuiltinTypes::FUNCTION_BASE))
            {
                // align field offset
                fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypData));
                fieldOffset += sizeof(HypData);
            }
        }

        baseTypeRef = baseTypeRef->GetBaseType();
    }

    // Add all fields from our class
    for (const SymbolTypeMember& member : m_symbolType->GetMembers())
    {
        if (member.type->IsOrHasBase(*BuiltinTypes::FUNCTION_BASE)) // method
        {
            HypMethod* method = new HypMethod(
                CreateNameFromDynamicString(*member.name),
                TypeId::Void(),
                TypeId::Void(),
                HypMethodFlags::MEMBER,
                {});

            methods.PushBack(method);
        }
        else
        {
            // align field offset
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypData));

            HypField* field = new HypField(
                CreateNameFromDynamicString(member.name),
                TypeId::ForType<HypData>(),
                TypeId::Void(),
                fieldOffset,
                sizeof(HypData),
                {});

            fields.PushBack(field);

            fieldOffset += sizeof(HypData);
        }
    }

    // @TODO: static members

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // Assert(m_typeObject != nullptr);
    // chunk->Append(m_typeObject->Build(visitor, mod));

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbolType->GetName() + (m_isProxyClass ? " <Proxy>" : "")));

    baseTypeRef = m_symbolType->GetBaseType();

    if (baseTypeRef != nullptr && baseTypeRef != BuiltinTypes::OBJECT)
    {
        chunk->Append(BytecodeUtil::Make<Comment>("Base type: " + baseTypeRef->GetName()));

        chunk->Append(BytecodeUtil::Make<LoadClass>(rp, HashCode::GetHashCode(baseTypeRef->GetName().Data()).Value()));
    }
    else
    {
        chunk->Append(BytecodeUtil::Make<ConstNull>(rp));
    }

    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // store object's register location
    const uint8 objReg = rp;

    { // load the type into register objReg
        auto instrType = BytecodeUtil::Make<BuildableType>();
        instrType->reg = objReg;
        instrType->name = m_symbolType->GetName();
        instrType->fields = std::move(fields);
        instrType->methods = std::move(methods);
        instrType->constants = std::move(constants);
        instrType->properties = std::move(properties);

        chunk->Append(std::move(instrType));
    }

#if 0 // come back to this
    if (m_memberExpressions.Any())
    {
        // push the class to the stack
        const int classStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

        // use above as self arg so PUSH
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        { // add instruction to store on stack
            auto instrPush = BytecodeUtil::Make<RawOperation<>>();
            instrPush->opcode = PUSH;
            instrPush->Accept<uint8>(rp);
            chunk->Append(std::move(instrPush));
        }

        // increment stack size for class
        visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

        for (SizeType index = 0; index < m_memberExpressions.Size(); index++)
        {
            Assert(index < MathUtil::MaxSafeValue<uint8>(), "Argument out of bouds of max arguments");

            const RC<AstExpression>& mem = m_memberExpressions[index];
            Assert(mem != nullptr);

            chunk->Append(mem->Build(visitor, mod));

            // increment to not overwrite object in register with out class
            rp = visitor->GetCompilationUnit()->GetInstructionStream().IncRegisterUsage();

            const int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

            { // load class from stack into register
                auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
                instrLoadOffset->GetBuilder().Load(rp).Local().ByOffset(stackSize - classStackLocation);
                chunk->Append(std::move(instrLoadOffset));
            }

            { // store data member
                auto instrMovMem = BytecodeUtil::Make<RawOperation<>>();
                instrMovMem->opcode = MOV_MEM;
                instrMovMem->Accept<uint8>(rp);
                instrMovMem->Accept<uint8>(uint8(index));
                instrMovMem->Accept<uint8>(objReg);
                chunk->Append(std::move(instrMovMem));
            }

            // no longer using obj in reg
            rp = visitor->GetCompilationUnit()->GetInstructionStream().DecRegisterUsage();

            { // comment for debug
                chunk->Append(BytecodeUtil::Make<Comment>("Store member " + m_symbolType->GetMembers()[index].name));
            }
        }

        // "return" our class to the last used register before popping
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        const int stackSize = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

        { // load class from stack into register so it is back in rp
            auto instrLoadOffset = BytecodeUtil::Make<StorageOperation>();
            instrLoadOffset->GetBuilder()
                .Load(rp)
                .Local()
                .ByOffset(stackSize - classStackLocation);

            chunk->Append(std::move(instrLoadOffset));
        }

        // pop class off stack
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        chunk->Append(Compiler::PopStack(visitor, 1));
    }
    else
    {
        Assert(objReg == visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister());
    }
#endif

    // { // store class in static table
    //     chunk->Append(BytecodeUtil::Make<Comment>("Store class " + m_symbolType->GetName() + " in static data at index " + String::ToString(m_symbolType->GetId())));

    //     auto instrStoreStatic = BytecodeUtil::Make<StorageOperation>();
    //     instrStoreStatic->GetBuilder().Store(rp).Static().ByIndex(m_symbolType->GetId());
    //     chunk->Append(std::move(instrStoreStatic));
    // }

    chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbolType->GetName()));

    // rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // // Load it from static storage
    // auto instrLoadStatic = BytecodeUtil::Make<StorageOperation>();
    // instrLoadStatic->GetBuilder().Load(rp).Static().ByIndex(m_symbolType->GetId());
    // chunk->Append(std::move(instrLoadStatic));

    // Assert(m_typeRef != nullptr);
    // chunk->Append(m_typeRef->Build(visitor, mod));

    return chunk;
}

void AstClass::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_isVisited);

    Assert(m_typeRef != nullptr);
    m_typeRef->Optimize(visitor, mod);
}

RC<AstStatement> AstClass::Clone() const
{
    return CloneImpl();
}

bool AstClass::IsLiteral() const
{
    return false;
}

Tribool AstClass::IsTrue() const
{
    return Tribool::True();
}

bool AstClass::MayHaveSideEffects() const
{
    return true;
}

SymbolTypeRef AstClass::GetExprType() const
{
    return BuiltinTypes::CLASS_TYPE;
}

SymbolTypeRef AstClass::GetHeldType() const
{
    Assert(m_symbolType != nullptr);

    return m_symbolType;
}

const AstExpression* AstClass::GetValueOf() const
{
    Assert(m_isVisited);
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetValueOf();
}

const AstExpression* AstClass::GetDeepValueOf() const
{
    Assert(m_isVisited);
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetDeepValueOf();
}

const String& AstClass::GetName() const
{
    return m_name;
}

} // namespace hyperion::compiler
