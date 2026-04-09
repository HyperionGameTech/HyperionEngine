/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <util/Util.hpp>

#include <Core/Defines.hpp>

#include <Core/utilities/Format.hpp>

#include <Core/io/BufferedByteReader.hpp>

#include <Core/reflection/ClassAttribute.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <regex>
#include <string>
#include <sstream>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

Optional<String> ExtractCXXClassName(const String& line)
{
    static const std::regex s_pattern(
        "(?:class|struct|(?:enum class)|enum)\\s+(?:alignas\\(.*\\)\\s+)?(?:HYP_API\\s+)?(\\w+)");

    std::string str = line.Data();
    std::smatch match;

    if (std::regex_search(str, match, s_pattern))
    {
        return match[1].str().c_str();
    }

    return {};
}

Array<String> ExtractCXXBaseClasses(const String& line)
{
    static const std::regex s_pattern(
        "((?:class|struct|(?:enum class)|enum)\\s+(?:alignas\\(.*\\)\\s+)?(?:HYP_API\\s+)?(?:\\w+)\\s*(?:final)?\\s*:\\s*((?:public|private|protected)?\\s*(?:\\w+\\s*,?\\s*)+))");

    Array<String> results;

    std::string str = line.Data();
    std::smatch match;

    if (std::regex_search(str, match, s_pattern))
    {
        std::string baseClasses = match[1].str();
        std::stringstream ss(baseClasses);
        std::string part;

        while (std::getline(ss, part, ','))
        {
            std::stringstream s2(part);
            std::string token, last;

            while (s2 >> token)
            {
                last = token;
            }

            if (!last.empty())
            {
                results.PushBack(last.c_str());
            }
        }
    }
    return results;
}

