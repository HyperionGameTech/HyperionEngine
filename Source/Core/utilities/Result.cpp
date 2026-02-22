#include <Core/utilities/Result.hpp>

#ifndef HYP_TOOL
#include <Result.generated.inl>
#endif

namespace Hyperion {
namespace utilities {

class NullError final : public Error
{
public:
    NullError() = default;
};

HYP_API const Error& GetNullError()
{
    static NullError s_nullError;

    return s_nullError;
}

} // namespace utilities
} // namespace Hyperion
