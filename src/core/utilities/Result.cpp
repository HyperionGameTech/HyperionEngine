#include <core/utilities/Result.hpp>

namespace hyperion {
namespace utilities {

class NullError final : public IError
{
public:
    NullError() = default;
    virtual ~NullError() override = default;

    virtual const String& GetMessage() const override
    {
        static const String message = "<null>";

        return message;
    }

    virtual ANSIStringView GetFunctionName() const override
    {
        return ANSIStringView();
    }
};

HYP_API const IError& GetNullError()
{
    static NullError null_error;

    return null_error;
}

} // namespace utilities
} // namespace hyperion