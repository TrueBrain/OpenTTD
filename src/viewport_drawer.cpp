/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file viewport_drawer.cpp Drawing a viewport.
 */

#include "stdafx.h"
#include "blitter/factory.hpp"
#include "bridge_map.h"
#include "landscape.h"
#include "linkgraph/linkgraph_gui.h"
#include "tilehighlight_func.h"
#include "town_kdtree.h"
#include "vehicle_base.h"
#include "vehicle_func.h"
#include "viewport_drawer.h"
#include "viewport_func.h"
#include "viewport_kdtree.h"
#include "viewport_sprite_sorter.h"
#include "zoom_func.h"

#include "table/autorail.h"
#include "table/string_colours.h"

#include "safeguards.h"

bool _draw_bounding_boxes = false;
bool _draw_dirty_blocks = false;
uint _dirty_block_colour = 0;

static const int MAX_TILE_EXTENT_TOP    = ZOOM_LVL_BASE * MAX_BUILDING_PIXELS;             ///< Maximum top    extent of tile relative to north corner (not considering bridges).
static const int MAX_TILE_EXTENT_BOTTOM = ZOOM_LVL_BASE * (TILE_PIXELS + 2 * TILE_HEIGHT); ///< Maximum bottom extent of tile relative to north corner (worst case: #SLOPE_STEEP_N).

static VpSpriteSorter _vp_sprite_sorter = nullptr;

/* [direction][side] */
static const HighLightStyle _autorail_type[6][2] = {
	{ HT_DIR_X,  HT_DIR_X },
	{ HT_DIR_Y,  HT_DIR_Y },
	{ HT_DIR_HU, HT_DIR_HL },
	{ HT_DIR_HL, HT_DIR_HU },
	{ HT_DIR_VL, HT_DIR_VR },
	{ HT_DIR_VR, HT_DIR_VL }
};

/** Helper class for getting the best sprite sorter. */
struct ViewportSSCSS {
	VpSorterChecker fct_checker; ///< The check function.
	VpSpriteSorter fct_sorter;   ///< The sorting function.
};

/** List of sorters ordered from best to worst. */
static ViewportSSCSS _vp_sprite_sorters[] = {
#ifdef WITH_SSE
	{ &ViewportSortParentSpritesSSE41Checker, &ViewportSortParentSpritesSSE41 },
#endif
	{ &ViewportSortParentSpritesChecker, &ViewportSortParentSprites }
};


/** Choose the "best" sprite sorter and set _vp_sprite_sorter. */
void InitializeSpriteSorter()
{
	for (uint i = 0; i < lengthof(_vp_sprite_sorters); i++) {
		if (_vp_sprite_sorters[i].fct_checker()) {
			_vp_sprite_sorter = _vp_sprite_sorters[i].fct_sorter;
			break;
		}
	}
	assert(_vp_sprite_sorter != nullptr);
}

/**
 * Check if the parameter "check" is inside the interval between
 * begin and end, including both begin and end.
 * @note Whether \c begin or \c end is the biggest does not matter.
 *       This method will account for that.
 * @param begin The begin of the interval.
 * @param end   The end of the interval.
 * @param check The value to check.
 */
static bool IsInRangeInclusive(int begin, int end, int check)
{
	if (begin > end) Swap(begin, end);
	return begin <= check && check <= end;
}

/**
 * Checks whether a point is inside the selected a diagonal rectangle given by _thd.size and _thd.pos
 * @param x The x coordinate of the point to be checked.
 * @param y The y coordinate of the point to be checked.
 * @return True if the point is inside the rectangle, else false.
 */
static bool IsInsideRotatedRectangle(int x, int y)
{
	int dist_a = (_thd.size.x + _thd.size.y);      // Rotated coordinate system for selected rectangle.
	int dist_b = (_thd.size.x - _thd.size.y);      // We don't have to divide by 2. It's all relative!
	int a = ((x - _thd.pos.x) + (y - _thd.pos.y)); // Rotated coordinate system for the point under scrutiny.
	int b = ((x - _thd.pos.x) - (y - _thd.pos.y));

	/* Check if a and b are between 0 and dist_a or dist_b respectively. */
	return IsInRangeInclusive(dist_a, 0, a) && IsInRangeInclusive(dist_b, 0, b);
}

/**
 * Returns the y coordinate in the viewport coordinate system where the given
 * tile is painted.
 * @param tile Any tile.
 * @return The viewport y coordinate where the tile is painted.
 */
static int GetViewportY(Point tile)
{
	/* Each increment in X or Y direction moves down by half a tile, i.e. TILE_PIXELS / 2. */
	return (tile.y * (int)(TILE_PIXELS / 2) + tile.x * (int)(TILE_PIXELS / 2) - TilePixelHeightOutsideMap(tile.x, tile.y)) << ZOOM_LVL_SHIFT;
}

static bool IsPartOfAutoLine(int px, int py)
{
	px -= _thd.selstart.x;
	py -= _thd.selstart.y;

	if ((_thd.drawstyle & HT_DRAG_MASK) != HT_LINE) return false;

	switch (_thd.drawstyle & HT_DIR_MASK) {
		case HT_DIR_X:  return py == 0; // x direction
		case HT_DIR_Y:  return px == 0; // y direction
		case HT_DIR_HU: return px == -py || px == -py - 16; // horizontal upper
		case HT_DIR_HL: return px == -py || px == -py + 16; // horizontal lower
		case HT_DIR_VL: return px == py || px == py + 16; // vertical left
		case HT_DIR_VR: return px == py || px == py - 16; // vertical right
		default:
			NOT_REACHED();
	}
}

