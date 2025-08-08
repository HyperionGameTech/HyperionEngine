@interface BlockInvoker : NSObject
+ (void)invoke:(void (^)(void))block;
@end