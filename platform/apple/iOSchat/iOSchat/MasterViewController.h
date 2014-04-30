//
//  MasterViewController.h
//  iOSchat
//

#import <UIKit/UIKit.h>

@class DetailViewController;

@interface MasterViewController : UIViewController <UITableViewDelegate>

+ (MasterViewController*)shared;
- (void)addContact:(id)sender;
- (void)setContacts:(NSArray*)latest;

@property (strong, nonatomic) DetailViewController *detailViewController;
@property (strong, nonatomic) IBOutlet UITableView *tableView;

@end
