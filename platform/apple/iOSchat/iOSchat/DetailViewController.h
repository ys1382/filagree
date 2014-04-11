//
//  DetailViewController.h
//  iOSchat
//
//  Created by Yusuf on 4/10/14.
//  Copyright (c) 2014 fisil. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface DetailViewController : UIViewController <UISplitViewControllerDelegate>

@property (strong, nonatomic) id detailItem;

@property (weak, nonatomic) IBOutlet UILabel *detailDescriptionLabel;
@end
