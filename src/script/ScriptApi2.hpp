#ifndef HYPERION_V2_SCRIPT_SCRIPT_API_2_HPP
#define HYPERION_V2_SCRIPT_SCRIPT_API_2_HPP

#include <core/lib/String.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/RefCountedPtr.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>

#include <script/vm/Value.hpp>
#include <script/vm/VM.hpp>

namespace hyperion {

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
    Array<Symbol>               members;
    Array<Symbol>               static_members;

    RC<AstTypeExpression>       expr;
    RC<AstVariableDeclaration>  var_decl;
};

class Context
{
public:
    template <class T>
    Context &Class(
        String name,
        Array<Symbol> members,
        Array<Symbol> static_members
    )
    {
        ClassDefinition def {
            TypeID::ForType<T>(),
            std::move(name),
            std::move(members),
            std::move(static_members)
        };

        m_class_definitions.PushBack(std::move(def));

        return *this;
    }

    void Visit(
        AstVisitor *visitor,
        CompilationUnit *compilation_unit
    );

    void BindAll(
        VM *vm
    );

private:
    static RC<AstExpression> ParseTypeExpression(const String &type_string);

    Array<ClassDefinition> m_class_definitions;
};

} // namespace scriptapi2
} // namespace hyperion

#endif