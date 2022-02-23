#ifndef HYPERION_JSON_STATE_H
#define HYPERION_JSON_STATE_H

#include <string>
#include <vector>

namespace hyperion {
namespace json {

struct Error {
    std::string message;

    Error() {}
    Error(const std::string &message) : message(message) {}
    Error(const Error &other) : message(other.message) {}
};

struct State {
    std::vector<Error> errors;

    void AddError(const Error &error)
    {
        errors.push_back(error);
    }
};

} // namespace json
} // namespace hyperion

#endif