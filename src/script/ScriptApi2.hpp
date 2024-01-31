#ifndef HYPERION_V2_SCRIPT_SCRIPT_API_2_HPP
#define HYPERION_V2_SCRIPT_SCRIPT_API_2_HPP

#include <core/lib/String.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Mutex.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>

#include <script/vm/Value.hpp>
#include <script/vm/VM.hpp>

namespace hyperion {

class APIInstance;

namespace scriptapi2 {

using namespace vm;
using namespace compiler;

struct Type
{
    String          type_string;
    SymbolTypePtr_t symbol_type;

    bool IsValid() const
        { return symbol_type != nullptr; }
};

struct Symbol
{
    String                              name;
    Type                                type;
    Variant<Value, NativeFunctionPtr_t> value;

    Symbol(
        const String &name,
        const String &type_string,
        const Value &value
    ) : name(name),
        type { type_string, nullptr },
        value(value)
    {
    }

    Symbol(
        const String &name,
        const String &type_string,
        NativeFunctionPtr_t value
    ) : name(name),
        type { type_string, nullptr },
        value(value)
    {
    }

    Symbol(const Symbol &other)
        : name(other.name),
          type(other.type),
          value(other.value)
    {
    }

    Symbol &operator=(const Symbol &other)
    {
        name = other.name;
        type = other.type;
        value = other.value;

        return *this;
    }

    Symbol(Symbol &&other) noexcept
        : name(std::move(other.name)),
          type(std::move(other.type)),
          value(std::move(other.value))
    {
    }

    Symbol &operator=(Symbol &&other) noexcept
    {
        name = std::move(other.name);
        type = std::move(other.type);
        value = std::move(other.value);

        return *this;
    }

    ~Symbol() = default;
};

struct ClassDefinition
{
    TypeID                      native_type_id;
    String                      name;
    Optional<String>            generic_params_string;
    Array<Symbol>               members;
    Array<Symbol>               static_members;

    RC<AstExpression>           expr;
    RC<AstVariableDeclaration>  var_decl;
};

struct GlobalDefinition
{
    Symbol                      symbol;
    Optional<String>            generic_params_string;

    RC<AstVariableDeclaration>  var_decl;
};

class Context;
class ClassBuilder
{
public:
    ClassBuilder(Context *context, ClassDefinition class_definition);
    ClassBuilder(const ClassBuilder &other)                 = delete;
    ClassBuilder &operator=(const ClassBuilder &other)      = delete;
    ClassBuilder(ClassBuilder &&other) noexcept             = default;
    ClassBuilder &operator=(ClassBuilder &&other) noexcept  = default;
    ~ClassBuilder()                                         = default;

    ClassBuilder &Member(
        String name,
        String type_string,
        Value value
    );

    ClassBuilder &Method(
        String name,
        String type_string,
        NativeFunctionPtr_t fn
    );

    ClassBuilder &StaticMember(
        String name,
        String type_string,
        Value value
    );

    ClassBuilder &StaticMethod(
        String name,
        String type_string,
        NativeFunctionPtr_t fn
    );

    void Build();

private:
    Context         *m_context;
    ClassDefinition m_class_definition;
};

class Context
{
    friend class ClassBuilder;

public:
    template <class T>
    ClassBuilder Class(String name, Optional<String> generic_params_string = { })
    {
        ClassDefinition def {
            TypeID::ForType<T>(),
            std::move(name),
            std::move(generic_params_string),
            {},
            {}
        };

        return ClassBuilder(this, std::move(def));
    }

    Context &Global(
        String name,
        String type_string,
        Value value
    );

    Context &Global(
        String name,
        String generic_params_string,
        String type_string,
        Value value
    );

    Context &Global(
        String name,
        String type_string,
        NativeFunctionPtr_t fn
    );

    Context &Global(
        String name,
        String generic_params_string,
        String type_string,
        NativeFunctionPtr_t fn
    );

    void Visit(
        AstVisitor *visitor,
        CompilationUnit *compilation_unit
    );

    void BindAll(
        APIInstance &api_instance,
        VM *vm
    );

private:
    static Array<RC<AstParameter>> ParseGenericParams(const String &generic_params_string);
    static RC<AstExpression> ParseTypeExpression(const String &type_string);

    Array<GlobalDefinition>     m_globals;
    Array<ClassDefinition>      m_class_definitions;
    Mutex                       m_mutex;
};

} // namespace scriptapi2
} // namespace hyperion

#endif