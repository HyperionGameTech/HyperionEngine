#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace Hyperion {

enum AstClassFlags : uint8
{
    CLASS_FLAG_NONE = 0x0,
    CLASS_FLAG_IS_PROXY = 0x1,
    CLASS_FLAG_IS_ENUM = 0x2,
    CLASS_FLAG_IS_STRUCT = 0x4,
    CLASS_FLAG_ANONYMOUS = 0x8,
    CLASS_FLAG_EXTERN = 0x10
};

HYP_MAKE_ENUM_FLAGS(AstClassFlags);

class AstClass : public AstExpression
{
public:
    AstClass(
        const String& name,
        const RC<AstTypeSpecifier>& baseSpec,
        const Array<RC<AstVariableDeclaration>>& dataMembers,
        const Array<RC<AstVariableDeclaration>>& functionMembers,
        const Array<RC<AstVariableDeclaration>>& staticMembers,
        EnumFlags<AstClassFlags> classFlags,
        const SourceLocation& location);

    AstClass(
        const String& name,
        const SymbolType* baseType,
        const Array<RC<AstVariableDeclaration>>& dataMembers,
        const Array<RC<AstVariableDeclaration>>& functionMembers,
        const Array<RC<AstVariableDeclaration>>& staticMembers,
        EnumFlags<AstClassFlags> classFlags,
        const SourceLocation& location);

    virtual ~AstClass() override = default;

    /** enable setting to that variable declarations can change the type name */
    HYP_FORCE_INLINE void SetName(const String& name)
    {
        m_name = name;
    }

    HYP_FORCE_INLINE Array<RC<AstVariableDeclaration>>& GetDataMembers()
    {
        return m_dataMembers;
    }

    HYP_FORCE_INLINE const Array<RC<AstVariableDeclaration>>& GetDataMembers() const
    {
        return m_dataMembers;
    }

    Array<RC<AstVariableDeclaration>>& GetFunctionMembers()
    {
        return m_functionMembers;
    }

    const Array<RC<AstVariableDeclaration>>& GetFunctionMembers() const
    {
        return m_functionMembers;
    }

    Array<RC<AstVariableDeclaration>>& GetStaticMembers()
    {
        return m_staticMembers;
    }

    const Array<RC<AstVariableDeclaration>>& GetStaticMembers() const
    {
        return m_functionMembers;
    }

    bool IsEnum() const
    {
        return m_flags[CLASS_FLAG_IS_ENUM];
    }

    bool IsProxyClass() const
    {
        return m_flags[CLASS_FLAG_IS_PROXY];
    }

    bool IsAnonymous() const
    {
        return m_flags[CLASS_FLAG_ANONYMOUS];
    }

    bool IsExternClass() const
    {
        return m_flags[CLASS_FLAG_EXTERN];
    }

    void SetPreRegister(bool preRegister)
    {
        m_preRegister = preRegister;
    }

    HYP_FORCE_INLINE const SymbolType* GetBaseType() const
    {
        return m_baseType;
    }

    HYP_FORCE_INLINE SymbolType* GetSymbolType() const
    {
        return m_symbolType;
    }

    void SetSymbolType(SymbolType* symbolType)
    {
        m_symbolType = symbolType;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual const SymbolType* GetExprType() const override;
    virtual const SymbolType* GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual const String& GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstClass>());
        hc.Add(m_name);
        hc.Add(m_baseSpec ? m_baseSpec->GetHashCode() : HashCode());

        for (auto& member : m_dataMembers)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto& member : m_functionMembers)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto& member : m_staticMembers)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        hc.Add(m_flags);

        return hc;
    }

protected:
    String m_name;
    RC<AstTypeSpecifier> m_baseSpec;
    const SymbolType* m_baseType;
    Array<RC<AstVariableDeclaration>> m_dataMembers;
    Array<RC<AstVariableDeclaration>> m_functionMembers;
    Array<RC<AstVariableDeclaration>> m_staticMembers;
    EnumFlags<AstClassFlags> m_flags;

    SymbolType* m_symbolType;

    RC<AstTypeRef> m_typeRef;
    Array<RC<AstVariableDeclaration>> m_outsideMembers;
    Array<RC<AstVariableDeclaration>> m_combinedMembers;
    bool m_preRegister;

    RC<AstClass> CloneImpl() const
    {
        if (m_baseType != nullptr)
        {
            return RC<AstClass>(new AstClass(
                m_name,
                m_baseType,
                CloneAllAstNodes(m_dataMembers),
                CloneAllAstNodes(m_functionMembers),
                CloneAllAstNodes(m_staticMembers),
                m_flags,
                m_location));
        }

        return RC<AstClass>(new AstClass(
            m_name,
            CloneAstNode(m_baseSpec),
            CloneAllAstNodes(m_dataMembers),
            CloneAllAstNodes(m_functionMembers),
            CloneAllAstNodes(m_staticMembers),
            m_flags,
            m_location));
    }
};

} // namespace Hyperion
