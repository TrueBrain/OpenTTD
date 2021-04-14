/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file game_info.h Convert NetworkGameInfo to Packet and back.
 */

#ifndef NETWORK_CORE_GAME_INFO_H
#define NETWORK_CORE_GAME_INFO_H

#include "core.h"
#include "config.h"
#include "../../newgrf_config.h"
#include "../../date_type.h"

/**
 * Game Info Protocol v5
 * ---------------------
 *
 *  uint8   Game Info version.
 *  string  Join key of the server
 *  uint8   Number of GRFs attached (n)
 *  For each GRF:
 *    uint32     GRF ID
 *    bytes[16]  MD5 checksum of the GRF
 *
 *  uint32  Current game date in days since 1-1-0 (DMY)
 *  uint32  Game introduction date in days since 1-1-0 (DMY)
 *
 *  uint8   Maximum number of companies allowed on the server
 *  uint8   Number of companies on the server
 *  uint8   Maximum number of clients allowed on the server
 *  uint8   Number of clients on the server
 *  uint8   Maximum number of spectators allowed on the server
 *  uint8   Number of spectators on the server
 *
 *  string  Name of the server
 *  string  Revision of the server
 *  uint8   Whether the server uses a password (0 = no, 1 = yes)
 *  uint8   Whether the server is dedicated (0 = no, 1 = yes)
 *
 *  uint16  Width of the map in tiles
 *  uint16  Height of the map in tiles
 *  uint8   Type of map (0 = temperate, 1 = arctic, 2 = desert, 3 = toyland)
 */

/**
 * The game information that is not generated on-the-fly and has to
 * be sent to the clients.
 */
struct NetworkServerGameInfo {
	char join_key[NETWORK_JOIN_KEY_LENGTH];         ///< Join key
	byte clients_on;                                ///< Current count of clients on server
};

/**
 * The game information that is sent from the server to the clients.
 */
struct NetworkGameInfo : NetworkServerGameInfo {
	GRFConfig *grfconfig;                           ///< List of NewGRF files used
	Date start_date;                                ///< When the game started
	Date game_date;                                 ///< Current date
	uint16 map_width;                               ///< Map width
	uint16 map_height;                              ///< Map height
	char server_name[NETWORK_NAME_LENGTH];          ///< Server name
	char server_revision[NETWORK_REVISION_LENGTH];  ///< The version number the server is using (e.g.: 'r304' or 0.5.0)
	bool dedicated;                                 ///< Is this a dedicated server?
	bool version_compatible;                        ///< Can we connect to this server or not? (based on server_revision)
	bool compatible;                                ///< Can we connect to this server or not? (based on server_revision _and_ grf_match
	bool use_password;                              ///< Is this server passworded?
	byte game_info_version;                         ///< Version of the game info
	byte server_lang;                               ///< Language of the server (we should make a nice table for this)
	byte clients_max;                               ///< Max clients allowed on server
	byte companies_on;                              ///< How many started companies do we have
	byte companies_max;                             ///< Max companies allowed on server
	byte spectators_on;                             ///< How many spectators do we have?
	byte spectators_max;                            ///< Max spectators allowed on server
	byte map_set;                                   ///< Graphical set
};

const char *GetNetworkRevisionString();
bool IsNetworkCompatibleVersion(const char *other);

void FillNetworkGameInfo(NetworkGameInfo &ngi);

void ReceiveGRFIdentifier(Packet *p, GRFIdentifier *grf, char *name, int size);
void SendGRFIdentifier(Packet *p, const GRFIdentifier *grf, const char *name);

void ReceiveNetworkGameInfo(Packet *p, NetworkGameInfo *info);
void SendNetworkGameInfo(Packet *p, const NetworkGameInfo *info);

#endif /* NETWORK_CORE_GAME_INFO_H */
