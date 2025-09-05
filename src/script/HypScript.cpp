#include <script/HypScript.hpp>

#include <script/vm/BytecodeStream.hpp>

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

#include <script/vm/VM.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

// #define HYP_PRINT_AST

HYP_DEFINE_LOG_CHANNEL(HypScript);

static IdGenerator g_scriptHandleGenerator;

#pragma region Opaque Handles

#pragma endregion Opaque Handles

HypScript& HypScript::GetInstance()
{
    static HypScript instance;

    return instance;
}

HypScript::HypScript()
    : m_vm(new VM())
{
}

HypScript::~HypScript()
{
    for (auto& pair : m_scripts)
    {
        delete pair.second;
    }

    m_scripts.Clear();

    delete m_vm;
}

void HypScript::Initialize()
{
}

void HypScript::DestroyScript(ScriptHandle scriptHandle)
{
    if (scriptHandle == INVALID_SCRIPT)
    {
        return;
    }

    Mutex::Guard guard(m_mutex);

    auto it = m_scripts.Find(scriptHandle);

    if (it != m_scripts.End())
    {
        delete it->second;
        m_scripts.Erase(it);
    }
}

ScriptHandle HypScript::Compile(
    SourceFile& sourceFile,
    ErrorList& outErrorList)
{
    if (!sourceFile.IsValid())
    {
        return INVALID_SCRIPT;
    }

    SourceStream sourceStream(&sourceFile);
    TokenStream tokenStream(TokenStreamInfo { sourceFile.GetFilePath() });

    CompilationUnit compilationUnit;
    BuiltinTypes::AddToSymbolTable(compilationUnit.GetGlobalModule()->m_scopes.Top().identifierTable);

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
            return INVALID_SCRIPT;
        }

        BuildParams buildParams {};

        CodeGenerator codeGenerator(buildParams);
        codeGenerator.Visit(&bytecodeChunk);
        codeGenerator.Bake();

        ScriptHandle scriptHandle = (ScriptHandle)g_scriptHandleGenerator.Next();

        Script_Instance* scriptInstance = new Script_Instance {
            BytecodeStream(codeGenerator.GetInternalByteStream().GetData())
        };

        {
            Mutex::Guard guard(m_mutex);
            m_scripts.Insert({ scriptHandle, scriptInstance });
        }

        return scriptHandle;
    }

    return INVALID_SCRIPT;
}

InstructionStream* HypScript::Decompile(ScriptHandle scriptHandle, std::ostream* os) const
{
    if (scriptHandle == INVALID_SCRIPT)
    {
        return nullptr;
    }

    Mutex::Guard guard(m_mutex);

    auto it = m_scripts.Find(scriptHandle);

    if (it == m_scripts.End())
    {
        return nullptr;
    }

    Script_Instance* scriptInstance = it->second;
    Assert(scriptInstance != nullptr);

    return DecompilationUnit().Decompile(scriptInstance->stream, os);
}

void HypScript::Run(ScriptHandle scriptHandle)
{
    if (scriptHandle == INVALID_SCRIPT)
    {
        return;
    }

    Script_Instance* scriptInstance;

    {
        Mutex::Guard guard(m_mutex);
        auto it = m_scripts.Find(scriptHandle);

        if (it == m_scripts.End())
        {
            return;
        }

        scriptInstance = it->second;
        Assert(scriptInstance != nullptr);
    }

    m_vm->Execute(scriptInstance);
}

