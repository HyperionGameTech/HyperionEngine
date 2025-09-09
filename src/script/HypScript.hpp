#pragma once

#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>

#include <script/vm/Value.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {

class HypScript;
class Script_Interpreter;
class Script_SymbolTable;
class InstructionStream;

struct Script_Instance;

class HypScript
{
public:
    using ArgCount = uint16;

    static HypScript& GetInstance();

    HypScript();
    HypScript(const HypScript& other) = delete;
    HypScript& operator=(const HypScript& other) = delete;
    ~HypScript();

    Script_Interpreter* GetVM() const
    {
        return m_vm;
    }

    Script_Instance* GetGlobalInstance() const
    {
        return m_globalInstance;
    }

    void Initialize();

    void DestroyScript(Script_Instance* instance);

    Script_Instance* Compile(SourceFile& sourceFile, ErrorList& outErrorList);
    InstructionStream* Decompile(Script_Instance* instance, std::ostream* os = nullptr) const;

    void Run(Script_Instance* instance);

    template <class T>
    static inline Script_Value CreateArgument(T&& item)
    {
        return Script_Value(HypData(std::forward<T>(item)));
    }

    template <class... Args>
    static inline auto CreateArguments(Args&&... args) -> FixedArray<Script_Value, sizeof...(Args)>
    {
        return FixedArray<Script_Value, sizeof...(Args)> { CreateArgument(args)... };
    }

    Script_Value CallFunctionArgV(Script_Instance* instance, const Script_Value& value, Script_Value* args, ArgCount numArgs);

    bool GetFunctionHandle(const char* name, Script_Value& outValue);
    bool GetExportedValue(const char* name, Script_Value& outValue, bool getReference);

    Script_SymbolTable& GetExportedSymbols() const;

    /*! \brief Implements OpGetMember in the virtual machine.
     *  Gets a field or method by name and sets `outValue` to the value.
     *  Returns true on found, false otherwise. */
    bool GetMember(const Script_Value& targetValue, const char* memberName, Script_Value& outValue);

    /*! \brief Implements OpSetField in the virtual machine. Sets a field with the name `memberName` to the value held in `value`.
     *  If the field was not found, returns false.
     *  Returns true on success. */
    bool SetField(Script_Value& targetValue, const char* memberName, Script_Value&& value);

    template <class... Args>
    Script_Value CallFunction(Script_Instance* instance, const Script_Value& value, Args&&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        return CallFunctionArgV(instance, value, arguments.Data(), arguments.Size());
    }

    void ReadLastReturnValue(Script_Instance* instance, Script_Value& outValue);

private:
    Script_Interpreter* m_vm;
    Script_Instance* m_globalInstance;
};

} // namespace hyperion
