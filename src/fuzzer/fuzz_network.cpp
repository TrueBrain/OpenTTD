/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fuzz_network.cpp Functions related to starting OpenTTD. */

#include "../stdafx.h"

#include "../blitter/factory.hpp"
#include "../sound/sound_driver.hpp"
#include "../music/music_driver.hpp"
#include "../video/video_driver.hpp"

#include "../fontcache.h"
#include "../error.h"
#include "../gui.h"

#include "../base_media_base.h"
#include "../saveload/saveload.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../news_func.h"
#include "../fios.h"
#include "../aircraft.h"
#include "../roadveh.h"
#include "../train.h"
#include "../ship.h"
#include "../console_func.h"
#include "../screenshot.h"
#include "../network/network.h"
#include "../network/network_func.h"
#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../settings_func.h"
#include "../genworld.h"
#include "../progress.h"
#include "../strings_func.h"
#include "../date_func.h"
#include "../vehicle_func.h"
#include "../gamelog.h"
#include "../animated_tile_func.h"
#include "../roadstop_base.h"
#include "../elrail_func.h"
#include "../rev.h"
#include "../highscore.h"
#include "../station_base.h"
#include "../crashlog.h"
#include "../engine_func.h"
#include "../core/random_func.hpp"
#include "../rail_gui.h"
#include "../road_gui.h"
#include "../core/backup_type.hpp"
#include "../hotkeys.h"
#include "../newgrf.h"
#include "../misc/getoptdata.h"
#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../town.h"
#include "../subsidy_func.h"
#include "../gfx_layout.h"
#include "../viewport_func.h"
#include "../viewport_sprite_sorter.h"
#include "../framerate_type.h"
#include "../industry.h"
#include "../network/network_gui.h"
#include "../misc_cmd.h"

#include "../linkgraph/linkgraphschedule.h"

#include <stdarg.h>
#include <system_error>

#include "../safeguards.h"

extern void LoadIntroGame(bool load_newgrfs = true);
extern void TestCommand(const unsigned char *buf, size_t len);
extern void CheckCaches();

void fuzz_network(const uint8_t *buf, size_t len)
{
	const uint8 *cbuf = buf;
	while (len > 6) {
		size_t size = *cbuf++;
		if (size == 0) {
			break;
		}

		if (size > len - 1) {
			size = len - 1;
		}

		TestCommand(cbuf, size);

		cbuf += size;
		len -= size + 1;
	}
	::GameLoop();
	CheckCaches();
}

bool DoInitialization()
{
	char *args[] = {"./openttd", "-vnull"};
	openttd_main(2, args);
	::GameLoop();
	return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len) {
	static bool Initialized = DoInitialization();

	LoadIntroGame(false);
    fuzz_network(buf, len);
    return 0;
}
