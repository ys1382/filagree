//
//  IMclient.m
//  

#import "IMclient.h"
#import "LoginViewController.h"
#import "MasterViewController.h"
#import "DetailViewController.h"

#include "interpret.h"
#include "hal_apple.h"
#include "compile.h"
#include "sys.h"
#include "hal_apple.h"

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
    NSFList *list2 = (NSFList*)f2o(context, latest);
    NSArray *list3 = list2.ordered;
    MasterViewController *mvc = [MasterViewController shared];
    [mvc setContacts:list3];
    
    return NULL;
}

struct variable *handle_chat(struct context *context)
{
    struct variable *args = (struct variable*)stack_pop(context->operand_stack);
    struct variable *from = param_var(args, 1);
    NSString *from2 = byte_array_to_nsstring(from->str);
    struct variable *message = param_var(args, 2);
    NSString *message2 = byte_array_to_nsstring(message->str);
    [DetailViewController handleChat:from2 message:message2];

    return NULL;
}

void add_callback(const char *key, void *fnc)
{
    struct byte_array *key2 = byte_array_from_string(key);
    struct variable *key3 = variable_new_str(context, key2);
    struct variable *val = variable_new_cfnc(context, fnc);
    variable_map_insert(context, callback, key3, val);
}

+ (id)shared
{
    if (NULL != imc)
        return imc;
    imc = [IMclient alloc];

    struct byte_array *mesh = read_resource("mesh.fg");
    struct byte_array *client = read_resource("im_client.fg");
    struct byte_array *args = byte_array_from_string("id='IOS'");
    struct byte_array *native1 = byte_array_from_string("client.main = roster_ui ");
    struct byte_array *native2 = byte_array_from_string("client.update_roster = update_roster ");
    struct byte_array *native3 = byte_array_from_string("client.handle_chat = handle_chat ");
    struct byte_array *script = byte_array_concatenate(6, mesh, args, client, native1, native2, native3);
    
    struct byte_array *program = build_string(script, NULL);
    context = context_new(NULL, true, true);

    callback = variable_new_list(context, NULL);
    add_callback("roster_ui", &roster_ui);
    add_callback("update_roster", &roster_update);
    add_callback("handle_chat", &handle_chat);

    context->singleton->callback = callback;
    execute_with(context, program, true);

    return imc;
}

- (void)addContact:(NSString*)username
{
    struct byte_array *code0 = byte_array_from_string("sys.print('client = ' + client)");
    interpret_string(context, code0);
    
    NSString *code = [NSString stringWithFormat:@"client.add_contact('%@')", username];
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

- (void)send:(NSString*)recipient message:(NSString*)message {
    NSString *code = [NSString stringWithFormat:@"client.send(%@,%@)", recipient, message];
    interpret_string_with(context, nsstring_to_byte_array(code));
}

@end
