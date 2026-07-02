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
        const Array<SharedPtr<AstExpression>>& keys,
        const Array<SharedPtr<AstExpression>>& values,
        const SourceLocation& location);

    virtual ~AstHashMap() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

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
    Array<SharedPtr<AstExpression>> m_keys;
    Array<SharedPtr<AstExpression>> m_values;

    // set while analyzing
    Array<SharedPtr<AstExpression>> m_replacedKeys;
    Array<SharedPtr<AstExpression>> m_replacedValues;
    SharedPtr<AstTypeSpecifier> m_mapTypeExpr;
    SharedPtr<AstTypeRef> m_resolvedMapTypeRef;
    SharedPtr<AstExpression> m_arrayExpr;
    const SymbolType* m_keyType;
    const SymbolType* m_valueType;
    const SymbolType* m_exprType;
    SharedPtr<AstBlock> m_block;

    SharedPtr<AstHashMap> CloneImpl() const
    {
        return SharedPtr<AstHashMap>(new AstHashMap(
            CloneAllAstNodes(m_keys),
            CloneAllAstNodes(m_values),
            m_location));
    }
};

} // namespace Hyperion
