#include <script/ScriptApi.hpp>
#include <script/ScriptApi2.hpp>

#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/TokenStream.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>

#include <script/vm/VM.hpp>

#include <core/object/HypData.hpp>

#include <core/memory/ByteBuffer.hpp>

namespace hyperion {
namespace scriptapi2 {

// ClassBuilder

ClassBuilder::ClassBuilder(Context* context, ClassDefinition classDefinition)
    : m_context(context),
      m_classDefinition(std::move(classDefinition))
{
    Assert(m_context != nullptr);
}

ClassBuilder& ClassBuilder::Member(String name, String typeString, HypData&& value)
{
    m_classDefinition.members.PushBack(Symbol { name, typeString, Value(std::move(value)) });

    return *this;
}

ClassBuilder& ClassBuilder::Method(String name, String typeString, Script_NativeFunction fn)
{
    Script_VMData vmData;
    vmData.type = Script_VMData::NATIVE_FUNCTION;
    vmData.nativeFunc = fn;

    m_classDefinition.members.PushBack(Symbol { name, typeString, Value(vmData) });

    return *this;
}

ClassBuilder& ClassBuilder::StaticMember(String name, String typeString, HypData&& value)
{
    m_classDefinition.staticMembers.PushBack(Symbol { name, typeString, Value(std::move(value)) });

    return *this;
}

ClassBuilder& ClassBuilder::StaticMethod(String name, String typeString, Script_NativeFunction fn)
{
    Script_VMData vmData;
    vmData.type = Script_VMData::NATIVE_FUNCTION;
    vmData.nativeFunc = fn;

    m_classDefinition.staticMembers.PushBack(Symbol { name, typeString, Value(vmData) });

    return *this;
}

void ClassBuilder::Build()
{
    Mutex::Guard guard(m_context->m_mutex);

    // Add `nativeTypeId` member to class
    m_classDefinition.staticMembers.PushBack({ "nativeTypeId", { "uint" }, Value(HypData(m_classDefinition.nativeTypeId.Value())) });
    m_context->m_classDefinitions.PushBack(std::move(m_classDefinition));
}

// Context

Context& Context::Global(String name, String typeString, HypData&& value)
{
    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, Value(std::move(value)) } });

    return *this;
}

Context& Context::Global(String name, String genericParamsString, String typeString, HypData&& value)
{
    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, Value(std::move(value)) }, std::move(genericParamsString) });

    return *this;
}

Context& Context::Global(String name, String typeString, Script_NativeFunction fn)
{
    Script_VMData vmData;
    vmData.type = Script_VMData::NATIVE_FUNCTION;
    vmData.nativeFunc = fn;

    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, Value(vmData) } });

    return *this;
}

Context& Context::Global(String name, String genericParamsString, String typeString, Script_NativeFunction fn)
{
    Script_VMData vmData;
    vmData.type = Script_VMData::NATIVE_FUNCTION;
    vmData.nativeFunc = fn;

    Mutex::Guard guard(m_mutex);

    m_globals.PushBack(GlobalDefinition { Symbol { name, typeString, Value(vmData) }, std::move(genericParamsString) });

    return *this;
}

RC<AstExpression> Context::ParseTypeExpression(const String& typeString)
{
    AstIterator astIterator;

    SourceFile sourceFile(SourceLocation::eof.GetFileName(), typeString.Size() + 1);

    ByteBuffer temp(typeString.Size() + 1, typeString.Data());
    sourceFile.ReadIntoBuffer(temp);

    // use the lexer and parser on this file buffer
    TokenStream tokenStream(TokenStreamInfo { SourceLocation::eof.GetFileName() });

    CompilationUnit compilationUnit;

    Lexer lexer(SourceStream(&sourceFile), &tokenStream, &compilationUnit);
    lexer.Analyze();

    Parser parser(&astIterator, &tokenStream, &compilationUnit);

    RC<AstPrototypeSpecification> typeSpec = parser.ParsePrototypeSpecification();

    Assert(!compilationUnit.GetErrorList().HasFatalErrors(),
        "Failed to parse type expression: {}", typeString.Data());

    return typeSpec;
}

Array<RC<AstParameter>> Context::ParseGenericParams(const String& genericParamsString)
{
    AstIterator astIterator;

    SourceFile sourceFile(SourceLocation::eof.GetFileName(),
        genericParamsString.Size() + 1);

    ByteBuffer temp(genericParamsString.Size() + 1,
        genericParamsString.Data());
    sourceFile.ReadIntoBuffer(temp);

    // use the lexer and parser on this file buffer
    TokenStream tokenStream(TokenStreamInfo { SourceLocation::eof.GetFileName() });

    CompilationUnit compilationUnit;

    Lexer lexer(SourceStream(&sourceFile), &tokenStream, &compilationUnit);
    lexer.Analyze();

    Parser parser(&astIterator, &tokenStream, &compilationUnit);

    Array<RC<AstParameter>> genericParams = parser.ParseGenericParameters();

    Assert(!compilationUnit.GetErrorList().HasFatalErrors(),
        "Failed to parse generic parameters: {}",
        genericParamsString.Data());

    return genericParams;
}

