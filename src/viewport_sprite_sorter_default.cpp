/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file viewport_sprite_sorter_default.cpp Sprite sorter that is always available. */

#include "stdafx.h"
#include "viewport_sprite_sorter.h"
#include <forward_list>
#include <stack>

#include "safeguards.h"


/** This fallback sprite checker always exists. */
bool ViewportSortParentSpritesChecker()
{
	return true;
}

/** Sort parent sprites pointer array replicating the way original sorter did it. */
void ViewportSortParentSprites(ParentSpriteToSortVector *psdv)
{
	if (psdv->size() < 2) return;

	/* We rely on sprites being, for the most part, already ordered.
	 * So we don't need to move many of them and can keep track of their
	 * order efficiently by using stack. We always move sprites to the front
	 * of the current position, i.e. to the top of the stack.
	 * Also use special constants to indicate sorting state without
	 * adding extra fields to ParentSpriteToDraw structure.
	 */
	const uint32 ORDER_COMPARED = UINT32_MAX; // Sprite was compared but we still need to compare the ones preceding it
	const uint32 ORDER_RETURNED = UINT32_MAX - 1; // Makr sorted sprite in case there are other occurrences of it in the stack
	std::stack<ParentSpriteToDraw *> sprite_order;
	uint32 next_order = 0;

	std::forward_list<std::pair<int64, ParentSpriteToDraw *>> sprite_list;  // We store sprites in a list sorted by xmin+ymin

	/* Initialize sprite list and order. */
	for (auto p = psdv->rbegin(); p != psdv->rend(); p++) {
		sprite_list.push_front(std::make_pair((*p)->xmin + (*p)->ymin, *p));
		sprite_order.push(*p);
		(*p)->order = next_order++;
	}

	sprite_list.sort();

	std::vector<ParentSpriteToDraw *> preceding;  // Temporarily stores sprites that precede current and their position in the list
	auto preceding_prev = sprite_list.begin(); // Store iterator in case we need to delete a single preciding sprite
	auto out = psdv->begin();  // Iterator to output sorted sprites

	while (!sprite_order.empty()) {

		auto s = sprite_order.top();
		sprite_order.pop();

		/* Sprite is already sorted, ignore it. */
		if (s->order == ORDER_RETURNED) continue;

		/* Sprite was already compared, just need to output it. */
		if (s->order == ORDER_COMPARED) {
			*(out++) = s;
			s->order = ORDER_RETURNED;
			continue;
		}

		preceding.clear();

		/* We only need sprites with xmin <= s->xmax && ymin <= s->ymax && zmin <= s->zmax
		 * So by iterating sprites with xmin + ymin <= s->xmax + s->ymax
		 * we get all we need and some more that we filter out later.
		 * We don't include zmin into the sum as there are usually more neighbors on x and y than z
		 * so including it will actually increase the amount of false positives.
		 * Also min coordinates can be > max so using max(xmin, xmax) + max(ymin, ymax)
		 * to ensure that we iterate the current sprite as we need to remove it from the list.
		 */
		auto ssum = std::max(s->xmax, s->xmin) + std::max(s->ymax, s->ymin);
		auto prev = sprite_list.before_begin();
		auto x = sprite_list.begin();
		while (x != sprite_list.end() && ((*x).first <= ssum)) {
			auto p = (*x).second;
			if (p == s) {
				/* We found the current sprite, remove it and move on. */
				x = sprite_list.erase_after(prev);
				continue;
			}

			auto p_prev = prev;
			prev = x++;

			if (s->xmax < p->xmin || s->ymax < p->ymin || s->zmax < p->zmin) continue;
			if (s->xmin <= p->xmax && // overlap in X?
					s->ymin <= p->ymax && // overlap in Y?
					s->zmin <= p->zmax) { // overlap in Z?
				if (s->xmin + s->xmax + s->ymin + s->ymax + s->zmin + s->zmax <=
						p->xmin + p->xmax + p->ymin + p->ymax + p->zmin + p->zmax) {
					continue;
				}
			}
			preceding.push_back(p);
			preceding_prev = p_prev;
		}

		if (preceding.empty()) {
			/* No preceding sprites, add current one to the output */
			*(out++) = s;
			s->order = ORDER_RETURNED;
			continue;
		}

		/* Optimization for the case when we only have 1 sprite to move. */
		if (preceding.size() == 1) {
			auto p = preceding[0];
			/* We can only output the preceding sprite if there can't be any other sprites preceding it. */
			if (p->xmax <= s->xmax && p->ymax <= s->ymax && p->zmax <= s->zmax) {
				p->order = ORDER_RETURNED;
				s->order = ORDER_RETURNED;
				sprite_list.erase_after(preceding_prev);
				*(out++) = p;
				*(out++) = s;
				continue;
			}
		}

		/* Sort all preceding sprites by order and assign new orders in reverse (as original sorter did). */
		std::sort(preceding.begin(), preceding.end(), [](const ParentSpriteToDraw *a, const ParentSpriteToDraw *b) {
			return a->order > b->order;
		});

		s->order = ORDER_COMPARED;
		sprite_order.push(s);  // Still need to output so push it back for now

		for (auto p: preceding) {
			p->order = next_order++;
			sprite_order.push(p);
		}
	}
}
