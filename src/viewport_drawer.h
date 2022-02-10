/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_drawer.h Functions related to drawing on viewports. */

#ifndef VIEWPORT_DRAWER_H
#define VIEWPORT_DRAWER_H

#include <vector>

#include "gfx_type.h"
#include "strings_func.h"
#include "tile_cmd.h"
#include "viewport_kdtree.h"
#include "viewport_sprite_sorter.h"

struct StringSpriteToDraw {
	StringID string;
	Colours colour;
	int32 x;
	int32 y;
	uint64 params[2];
	uint16 width;
};

struct TileSpriteToDraw {
	SpriteID image;
	PaletteID pal;
	const SubSprite *sub;           ///< only draw a rectangular part of the sprite
	int32 x;                        ///< screen X coordinate of sprite
	int32 y;                        ///< screen Y coordinate of sprite
};

struct ChildScreenSpriteToDraw {
	SpriteID image;
	PaletteID pal;
	const SubSprite *sub;           ///< only draw a rectangular part of the sprite
	int32 x;
	int32 y;
	int next;                       ///< next child to draw (-1 at the end)
};

/** Enumeration of multi-part foundations */
enum FoundationPart {
	FOUNDATION_PART_NONE     = 0xFF,  ///< Neither foundation nor groundsprite drawn yet.
	FOUNDATION_PART_NORMAL   = 0,     ///< First part (normal foundation or no foundation)
	FOUNDATION_PART_HALFTILE = 1,     ///< Second part (halftile foundation)
	FOUNDATION_PART_END
};

/**
 * Mode of "sprite combining"
 * @see StartSpriteCombine
 */
enum SpriteCombineMode {
	SPRITE_COMBINE_NONE,     ///< Every #AddSortableSpriteToDraw start its own bounding box
	SPRITE_COMBINE_PENDING,  ///< %Sprite combining will start with the next unclipped sprite.
	SPRITE_COMBINE_ACTIVE,   ///< %Sprite combining is active. #AddSortableSpriteToDraw outputs child sprites.
};

typedef std::vector<TileSpriteToDraw> TileSpriteToDrawVector;
typedef std::vector<StringSpriteToDraw> StringSpriteToDrawVector;
typedef std::vector<ParentSpriteToDraw> ParentSpriteToDrawVector;
typedef std::vector<ChildScreenSpriteToDraw> ChildScreenSpriteToDrawVector;

/** Data structure storing rendering information */
class ViewportDrawer {
	DrawPixelInfo dpi;

	StringSpriteToDrawVector string_sprites_to_draw;
	TileSpriteToDrawVector tile_sprites_to_draw;
	ParentSpriteToDrawVector parent_sprites_to_draw;
	ChildScreenSpriteToDrawVector child_screen_sprites_to_draw;

	int *last_child;
	TileInfo *cur_ti;
	const Viewport *vp;

	int foundation[FOUNDATION_PART_END];             ///< Foundation sprites (index into parent_sprites_to_draw).
	FoundationPart foundation_part;                  ///< Currently active foundation for ground sprite drawing.
	int *last_foundation_child[FOUNDATION_PART_END]; ///< Tail of ChildSprite list of the foundations. (index into child_screen_sprites_to_draw)
	Point foundation_offset[FOUNDATION_PART_END];    ///< Pixel offset for ground sprites on the foundations.

	void AddTileSpriteToDraw(SpriteID image, PaletteID pal, int32 x, int32 y, int z, const SubSprite *sub = nullptr, int extra_offs_x = 0, int extra_offs_y = 0);
	void AddChildSpriteToFoundation(SpriteID image, PaletteID pal, const SubSprite *sub, FoundationPart foundation_part, int extra_offs_x, int extra_offs_y);
	void AddCombinedSprite(SpriteID image, PaletteID pal, int x, int y, int z, const SubSprite *sub);
	void DrawSelectionSprite(SpriteID image, PaletteID pal, const TileInfo *ti, int z_offset, FoundationPart foundation_part);
	void DrawTileSelectionRect(const TileInfo *ti, PaletteID pal);
	void DrawAutorailSelection(const TileInfo *ti, uint autorail_type);
	void DrawTileHighlightType(const TileInfo *ti, TileHighlightType tht);
	void ViewportAddLandscape();
	void HighlightTownLocalAuthorityTiles(const TileInfo *ti);
	void DrawTileSelection(const TileInfo *ti);
    void ViewportDrawTileSprites(const TileSpriteToDrawVector *tstdv);
    void ViewportDrawParentSprites(const ParentSpriteToSortVector *psd, const ChildScreenSpriteToDrawVector *csstdv);
    void ViewportDrawBoundingBoxes(const ParentSpriteToSortVector *psd);
    void ViewportDrawDirtyBlocks();
    void ViewportDrawStrings(ZoomLevel zoom, const StringSpriteToDrawVector *sstdv);

public:
	SpriteCombineMode combine_sprites;               ///< Current mode of "sprite combining". @see StartSpriteCombine

	ViewportDrawer(const struct Viewport *vp) : vp(vp) {};

	void AddStringToDraw(int x, int y, StringID string, uint64 params_1, uint64 params_2, Colours colour, uint16 width);
	void DrawGroundSpriteAt(SpriteID image, PaletteID pal, int32 x, int32 y, int z, const SubSprite *sub, int extra_offs_x, int extra_offs_y);
	void OffsetGroundSprite(int x, int y);
	void AddSortableSpriteToDraw(SpriteID image, PaletteID pal, int x, int y, int w, int h, int dz, int z, bool transparent, int bb_offset_x, int bb_offset_y, int bb_offset_z, const SubSprite *sub);
	void AddChildSpriteScreen(SpriteID image, PaletteID pal, int x, int y, bool transparent, const SubSprite *sub, bool scale);
	void ViewportDoDraw(int left, int top, int right, int bottom);
	void ViewportDoBlitter();
};

#endif /* VIEWPORT_DRAWER_H */
