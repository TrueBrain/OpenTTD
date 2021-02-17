/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_v.mm Code related to the cocoa video driver(s). */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  Nothing at the moment.                                                    *
 ******************************************************************************/

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
#include "../../core/geometry_type.hpp"
#include "../../core/math_func.hpp"
#include "cocoa_v.h"
#include "cocoa_wnd.h"
#include "../../blitter/factory.hpp"
#include "../../framerate_type.h"
#include "../../network/network.h"
#include "../../gfx_func.h"
#include "../../thread.h"
#include "../../core/random_func.hpp"
#include "../../settings_type.h"
#include "../../window_func.h"
#include "../../window_gui.h"

#include <array>
#import <sys/param.h> /* for MAXPATHLEN */
#import <sys/time.h> /* gettimeofday */

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

/* On some old versions of MAC OS this may not be defined.
 * Those versions generally only produce code for PPC. So it should be safe to
 * set this to 0. */
#ifndef kCGBitmapByteOrder32Host
#define kCGBitmapByteOrder32Host 0
#endif

bool _cocoa_video_started = false;

extern bool _tab_is_down;

#ifdef _DEBUG
static uint32 _tEvent;
#endif


/** List of common display/window sizes. */
static const Dimension _default_resolutions[] = {
	{  640,  480 },
	{  800,  600 },
	{ 1024,  768 },
	{ 1152,  864 },
	{ 1280,  800 },
	{ 1280,  960 },
	{ 1280, 1024 },
	{ 1400, 1050 },
	{ 1600, 1200 },
	{ 1680, 1050 },
	{ 1920, 1200 },
	{ 2560, 1440 }
};


static uint32 GetTick()
{
	struct timeval tim;

	gettimeofday(&tim, NULL);
	return tim.tv_usec / 1000 + tim.tv_sec * 1000;
}


VideoDriver_Cocoa::VideoDriver_Cocoa()
{
	this->active        = false;
	this->setup         = false;
	this->buffer_locked = false;

	this->window    = nil;
	this->cocoaview = nil;
	this->delegate  = nil;

	this->color_space = nullptr;
}

/**
 * Stop the cocoa video subdriver.
 */
void VideoDriver_Cocoa::Stop()
{
	if (!_cocoa_video_started) return;

	CocoaExitApplication();

	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];
	[ this->cocoaview release ];
	[ this->delegate release ];

	CGColorSpaceRelease(this->color_space);

	_cocoa_video_started = false;
}

/** Common driver initialization. */
const char *VideoDriver_Cocoa::Initialize()
{
	if (!MacOSVersionIsAtLeast(10, 7, 0)) return "The Cocoa video driver requires Mac OS X 10.7 or later.";

	if (_cocoa_video_started) return "Already started";
	_cocoa_video_started = true;

	/* Don't create a window or enter fullscreen if we're just going to show a dialog. */
	if (!CocoaSetupApplication()) return nullptr;

	this->UpdateAutoResolution();
	this->orig_res = _cur_resolution;

	return nullptr;
}

/**
 * Start the main programme loop when using a cocoa video driver.
 */
void VideoDriver_Cocoa::MainLoop()
{
	/* Restart game loop if it was already running (e.g. after bootstrapping),
	 * otherwise this call is a no-op. */
	[ [ NSNotificationCenter defaultCenter ] postNotificationName:OTTDMainLaunchGameEngine object:nil ];

	/* Start the main event loop. */
	[ NSApp run ];
}

/**
 * Change the resolution when using a cocoa video driver.
 *
 * @param w New window width.
 * @param h New window height.
 * @return Whether the video driver was successfully updated.
 */
bool VideoDriver_Cocoa::ChangeResolution(int w, int h)
{
	NSSize screen_size = [ [ NSScreen mainScreen ] frame ].size;
	if (w > screen_size.width) w = screen_size.width;
	if (h > screen_size.height) h = screen_size.height;

	NSRect contentRect = NSMakeRect(0, 0, w, h);
	[ this->window setContentSize:contentRect.size ];

	/* Ensure frame height - title bar height >= view height */
	float content_height = [ this->window contentRectForFrameRect:[ this->window frame ] ].size.height;
	contentRect.size.height = Clamp(h, 0, (int)content_height);

	if (this->cocoaview != nil) {
		h = (int)contentRect.size.height;
		[ this->cocoaview setFrameSize:contentRect.size ];
	}

	[ (OTTD_CocoaWindow *)this->window center ];
	this->AllocateBackingStore();

	return true;
}

/**
 * Toggle between windowed and full screen mode for cocoa display driver.
 *
 * @param full_screen Whether to switch to full screen or not.
 * @return Whether the mode switch was successful.
 */
