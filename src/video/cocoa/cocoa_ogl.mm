/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_ogl.mm Code related to the cocoa OpengL video driver. */

#ifdef WITH_COCOA

#include "../../stdafx.h"
#include "../../os/macosx/macos.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#undef Rect
#undef Point

#include "../../openttd.h"
#include "../../debug.h"
#include "../../core/geometry_func.hpp"
#include "../../core/math_func.hpp"
#include "../../core/mem_func.hpp"
#include "cocoa_ogl.h"
#include "cocoa_wnd.h"
#include "../../blitter/factory.hpp"
#include "../../gfx_func.h"
#include "../../framerate_type.h"
#include "../opengl.h"

#import <dlfcn.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>


/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

/** Platform-specific callback to get an OpenGL funtion pointer. */
static OGLProc GetOGLProcAddressCallback(const char *proc)
{
	static void *dl = nullptr;

	if (dl == nullptr) {
		dl = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
	}

	return reinterpret_cast<OGLProc>(dlsym(dl, proc));
}


@interface OTTD_OpenGLView : NSOpenGLView {
@private
	GLint currentVirtualScreen;
}
- (nullable instancetype)initWithFrame:(NSRect)frameRect pixelFormat:(nullable NSOpenGLPixelFormat*)format;
@end

@implementation OTTD_OpenGLView

- (nullable instancetype)initWithFrame:(NSRect)frameRect pixelFormat:(nullable NSOpenGLPixelFormat*)format
{
	if (self = [ super initWithFrame:frameRect pixelFormat:format ]) {
		self.wantsBestResolutionOpenGLSurface = YES;
	}
	return self;
}

- (BOOL)acceptsFirstResponder
{
	return NO;
}

- (BOOL)isOpaque
{
	return YES;
}

- (void)update
{
	[ super update ];

	GLint newVirtualScreen = [ self.openGLContext currentVirtualScreen];
	if ( currentVirtualScreen != newVirtualScreen ) {
		currentVirtualScreen = newVirtualScreen;

		if (OpenGLBackend::Get() != nullptr) {
			OpenGLBackend::Get()->RequeryHardwareCapabilities();

			[ self setNeedsDisplay:YES ];
		}
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	OpenGLBackend::Get()->Paint();
	if (_cursor.in_window) OpenGLBackend::Get()->DrawMouseCursor();

	glFlush();
}

@end

@interface OTTD_OpenGLLayer : NSOpenGLLayer {
@private
	NSOpenGLPixelFormat *_pixelFormat;
	NSOpenGLContext *_context;
}

- (instancetype)initWithContext:(NSOpenGLContext*)context pixelFormat:(NSOpenGLPixelFormat *)format;
@end

@implementation OTTD_OpenGLLayer

- (instancetype)initWithContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)format
{
	if (self = [ super init ]) {
		self->_context = context;
		self->_pixelFormat = format;

		self.magnificationFilter = kCAFilterNearest;
		self.opaque = YES;
	}

	return self;
}

- (void)dealloc
{
	[ self->_pixelFormat release ];
	[ self->_context release ];
	[ super dealloc ];
}

- (NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32_t)mask
{
	return self->_pixelFormat;
}

- (NSOpenGLContext *)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)pixelFormat
{
	return self->_context;
}

- (void)drawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts
{
	[ context makeCurrentContext ];

	OpenGLBackend::Get()->Paint();
	if (_cursor.in_window) OpenGLBackend::Get()->DrawMouseCursor();

	[ super drawInOpenGLContext:context pixelFormat:pixelFormat forLayerTime:t displayTime:ts ];
}
@end


@interface OTTD_OpenGLLayerView : NSView {
@private
	NSOpenGLPixelFormat *_pixelFormat;
	NSOpenGLContext *_context;
}
- (nullable instancetype)initWithFrame:(NSRect)frameRect pixelFormat:(nullable NSOpenGLPixelFormat *)format;

- (NSOpenGLContext *)openGLContext;
@end

@implementation OTTD_OpenGLLayerView

- (nullable instancetype)initWithFrame:(NSRect)frameRect pixelFormat:(nullable NSOpenGLPixelFormat *)format
{
	if (self = [ super initWithFrame:frameRect ]) {
		/* Allocate OpenGL context. */
		self->_pixelFormat = [ format retain ];
		self->_context = [ [ NSOpenGLContext alloc ] initWithFormat:format shareContext:nil ];
		if (self->_context == nil) {
			/* Context creation failed somehow. */
			[ self release ];
			return nil;
		}

		/* We manage our content updates ourselves. */
		self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
		self.wantsLayer = YES;
	}
	return self;
}

- (void)dealloc
{
	[ self->_pixelFormat release ];
	[ self->_context release ];
	[ super dealloc ];
}

- (BOOL)acceptsFirstResponder
{
	return NO;
}

- (BOOL)isOpaque
{
	return YES;
}

- (CALayer *)makeBackingLayer
{
	return [ [ OTTD_OpenGLLayer alloc ] initWithContext:self->_context pixelFormat:self->_pixelFormat ];
}

- (NSOpenGLContext *)openGLContext
{
	return self->_context;
}

@end

static FVideoDriver_CocoaOpenGL iFVideoDriver_CocoaOpenGL;


VideoDriver_CocoaOpenGL::VideoDriver_CocoaOpenGL()
{
	this->gl_view = nullptr;
	MemSetT(&this->dirty_rect, 0);
}