Array<String> ExtractCXXNamespacePath(const String& source)
{
    Array<String> namespacePath;               // Accumulated path of active namespaces
    Array<uint32> blockNamespaceSegmentCounts; // Stack of counts per '{' indicating how many namespace segments to pop on '}'

    // Track parsing state to ignore strings and comments
    int isInComment = 0; // 0 = none, 1 = line, 2 = block
    bool isInString = false;
    bool isEscaped = false;

    UTF8StringView sv = source;

    for (auto it = sv.Begin(); it != sv.End(); ++it)
    {
        const utf::Char32 ch = *it;

        if (ch == 0)
        {
            break;
        }

        // Handle escape inside string
        if (isEscaped)
        {
            isEscaped = false;
            continue;
        }

        // String/comment handling
        if (ch == '\\')
        {
            if (isInString)
            {
                isEscaped = true;
            }
            continue;
        }

        if (ch == '"' && isInComment == 0)
        {
            isInString = !isInString;
            continue;
        }

        if (!isInString)
        {
            if (isInComment == 1) // line comment
            {
                if (ch == '\n')
                {
                    isInComment = 0;
                }
                continue;
            }
            else if (isInComment == 2) // block comment
            {
                if (it + 1 != sv.End() && ch == '*' && *(it + 1) == '/')
                {
                    isInComment = 0;
                    ++it; // consume '/'
                }
                continue;
            }

            // Entering a comment?
            if (it + 1 != sv.End() && ch == '/')
            {
                const utf::Char32 next = *(it + 1);
                if (next == '/')
                {
                    isInComment = 1; // line comment
                    ++it;            // consume second '/'
                    continue;
                }
                if (next == '*')
                {
                    isInComment = 2; // block comment
                    ++it;            // consume '*'
                    continue;
                }
            }

            // Detect 'namespace' keyword (ensure word boundaries)
            if (ch == 'n')
            {
                // Ensure we have at least 9 characters remaining and exact match
                auto ns_end_it = it + 9;
                if (ns_end_it <= sv.End() && sv.Substr(it, ns_end_it) == UTF8StringView("namespace"))
                {
                    // Check preceding and following boundaries to avoid matching within identifiers
                    bool valid_prefix = true; // Can't reliably look behind with this iterator type; allow and rely on '{' check below

                    bool valid_suffix = true;
                    if (ns_end_it != sv.End())
                    {
                        utf::Char32 after = *ns_end_it;
                        if (std::isalnum(after) || after == '_')
                        {
                            valid_suffix = false;
                        }
                    }

                    if (valid_prefix && valid_suffix)
                    {
                        // Advance iterator to the character after 'namespace'
                        it = ns_end_it;

                        // Skip whitespace
                        while (it != sv.End() && std::isspace(*it))
                        {
                            ++it;
                        }

                        // Handle anonymous namespace: 'namespace { '
                        if (it != sv.End() && *it == '{')
                        {
                            // Anonymous namespace does not contribute to named path, but must track brace for proper popping
                            blockNamespaceSegmentCounts.PushBack(0);
                            // consume '{'
                            continue;
                        }

                        // Parse qualified name: foo::bar
                        Array<String> segments;
                        String current;

                        while (it != sv.End())
                        {
                            const utf::Char32 c = *it;
                            if (std::isalnum(c) || c == '_')
                            {
                                current.Append(c);
                                ++it;
                                continue;
                            }
                            // scope resolution '::'
                            if (c == ':' && (it + 1) != sv.End() && *(it + 1) == ':')
                            {
                                if (!current.Empty())
                                {
                                    segments.PushBack(current);
                                    current.Clear();
                                }
                                it += 2; // consume '::'
                                continue;
                            }
                            break;
                        }

                        if (!current.Empty())
                        {
                            segments.PushBack(current);
                            current.Clear();
                        }

                        // Skip whitespace after name
                        while (it != sv.End() && std::isspace(*it))
                        {
                            ++it;
                        }

                        // Namespace alias: namespace foo = bar::baz; -> ignore
                        if (it != sv.End() && *it == '=')
                        {
                            // Fast-forward to ';'
                            while (it != sv.End() && *it != ';')
                            {
                                ++it;
                            }
                            continue;
                        }

                        // Only open a namespace when followed by '{'
                        if (it != sv.End() && *it == '{')
                        {
                            // Push all parsed segments
                            for (const String& seg : segments)
                            {
                                if (!seg.Empty())
                                {
                                    namespacePath.PushBack(seg);
                                }
                            }

                            blockNamespaceSegmentCounts.PushBack(uint32(segments.Size()));

                            // Do not push another generic block for this '{' since we've accounted for it
                            continue;
                        }

                        // Forward declaration like 'namespace foo::bar;' -> no-op
                        continue;
                    }
                }
            }

            // Generic brace handling for non-namespace blocks
            if (ch == '{')
            {
                // Entered a non-namespace block; push 0 so that '}' pops nothing from namespace path
                blockNamespaceSegmentCounts.PushBack(0);
                continue;
            }

            if (ch == '}')
            {
                if (!blockNamespaceSegmentCounts.Empty())
                {
                    const uint32 segmentCount = blockNamespaceSegmentCounts.PopBack();
                    
                    for (uint32 n = 0; n < segmentCount && !namespacePath.Empty(); ++n)
                    {
                        namespacePath.PopBack();
                    }
                }
                continue;
            }
        }
    }

    return namespacePath;
}

String BuildNamespaceString(Span<const String> namespaceParts)
{
    String str;
    for (size_t i = 0; i < namespaceParts.Size(); i++)
    {
        str += namespaceParts[i];

        if (i + 1 < namespaceParts.Size())
        {
            str += "::";
        }
    }

    return str;
}

bool IsCXXClassDecl(const String& line)
{
    static const std::regex s_pattern(
        "(?:class)\\s+(?:alignas\\(.*\\)\\s+)?(?:HYP_API\\s+)?(\\w+)");

    std::string str = line.Data();
    std::smatch match;

    return std::regex_search(str, match, s_pattern);
}

bool IsCXXStructDecl(const String& line)
{
    static const std::regex s_pattern(
        "(?:struct)\\s+(?:alignas\\(.*\\)\\s+)?(?:HYP_API\\s+)?(\\w+)");

    std::string str = line.Data();
    std::smatch match;

    return std::regex_search(str, match, s_pattern);
}

bool IsCXXEnumDecl(const String& line)
{
    static const std::regex s_pattern(
        "(?:enum|(?:enum class))\\s+(\\w+)");

    std::string str = line.Data();
    std::smatch match;

    return std::regex_search(str, match, s_pattern);
}

