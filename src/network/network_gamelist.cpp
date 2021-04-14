/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file network_gamelist.cpp This file handles the GameList
 * Also, it handles the request to a server for data about the server
 */

#include "../stdafx.h"
#include "../debug.h"
#include "../window_func.h"
#include "network_internal.h"
#include "network_udp.h"
#include "network_gamelist.h"
#include <atomic>

#include "../safeguards.h"

NetworkGameList *_network_game_list = nullptr;
int _network_game_list_version = 0;

/**
 * Add a new item to the linked gamelist. If the IP and Port match
 * return the existing item instead of adding it again
 * @param address the address of the to-be added item
 * @return a point to the newly added or already existing item
 */
NetworkGameList *NetworkGameListAddItem(ServerAddress address)
{
	if (address.IsDirectAddress()) {
		const char *hostname = address.direct_address.GetHostname();

		/* Do not query the 'any' address. */
		if (StrEmpty(hostname) ||
				strcmp(hostname, "0.0.0.0") == 0 ||
				strcmp(hostname, "::") == 0) {
			return nullptr;
		}
	}

	NetworkGameList *item, *prev_item;

	prev_item = nullptr;
	for (item = _network_game_list; item != nullptr; item = item->next) {
		if (item->address == address) return item;
		prev_item = item;
	}

	item = CallocT<NetworkGameList>(1);
	item->next = nullptr;
	item->address = address;
	item->version = _network_game_list_version;

	if (prev_item == nullptr) {
		_network_game_list = item;
	} else {
		prev_item->next = item;
	}
	DEBUG(net, 4, "[gamelist] added server to list");

	UpdateNetworkGameWindow();

	return item;
}

void CheckGameCompatability(NetworkGameList *item)
{
	item->info.compatible = true;
	{
		/* Checks whether there needs to be a request for names of GRFs and makes
		 * the request if necessary. GRFs that need to be requested are the GRFs
		 * that do not exist on the clients system and we do not have the name
		 * resolved of, i.e. the name is still UNKNOWN_GRF_NAME_PLACEHOLDER.
		 * The in_request array and in_request_count are used so there is no need
		 * to do a second loop over the GRF list, which can be relatively expensive
		 * due to the string comparisons. */
		const GRFConfig *in_request[NETWORK_MAX_GRF_COUNT];
		const GRFConfig *c;
		uint in_request_count = 0;

		for (c = item->info.grfconfig; c != nullptr; c = c->next) {
			if (c->status == GCS_NOT_FOUND) item->info.compatible = false;
			if (c->status != GCS_NOT_FOUND || strcmp(c->GetName(), UNKNOWN_GRF_NAME_PLACEHOLDER) != 0) continue;
			in_request[in_request_count] = c;
			in_request_count++;
		}

		// TODO -- See if we can't do this via GC
		// if (in_request_count > 0) {
		// 	/* There are 'unknown' GRFs, now send a request for them */
		// 	uint i;
		// 	Packet packet(PACKET_UDP_CLIENT_GET_NEWGRFS);

		// 	packet.Send_uint8(in_request_count);
		// 	for (i = 0; i < in_request_count; i++) {
		// 		this->SendGRFIdentifier(&packet, &in_request[i]->ident);
		// 	}

		// 	this->SendPacket(&packet, &item->address.direct_address);
		// }
	}

	/* Check if we are allowed on this server based on the revision-match */
	item->info.version_compatible = IsNetworkCompatibleVersion(item->info.server_revision);
	item->info.compatible &= item->info.version_compatible; // Already contains match for GRFs

	item->online = true;
}

/**
 * Remove an item from the gamelist linked list
 * @param remove pointer to the item to be removed
 */
void NetworkGameListRemoveItem(NetworkGameList *remove)
{
	NetworkGameList *prev_item = nullptr;
	for (NetworkGameList *item = _network_game_list; item != nullptr; item = item->next) {
		if (remove == item) {
			if (prev_item == nullptr) {
				_network_game_list = remove->next;
			} else {
				prev_item->next = remove->next;
			}

			/* Remove GRFConfig information */
			ClearGRFConfigList(&remove->info.grfconfig);
			free(remove);
			remove = nullptr;

			DEBUG(net, 4, "[gamelist] removed server from list");
			NetworkRebuildHostList();
			UpdateNetworkGameWindow();
			return;
		}
		prev_item = item;
	}
}

static const uint MAX_GAME_LIST_REQUERY_COUNT  = 10; ///< How often do we requery in number of times per server?
static const uint REQUERY_EVERY_X_GAMELOOPS    = 60; ///< How often do we requery in time?
static const uint REFRESH_GAMEINFO_X_REQUERIES = 50; ///< Refresh the game info itself after REFRESH_GAMEINFO_X_REQUERIES * REQUERY_EVERY_X_GAMELOOPS game loops

/**
 * Rebuild the GRFConfig's of the servers in the game list as we did
 * a rescan and might have found new NewGRFs.
 */
void NetworkAfterNewGRFScan()
{
	for (NetworkGameList *item = _network_game_list; item != nullptr; item = item->next) {
		/* Reset compatibility state */
		item->info.compatible = item->info.version_compatible;

		for (GRFConfig *c = item->info.grfconfig; c != nullptr; c = c->next) {
			assert(HasBit(c->flags, GCF_COPY));

			const GRFConfig *f = FindGRFConfig(c->ident.grfid, FGCM_EXACT, c->ident.md5sum);
			if (f == nullptr) {
				/* Don't know the GRF, so mark game incompatible and the (possibly)
				 * already resolved name for this GRF (another server has sent the
				 * name of the GRF already. */
				c->name = FindUnknownGRFName(c->ident.grfid, c->ident.md5sum, true);
				c->status = GCS_NOT_FOUND;

				/* If we miss a file, we're obviously incompatible. */
				item->info.compatible = false;
			} else {
				c->filename = f->filename;
				c->name = f->name;
				c->info = f->info;
				c->status = GCS_UNKNOWN;
			}
		}
	}

	InvalidateWindowClassesData(WC_NETWORK_WINDOW);
}

void NetworkGameListRemoveExpired()
{
	NetworkGameList *prev_item = nullptr;
	for (NetworkGameList *item = _network_game_list; item != nullptr;) {
		if (!item->manually && item->version < _network_game_list_version) {
			NetworkGameList *remove = item;
			item = item->next;

			if (prev_item == nullptr) {
				_network_game_list = item;
			} else {
				prev_item->next = item;
			}

			/* Remove GRFConfig information */
			ClearGRFConfigList(&remove->info.grfconfig);
			free(remove);
		} else {
			prev_item = item;
			item = item->next;
		}
	}

	NetworkRebuildHostList();
	UpdateNetworkGameWindow();
}
