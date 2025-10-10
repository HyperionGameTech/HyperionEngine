#include <core/utilities/Result.hpp>

namespace hyperion {
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
} // namespace hyperion