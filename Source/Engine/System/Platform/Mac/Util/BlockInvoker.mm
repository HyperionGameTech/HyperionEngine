#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#import "BlockInvoker.h"

@implementation BlockInvoker
+ (void)invoke:(void (^)(void))block
{
    block();
}
@end
