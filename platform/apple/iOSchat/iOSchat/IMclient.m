//
//  IMclient.m
//  

#import "IMclient.h"
#import "LoginViewController.h"
#import "MasterViewController.h"

#include "interpret.h"
#include "hal_apple.h"
#include "compile.h"
#include "sys.h"


@implementation IMclient

static struct context *context = NULL;
static IMclient *imc = NULL;
static struct variable *callback = NULL;


struct variable *roster_ui(struct context *context)
{
//    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
//    int32_t milliseconds = param_int(args, 1)
    ;

    [[NSOperationQueue mainQueue] addOperationWithBlock:^ {

        UIStoryboard *mainStoryboard;
        if ([[UIDevice currentDevice] userInterfaceIdiom] ==UIUserInterfaceIdiomPad)
            mainStoryboard = [UIStoryboard storyboardWithName:@"Main_iPad" bundle:nil];
        else
            mainStoryboard = [UIStoryboard storyboardWithName:@"Main_iPhone" bundle:nil];
        UIViewController *toViewController = [mainStoryboard instantiateViewControllerWithIdentifier:@"RosterScene"];
        LoginViewController *lvc = [LoginViewController shared];
        [lvc presentViewController:toViewController animated:NO completion:nil];
    }];

    return NULL;
}

struct variable *roster_update(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *latest = param_var(args, 1);
    [[MasterViewController shared] setContacts:(NSArray*)f2o(context, latest)];

    return NULL;
}

+ (id)shared
{
    if (NULL != imc)
        return imc;
    imc = [IMclient alloc];

    struct byte_array *ui = read_resource("ui.fg");
    struct byte_array *mesh = read_resource("mesh.fg");
    struct byte_array *client = read_resource("im_client.fg");
    struct byte_array *args = byte_array_from_string("id='IOS'");
    struct byte_array *native1 = byte_array_from_string("client.main = roster_ui ");
    struct byte_array *native2 = byte_array_from_string("client.update_roster = roster_update");
    struct byte_array *script = byte_array_concatenate(6, ui, mesh, args, client, native1, native2);
    
    struct byte_array *program = build_string(script, NULL);
    context = context_new(NULL, true, true);

    callback = variable_new_list(context, NULL);
    struct byte_array *key = byte_array_from_string("roster_ui");
    struct variable *key2 = variable_new_str(context, key);
    struct variable *val = variable_new_cfnc(context, &roster_ui);
    variable_map_insert(context, callback, key2, val);

    
    context->singleton->callback = callback;
    execute_with(context, program, true);

    return imc;
}

- (void)addContact:(NSString*)username
{
    NSString *code = [NSString stringWithFormat:@"client.addContact('%@')", username];
    interpret_string_with(context, nsstring_to_byte_array(code));
}

- (void)signup:(NSString*)username withPassword:(NSString*)password
{
    NSString *code = [NSString stringWithFormat:@"client.signup('%@','%@')", username, password];
    interpret_string_with(context, nsstring_to_byte_array(code));
}

- (void)signin:(NSString*)username withPassword:(NSString*)password
{
    NSString *code = [NSString stringWithFormat:@"client.signin('%@','%@')", username, password];
    interpret_string_with(context, nsstring_to_byte_array(code));
}

@end
