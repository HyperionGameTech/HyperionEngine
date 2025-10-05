/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Variant.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/StaticMessage.hpp>
#include <core/utilities/Format.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/debug/Debug.hpp>

#include <core/object/HypObjectMacros.hpp>

#include <core/Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

// @TODO: Global, thread-local Stack<T> of IError*.
// Will be needed to change Result to hold ptr to IError.
// This is necessary for C# interop with Result.

class IError;

HYP_API extern const IError& GetNullError();

class IError
{
public:
    virtual ~IError() = default;

    virtual const String& GetMessage() const = 0;
    virtual ANSIStringView GetFunctionName() const = 0;
};

HYP_STRUCT(Size = 128)
class Error : public IError
{
    HYP_STRUCT_BODY(Error);

public:
    Error()
        : m_message(String::empty),
          m_currentFunction("<unknown>")
    {
    }

    template <auto FormatString, class... Args>
    Error(const StaticMessage& currentFunction, ValueWrapper<FormatString>, Args&&... args)
        : m_message(Format<FormatString>(std::forward<Args>(args)...)),
          m_currentFunction(currentFunction.value)
    {
    }

    virtual ~Error() override = default;

    virtual const String& GetMessage() const override
    {
        return m_message;
    }

    virtual ANSIStringView GetFunctionName() const override
    {
        return m_currentFunction;
    }

protected:
    String m_message;
    ANSIStringView m_currentFunction;
};

