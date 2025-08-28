#pragma once

#include <core/containers/String.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/TypeId.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/threading/Mutex.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstParameter.hpp>

#include <script/vm/Value.hpp>
#include <script/vm/VM.hpp>

namespace hyperion {

class APIInstance;
class HypMethod;

namespace scriptapi2 {

using namespace vm;
using namespace compiler;

struct Type
{
    String typeString;
    SymbolTypeRef symbolType;

    bool IsValid() const
    {
        return symbolType != nullptr;
    }
};

struct Symbol
{
    String name;
    Type type;
    Value value;

    Symbol(
        const String& name,
        const String& typeString,
        Value&& value)
        : name(name),
          type { typeString, nullptr },
          value(std::move(value))
    {
    }

    Symbol(
        const String& name,
        const String& typeString,
        HypMethod* hypMethod)
        : name(name),
          type { typeString, nullptr }
    {
        Script_VMData vmData;
        vmData.type = Script_VMData::NATIVE_FUNCTION;
        vmData.nativeFunc = hypMethod;

        value = Value(vmData);
    }

    Symbol(const Symbol& other) = delete;
    Symbol& operator=(const Symbol& other) = delete;

    Symbol(Symbol&& other) noexcept
        : name(std::move(other.name)),
          type(std::move(other.type)),
          value(std::move(other.value))
    {
    }

    Symbol& operator=(Symbol&& other) noexcept
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
    TypeId nativeTypeId;
    String name;
    Optional<String> genericParamsString;
    Array<Symbol> members;
    Array<Symbol> staticMembers;

    RC<AstExpression> expr;
    RC<AstVariableDeclaration> varDecl;
};

struct GlobalDefinition
{
    Symbol symbol;
    Optional<String> genericParamsString;

    RC<AstVariableDeclaration> varDecl;
};

class Context;
class ClassBuilder
{
public:
    ClassBuilder(Context* context, ClassDefinition classDefinition);
    ClassBuilder(const ClassBuilder& other) = delete;
    ClassBuilder& operator=(const ClassBuilder& other) = delete;
    ClassBuilder(ClassBuilder&& other) noexcept = default;
    ClassBuilder& operator=(ClassBuilder&& other) noexcept = default;
    ~ClassBuilder() = default;

    ClassBuilder& Member(
        String name,
        String typeString,
        HypData&& value);

    ClassBuilder& Method(
        String name,
        String typeString,
        HypMethod* hypMethod);

    ClassBuilder& StaticMember(
        String name,
        String typeString,
        HypData&& value);

    ClassBuilder& StaticMethod(
        String name,
        String typeString,
        HypMethod* hypMethod);

    void Build();

private:
    Context* m_context;
    ClassDefinition m_classDefinition;
};

class Context
{
    friend class ClassBuilder;

public:
    template <class T>
    ClassBuilder Class(String name, Optional<String> genericParamsString = {})
    {
        ClassDefinition def {
            TypeId::ForType<T>(),
            std::move(name),
            std::move(genericParamsString),
            {},
            {}
        };

        return ClassBuilder(this, std::move(def));
    }

    Context& Global(
        String name,
        String typeString,
        HypData&& value);

    Context& Global(
        String name,
        String genericParamsString,
        String typeString,
        HypData&& value);

    Context& Global(
        String name,
        String typeString,
        HypMethod* hypMethod);

    Context& Global(
        String name,
        String genericParamsString,
        String typeString,
        HypMethod* hypMethod);

    void Visit(
        AstVisitor* visitor,
        CompilationUnit* compilationUnit);

    void BindAll(
        APIInstance& apiInstance,
        VM* vm);

private:
    static Array<RC<AstParameter>> ParseGenericParams(const String& genericParamsString);
    static RC<AstExpression> ParseTypeExpression(const String& typeString);

    Array<GlobalDefinition> m_globals;
    Array<ClassDefinition> m_classDefinitions;
    Mutex m_mutex;
};

} // namespace scriptapi2
} // namespace hyperion
