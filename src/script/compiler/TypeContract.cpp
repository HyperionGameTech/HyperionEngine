#include <script/compiler/TypeContract.hpp>

namespace hyperion {
namespace compiler {

const std::map<TypeContract::Type, std::string> TypeContract::type_contract_strings = {
    { TypeContract::Type::TC_IS, "is" },
    { TypeContract::Type::TC_ISNOT, "isnot" },
    { TypeContract::Type::TC_HAS, "has" },
};

std::string TypeContract::ToString(TypeContract::Type type)
{
    auto it = TypeContract::type_contract_strings.find(type);
    return (it != TypeContract::type_contract_strings.end()) ? it->second : "??";
}

TypeContract::Type TypeContract::FromString(const std::string &str)
{
    for (const auto &it : TypeContract::type_contract_strings) {
        if (it.second == str) {
            return it.first;
        }
    }
    return TypeContract::Type::TC_INVALID;
}

} // namespace compiler
} // namespace hyperion
