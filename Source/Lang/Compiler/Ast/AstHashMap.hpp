#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Core/Containers/Array.hpp>

#include <string>

namespace Hyperion {

class AstTypeSpecifier;
class AstTypeRef;
class AstBlock;

HYP_CLASS()
class AstHashMap : public AstExpression
{
    HYP_OBJECT_BODY(AstHashMap);

public:
    AstHashMap(
        const Array<Handle<AstExpression>>& keys,
        const Array<Handle<AstExpression>>& values,
        const SourceLocation& location);

    virtual ~AstHashMap() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstHashMap>());

        for (size_t index = 0; index < m_keys.Size(); ++index)
        {
            hc.Add(m_keys[index] ? m_keys[index]->GetHashCode() : HashCode());

            if (index >= m_values.Size())
            {
                hc.Add(HashCode());

                continue;
            }

            hc.Add(m_values[index] ? m_values[index]->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    Array<Handle<AstExpression>> m_keys;
    Array<Handle<AstExpression>> m_values;

    // set while analyzing
    Array<Handle<AstExpression>> m_replacedKeys;
    Array<Handle<AstExpression>> m_replacedValues;
    Handle<AstTypeSpecifier> m_mapTypeExpr;
    Handle<AstTypeRef> m_resolvedMapTypeRef;
    Handle<AstExpression> m_arrayExpr;
    const SymbolType* m_keyType;
    const SymbolType* m_valueType;
    const SymbolType* m_exprType;
    Handle<AstBlock> m_block;

    Handle<AstHashMap> CloneImpl() const
    {
        return MakeHandle<AstHashMap>(
            CloneAllAstNodes(m_keys),
            CloneAllAstNodes(m_values),
            m_location);
    }
};

} // namespace Hyperion
