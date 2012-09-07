#import <Cocoa/Cocoa.h>
#import "IOSurfaceTestView.h"
#include "Client.h"

#define INITIALWIDTH  640
#define INITIALHEIGHT 480

class ScopedPool
{
public:
    ScopedPool() { mPool = [[NSAutoreleasePool alloc] init]; }
    ~ScopedPool() { [mPool drain]; }

private:
    NSAutoreleasePool* mPool;
};

static void headerCallback(int width, int height, void* userData)
{
    ScopedPool pool;

    NSWindow* window = static_cast<NSWindow*>(userData);
    NSSize sz = NSMakeSize(width, height);
    float ratio = sz.height / sz.width;
    [window setAspectRatio:sz];
    sz.width = INITIALWIDTH;
    sz.height = sz.width * ratio;
    [window setContentSize:sz];
}

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
-(void)imageReady:(ImageWrapper*) imageWrapper;
+(Main*)instance;
@end

@implementation Main
static Main *sInstance = 0;

-(void)imageReady:(ImageWrapper *) imageWrapper
{
    //printf("image ready\n");
    [ioview setImage:imageWrapper->image];
    [ioview setNeedsDisplay:YES];
    CVBufferRelease(imageWrapper->image);
    [imageWrapper release];
}

-(id)init
{
    self = [super init];
    sInstance = self;

    return self;
}

+(Main*)instance
{
    return sInstance;
}
@end

void postImage(CVImageBufferRef image)
{
    ImageWrapper* wrapper = [[ImageWrapper alloc] init];
    wrapper->image = CVBufferRetain(image);
    [[Main instance] performSelectorOnMainThread:@selector(imageReady:) withObject:wrapper waitUntilDone:NO];
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Syntax: %s <host>\n", argv[0]);
        return 1;
    }

    ScopedPool pool;
    NSApplication* app = [NSApplication sharedApplication];

    Main* m = [[Main alloc] init];

    NSRect rect = NSMakeRect(0, 0, INITIALWIDTH, INITIALHEIGHT);
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

    NSRect screenRect = [[NSScreen mainScreen] frame];

    Client client(screenRect.size.width, screenRect.size.height, argv[1],
                  headerCallback, window);

    [app run];

    return 0;
}
