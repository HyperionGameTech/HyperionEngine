#include <script/HypScript.hpp>

#include <script/compiler/Module.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/dis/DecompilationUnit.hpp>
#include <script/compiler/emit/codegen/CodeGenerator.hpp>
#include <script/compiler/builtins/Builtins.hpp>

#include <script/ScriptBindings.hpp>
#include <script/ScriptApi.hpp>

#include <script/vm/VM.hpp>

namespace hyperion {

static IdGenerator g_scriptHandleGenerator;

#pragma region Opaque Handles

struct ScriptHandleData
{
    BytecodeStream bytecodeStream;
};

#pragma endregion Opaque Handles

HypScript& HypScript::GetInstance()
{
    static HypScript instance;

    return instance;
}

HypScript::HypScript()
    : m_apiInstance(),
      m_vm(new VM(m_apiInstance))
{
}

HypScript::~HypScript()
{
    for (auto& pair : m_scripts)
    {
        delete pair.second;
    }

    m_scripts.Clear();

    m_apiInstance.SetVM(nullptr);
    delete m_vm;
}

void HypScript::Initialize()
{
    g_scriptBindings.GenerateAll(m_context);

    m_apiInstance.SetVM(m_vm);

    // Initialize builtins
    TokenStream tokenStream(TokenStreamInfo { "<builtins>" });

    AstIterator astIterator;
    CompilationUnit compilationUnit;
    SemanticAnalyzer semanticAnalyzer(&astIterator, &compilationUnit);

    compilationUnit.GetBuiltins().Visit(&semanticAnalyzer);

    m_context.Visit(&semanticAnalyzer, &compilationUnit);

    Parser parser(&astIterator, &tokenStream, &compilationUnit);
    parser.Parse(false);

    semanticAnalyzer.Analyze(false);

    ErrorList errorList = compilationUnit.GetErrorList();

    if (errorList.HasFatalErrors())
    {
        // write errors to stdout
        errorList.WriteOutput(std::cout);

        HYP_FAIL("Fatal errors occurred while initializing HypScript!");
    }

    // only optimize if there were no errors
    // before this point
    astIterator.ResetPosition();

    Optimizer optimizer(&astIterator, &compilationUnit);
    optimizer.Optimize();

    // compile into bytecode instructions
    astIterator.ResetPosition();

    Compiler compiler(&astIterator, &compilationUnit);

    if (!compiler.Compile())
    {
        HYP_FAIL("Failed to compile HypScript builtins!");
    }

    m_context.BindAll(m_apiInstance, m_vm);
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

    Lexer lex(sourceStream, &tokenStream, &compilationUnit);
    lex.Analyze();

    AstIterator astIterator;
    SemanticAnalyzer semanticAnalyzer(&astIterator, &compilationUnit);

    compilationUnit.GetBuiltins().Visit(&semanticAnalyzer);

    m_context.Visit(&semanticAnalyzer, &compilationUnit);

    Parser parser(&astIterator, &tokenStream, &compilationUnit);
    parser.Parse();

    semanticAnalyzer.Analyze();

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
        ScriptHandleData* scriptHandleData = new ScriptHandleData {
            BytecodeStream(codeGenerator.GetInternalByteStream().GetData())
        };

        {
            Mutex::Guard guard(m_mutex);
            m_scripts.Insert({ scriptHandle, scriptHandleData });
        }

        return scriptHandle;
    }

    return INVALID_SCRIPT;
}

InstructionStream HypScript::Decompile(
    ScriptHandle scriptHandle,
    std::ostream* os) const
{
    if (scriptHandle == INVALID_SCRIPT)
    {
        return InstructionStream();
    }

    Mutex::Guard guard(m_mutex);

    auto it = m_scripts.Find(scriptHandle);

    if (it == m_scripts.End())
    {
        return InstructionStream();
    }

    ScriptHandleData* scriptHandleData = it->second;
    Assert(scriptHandleData != nullptr);

    return DecompilationUnit().Decompile(scriptHandleData->bytecodeStream, os);
}

void HypScript::Run(ScriptHandle scriptHandle)
{
    if (scriptHandle == INVALID_SCRIPT)
    {
        return;
    }

    ScriptHandleData* scriptHandleData;

    {
        Mutex::Guard guard(m_mutex);
        auto it = m_scripts.Find(scriptHandle);

        if (it == m_scripts.End())
        {
            return;
        }

        scriptHandleData = it->second;
        Assert(scriptHandleData != nullptr);
    }

    m_vm->Execute(&scriptHandleData->bytecodeStream);
}

void HypScript::CallFunctionArgV(ScriptHandle scriptHandle, FunctionHandle functionHandle, Value* args, ArgCount numArgs)
{
    Assert(scriptHandle != INVALID_SCRIPT);
    Assert(functionHandle != INVALID_FUNCTION);

    Script_ExecutionThread* mainThread = m_vm->GetMainThread();

    if (numArgs != 0)
    {
        Assert(args != nullptr);

        for (ArgCount i = 0; i < numArgs; i++)
        {
            mainThread->m_stack.Push(std::move(args[i]));
        }
    }

    ScriptHandleData* scriptHandleData;

    {
        Mutex::Guard guard(m_mutex);
        auto it = m_scripts.Find(scriptHandle);
        Assert(it != m_scripts.End());

        scriptHandleData = it->second;
        Assert(scriptHandleData != nullptr);
    }

    // Create a reference since we know the lifetime of the function handle will exist longer than the call
    // and we don't want to move the underlying value
    Script_VMData vmData;
    vmData.type = Script_VMData::VALUE_REF;
    vmData.valueRef = reinterpret_cast<Value*>(functionHandle);

    m_vm->InvokeNow(&scriptHandleData->bytecodeStream, Value(vmData), numArgs);

    if (numArgs != 0)
    {
        mainThread->m_stack.Pop(numArgs);
    }
}

void HypScript::ReadLastReturnValue(Value& outValue)
{
    Script_ExecutionThread* mainThread = m_vm->GetMainThread();
    outValue = ScriptApi_ShallowCopy(mainThread->m_regs[0], m_vm->GetGC());
}

bool HypScript::GetMember(ObjectHandle objectHandle, const char* memberName, Value*& outValue)
{
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
}

bool HypScript::SetMember(ObjectHandle objectHandle, const char* memberName, Value&& value)
{
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
    return GetExportedSymbols().Find(hashFnv1(name), outValue);
}

ExportedSymbolTable& HypScript::GetExportedSymbols() const
{
    return m_vm->GetExportedSymbols();
}

} // namespace hyperion