void HypScript::CallFunctionArgV(ScriptHandle scriptHandle, FunctionHandle functionHandle, Value* args, ArgCount numArgs)
{
    Assert(scriptHandle != INVALID_SCRIPT);
    Assert(functionHandle != INVALID_FUNCTION);

    Script_Instance* scriptInstance;

    {
        Mutex::Guard guard(m_mutex);
        auto it = m_scripts.Find(scriptHandle);
        Assert(it != m_scripts.End());

        scriptInstance = it->second;
        Assert(scriptInstance != nullptr);
    }

    if (numArgs != 0)
    {
        Assert(args != nullptr);

        for (ArgCount i = 0; i < numArgs; i++)
        {
            scriptInstance->thread.m_stack.Push(std::move(args[i]));
        }
    }

    // Create a reference since we know the lifetime of the function handle will exist longer than the call
    // and we don't want to move the underlying value
    Script_VMData vmData;
    vmData.type = Script_VMData::VALUE_REF;
    vmData.valueRef = reinterpret_cast<Value*>(functionHandle);

    m_vm->InvokeNow(scriptInstance, Value(vmData), numArgs);

    if (numArgs != 0)
    {
        scriptInstance->thread.m_stack.Pop(numArgs);
    }
}

void HypScript::ReadLastReturnValue(ScriptHandle scriptHandle, Value& outValue)
{
    Assert(scriptHandle != INVALID_SCRIPT);

    Script_Instance* scriptInstance;

    {
        Mutex::Guard guard(m_mutex);
        auto it = m_scripts.Find(scriptHandle);
        Assert(it != m_scripts.End());

        scriptInstance = it->second;
        Assert(scriptInstance != nullptr);
    }

    outValue = ScriptApi_ShallowCopy(scriptInstance->thread.m_regs[0], m_vm->GetGC());
}

bool HypScript::GetMember(ObjectHandle objectHandle, const char* memberName, Value*& outValue)
{
    HYP_NOT_IMPLEMENTED();

#if 0
    
    if (objectHandle == INVALID_OBJECT)
    {
        return false;
    }

    Value& objectValue = *reinterpret_cast<Value*>(objectHandle);

    VMObject* object = objectValue.GetObject();

    if (!object)
    {
        return false;
    }

    if (Member* member = object->LookupMemberFromHash(hashFnv1(memberName)))
    {
        outValue = &member->value;

        return true;
    }

    return false;
#endif
}

bool HypScript::SetMember(ObjectHandle objectHandle, const char* memberName, Value&& value)
{
    HYP_NOT_IMPLEMENTED();
#if 0
    if (objectHandle == INVALID_OBJECT)
    {
        return false;
    }

    Value& objectValue = *reinterpret_cast<Value*>(objectHandle);

    VMObject* object = objectValue.GetObject();

    if (!object)
    {
        return false;
    }

    if (Member* member = object->LookupMemberFromHash(hashFnv1(memberName)))
    {
        member->value.AssignValue(std::move(value), false);

        return true;
    }

    return false;
#endif
}

bool HypScript::GetFunctionHandle(const char* name, FunctionHandle& outFunctionHandle)
{
    outFunctionHandle = INVALID_FUNCTION;

    Value* pValue;
    if (!GetExportedValue(name, pValue))
    {
        return false;
    }

    if (!pValue->IsFunction())
    {
        return false;
    }

    outFunctionHandle = (FunctionHandle)((uintptr_t)pValue);

    return true;
}

bool HypScript::GetObjectHandle(const char* name, ObjectHandle& outObjectHandle)
{
    outObjectHandle = INVALID_OBJECT;

    Value* pValue;

    if (!GetExportedValue(name, pValue))
    {
        return false;
    }

    if (!pValue->IsRef())
    {
        return false;
    }

    Value* pRef = pValue->GetRef();
    if (pRef == nullptr || !pRef->IsValid())
    {
        return false;
    }

    outObjectHandle = (ObjectHandle)((uintptr_t)pRef);

    return true;
}

bool HypScript::GetExportedValue(const char* name, Value*& outValue)
{
    return GetExportedSymbols().Find(HashCode::GetHashCode(name).Value(), outValue);
}

ExportedSymbolTable& HypScript::GetExportedSymbols() const
{
    return m_vm->GetExportedSymbols();
}

} // namespace hyperion