void Context::Visit(AstVisitor* visitor, CompilationUnit* compilationUnit)
{
    Mutex::Guard guard(m_mutex);

    for (GlobalDefinition& global : m_globals)
    {
        IdentifierFlagBits identifierFlags = IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_NATIVE;

        RC<AstPrototypeSpecification> typeSpec =
            ParseTypeExpression(global.symbol.type.typeString)
                .Cast<AstPrototypeSpecification>();
        Assert(typeSpec != nullptr);

        RC<AstExpression> expr(new AstAsExpression(
            RC<AstNil>(new AstNil(SourceLocation::eof)),
            RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                RC<AstTypeRef>(
                    new AstTypeRef(BuiltinTypes::ANY, SourceLocation::eof)),
                SourceLocation::eof)),
            SourceLocation::eof));

        if (global.genericParamsString.HasValue())
        {
            const Array<RC<AstParameter>> genericParams =
                ParseGenericParams(*global.genericParamsString);

            if (genericParams.Any())
            {
                expr.Reset(new AstTemplateExpression(
                    expr, genericParams, typeSpec,
                    AST_TEMPLATE_EXPRESSION_FLAG_NATIVE, SourceLocation::eof));

                identifierFlags |= IdentifierFlags::FLAG_GENERIC;

                typeSpec.Reset(); // reset typeSpec so we don't double-visit it
            }
        }

        global.varDecl.Reset(
            new AstVariableDeclaration(global.symbol.name, typeSpec, expr,
                identifierFlags, SourceLocation::eof));

        visitor->GetAstIterator()->Push(global.varDecl);
    }

    for (ClassDefinition& classDefinition : m_classDefinitions)
    {
        Array<RC<AstVariableDeclaration>> members;
        members.Resize(classDefinition.members.Size());

        Array<RC<AstVariableDeclaration>> staticMembers;
        staticMembers.Resize(classDefinition.staticMembers.Size());

        FixedArray<Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*>, 2>
            memberArrays {
                Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*> {
                    &members, &classDefinition.members },
                Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*> {
                    &staticMembers, &classDefinition.staticMembers }
            };

        for (Pair<Array<RC<AstVariableDeclaration>>*, Array<Symbol>*>& memberArray : memberArrays)
        {
            for (SizeType i = 0; i < memberArray.second->Size(); ++i)
            {
                const Symbol& symbol = (*memberArray.second)[i];

                RC<AstPrototypeSpecification> typeSpec =
                    ParseTypeExpression(symbol.type.typeString)
                        .Cast<AstPrototypeSpecification>();
                Assert(typeSpec != nullptr);

                (*memberArray.first)[i].Reset(new AstVariableDeclaration(
                    symbol.name, typeSpec,
                    RC<AstAsExpression>(new AstAsExpression(
                        RC<AstNil>(new AstNil(SourceLocation::eof)),
                        RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
                            RC<AstTypeRef>(
                                new AstTypeRef(BuiltinTypes::ANY, SourceLocation::eof)),
                            SourceLocation::eof)),
                        SourceLocation::eof)),
                    IdentifierFlags::FLAG_NATIVE, SourceLocation::eof));
            }
        }

        classDefinition.expr.Reset(
            new AstTypeExpression(classDefinition.name, nullptr, members, {},
                staticMembers, false, SourceLocation::eof));

        IdentifierFlagBits identifierFlags =
            IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_NATIVE;

        if (classDefinition.genericParamsString.HasValue())
        {
            const Array<RC<AstParameter>> genericParams =
                ParseGenericParams(*classDefinition.genericParamsString);

            if (genericParams.Any())
            {
                classDefinition.expr.Reset(new AstTemplateExpression(
                    classDefinition.expr, genericParams, nullptr,
                    AST_TEMPLATE_EXPRESSION_FLAG_NATIVE, SourceLocation::eof));

                identifierFlags |= IdentifierFlags::FLAG_GENERIC;
            }
        }

        classDefinition.varDecl.Reset(new AstVariableDeclaration(
            classDefinition.name, nullptr, classDefinition.expr, identifierFlags,
            SourceLocation::eof));

        visitor->GetAstIterator()->Push(classDefinition.varDecl);
    }
}

