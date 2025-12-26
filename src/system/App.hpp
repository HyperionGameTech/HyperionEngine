#pragma once
#include <core/Defines.hpp>

#include <core/reflection/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

namespace Hyperion {

class Game;
class SimThread;

class AppContextBase;

class HYP_API App final
{
public:
    static App& GetInstance();

    App(const App& other) = delete;
    App& operator=(const App& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App&& other) noexcept = delete;
    virtual ~App();

protected:
    App();
};

} // namespace Hyperion
