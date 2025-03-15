/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RESULT_HPP
#define HYPERION_RESULT_HPP

#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/StaticMessage.hpp>
#include <core/utilities/Format.hpp>

#include <core/debug/Debug.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

// @TODO: Global, thread-local Stack<T> of IError*.
// Will be needed to change Result to hold ptr to IError.
// This is necessary for C# interop with Result.

HYP_STRUCT()
class IError
{
public:
    virtual ~IError() = default;

    virtual const String &GetMessage() const = 0;
    virtual ANSIStringView GetFunctionName() const = 0;
};

HYP_STRUCT(Size=128)
class Error : public IError
{
public:
    Error()
        : m_message(String::empty),
          m_current_function("<unknown>")
    {
    }

    template <auto FormatString, class... Args>
    Error(const StaticMessage &current_function, ValueWrapper<FormatString>, Args &&... args)
        : m_message(Format<FormatString>(std::forward<Args>(args)...)),
          m_current_function(current_function.value)
    {
    }

    virtual ~Error() override = default;
    
    virtual const String &GetMessage() const override
    {
        return m_message;
    }
    
    virtual ANSIStringView GetFunctionName() const override
    {
        return m_current_function;
    }

protected:
    String          m_message;
    ANSIStringView  m_current_function;
};

#define HYP_MAKE_ERROR(ErrorType, message, ...) ErrorType(HYP_STATIC_MESSAGE(HYP_PRETTY_FUNCTION_NAME), ValueWrapper<HYP_STATIC_STRING(message)>(), ##__VA_ARGS__)

/*! \brief A class that represents a result that can either be a value or an error.
 *  The value and error types are specified by the template parameters.
 *  The error type defaults to Error if not specified. */
template <class T = void, class ErrorType = Error>
class TResult;

template <class T, class ErrorType>
class TResult
{
public:
    static_assert(std::is_base_of_v<IError, ErrorType>, "ErrorType must implement IError");

    // friend decl
    template <class OtherT, class OtherErrorType>
    friend class TResult;

    TResult(const T &value)
        : m_value(value)
    {
    }

    TResult(T &&value)
        : m_value(std::move(value))
    {
    }

    TResult(const ErrorType &error)
        : m_value(error)
    {
    }

    TResult(ErrorType &&error)
        : m_value(std::move(error))
    {
    }

    TResult(const TResult &other)                   = default;
    TResult &operator=(const TResult &other)        = default;

    template <class OtherT>
    TResult(const TResult<OtherT, ErrorType> &other)
    {
        if (other.HasValue()) {
            m_value = T(other.GetValue());
        } else {
            m_value = other.GetError();
        }
    }

    template <class OtherT>
    TResult &operator=(const TResult<OtherT, ErrorType> &other)
    {
        if (this == &other) {
            return *this;
        }

        if (other.HasValue()) {
            m_value = T(other.GetValue());
        } else if (other.HasError()) {
            m_value = other.GetError();
        } else {
            m_value.Reset();
        }

        return *this;
    }

    TResult(TResult &&other) noexcept               = default;
    TResult &operator=(TResult &&other) noexcept    = default;

    template <class OtherT>
    TResult(TResult<OtherT, ErrorType> &&other) noexcept
    {
        if (other.HasValue()) {
            m_value = T(std::move(other.GetValue()));
        } else if (other.HasError()) {
            m_value = std::move(other.GetError_NonConst());
        }
    }

    template <class OtherT>
    TResult &operator=(TResult<OtherT, ErrorType> &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (other.HasValue()) {
            m_value = T(std::move(other.GetValue()));
        } else if (other.HasError()) {
            m_value = std::move(other.GetError_NonConst());
        } else {
            m_value.Reset();
        }

        return *this;
    }

