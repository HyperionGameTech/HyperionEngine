#ifndef ERROR_LIST_HPP
#define ERROR_LIST_HPP

#include <script/compiler/CompilerError.hpp>
#include <util/utf8.hpp>

#include <vector>
#include <algorithm>
#include <ostream>

namespace hyperion {
namespace compiler {

class ErrorList {
public:
    ErrorList();
    ErrorList(const ErrorList &other);

    inline void AddError(const CompilerError &error) { m_errors.push_back(error); }
    inline void ClearErrors() { m_errors.clear(); }
    inline void SortErrors() { std::sort(m_errors.begin(), m_errors.end()); }

    bool HasFatalErrors() const;
    std::ostream &WriteOutput(std::ostream &os) const; // @TODO make UTF8 compatible

    std::vector<CompilerError> m_errors;
};

} // namespace compiler
} // namespace hyperion

#endif
