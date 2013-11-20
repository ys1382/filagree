#import "ViewController.h"
#import "interpret.h"
#import "compile.h"

void hal_set_content();

@interface ViewController ()

@end

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

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    hal_set_content(self.view);

#if 0
    struct byte_array *script = byte_array_from_string("sys.print('hi')");
#else if 1
    struct byte_array *script = read_resource("ui.fg");
#else
    struct byte_array *ui = read_resource("ui.fg");
    struct byte_array *sync = read_resource("sync.fg");
    struct byte_array *mesh = read_resource("mesh.fg");
    struct byte_array *im_client = read_resource("sync_client.fg");
    struct byte_array *script = byte_array_concatenate(4, ui, mesh, sync, im_client);
#endif
    struct byte_array *program = build_string(script);
    execute(program);
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
