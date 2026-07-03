#pragma once

#include <Core/Defines.hpp>

#import <UIKit/UIKit.h>

namespace Hyperion {

class Game;

ENGINE_API extern Game* g_gameToLaunch;

} // namespace Hyperion

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;

@end
