//
//  ViewController.m
//  DexFileTransfer
//
//  Created by Dexter on 11/22/24.
//

#import "ViewController.h"
#include "DexFtServer.h"
#include "DexFtClient.h"
#import <Photos/Photos.h>
// Libraries to get local private address
#import <ifaddrs.h>
#import <arpa/inet.h>

@interface ViewController ()
@property (weak, nonatomic) IBOutlet UILabel *localIp;
@property (weak, nonatomic) IBOutlet UISwitch *startSwitch;
@property (weak, nonatomic) IBOutlet UITextField *ftServerIp;
@property (weak, nonatomic) IBOutlet UITextField *filePattern;
@property (weak, nonatomic) IBOutlet UITextView *infoMessage;
@property (weak, nonatomic) IBOutlet UIButton *sendButton;
@end

@implementation ViewController

Dex::FileTransferServer ftServer;

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
    
    // Clear local ip label
    _localIp.text = @"";
    
    _infoMessage.text = @"";
    
    // Add tap gesture recognizer to dismiss keyboard
    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc]
                                    initWithTarget:self
                                    action:@selector(dismissKeyboard)];
    [self.view addGestureRecognizer:tap];
}

- (IBAction)startSwitchChanged:(id)sender {
    NSLog(@"startSwitchChanged %d", _startSwitch.isOn);
    
    if (_startSwitch.isOn) {
        NSLog(@"Turning on");
        
        self.infoMessage.text = @"Server listening...";
        
        // Retrieve and display the private IP address
        NSString *privateIPAddress = [self getPrivateIPAddress];
        NSLog(@"Private IP Address: %@", privateIPAddress);
        _localIp.text = privateIPAddress;
        
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            ftServer.runServer();
            
            dispatch_async(dispatch_get_main_queue(), ^{
                NSLog(@"UI can be updated after blocking function completes");
            });
        });
    } else {
        NSLog(@"Turning off");
        ftServer.stopServer();
        _localIp.text = @"";
        self.infoMessage.text = @"Server stopped";
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

- (IBAction)sendButtonPressed:(id)sender {
    std::string cftServerIp = [self.ftServerIp.text UTF8String];
    std::string cfilePattern = [self.filePattern.text UTF8String];
    self.sendButton.enabled = false;
    self.infoMessage.text = @"Sending...";
    printf("sendButtonPressed [%s] [%s]\n", cftServerIp.c_str(), cfilePattern.c_str());

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        Dex::FileTransferClient ftClient;
        int result = ftClient.runClient(cftServerIp.c_str(), Command::PUSH, cfilePattern.c_str());
        
        dispatch_async(dispatch_get_main_queue(), ^{
            printf("Send complete!\n");
            self.sendButton.enabled = true;
            if (result == 0) {
                self.infoMessage.text = @"Send complete";
            } else {
                self.infoMessage.text = @"Send failed";
            }
        });
    });
}

- (void)dismissKeyboard {
    [self.view endEditing:YES];
}
@end
