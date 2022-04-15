#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>
#include <cctype>
#include <algorithm>

namespace hyperion {

class StringUtil {
public:
    static std::string ToLower(const std::string &str)
    {
        std::string result(str);

        std::transform(result.begin(), result.end(), result.begin(), [](char ch) { return std::tolower(ch); });

        return result;
    }

    static std::string ToUpper(const std::string &str)
    {
        std::string result(str);

        std::transform(result.begin(), result.end(), result.begin(), [](char ch) { return std::toupper(ch); });

        return result;
    }

    static constexpr bool StartsWith(const std::string &text, const std::string &token)
    {
        if (text.length() < token.length()) {
            return false;
        }
        return (text.compare(0, token.length(), token) == 0);
    }

    static constexpr bool EndsWith(const std::string &text, const std::string &token)
    {
        if (text.length() < token.length()) {
            return false;
        }
        return std::equal(text.begin() + text.size() - token.size(),
            text.end(), token.begin());
    }

    static inline bool Contains(const std::string &text, const std::string &token)
    {
        return text.find(token) != std::string::npos;
    }

    template <class LambdaFunction>
    static inline void SplitBuffered(const std::string &text, char sep, LambdaFunction func)
    {
        std::string accum;
        accum.reserve(1024);

        for (char ch : text) {
            if (ch == sep) {
                func(accum);
                accum.clear();

                continue;
            }

            accum += ch;
        }

        if (accum.length()) {
            func(accum);
        }
    }

    static inline std::vector<std::string> Split(const std::string &text, char sep)
    {
        std::vector<std::string> tokens;
        std::string working_string;
        working_string.reserve(text.length());

        for (char ch : text) {
            if (ch == sep) {
                tokens.push_back(working_string);
                working_string.clear();
                continue;
            }

            working_string += ch;
        }

        if (!working_string.empty()) {
            tokens.push_back(working_string);
        }

        return tokens;
    }

    static inline std::vector<std::string> RemoveEmpty(const std::vector<std::string> &strings)
    {
        std::vector<std::string> res;
        res.reserve(strings.size());

        for (auto &&str : strings) {
            if (!str.empty()) {
                res.push_back(str);
            }
        }

        return res;
    }

    static inline std::string TrimLeft(const std::string &s)
    {
        std::string res(s);
        res.erase(res.begin(), std::find_if(res.begin(), res.end(),
            [](int c) { return !std::isspace(c); }));
        return res;
    }

    static inline std::string TrimRight(const std::string &s)
    {
        std::string res(s);
        res.erase(std::find_if(res.rbegin(), res.rend(),
            [](int c) { return !std::isspace(c); }).base(), res.end());
        return res;
    }

    static inline std::string Trim(const std::string &s)
    {
        return TrimLeft(TrimRight(s));
    }

    static inline std::string ReplaceAll(const std::string &text,
        const std::string &from, const std::string &to)
    {
        std::string result(text);
        if (from.empty()) {
            return result;
        }
        size_t start_pos = 0;
        while ((start_pos = text.find(from, start_pos)) != std::string::npos) {
            result.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return result;
    }

    static inline std::string BasePath(const std::string &filepath)
    {
        if (!Contains(filepath, "/") && !Contains(filepath, "\\")) {
            return "";
        }

        return filepath.substr(0, filepath.find_last_of("\\/"));
    }

    template <typename T>
    static inline bool Parse(const std::string &str, T *out_value)
    {
        std::istringstream ss(str);
        T value;

        if (!(ss >> std::boolalpha >> value)) {
            return false;
        }

        *out_value = value;

        return true;
    }

    template <typename T>
    static inline T Parse(const std::string &str)
    {
        T value{};

        Parse(str, &value);

        return value;
    }

    template <typename T>
    static inline bool IsNumber(const std::string &str) {
        T value{};

        return Parse(str, &value);
    }
};

} // namespace hyperion

#endif
