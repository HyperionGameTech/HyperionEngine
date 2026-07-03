#import "AppDelegate.h"

#include <HyperionEngine.hpp>

#include <Framework/Threads/MainThread.hpp>
#include <Framework/EngineGlobals.hpp>

@interface AppDelegate ()
@property (nonatomic, strong) CADisplayLink *displayLink;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    if (!Hyp_Initialize(argc, argv))
    {
        return NO;
    }

    // @TODO !!! Don't just hardcode DefaultGame.
    auto gameInstance = MakeUnique<game::DefaultGame>();
    Hyp_SetGame(gameInstance.Get());

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