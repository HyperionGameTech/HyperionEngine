/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/ParseUtil.hpp>

#include <regex>
#include <string>
#include <sstream>

namespace hyperion {
namespace buildtool {

Optional<String> ExtractCXXClassName(const String& line)
{
    static const std::regex pattern(
        "(?:class|struct|(?:enum class)|enum)\\s+(?:alignas\\(.*\\)\\s+)?(?:HYP_API\\s+)?(\\w+)");

    std::string str = line.Data();
    std::smatch match;

    if (std::regex_search(str, match, pattern))
    {
        return match[1].str().c_str();
    }

    return {};
}

Array<String> ExtractCXXBaseClasses(const String& line)
{
    static const std::regex pattern(
        "((?:class|struct)\\s+(?:alignas\\(.*\\)\\s+)?(?:HYP_API\\s+)?(?:\\w+)\\s*(?:final)?\\s*:\\s*((?:public|private|protected)?\\s*(?:\\w+\\s*,?\\s*)+))");

    Array<String> results;

    std::string str = line.Data();
    std::smatch match;

    if (std::regex_search(str, match, pattern))
    {
        std::string base_classes = match[1].str();
        std::stringstream ss(base_classes);
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

} // namespace buildtool
} // namespace hyperion