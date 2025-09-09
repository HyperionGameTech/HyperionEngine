#include <script/HypScript.hpp>

#include <script/vm/Stream.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/codegen/CodeGenerator.hpp>

#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/dis/DecompilationUnit.hpp>

#include <script/compiler/AstPrintVisitor.hpp>

#include <script/vm/Interpreter.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMemberFwd.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/Format.hpp>

namespace hyperion {

// #define HYP_PRINT_AST

HYP_DEFINE_LOG_CHANNEL(HypScript);

#pragma region HypClass SymbolType Interop

static const TypeId g_typeIdVoid = TypeId::Void();
static const TypeId g_typeIdI8 = TypeId::ForType<int8>();
static const TypeId g_typeIdU8 = TypeId::ForType<uint8>();
static const TypeId g_typeIdI16 = TypeId::ForType<int16>();
static const TypeId g_typeIdU16 = TypeId::ForType<uint16>();
static const TypeId g_typeIdI32 = TypeId::ForType<int32>();
static const TypeId g_typeIdU32 = TypeId::ForType<uint32>();
static const TypeId g_typeIdI64 = TypeId::ForType<int64>();
static const TypeId g_typeIdU64 = TypeId::ForType<uint64>();
static const TypeId g_typeIdF32 = TypeId::ForType<float32>();
static const TypeId g_typeIdF64 = TypeId::ForType<float64>();
static const TypeId g_typeIdBool = TypeId::ForType<bool>();
static const TypeId g_typeIdString = TypeId::ForType<String>();

static HashMap<TypeId, SymbolTypeRef> g_symbolTypeCache;

SymbolTypeRef HypClassToSymbolType(const HypClass* hypClass);

SymbolTypeRef TypeIdToSymbolTypeImpl(TypeId typeId)
{
    static const HashMap<TypeId, SymbolTypeRef> s_builtinTypes = {
        { g_typeIdVoid, BuiltinTypes::s_voidType },
        { g_typeIdI8, BuiltinTypes::s_intType },
        { g_typeIdI16, BuiltinTypes::s_intType },
        { g_typeIdI32, BuiltinTypes::s_intType },
        { g_typeIdI64, BuiltinTypes::s_intType },
        { g_typeIdU8, BuiltinTypes::s_unsignedIntType },
        { g_typeIdU16, BuiltinTypes::s_unsignedIntType },
        { g_typeIdU32, BuiltinTypes::s_unsignedIntType },
        { g_typeIdU64, BuiltinTypes::s_unsignedIntType },
        { g_typeIdF32, BuiltinTypes::s_floatType },
        { g_typeIdF64, BuiltinTypes::s_floatType },
        { g_typeIdBool, BuiltinTypes::s_boolType },
        { g_typeIdString, BuiltinTypes::s_stringType }
    };

    auto it = s_builtinTypes.Find(typeId);
    if (it != s_builtinTypes.End())
    {
        return it->second;
    }

    const HypClass* hypClass = GetClass(typeId);

    if (!hypClass)
    {
        HYP_LOG(HypScript, Warning, "No HypClass found for `{}`", LookupTypeName(typeId));

        return BuiltinTypes::s_errorType;
    }

    return HypClassToSymbolType(hypClass);
}

SymbolTypeRef TypeIdToSymbolType(TypeId typeId)
{
    auto it = g_symbolTypeCache.Find(typeId);

    if (it != g_symbolTypeCache.End())
    {
        return it->second;
    }

    SymbolTypeRef newType = TypeIdToSymbolTypeImpl(typeId);

    g_symbolTypeCache.Insert(typeId, newType);

    return newType;
}

SymbolTypeRef HypClassToSymbolType(const HypClass* hypClass)
{
    if (!hypClass)
    {
        return nullptr;
    }

    auto it = g_symbolTypeCache.Find(hypClass->GetTypeId());
    if (it != g_symbolTypeCache.End())
    {
        return it->second;
    }

    SymbolTypeRef parentType = BuiltinTypes::s_objectType;

    if (hypClass->GetParent() != nullptr)
    {
        parentType = HypClassToSymbolType(hypClass->GetParent());
    }

    // check if it was added by the parent call
    it = g_symbolTypeCache.Find(hypClass->GetTypeId());
    if (it != g_symbolTypeCache.End())
    {
        return it->second;
    }

    HYP_LOG(HypScript, Debug, "Creating SymbolType for HypClass '{}'", *hypClass->GetName());

    SymbolTypeRef newType = SymbolType::Object(
        *hypClass->GetName(),
        parentType,
        {},
        {});

    it = g_symbolTypeCache.Insert(hypClass->GetTypeId(), newType).first;

    for (const HypField* field : hypClass->GetFields())
    {
        HYP_LOG(HypScript, Debug, "Adding field {}", *field->GetName());

        const TypeId fieldTypeId = field->GetTypeId();

        SymbolTypeRef symbolType = TypeIdToSymbolType(fieldTypeId);

        if (!symbolType)
        {
            HYP_LOG(HypScript, Warning, "Failed to get SymbolType for field '{}' of type '{}' in class '{}'",
                *field->GetName(),
                LookupTypeName(fieldTypeId),
                *hypClass->GetName());

            symbolType = BuiltinTypes::s_errorType;
        }

        if (symbolType == BuiltinTypes::s_errorType)
        {
            HYP_LOG(HypScript, Warning, "Field '{}' in class '{}' has error type",
                *field->GetName(),
                *hypClass->GetName());

            continue;
        }

        newType->GetMembers().PushBack(SymbolTypeMember {
            *field->GetName(),
            symbolType });
    }

    for (const HypMethod* method : hypClass->GetMethods())
    {
        HYP_LOG(HypScript, Debug, "Adding method {}", *method->GetName());

        Array<GenericInstanceTypeInfo::Arg> parameters;
        parameters.Reserve(method->GetParameters().Size() + 1);

        SymbolTypeRef symbolType = TypeIdToSymbolType(method->GetTypeId());

        if (!symbolType)
        {
            HYP_LOG(HypScript, Warning, "Failed to get SymbolType for return type '{}' of method '{}' in class '{}'",
                LookupTypeName(method->GetTypeId()),
                *method->GetName(),
                *hypClass->GetName());

            symbolType = BuiltinTypes::s_errorType;
        }

        if (symbolType == BuiltinTypes::s_errorType)
        {
            HYP_LOG(HypScript, Warning, "Method '{}' in class '{}' has error return type",
                *method->GetName(),
                *hypClass->GetName());

            continue;
        }

        // add return type as first parameter with name "@return"
        parameters.PushBack(GenericInstanceTypeInfo::Arg {
            "@return",
            symbolType });

        bool errored = false;

        for (SizeType paramIndex = 0; paramIndex < method->GetParameters().Size(); paramIndex++)
        {
            const HypMethodParameter& param = method->GetParameters()[paramIndex];

            HYP_LOG(HypScript, Debug, "  with param type {}", LookupTypeName(param.typeId));

            symbolType = TypeIdToSymbolType(param.typeId);

            if (!symbolType)
            {
                HYP_LOG(HypScript, Warning, "Failed to get SymbolType for parameter {} of type '{}' in method '{}' in class '{}'",
                    paramIndex,
                    LookupTypeName(param.typeId),
                    *method->GetName(),
                    *hypClass->GetName());

                symbolType = BuiltinTypes::s_errorType;
            }

            if (symbolType == BuiltinTypes::s_errorType)
            {
                HYP_LOG(HypScript, Warning, "Parameter {} in method '{}' in class '{}' has error type",
                    paramIndex,
                    *method->GetName(),
                    *hypClass->GetName());

                errored = true;

                break;
            }

            parameters.PushBack(GenericInstanceTypeInfo::Arg {
                HYP_FORMAT("arg{}", paramIndex),
                symbolType });
        }

        symbolType.Reset();

        if (errored)
        {
            continue;
        }

        newType->GetMembers().PushBack(SymbolTypeMember {
            *method->GetName(),
            SymbolType::GenericInstance(
                BuiltinTypes::s_functionType,
                {}, {},
                GenericInstanceTypeInfo { parameters }) });
    }

    return newType;
}

#pragma endregion HypClass SymbolType Interop

#pragma region HypScriptImpl

struct HypScriptImpl
{
    Script_Interpreter* vm;
    Script_Instance* globalInstance;

