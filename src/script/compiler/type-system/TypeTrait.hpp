#ifndef TYPE_TRAIT_HPP
#define TYPE_TRAIT_HPP

#include <string>

namespace hyperion::compiler {

class TypeTrait {
public:
    TypeTrait(const std::string &name);

    inline const std::string &GetName() const { return m_name; }

private:
    std::string m_name;
};

} // namespace hyperion::compiler

#endif
