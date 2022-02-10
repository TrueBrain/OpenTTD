/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file viewport_kdtree.cpp Implementation for accessing the k-d tree of towns
 */

#include "stdafx.h"
#include "viewport_kdtree.h"

#include "company_func.h"
#include "signs_func.h"
#include "town.h"
#include "viewport_func.h"
#include "waypoint_base.h"
#include "waypoint_func.h"
#include "window_func.h"
#include "zoom_func.h"

#include "safeguards.h"

ViewportSignKdtree _viewport_sign_kdtree(&Kdtree_ViewportSignXYFunc);
static int _viewport_sign_maxwidth = 0;

const Station *_viewport_highlight_station; ///< Currently selected station for coverage area highlight
const Town *_viewport_highlight_town;       ///< Currently selected town for coverage area highlight

ViewportSignKdtreeItem ViewportSignKdtreeItem::MakeStation(StationID id)
{
	ViewportSignKdtreeItem item;
	item.type = VKI_STATION;
	item.id.station = id;

	const Station *st = Station::Get(id);
	assert(st->sign.kdtree_valid);
	item.center = st->sign.center;
	item.top = st->sign.top;

	/* Assume the sign can be a candidate for drawing, so measure its width */
	_viewport_sign_maxwidth = std::max<int>(_viewport_sign_maxwidth, st->sign.width_normal);

	return item;
}

ViewportSignKdtreeItem ViewportSignKdtreeItem::MakeWaypoint(StationID id)
{
	ViewportSignKdtreeItem item;
	item.type = VKI_WAYPOINT;
	item.id.station = id;

	const Waypoint *st = Waypoint::Get(id);
	assert(st->sign.kdtree_valid);
	item.center = st->sign.center;
	item.top = st->sign.top;

	/* Assume the sign can be a candidate for drawing, so measure its width */
	_viewport_sign_maxwidth = std::max<int>(_viewport_sign_maxwidth, st->sign.width_normal);

	return item;
}

ViewportSignKdtreeItem ViewportSignKdtreeItem::MakeTown(TownID id)
{
	ViewportSignKdtreeItem item;
	item.type = VKI_TOWN;
	item.id.town = id;

	const Town *town = Town::Get(id);
	assert(town->cache.sign.kdtree_valid);
	item.center = town->cache.sign.center;
	item.top = town->cache.sign.top;

	/* Assume the sign can be a candidate for drawing, so measure its width */
	_viewport_sign_maxwidth = std::max<int>(_viewport_sign_maxwidth, town->cache.sign.width_normal);

	return item;
}

ViewportSignKdtreeItem ViewportSignKdtreeItem::MakeSign(SignID id)
{
	ViewportSignKdtreeItem item;
	item.type = VKI_SIGN;
	item.id.sign = id;

	const Sign *sign = Sign::Get(id);
	assert(sign->sign.kdtree_valid);
	item.center = sign->sign.center;
	item.top = sign->sign.top;

	/* Assume the sign can be a candidate for drawing, so measure its width */
	_viewport_sign_maxwidth = std::max<int>(_viewport_sign_maxwidth, sign->sign.width_normal);

	return item;
}

void RebuildViewportKdtree()
{
	/* Reset biggest size sign seen */
	_viewport_sign_maxwidth = 0;

	std::vector<ViewportSignKdtreeItem> items;
	items.reserve(BaseStation::GetNumItems() + Town::GetNumItems() + Sign::GetNumItems());

	for (const Station *st : Station::Iterate()) {
		if (st->sign.kdtree_valid) items.push_back(ViewportSignKdtreeItem::MakeStation(st->index));
	}

	for (const Waypoint *wp : Waypoint::Iterate()) {
		if (wp->sign.kdtree_valid) items.push_back(ViewportSignKdtreeItem::MakeWaypoint(wp->index));
	}

	for (const Town *town : Town::Iterate()) {
		if (town->cache.sign.kdtree_valid) items.push_back(ViewportSignKdtreeItem::MakeTown(town->index));
	}

	for (const Sign *sign : Sign::Iterate()) {
		if (sign->sign.kdtree_valid) items.push_back(ViewportSignKdtreeItem::MakeSign(sign->index));
	}

	_viewport_sign_kdtree.Build(items.begin(), items.end());
}