/**
 * Schedules a tile sprite for drawing.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x position x (world coordinates) of the sprite.
 * @param y position y (world coordinates) of the sprite.
 * @param z position z (world coordinates) of the sprite.
 * @param sub Only draw a part of the sprite.
 * @param extra_offs_x Pixel X offset for the sprite position.
 * @param extra_offs_y Pixel Y offset for the sprite position.
 */
void ViewportDrawer::AddTileSpriteToDraw(SpriteID image, PaletteID pal, int32 x, int32 y, int z, const SubSprite *sub, int extra_offs_x, int extra_offs_y)
{
	assert((image & SPRITE_MASK) < MAX_SPRITES);

	TileSpriteToDraw &ts = this->tile_sprites_to_draw.emplace_back();
	ts.image = image;
	ts.pal = pal;
	ts.sub = sub;
	Point pt = RemapCoords(x, y, z);
	ts.x = pt.x + extra_offs_x;
	ts.y = pt.y + extra_offs_y;
}

/**
 * Adds a child sprite to the active foundation.
 *
 * The pixel offset of the sprite relative to the ParentSprite is the sum of the offset passed to OffsetGroundSprite() and extra_offs_?.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param sub Only draw a part of the sprite.
 * @param foundation_part Foundation part.
 * @param extra_offs_x Pixel X offset for the sprite position.
 * @param extra_offs_y Pixel Y offset for the sprite position.
 */
void ViewportDrawer::AddChildSpriteToFoundation(SpriteID image, PaletteID pal, const SubSprite *sub, FoundationPart foundation_part, int extra_offs_x, int extra_offs_y)
{
	assert(IsInsideMM(foundation_part, 0, FOUNDATION_PART_END));
	assert(this->foundation[foundation_part] != -1);
	Point offs = this->foundation_offset[foundation_part];

	/* Change the active ChildSprite list to the one of the foundation */
	int *old_child = this->last_child;
	this->last_child = this->last_foundation_child[foundation_part];

	this->AddChildSpriteScreen(image, pal, offs.x + extra_offs_x, offs.y + extra_offs_y, false, sub, false);

	/* Switch back to last ChildSprite list */
	this->last_child = old_child;
}

/**
 * Draws a ground sprite at a specific world-coordinate relative to the current tile.
 * If the current tile is drawn on top of a foundation the sprite is added as child sprite to the "foundation"-ParentSprite.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x position x (world coordinates) of the sprite relative to current tile.
 * @param y position y (world coordinates) of the sprite relative to current tile.
 * @param z position z (world coordinates) of the sprite relative to current tile.
 * @param sub Only draw a part of the sprite.
 * @param extra_offs_x Pixel X offset for the sprite position.
 * @param extra_offs_y Pixel Y offset for the sprite position.
 */
void ViewportDrawer::DrawGroundSpriteAt(SpriteID image, PaletteID pal, int32 x, int32 y, int z, const SubSprite *sub, int extra_offs_x, int extra_offs_y)
{
	/* Switch to first foundation part, if no foundation was drawn */
	if (this->foundation_part == FOUNDATION_PART_NONE) this->foundation_part = FOUNDATION_PART_NORMAL;

	if (this->foundation[this->foundation_part] != -1) {
		Point pt = RemapCoords(x, y, z);
		this->AddChildSpriteToFoundation(image, pal, sub, this->foundation_part, pt.x + extra_offs_x * ZOOM_LVL_BASE, pt.y + extra_offs_y * ZOOM_LVL_BASE);
	} else {
		this->AddTileSpriteToDraw(image, pal, this->cur_ti->x + x, this->cur_ti->y + y, this->cur_ti->z + z, sub, extra_offs_x * ZOOM_LVL_BASE, extra_offs_y * ZOOM_LVL_BASE);
	}
}

/**
 * Called when a foundation has been drawn for the current tile.
 * Successive ground sprites for the current tile will be drawn as child sprites of the "foundation"-ParentSprite, not as TileSprites.
 *
 * @param x sprite x-offset (screen coordinates) of ground sprites relative to the "foundation"-ParentSprite.
 * @param y sprite y-offset (screen coordinates) of ground sprites relative to the "foundation"-ParentSprite.
 */
void ViewportDrawer::OffsetGroundSprite(int x, int y)
{
	/* Switch to next foundation part */
	switch (this->foundation_part) {
		case FOUNDATION_PART_NONE:
			this->foundation_part = FOUNDATION_PART_NORMAL;
			break;
		case FOUNDATION_PART_NORMAL:
			this->foundation_part = FOUNDATION_PART_HALFTILE;
			break;
		default: NOT_REACHED();
	}

	/* this->last_child == nullptr if foundation sprite was clipped by the viewport bounds */
	if (this->last_child != nullptr) this->foundation[this->foundation_part] = (uint)this->parent_sprites_to_draw.size() - 1;

	this->foundation_offset[this->foundation_part].x = x * ZOOM_LVL_BASE;
	this->foundation_offset[this->foundation_part].y = y * ZOOM_LVL_BASE;
	this->last_foundation_child[this->foundation_part] = this->last_child;
}

