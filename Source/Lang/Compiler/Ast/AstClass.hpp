#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>

#include <Core/Utilities/EnumFlags.hpp>

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

HYP_CLASS()
class AstClass : public AstExpression
{
    HYP_OBJECT_BODY(AstClass);

public:
    AstClass(
        const String& name,
        const Handle<AstTypeSpecifier>& baseSpec,
        const Array<Handle<AstVariableDeclaration>>& dataMembers,
        const Array<Handle<AstVariableDeclaration>>& functionMembers,
        const Array<Handle<AstVariableDeclaration>>& staticMembers,
        EnumFlags<AstClassFlags> classFlags,
        const SourceLocation& location);

    AstClass(
        const String& name,
        const SymbolType* baseType,
        const Array<Handle<AstVariableDeclaration>>& dataMembers,
        const Array<Handle<AstVariableDeclaration>>& functionMembers,
        const Array<Handle<AstVariableDeclaration>>& staticMembers,
        EnumFlags<AstClassFlags> classFlags,
        const SourceLocation& location);

    virtual ~AstClass() override;

    /** enable setting to that variable declarations can change the type name */
    HYP_FORCE_INLINE void SetName(const String& name)
    {
        m_name = name;
    }

    HYP_FORCE_INLINE Array<Handle<AstVariableDeclaration>>& GetDataMembers()
    {
        return m_dataMembers;
    }

    HYP_FORCE_INLINE const Array<Handle<AstVariableDeclaration>>& GetDataMembers() const
    {
        return m_dataMembers;
    }

    HYP_FORCE_INLINE Array<Handle<AstVariableDeclaration>>& GetFunctionMembers()
    {
        return m_functionMembers;
    }

    HYP_FORCE_INLINE const Array<Handle<AstVariableDeclaration>>& GetFunctionMembers() const
    {
        return m_functionMembers;
    }

    HYP_FORCE_INLINE Array<Handle<AstVariableDeclaration>>& GetStaticMembers()
    {
        return m_staticMembers;
    }

    HYP_FORCE_INLINE const Array<Handle<AstVariableDeclaration>>& GetStaticMembers() const
    {
        return m_staticMembers;
    }

    HYP_FORCE_INLINE bool IsEnum() const
    {
        return m_flags[CLASS_FLAG_IS_ENUM];
    }

    HYP_FORCE_INLINE bool IsStruct() const
    {
        return m_flags[CLASS_FLAG_IS_STRUCT];
    }

    HYP_FORCE_INLINE bool IsProxyClass() const
    {
        return m_flags[CLASS_FLAG_IS_PROXY];
    }

    HYP_FORCE_INLINE bool IsAnonymous() const
    {
        return m_flags[CLASS_FLAG_ANONYMOUS];
    }

    HYP_FORCE_INLINE bool IsExternClass() const
    {
        return m_flags[CLASS_FLAG_EXTERN];
    }

    HYP_FORCE_INLINE void SetPreRegister(bool preRegister)
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

    HYP_FORCE_INLINE const Handle<AstVariableDeclaration>& GetClassRefDecl() const
    {
        return m_refDecl;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override;
    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstTypeSpecifier> m_baseSpec;
    const SymbolType* m_baseType;
    Array<Handle<AstVariableDeclaration>> m_dataMembers;
    Array<Handle<AstVariableDeclaration>> m_functionMembers;
    Array<Handle<AstVariableDeclaration>> m_staticMembers;
    EnumFlags<AstClassFlags> m_flags;

    SymbolType* m_symbolType;

    Handle<AstTypeRef> m_typeRef;
    Array<Handle<AstVariableDeclaration>> m_outsideMembers;
    Array<Handle<AstVariableDeclaration>> m_combinedMembers;
    bool m_preRegister;

    // ClassRef variable decl, lives on the stack
    Handle<AstVariableDeclaration> m_refDecl;
    Handle<AstTypeRef> m_baseTypeRef;

    Handle<AstClass> CloneImpl() const
    {
        if (m_baseType != nullptr)
        {
            return MakeHandle<AstClass>(
                m_name,
                m_baseType,
                CloneAllAstNodes(m_dataMembers),
                CloneAllAstNodes(m_functionMembers),
                CloneAllAstNodes(m_staticMembers),
                m_flags,
                m_location);
        }

        return MakeHandle<AstClass>(
            m_name,
            CloneAstNode(m_baseSpec),
            CloneAllAstNodes(m_dataMembers),
            CloneAllAstNodes(m_functionMembers),
            CloneAllAstNodes(m_staticMembers),
            m_flags,
            m_location);
    }
};

} // namespace Hyperion