static Rect ExpandRectWithViewportSignMargins(Rect r, ZoomLevel zoom)
{
	/* Pessimistically always use normal font, but also assume small font is never larger in either dimension */
	const int fh = FONT_HEIGHT_NORMAL;
	const int max_tw = _viewport_sign_maxwidth / 2 + 1;
	const int expand_y = ScaleByZoom(VPSM_TOP + fh + VPSM_BOTTOM, zoom);
	const int expand_x = ScaleByZoom(VPSM_LEFT + max_tw + VPSM_RIGHT, zoom);

	r.left -= expand_x;
	r.right += expand_x;
	r.top -= expand_y;
	r.bottom += expand_y;

	return r;
}

void ViewportAddKdtreeSigns(DrawPixelInfo *dpi)
{
	Rect search_rect{ dpi->left, dpi->top, dpi->left + dpi->width, dpi->top + dpi->height };
	search_rect = ExpandRectWithViewportSignMargins(search_rect, dpi->zoom);

	bool show_stations = HasBit(_display_opt, DO_SHOW_STATION_NAMES) && _game_mode != GM_MENU;
	bool show_waypoints = HasBit(_display_opt, DO_SHOW_WAYPOINT_NAMES) && _game_mode != GM_MENU;
	bool show_towns = HasBit(_display_opt, DO_SHOW_TOWN_NAMES) && _game_mode != GM_MENU;
	bool show_signs = HasBit(_display_opt, DO_SHOW_SIGNS) && !IsInvisibilitySet(TO_SIGNS);
	bool show_competitors = HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS);

	const BaseStation *st;
	const Sign *si;

	/* Collect all the items first and draw afterwards, to ensure layering */
	std::vector<const BaseStation *> stations;
	std::vector<const Town *> towns;
	std::vector<const Sign *> signs;

	_viewport_sign_kdtree.FindContained(search_rect.left, search_rect.top, search_rect.right, search_rect.bottom, [&](const ViewportSignKdtreeItem & item) {
		switch (item.type) {
			case ViewportSignKdtreeItem::VKI_STATION:
				if (!show_stations) break;
				st = BaseStation::Get(item.id.station);

				/* Don't draw if station is owned by another company and competitor station names are hidden. Stations owned by none are never ignored. */
				if (!show_competitors && _local_company != st->owner && st->owner != OWNER_NONE) break;

				stations.push_back(st);
				break;

			case ViewportSignKdtreeItem::VKI_WAYPOINT:
				if (!show_waypoints) break;
				st = BaseStation::Get(item.id.station);

				/* Don't draw if station is owned by another company and competitor station names are hidden. Stations owned by none are never ignored. */
				if (!show_competitors && _local_company != st->owner && st->owner != OWNER_NONE) break;

				stations.push_back(st);
				break;

			case ViewportSignKdtreeItem::VKI_TOWN:
				if (!show_towns) break;
				towns.push_back(Town::Get(item.id.town));
				break;

			case ViewportSignKdtreeItem::VKI_SIGN:
				if (!show_signs) break;
				si = Sign::Get(item.id.sign);

				/* Don't draw if sign is owned by another company and competitor signs should be hidden.
				* Note: It is intentional that also signs owned by OWNER_NONE are hidden. Bankrupt
				* companies can leave OWNER_NONE signs after them. */
				if (!show_competitors && _local_company != si->owner && si->owner != OWNER_DEITY) break;

				signs.push_back(si);
				break;

			default:
				NOT_REACHED();
		}
	});

	/* Layering order (bottom to top): Town names, signs, stations */

	for (const auto *t : towns) {
		ViewportAddString(dpi, ZOOM_LVL_OUT_16X, &t->cache.sign,
			_settings_client.gui.population_in_label ? STR_VIEWPORT_TOWN_POP : STR_VIEWPORT_TOWN,
			STR_VIEWPORT_TOWN_TINY_WHITE, STR_VIEWPORT_TOWN_TINY_BLACK,
			t->index, t->cache.population);
	}

	for (const auto *si : signs) {
		ViewportAddString(dpi, ZOOM_LVL_OUT_16X, &si->sign,
			STR_WHITE_SIGN,
			(IsTransparencySet(TO_SIGNS) || si->owner == OWNER_DEITY) ? STR_VIEWPORT_SIGN_SMALL_WHITE : STR_VIEWPORT_SIGN_SMALL_BLACK, STR_NULL,
			si->index, 0, (si->owner == OWNER_NONE) ? COLOUR_GREY : (si->owner == OWNER_DEITY ? INVALID_COLOUR : _company_colours[si->owner]));
	}

	for (const auto *st : stations) {
		if (Station::IsExpected(st)) {
			/* Station */
			ViewportAddString(dpi, ZOOM_LVL_OUT_16X, &st->sign,
				STR_VIEWPORT_STATION, STR_VIEWPORT_STATION_TINY, STR_NULL,
				st->index, st->facilities, (st->owner == OWNER_NONE || !st->IsInUse()) ? COLOUR_GREY : _company_colours[st->owner]);
		} else {
			/* Waypoint */
			ViewportAddString(dpi, ZOOM_LVL_OUT_16X, &st->sign,
				STR_VIEWPORT_WAYPOINT, STR_VIEWPORT_WAYPOINT_TINY, STR_NULL,
				st->index, st->facilities, (st->owner == OWNER_NONE || !st->IsInUse()) ? COLOUR_GREY : _company_colours[st->owner]);
		}
	}
}

