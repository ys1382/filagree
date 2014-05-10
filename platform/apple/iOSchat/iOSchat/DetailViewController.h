//
//  DetailViewController.h
//  iOSchat
//

#import <UIKit/UIKit.h>

@interface DetailViewController : UIViewController <UISplitViewControllerDelegate>

@property (weak, nonatomic) IBOutlet UITextView *transcript;
@property (weak, nonatomic) IBOutlet UITextField *input;
@property (strong, nonatomic) id detailItem;

//@property (weak, nonatomic) IBOutlet UILabel *detailDescriptionLabel;
@end
