#include <script/compiler/ErrorList.hpp>

#include <core/utilities/StringUtil.hpp>

#include <core/containers/FlatSet.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <fstream>

namespace hyperion {

ErrorList::ErrorList()
    : m_errorSuppressionDepth(0)
{
}

ErrorList::ErrorList(const ErrorList& other)
    : m_errors(other.m_errors),
      m_errorSuppressionDepth(other.m_errorSuppressionDepth)
{
}

ErrorList& ErrorList::operator=(const ErrorList& other)
{
    m_errors = other.m_errors;
    m_errorSuppressionDepth = other.m_errorSuppressionDepth;

    return *this;
}

ErrorList::ErrorList(ErrorList&& other) noexcept
    : m_errors(std::move(other.m_errors)),
      m_errorSuppressionDepth(other.m_errorSuppressionDepth)
{
    other.m_errorSuppressionDepth = 0;
}

ErrorList& ErrorList::operator=(ErrorList&& other) noexcept
{
    m_errors = std::move(other.m_errors);
    m_errorSuppressionDepth = other.m_errorSuppressionDepth;

    other.m_errorSuppressionDepth = 0;

    return *this;
}

bool ErrorList::HasFatalErrors() const
{
    return m_errors.FindIf([](const CompilerError& error)
               {
                   return error.GetLevel() == LEVEL_ERROR;
               })
        != m_errors.End();
}

std::ostream& ErrorList::WriteOutput(std::ostream& os) const
{
    FlatSet<String> errorFilenames;
    Array<String> currentFileLines;

    for (const CompilerError& error : m_errors)
    {
        const String& path = error.GetLocation().GetFileName();

        if (errorFilenames.Insert(path).second)
        {
            currentFileLines.Clear();

            FileBufferedReaderSource source { path };
            BufferedReader reader { &source };

            if (reader.IsOpen())
            {
                currentFileLines = reader.ReadAllLines();
            }

            Array<String> split = path.Split('\\', '/');

            String realFilename = split.Any()
                ? split.Back()
                : path;

            realFilename = FilePath(realFilename).StripExtension();

            os << "\33[0m" << "In file \"" << realFilename << "\":\n";
        }

        const String& errorText = error.GetText();

        switch (error.GetLevel())
        {
        case LEVEL_INFO:
            os << "Info";
            break;
        case LEVEL_WARN:
            os << "\33[33m" << "Warning";
            break;
        case LEVEL_ERROR:
            os << "\33[31m" << "Error";
            break;
        }

        os << "\33[0m"
           << " at line " << (error.GetLocation().GetLine() + 1)
           << ", col " << (error.GetLocation().GetColumn() + 1);

        os << ": " << errorText << '\n';

        if (currentFileLines.Size() > error.GetLocation().GetLine())
        {
            // render the line in question
            os << "\n\t" << currentFileLines[error.GetLocation().GetLine()];
            os << "\n\t";

            for (SizeType i = 0; i < error.GetLocation().GetColumn(); i++)
            {
                os << ' ';
            }

            os << "^";
        }
        else
        {
            os << '\t' << "<line not found>";
        }

        os << "\33[0m" << '\n';
    }

    return os;
}

} // namespace hyperion
