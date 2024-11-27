//
//  ViewController.m
//  DexFileTransfer
//
//  Created by Dexter on 11/22/24.
//

#import "ViewController.h"
#include "DexFtServer.h"
#include "DexFtClient.h"
#include <vector>
#import <Photos/Photos.h>
#import <PhotosUI/PhotosUI.h>
// Libraries to get local private address
#import <ifaddrs.h>
#import <arpa/inet.h>

@interface ViewController ()
@property (weak, nonatomic) IBOutlet UILabel *ipLabel;
@property (weak, nonatomic) IBOutlet UISwitch *startSwitch;
@property (nonatomic, strong) NSMutableArray<NSString *> *selectedFileNames;
@property (weak, nonatomic) IBOutlet UITextField *ftServerIP;
@property (weak, nonatomic) IBOutlet UIButton *sendFilesButton;
@end

@implementation ViewController

Dex::FileTransferServer ftServer;
Dex::FileTransferClient ftClient;

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
    
    // Initialize the array for storing selected file names
    self.selectedFileNames = [NSMutableArray array];

}

- (IBAction)openFilesButtonPressed:(id)sender {
    [self openFilePicker ];
}

- (IBAction)processButtonPressed:(id)sender {
    // Get the text from the UITextField (which is of type NSString*)
    NSString *ip = _ftServerIP.text;
    _sendFilesButton.enabled = false;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self processSelectedFileNames:ip];

        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"UI can be updated after blocking function completes");
            self->_sendFilesButton.enabled = true;
        });
    });
    
}

- (void)processSelectedFileNames:(NSString*)ip {
    if (self.selectedFileNames.count == 0) {
        NSLog(@"No files selected.");
        return;
    }
        
    // Create a C++ list to hold the file names
    std::vector<std::string> cppFileNames;
    
    NSLog(@"Processing selected files:");
    for (NSString *fileName in self.selectedFileNames) {
        // Perform your custom operation for each file name
        NSLog(@"File name: %@", fileName);
                
        // Convert NSString to std::string
         const char *utf8String = [fileName UTF8String];
         cppFileNames.push_back(std::string(utf8String));
        
//        std::string fileNameStr = std::string(utf8String);
//        PHAsset *foundAsset = nil;
//        bool found = ftClient.findFileInCameraRoll(fileNameStr, &foundAsset);
//        
//        if (found) {
//            // File was found, and foundAsset contains the PHAsset reference
//            NSLog(@"Found asset: %@", foundAsset);
//        } else {
//            // File not found
//            NSLog(@"File not found in Camera Roll");
//        }
    }
    const char *cIP = [ip UTF8String];
    ftClient.runClientIOS(cIP, Command::PUSH, "", cppFileNames);
}

// Function to open the file picker
- (void)openFilePicker {
    PHPickerConfiguration *config = [[PHPickerConfiguration alloc] init];
    config.selectionLimit = 0; // Allow multiple selection (0 = unlimited)
    config.filter = [PHPickerFilter anyFilterMatchingSubfilters:@[
        [PHPickerFilter imagesFilter],
        [PHPickerFilter videosFilter]
    ]];
    
    PHPickerViewController *picker = [[PHPickerViewController alloc] initWithConfiguration:config];
    picker.delegate = self; // Set the delegate
    [self presentViewController:picker animated:YES completion:nil];
}

// Delegate method called when the user finishes picking files
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results {
    [self dismissViewControllerAnimated:YES completion:nil];
    [self.selectedFileNames removeAllObjects]; // Clear previous selections

    for (PHPickerResult *result in results) {
        NSItemProvider *itemProvider = result.itemProvider;
        if ([itemProvider hasItemConformingToTypeIdentifier:@"public.image"] ||
            [itemProvider hasItemConformingToTypeIdentifier:@"public.movie"]) {
            
            [itemProvider loadFileRepresentationForTypeIdentifier:itemProvider.registeredTypeIdentifiers.firstObject
                                                completionHandler:^(NSURL * _Nullable url, NSError * _Nullable error) {
                if (url && !error) {
                    NSString *fileName = url.lastPathComponent;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self.selectedFileNames addObject:fileName];
                        NSLog(@"Added file: %@", fileName);
                    });
                }
            }];
        }
    }
}

- (IBAction)startSwitchChanged:(id)sender {
    NSLog(@"startSwitchChanged %d", _startSwitch.isOn);
    
    if (_startSwitch.isOn) {
        NSLog(@"Turning on");

        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
//            Dex::FileTransferServer ft;
            //        ft.foo();
            ftServer.runServer();
            
            dispatch_async(dispatch_get_main_queue(), ^{
                NSLog(@"UI can be updated after blocking function completes");
            });
        });
    } else {
        NSLog(@"Turning off");
        ftServer.stopServer();
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

@end