/**
 * Adds a child sprite to a parent sprite.
 * In contrast to "AddChildSpriteScreen()" the sprite position is in world coordinates
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x position x of the sprite.
 * @param y position y of the sprite.
 * @param z position z of the sprite.
 * @param sub Only draw a part of the sprite.
 */
void ViewportDrawer::AddCombinedSprite(SpriteID image, PaletteID pal, int x, int y, int z, const SubSprite *sub)
{
	Point pt = RemapCoords(x, y, z);
	const Sprite *spr = GetSprite(image & SPRITE_MASK, ST_NORMAL);

	if (pt.x + spr->x_offs >= this->dpi.left + this->dpi.width ||
			pt.x + spr->x_offs + spr->width <= this->dpi.left ||
			pt.y + spr->y_offs >= this->dpi.top + this->dpi.height ||
			pt.y + spr->y_offs + spr->height <= this->dpi.top)
		return;

	const ParentSpriteToDraw &pstd = this->parent_sprites_to_draw.back();
	this->AddChildSpriteScreen(image, pal, pt.x - pstd.left, pt.y - pstd.top, false, sub, false);
}

/**
 * Draw a (transparent) sprite at given coordinates with a given bounding box.
 * The bounding box extends from (x + bb_offset_x, y + bb_offset_y, z + bb_offset_z) to (x + w - 1, y + h - 1, z + dz - 1), both corners included.
 * Bounding boxes with bb_offset_x == w or bb_offset_y == h or bb_offset_z == dz are allowed and produce thin slices.
 *
 * @note Bounding boxes are normally specified with bb_offset_x = bb_offset_y = bb_offset_z = 0. The extent of the bounding box in negative direction is
 *       defined by the sprite offset in the grf file.
 *       However if modifying the sprite offsets is not suitable (e.g. when using existing graphics), the bounding box can be tuned by bb_offset.
 *
 * @pre w >= bb_offset_x, h >= bb_offset_y, dz >= bb_offset_z. Else w, h or dz are ignored.
 *
 * @param image the image to combine and draw,
 * @param pal the provided palette,
 * @param x position X (world) of the sprite,
 * @param y position Y (world) of the sprite,
 * @param w bounding box extent towards positive X (world),
 * @param h bounding box extent towards positive Y (world),
 * @param dz bounding box extent towards positive Z (world),
 * @param z position Z (world) of the sprite,
 * @param transparent if true, switch the palette between the provided palette and the transparent palette,
 * @param bb_offset_x bounding box extent towards negative X (world),
 * @param bb_offset_y bounding box extent towards negative Y (world),
 * @param bb_offset_z bounding box extent towards negative Z (world)
 * @param sub Only draw a part of the sprite.
 */
void ViewportDrawer::AddSortableSpriteToDraw(SpriteID image, PaletteID pal, int x, int y, int w, int h, int dz, int z, bool transparent, int bb_offset_x, int bb_offset_y, int bb_offset_z, const SubSprite *sub)
{
	int32 left, right, top, bottom;

	assert((image & SPRITE_MASK) < MAX_SPRITES);

	/* make the sprites transparent with the right palette */
	if (transparent) {
		SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	}

	if (this->combine_sprites == SPRITE_COMBINE_ACTIVE) {
		this->AddCombinedSprite(image, pal, x, y, z, sub);
		return;
	}

	this->last_child = nullptr;

	Point pt = RemapCoords(x, y, z);
	int tmp_left, tmp_top, tmp_x = pt.x, tmp_y = pt.y;

	/* Compute screen extents of sprite */
	if (image == SPR_EMPTY_BOUNDING_BOX) {
		left = tmp_left = RemapCoords(x + w          , y + bb_offset_y, z + bb_offset_z).x;
		right           = RemapCoords(x + bb_offset_x, y + h          , z + bb_offset_z).x + 1;
		top  = tmp_top  = RemapCoords(x + bb_offset_x, y + bb_offset_y, z + dz         ).y;
		bottom          = RemapCoords(x + w          , y + h          , z + bb_offset_z).y + 1;
	} else {
		const Sprite *spr = GetSprite(image & SPRITE_MASK, ST_NORMAL);
		left = tmp_left = (pt.x += spr->x_offs);
		right           = (pt.x +  spr->width );
		top  = tmp_top  = (pt.y += spr->y_offs);
		bottom          = (pt.y +  spr->height);
	}

	if (_draw_bounding_boxes && (image != SPR_EMPTY_BOUNDING_BOX)) {
		/* Compute maximal extents of sprite and its bounding box */
		left   = std::min(left  , RemapCoords(x + w          , y + bb_offset_y, z + bb_offset_z).x);
		right  = std::max(right , RemapCoords(x + bb_offset_x, y + h          , z + bb_offset_z).x + 1);
		top    = std::min(top   , RemapCoords(x + bb_offset_x, y + bb_offset_y, z + dz         ).y);
		bottom = std::max(bottom, RemapCoords(x + w          , y + h          , z + bb_offset_z).y + 1);
	}

	/* Do not add the sprite to the viewport, if it is outside */
	if (left   >= this->dpi.left + this->dpi.width ||
	    right  <= this->dpi.left                 ||
	    top    >= this->dpi.top + this->dpi.height ||
	    bottom <= this->dpi.top) {
		return;
	}

	ParentSpriteToDraw &ps = this->parent_sprites_to_draw.emplace_back();
	ps.x = tmp_x;
	ps.y = tmp_y;

	ps.left = tmp_left;
	ps.top  = tmp_top;

	ps.image = image;
	ps.pal = pal;
	ps.sub = sub;
	ps.xmin = x + bb_offset_x;
	ps.xmax = x + std::max(bb_offset_x, w) - 1;

	ps.ymin = y + bb_offset_y;
	ps.ymax = y + std::max(bb_offset_y, h) - 1;

	ps.zmin = z + bb_offset_z;
	ps.zmax = z + std::max(bb_offset_z, dz) - 1;

	ps.first_child = -1;

	this->last_child = &ps.first_child;

	if (this->combine_sprites == SPRITE_COMBINE_PENDING) this->combine_sprites = SPRITE_COMBINE_ACTIVE;
}