    HypScriptImpl()
        : vm(new Script_Interpreter()),
          globalInstance(nullptr)
    {
    }

    HypScriptImpl(const HypScriptImpl& other) = delete;
    HypScriptImpl& operator=(const HypScriptImpl& other) = delete;

    ~HypScriptImpl()
    {
        if (globalInstance)
        {
            delete globalInstance;
            globalInstance = nullptr;
        }

        delete vm;
    }
};

#pragma endregion HypScriptImpl

HypScript& HypScript::GetInstance()
{
    static HypScript s_hypScriptInstance;

    return s_hypScriptInstance;
}

HypScript::HypScript()
    : m_impl(MakePimpl<HypScriptImpl>())
{
}

HypScript::~HypScript() = default;

Script_Interpreter* HypScript::GetVM() const
{
    return m_impl->vm;
}

Script_Instance* HypScript::GetGlobalInstance() const
{
    return m_impl->globalInstance;
}

void HypScript::Initialize()
{
    BuiltinTypes::Initialize();

    // create the global script instance using Lib.hyp (generated by HypBuildTool)
    static const FilePath s_libPath = CoreApi_GetExecutablePath() / "Lib.hyp";

    FileBufferedReaderSource source { s_libPath };
    BufferedByteReader reader { &source };

    if (!reader.IsOpen())
    {
        HYP_LOG(HypScript, Error, "Failed to open Lib.hyp at path '{}'! Did the build tool run correctly?", s_libPath);

        return;
    }

    ByteBuffer byteBuffer = reader.ReadBytes();

    SourceFile sourceFile(s_libPath, byteBuffer.Size());
    sourceFile.ReadIntoBuffer(byteBuffer);

    ErrorList errorList;

    Script_Instance* instance = Compile(sourceFile, errorList);

    if (errorList.HasFatalErrors())
    {
        HYP_LOG(HypScript, Error, "Failed to compile Lib.hyp! The script system will not function correctly.");

        return;
    }

    Assert(instance != nullptr);

    // run the main script to initialize classes, functions, etc.
    Run(instance);
}

void HypScript::DestroyScript(Script_Instance* instance)
{
    if (!instance)
    {
        return;
    }

    delete instance;
}

Script_Instance* HypScript::Compile(SourceFile& sourceFile, ErrorList& outErrorList)
{
    if (!sourceFile.IsValid())
    {
        return nullptr;
    }

    SourceStream sourceStream(&sourceFile);
    TokenStream tokenStream(TokenStreamInfo { sourceFile.GetFilePath() });

    CompilationUnit compilationUnit;

    BuiltinTypes::AddToSymbolTable(compilationUnit.GetGlobalModule()->scopeTree.Top().identifierTable);

    Lexer lex(sourceStream, &tokenStream, &compilationUnit);
    lex.Analyze();

    AstIterator astIterator;
    SemanticAnalyzer semanticAnalyzer(&astIterator, &compilationUnit);

    Parser parser(&astIterator, &tokenStream, &compilationUnit);
    parser.Parse();

    astIterator.ResetPosition();
    semanticAnalyzer.Analyze();

#ifdef HYP_PRINT_AST
    AstPrintVisitor printer(&astIterator, &compilationUnit);
    printer.SetUseColors(true);
    printer.SetShowDetails(true);
    printer.SetShowLocations(true);
    printer.SetShowTypes(true);

    astIterator.ResetPosition();
    HYP_LOG(HypScript, Info, "{}", printer.PrintTree(astIterator.Peek()));
#endif

    outErrorList = compilationUnit.GetErrorList();
    outErrorList.WriteOutput(std::cout);

    if (!outErrorList.HasFatalErrors())
    {
        // only optimize if there were no errors
        // before this point
        astIterator.ResetPosition();

        Optimizer optimizer(&astIterator, &compilationUnit);
        optimizer.Optimize();

        // compile into bytecode instructions
        astIterator.ResetPosition();

        Compiler compiler(&astIterator, &compilationUnit);
        BytecodeChunk bytecodeChunk;

        if (auto compileResult = compiler.Compile())
        {
            bytecodeChunk.Append(std::move(compileResult));
        }
        else
        {
            return nullptr;
        }

        BuildParams buildParams {};

        CodeGenerator codeGenerator(buildParams);
        codeGenerator.Visit(&bytecodeChunk);
        codeGenerator.Bake();

        Script_Instance* scriptInstance = new Script_Instance {
            Script_Stream(codeGenerator.GetInternalByteStream().GetData())
        };

        return scriptInstance;
    }

    return nullptr;
}

InstructionStream* HypScript::Decompile(Script_Instance* instance, std::ostream* os) const
{
    if (!instance)
    {
        return nullptr;
    }

    return DecompilationUnit().Decompile(instance->stream, os);
}

void HypScript::Run(Script_Instance* instance)
{
    if (!instance)
    {
        return;
    }

    m_impl->vm->Execute(instance);
}

Script_Value HypScript::CallFunctionArgV(Script_Instance* instance, const Script_Value& value, Script_Value* args, ArgCount numArgs)
{
    Assert(instance != nullptr);
    Assert(value.IsFunction());

    if (numArgs != 0)
    {
        Assert(args != nullptr);

        for (ArgCount i = 0; i < numArgs; i++)
        {
            instance->thread.m_stack.Push(std::move(args[i]));
        }
    }

    // Create a reference since we know the lifetime of the function handle will exist longer than the call
    // and we don't want to move the underlying value
    Script_VMData vmData;
    vmData.type = Script_VMData::VALUE_REF;
    vmData.valueRef = const_cast<Script_Value*>(&value);

    m_impl->vm->InvokeNow(instance, Script_Value(vmData), numArgs);

    if (numArgs != 0)
    {
        instance->thread.m_stack.Pop(numArgs);
    }

    return std::move(instance->thread.GetRegisters()[0]);
}

void HypScript::ReadLastReturnValue(Script_Instance* instance, Script_Value& outValue)
{
    Assert(instance != nullptr);

    outValue = ScriptApi_ShallowCopy(instance->thread.m_regs[0], m_impl->vm->GetGC());
}

bool HypScript::GetMember(Script_Instance* instance, const Script_Value& targetValue, const char* memberName, Script_Value& outValue)
{
    outValue = Script_Value();

    if (!targetValue.IsValid())
    {
        return false;
    }

    const AnyHandle& object = targetValue.GetObject();

    if (!object)
    {
        return false;
    }

    const HypClass* hypClass = object.ptr->InstanceClass();
    Assert(hypClass != nullptr);

    IHypMember* member = hypClass->GetMember(WeakName(memberName));

    if (!member)
    {
        return false;
    }

    if (member->GetMemberType() == HypMemberType::TYPE_FIELD)
    {
        HypField* field = static_cast<HypField*>(member);

        outValue = ScriptApi_MakeValue(field->Get(*targetValue.GetHypData()));

        return true;
    }

    if (member->GetMemberType() == HypMemberType::TYPE_METHOD)
    {
        HypMethod* method = static_cast<HypMethod*>(member);

        Script_VMData vmData;

        if (method->IsScriptFunction())
        {
            Assert(method->GetParameters().Size() <= UINT8_MAX);

            vmData.type = Script_VMData::FUNCTION;
            vmData.func.m_addr = method->GetScriptAddress();
            vmData.func.m_nargs = (uint8)method->GetParameters().Size();
            vmData.func.m_flags = (uint8)method->GetFlags();
        }
        else
        {
            vmData.type = Script_VMData::NATIVE_FUNCTION;
            vmData.nativeFunc = method;
        }

        outValue = ScriptApi_MakeValue(vmData);

        return true;
    }

    return false;
}

bool HypScript::SetField(Script_Value& targetValue, const char* memberName, Script_Value&& value)
{
    if (!targetValue.IsValid())
    {
        return false;
    }

    const AnyHandle& object = targetValue.GetObject();

    if (!object)
    {
        return false;
    }

    const HypClass* hypClass = object.ptr->InstanceClass();
    Assert(hypClass != nullptr);

    HypField* field = hypClass->GetField(WeakName(memberName));

    if (!field)
    {
        return false;
    }

    field->Set(*targetValue.GetHypData(), std::move(*value.GetHypData()));

    return true;
}

bool HypScript::GetFunctionHandle(Script_Instance* instance, const char* name, Script_Value& outValue)
{
    outValue = Script_Value();

    Script_Value* pValue;
    if (!instance->exportedSymbols.Find(HashCode::GetHashCode(name).Value(), pValue))
    {
        return false;
    }

    if (!pValue->IsFunction())
    {
        return false;
    }

    outValue = *pValue;

    return true;
}

bool HypScript::GetExportedValue(Script_Instance* instance, const char* name, Script_Value& outValue, bool getReference)
{
    outValue = Script_Value();

    Script_Value* pValue;
    if (!instance->exportedSymbols.Find(HashCode::GetHashCode(name).Value(), pValue))
    {
        return false;
    }

    Assert(pValue != nullptr);

    pValue = pValue->Deref();

    if (getReference || ScriptApi_ShouldValuePassByRef(*pValue))
    {
        outValue = ScriptApi_MakeRef(pValue);

        return true;
    }

    outValue = *pValue;

    return true;
}

Script_SymbolTable& HypScript::GetExportedSymbols(Script_Instance* instance) const
{
    return instance->exportedSymbols;
}

} // namespace hyperion
