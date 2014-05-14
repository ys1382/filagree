//
//  DetailViewController.h
//  iOSchat
//

#import <UIKit/UIKit.h>

@interface DetailViewController : UIViewController <UISplitViewControllerDelegate>

+ (void)handleChat:(NSString*)from message:(NSString*)message;

@property (weak, nonatomic) IBOutlet UIScrollView *scrollview;
@property (weak, nonatomic) IBOutlet UITextView *transcript;
@property (weak, nonatomic) IBOutlet UITextField *input;
@property (weak, nonatomic) IBOutlet UIButton *send;
@property (strong, nonatomic) id detailItem;

@end