bool IsCXXEnumClassDecl(const String& line)
{
    static const std::regex s_pattern(
        "(?:enum class)\\s+(\\w+)");

    std::string str = line.Data();
    std::smatch match;

    return std::regex_search(str, match, s_pattern);
}

String GetDateTimeString()
{
    char buf[80];
#ifdef HYP_UNIX
    time_t now = time(nullptr);
    struct tm tstruct;
    localtime_r(&now, &tstruct);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

#elif defined(HYP_WINDOWS)
    std::time_t now = std::time(nullptr);
    std::tm tstruct;
    localtime_s(&tstruct, &now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
#else
#error Platform not supported
#endif

    return String(buf);
}

String GetGeneratedFilePreamble(const String& srcPath)
{
    // replace slashes with portable '/' to reduce diffs
    return HYP_FORMAT("/* Auto-generated by Hyperion CodeGen v{}.{}.{}. Do not modify this file directly.\n"
                      " * Source: {}\n"
                      " */\n\n",
        HYP_CODEGEN_VERSION_MAJOR,
        HYP_CODEGEN_VERSION_MINOR,
        HYP_CODEGEN_VERSION_PATCH,
        srcPath.Length() ? srcPath.ReplaceAll("\\", "/") : "<no source file>");
}

static bool AreFileContentsSame(const FilePath& pathA, const FilePath& pathB)
{
    FileBufferedReaderSource sourceA { pathA };
    FileBufferedReaderSource sourceB { pathB };

    BufferedByteReader readerA { &sourceA };
    BufferedByteReader readerB { &sourceB };

    if (!readerA.IsOpen() || !readerB.IsOpen())
    {
        return false;
    }

    const size_t BufferSize = 4096;
    ByteBuffer bufferA(BufferSize);
    ByteBuffer bufferB(BufferSize);

    while (true)
    {
        size_t bytesReadA = readerA.ReadBytes(bufferA.Data(), BufferSize);
        size_t bytesReadB = readerB.ReadBytes(bufferB.Data(), BufferSize);

        if (bytesReadA != bytesReadB)
        {
            return false;
        }

        if (bytesReadA == 0)
        {
            break; // reached end of both files
        }

        if (Memory::Compare(bufferA.Data(), bufferB.Data(), bytesReadA) != 0)
        {
            return false;
        }
    }

    return true;
}

Result ReplaceFileIfDifferent(FilePath& tempFilePath, const FilePath& targetFilePath)
{
    if (!targetFilePath.Exists())
    {
        // target file doesn't exist; rename temp to target
        bool renamed = tempFilePath.Rename(targetFilePath);

        if (!renamed)
        {
            return HYP_MAKE_ERROR(Error, "Failed to rename file: {} to {}", tempFilePath, targetFilePath);
        }

        tempFilePath = targetFilePath;

        return {};
    }

    // if file sizes differ, replace the original
    const size_t targetFileSize = targetFilePath.FileSizeOnDisk();
    const size_t newFileSize = tempFilePath.FileSizeOnDisk();

    if (targetFileSize != newFileSize || !AreFileContentsSame(targetFilePath, tempFilePath))
    {
        bool result = targetFilePath.Remove();
        if (!result)
        {
            return HYP_MAKE_ERROR(Error, "Failed to remove file: {}", targetFilePath);
        }

        result = tempFilePath.Rename(targetFilePath);
        if (!result)
        {
            return HYP_MAKE_ERROR(Error, "Failed to rename file: {} to {}", tempFilePath, targetFilePath);
        }

        tempFilePath = targetFilePath;

        return {};
    }

    // sizes are the same; just remove the temp file
    bool result = tempFilePath.Remove();
    if (!result)
    {
        return HYP_MAKE_ERROR(Error, "Failed to remove file: {}", tempFilePath);
    }

    tempFilePath = targetFilePath;

    return {};
}

bool CheckAttrCSV(const ClassAttributeValue& attrValue, const String& expected)
{
    if (!attrValue.IsValid() || !attrValue.IsString())
    {
        return false;
    }

    String expectedLower = expected.ToLower();

    String value = attrValue.GetString();
    Array<String> parts = value.Split(',');

    for (String& part : parts)
    {
        part = part.Trimmed();
        part = part.ToLower();

        if (part == expectedLower)
        {
            return true;
        }
    }

    return false;

}

} // namespace CodeGen
} // namespace Hyperion