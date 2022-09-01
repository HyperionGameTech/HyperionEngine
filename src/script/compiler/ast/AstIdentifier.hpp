#ifndef AST_IDENTIFIER_HPP
#define AST_IDENTIFIER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>

#include <string>

namespace hyperion::compiler {

// forward declaration
class Scope;
class AstTypeObject;

struct AstIdentifierProperties {
    Identifier *m_identifier = nullptr;

    IdentifierType m_identifier_type = IDENTIFIER_TYPE_UNKNOWN;

    bool m_is_in_function = false;
    bool m_is_in_pure_function = false;

    int m_depth = 0;
    Scope *m_function_scope = nullptr;

    // if the found identifier was a type...
    SymbolTypePtr_t m_found_type = nullptr;

    // getters & setters
    Identifier *GetIdentifier() { return m_identifier; }
    const Identifier *GetIdentifier() const { return m_identifier; }
    void SetIdentifier(Identifier *identifier) { m_identifier = identifier; }

    IdentifierType GetIdentifierType() const { return m_identifier_type; }
    void SetIdentifierType(IdentifierType identifier_type) { m_identifier_type = identifier_type; }

    bool IsInFunction() const { return m_is_in_function; }
    bool IsInPureFunction() const { return m_is_in_pure_function; }
    int GetDepth() const { return m_depth; }
    Scope *GetFunctionScope() const { return m_function_scope; }
};

class AstIdentifier : public AstExpression {
public:
    AstIdentifier(const std::string &name,
        const SourceLocation &location);
    virtual ~AstIdentifier() = default;

    void PerformLookup(AstVisitor *visitor, Module *mod);
    void CheckInFunction(AstVisitor *visitor, Module *mod);

    AstIdentifierProperties &GetProperties() { return m_properties; }
    const AstIdentifierProperties &GetProperties() const { return m_properties; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override = 0;
    
    virtual Pointer<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override = 0;
    virtual SymbolTypePtr_t GetExprType() const override = 0;
    virtual ExprAccessBits GetExprAccess() const override;

    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

    virtual const std::string &GetName() const override;

    /** temporary, used for extracting a type object located in the stored value */
    const AstTypeObject *ExtractTypeObject() const;

protected:
    std::string m_name;
    
    AstIdentifierProperties m_properties;

    int GetStackOffset(int stack_size) const;
};

} // namespace hyperion::compiler

#endif
