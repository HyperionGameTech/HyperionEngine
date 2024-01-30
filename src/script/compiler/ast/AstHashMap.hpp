#ifndef AST_HASH_MAP_HPP
#define AST_HASH_MAP_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <core/lib/DynArray.hpp>

#include <string>

namespace hyperion::compiler {

class AstTypeObject;
class AstPrototypeSpecification;
class AstBlock;

class AstHashMap : public AstExpression
{
public:
    AstHashMap(
        const Array<RC<AstExpression>> &keys,
        const Array<RC<AstExpression>> &values,
        const SourceLocation &location
    );

    virtual ~AstHashMap() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;    
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstHashMap>());
        
        for (SizeType index = 0; index < m_keys.Size(); ++index) {
            hc.Add(m_keys[index] ? m_keys[index]->GetHashCode() : HashCode());

            if (index >= m_values.Size()) {
                hc.Add(HashCode());

                continue;
            }

            hc.Add(m_values[index] ? m_values[index]->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    Array<RC<AstExpression>>        m_keys;
    Array<RC<AstExpression>>        m_values;

    // set while analyzing
    Array<RC<AstExpression>>        m_replaced_keys;
    Array<RC<AstExpression>>        m_replaced_values;
    RC<AstPrototypeSpecification>   m_map_type_expr;
    RC<AstExpression>               m_array_expr;
    SymbolTypePtr_t                 m_key_type;
    SymbolTypePtr_t                 m_value_type;
    SymbolTypePtr_t                 m_expr_type;
    RC<AstTypeObject>               m_type_object;
    RC<AstBlock>                    m_block;

    RC<AstHashMap> CloneImpl() const
    {
        return RC<AstHashMap>(new AstHashMap(
            CloneAllAstNodes(m_keys),
            CloneAllAstNodes(m_values),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