#define HYP_MAKE_ERROR(ErrorType, message, ...) ErrorType(HYP_STATIC_MESSAGE(HYP_FUNCTION_NAME_LIT), ValueWrapper<HYP_STATIC_STRING(message)>(), ##__VA_ARGS__)

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

    TResult(const T& value)
        : m_value(value)
    {
    }

    TResult(T&& value)
        : m_value(std::move(value))
    {
    }

    TResult(const ErrorType& error)
        : m_value(MakePimpl<ErrorType>(error))
    {
    }

    TResult(ErrorType&& error)
        : m_value(MakePimpl<ErrorType>(std::move(error)))
    {
    }

    TResult(const TResult& other)
        : m_value(other.HasError() ? Variant<T, Pimpl<ErrorType>>(MakePimpl<ErrorType>(other.GetError())) : Variant<T, Pimpl<ErrorType>>(T(other.GetValue())))
    {
    }

    TResult& operator=(const TResult& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.HasError())
        {
            m_value = MakePimpl<ErrorType>(other.GetError());
        }
        else
        {
            m_value = T(other.GetValue());
        }

        return *this;
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_constructible_v<T, const OtherT&>>>
    TResult(const TResult<OtherT, ErrorType>& other)
    {
        if (other.HasValue())
        {
            m_value = T(other.GetValue());
        }
        else
        {
            m_value = MakePimpl<ErrorType>(other.GetError());
        }
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_constructible_v<T, const OtherT&>>>
    TResult& operator=(const TResult<OtherT, ErrorType>& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.HasValue())
        {
            m_value = T(other.GetValue());
        }
        else if (other.HasError())
        {
            m_value = MakePimpl<ErrorType>(other.GetError());
        }
        else
        {
            m_value.Reset();
        }

        return *this;
    }

    TResult(TResult&& other) noexcept = default;
    TResult& operator=(TResult&& other) noexcept = default;

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_constructible_v<T, OtherT&&>>>
    TResult(TResult<OtherT, ErrorType>&& other) noexcept
    {
        if (other.HasValue())
        {
            m_value = T(std::move(other.GetValue()));
        }
        else if (other.HasError())
        {
            m_value = MakePimpl<ErrorType>(std::move(other.GetError_NonConst()));
        }
    }

    template <class OtherT, typename = std::enable_if_t<!std::is_same_v<T, OtherT> && std::is_constructible_v<T, OtherT&&>>>
    TResult& operator=(TResult<OtherT, ErrorType>&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.HasValue())
        {
            m_value = T(std::move(other.GetValue()));
        }
        else if (other.HasError())
        {
            m_value = MakePimpl<ErrorType>(std::move(other.GetError_NonConst()));
        }
        else
        {
            m_value.Reset();
        }

        return *this;
    }

    ~TResult() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_value.template Is<T>();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !m_value.template Is<T>();
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_value.template Is<T>();
    }

    HYP_FORCE_INLINE bool HasError() const
    {
        return m_value.template Is<Pimpl<ErrorType>>();
    }

    HYP_FORCE_INLINE T& GetValue() &
    {
        HYP_CORE_ASSERT(HasValue(), "Result does not contain a value");

        return m_value.template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE const T& GetValue() const&
    {
        HYP_CORE_ASSERT(HasValue(), "Result does not contain a value");

        return m_value.template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE T GetValue() &&
    {
        HYP_CORE_ASSERT(HasValue(), "Result does not contain a value");

        return std::move(m_value).template GetUnchecked<T>();
    }

    HYP_FORCE_INLINE T GetValueOr(T&& defaultValue) const&
    {
        if (HasValue())
        {
            return m_value.template GetUnchecked<T>();
        }

        return std::forward<T>(defaultValue);
    }

    HYP_FORCE_INLINE T GetValueOr(T&& defaultValue) &&
    {
        if (HasValue())
        {
            return std::move(m_value).template GetUnchecked<T>();
        }
        else
        {
            return std::forward<T>(defaultValue);
        }
    }

    HYP_FORCE_INLINE const ErrorType& GetError() const
    {
        return const_cast<TResult*>(this)->GetError_NonConst();
    }

    HYP_FORCE_INLINE T& operator*()
    {
        return GetValue();
    }

    HYP_FORCE_INLINE const T& operator*() const
    {
        return GetValue();
    }

    HYP_FORCE_INLINE T* operator->()
    {
        return &GetValue();
    }

    HYP_FORCE_INLINE const T* operator->() const
    {
        return &GetValue();
    }

    template <class OtherT, class OtherErrorType>
    HYP_FORCE_INLINE bool operator==(const TResult<OtherT, OtherErrorType>& other) const
    {
        if (HasValue() != other.HasValue())
        {
            return false;
        }

        if (HasValue())
        {
            return GetValue() == other.GetValue();
        }

        return true;
    }

    template <class OtherT, class OtherErrorType>
    HYP_FORCE_INLINE bool operator!=(const TResult<OtherT, OtherErrorType>& other) const
    {
        if (HasValue() != other.HasValue())
        {
            return true;
        }

        if (HasValue())
        {
            return GetValue() != other.GetValue();
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const T& value) const
    {
        return HasValue() && m_value.template GetUnchecked<T>() == value;
    }

    HYP_FORCE_INLINE bool operator!=(const T& value) const
    {
        return !HasValue() || m_value.template GetUnchecked<T>() == value;
    }

    HYP_FORCE_INLINE bool operator==(const ErrorType& error) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ErrorType& error) const = delete;

private:
    HYP_FORCE_INLINE ErrorType& GetError_NonConst()
    {
        static const ErrorType nullError;

        if (HasError())
        {
            return *m_value.template GetUnchecked<Pimpl<ErrorType>>();
        }
        else
        {
            return const_cast<ErrorType&>(nullError);
        }
    }

    Variant<T, Pimpl<ErrorType>> m_value;
};

template <class ErrorType>
class TResult<void, ErrorType>
{
public:
    static_assert(std::is_base_of_v<IError, ErrorType>, "ErrorType must implement IError");

    TResult() = default;

    TResult(const ErrorType& error)
        : m_error(MakePimpl<ErrorType>(error))
    {
    }

    TResult(ErrorType&& error)
        : m_error(MakePimpl<ErrorType>(std::move(error)))
    {
    }

    TResult(const TResult& other)
        : m_error(other.HasError() ? MakePimpl<ErrorType>(other.GetError()) : Pimpl<ErrorType>())
    {
    }

    TResult& operator=(const TResult& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.HasError())
        {
            m_error = MakePimpl<ErrorType>(other.GetError());
        }
        else
        {
            m_error.Reset();
        }

        return *this;
    }

    TResult(TResult&& other) noexcept
        : m_error(std::move(other.m_error))
    {
    }

    TResult& operator=(TResult&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_error = std::move(other.m_error);

        return *this;
    }

    ~TResult() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_error == nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_error != nullptr;
    }

    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_error == nullptr;
    }

    HYP_FORCE_INLINE bool HasError() const
    {
        return m_error != nullptr;
    }

    HYP_FORCE_INLINE const ErrorType& GetError() const
    {
        static const ErrorType nullError;

        if (HasError())
        {
            return *m_error;
        }
        else
        {
            return nullError;
        }
    }

    template <class OtherErrorType>
    HYP_FORCE_INLINE bool operator==(const TResult<void, OtherErrorType>& other) const
    {
        return bool(m_error) == bool(other.m_error);
    }

    template <class OtherErrorType>
    HYP_FORCE_INLINE bool operator!=(const TResult<void, OtherErrorType>& other) const
    {
        return bool(m_error) != bool(other.m_error);
    }

    HYP_FORCE_INLINE bool operator==(const ErrorType& error) const = delete;
    HYP_FORCE_INLINE bool operator!=(const ErrorType& error) const = delete;

private:
    Pimpl<ErrorType> m_error;
};

/*! \brief Default Result class - see TResult<T, ErrorType> for custom T or Error type. */
HYP_STRUCT(Size = 8)
class Result : public TResult<void, Error>
{
    HYP_STRUCT_BODY(Result);

public:
    using TResult::operator bool;
    using TResult::operator!;
    using TResult::operator==;
    using TResult::operator!=;

    Result() = default;

    Result(const Error& error)
        : TResult(error)
    {
    }

    Result(Error&& error)
        : TResult(std::move(error))
    {
    }

    Result(const Result& other) = default;
    Result& operator=(const Result& other) = default;
    Result(Result&& other) noexcept = default;
    Result& operator=(Result&& other) noexcept = default;
    ~Result() = default;

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasValue() const
    {
        return TResult::HasValue();
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool HasError() const
    {
        return TResult::HasError();
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Error& GetError() const
    {
        return TResult::GetError();
    }
};

} // namespace utilities

using utilities::Error;
using utilities::GetNullError;
using utilities::IError;
using utilities::Result;
using utilities::TResult;

} // namespace hyperion