bool VideoDriver_Cocoa::ToggleFullscreen(bool full_screen)
{
	if (this->IsFullscreen() == full_screen) return true;

	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		[ this->window performSelector:@selector(toggleFullScreen:) withObject:this->window ];
		this->UpdateVideoModes();
		return true;
	}

	return false;
}

/**
 * Callback invoked after the blitter was changed.
 *
 * @return True if no error.
 */
bool VideoDriver_Cocoa::AfterBlitterChange()
{
	this->ChangeResolution(_screen.width, _screen.height);
	return true;
}

/**
 * An edit box lost the input focus. Abort character compositing if necessary.
 */
void VideoDriver_Cocoa::EditBoxLostFocus()
{
	[ [ this->cocoaview inputContext ] discardMarkedText ];
	/* Clear any marked string from the current edit box. */
	HandleTextInput(NULL, true);
}

/**
 * Get the resolution of the main screen.
 */
Dimension VideoDriver_Cocoa::GetScreenSize() const
{
	NSRect frame = [ [ NSScreen mainScreen ] frame ];
	return { static_cast<uint>(NSWidth(frame)), static_cast<uint>(NSHeight(frame)) };
}

/** Lock video buffer for drawing if it isn't already mapped. */
bool VideoDriver_Cocoa::LockVideoBuffer()
{
	if (this->buffer_locked) return false;
	this->buffer_locked = true;

	_screen.dst_ptr = this->GetVideoPointer();
	assert(_screen.dst_ptr != nullptr);

	return true;
}

/** Unlock video buffer. */
void VideoDriver_Cocoa::UnlockVideoBuffer()
{
	if (_screen.dst_ptr != nullptr) {
		/* Hand video buffer back to the drawing backend. */
		this->ReleaseVideoPointer();
		_screen.dst_ptr = nullptr;
	}

	this->buffer_locked = false;
}

/**
 * Are we in fullscreen mode
 * @return whether fullscreen mode is currently used
 */
bool VideoDriver_Cocoa::IsFullscreen()
{
	return this->window != nil && ([ this->window styleMask ] & NSWindowStyleMaskFullScreen) != 0;
}

/**
 * Handle a change of the display area.
 */
void VideoDriver_Cocoa::GameSizeChanged()
{
	/* Store old window size if we entered fullscreen mode. */
	bool fullscreen = this->IsFullscreen();
	if (fullscreen && !_fullscreen) this->orig_res = _cur_resolution;
	_fullscreen = fullscreen;

	BlitterFactory::GetCurrentBlitter()->PostResize();

	::GameSizeChanged();
}

/**
 * Update the video modus.
 */
void VideoDriver_Cocoa::UpdateVideoModes()
{
	_resolutions.clear();

	if (this->IsFullscreen()) {
		/* Full screen, there is only one possible resolution. */
		NSSize screen = [ [ this->window screen ] frame ].size;
		_resolutions.emplace_back((uint)screen.width, (uint)screen.height);
	} else {
		/* Windowed; offer a selection of common window sizes up until the
		 * maximum usable screen space. This excludes the menu and dock areas. */
		NSSize maxSize = [ [ NSScreen mainScreen] visibleFrame ].size;
		for (const auto &d : _default_resolutions) {
			if (d.width < maxSize.width && d.height < maxSize.height) _resolutions.push_back(d);
		}
		_resolutions.emplace_back((uint)maxSize.width, (uint)maxSize.height);
	}
}

/**
 * Build window and view with a given size.
 * @param width Window width.
 * @param height Window height.
 */
