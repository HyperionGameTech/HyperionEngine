#ifndef TYPE_CONTRACT_HPP
#define TYPE_CONTRACT_HPP

#include <string>
#include <map>

namespace hyperion::compiler {

class TypeContract {
public:
    enum Type {
        TC_INVALID = -1,
        TC_IS, // is of type
        TC_ISNOT, // is not of type
        TC_HAS, // has a member with name
    };

    static std::string ToString(Type type);
    static TypeContract::Type FromString(const std::string &str);

private:
    static const std::map<TypeContract::Type, std::string> type_contract_strings;
};

} // namespace hyperion::compiler

#endif