const char *VideoDriver_CocoaOpenGL::Start(const StringList &param)
{
	const char *err = this->Initialize();
	if (err != nullptr) return err;

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	if (bpp != 8 && bpp != 32) {
		this->Stop();
		return "The cocoa OpenGL subdriver only supports 8 and 32 bpp.";
	}

	/* Try to allocate GL context. */
	err = this->AllocateContext(GetDriverParamBool(param, "software"), !GetDriverParamBool(param, "no_layer"));
	if (err != nullptr) {
		this->Stop();
		return err;
	}

	bool fullscreen = _fullscreen;
	if (!this->MakeWindow(_cur_resolution.width, _cur_resolution.height)) {
		this->Stop();
		return "Could not create window";
	}

	this->AllocateBackingStore(true);

	if (fullscreen) this->ToggleFullscreen(fullscreen);

	this->GameSizeChanged();
	this->UpdateVideoModes();
	MarkWholeScreenDirty();

	return nullptr;

}

void VideoDriver_CocoaOpenGL::Stop()
{
	this->VideoDriver_Cocoa::Stop();

	OpenGLBackend::Destroy();
	[ this->gl_view release ];
}

/**
 * Set dirty a rectangle managed by a cocoa video subdriver.
 *
 * @param left Left x cooordinate of the dirty rectangle.
 * @param top Uppder y coordinate of the dirty rectangle.
 * @param width Width of the dirty rectangle.
 * @param height Height of the dirty rectangle.
 */
void VideoDriver_CocoaOpenGL::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	this->dirty_rect = BoundingRect(this->dirty_rect, r);
}

void VideoDriver_CocoaOpenGL::ClearSystemSprites()
{
	OpenGLBackend::Get()->ClearCursorCache();
}

const char *VideoDriver_CocoaOpenGL::AllocateContext(bool allow_software, bool use_layer)
{
	NSOpenGLPixelFormatAttribute attribs[] = {
		NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAllowOfflineRenderers,
		(NSOpenGLPixelFormatAttribute)kCGLPFASupportsAutomaticGraphicsSwitching,
		allow_software ? (NSOpenGLPixelFormatAttribute)0 : NSOpenGLPFAAccelerated,
		allow_software ? (NSOpenGLPixelFormatAttribute)0 : NSOpenGLPFANoRecovery,
		(NSOpenGLPixelFormatAttribute)0,
	};

	NSOpenGLPixelFormat *pf = [ [ [ NSOpenGLPixelFormat alloc ] initWithAttributes:attribs ] autorelease ];
	if (pf == nil) return "No suitable pixel format found";

	if (use_layer) {
		OTTD_OpenGLLayerView *view = [ [ OTTD_OpenGLLayerView alloc ] initWithFrame:NSMakeRect(0, 0, 64, 64) pixelFormat:pf ];
		if (view == nil) return "Can't create OpenGL view";
		this->gl_view = view;

		[ view.openGLContext makeCurrentContext ];
	} else {
		OTTD_OpenGLView *view = [ [ OTTD_OpenGLView alloc ] initWithFrame:NSMakeRect(0, 0, 64, 64) pixelFormat:pf ];
		if (view == nil) return "Can't create OpenGL view";
		this->gl_view = view;

		[ view.openGLContext makeCurrentContext ];
	}

	return OpenGLBackend::Create(&GetOGLProcAddressCallback);
}

NSView *VideoDriver_CocoaOpenGL::AllocateDrawView()
{
	this->gl_view.frame = this->cocoaview.bounds;
	return [ this->gl_view retain ];
}

/** Resize the window. */
void VideoDriver_CocoaOpenGL::AllocateBackingStore(bool force)
{
	if (this->window == nil || this->gl_view == nil || this->setup) return;

	if (_screen.dst_ptr != nullptr) this->ReleaseVideoPointer();

	MemSetT(&this->dirty_rect, 0);
	OpenGLBackend::Get()->Resize(this->gl_view.bounds.size.width, this->gl_view.bounds.size.height, force);
	_screen.dst_ptr = this->GetVideoPointer();

	/* Redraw screen */
	this->GameSizeChanged();
}

void *VideoDriver_CocoaOpenGL::GetVideoPointer()
{
	if (BlitterFactory::GetCurrentBlitter()->NeedsAnimationBuffer()) {
		this->anim_buffer = OpenGLBackend::Get()->GetAnimBuffer();
	}
	return OpenGLBackend::Get()->GetVideoBuffer();
}

void VideoDriver_CocoaOpenGL::ReleaseVideoPointer()
{
	if (this->anim_buffer != nullptr) OpenGLBackend::Get()->ReleaseAnimBuffer(this->dirty_rect);
	OpenGLBackend::Get()->ReleaseVideoBuffer(this->dirty_rect);
	MemSetT(&this->dirty_rect, 0);
	_screen.dst_ptr = nullptr;
	this->anim_buffer = nullptr;
}

void VideoDriver_CocoaOpenGL::CheckPaletteAnim()
{
	if (_cur_palette.count_dirty == 0) return;

	[ this->cocoaview setNeedsDisplay:YES ];
}

void VideoDriver_CocoaOpenGL::Draw(bool force_update)
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		/* Always push a changed palette to OpenGL. */
		OpenGLBackend::Get()->UpdatePalette(_cur_palette.palette, _cur_palette.first_dirty, _cur_palette.count_dirty);
		if (blitter->UsePaletteAnimation() == Blitter::PALETTE_ANIMATION_BLITTER) {
			blitter->PaletteAnimate(_cur_palette);
		}

		_cur_palette.count_dirty = 0;
	}

	[ this->cocoaview setNeedsDisplay:YES ];
	if (force_update) [ this->cocoaview displayIfNeeded ];
}

#endif /* WITH_COCOA */
