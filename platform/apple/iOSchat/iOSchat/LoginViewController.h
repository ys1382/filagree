//
//  LoginViewController.h
//  iOSchat
//

#import <UIKit/UIKit.h>

@interface LoginViewController : UIViewController

+ (LoginViewController*)shared;


@property (weak, nonatomic) IBOutlet UITextField *username;
@property (weak, nonatomic) IBOutlet UITextField *password;

@property (weak, nonatomic) IBOutlet UIButton *signin;
@property (weak, nonatomic) IBOutlet UIButton *signup;

@property (weak, nonatomic) IBOutlet UILabel *authError;

@end