/**
 * Add a child sprite to a parent sprite.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param x sprite x-offset (screen coordinates) relative to parent sprite.
 * @param y sprite y-offset (screen coordinates) relative to parent sprite.
 * @param transparent if true, switch the palette between the provided palette and the transparent palette,
 * @param sub Only draw a part of the sprite.
 */
void ViewportDrawer::AddChildSpriteScreen(SpriteID image, PaletteID pal, int x, int y, bool transparent, const SubSprite *sub, bool scale)
{
	assert((image & SPRITE_MASK) < MAX_SPRITES);

	/* If the ParentSprite was clipped by the viewport bounds, do not draw the ChildSprites either */
	if (this->last_child == nullptr) return;

	/* make the sprites transparent with the right palette */
	if (transparent) {
		SetBit(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	}

	*this->last_child = (uint)this->child_screen_sprites_to_draw.size();

	ChildScreenSpriteToDraw &cs = this->child_screen_sprites_to_draw.emplace_back();
	cs.image = image;
	cs.pal = pal;
	cs.sub = sub;
	cs.x = scale ? x * ZOOM_LVL_BASE : x;
	cs.y = scale ? y * ZOOM_LVL_BASE : y;
	cs.next = -1;

	/* Append the sprite to the active ChildSprite list.
	 * If the active ParentSprite is a foundation, update last_foundation_child as well.
	 * Note: ChildSprites of foundations are NOT sequential in the vector, as selection sprites are added at last. */
	if (this->last_foundation_child[0] == this->last_child) this->last_foundation_child[0] = &cs.next;
	if (this->last_foundation_child[1] == this->last_child) this->last_foundation_child[1] = &cs.next;
	this->last_child = &cs.next;
}

void ViewportDrawer::AddStringToDraw(int x, int y, StringID string, uint64 params_1, uint64 params_2, Colours colour, uint16 width)
{
	assert(width != 0);
	StringSpriteToDraw &ss = this->string_sprites_to_draw.emplace_back();
	ss.string = string;
	ss.x = x;
	ss.y = y;
	ss.params[0] = params_1;
	ss.params[1] = params_2;
	ss.width = width;
	ss.colour = colour;
}

/**
 * Draws sprites between ground sprite and everything above.
 *
 * The sprite is either drawn as TileSprite or as ChildSprite of the active foundation.
 *
 * @param image the image to draw.
 * @param pal the provided palette.
 * @param ti TileInfo Tile that is being drawn
 * @param z_offset Z offset relative to the groundsprite. Only used for the sprite position, not for sprite sorting.
 * @param foundation_part Foundation part the sprite belongs to.
 */
void ViewportDrawer::DrawSelectionSprite(SpriteID image, PaletteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part)
{
	/* FIXME: This is not totally valid for some autorail highlights that extend over the edges of the tile. */
	if (this->foundation[foundation_part] == -1) {
		/* draw on real ground */
		this->AddTileSpriteToDraw(image, pal, ti->x, ti->y, ti->z + z_offset);
	} else {
		/* draw on top of foundation */
		this->AddChildSpriteToFoundation(image, pal, nullptr, foundation_part, 0, -z_offset * ZOOM_LVL_BASE);
	}
}

/**
 * Draws a selection rectangle on a tile.
 *
 * @param ti TileInfo Tile that is being drawn
 * @param pal Palette to apply.
 */
void ViewportDrawer::DrawTileSelectionRect(const TileInfo *ti, PaletteID pal)
{
	if (!IsValidTile(ti->tile)) return;

	SpriteID sel;
	if (IsHalftileSlope(ti->tileh)) {
		Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
		SpriteID sel2 = SPR_HALFTILE_SELECTION_FLAT + halftile_corner;
		this->DrawSelectionSprite(sel2, pal, ti, 7 + TILE_HEIGHT, FOUNDATION_PART_HALFTILE);

		Corner opposite_corner = OppositeCorner(halftile_corner);
		if (IsSteepSlope(ti->tileh)) {
			sel = SPR_HALFTILE_SELECTION_DOWN;
		} else {
			sel = ((ti->tileh & SlopeWithOneCornerRaised(opposite_corner)) != 0 ? SPR_HALFTILE_SELECTION_UP : SPR_HALFTILE_SELECTION_FLAT);
		}
		sel += opposite_corner;
	} else {
		sel = SPR_SELECT_TILE + SlopeToSpriteOffset(ti->tileh);
	}
	this->DrawSelectionSprite(sel, pal, ti, 7, FOUNDATION_PART_NORMAL);
}

/**
 * Draws autorail highlights.
 *
 * @param *ti TileInfo Tile that is being drawn
 * @param autorail_type Offset into _AutorailTilehSprite[][]
 */
void ViewportDrawer::DrawAutorailSelection(const TileInfo *ti, uint autorail_type)
{
	SpriteID image;
	PaletteID pal;
	int offset;

	FoundationPart foundation_part = FOUNDATION_PART_NORMAL;
	Slope autorail_tileh = RemoveHalftileSlope(ti->tileh);
	if (IsHalftileSlope(ti->tileh)) {
		static const uint _lower_rail[4] = { 5U, 2U, 4U, 3U };
		Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
		if (autorail_type != _lower_rail[halftile_corner]) {
			foundation_part = FOUNDATION_PART_HALFTILE;
			/* Here we draw the highlights of the "three-corners-raised"-slope. That looks ok to me. */
			autorail_tileh = SlopeWithThreeCornersRaised(OppositeCorner(halftile_corner));
		}
	}

	offset = _AutorailTilehSprite[autorail_tileh][autorail_type];
	if (offset >= 0) {
		image = SPR_AUTORAIL_BASE + offset;
		pal = PAL_NONE;
	} else {
		image = SPR_AUTORAIL_BASE - offset;
		pal = PALETTE_SEL_TILE_RED;
	}

	this->DrawSelectionSprite(image, _thd.make_square_red ? PALETTE_SEL_TILE_RED : pal, ti, 7, foundation_part);
}

/**
 * Draw tile highlight for coverage area highlight.
 * @param *ti TileInfo Tile that is being drawn
 * @param tht Highlight type to draw.
 */
void ViewportDrawer::DrawTileHighlightType(const TileInfo *ti, TileHighlightType tht)
{
	switch (tht) {
		default:
		case THT_NONE: break;
		case THT_WHITE: this->DrawTileSelectionRect(ti, PAL_NONE); break;
		case THT_BLUE:  this->DrawTileSelectionRect(ti, PALETTE_SEL_TILE_BLUE); break;
		case THT_RED:   this->DrawTileSelectionRect(ti, PALETTE_SEL_TILE_RED); break;
	}
}

/**
 * Highlights tiles insede local authority of selected towns.
 * @param *ti TileInfo Tile that is being drawn
 */
void ViewportDrawer::HighlightTownLocalAuthorityTiles(const TileInfo *ti)
{
	/* Going through cases in order of computational time. */

	if (_town_local_authority_kdtree.Count() == 0) return;

	/* Tile belongs to town regardless of distance from town. */
	if (GetTileType(ti->tile) == MP_HOUSE) {
		if (!Town::GetByTile(ti->tile)->show_zone) return;

		this->DrawTileSelectionRect(ti, PALETTE_CRASH);
		return;
	}

	/* If the closest town in the highlighted list is far, we can stop searching. */
	TownID tid = _town_local_authority_kdtree.FindNearest(TileX(ti->tile), TileY(ti->tile));
	Town *closest_highlighted_town = Town::Get(tid);

	if (DistanceManhattan(ti->tile, closest_highlighted_town->xy) >= _settings_game.economy.dist_local_authority) return;

	/* Tile is inside of the local autrhority distance of a highlighted town,
	   but it is possible that a non-highlighted town is even closer. */
	Town *closest_town = ClosestTownFromTile(ti->tile, _settings_game.economy.dist_local_authority);

	if (closest_town->show_zone) {
		this->DrawTileSelectionRect(ti, PALETTE_CRASH);
	}
}

/**
 * Checks if the specified tile is selected and if so draws selection using correct selectionstyle.
 * @param *ti TileInfo Tile that is being drawn
 */
void ViewportDrawer::DrawTileSelection(const TileInfo *ti)
{
	/* Highlight tiles insede local authority of selected towns. */
	this->HighlightTownLocalAuthorityTiles(ti);

	/* Draw a red error square? */
	bool is_redsq = _thd.redsq == ti->tile;
	if (is_redsq) this->DrawTileSelectionRect(ti, PALETTE_TILE_RED_PULSATING);

	TileHighlightType tht = GetTileHighlightType(ti->tile);
	this->DrawTileHighlightType(ti, tht);

	/* No tile selection active? */
	if ((_thd.drawstyle & HT_DRAG_MASK) == HT_NONE) return;

	if (_thd.diagonal) { // We're drawing a 45 degrees rotated (diagonal) rectangle
		if (IsInsideRotatedRectangle((int)ti->x, (int)ti->y)) goto draw_inner;
		return;
	}

	/* Inside the inner area? */
	if (IsInsideBS(ti->x, _thd.pos.x, _thd.size.x) &&
			IsInsideBS(ti->y, _thd.pos.y, _thd.size.y)) {
draw_inner:
		if (_thd.drawstyle & HT_RECT) {
			if (!is_redsq) this->DrawTileSelectionRect(ti, _thd.make_square_red ? PALETTE_SEL_TILE_RED : PAL_NONE);
		} else if (_thd.drawstyle & HT_POINT) {
			/* Figure out the Z coordinate for the single dot. */
			int z = 0;
			FoundationPart foundation_part = FOUNDATION_PART_NORMAL;
			if (ti->tileh & SLOPE_N) {
				z += TILE_HEIGHT;
				if (RemoveHalftileSlope(ti->tileh) == SLOPE_STEEP_N) z += TILE_HEIGHT;
			}
			if (IsHalftileSlope(ti->tileh)) {
				Corner halftile_corner = GetHalftileSlopeCorner(ti->tileh);
				if ((halftile_corner == CORNER_W) || (halftile_corner == CORNER_E)) z += TILE_HEIGHT;
				if (halftile_corner != CORNER_S) {
					foundation_part = FOUNDATION_PART_HALFTILE;
					if (IsSteepSlope(ti->tileh)) z -= TILE_HEIGHT;
				}
			}
			this->DrawSelectionSprite(_cur_dpi->zoom <= ZOOM_LVL_DETAIL ? SPR_DOT : SPR_DOT_SMALL, PAL_NONE, ti, z, foundation_part);
		} else if (_thd.drawstyle & HT_RAIL) {
			/* autorail highlight piece under cursor */
			HighLightStyle type = _thd.drawstyle & HT_DIR_MASK;
			assert(type < HT_DIR_END);
			this->DrawAutorailSelection(ti, _autorail_type[type][0]);
		} else if (IsPartOfAutoLine(ti->x, ti->y)) {
			/* autorail highlighting long line */
			HighLightStyle dir = _thd.drawstyle & HT_DIR_MASK;
			uint side;

			if (dir == HT_DIR_X || dir == HT_DIR_Y) {
				side = 0;
			} else {
				TileIndex start = TileVirtXY(_thd.selstart.x, _thd.selstart.y);
				side = Delta(Delta(TileX(start), TileX(ti->tile)), Delta(TileY(start), TileY(ti->tile)));
			}

			this->DrawAutorailSelection(ti, _autorail_type[dir][side]);
		}
		return;
	}

	/* Check if it's inside the outer area? */
	if (!is_redsq && (tht == THT_NONE || tht == THT_RED) && _thd.outersize.x > 0 &&
			IsInsideBS(ti->x, _thd.pos.x + _thd.offs.x, _thd.size.x + _thd.outersize.x) &&
			IsInsideBS(ti->y, _thd.pos.y + _thd.offs.y, _thd.size.y + _thd.outersize.y)) {
		/* Draw a blue rect. */
		this->DrawTileSelectionRect(ti, PALETTE_SEL_TILE_BLUE);
		return;
	}
}

/**
 * Add the landscape to the viewport, i.e. all ground tiles and buildings.
 */
void ViewportDrawer::ViewportAddLandscape()
{
	assert(this->dpi.top <= this->dpi.top + this->dpi.height);
	assert(this->dpi.left <= this->dpi.left + this->dpi.width);

	Point upper_left = InverseRemapCoords(this->dpi.left, this->dpi.top);
	Point upper_right = InverseRemapCoords(this->dpi.left + this->dpi.width, this->dpi.top);

	/* Transformations between tile coordinates and viewport rows/columns: See vp_column_row
	 *   column = y - x
	 *   row    = x + y
	 *   x      = (row - column) / 2
	 *   y      = (row + column) / 2
	 * Note: (row, columns) pairs are only valid, if they are both even or both odd.
	 */

	/* Columns overlap with neighbouring columns by a half tile.
	 *  - Left column is column of upper_left (rounded down) and one column to the left.
	 *  - Right column is column of upper_right (rounded up) and one column to the right.
	 * Note: Integer-division does not round down for negative numbers, so ensure rounding with another increment/decrement.
	 */
	int left_column = (upper_left.y - upper_left.x) / (int)TILE_SIZE - 2;
	int right_column = (upper_right.y - upper_right.x) / (int)TILE_SIZE + 2;

	int potential_bridge_height = ZOOM_LVL_BASE * TILE_HEIGHT * _settings_game.construction.max_bridge_height;

	/* Rows overlap with neighbouring rows by a half tile.
	 * The first row that could possibly be visible is the row above upper_left (if it is at height 0).
	 * Due to integer-division not rounding down for negative numbers, we need another decrement.
	 */
	int row = (upper_left.x + upper_left.y) / (int)TILE_SIZE - 2;
	bool last_row = false;
	for (; !last_row; row++) {
		last_row = true;
		for (int column = left_column; column <= right_column; column++) {
			/* Valid row/column? */
			if ((row + column) % 2 != 0) continue;

			Point tilecoord;
			tilecoord.x = (row - column) / 2;
			tilecoord.y = (row + column) / 2;
			assert(column == tilecoord.y - tilecoord.x);
			assert(row == tilecoord.y + tilecoord.x);

			TileType tile_type;
			TileInfo tile_info;
			this->cur_ti = &tile_info;
			tile_info.x = tilecoord.x * TILE_SIZE; // FIXME tile_info should use signed integers
			tile_info.y = tilecoord.y * TILE_SIZE;

			if (IsInsideBS(tilecoord.x, 0, MapSizeX()) && IsInsideBS(tilecoord.y, 0, MapSizeY())) {
				/* This includes the south border at MapMaxX / MapMaxY. When terraforming we still draw tile selections there. */
				tile_info.tile = TileXY(tilecoord.x, tilecoord.y);
				tile_type = GetTileType(tile_info.tile);
			} else {
				tile_info.tile = INVALID_TILE;
				tile_type = MP_VOID;
			}

			if (tile_type != MP_VOID) {
				/* We are inside the map => paint landscape. */
				tile_info.tileh = GetTilePixelSlope(tile_info.tile, &tile_info.z);
			} else {
				/* We are outside the map => paint black. */
				tile_info.tileh = GetTilePixelSlopeOutsideMap(tilecoord.x, tilecoord.y, &tile_info.z);
			}

			int viewport_y = GetViewportY(tilecoord);

			if (viewport_y + MAX_TILE_EXTENT_BOTTOM < this->dpi.top) {
				/* The tile in this column is not visible yet.
				 * Tiles in other columns may be visible, but we need more rows in any case. */
				last_row = false;
				continue;
			}

			int min_visible_height = viewport_y - (this->dpi.top + this->dpi.height);
			bool tile_visible = min_visible_height <= 0;

			if (tile_type != MP_VOID) {
				/* Is tile with buildings visible? */
				if (min_visible_height < MAX_TILE_EXTENT_TOP) tile_visible = true;

				if (IsBridgeAbove(tile_info.tile)) {
					/* Is the bridge visible? */
					TileIndex bridge_tile = GetNorthernBridgeEnd(tile_info.tile);
					int bridge_height = ZOOM_LVL_BASE * (GetBridgePixelHeight(bridge_tile) - TilePixelHeight(tile_info.tile));
					if (min_visible_height < bridge_height + MAX_TILE_EXTENT_TOP) tile_visible = true;
				}

				/* Would a higher bridge on a more southern tile be visible?
				 * If yes, we need to loop over more rows to possibly find one. */
				if (min_visible_height < potential_bridge_height + MAX_TILE_EXTENT_TOP) last_row = false;
			} else {
				/* Outside of map. If we are on the north border of the map, there may still be a bridge visible,
				 * so we need to loop over more rows to possibly find one. */
				if ((tilecoord.x <= 0 || tilecoord.y <= 0) && min_visible_height < potential_bridge_height + MAX_TILE_EXTENT_TOP) last_row = false;
			}

			if (tile_visible) {
				last_row = false;
				this->foundation_part = FOUNDATION_PART_NONE;
				this->foundation[0] = -1;
				this->foundation[1] = -1;
				this->last_foundation_child[0] = nullptr;
				this->last_foundation_child[1] = nullptr;

				_tile_type_procs[tile_type]->draw_tile_proc(&tile_info);
				if (tile_info.tile != INVALID_TILE) this->DrawTileSelection(&tile_info);
			}
		}
	}
}

void ViewportDrawer::ViewportDrawTileSprites(const TileSpriteToDrawVector *tstdv)
{
	for (const TileSpriteToDraw &ts : *tstdv) {
		DrawSpriteViewport(ts.image, ts.pal, ts.x, ts.y, ts.sub);
	}
}

void ViewportDrawer::ViewportDrawParentSprites(const ParentSpriteToSortVector *psd, const ChildScreenSpriteToDrawVector *csstdv)
{
	for (const ParentSpriteToDraw *ps : *psd) {
		if (ps->image != SPR_EMPTY_BOUNDING_BOX) DrawSpriteViewport(ps->image, ps->pal, ps->x, ps->y, ps->sub);

		int child_idx = ps->first_child;
		while (child_idx >= 0) {
			const ChildScreenSpriteToDraw *cs = csstdv->data() + child_idx;
			child_idx = cs->next;
			DrawSpriteViewport(cs->image, cs->pal, ps->left + cs->x, ps->top + cs->y, cs->sub);
		}
	}
}

/**
 * Draws the bounding boxes of all ParentSprites
 * @param psd Array of ParentSprites
 */
void ViewportDrawer::ViewportDrawBoundingBoxes(const ParentSpriteToSortVector *psd)
{
	for (const ParentSpriteToDraw *ps : *psd) {
		Point pt1 = RemapCoords(ps->xmax + 1, ps->ymax + 1, ps->zmax + 1); // top front corner
		Point pt2 = RemapCoords(ps->xmin    , ps->ymax + 1, ps->zmax + 1); // top left corner
		Point pt3 = RemapCoords(ps->xmax + 1, ps->ymin    , ps->zmax + 1); // top right corner
		Point pt4 = RemapCoords(ps->xmax + 1, ps->ymax + 1, ps->zmin    ); // bottom front corner

		DrawBox(        pt1.x,         pt1.y,
		        pt2.x - pt1.x, pt2.y - pt1.y,
		        pt3.x - pt1.x, pt3.y - pt1.y,
		        pt4.x - pt1.x, pt4.y - pt1.y);
	}
}

/**
 * Draw/colour the blocks that have been redrawn.
 */
void ViewportDrawer::ViewportDrawDirtyBlocks()
{
	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	const DrawPixelInfo *dpi = _cur_dpi;
	void *dst;
	int right =  UnScaleByZoom(dpi->width,  dpi->zoom);
	int bottom = UnScaleByZoom(dpi->height, dpi->zoom);

	int colour = _string_colourmap[_dirty_block_colour & 0xF];

	dst = dpi->dst_ptr;

	byte bo = UnScaleByZoom(dpi->left + dpi->top, dpi->zoom) & 1;
	do {
		for (int i = (bo ^= 1); i < right; i += 2) blitter->SetPixel(dst, i, 0, (uint8)colour);
		dst = blitter->MoveTo(dst, 0, 1);
	} while (--bottom > 0);
}

void ViewportDrawer::ViewportDrawStrings(ZoomLevel zoom, const StringSpriteToDrawVector *sstdv)
{
	for (const StringSpriteToDraw &ss : *sstdv) {
		TextColour colour = TC_BLACK;
		bool small = HasBit(ss.width, 15);
		int w = GB(ss.width, 0, 15);
		int x = UnScaleByZoom(ss.x, zoom);
		int y = UnScaleByZoom(ss.y, zoom);
		int h = VPSM_TOP + (small ? FONT_HEIGHT_SMALL : FONT_HEIGHT_NORMAL) + VPSM_BOTTOM;

		SetDParam(0, ss.params[0]);
		SetDParam(1, ss.params[1]);

		if (ss.colour != INVALID_COLOUR) {
			/* Do not draw signs nor station names if they are set invisible */
			if (IsInvisibilitySet(TO_SIGNS) && ss.string != STR_WHITE_SIGN) continue;

			if (IsTransparencySet(TO_SIGNS) && ss.string != STR_WHITE_SIGN) {
				/* Don't draw the rectangle.
				 * Real colours need the TC_IS_PALETTE_COLOUR flag.
				 * Otherwise colours from _string_colourmap are assumed. */
				colour = (TextColour)_colour_gradient[ss.colour][6] | TC_IS_PALETTE_COLOUR;
			} else {
				/* Draw the rectangle if 'transparent station signs' is off,
				 * or if we are drawing a general text sign (STR_WHITE_SIGN). */
				DrawFrameRect(
					x, y, x + w, y + h, ss.colour,
					IsTransparencySet(TO_SIGNS) ? FR_TRANSPARENT : FR_NONE
				);
			}
		}

		DrawString(x + VPSM_LEFT, x + w - 1 - VPSM_RIGHT, y + VPSM_TOP, ss.string, colour, SA_HOR_CENTER);
	}
}

void ViewportDrawer::ViewportDoDraw(const Viewport *vp, int left, int top, int right, int bottom)
{
	DrawPixelInfo *old_dpi = _cur_dpi;
	_cur_dpi = &this->dpi;

	this->dpi.zoom = vp->zoom;
	int mask = ScaleByZoom(-1, vp->zoom);

	this->combine_sprites = SPRITE_COMBINE_NONE;

	this->dpi.width = (right - left) & mask;
	this->dpi.height = (bottom - top) & mask;
	this->dpi.left = left & mask;
	this->dpi.top = top & mask;
	this->dpi.pitch = old_dpi->pitch;
	this->last_child = nullptr;

	int x = UnScaleByZoom(this->dpi.left - (vp->virtual_left & mask), vp->zoom) + vp->left;
	int y = UnScaleByZoom(this->dpi.top - (vp->virtual_top & mask), vp->zoom) + vp->top;

	this->dpi.dst_ptr = BlitterFactory::GetCurrentBlitter()->MoveTo(old_dpi->dst_ptr, x - old_dpi->left, y - old_dpi->top);

	this->ViewportAddLandscape();
	ViewportAddVehicles(&this->dpi);

	ViewportAddKdtreeSigns(&this->dpi);

	DrawTextEffects(&this->dpi);

	if (this->tile_sprites_to_draw.size() != 0) this->ViewportDrawTileSprites(&this->tile_sprites_to_draw);

	for (auto &psd : this->parent_sprites_to_draw) {
		this->parent_sprites_to_sort.push_back(&psd);
	}

	_vp_sprite_sorter(&this->parent_sprites_to_sort);
	this->ViewportDrawParentSprites(&this->parent_sprites_to_sort, &this->child_screen_sprites_to_draw);

	if (_draw_bounding_boxes) this->ViewportDrawBoundingBoxes(&this->parent_sprites_to_sort);
	if (_draw_dirty_blocks) ViewportDrawDirtyBlocks();

	DrawPixelInfo dp = this->dpi;
	ZoomLevel zoom = this->dpi.zoom;
	dp.zoom = ZOOM_LVL_NORMAL;
	dp.width = UnScaleByZoom(dp.width, zoom);
	dp.height = UnScaleByZoom(dp.height, zoom);
	_cur_dpi = &dp;

	if (vp->overlay != nullptr && vp->overlay->GetCargoMask() != 0 && vp->overlay->GetCompanyMask() != 0) {
		/* translate to window coordinates */
		dp.left = x;
		dp.top = y;
		vp->overlay->Draw(&dp);
	}

	if (this->string_sprites_to_draw.size() != 0) {
		/* translate to world coordinates */
		dp.left = UnScaleByZoom(this->dpi.left, zoom);
		dp.top = UnScaleByZoom(this->dpi.top, zoom);
		this->ViewportDrawStrings(zoom, &this->string_sprites_to_draw);
	}

	_cur_dpi = old_dpi;

	this->string_sprites_to_draw.clear();
	this->tile_sprites_to_draw.clear();
	this->parent_sprites_to_draw.clear();
	this->parent_sprites_to_sort.clear();
	this->child_screen_sprites_to_draw.clear();
}
