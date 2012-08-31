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

int main(int argc, char** argv)
{
    ScopedPool pool;
    [NSApplication sharedApplication];

    Client client;

    NSRect rect = NSMakeRect(0, 0, 1440, 900);
    NSWindow* window = [[[NSWindow alloc]
 		         initWithContentRect:rect
		       	 styleMask:NSTitledWindowMask
		         backing:NSBackingStoreBuffered
		         defer:NO]
                        autorelease];
    [window center];
    [window makeKeyAndOrderFront:nil];
    [window retain];

    NSView* content_view = [window contentView];
    IOSurfaceTestView* view = [[[IOSurfaceTestView alloc]
 		                initWithFrame:[content_view bounds]]
                               autorelease];
    [content_view addSubview:view];
    [view retain];

    [[NSRunLoop currentRunLoop] run];

    return 0;
}
