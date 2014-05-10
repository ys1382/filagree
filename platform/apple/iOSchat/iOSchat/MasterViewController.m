//
//  MasterViewController.m
//  iOSchat
//

#import "MasterViewController.h"
#import "DetailViewController.h"
#import "LoginViewController.h"
#import "IMclient.h"

@interface MasterViewController () {
    NSMutableArray *contacts;
}
@end

static MasterViewController *theMVC = NULL;

@implementation MasterViewController

+ (MasterViewController*)shared {
    return theMVC;
}

- (void)awakeFromNib
{
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        //self.clearsSelectionOnViewWillAppear = NO;
        self.preferredContentSize = CGSizeMake(320.0, 600.0);
    }
    [super awakeFromNib];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self.navigationController setToolbarHidden:NO];
    self.navigationItem.leftBarButtonItem = self.editButtonItem;
    self.detailViewController = (DetailViewController *)[[self.splitViewController.viewControllers lastObject] topViewController];
    [self setEditing:false];
    [self.tableView setDelegate:self];
    theMVC = self;

    NSString *title = [[LoginViewController shared] username].text;
    [self setTitle:title];
}

- (void)addContact:(id)sender
{
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"New Contact"
                                                    message:@"Whom would you like to add?"
                                                   delegate:self
                                          cancelButtonTitle:@"Cancel"
                                          otherButtonTitles:@"Done", nil];
    
    alert.alertViewStyle = UIAlertViewStylePlainTextInput;
    UITextField *textField = [alert textFieldAtIndex:0];
    textField.keyboardType = UIKeyboardTypeDefault;
    textField.placeholder = @"your new best friend's username";

    [alert show];
}

- (void)setContacts:(NSMutableArray*)latest {
    contacts = latest;
    [self.tableView reloadData];
}

- (void)alertView:(UIAlertView *)alert clickedButtonAtIndex:(NSInteger)buttonIndex
{
    if (1 != buttonIndex)
        return;
    UITextField *textField = [alert textFieldAtIndex:0];
    NSString *text = textField.text;
    if (text == nil)
        return;

    [[IMclient shared] addContact:text];
/*
    if (!contacts)
        contacts = [[NSMutableArray alloc] init];
    [contacts insertObject:text atIndex:0];//[NSDate date] atIndex:0];
    NSIndexPath *indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
    [self.tableView insertRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
*/
}



- (void)setEditing:(BOOL)editing animated:(BOOL)animated
{
    NSLog(@"Editing %i", editing);
    if (editing)
        self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Signout" style:UIBarButtonItemStylePlain target:self action:@selector(signout:)];
    else
        self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self action:@selector(addContact:)];
    [super setEditing:editing animated:animated];
}


#pragma mark - Table View

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return contacts.count;
}

- (void)signout:(id)sender {
    [self.navigationController popViewControllerAnimated:TRUE];
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    return YES;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell" forIndexPath:indexPath];

    NSDate *object = contacts[indexPath.row];
    cell.textLabel.text = [object description];
    return cell;
}


- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    assert(editingStyle == UITableViewCellEditingStyleDelete);
    [contacts removeObjectAtIndex:indexPath.row];
    [tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationFade];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        NSDate *object = contacts[indexPath.row];
        self.detailViewController.detailItem = object;
    }
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    if ([[segue identifier] isEqualToString:@"showDetail"]) {
        NSIndexPath *indexPath = [self.tableView indexPathForSelectedRow];
        NSDate *object = contacts[indexPath.row];
        [[segue destinationViewController] setDetailItem:object];
    }
}

@end
