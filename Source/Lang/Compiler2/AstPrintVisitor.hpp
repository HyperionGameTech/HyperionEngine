#pragma once

#include <Lang/compiler/AstVisitor.hpp>
#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>

namespace Hyperion {

class AstPrintVisitor : public AstVisitor
{
public:
    AstPrintVisitor(
        AstIterator* astIterator,
        CompilationUnit* compilationUnit,
        size_t indentSize = 2);

    virtual ~AstPrintVisitor() = default;

    WideString PrintNode(AstStatement* node, size_t depth = 0) const;
    WideString PrintTree(AstStatement* root) const;

    WideString GetIndentation(size_t depth) const;

    void SetShowTypes(bool show)
    {
        m_showTypes = show;
    }

    void SetShowLocations(bool show)
    {
        m_showLocations = show;
    }

    void SetUseColors(bool use)
    {
        m_useColors = use;
    }

    void SetShowDetails(bool show)
    {
        m_showDetails = show;
    }

private:
    WideString FormatNodeName(const WideString& nodeName) const;
    WideString FormatValue(const WideString& value) const;
    WideString FormatType(const WideString& type) const;
    WideString FormatLocation(const SourceLocation& location) const;

    WideString GetNodeDescription(AstStatement* node) const;
    WideString GetBasicNodeInfo(AstStatement* node) const;
    WideString GetTypeInfo(AstStatement* node) const;
    WideString GetDetailedInfo(AstStatement* node, size_t depth) const;
    WideString PrintChildren(AstStatement* node, size_t depth) const;
    Array<AstStatement*> GetChildNodes(AstStatement* node) const;
    WideString Colorize(const WideString& text, const WideString& colorCode) const;

    size_t m_indentSize;

    bool m_showTypes : 1;
    bool m_showLocations : 1;
    bool m_useColors : 1;
    bool m_showDetails : 1;
};

} // namespace Hyperion
