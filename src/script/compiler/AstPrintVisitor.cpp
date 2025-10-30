#include <script/compiler/AstPrintVisitor.hpp>
#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstConstant.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstIfStatement.hpp>
#include <script/compiler/ast/AstForLoop.hpp>
#include <script/compiler/ast/AstWhileLoop.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstClass.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstHashMap.hpp>
#include <script/compiler/ast/AstDirective.hpp>
#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstBreakStatement.hpp>
#include <script/compiler/ast/AstContinueStatement.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>

namespace hyperion {

static const WideString s_colorReset = L"\033[0m";
static const WideString s_colorNodeName = L"\033[1;96m";
static const WideString s_colorValue = L"\033[1;92m";
static const WideString s_colorType = L"\033[1;95m";
static const WideString s_colorLocation = L"\033[38;5;240m";
static const WideString s_colorKeyword = L"\033[1;94m";
static const WideString s_colorComment = L"\033[38;5;245m";

AstPrintVisitor::AstPrintVisitor(
    AstIterator* astIterator,
    CompilationUnit* compilationUnit,
    SizeType indentSize)
    : AstVisitor(astIterator, compilationUnit),
      m_indentSize(indentSize),
      m_showTypes(true),
      m_showLocations(false),
      m_useColors(true),
      m_showDetails(false)
{
}

WideString AstPrintVisitor::PrintNode(AstStatement* node, SizeType depth) const
{
    if (!node)
    {
        return GetIndentation(depth) + Colorize(L"├─ <null>", s_colorComment) + L"\n";
    }

    WideString result;
    const WideString indentation = GetIndentation(depth);

    // Create beautiful tree connectors
    WideString connector = depth == 0 ? L"┌─ " : L"├─ ";

    // Start with node name
    WideString nodeName = GetNodeDescription(node);
    result += indentation + Colorize(connector, s_colorComment) + FormatNodeName(nodeName);

    WideString nodeInfo = GetBasicNodeInfo(node);

    if (!nodeInfo.Empty())
    {
        result += L" " + nodeInfo;
    }

    // Add type information if enabled
    if (m_showTypes)
    {
        WideString typeInfo = GetTypeInfo(node);
        if (!typeInfo.Empty())
        {
            result += L" " + FormatType(typeInfo);
        }
    }

    // Add location information if enabled
    if (m_showLocations)
    {
        result += L" " + FormatLocation(node->GetLocation());
    }

    result += L"\n";

    // Add detailed information if enabled
    if (m_showDetails)
    {
        WideString details = GetDetailedInfo(node, depth);
        if (!details.Empty())
        {
            result += details;
        }
    }

    result += PrintChildren(node, depth);

    return result;
}

WideString AstPrintVisitor::PrintTree(AstStatement* root) const
{
    if (!root)
    {
        return Colorize(L"(empty tree)", s_colorComment) + L"\n";
    }

    WideString result;

    result += Colorize(L"╔══════════════════════════════════════╗", s_colorKeyword) + L"\n";
    result += Colorize(L"║              AST Tree                ║", s_colorKeyword) + L"\n";
    result += Colorize(L"╚══════════════════════════════════════╝", s_colorKeyword) + L"\n\n";

    result += PrintNode(root, 0);
    result += L"\n" + Colorize(L"└─ " + Colorize(L"End of tree", s_colorComment), s_colorComment) + L"\n";

    return result;
}

WideString AstPrintVisitor::GetIndentation(SizeType depth) const
{
    WideString result;

    for (SizeType i = 0; i < depth * m_indentSize; ++i)
    {
        result += L" ";
    }

    return result;
}

WideString AstPrintVisitor::FormatNodeName(const WideString& nodeName) const
{
    return Colorize(nodeName, s_colorNodeName);
}

WideString AstPrintVisitor::FormatValue(const WideString& value) const
{
    return Colorize(value, s_colorValue);
}

WideString AstPrintVisitor::FormatType(const WideString& type) const
{
    return Colorize(L"[" + type + L"]", s_colorType);
}

WideString AstPrintVisitor::FormatLocation(const SourceLocation& location) const
{
    return Colorize(L"@" + WideString::ToString(location.GetLine()) + L":" + WideString::ToString(location.GetColumn()), s_colorLocation);
}

WideString AstPrintVisitor::Colorize(const WideString& text, const WideString& colorCode) const
{
    if (!m_useColors)
    {
        return text;
    }
    return colorCode + text + s_colorReset;
}

WideString AstPrintVisitor::GetNodeDescription(AstStatement* node) const
{
    if (dynamic_cast<AstBinaryExpression*>(node))
    {
        return L"BinaryExpression";
    }

    if (dynamic_cast<AstUnaryExpression*>(node))
    {
        return L"UnaryExpression";
    }

    if (dynamic_cast<AstCallExpression*>(node))
    {
        return L"CallExpression";
    }

    if (dynamic_cast<AstMemberCallExpression*>(node))
    {
        return L"MemberCallExpression";
    }

    if (dynamic_cast<AstIdentifier*>(node))
    {
        return L"Identifier";
    }

    if (dynamic_cast<AstInteger*>(node))
    {
        return L"Integer";
    }

    if (dynamic_cast<AstFloat*>(node))
    {
        return L"Float";
    }

    if (dynamic_cast<AstString*>(node))
    {
        return L"WideString";
    }

    if (dynamic_cast<AstBlock*>(node))
    {
        return L"Block";
    }

    if (dynamic_cast<AstIfStatement*>(node))
    {
        return L"IfStatement";
    }

    if (dynamic_cast<AstForLoop*>(node))
    {
        return L"ForLoop";
    }

    if (dynamic_cast<AstWhileLoop*>(node))
    {
        return L"WhileLoop";
    }

    if (dynamic_cast<AstFunctionExpression*>(node))
    {
        return L"FunctionExpression";
    }

    if (dynamic_cast<AstClass*>(node))
    {
        return L"Class";
    }

    if (dynamic_cast<AstVariableDeclaration*>(node))
    {
        return L"VariableDeclaration";
    }

    if (dynamic_cast<AstReturnStatement*>(node))
    {
        return L"ReturnStatement";
    }

    if (dynamic_cast<AstMember*>(node))
    {
        return L"Member";
    }

    if (dynamic_cast<AstArrayExpression*>(node))
    {
        return L"ArrayExpression";
    }

    if (dynamic_cast<AstArrayAccess*>(node))
    {
        return L"ArrayAccess";
    }

    if (dynamic_cast<AstHashMap*>(node))
    {
        return L"HashMap";
    }

    if (dynamic_cast<AstDirective*>(node))
    {
        return L"Directive";
    }

    if (dynamic_cast<AstImport*>(node))
    {
        return L"Import";
    }

    if (dynamic_cast<AstModuleDeclaration*>(node))
    {
        return L"ModuleDeclaration";
    }

    if (dynamic_cast<AstBreakStatement*>(node))
    {
        return L"BreakStatement";
    }

    if (dynamic_cast<AstContinueStatement*>(node))
    {
        return L"ContinueStatement";
    }

    if (dynamic_cast<AstTernaryExpression*>(node))
    {
        return L"TernaryExpression";
    }

    return L"AstNode";
}

WideString AstPrintVisitor::GetBasicNodeInfo(AstStatement* node) const
{
    WideString result;

    if (auto* identifier = dynamic_cast<AstIdentifier*>(node))
    {
        result += FormatValue(L"'" + identifier->GetName().ToWide() + L"'");
    }
    else if (auto* integer = dynamic_cast<AstInteger*>(node))
    {
        result += FormatValue(integer->ToString().ToWide());
    }
    else if (auto* floatNode = dynamic_cast<AstFloat*>(node))
    {
        result += FormatValue(floatNode->ToString().ToWide());
    }
    else if (auto* stringNode = dynamic_cast<AstString*>(node))
    {
        result += FormatValue(stringNode->ToString().ToWide());
    }
    else if (auto* varDecl = dynamic_cast<AstVariableDeclaration*>(node))
    {
        result += FormatValue(L"'" + varDecl->GetName().ToWide() + L"'");
    }
    else
    {
        // For other nodes, try to get a basic string representation
        WideString nodeString = node->ToString().ToWide();
        if (!nodeString.Empty() && nodeString.Size() < 100)
        {
            result += FormatValue(L"(" + nodeString + L")");
        }
    }

    return result;
}

WideString AstPrintVisitor::GetTypeInfo(AstStatement* node) const
{
    if (AstExpression* expr = dynamic_cast<AstExpression*>(node))
    {
        const SymbolType* symbolType = expr->GetExprType();

        if (symbolType)
        {
            return symbolType->ToString(true).ToWide();
        }
    }

    return WideString::empty;
}

WideString AstPrintVisitor::GetDetailedInfo(AstStatement* node, SizeType depth) const
{
    WideString result;
    const WideString indentation = GetIndentation(depth + 1);

    if (auto* callExpr = dynamic_cast<AstCallExpression*>(node))
    {
        result += indentation
            + Colorize(L"│ ", s_colorComment)
            + Colorize(L"Arguments: ", s_colorComment)
            + FormatValue(WideString::ToString(callExpr->GetArguments().Size())) + L"\n";
    }
    else if (auto* block = dynamic_cast<AstBlock*>(node))
    {
        result += indentation
            + Colorize(L"│ ", s_colorComment)
            + Colorize(L"Statements: ", s_colorComment)
            + FormatValue(WideString::ToString(block->GetChildren().Size())) + L"\n";
    }
    else if (auto* varDecl = dynamic_cast<AstVariableDeclaration*>(node))
    {
        result += indentation
            + Colorize(L"│ ", s_colorComment)
            + Colorize(L"Variable: ", s_colorComment)
            + FormatValue(L"'" + varDecl->GetName().ToWide() + L"'") + L"\n";
    }

    return result;
}

WideString AstPrintVisitor::PrintChildren(AstStatement* node, SizeType depth) const
{
    WideString result;
    Array<AstStatement*> children = GetChildNodes(node);

    for (SizeType i = 0; i < children.Size(); ++i)
    {
        AstStatement* child = children[i];
        const bool isLast = (i == children.Size() - 1);

        if (child)
        {
            WideString childResult = PrintNode(child, depth + 1);

            if (isLast && depth > 0)
            {
                SizeType pos = childResult.FindFirstIndex(L"├─");

                if (pos != WideString::NotFound)
                {
                    childResult = childResult.Substr(0, pos) + L"└─" + childResult.Substr(pos + 2);
                }
            }

            result += childResult;
        }
        else
        {
            const WideString connector = isLast ? L"└─ " : L"├─ ";
            result += GetIndentation(depth + 1) + Colorize(connector, s_colorComment) + Colorize(L"<null child>", s_colorComment) + L"\n";
        }
    }

    return result;
}

Array<AstStatement*> AstPrintVisitor::GetChildNodes(AstStatement* node) const
{
    Array<AstStatement*> children;

    if (auto* moduleDecl = dynamic_cast<AstModuleDeclaration*>(node))
    {
        for (const auto& child : moduleDecl->GetChildren())
        {
            children.PushBack(child.Get());
        }
    }
    else if (auto* binExpr = dynamic_cast<AstBinaryExpression*>(node))
    {
        children.PushBack(binExpr->GetLeft().Get());
        children.PushBack(binExpr->GetRight().Get());
    }
    else if (auto* callExpr = dynamic_cast<AstCallExpression*>(node))
    {
        for (const auto& arg : callExpr->GetArguments())
        {
            if (arg && arg->GetExpr())
            {
                children.PushBack(arg->GetExpr().Get());
            }
        }
    }
    else if (auto* memberCall = dynamic_cast<AstMemberCallExpression*>(node))
    {
        if (memberCall->GetTarget())
        {
            children.PushBack(memberCall->GetTarget());
        }

        if (const RC<AstArgumentList>& argumentList = memberCall->GetArguments())
        {
            for (const auto& arg : argumentList->GetArguments())
            {
                if (arg && arg->GetExpr())
                {
                    children.PushBack(arg->GetExpr().Get());
                }
            }
        }
    }
    else if (auto* block = dynamic_cast<AstBlock*>(node))
    {
        for (const auto& child : block->GetChildren())
        {
            children.PushBack(child.Get());
        }
    }
    else if (auto* ifStmt = dynamic_cast<AstIfStatement*>(node))
    {
        children.PushBack(ifStmt->GetConditional().Get());
        children.PushBack(ifStmt->GetBlock().Get());
        if (ifStmt->GetElseBlock())
        {
            children.PushBack(ifStmt->GetElseBlock().Get());
        }
    }
    else if (auto* varDecl = dynamic_cast<AstVariableDeclaration*>(node))
    {
        if (varDecl->GetAssignment())
        {
            children.PushBack(varDecl->GetAssignment().Get());
        }
    }
    else if (auto* returnStmt = dynamic_cast<AstReturnStatement*>(node))
    {
        if (returnStmt->GetExpression())
        {
            children.PushBack(returnStmt->GetExpression().Get());
        }
    }
    else if (auto* member = dynamic_cast<AstMember*>(node))
    {
        if (member->GetTarget())
        {
            children.PushBack(member->GetTarget());
        }
    }
    else if (auto* arrayExpr = dynamic_cast<AstArrayExpression*>(node))
    {
        for (const auto& element : arrayExpr->GetMembers())
        {
            children.PushBack(element.Get());
        }
    }
    else if (auto* arrayAccess = dynamic_cast<AstArrayAccess*>(node))
    {
        children.PushBack(arrayAccess->GetTarget());
        children.PushBack(arrayAccess->GetIndex());
    }

    return children;
}

} // namespace hyperion
