#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Optimizer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <script/compiler/emit/StorageOperation.hpp>

#include <Core/reflection/Object.hpp>
#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/Method.hpp>

#include <Core/containers/HashMap.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Format.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

static const String s_reservedClassMemberNames[] = {
    "$construct"
};

AstClass::AstClass(
    const String& name,
    const RC<AstTypeSpecifier>& baseSpec,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    EnumFlags<AstClassFlags> flags,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_name(name),
      m_baseType(nullptr),
      m_baseSpec(baseSpec),
      m_dataMembers(dataMembers),
      m_functionMembers(functionMembers),
      m_staticMembers(staticMembers),
      m_flags(flags),
      m_preRegister(false),
      m_symbolType(nullptr)
{
}

AstClass::AstClass(
    const String& name,
    const SymbolType* baseType,
    const Array<RC<AstVariableDeclaration>>& dataMembers,
    const Array<RC<AstVariableDeclaration>>& functionMembers,
    const Array<RC<AstVariableDeclaration>>& staticMembers,
    EnumFlags<AstClassFlags> flags,
    const SourceLocation& location)
    : AstClass(
          name,
          RC<AstTypeSpecifier>(),
          dataMembers,
          functionMembers,
          staticMembers,
          flags,
          location)
{
    m_baseType = baseType;
}

void AstClass::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr && mod != nullptr);

    // classes can only be defined at global scope (except for anonymous classes, as they don't register a type at runtime)
    if (!mod->IsInGlobalScope() && !(m_flags & CLASS_FLAG_ANONYMOUS))
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_classes_may_only_be_defined_in_global_scope,
            m_location,
            m_name));

        return;
    }

    // Create scope
    ScopeGuard scopeGuard(mod, SCOPE_TYPE_CLASS_DEFINITION, IsExternClass() ? EXTERN_CLASS_FLAG : 0);

    if (m_baseSpec != nullptr)
    {
        m_baseSpec->Visit(visitor, mod);

        if (const SymbolType* baseTypeInner = m_baseSpec->GetHeldType())
        {
            m_baseType = baseTypeInner;
        }
    }

    SymbolType* newType = nullptr;

    if (IsEnum())
    {
        if (!m_baseType)
        {
            m_baseType = BuiltinTypes::s_int32Type;
        }

        if (!m_baseType->IsIntegral())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_enum_underlying_type_must_be_integer,
                m_location,
                m_baseType->ToString()));

            m_baseType = BuiltinTypes::s_int32Type;
        }

        // Create a generic instance of the enum type
        newType = SymbolType::Enum(
            m_name,
            m_baseType,
            {});
    }
    else if (IsStruct())
    {
        if (m_baseType != nullptr)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_struct_cannot_have_base,
                m_location));
        }

        newType = SymbolType::Struct(
            m_name,
            {}, {});
    }
    else // Class
    {
        if (m_baseType != nullptr)
        {
            if (!m_baseType->IsOrHasBase(*BuiltinTypes::s_objectType))
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_invalid_base_class,
                    m_location,
                    m_baseType->ToString()));

                m_baseType = BuiltinTypes::s_objectType;
            }

            newType = SymbolType::Extend(
                m_name,
                m_baseType,
                {}, {});
        }
        else
        {
            newType = SymbolType::Class(
                m_name,
                BuiltinTypes::s_objectType,
                {}, {});
        }

        if (IsProxyClass())
        {
            newType->GetFlags() |= STF_PROXY;
        }
    }

    Assert(newType != nullptr);

    if (m_symbolType)
    {
        m_symbolType->Assign(std::move(*newType));

        delete newType;
    }
    else
    {
        m_symbolType = newType;
    }

    newType = nullptr;

    const Array<RC<AstVariableDeclaration>>* allMembers[] = {
        &m_dataMembers,
        &m_functionMembers,
        &m_staticMembers
    };

    for (const String& reserved : s_reservedClassMemberNames)
    {
        for (const Array<RC<AstVariableDeclaration>>* members : allMembers)
        {
            for (const RC<AstVariableDeclaration>& member : *members)
            {
                if (member->GetIdentifierFlags() & IdentifierFlags::LAX)
                {
                    continue;
                }

                if (member != nullptr && member->GetName() == reserved)
                {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_reserved_identifier,
                        member->GetLocation(),
                        member->GetName()));
                }
            }
        }
    }

    m_symbolType->Register(visitor->GetCompilationUnit());
    mod->scopeTree.Root().identifierTable.AddSymbolType(m_symbolType);

    SymbolType* selfAliasType = SymbolType::Alias("SelfType", { m_symbolType });
    selfAliasType->Register(visitor->GetCompilationUnit());
    mod->scopeTree.Top().identifierTable.AddSymbolType(selfAliasType);

    if (IsProxyClass())
    {
        // Proxy classes cannot have data members or function members
        if (!m_dataMembers.Empty())
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_proxy_class_may_only_contain_methods,
                m_dataMembers.Front()->GetLocation()));
        }
    }
    else
    {
        ConstantValue nextEnumValue(INVALID_CONSTANT_NUMBER);

        if (m_baseType != nullptr)
        {
            if (m_baseType->IsUnsignedIntegral())
            {
                nextEnumValue = ConstantValue { uint64(0), m_baseType->GetConstantBitSize() };
            }
            else
            {
                nextEnumValue = ConstantValue { int64(0), m_baseType->GetConstantBitSize() };
            }
        }

        HashMap<uint64, Array<String>> revEnumMembers;

        // ===== STATIC DATA MEMBERS ======
        {
            for (const RC<AstVariableDeclaration>& decl : m_staticMembers)
            {
                if (!decl)
                {
                    continue;
                }

                if (!m_preRegister && IsEnum())
                {
                    Assert(m_baseType != nullptr);

                    if (!decl->GetAssignment())
                    {
                        // auto assign the next value
                        if (m_baseType->IsUnsignedIntegral())
                        {
                            decl->SetAssignment(RC<AstExpression>(new AstUnsignedInteger(
                                nextEnumValue.AsUInt(),
                                nextEnumValue.bitSize,
                                decl->GetLocation())));
                        }
                        else
                        {
                            decl->SetAssignment(RC<AstExpression>(new AstInteger(
                                nextEnumValue.AsInt(),
                                nextEnumValue.bitSize,
                                decl->GetLocation())));
                        }
                    }
                }

                if (IsExternClass())
                {
                    decl->ApplyIdentifierFlags(IdentifierFlags::EXTERN);
                }

                decl->ApplyIdentifierFlags(IdentifierFlags::PREREGISTER, m_preRegister);
                decl->Visit(visitor, mod);

                // Update the next enum value
                if (!m_preRegister && IsEnum())
                {
                    const RC<AstExpression>& realAssignment = decl->GetRealAssignment();
                    Assert(realAssignment != nullptr);

                    // so we can pre-evaluate the constant value
                    realAssignment->Optimize(visitor, mod);

                    Assert(m_baseType != nullptr);

                    const ConstantValue constantValue = realAssignment->GetConstantValue();

                    if (!constantValue.IsValid())
                    {
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_enum_assignment_not_constant,
                            realAssignment->GetLocation(),
                            decl->GetName()));

                        continue;
                    }

                    if (m_baseType->IsUnsignedIntegral())
                    {
                        nextEnumValue = ConstantValue(constantValue.AsUInt() + 1, nextEnumValue.bitSize);

                        revEnumMembers[constantValue.AsUInt()].PushBack(decl->GetName());
                    }
                    else
                    {
                        nextEnumValue = ConstantValue(constantValue.AsInt() + 1, nextEnumValue.bitSize);

                        revEnumMembers[std::bit_cast<uint64>(constantValue.AsInt())].PushBack(decl->GetName());
                    }
                }

                const String& memberName = decl->GetName();

                Assert(decl->GetIdentifier() != nullptr);

                const SymbolType* memberType = decl->GetIdentifier()->GetSymbolType();
                Assert(memberType != nullptr);

                m_symbolType->GetStaticMembers().PushBack(SymbolTypeMember {
                    memberName,
                    const_cast<SymbolType*>(memberType),
                    decl->GetRealAssignment() });
            }
        }
