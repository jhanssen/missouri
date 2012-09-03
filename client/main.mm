#import <Cocoa/Cocoa.h>
#import "IOSurfaceTestView.h"
#include "Client.h"

class ScopedPool
{
public:
    ScopedPool() { mPool = [[NSAutoreleasePool alloc] init]; }
    ~ScopedPool() { [mPool drain]; }

private:
    NSAutoreleasePool* mPool;
};

@interface ImageWrapper : NSObject
{
@public
    CVImageBufferRef image;
}
@end

@implementation ImageWrapper
@end

@interface Main : NSObject
{
@public
    IOSurfaceTestView* ioview;
}
-(id)init;
-(void)imageReady:(NSNotification *) imageNotification;
@end

@implementation Main
-(void)imageReady:(NSNotification *) imageNotification
{
    //printf("image ready\n");
    ImageWrapper* wrapper = [imageNotification object];
    [ioview setImage:wrapper->image];
    [ioview setNeedsDisplay:YES];
    CVBufferRelease(wrapper->image);
    [wrapper release];
}

-(id) init
{
    self = [super init];

    if (self != nil) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(imageReady:) name:@"MImageReady" object:nil];
    }

    return self;
}
@end

void postImage(CVImageBufferRef image)
{
    ImageWrapper* wrapper = [[ImageWrapper alloc] init];
    wrapper->image = CVBufferRetain(image);
    [[NSNotificationCenter defaultCenter] postNotificationName:@"MImageReady" object:wrapper];
}

int main(int argc, char** argv)
{
    ScopedPool pool;
    NSApplication* app = [NSApplication sharedApplication];

    Main* m = [[Main alloc] init];

    NSRect rect = NSMakeRect(0, 0, 640, 480);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:rect
                                         styleMask:(NSResizableWindowMask | NSClosableWindowMask | NSTitledWindowMask | NSMiniaturizableWindowMask)
                                         backing:NSBackingStoreBuffered defer:NO];
    [window retain];
    [window center];
    [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

    NSView* content_view = [window contentView];
    [content_view setAutoresizesSubviews:YES];
    IOSurfaceTestView* ioview = [[[IOSurfaceTestView alloc]
                                  initWithFrame:[content_view bounds]]
                                 autorelease];
    [ioview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    m->ioview = ioview;
    [content_view addSubview:ioview];
    [ioview retain];

    [app activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:window];

    Client client;

    [app run];

    return 0;
}
