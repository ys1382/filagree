//
//  DetailViewController.m
//  iOSchat
//


#import "DetailViewController.h"
#import "IMclient.h"


@interface DetailViewController ()

@property (strong, nonatomic) UIPopoverController *masterPopoverController;

- (void)configureView;
+ (NSMutableArray *) chatFor:(NSString *)from;

@end


static DetailViewController *theDVC = NULL;
static NSMutableDictionary *chats = NULL;
static NSString *current = NULL;


@implementation DetailViewController


#pragma mark - Managing the detail item

- (void)setDetailItem:(id)newDetailItem
{
    if (_detailItem != newDetailItem)
    {
        _detailItem = newDetailItem;
        [self configureView];
    }

    if (self.masterPopoverController != nil)
        [self.masterPopoverController dismissPopoverAnimated:YES];
}

- (void)configureView
{
    if (NULL == self.detailItem)
        return;
    self.title = self.detailItem;

    NSArray *record = [chats objectForKey:current];
    NSString *record2 = @"";
    for (NSString *line in record)
        record2 = [record2 stringByAppendingFormat:@"%@\n", line];
    self.transcript.text = record2;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    theDVC = self;
    current = self.detailItem;

    if (NULL == chats)
        chats = [[NSMutableDictionary alloc] init];

    [self configureView];
    [self registerForKeyboardNotifications];
}

/*
// hide the keyboard when you hit return
- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    [textField resignFirstResponder];
    return NO;
}
*/

// Call this method somewhere in your view controller setup code.
- (void)registerForKeyboardNotifications
{
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(keyboardWasShown:)
                                                 name:UIKeyboardDidShowNotification object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(keyboardWillBeHidden:)
                                                 name:UIKeyboardWillHideNotification object:nil];
}

// Called when the UIKeyboardDidShowNotification is sent.
- (void)keyboardWasShown:(NSNotification*)aNotification
{
    NSDictionary* info = [aNotification userInfo];
    CGSize kbSize = [[info objectForKey:UIKeyboardFrameBeginUserInfoKey] CGRectValue].size;
    
    UIEdgeInsets contentInsets = UIEdgeInsetsMake(0.0, 0.0, kbSize.height, 0.0);
    [self scrollview].contentInset = contentInsets;
    [self scrollview].scrollIndicatorInsets = contentInsets;
    
    // If active text field is hidden by keyboard, scroll it so it's visible
    // Your app might not need or want this behavior.
    CGRect aRect = self.view.frame;
    aRect.size.height -= kbSize.height;

    if (!CGRectContainsPoint(aRect, self.input.frame.origin) )
        [[self scrollview] scrollRectToVisible:self.input.frame animated:YES];
}

// Called when the UIKeyboardWillHideNotification is sent
- (void)keyboardWillBeHidden:(NSNotification*)aNotification
{
    UIEdgeInsets contentInsets = UIEdgeInsetsZero;
    [self scrollview].contentInset = contentInsets;
    [self scrollview].scrollIndicatorInsets = contentInsets;
}

- (IBAction)sent:(id)sender
{
    NSString *recipient = self.detailItem;
    NSString *message = self.input.text;
    [self.input resignFirstResponder]; // hide keyboard

    [[IMclient shared] send:recipient message:message];
    
    NSMutableArray *record = [DetailViewController chatFor:recipient];
    message = [NSString stringWithFormat:@"You: %@", message];
    [record addObject:message];
}

+ (NSMutableArray *) chatFor:(NSString *)from
{
    NSMutableArray *record = [chats objectForKey:from];
    if (NULL == record)
    {
        record = [NSMutableArray array];
        [chats setObject:record forKey:from];
    }
    return record;
}

+ (void)handleChat:(NSString *)from message:(NSString *)message
{
    NSMutableArray *record = [DetailViewController chatFor:from];
    message = [NSString stringWithFormat:@"%@: %@", from, message];
    [record addObject:message];

    if ([from isEqualToString:current])
        [theDVC configureView];
}

#pragma mark - Split view

- (void)splitViewController:(UISplitViewController *)splitController willHideViewController:(UIViewController *)viewController withBarButtonItem:(UIBarButtonItem *)barButtonItem forPopoverController:(UIPopoverController *)popoverController
{
    barButtonItem.title = NSLocalizedString(@"Master", @"Master");
    [self.navigationItem setLeftBarButtonItem:barButtonItem animated:YES];
    self.masterPopoverController = popoverController;
}

- (void)splitViewController:(UISplitViewController *)splitController willShowViewController:(UIViewController *)viewController invalidatingBarButtonItem:(UIBarButtonItem *)barButtonItem
{
    // Called when the view is shown again in the split view, invalidating the button and popover controller.
    [self.navigationItem setLeftBarButtonItem:nil animated:YES];
    self.masterPopoverController = nil;
}

@end
