#import "AppDelegate.h"
#import "interpret.h"
#import "compile.h"

struct byte_array *read_resource(const char *path)
{
    NSString *path2 = [NSString stringWithUTF8String:path];
    NSString *resource = [path2 stringByDeletingPathExtension];
    NSString *type = [path2 pathExtension];
    NSString* path3 = [[NSBundle mainBundle] pathForResource:resource ofType:type inDirectory:@""];
    if (!path3) {
        NSLog(@"can't load file");
        return NULL;
    }
    NSError *error = nil;
    NSString* contents = [NSString stringWithContentsOfFile:path3 encoding:NSUTF8StringEncoding error:&error];
    if (error) {
        NSLog(@"%@", [error localizedDescription]);
        return NULL;
    }

    const char *contents2 = [contents UTF8String];
    return byte_array_from_string(contents2);
}

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    // Override point for customization after application launch.
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];

//  struct byte_array *script = byte_array_from_string("sys.print('hi')");

    struct byte_array *ui = read_resource("ui.fg");
    struct byte_array *try = read_resource("try.fg");
    byte_array_append(ui, try);
    struct byte_array *program = build_string(ui);
    execute(program);
    
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end
