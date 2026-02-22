#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <Core/containers/Array.hpp>

#include <string>

namespace Hyperion {

class AstTypeSpecifier;
class AstBlock;

class AstHashMap : public AstExpression
{
public:
    AstHashMap(
        const Array<RC<AstExpression>>& keys,
        const Array<RC<AstExpression>>& values,
        const SourceLocation& location);

    virtual ~AstHashMap() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstHashMap>());

        for (SizeType index = 0; index < m_keys.Size(); ++index)
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
    Array<RC<AstExpression>> m_keys;
    Array<RC<AstExpression>> m_values;

    // set while analyzing
    Array<RC<AstExpression>> m_replacedKeys;
    Array<RC<AstExpression>> m_replacedValues;
    RC<AstTypeSpecifier> m_mapTypeExpr;
    RC<AstExpression> m_arrayExpr;
    const SymbolType* m_keyType;
    const SymbolType* m_valueType;
    const SymbolType* m_exprType;
    RC<AstBlock> m_block;

    RC<AstHashMap> CloneImpl() const
    {
        return RC<AstHashMap>(new AstHashMap(
            CloneAllAstNodes(m_keys),
            CloneAllAstNodes(m_values),
            m_location));
    }
};

} // namespace Hyperion