void Context::BindAll(APIInstance& apiInstance, VM* vm)
{
    Mutex::Guard guard(m_mutex);

    for (GlobalDefinition& global : m_globals)
    {
        Assert(global.varDecl != nullptr);
        Assert(global.varDecl->GetIdentifier() != nullptr);

        const int stackLocation = global.varDecl->GetIdentifier()->GetStackLocation();
        Assert(stackLocation != -1, "Global {} has no stack location", global.symbol.name.Data());

        Script_VMData vmData;
        vmData.type = Script_VMData::VALUE_REF;
        vmData.valueRef = global.symbol.value.Deref();

        Assert(vm->GetMainThread()->GetStack().STACK_SIZE > stackLocation);
        vm->GetMainThread()->GetStack().GetData()[stackLocation].AssignValue(Value(vmData), false);
    }

#if 0 // TODO: fix
    for (ClassDefinition& classDefinition : m_classDefinitions)
    {
        Assert(classDefinition.expr != nullptr);

        Assert(classDefinition.varDecl != nullptr);
        Assert(classDefinition.varDecl->GetIdentifier() != nullptr);

        const int stackLocation = classDefinition.varDecl->GetIdentifier()->GetStackLocation();
        Assert(stackLocation != -1, "Class {} has no stack location", classDefinition.name.Data());

        // Ensure class SymbolType is registered
        SymbolTypeRef heldType = classDefinition.expr->GetHeldType();
        Assert(heldType != nullptr);
        heldType = heldType->GetUnaliased();

        Assert(heldType->GetId() != -1, "Class {} has no ID", classDefinition.name.Data());

        // Load the class object from the VM - it is stored in StaticMemory
        // at the index

        const int index = heldType->GetId();
        Assert(vm->m_staticMemory.staticSize > index);

        Array<Member> classObjectMembers;
        classObjectMembers.Resize(heldType->GetMembers().Size());

        for (SizeType i = 0; i < heldType->GetMembers().Size(); ++i)
        {
            SymbolTypeMember& member = heldType->GetMembers()[i];

            auto symbolIt = classDefinition.staticMembers.FindIf(
                [&member](const Symbol& symbol)
                {
                    return symbol.name == member.name;
                });

            if (symbolIt == classDefinition.staticMembers.End())
            {
                continue;
            }

            Symbol& symbol = *symbolIt;

            Script_VMData vmData;
            vmData.type = Script_VMData::VALUE_REF;
            vmData.valueRef = symbol.value.Deref();

            Memory::StrCpy(classObjectMembers[i].name, symbol.name.Data(), MathUtil::Min(symbol.name.Size(), 255));
            classObjectMembers[i].hash = HashCode::GetHashCode(classObjectMembers[i].name).Value();
            classObjectMembers[i].value = Value(vmData);
        }

        VMObject classObject(classObjectMembers.Data(), classObjectMembers.Size(), Value());

        Array<Member> protoObjectMembers;
        protoObjectMembers.Resize(classDefinition.members.Size());

        for (SizeType i = 0; i < classDefinition.members.Size(); ++i)
        {
            Symbol& symbol = classDefinition.members[i];

            Script_VMData vmData;
            vmData.type = Script_VMData::VALUE_REF;
            vmData.valueRef = symbol.value.Deref();

            Memory::StrCpy(protoObjectMembers[i].name, symbol.name.Data(), MathUtil::Min(symbol.name.Size(), 255));
            protoObjectMembers[i].hash = HashCode::GetHashCode(protoObjectMembers[i].name).Value();
            protoObjectMembers[i].value = Value(vmData);
        }

        // Set class object in static memory
        Value& classObjectValue = vm->m_staticMemory[index];
        classObjectValue.AssignValue(Value(HypData(std::move(classObject))), false);

        // Add __intern member
        protoObjectMembers.PushBack(Member { "__intern", hashFnv1("__intern"), Value() });

        Script_VMData classObjectRefVmData;
        classObjectRefVmData.type = Script_VMData::VALUE_REF;
        classObjectRefVmData.valueRef = &classObjectValue;

        VMObject protoObject(protoObjectMembers.Data(), protoObjectMembers.Size(), Value(classObjectRefVmData));

        // Set $proto for class object
        VMObject* classObjectPtr = classObjectValue.GetObject();
        Assert(classObjectPtr != nullptr);

        // add $proto member to class object
        classObjectPtr->SetMember("$proto", Value(HypData(std::move(protoObject))));

        VMObject* protoObjectPtr = classObjectValue.GetObject()->LookupMemberFromHash(VMObject::PROTO_MEMBER_HASH)->value.GetObject();
        Assert(protoObjectPtr != nullptr);

        apiInstance.classBindings.classPrototypes.Set(classDefinition.name, protoObjectPtr);
        apiInstance.classBindings.classNames.Set(classDefinition.nativeTypeId, classDefinition.name);

        // add a reference to the class object in the global scope (it's stored in static memory)
        Value value;
        {
            Script_VMData vmData;
            vmData.type = Script_VMData::VALUE_REF;
            vmData.valueRef = &classObjectValue;
            value = Value(vmData);
        }

        // Set class object in global scope
        Assert(vm->GetMainThread()->GetStack().STACK_SIZE > stackLocation);
        vm->GetMainThread()->GetStack().GetData()[stackLocation].AssignValue(std::move(value), false);

        DebugLog(LogType::Debug, "Bound class %s at stack location %u\n", classDefinition.name.Data(), stackLocation);
    }
#endif
}

} // namespace scriptapi2
} // namespace hyperion
