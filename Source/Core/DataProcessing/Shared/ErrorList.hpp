#pragma once

#include <Core/Containers/FlatSet.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Defines.hpp>

#include <type_traits>

namespace Hyperion::DataProcessing {

template <class TErrorType>
class ErrorList
{
public:
    using ErrorType = TErrorType;

    ErrorList()
        : m_errorSuppressionDepth(0)
    {
    }

    ErrorList(const ErrorList& other)
        : m_errors(other.m_errors),
          m_errorSuppressionDepth(other.m_errorSuppressionDepth)
    {
    }

    ErrorList& operator=(const ErrorList& other)
    {
        m_errors = other.m_errors;
        m_errorSuppressionDepth = other.m_errorSuppressionDepth;

        return *this;
    }

    ErrorList(ErrorList&& other) noexcept = default;
    ErrorList& operator=(ErrorList&& other) noexcept = default;

    ~ErrorList() = default;

    size_t Size() const { return m_errors.Size(); }

    ErrorType& operator[](size_t index) { return m_errors[index]; }
    const ErrorType& operator[](size_t index) const { return m_errors[index]; }

    void AddError(const ErrorType& error)
    {
        if (ErrorsSuppressed())
        {
            return;
        }

        m_errors.Insert(error);
    }

    void ClearErrors() { m_errors.Clear(); }

    void Concatenate(const ErrorList& other) { m_errors.Merge(other.m_errors); }

    bool ErrorsSuppressed() const { return m_errorSuppressionDepth > 0; }

    void SuppressErrors(bool suppress)
    {
        if (suppress)
        {
            m_errorSuppressionDepth++;
        }
        else
        {
            if (m_errorSuppressionDepth <= 0)
            {
                return;
            }

            m_errorSuppressionDepth--;
        }
    }

    bool HasFatalErrors() const
    {
        for (auto& error : m_errors)
        {
            using LevelType = std::decay_t<decltype(error.GetLevel())>;

            if (error.GetLevel() == LevelType(0))
            {
                return true;
            }
        }

        return false;
    }

    template <class F>
    bool HasError(F&& pred) const
    {
        for (ErrorType& error : m_errors)
        {
            if (pred(error))
            {
                return true;
            }
        }

        return false;
    }
    
    /// Get the error list as a printable string, with lines, cols and msg texts
    void WriteAllMessages(String& outString) const
    {
        // @TODO: StringBuilder

        FlatSet<String> errorFilenames;
        Array<String> currentFileLines;

        for (const ErrorType& error : m_errors)
        {
            const String& path = error.GetLocation().GetFileName();

            if (errorFilenames.Insert(path).second)
            {
                currentFileLines.Resize(0);

                FileByteReader stream { FilePath(path) };

                if (!stream.Eof())
                {
                    currentFileLines.Concat(String(stream.Read().ToByteView()).Split('\n'));
                    stream.Close();
                }

                Array<String> split = path.Split('\\', '/');

                const String& realFilename = split.Any()
                    ? split.Back()
                    : path;

                outString += "In file \"" + realFilename + "\":\n";
            }

            const String& errorText = error.GetText();

            switch (error.GetLevel())
            {
            case ErrorType::Level::Diagnostic:
                outString += "Diagnostic";
                break;
            case ErrorType::Level::Warning:
                outString += "Warning";
                break;
            case ErrorType::Level::Error:
                outString += "Error";
                break;
            }

            outString += " at line " + String::ToString(error.GetLocation().GetLine() + 1)
               + ", col " + String::ToString(error.GetLocation().GetColumn() + 1);

            outString += ": " + errorText + '\n';

            if (error.GetLocation().GetLine() > 0
                && currentFileLines.Size() >= error.GetLocation().GetLine())
            {
                // render the line in question
                outString += "\n\t" + currentFileLines[error.GetLocation().GetLine() - 1] + "\n\t";

                for (size_t i = 0; i < error.GetLocation().GetColumn(); i++)
                {
                    outString += ' ';
                }

                outString += '^';
            }
            else
            {
                outString += "\t<line not found>";
            }

            outString += '\n';
        }
    }

private:
    FlatSet<ErrorType> m_errors;
    uint32 m_errorSuppressionDepth;
};

} // namespace Hyperion::DataProcessing
