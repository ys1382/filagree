//
//  LoginViewController.m
//  iOSchat
//

#import "LoginViewController.h"

#import "IMclient.h"

@interface LoginViewController ()

@end

static LoginViewController *theLVC = NULL;

@implementation LoginViewController

+ (LoginViewController*)shared {
    return theLVC;
}

- (void)viewWillAppear:(BOOL)animated {
    [self.navigationController setNavigationBarHidden:YES animated:animated];
    [super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
    [self.navigationController setNavigationBarHidden:NO animated:animated];
    [super viewWillDisappear:animated];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    theLVC = self;
}

- (IBAction)signined:(id)sender {
    [[IMclient shared] signin:[self.username text] withPassword:[self.password text]];
}

- (IBAction)signuped:(id)sender {
    [[IMclient shared] signup:[self.username text] withPassword:[self.password text]];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
