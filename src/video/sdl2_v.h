/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_v.h Base of the SDL2 video driver. */

#ifndef VIDEO_SDL_H
#define VIDEO_SDL_H

#include <condition_variable>

#include "video_driver.hpp"

/** The SDL video driver. */
class VideoDriver_SDL_Base : public VideoDriver {
public:
	VideoDriver_SDL_Base() : sdl_window(nullptr) {}

	const char *Start(const StringList &param) override;

	void Stop() override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	void AcquireBlitterLock() override;

	void ReleaseBlitterLock() override;

	bool ClaimMousePointer() override;

	void EditBoxGainedFocus() override;

	void EditBoxLostFocus() override;

	const char *GetName() const override { return "sdl"; }

protected:
	struct SDL_Window *sdl_window; ///< Main SDL window.
	Palette local_palette; ///< Copy of _cur_palette.
	bool draw_threaded; ///< Whether the drawing is/may be done in a separate thread.
	std::recursive_mutex *draw_mutex = nullptr; ///< Mutex to keep the access to the shared memory controlled.
	std::condition_variable_any *draw_signal = nullptr; ///< Signal to draw the next frame.
	volatile bool draw_continue; ///< Should we keep continue drawing?
	bool buffer_locked; ///< Video buffer was locked by the main thread.

	Dimension GetScreenSize() const override;

	/** Indicate to the driver the client-side might have changed. */
	void ClientSizeChanged(int w, int h, bool force);
	/** Lock video buffer for drawing if it isn't already mapped. */
	bool LockVideoBuffer() override;
	/** Unlock video buffer. */
	void UnlockVideoBuffer() override;

	/** (Re-)create the backing store. */
	virtual bool AllocateBackingStore(int w, int h, bool force = false) = 0;
	/** Get a pointer to the video buffer. */
	virtual void *GetVideoPointer() = 0;
	/** Hand video buffer back to the painting backend. */
	virtual void ReleaseVideoPointer() = 0;
	/** Window got a paint message. */
	virtual void Paint() = 0;
	/** Thread function for threaded drawing. */
	virtual void PaintThread() = 0;
	/** Draw the mouse cursor. */
	virtual void DrawMouseCursor() = 0;
	/** Create the main window. */
	virtual bool CreateMainWindow(uint w, uint h, uint flags = 0);

private:
	int PollEvent();
	void LoopOnce();
	void MainLoopCleanup();
	bool CreateMainSurface(uint w, uint h, bool resize);
	const char *Initialize();
	void CheckPaletteAnim();

	static void PaintThreadThunk(VideoDriver_SDL_Base *drv);

#ifdef __EMSCRIPTEN__
	/* Convert a constant pointer back to a non-constant pointer to a member function. */
	static void EmscriptenLoop(void *self) { ((VideoDriver_SDL_Base *)self)->LoopOnce(); }
#endif

	/**
	 * This is true to indicate that keyboard input is in text input mode, and SDL_TEXTINPUT events are enabled.
	 */
	bool edit_box_focused;

	uint32 cur_ticks;
	uint32 last_cur_ticks;
	uint32 next_tick;

	int startup_display;
	std::thread draw_thread;
	std::unique_lock<std::recursive_mutex> draw_lock;
};

#endif /* VIDEO_SDL_H */
