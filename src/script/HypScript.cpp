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

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

// #define HYP_PRINT_AST

HYP_DEFINE_LOG_CHANNEL(HypScript);

#pragma region Opaque Handles

#pragma endregion Opaque Handles

HypScript& HypScript::GetInstance()
{
    static HypScript instance;

    return instance;
}

HypScript::HypScript()
    : m_vm(new Script_Interpreter())
{
}

HypScript::~HypScript()
{
    delete m_vm;
}

void HypScript::Initialize()
{
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

    m_vm->Execute(instance);
}

void HypScript::CallFunctionArgV(Script_Instance* instance, Script_FunctionHandle functionHandle, Script_Value* args, ArgCount numArgs)
{
    Assert(instance != nullptr);
    Assert(functionHandle != INVALID_FUNCTION);

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
    vmData.valueRef = reinterpret_cast<Script_Value*>(functionHandle);

    m_vm->InvokeNow(instance, Script_Value(vmData), numArgs);

    if (numArgs != 0)
    {
        instance->thread.m_stack.Pop(numArgs);
    }
}

void HypScript::ReadLastReturnValue(Script_Instance* instance, Script_Value& outValue)
{
    Assert(instance != nullptr);

    outValue = ScriptApi_ShallowCopy(instance->thread.m_regs[0], m_vm->GetGC());
}

bool HypScript::GetMember(Script_ObjectHandle objectHandle, const char* memberName, Script_Value*& outValue)
{
    HYP_NOT_IMPLEMENTED();

#if 0
    
    if (objectHandle == INVALID_OBJECT)
    {
        return false;
    }

    Script_Value& objectValue = *reinterpret_cast<Script_Value*>(objectHandle);

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

bool HypScript::SetMember(Script_ObjectHandle objectHandle, const char* memberName, Script_Value&& value)
{
    HYP_NOT_IMPLEMENTED();
#if 0
    if (objectHandle == INVALID_OBJECT)
    {
        return false;
    }

    Script_Value& objectValue = *reinterpret_cast<Script_Value*>(objectHandle);

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

bool HypScript::GetFunctionHandle(const char* name, Script_FunctionHandle& outFunctionHandle)
{
    outFunctionHandle = INVALID_FUNCTION;

    Script_Value* pValue;
    if (!GetExportedValue(name, pValue))
    {
        return false;
    }

    if (!pValue->IsFunction())
    {
        return false;
    }

    outFunctionHandle = (Script_FunctionHandle)((uintptr_t)pValue);

    return true;
}

bool HypScript::GetObjectHandle(const char* name, Script_ObjectHandle& outObjectHandle)
{
    outObjectHandle = INVALID_OBJECT;

    Script_Value* pValue;

    if (!GetExportedValue(name, pValue))
    {
        return false;
    }

    if (!pValue->IsRef())
    {
        return false;
    }

    Script_Value* pRef = pValue->GetRef();
    if (pRef == nullptr || !pRef->IsValid())
    {
        return false;
    }

    outObjectHandle = (Script_ObjectHandle)((uintptr_t)pRef);

    return true;
}

bool HypScript::GetExportedValue(const char* name, Script_Value*& outValue)
{
    return GetExportedSymbols().Find(HashCode::GetHashCode(name).Value(), outValue);
}

Script_SymbolTable& HypScript::GetExportedSymbols() const
{
    return m_vm->GetExportedSymbols();
}

} // namespace hyperion