    ~TResult()                                      = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_value.template Is<T>(); }

    HYP_FORCE_INLINE bool operator!() const
        { return !m_value.template Is<T>(); }

    HYP_FORCE_INLINE bool HasValue() const
        { return m_value.template Is<T>(); }

    HYP_FORCE_INLINE bool HasError() const
        { return m_value.template Is<ErrorType>(); }

    HYP_FORCE_INLINE T &GetValue() &
    {
        AssertThrowMsg(HasValue(), "Result does not contain a value");

        return m_value.template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE const T &GetValue() const &
    {
        AssertThrowMsg(HasValue(), "Result does not contain a value");

        return m_value.template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE T GetValue() &&
    {
        AssertThrowMsg(HasValue(), "Result does not contain a value");

        return std::move(m_value.template GetUnchecked<T>());
    }

    HYP_FORCE_INLINE T GetValueOr(T &&default_value) const &
    {
        if (HasValue()) {
            return m_value.template GetUnchecked<T>();
        }

        return std::forward<T>(default_value);
    }

    HYP_FORCE_INLINE T GetValueOr(T &&default_value) &&
    {
        if (HasValue()) {
            return std::move(m_value.template GetUnchecked<T>());
        } else {
            return std::forward<T>(default_value);
        }
    }

    HYP_FORCE_INLINE const ErrorType &GetError() const
        { return const_cast<TResult *>(this)->GetError_NonConst(); }

    HYP_FORCE_INLINE T &operator*()
        { return GetValue(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return GetValue(); }

    HYP_FORCE_INLINE T *operator->()
        { return &GetValue(); }

    HYP_FORCE_INLINE const T *operator->() const
        { return &GetValue(); }

    template <class OtherT, class OtherErrorType>
    HYP_FORCE_INLINE bool operator==(const TResult<OtherT, OtherErrorType> &other) const
    {
        if (HasValue() != other.HasValue()) {
            return false;
        }

        if (HasValue()) {
            return GetValue() == other.GetValue();
        }

        return true;
    }
    
    template <class OtherT, class OtherErrorType>
    HYP_FORCE_INLINE bool operator!=(const TResult<OtherT, OtherErrorType> &other) const
    {
        if (HasValue() != other.HasValue()) {
            return true;
        }

        if (HasValue()) {
            return GetValue() != other.GetValue();
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const T &value) const
        { return HasValue() && m_value.template GetUnchecked<T>() == value; }

    HYP_FORCE_INLINE bool operator!=(const T &value) const
        { return !HasValue() || m_value.template GetUnchecked<T>() == value; }

    HYP_FORCE_INLINE bool operator==(const ErrorType &error) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ErrorType &error) const = delete;

private:
    HYP_FORCE_INLINE ErrorType &GetError_NonConst()
    {
        static const ErrorType null_error;

        if (HasError()) {
            return m_value.template GetUnchecked<ErrorType>();
        } else {
            return const_cast<ErrorType &>(null_error);
        }
    }

    Variant<T, ErrorType>   m_value;
};

template <class ErrorType>
class TResult<void, ErrorType>
{
public:
    static_assert(std::is_base_of_v<IError, ErrorType>, "ErrorType must implement IError");

    TResult()                                       = default;

    TResult(const ErrorType &error)
        : m_error(error)
    {
    }

    TResult(ErrorType &&error)
        : m_error(std::move(error))
    {
    }

    TResult(const TResult &other)                   = default;
    TResult &operator=(const TResult &other)        = default;
    TResult(TResult &&other) noexcept               = default;
    TResult &operator=(TResult &&other) noexcept    = default;
    ~TResult()                                      = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return !m_error.HasValue(); }

    HYP_FORCE_INLINE bool operator!() const
        { return m_error.HasValue(); }

    HYP_FORCE_INLINE bool HasValue() const
        { return !m_error.HasValue(); }

    HYP_FORCE_INLINE bool HasError() const
        { return m_error.HasValue(); }

    HYP_FORCE_INLINE const ErrorType &GetError() const
    {
        static const ErrorType null_error;

        if (HasError()) {
            return m_error.Get();
        } else {
            return null_error;
        }
    }

    template <class OtherErrorType>
    HYP_FORCE_INLINE bool operator==(const TResult<void, OtherErrorType> &other) const
        { return m_error.HasValue() == other.m_error.HasValue(); }
    
    template <class OtherErrorType>
    HYP_FORCE_INLINE bool operator!=(const TResult<void, OtherErrorType> &other) const
        { return m_error.HasValue() != other.m_error.HasValue(); }

    HYP_FORCE_INLINE bool operator==(const ErrorType &error) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ErrorType &error) const = delete;

private:
    Optional<ErrorType> m_error;
};

/*! \brief Default Result class - see TResult<T, ErrorType> for custom T or Error type. */
HYP_STRUCT(Size=136)
class Result : public TResult<void, Error>
{
public:
    using TResult::operator bool;
    using TResult::operator!;
    using TResult::operator==;
    using TResult::operator!=;

    Result()                                    = default;

    Result(const Error &error)
        : TResult(error)
    {
    }

    Result(Error &&error)
        : TResult(std::move(error))
    {
    }

    Result(const Result &other)                 = default;
    Result &operator=(const Result &other)      = default;
    Result(Result &&other) noexcept             = default;
    Result &operator=(Result &&other) noexcept  = default;
    ~Result()                                   = default;

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasValue() const
        { return TResult::HasValue(); }

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasError() const
        { return TResult::HasError(); }

    HYP_METHOD()
    HYP_FORCE_INLINE const Error &GetError() const
        { return TResult::GetError(); }
};

} // namespace utilities

using utilities::TResult;
using utilities::Result;
using utilities::IError;
using utilities::Error;

} // namespace hyperion

#endif