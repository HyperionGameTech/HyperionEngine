#include <Lang/Compiler/Ast/AstHashMap.hpp>
#include <Lang/Compiler/Ast/AstAsExpression.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstTemplateInstantiation.hpp>
#include <Lang/Compiler/Ast/AstArrayExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Lang/Compiler/Emit/StorageOperation.hpp>

#include <Lang/Instructions.hpp>
#include <Core/HashCode.hpp>
#include <Core/Debug/Debug.hpp>

#include <Core/Containers/FlatSet.hpp>

#include <Core/Types.hpp>

#include <AstHashMap.generated.inl>

namespace Hyperion {

/// @TODO: Should be reworked to use intrinsic bytecode instructions rather than constructing nested arrays and using Map.FromArray!

AstHashMap::AstHashMap(
    const Array<Handle<AstExpression>>& keys,
    const Array<Handle<AstExpression>>& values,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_keys(keys),
      m_values(values),
      m_keyType(nullptr),
      m_valueType(nullptr),
      m_exprType(nullptr)
{
}

void AstHashMap::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_keys.Size() == m_values.Size());

    m_replacedKeys.Reserve(m_keys.Size());
    m_replacedValues.Reserve(m_values.Size());

    m_exprType = BuiltinTypes::s_errorType;

    m_keyType = BuiltinTypes::s_errorType;
    m_valueType = BuiltinTypes::s_errorType;

    Array<Pair<Handle<AstExpression>, Handle<AstExpression>>> keyValuePairs;
    keyValuePairs.Reserve(m_keys.Size());

    for (size_t i = 0; i < m_keys.Size(); ++i)
    {
        Assert(m_keys[i] != nullptr);
        Assert(m_values[i] != nullptr);

        keyValuePairs.PushBack({ m_keys[i], m_values[i] });
    }

    if (keyValuePairs.Any())
    {
        for (auto& keyValuePair : keyValuePairs)
        {
            keyValuePair.first->Visit(visitor, mod);
            keyValuePair.second->Visit(visitor, mod);

            const SymbolType* keyType = keyValuePair.first->GetExprType();
            const SymbolType* valueType = keyValuePair.second->GetExprType();

            if (!keyType || !valueType)
            {
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_internal_error,
                    m_location));

                continue;
            }

            keyType = keyType->GetUnaliased();
            valueType = valueType->GetUnaliased();

            if (m_keyType->TypeEqual(*BuiltinTypes::s_errorType))
            {
                m_keyType = keyType;
            }
            else
            {
                m_keyType = SymbolType::TypePromotion(m_keyType, keyType);
            }

            if (m_valueType->TypeEqual(*BuiltinTypes::s_errorType))
            {
                m_valueType = valueType;
            }
            else if (!m_valueType->TypeEqual(*valueType))
            {
                m_valueType = SymbolType::TypePromotion(m_valueType, valueType);
            }

            m_replacedKeys.PushBack(CloneAstNode(keyValuePair.first));
            m_replacedValues.PushBack(CloneAstNode(keyValuePair.second));
        }
    }
    else
    {
        m_keyType = BuiltinTypes::s_anyType;
        m_valueType = BuiltinTypes::s_anyType;
    }

    // if either key or value type is undefined, set it to `Any`

    if (m_keyType->TypeEqual(*BuiltinTypes::s_errorType) || m_valueType->TypeEqual(*BuiltinTypes::s_errorType))
    {
        // add error
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_cannot_determine_implicit_type,
            m_location));
    }

    for (size_t i = 0; i < m_replacedKeys.Size(); ++i)
    {
        auto& key = m_replacedKeys[i];
        Assert(key != nullptr);

        auto& value = m_replacedValues[i];
        Assert(value != nullptr);

        auto& replacedKey = m_replacedKeys[i];
        Assert(replacedKey != nullptr);

        auto& replacedValue = m_replacedValues[i];
        Assert(replacedValue != nullptr);

        if (const SymbolType* keyType = key->GetExprType())
        {
            if (!keyType->TypeEqual(*m_keyType))
            {
                // Add cast
                replacedKey = MakeHandle<AstAsExpression>(
                    replacedKey,
                    MakeHandle<AstTypeSpecifier>(
                        MakeHandle<AstTypeRef>(
                            m_keyType,
                            key->GetLocation()),
                        key->GetLocation()),
                    key->GetLocation());
            }
        }

        if (const SymbolType* valueType = value->GetExprType())
        {
            if (!valueType->TypeEqual(*m_valueType))
            {
                // Add cast
                replacedValue = MakeHandle<AstAsExpression>(
                    replacedValue,
                    MakeHandle<AstTypeSpecifier>(
                        MakeHandle<AstTypeRef>(
                            m_valueType,
                            value->GetLocation()),
                        value->GetLocation()),
                    value->GetLocation());
            }
        }
    }

    /// \todo : Cache generic instance types
    m_mapTypeExpr = MakeHandle<AstTemplateInstantiation>(
        MakeHandle<AstTypeSpecifier>(MakeHandle<AstTypeRef>(BuiltinTypes::s_mapType, m_location), m_location),
        Array<Handle<AstTypeSpecifier>> {
            MakeHandle<AstTypeSpecifier>(MakeHandle<AstTypeRef>(m_keyType, m_location), m_location),
            MakeHandle<AstTypeSpecifier>(MakeHandle<AstTypeRef>(m_valueType, m_location), m_location)
        },
        nullptr, // no function return type
        m_location);

    m_mapTypeExpr->Visit(visitor, mod);

    const SymbolType* mapType = m_mapTypeExpr->GetHeldType();

    if (!mapType)
    {
        visitor->ReportInternalError(m_location);

        return;
    }

    mapType = mapType->GetUnaliased();

    m_exprType = mapType;

    m_resolvedMapTypeRef = MakeHandle<AstTypeRef>(m_exprType, m_location);
    m_resolvedMapTypeRef->Visit(visitor, mod);

    Array<Handle<AstExpression>> keyValueArrayExpressions;
    keyValueArrayExpressions.Reserve(m_replacedKeys.Size());

    for (size_t i = 0; i < m_replacedKeys.Size(); ++i)
    {
        auto& key = m_replacedKeys[i];
        Assert(key != nullptr);

        auto& value = m_replacedValues[i];
        Assert(value != nullptr);

        Array<Handle<AstExpression>> keyValuePair;
        keyValuePair.Reserve(2);

        keyValuePair.PushBack(key);
        keyValuePair.PushBack(value);

        keyValueArrayExpressions.PushBack(MakeHandle<AstArrayExpression>(
            keyValuePair,
            m_location));
    }

    m_arrayExpr = MakeHandle<AstArrayExpression>(
        keyValueArrayExpressions,
        m_location);

    m_arrayExpr->Visit(visitor, mod);
}

