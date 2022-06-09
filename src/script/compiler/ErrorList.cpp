#include <script/compiler/ErrorList.hpp>
#include <util/termcolor.hpp>
#include <util/string_util.h>

#include <unordered_set>
#include <map>
#include <fstream>

namespace hyperion::compiler {

ErrorList::ErrorList()
{
}

ErrorList::ErrorList(const ErrorList &other)
    : m_errors(other.m_errors)
{
}

bool ErrorList::HasFatalErrors() const
{
    if (!m_errors.empty()) {
        for (const CompilerError &error : m_errors) {
            if (error.GetLevel() == LEVEL_ERROR) {
                return true;
            }
        }
    }
    
    return false;
}

std::ostream &ErrorList::WriteOutput(std::ostream &os) const
{
    std::unordered_set<std::string> error_filenames;
    std::vector<std::string> current_file_lines;

    for (const CompilerError &error : m_errors) {
        const std::string &path = error.GetLocation().GetFileName();

        if (error_filenames.insert(path).second) {
            current_file_lines.clear();
            // read lines into current_lines vector
            // this is used so that we can print out the line that an error occured on
            std::ifstream is(path);
            if (is.is_open()) {
                std::string line;
                while (std::getline(is, line)) {
                    current_file_lines.push_back(line);
                }
            }

            auto split = StringUtil::SplitPath(path);
            std::string real_filename = !split.empty()
                ? split.back()
                : path;

            real_filename = StringUtil::StripExtension(real_filename);

            os << termcolor::reset << "In file \"" << real_filename << "\":\n";
        }

        const std::string &error_text = error.GetText();

        switch (error.GetLevel()) {
            case LEVEL_INFO:
                os << termcolor::white << termcolor::on_blue << termcolor::bold << "Info";
                break;
            case LEVEL_WARN:
                os << termcolor::white << termcolor::on_yellow << termcolor::bold << "Warning";
                break;
            case LEVEL_ERROR:
                os << termcolor::white << termcolor::on_red << termcolor::bold << "Error";
                break;
        }

        os << termcolor::reset
           << " at line "    << (error.GetLocation().GetLine() + 1)
           << ", col " << (error.GetLocation().GetColumn() + 1);

        os << ": " << error_text << '\n';

        if (current_file_lines.size() > error.GetLocation().GetLine()) {
            // render the line in question
            os << "\n\t" << current_file_lines[error.GetLocation().GetLine()];
            os << "\n\t";

            for (size_t i = 0; i < error.GetLocation().GetColumn(); i++) {
                os << ' ';
            }

            os << termcolor::green << "^";
        } else {
            os << '\t' << "<line not found>";
        }

        os << termcolor::reset << '\n';
    }

    return os;
}

} // namespace hyperion::compiler
