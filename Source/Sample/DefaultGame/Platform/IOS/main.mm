#import <UIKit/UIKit.h>
#import <System/Platform/IOS/AppDelegate.h>

#include <Game/DefaultGame.hpp>

using namespace Hyperion;

static game::DefaultGame s_gameInstance;

int main(int argc, char* argv[])
{
    g_gameToLaunch = &s_gameInstance;

    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