UniquePtr<Buildable> AstHashMap::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    Assert(m_resolvedMapTypeRef != nullptr);
    chunk->Append(m_resolvedMapTypeRef->Build(visitor, mod));

    // get active register
    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    { // keep ClassRef in memory so we can do Map<K, V>.FromArray(...), so push it to the stack
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    int classStackLocation = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    Assert(m_arrayExpr != nullptr);
    chunk->Append(m_arrayExpr->Build(visitor, mod));

    const uint8 arrayReg = rp;

    // LoadDeref resolves through any reference wrapper created by the outer array
    // build so the PUSH below stores a stable value copy, not a reference to a
    // register that gets overwritten by the class load that follows.
    chunk->Append(BytecodeUtil::Make<LoadDeref>(arrayReg, arrayReg));

    // move array to stack
    {
        auto instrPush = BytecodeUtil::Make<RawOperation<>>();
        instrPush->opcode = PUSH;
        instrPush->Accept<uint8>(rp);
        chunk->Append(std::move(instrPush));
    }

    // increment stack size
    visitor->GetCompilationUnit()->GetInstructionStream().IncStackSize();

    { // load class from stack back into register
        auto storageOperation = BytecodeUtil::Make<StorageOperation>();
        storageOperation->GetBuilder()
            .Load(rp)
            .Local()
            .ByOffset(visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize() - classStackLocation);

        chunk->Append(std::move(storageOperation));
    }

    { // load member Map::FromArray
        static constexpr uint64 FromArrayMethodHash = "FromArray"_sh.GetHashCode().Value();

        chunk->Append(Compiler::LoadMemberFromHash(visitor, mod, FromArrayMethodHash));
    }

    // Here map class and array should be the 2 items on the stack
    // so we call `from`, and the class will be the first arg, and the array will be the second arg

    { // call the `from` method
        chunk->Append(Compiler::BuildCall(
            visitor,
            mod,
            nullptr, // no target -- handled above
            uint8(1) // num args
        ));
    }

    // decrement stack size for array type expr
    chunk->Append(BytecodeUtil::Make<PopLocal>(2));

    // pop array and type from stack
    for (uint32 i = 0; i < 2; i++)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    return chunk;
}

void AstHashMap::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_resolvedMapTypeRef != nullptr)
    {
        m_resolvedMapTypeRef->Optimize(visitor, mod);
    }

    if (m_arrayExpr != nullptr)
    {
        m_arrayExpr->Optimize(visitor, mod);
    }
}

Handle<AstStatement> AstHashMap::Clone() const
{
    return CloneImpl();
}

Tribool AstHashMap::IsTrue() const
{
    return Tribool::True();
}

bool AstHashMap::MayHaveSideEffects() const
{
    // return true because we have calls to __map_new and __map_set
    return true;
}

const SymbolType* AstHashMap::GetExprType() const
{
    if (!m_exprType)
    {
        return BuiltinTypes::s_errorType;
    }

    return m_exprType;
}

} // namespace Hyperion