/**
 * Test whether a sign is below the mouse
 * @param vp the clicked viewport
 * @param x X position of click
 * @param y Y position of click
 * @param sign the sign to check
 * @return true if the sign was hit
 */
static bool CheckClickOnViewportSign(const Viewport *vp, int x, int y, const ViewportSign *sign)
{
	bool small = (vp->zoom >= ZOOM_LVL_OUT_16X);
	int sign_half_width = ScaleByZoom((small ? sign->width_small : sign->width_normal) / 2, vp->zoom);
	int sign_height = ScaleByZoom(VPSM_TOP + (small ? FONT_HEIGHT_SMALL : FONT_HEIGHT_NORMAL) + VPSM_BOTTOM, vp->zoom);

	return y >= sign->top && y < sign->top + sign_height &&
			x >= sign->center - sign_half_width && x < sign->center + sign_half_width;
}

/**
 * Check whether any viewport sign was clicked, and dispatch the click.
 * @param vp the clicked viewport
 * @param x X position of click
 * @param y Y position of click
 * @return true if the sign was hit
 */
bool CheckClickOnViewportSign(const Viewport *vp, int x, int y)
{
	if (_game_mode == GM_MENU) return false;

	x = ScaleByZoom(x - vp->left, vp->zoom) + vp->virtual_left;
	y = ScaleByZoom(y - vp->top, vp->zoom) + vp->virtual_top;

	Rect search_rect{ x - 1, y - 1, x + 1, y + 1 };
	search_rect = ExpandRectWithViewportSignMargins(search_rect, vp->zoom);

	bool show_stations = HasBit(_display_opt, DO_SHOW_STATION_NAMES) && !IsInvisibilitySet(TO_SIGNS);
	bool show_waypoints = HasBit(_display_opt, DO_SHOW_WAYPOINT_NAMES) && !IsInvisibilitySet(TO_SIGNS);
	bool show_towns = HasBit(_display_opt, DO_SHOW_TOWN_NAMES);
	bool show_signs = HasBit(_display_opt, DO_SHOW_SIGNS) && !IsInvisibilitySet(TO_SIGNS);
	bool show_competitors = HasBit(_display_opt, DO_SHOW_COMPETITOR_SIGNS);

	/* Topmost of each type that was hit */
	BaseStation *st = nullptr, *last_st = nullptr;
	Town *t = nullptr, *last_t = nullptr;
	Sign *si = nullptr, *last_si = nullptr;

	/* See ViewportAddKdtreeSigns() for details on the search logic */
	_viewport_sign_kdtree.FindContained(search_rect.left, search_rect.top, search_rect.right, search_rect.bottom, [&](const ViewportSignKdtreeItem & item) {
		switch (item.type) {
			case ViewportSignKdtreeItem::VKI_STATION:
				if (!show_stations) break;
				st = BaseStation::Get(item.id.station);
				if (!show_competitors && _local_company != st->owner && st->owner != OWNER_NONE) break;
				if (CheckClickOnViewportSign(vp, x, y, &st->sign)) last_st = st;
				break;

			case ViewportSignKdtreeItem::VKI_WAYPOINT:
				if (!show_waypoints) break;
				st = BaseStation::Get(item.id.station);
				if (!show_competitors && _local_company != st->owner && st->owner != OWNER_NONE) break;
				if (CheckClickOnViewportSign(vp, x, y, &st->sign)) last_st = st;
				break;

			case ViewportSignKdtreeItem::VKI_TOWN:
				if (!show_towns) break;
				t = Town::Get(item.id.town);
				if (CheckClickOnViewportSign(vp, x, y, &t->cache.sign)) last_t = t;
				break;

			case ViewportSignKdtreeItem::VKI_SIGN:
				if (!show_signs) break;
				si = Sign::Get(item.id.sign);
				if (!show_competitors && _local_company != si->owner && si->owner != OWNER_DEITY) break;
				if (CheckClickOnViewportSign(vp, x, y, &si->sign)) last_si = si;
				break;

			default:
				NOT_REACHED();
		}
	});

	/* Select which hit to handle based on priority */
	if (last_st != nullptr) {
		if (Station::IsExpected(last_st)) {
			ShowStationViewWindow(last_st->index);
		} else {
			ShowWaypointWindow(Waypoint::From(last_st));
		}
		return true;
	} else if (last_t != nullptr) {
		ShowTownViewWindow(last_t->index);
		return true;
	} else if (last_si != nullptr) {
		HandleClickOnSign(last_si);
		return true;
	} else {
		return false;
	}
}

