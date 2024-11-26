//
//  ViewController.m
//  DexFileTransfer
//
//  Created by Dexter on 11/22/24.
//

#import "ViewController.h"
#import <UIKit/UIKit.h>
#import <Photos/Photos.h>
#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h> // Correct framework for UTType
#import <AVFoundation/AVFoundation.h>

#include "DexFtServer.h"
// Libraries to get local private address
#import <ifaddrs.h>
#import <arpa/inet.h>

@interface ViewController () <PHPickerViewControllerDelegate>
@property (weak, nonatomic) IBOutlet UILabel *ipLabel;
@property (weak, nonatomic) IBOutlet UISwitch *startSwitch;
@property (strong, nonatomic) NSMutableDictionary *mediaDictionary;
@end

@implementation ViewController

Dex::FileTransferServer ft;

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    
    // Check photo and video permission
    [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
            switch (status) {
                case PHAuthorizationStatusAuthorized:
                    NSLog(@"Access granted to the photo library");
                    break;
                case PHAuthorizationStatusDenied:
                case PHAuthorizationStatusRestricted:
                    NSLog(@"Access denied or restricted");
                    break;
                case PHAuthorizationStatusNotDetermined:
                    NSLog(@"Permission not determined");
                    break;
                default:
                    break;
            }
        }];

    // Retrieve and display the private IP address
    NSString *privateIPAddress = [self getPrivateIPAddress];
    NSLog(@"Private IP Address: %@", privateIPAddress);
    _ipLabel.text = privateIPAddress;
    
    // Initialize the dictionary
    self.mediaDictionary = [NSMutableDictionary dictionary];
}

- (IBAction)sendPhotosButtonPressed:(id)sender {
    [self pickMediaFiles];
}

- (IBAction)startSwitchChanged:(id)sender {
    NSLog(@"startSwitchChanged %d", _startSwitch.isOn);
    
    if (_startSwitch.isOn) {
        NSLog(@"Turning on");

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
//            Dex::FileTransferServer ft;
            //        ft.foo();
            ft.runServer();
            
            dispatch_async(dispatch_get_main_queue(), ^{
                NSLog(@"UI can be updated after blocking function completes");
            });
        });
    } else {
        NSLog(@"Turning off");
        ft.stopServer();
    }
}

- (NSString *)getPrivateIPAddress {
    NSString *address = @"Not Available";
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;

    // Retrieve the current interfaces
    if (getifaddrs(&interfaces) == 0) {
        temp_addr = interfaces;

        while (temp_addr != NULL) {
            // Check for IPv4 interface and ignore loopback addresses
            if (temp_addr->ifa_addr->sa_family == AF_INET) {
                NSString *interfaceName = [NSString stringWithUTF8String:temp_addr->ifa_name];
                if (![interfaceName isEqualToString:@"lo0"]) {
                    // Convert the address to a readable format
                    char ipAddress[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr, ipAddress, INET_ADDRSTRLEN);

                    address = [NSString stringWithUTF8String:ipAddress];
                    break; // Break after finding the first non-loopback IPv4 address
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }

    // Free the allocated memory for interfaces
    freeifaddrs(interfaces);

    return address;
}

- (void)pickMediaFiles {
    // PHPickerViewController for iOS 14+
    PHPickerConfiguration *config = [[PHPickerConfiguration alloc] init];
    config.selectionLimit = 0;  // Set to 0 for unlimited selection
    config.filter = [PHPickerFilter imagesFilter];  // Only images, if required
    
    PHPickerViewController *picker = [[PHPickerViewController alloc] initWithConfiguration:config];
    picker.delegate = self;  // Set delegate to self
    [self presentViewController:picker animated:YES completion:nil];
}

#pragma mark - PHPickerViewController Delegate Methods

// Delegate method for PHPickerViewController (iOS 14+)
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results {
    [picker dismissViewControllerAnimated:YES completion:nil];
    
    // Ensure that results are not empty
    if (results.count == 0) {
        NSLog(@"No files selected.");
        return;
    }
    
    // Print each file selected
    for (PHPickerResult *result in results) {
        // Attempt to load the object as a UIImage for images or an AVURLAsset for videos
        [result.itemProvider loadObjectOfClass:[UIImage class] completionHandler:^(id  _Nullable object, NSError * _Nullable error) {
            if (object && [object isKindOfClass:[UIImage class]]) {
                // Handle image
                NSLog(@"Selected image: %@", object);
                
                // Format the date (you can do this for the image if required, here is just an example):
                NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
                [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
                NSString *formattedDate = [formatter stringFromDate:[NSDate date]];  // You may need to associate with actual asset date
                
                // Save the image in dictionary with a timestamp as the key (use .jpg extension)
                NSString *key = [formattedDate stringByAppendingString:@".jpg"];
                self.mediaDictionary[key] = object;
                
                // Print asset details
                NSLog(@"Image file saved with key: %@", key);
            } else if (object && [object isKindOfClass:[AVURLAsset class]]) {
                // Handle video
                AVURLAsset *videoAsset = (AVURLAsset *)object;
                
                NSLog(@"Selected video URL: %@", videoAsset.URL);
                
                // Similarly format date for video
                NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
                [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
                NSString *formattedDate = [formatter stringFromDate:[NSDate date]];  // Adjust accordingly
                
                // Save video in dictionary with timestamp as the key (use .mp4 extension)
                NSString *key = [formattedDate stringByAppendingString:@".mp4"];
                self.mediaDictionary[key] = videoAsset;
                
                // Print asset details
                NSLog(@"Video file saved with key: %@", key);
            } else {
                NSLog(@"Error loading asset: %@", error.localizedDescription);
            }
        }];
    }
}

@end