#if 0
        if (!m_preRegister && IsEnum())
        {
            for (const auto& [value, names] : revEnumMembers)
            {
                if (names.Size() <= 1)
                {
                    continue;
                }

                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_enum_value_already_used,
                    m_location,
                    m_name,
                    String::ToString(m_baseType->IsUnsignedIntegral() ? value : std::bit_cast<int64>(value)),
                    String::Join(names, ", ")));
            }
        }
#endif

        // won't be needing this anymore
        revEnumMembers.Clear();

        // ===== INSTANCE DATA MEMBERS =====

        // open the scope for data members
        {
            // Do data members first so we can use them all in functions.

            for (SizeType i = 0; i < m_dataMembers.Size(); i++)
            {
                const RC<AstVariableDeclaration>& decl = m_dataMembers[i];
                Assert(decl != nullptr);

                if (IsExternClass())
                {
                    decl->ApplyIdentifierFlags(IdentifierFlags::EXTERN);
                }

                decl->ApplyIdentifierFlags(IdentifierFlags::PREREGISTER, m_preRegister);

                decl->Visit(visitor, mod);

                const String& memberName = decl->GetName();

                Assert(decl->GetIdentifier() != nullptr);

                const SymbolType* memberType = decl->GetIdentifier()->GetSymbolType();
                Assert(memberType != nullptr);

                m_symbolType->GetMembers().PushBack(SymbolTypeMember {
                    memberName,
                    const_cast<SymbolType*>(memberType),
                    decl->GetRealAssignment() });
            }

            for (SizeType i = 0; i < m_functionMembers.Size(); i++)
            {
                const RC<AstVariableDeclaration>& decl = m_functionMembers[i];
                Assert(decl != nullptr);

                if (IsExternClass())
                {
                    decl->ApplyIdentifierFlags(IdentifierFlags::EXTERN);
                }

                decl->ApplyIdentifierFlags(IdentifierFlags::PREREGISTER, m_preRegister);

                decl->Visit(visitor, mod);

                const String& memberName = decl->GetName();

                Assert(decl->GetIdentifier() != nullptr);

                const SymbolType* memberType = decl->GetIdentifier()->GetSymbolType();
                Assert(memberType != nullptr);

                SymbolTypeMember member {
                    memberName,
                    const_cast<SymbolType*>(memberType),
                    decl->GetRealAssignment()
                };

                const bool isStaticMethod = decl->GetIdentifierFlags() & IdentifierFlags::STATIC_MEMBER;

                if (isStaticMethod)
                {
                    m_symbolType->GetStaticMembers().PushBack(std::move(member));
                }
                else
                {
                    m_symbolType->GetMembers().PushBack(std::move(member));
                }
            }

            if (!m_preRegister && !IsExternClass() && !IsProxyClass() && !IsEnum())
            {
                // add a $construct member. It'll need to call the user-defined constructor (if any).
                // user-defined constructor is a function member with the same name as the class

                /// \todo : Call base class constructor if applicable
                Optional<SymbolTypeMember> userDefinedConstructorMember;
                for (const SymbolTypeMember& member : m_symbolType->GetMembers())
                {
                    if (member.GetName() == m_name)
                    {
                        userDefinedConstructorMember = member;
                        break;
                    }
                }

                SymbolType* constructorSelfTypePlaceholder = SymbolType::Placeholder("SelfType");
                constructorSelfTypePlaceholder->Register(visitor->GetCompilationUnit());

                // add constructor
                Array<RC<AstParameter>> constructorParams;

                // add `self: typeof SelfType` parameter
                constructorParams.PushBack(RC<AstParameter>(new AstParameter(
                    "self",
                    RC<AstTypeSpecifier>(new AstTypeSpecifier(
                        RC<AstTypeRef>(new AstTypeRef(constructorSelfTypePlaceholder, m_location)),
                        m_location)),
                    nullptr,
                    false, /* variadic */
                    IdentifierFlags::CONSTANT,
                    m_location)));

                RC<AstBlock> constructorBody(new AstBlock(m_location));
                // initialize data members to default values
                for (const RC<AstVariableDeclaration>& dataMember : m_dataMembers)
                {
                    Assert(dataMember != nullptr);

                    if (dataMember->GetRealAssignment() != nullptr)
                    {
                        RC<AstExpression> expr(new AstBinaryExpression(
                            RC<AstMember>(new AstMember(
                                dataMember->GetName(),
                                RC<AstVariable>(new AstVariable("self", m_location)),
                                m_location)),
                            CloneAstNode(dataMember->GetRealAssignment()),
                            Operator::FindBinaryOperator(Operators::OP_assign),
                            m_location));

                        constructorBody->AddChild(expr);
                    }
                }

                // If there is a user-defined constructor, call it with the parameters passed to this constructor
                if (userDefinedConstructorMember.HasValue())
                {
                    const SymbolTypeMember& userDefinedConstructorMemberRef = userDefinedConstructorMember.Get();
                    Assert(userDefinedConstructorMemberRef.GetType() != nullptr);

                    Array<RC<AstArgument>> constructorArgs;
                    constructorArgs.Reserve(constructorParams.Size());

                    // Pass each parameter as an argument to the constructor
                    for (SizeType i = 0; i < constructorParams.Size(); i++)
                    {
                        const RC<AstParameter>& param = constructorParams[i];
                        Assert(param != nullptr);

                        constructorArgs.PushBack(RC<AstArgument>(new AstArgument(
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

                    constructorBody->AddChild(RC<AstExpression>(new AstCallExpression(
                        RC<AstMember>(new AstMember(
                            userDefinedConstructorMemberRef.GetName(),
                            RC<AstVariable>(new AstVariable("self", m_location)),
                            m_location)),
                        constructorArgs,
                        false, /* insertSelf */
                        m_location)));
                }

                // add return self to the constructor function body
                constructorBody->AddChild(RC<AstReturnStatement>(new AstReturnStatement(
                    RC<AstVariable>(new AstVariable("self", m_location)),
                    m_location)));

                RC<AstFunctionExpression> constructorExpr(new AstFunctionExpression(
                    constructorParams,
                    nullptr,
                    constructorBody,
                    m_location));

                // add the constructor expression to the AST
                RC<AstVariableDeclaration>& constructMemberDecl = m_functionMembers.EmplaceBack(new AstVariableDeclaration(
                    "$construct",
                    nullptr,
                    constructorExpr,
                    IdentifierFlags::CONSTANT | IdentifierFlags::LAX,
                    m_location));

                constructMemberDecl->Visit(visitor, mod);

                const SymbolType* constructorMemberType = constructorExpr->GetExprType();

                SymbolTypeMember constructorMember {
                    "$construct",
                    const_cast<SymbolType*>(constructorMemberType),
                    CloneAstNode(constructorExpr)
                };

                m_symbolType->GetMembers().PushBack(constructorMember);

                constructorExpr.Reset(); // release ref
            }
        }
    }

    if (!m_preRegister)
    {
        // resolve placeholders in members
        const SymbolType* resolvedType = SemanticAnalyzer::Helpers::ResolvePlaceholderType(visitor, mod, m_symbolType, m_location);
        Assert(resolvedType != nullptr);
        resolvedType->Register(visitor->GetCompilationUnit());

        m_symbolType->Assign(*resolvedType);
    }

    { // create a type ref for the symbol type
        m_typeRef.Reset(new AstTypeRef(m_symbolType, m_location));
        m_typeRef->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstClass::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_symbolType != nullptr);

    if (IsProxyClass() || IsExternClass())
    {
        // add a comment indicating that this is a proxy or extern class, nothing else to build
        const String classTypeStr = IsProxyClass() ? " <Proxy>" : (IsExternClass() ? " <Extern>" : "");

        UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();
        chunk->Append(BytecodeUtil::Make<Comment>("Void class table for " + m_symbolType->GetName() + classTypeStr));

        return chunk;
    }

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_DEFAULT);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Array<ClassTable::StaticFieldInfo> staticFields;

    for (const RC<AstVariableDeclaration>& decl : m_staticMembers)
    {
        Assert(decl != nullptr);

        ClassTable::StaticFieldInfo staticFieldInfo {};
        staticFieldInfo.name = decl->GetName();
        staticFieldInfo.typeId = TypeId::ForType<BoxedValue>();
        staticFieldInfo.targetTypeId = TypeId::ForType<ObjectBase>();

        chunk->Append(decl->Build(visitor, mod));

        staticFieldInfo.stackOffset = uint16(m_staticMembers.Size() - staticFields.Size()); // reverse order because stack
        staticFields.PushBack(staticFieldInfo);
    }

    // Build methods and store their values on the stack.
    // Then, pass those stack locations to the ClassTable instruction,
    // so it can set up the class properly.
    Array<ClassTable::MethodInfo> methods;

    // Add all fields from our class
    for (SizeType functionMemberIndex = 0; functionMemberIndex < m_functionMembers.Size(); functionMemberIndex++)
    {
        const RC<AstVariableDeclaration>& decl = m_functionMembers[functionMemberIndex];
        Assert(decl != nullptr);

        ClassTable::MethodInfo methodInfo {};
        methodInfo.name = decl->GetName();
        methodInfo.typeId = TypeId::ForType<BoxedValue>();
        methodInfo.targetTypeId = TypeId::ForType<ObjectBase>();

        // flags will be combined with the function's other flags (e.g VARIDIC) with the member is created during execution
        if (decl->GetIdentifierFlags() & IdentifierFlags::STATIC_MEMBER)
        {
            // static method
            methodInfo.flags = MethodFlags::STATIC;
        }
        else
        {
            // instance method
            methodInfo.flags = MethodFlags::MEMBER;
        }

        chunk->Append(decl->Build(visitor, mod));

        methodInfo.stackOffset = uint16(m_functionMembers.Size() - functionMemberIndex); // reverse order because stack
        methods.PushBack(methodInfo);
    }

    // store each method on the stack

    Array<ClassTable::FieldInfo> fields;

    SizeType fieldOffset = sizeof(ObjectBase); // start after base object

    // reserve space for `class` field to hold reference to the class this object is an instance of
    fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(ClassRef));
    fieldOffset += sizeof(ClassRef);

    if (!IsEnum())
    {
        // iterate through parent types and add up their fields to get offset
        const SymbolType* base = m_symbolType->GetBaseType();

        while (base != nullptr && !base->TypeEqual(*BuiltinTypes::s_objectType))
        {
            for (const SymbolTypeMember& member : base->GetMembers())
            {
                Assert(member.GetType() != nullptr);

                if (!member.GetType()->IsOrHasBase(*BuiltinTypes::s_functionBaseType)) // skip methods, they don't take up space on the instance
                {
                    // align field offset
                    fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(BoxedValue));
                    fieldOffset += sizeof(BoxedValue);
                }
            }

            base = base->GetBaseType();
        }
    }

    // Add all fields from our class
    for (const SymbolTypeMember& member : m_symbolType->GetMembers())
    {
        Assert(member.GetType() != nullptr);

        if (!member.GetType()->IsOrHasBase(*BuiltinTypes::s_functionBaseType)) // skip methods, they are handled above
        {
            // align field offset
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(BoxedValue));

            ClassTable::FieldInfo fieldInfo {};
            fieldInfo.name = member.GetName();
            fieldInfo.typeId = TypeId::ForType<BoxedValue>();
            fieldInfo.targetTypeId = TypeId::ForType<ObjectBase>();
            fieldInfo.offset = uint32(fieldOffset);
            fieldInfo.size = uint32(sizeof(BoxedValue));

            fields.PushBack(fieldInfo);

            fieldOffset += sizeof(BoxedValue);
        }
    }

    // static members

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    if (IsEnum())
    {
        chunk->Append(BytecodeUtil::Make<Comment>("Begin enum " + m_symbolType->GetName()));

        // no base type
        chunk->Append(BytecodeUtil::Make<ConstNull>(rp));
    }
    else
    {
        chunk->Append(BytecodeUtil::Make<Comment>("Begin class " + m_symbolType->GetName() + (IsProxyClass() ? " <Proxy>" : "")));

        const SymbolType* base = m_symbolType->GetBaseType();

        if (base != nullptr && !base->TypeEqual(*BuiltinTypes::s_objectType))
        {
            chunk->Append(BytecodeUtil::Make<Comment>("Base type: " + base->GetName()));

            chunk->Append(BytecodeUtil::Make<LoadClass>(rp, CreateNameFromDynamicString(base->GetName())));
        }
        else
        {
            chunk->Append(BytecodeUtil::Make<Comment>("Base type: <None>"));

            chunk->Append(BytecodeUtil::Make<ConstNull>(rp));
        }
    }

    rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    // store object's register location
    const uint8 objReg = rp;

    { // load the type into register objReg
        auto instrType = BytecodeUtil::Make<ClassTable>();
        instrType->reg = objReg;
        instrType->name = m_symbolType->GetName();
        instrType->staticFields = std::move(staticFields);
        instrType->fields = std::move(fields);
        instrType->methods = std::move(methods);

        instrType->flags = ClassFlags::DYNAMIC; // all classes defined in script are dynamic

        if (IsAnonymous())
        {
            instrType->flags = (ClassFlags)((uint8)instrType->flags | (uint8)ClassFlags::ANONYMOUS);
        }

        if (IsEnum())
        {
            instrType->flags = (ClassFlags)((uint8)instrType->flags | (uint8)ClassFlags::ENUM_TYPE);
        }
        else if (IsStruct())
        {
            instrType->flags = (ClassFlags)((uint8)instrType->flags | (uint8)ClassFlags::STRUCT_TYPE);
        }
        else
        {
            instrType->flags = (ClassFlags)((uint8)instrType->flags | (uint8)ClassFlags::CLASS_TYPE);
        }

        chunk->Append(std::move(instrType));
    }

    // pop anything we pushed for methods
    const int numFunctionMembers = int(m_functionMembers.Size());

    if (numFunctionMembers > 0)
    {
        for (int i = 0; i < numFunctionMembers; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, numFunctionMembers));
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

    if (IsEnum())
    {
        chunk->Append(BytecodeUtil::Make<Comment>("End enum " + m_symbolType->GetName()));
    }
    else
    {
        chunk->Append(BytecodeUtil::Make<Comment>("End class " + m_symbolType->GetName()));
    }

    return chunk;
}

void AstClass::Optimize(AstVisitor* visitor, Module* mod)
{
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

const SymbolType* AstClass::GetExprType() const
{
    return BuiltinTypes::s_classType;
}

const SymbolType* AstClass::GetHeldType() const
{
    return m_symbolType;
}

const AstExpression* AstClass::GetValueOf() const
{
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetValueOf();
}

const AstExpression* AstClass::GetDeepValueOf() const
{
    Assert(m_typeRef != nullptr);

    return m_typeRef->GetDeepValueOf();
}

const String& AstClass::GetName() const
{
    return m_name;
}

} // namespace Hyperion