void MarkCatchmentTilesDirty()
{
	if (_viewport_highlight_town != nullptr) {
		MarkWholeScreenDirty();
		return;
	}
	if (_viewport_highlight_station != nullptr) {
		if (_viewport_highlight_station->catchment_tiles.tile == INVALID_TILE) {
			MarkWholeScreenDirty();
			_viewport_highlight_station = nullptr;
		} else {
			BitmapTileIterator it(_viewport_highlight_station->catchment_tiles);
			for (TileIndex tile = it; tile != INVALID_TILE; tile = ++it) {
				MarkTileDirtyByTile(tile);
			}
		}
	}
}

/**
 * Select or deselect station for coverage area highlight.
 * Selecting a station will deselect a town.
 * @param *st Station in question
 * @param sel Select or deselect given station
 */
void SetViewportCatchmentStation(const Station *st, bool sel)
{
	if (_viewport_highlight_station != nullptr) SetWindowDirty(WC_STATION_VIEW, _viewport_highlight_station->index);
	if (_viewport_highlight_town != nullptr) SetWindowDirty(WC_TOWN_VIEW, _viewport_highlight_town->index);
	if (sel && _viewport_highlight_station != st) {
		MarkCatchmentTilesDirty();
		_viewport_highlight_station = st;
		_viewport_highlight_town = nullptr;
		MarkCatchmentTilesDirty();
	} else if (!sel && _viewport_highlight_station == st) {
		MarkCatchmentTilesDirty();
		_viewport_highlight_station = nullptr;
	}
	if (_viewport_highlight_station != nullptr) SetWindowDirty(WC_STATION_VIEW, _viewport_highlight_station->index);
}

/**
 * Select or deselect town for coverage area highlight.
 * Selecting a town will deselect a station.
 * @param *t Town in question
 * @param sel Select or deselect given town
 */
void SetViewportCatchmentTown(const Town *t, bool sel)
{
	if (_viewport_highlight_town != nullptr) SetWindowDirty(WC_TOWN_VIEW, _viewport_highlight_town->index);
	if (_viewport_highlight_station != nullptr) SetWindowDirty(WC_STATION_VIEW, _viewport_highlight_station->index);
	if (sel && _viewport_highlight_town != t) {
		_viewport_highlight_station = nullptr;
		_viewport_highlight_town = t;
		MarkWholeScreenDirty();
	} else if (!sel && _viewport_highlight_town == t) {
		_viewport_highlight_town = nullptr;
		MarkWholeScreenDirty();
	}
	if (_viewport_highlight_town != nullptr) SetWindowDirty(WC_TOWN_VIEW, _viewport_highlight_town->index);
}

/**
 * Get tile highlight type of coverage area for a given tile.
 * @param t Tile that is being drawn
 * @return Tile highlight type to draw
 */
TileHighlightType GetTileHighlightType(TileIndex t)
{
	if (_viewport_highlight_station != nullptr) {
		if (IsTileType(t, MP_STATION) && GetStationIndex(t) == _viewport_highlight_station->index) return THT_WHITE;
		if (_viewport_highlight_station->TileIsInCatchment(t)) return THT_BLUE;
	}

	if (_viewport_highlight_town != nullptr) {
		if (IsTileType(t, MP_HOUSE)) {
			if (GetTownIndex(t) == _viewport_highlight_town->index) {
				TileHighlightType type = THT_RED;
				for (const Station *st : _viewport_highlight_town->stations_near) {
					if (st->owner != _current_company) continue;
					if (st->TileIsInCatchment(t)) return THT_BLUE;
				}
				return type;
			}
		} else if (IsTileType(t, MP_STATION)) {
			for (const Station *st : _viewport_highlight_town->stations_near) {
				if (st->owner != _current_company) continue;
				if (GetStationIndex(t) == st->index) return THT_WHITE;
			}
		}
	}

	return THT_NONE;
}
