#ifndef AST_IDENTIFIER_HPP
#define AST_IDENTIFIER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/enums.hpp>

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
    inline Identifier *GetIdentifier() { return m_identifier; }
    inline const Identifier *GetIdentifier() const { return m_identifier; }
    inline void SetIdentifier(Identifier *identifier) { m_identifier = identifier; }

    inline IdentifierType GetIdentifierType() const { return m_identifier_type; }
    inline void SetIdentifierType(IdentifierType identifier_type) { m_identifier_type = identifier_type; }

    inline bool IsInFunction() const { return m_is_in_function; }
    inline bool IsInPureFunction() const { return m_is_in_pure_function; }
    inline int GetDepth() const { return m_depth; }
    inline Scope *GetFunctionScope() const { return m_function_scope; }
};

class AstIdentifier : public AstExpression {
public:
    AstIdentifier(const std::string &name,
        const SourceLocation &location);
    virtual ~AstIdentifier() = default;

    void PerformLookup(AstVisitor *visitor, Module *mod);
    void CheckInFunction(AstVisitor *visitor, Module *mod);

    inline const std::string &GetName() const { return m_name; }
    inline AstIdentifierProperties &GetProperties() { return m_properties; }
    inline const AstIdentifierProperties &GetProperties() const { return m_properties; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override = 0;
    
    virtual Pointer<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override = 0;
    virtual SymbolTypePtr_t GetExprType() const override = 0;

    virtual const AstExpression *GetValueOf() const override;

    /** temporary, used for extracting a type object located in the stored value */
    AstTypeObject *ExtractTypeObject() const;

protected:
    std::string m_name;
    
    AstIdentifierProperties m_properties;

    int GetStackOffset(int stack_size) const;
};

} // namespace hyperion::compiler

#endif
