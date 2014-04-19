//
//  IMclient.h
//  

#import <Foundation/Foundation.h>

@interface IMclient : NSObject

+ (IMclient*)shared;
- (void)signup:(NSString*)username withPassword:(NSString*)password;
- (void)signin:(NSString*)username withPassword:(NSString*)password;

@end
