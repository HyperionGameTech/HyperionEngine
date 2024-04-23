#include <core/utilities/UniqueID.hpp>
#include <core/threading/Threads.hpp>

#include <random>

namespace hyperion {
namespace utilities {
namespace detail {

uint64 UniqueID::Generate()
{
    static thread_local std::mt19937 random_engine(Threads::CurrentThreadID().value)
    std::uniform_int_distribution<uint64> distribution;

    return distribution(random_engine);
}

} // namespace detail
} // namespace utilities
} // namespace hyperion