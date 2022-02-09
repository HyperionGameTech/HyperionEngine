#ifndef FBOM_RESULT_H
#define FBOM_RESULT_H

#include <string>

namespace hyperion {
namespace fbom {

struct FBOMResult {
    enum {
        FBOM_OK = 0,
        FBOM_ERR = 1
    } value;

    std::string message;

    FBOMResult(decltype(FBOM_OK) value = FBOM_OK, std::string message = "")
        : value(value),
          message(message)
    {
    }

    FBOMResult(const FBOMResult &other)
        : value(other.value),
          message(other.message)
    {
    }

    inline operator int() const { return int(value); }
};

} // namespace fbom
} // namespace hyperion

#endif
