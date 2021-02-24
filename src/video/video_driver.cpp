/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video_driver.cpp Common code between video driver implementations. */

#include "../stdafx.h"
#include "../debug.h"
#include "../gfx_func.h"
#include "../progress.h"
#include "../thread.h"
#include "../window_func.h"
#include "video_driver.hpp"

void VideoDriver::GameLoop()
{
	this->next_game_tick += this->GetGameInterval();

	/* Avoid next_game_tick getting behind more and more if it cannot keep up. */
	auto now = std::chrono::steady_clock::now();
	if (this->next_game_tick < now - ALLOWED_DRIFT * this->GetGameInterval()) this->next_game_tick = now;

	{
		std::lock_guard<std::mutex> lock(this->game_state_mutex);

		::GameLoop();
	}
}

void VideoDriver::GameThread()
{
	while (!_exit_game) {
		this->GameLoop();

		auto now = std::chrono::steady_clock::now();
		if (this->next_game_tick > now) {
			std::this_thread::sleep_for(this->next_game_tick - now);
		} else {
			/* Either GameLoop() takes longer than GameInterval, or we are
			 * fast-forwarding. We have to check if the draw-thread wants to
			 * gain access to the game-state, and give him a chance to get the
			 * lock, as otherwise we would be keeping the lock constantly.
			 * Without the sleep_for(), we would normally not be yielding.
			 * So, we check if the draw-thread wants to have the game-state
			 * lock, and if so give it a chance to get it, by waiting for the
			 * draw-thread to inform us it got the lock.
			 */
			if (this->request_game_state_mutex) {
				std::mutex wait_mutex;
				std::lock_guard<std::mutex> wait_lock(wait_mutex);
				this->game_state_locked_signal.wait(wait_mutex);
			}
		}
	}
}

/* static */ void VideoDriver::GameThreadThunk(VideoDriver *drv)
{
	drv->GameThread();
}

void VideoDriver::StartGameThread()
{
	if (this->is_game_threaded) {
		this->is_game_threaded = StartNewThread(nullptr, "ottd:game", &VideoDriver::GameThreadThunk, this);
	}

	DEBUG(driver, 1, "using %sthread for game-loop", this->is_game_threaded ? "" : "no ");
}

void VideoDriver::Tick()
{
	auto now = std::chrono::steady_clock::now();

	/* If more than a millisecond has passed, increase the _realtime_tick. */
	if (now - this->last_realtime_tick >= std::chrono::milliseconds(1)) {
		auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_realtime_tick);
		_realtime_tick += delta.count();
		this->last_realtime_tick += delta;
	}

	if (!this->is_game_threaded && now >= this->next_game_tick) {
		this->GameLoop();

		/* For things like dedicated server, don't run a separate draw-tick. */
		if (!this->HasGUI()) {
			::InputLoop();
			::UpdateWindows();
			this->next_draw_tick = this->next_game_tick;
		}
	}

	if (this->HasGUI() && now >= this->next_draw_tick) {
		this->next_draw_tick += this->GetDrawInterval();
		/* Avoid next_draw_tick getting behind more and more if it cannot keep up. */
		if (this->next_draw_tick < now - ALLOWED_DRIFT * this->GetDrawInterval()) this->next_draw_tick = now;

		this->InputLoop();

		{
			/* Inform the game-thread that we want the game-state, get the
			 * lock on the game-state, and inform the game-thread that we
			 * got the lock. This avoids the game-thread to never yield.
			 * See comment in the game-thread for more details. */
			this->request_game_state_mutex = true;
			std::lock_guard<std::mutex> lock(this->game_state_mutex);
			this->game_state_locked_signal.notify_one();
			this->request_game_state_mutex = false;

			this->LockVideoBuffer();

			while (this->PollEvent()) {}
			::InputLoop();

			/* Prevent drawing when switching mode, as windows can be removed when they should still appear. */
			if (_switch_mode == SM_NONE || HasModalProgress()) {
				::UpdateWindows();
			}
		}

		this->CheckPaletteAnim();
		this->Paint();

		this->UnlockVideoBuffer();
	}
}

void VideoDriver::SleepTillNextTick()
{
	auto next_tick = this->next_draw_tick;
	auto now = std::chrono::steady_clock::now();

	if (!this->is_game_threaded) {
		next_tick = min(next_tick, this->next_game_tick);
	}

	if (next_tick > now) {
		std::this_thread::sleep_for(next_tick - now);
	}
}
