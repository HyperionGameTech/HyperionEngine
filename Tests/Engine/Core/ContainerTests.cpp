/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/SlimArray.hpp>

namespace Hyperion {
namespace tests {
namespace core {

template <class TArray>
static TArray MakeOneToFive()
{
    TArray values;

    for (int value = 1; value <= 5; value++)
    {
        values.PushBack(value);
    }

    return values;
}

HYP_ENGINE_TEST(Containers, RemoveAllRemovesTheLastElement)
{
    Array<int> array = MakeOneToFive<Array<int>>();
    array.RemoveAll([](int value) { return value % 2 == 1; });

    HYP_TEST_REQUIRE(array.Size() == 2);
    HYP_TEST_CHECK(array[0] == 2 && array[1] == 4);

    SlimArray<int> slim = MakeOneToFive<SlimArray<int>>();
    slim.RemoveAll([](int value) { return value == 5; });

    HYP_TEST_REQUIRE(slim.Size() == 4);
    HYP_TEST_CHECK(slim[3] == 4);
}

HYP_ENGINE_TEST(Containers, RemoveAllCanEmpty)
{
    Array<int> array = MakeOneToFive<Array<int>>();
    array.RemoveAll([](int) { return true; });

    HYP_TEST_CHECK(array.Empty());

    SlimArray<int> slim = MakeOneToFive<SlimArray<int>>();
    slim.RemoveAll([](int) { return true; });

    HYP_TEST_CHECK(slim.Empty());
}

} // namespace core
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
