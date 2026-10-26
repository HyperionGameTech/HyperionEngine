# Testing

Tests run through one xUnit project, `Tests/GameProjects/Runner`, which CI runs on Windows, Linux and macOS (`.github/workflows/game-project-tests.yml`).

```bash
./Tools/Scripts/RunTests.sh               # everything
./Tools/Scripts/RunTests.sh EngineTests   # only the engine tests
./Tools/Scripts/RunTests.sh Native        # only the C++ tests
```

The engine has to be built first (`Binaries/<Platform>/Release`, or point `HYPERION_BIN_DIR` at another build).

## Engine tests

Engine tests live in `Tests/Engine` and are compiled into the engine when `HYP_TESTS` is defined (every build but shipping, runtime-only, Android and iOS). They don't need a game project: the `EngineTestCommandlet` boots the engine headless, runs every registered test in one launch and writes a JSON report that the runner turns into one xUnit result per test.

A test registers itself at static initialization:

```cpp
#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

namespace Hyperion::tests {

HYP_ENGINE_TEST(SceneGraph, AddChildSetsParent)
{
    Handle<Scene> scene = MakeHandle<Scene>(NAME("TestScene"), ThreadId::Current(), SceneFlags::NONE);
    InitObject(scene);

    Handle<Node> child = MakeHandle<Node>();
    scene->GetRoot()->AddChild(child);

    HYP_TEST_CHECK(child->GetParent() == scene->GetRoot().Get());
    HYP_TEST_REQUIRE(scene->GetRoot()->NumChildren() == 1); // returns from the test if false
}

} // namespace Hyperion::tests

#endif // HYP_TESTS
```

## Game project tests

`Tests/GameProjects/Fixtures` has a C# and a C++ game. The runner generates each from `Source/Templates`, builds it against the engine and runs it headless, once to write content and once more to read it back. See the fixtures for how a test is added.
