#import <System/Platform/IOS/AppDelegate.h>

#include <HyperionEngine.hpp>

#include <Framework/Threads/MainThread.hpp>
#include <Framework/EngineGlobals.hpp>

using namespace Hyperion;

namespace Hyperion {

ENGINE_API Game* g_gameToLaunch;

} // namespace Hyperion

@interface AppDelegate ()
@property (nonatomic, strong) CADisplayLink *displayLink;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    if (g_gameToLaunch == nullptr)
    {
        HYP_LOG(Engine, Error, "No global game instance set!");
        return NO;
    }
    
    NSArray<NSString *> *arguments = [[NSProcessInfo processInfo] arguments];
    int argc = (int)arguments.count;
    
    char **argv = (char**)malloc((argc + 1) * sizeof(char *));
    
    for (int i = 0; i < argc; i++)
    {
        argv[i] = strdup([arguments[i] UTF8String]);
    }

    argv[argc] = NULL;

    auto cleanupArgsMemory = [&]()
        {
            for (int i = 0; i < argc; i++)
            {
                free(argv[i]);
            }

            free(argv);
            argv = NULL;
        };

    if (!Hyp_Initialize(argc, argv))
    {
        cleanupArgsMemory();
        return NO;
    }

    cleanupArgsMemory();
    
    Hyp_SetGame(g_gameToLaunch);

    if (!Hyp_LaunchThreads())
    {
        return NO;
    }

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(engineTick:)];
    
    if (@available(iOS 15.0, *)) {
        self.displayLink.preferredFrameRateRange = CAFrameRateRangeMake(60.0, 120.0, 120.0);
    }
    
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    
    return YES;
}

- (void)engineTick:(CADisplayLink *)sender {
    g_mainThreadInstance->Update();
}

- (void)applicationWillResignActive:(UIApplication *)application {
    self.displayLink.paused = YES;

    // @TODO
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    self.displayLink.paused = NO;

    // @TODO
}

- (void)applicationWillTerminate:(UIApplication *)application {
    [self.displayLink invalidate];
    self.displayLink = nil;
    
    Hyp_Shutdown();
}

@end
