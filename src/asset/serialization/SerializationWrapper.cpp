#include <asset/serialization/SerializationWrapper.hpp>

#include <scene/NodeProxy.hpp>
#include <scene/Node.hpp>

namespace hyperion {

void SerializationWrapper<Node>::OnPostLoad(Type &value)
{
    // Sets scene to be detached scene, owned by the thread this was invoked on
    value->SetScene(nullptr);
}

Node &SerializationWrapper<Node>::Unwrap(const Type &value)
{
    AssertThrow(value.IsValid());

    return *value;
}

} // namespace hyperion