bool VideoDriver_Cocoa::MakeWindow(int width, int height)
{
	this->setup = true;

	NSSize screen_size = [ [ NSScreen mainScreen ] frame ].size;
	if (width > screen_size.width) width = screen_size.width;
	if (height > screen_size.height) height = screen_size.height;

	NSRect contentRect = NSMakeRect(0, 0, width, height);

	/* Create main window. */
	unsigned int style = NSTitledWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask | NSClosableWindowMask;
	this->window = [ [ OTTD_CocoaWindow alloc ] initWithContentRect:contentRect styleMask:style backing:NSBackingStoreBuffered defer:NO driver:this ];
	if (this->window == nil) {
		DEBUG(driver, 0, "Could not create the Cocoa window.");
		this->setup = false;
		return false;
	}

	/* Add built in full-screen support when available (OS X 10.7 and higher)
	 * This code actually compiles for 10.5 and later, but only makes sense in conjunction
	 * with the quartz fullscreen support as found only in 10.7 and later
	 */
	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		NSWindowCollectionBehavior behavior = [ this->window collectionBehavior ];
		behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
		[ this->window setCollectionBehavior:behavior ];

		NSButton* fullscreenButton = [ this->window standardWindowButton:NSWindowFullScreenButton ];
		[ fullscreenButton setAction:@selector(toggleFullScreen:) ];
		[ fullscreenButton setTarget:this->window ];
	}

	this->delegate = [ [ OTTD_CocoaWindowDelegate alloc ] initWithDriver:this ];
	[ this->window setDelegate:this->delegate ];

	[ this->window center ];
	[ this->window makeKeyAndOrderFront:nil ];

	/* Create wrapper view for text input. */
	NSRect view_frame = [ this->window contentRectForFrameRect:[ this->window frame ] ];
	this->cocoaview = [ [ OTTD_CocoaView alloc ] initWithFrame:view_frame ];
	if (this->cocoaview == nil) {
		DEBUG(driver, 0, "Could not create the text wrapper view.");
		this->setup = false;
		return false;
	}
	[ this->cocoaview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable ];

	/* Create content view. */
	NSView *draw_view = this->AllocateDrawView();
	if (draw_view == nil) {
		DEBUG(driver, 0, "Could not create the drawing view.");
		this->setup = false;
		return false;
	}
	[ draw_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable ];

	[ this->window setContentView:this->cocoaview ];
	[ this->cocoaview addSubview:draw_view ];
	[ this->window makeFirstResponder:this->cocoaview ];
	[ draw_view release ];

	[ this->window setColorSpace:[ NSColorSpace sRGBColorSpace ] ];
	CGColorSpaceRelease(this->color_space);
	this->color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	if (this->color_space == nullptr) this->color_space = CGColorSpaceCreateDeviceRGB();
	if (this->color_space == nullptr) error("Could not get a valid colour space for drawing.");

	this->setup = false;

	return true;
}

bool VideoDriver_Cocoa::PollEvent()
{
#ifdef _DEBUG
	uint32 et0 = GetTick();
#endif
	NSEvent *event = [ NSApp nextEventMatchingMask:NSAnyEventMask
				untilDate:[ NSDate distantPast ]
				inMode:NSDefaultRunLoopMode dequeue:YES ];
#ifdef _DEBUG
	_tEvent += GetTick() - et0;
#endif

	if (event == nil) return false;

	[ NSApp sendEvent:event ];

	return true;
}


void VideoDriver_Cocoa::GameLoop()
{
	auto cur_ticks = std::chrono::steady_clock::now();
	auto last_realtime_tick = cur_ticks;
	auto next_game_tick = cur_ticks;
	auto next_draw_tick = cur_ticks;

	this->CheckPaletteAnim();
	for (;;) {
		@autoreleasepool {

			InteractiveRandom(); // randomness

			while (this->PollEvent()) {}

			if (_exit_game) {
				/* Restore saved resolution if in fullscreen mode. */
				if (this->IsFullscreen()) _cur_resolution = this->orig_res;
				break;
			}

			NSUInteger cur_mods = [ NSEvent modifierFlags ];

#if defined(_DEBUG)
			if (cur_mods & NSShiftKeyMask)
#else
			if (_tab_is_down)
#endif
			{
				if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
			} else if (_fast_forward & 2) {
				_fast_forward = 0;
			}

			cur_ticks = std::chrono::steady_clock::now();

			/* If more than a millisecond has passed, increase the _realtime_tick. */
			if (cur_ticks - last_realtime_tick > std::chrono::milliseconds(1)) {
				auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(cur_ticks - last_realtime_tick);
				_realtime_tick += delta.count();
				last_realtime_tick += delta;
			}

			if (cur_ticks >= next_game_tick || (_fast_forward && !_pause_mode)) {
				next_game_tick = cur_ticks + std::chrono::milliseconds(MILLISECONDS_PER_TICK);

				this->UnlockVideoBuffer();
				::GameLoop();
				this->LockVideoBuffer();
			}

			if (cur_ticks >= next_draw_tick) {
				next_draw_tick = cur_ticks + std::chrono::milliseconds(MILLISECONDS_PER_TICK);

				bool old_ctrl_pressed = _ctrl_pressed;

				_ctrl_pressed = !!(cur_mods & ( _settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? NSControlKeyMask : NSCommandKeyMask));
				_shift_pressed = !!(cur_mods & NSShiftKeyMask);

				if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

				InputLoop();
				UpdateWindows();
				this->CheckPaletteAnim();

				this->Draw();
			}

			if (!_fast_forward || _pause_mode) {
				CSleep(1);
			}
		}
	}
}


