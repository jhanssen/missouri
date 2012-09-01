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
  printf("image ready\n");
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
    [NSApplication sharedApplication];

    Main* m = [[Main alloc] init];

    NSRect rect = NSMakeRect(0, 0, 1440, 810);
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
    IOSurfaceTestView* ioview = [[[IOSurfaceTestView alloc]
                                  initWithFrame:[content_view bounds]]
                                 autorelease];
    m->ioview = ioview;
    [content_view addSubview:ioview];
    [ioview retain];

    Client client;

    [[NSRunLoop currentRunLoop] run];

    return 0;
}