/* Subclass of OTTD_CocoaView to fix Quartz rendering */
@interface OTTD_QuartzView : NSView {
	VideoDriver_CocoaQuartz *driver;
}
- (instancetype)initWithFrame:(NSRect)frameRect andDriver:(VideoDriver_CocoaQuartz *)drv;
@end

@implementation OTTD_QuartzView

- (instancetype)initWithFrame:(NSRect)frameRect andDriver:(VideoDriver_CocoaQuartz *)drv
{
	if (self = [ super initWithFrame:frameRect ]) {
		self->driver = drv;

		/* We manage our content updates ourselves. */
		self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
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

- (BOOL)wantsLayer
{
	return YES;
}

- (BOOL)wantsUpdateLayer
{
	return YES;
}

- (void)updateLayer
{
	if (driver->cgcontext == nullptr) return;

	CGImageRef fullImage = CGBitmapContextCreateImage(driver->cgcontext);
	self.layer.contents = (__bridge id)fullImage;
	self.layer.magnificationFilter = kCAFilterNearest;
	CGImageRelease(fullImage);
}

@end


static FVideoDriver_CocoaQuartz iFVideoDriver_CocoaQuartz;

/** Clear buffer to opaque black. */
static void ClearWindowBuffer(uint32 *buffer, uint32 pitch, uint32 height)
{
	uint32 fill = Colour(0, 0, 0).data;
	for (uint32 y = 0; y < height; y++) {
		for (uint32 x = 0; x < pitch; x++) {
			buffer[y * pitch + x] = fill;
		}
	}
}

VideoDriver_CocoaQuartz::VideoDriver_CocoaQuartz()
{
	this->window_width  = 0;
	this->window_height = 0;
	this->window_pitch  = 0;
	this->buffer_depth  = 0;
	this->window_buffer = nullptr;
	this->pixel_buffer  = nullptr;

	this->cgcontext     = nullptr;

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

const char *VideoDriver_CocoaQuartz::Start(const StringList &param)
{
	const char *err = this->Initialize();
	if (err != nullptr) return err;

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	if (bpp != 8 && bpp != 32) {
		Stop();
		return "The cocoa quartz subdriver only supports 8 and 32 bpp.";
	}

	bool fullscreen = _fullscreen;
	if (!this->MakeWindow(_cur_resolution.width, _cur_resolution.height)) {
		Stop();
		return "Could not create window";
	}

	this->AllocateBackingStore(true);

	if (fullscreen) this->ToggleFullscreen(fullscreen);

	this->GameSizeChanged();
	this->UpdateVideoModes();

	return nullptr;

}

void VideoDriver_CocoaQuartz::Stop()
{
	this->VideoDriver_Cocoa::Stop();

	CGContextRelease(this->cgcontext);

	free(this->window_buffer);
	free(this->pixel_buffer);
}

/**
 * Set dirty a rectangle managed by a cocoa video subdriver.
 *
 * @param left Left x cooordinate of the dirty rectangle.
 * @param top Uppder y coordinate of the dirty rectangle.
 * @param width Width of the dirty rectangle.
 * @param height Height of the dirty rectangle.
 */
void VideoDriver_CocoaQuartz::MakeDirty(int left, int top, int width, int height)
{
	if (this->num_dirty_rects < MAX_DIRTY_RECTS) {
		this->dirty_rects[this->num_dirty_rects].left = left;
		this->dirty_rects[this->num_dirty_rects].top = top;
		this->dirty_rects[this->num_dirty_rects].right = left + width;
		this->dirty_rects[this->num_dirty_rects].bottom = top + height;
	}
	this->num_dirty_rects++;
}

NSView *VideoDriver_CocoaQuartz::AllocateDrawView()
{
	return [ [ OTTD_QuartzView alloc ] initWithFrame:[ this->cocoaview bounds ] andDriver:this ];
}

/** Resize the window. */
void VideoDriver_CocoaQuartz::AllocateBackingStore(bool force)
{
	if (this->window == nil || this->cocoaview == nil || this->setup) return;

	this->UpdatePalette(0, 256);

	NSRect newframe = [ this->cocoaview frame ];

	this->window_width = (int)newframe.size.width;
	this->window_height = (int)newframe.size.height;
	this->window_pitch = Align(this->window_width, 16 / sizeof(uint32)); // Quartz likes lines that are multiple of 16-byte.
	this->buffer_depth = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	/* Create Core Graphics Context */
	free(this->window_buffer);
	this->window_buffer = malloc(this->window_pitch * this->window_height * sizeof(uint32));
	/* Initialize with opaque black. */
	ClearWindowBuffer((uint32 *)this->window_buffer, this->window_pitch, this->window_height);

	CGContextRelease(this->cgcontext);
	this->cgcontext = CGBitmapContextCreate(
		this->window_buffer,       // data
		this->window_width,        // width
		this->window_height,       // height
		8,                         // bits per component
		this->window_pitch * 4,    // bytes per row
		this->color_space,         // color space
		kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host
	);

	assert(this->cgcontext != NULL);
	CGContextSetShouldAntialias(this->cgcontext, FALSE);
	CGContextSetAllowsAntialiasing(this->cgcontext, FALSE);
	CGContextSetInterpolationQuality(this->cgcontext, kCGInterpolationNone);

	if (this->buffer_depth == 8) {
		free(this->pixel_buffer);
		this->pixel_buffer = malloc(this->window_width * this->window_height);
		if (this->pixel_buffer == nullptr) usererror("Out of memory allocating pixel buffer");
	} else {
		free(this->pixel_buffer);
		this->pixel_buffer = nullptr;
	}

	/* Tell the game that the resolution has changed */
	_screen.width   = this->window_width;
	_screen.height  = this->window_height;
	_screen.pitch   = this->buffer_depth == 8 ? this->window_width : this->window_pitch;
	_screen.dst_ptr = this->GetVideoPointer();

	/* Redraw screen */
	this->num_dirty_rects = MAX_DIRTY_RECTS;
	this->GameSizeChanged();
}

/**
 * This function copies 8bpp pixels from the screen buffer in 32bpp windowed mode.
 *
 * @param left The x coord for the left edge of the box to blit.
 * @param top The y coord for the top edge of the box to blit.
 * @param right The x coord for the right edge of the box to blit.
 * @param bottom The y coord for the bottom edge of the box to blit.
 */
void VideoDriver_CocoaQuartz::BlitIndexedToView32(int left, int top, int right, int bottom)
{
	const uint32 *pal   = this->palette;
	const uint8  *src   = (uint8*)this->pixel_buffer;
	uint32       *dst   = (uint32*)this->window_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_pitch;

	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}


/** Update the palette */
void VideoDriver_CocoaQuartz::UpdatePalette(uint first_color, uint num_colors)
{
	if (this->buffer_depth != 8) return;

	for (uint i = first_color; i < first_color + num_colors; i++) {
		uint32 clr = 0xff000000;
		clr |= (uint32)_cur_palette.palette[i].r << 16;
		clr |= (uint32)_cur_palette.palette[i].g << 8;
		clr |= (uint32)_cur_palette.palette[i].b;
		this->palette[i] = clr;
	}

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

void VideoDriver_CocoaQuartz::CheckPaletteAnim()
{
	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				this->UpdatePalette(_cur_palette.first_dirty, _cur_palette.count_dirty);
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_cur_palette);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}
}

/** Draw window
 * @param force_update Whether to redraw unconditionally
 */
void VideoDriver_CocoaQuartz::Draw(bool force_update)
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	/* Check if we need to do anything */
	if (this->num_dirty_rects == 0 || [ this->window isMiniaturized ]) return;

	if (this->num_dirty_rects >= MAX_DIRTY_RECTS) {
		this->num_dirty_rects = 1;
		this->dirty_rects[0].left = 0;
		this->dirty_rects[0].top = 0;
		this->dirty_rects[0].right = this->window_width;
		this->dirty_rects[0].bottom = this->window_height;
	}

	/* Build the region of dirty rectangles */
	for (int i = 0; i < this->num_dirty_rects; i++) {
		/* We only need to blit in indexed mode since in 32bpp mode the game draws directly to the image. */
		if (this->buffer_depth == 8) {
			BlitIndexedToView32(
				this->dirty_rects[i].left,
				this->dirty_rects[i].top,
				this->dirty_rects[i].right,
				this->dirty_rects[i].bottom
			);
		}

		NSRect dirtyrect;
		dirtyrect.origin.x = this->dirty_rects[i].left;
		dirtyrect.origin.y = this->window_height - this->dirty_rects[i].bottom;
		dirtyrect.size.width = this->dirty_rects[i].right - this->dirty_rects[i].left;
		dirtyrect.size.height = this->dirty_rects[i].bottom - this->dirty_rects[i].top;

		/* Normally drawRect will be automatically called by Mac OS X during next update cycle,
		 * and then blitting will occur. If force_update is true, it will be done right now. */
		[ this->cocoaview setNeedsDisplayInRect:dirtyrect ];
		if (force_update) [ this->cocoaview displayIfNeeded ];
	}

	this->num_dirty_rects = 0;
}

#endif /* WITH_COCOA */
