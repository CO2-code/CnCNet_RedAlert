/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 * SPAWNER.H -- CnCNet Spawner support.                                                        *
 *                                                                                             *
 * The Spawner lets an external launcher (the CnCNet client) start a multiplayer match         *
 * directly from a SPAWN.INI file. This module holds the match settings that are not part of   *
 * the regular game session -- per-slot house overrides, starting locations, spectators and    *
 * the peer table / packet protection used for CnCNet's UDP and tunnel networking.             *
 *                                                                                             *
 * This header is deliberately free of game types so that it can be included from FUNCTION.H.  *
 *---------------------------------------------------------------------------------------------*/

#ifndef SPAWNER_H
#define SPAWNER_H

/*
**	Number of multiplayer house slots (HOUSE_MULTI1..HOUSE_MULTI8) that SPAWN.INI can configure.
*/
#define	SPAWNER_SLOT_COUNT		8

/*
**	Maximum number of remote peers (Other1..Other7 plus a spare).
*/
#define	SPAWNER_MAX_PEERS		8

/*
**	Value used by the per-slot colour / country / handicap tables for "not overridden".
*/
#define	SPAWNER_NOT_SET			0xFF

/*
**	Size of the buffer that holds the map hash string.
*/
#define	SPAWNER_MAP_HASH_SIZE	64

class INIClass;
class HouseClass;


/*
**	Settings that SPAWN.INI supplies in addition to the normal session options.
*/
struct SpawnerSettingsType {

	/*
	**	True once SPAWN.INI has been read for this run. Everything below is only meaningful
	**	when this is set.
	*/
	bool Active;

	/*
	**	True if the match was created by CnCNet's quick match system.
	*/
	bool QuickMatch;

	/*
	**	Overrides for each multiplayer slot, indexed from HOUSE_MULTI1. Colour, country and
	**	handicap use SPAWNER_NOT_SET for "leave alone". A spawn location of -1 means "pick one
	**	automatically", otherwise it is the scenario waypoint number to start at.
	*/
	unsigned char HouseColor[SPAWNER_SLOT_COUNT];
	unsigned char HouseCountry[SPAWNER_SLOT_COUNT];
	unsigned char HouseHandicap[SPAWNER_SLOT_COUNT];
	int SpawnLocation[SPAWNER_SLOT_COUNT];

	/*
	**	Slots that are occupied by an observer rather than a player.
	*/
	bool Spectator[SPAWNER_SLOT_COUNT];

	/*
	**	Player colours whose chat messages should be ignored.
	*/
	bool IgnoredColor[256];

	/*
	**	Hash of the map as supplied by the launcher (reported in the statistics packet).
	*/
	char MapHash[SPAWNER_MAP_HASH_SIZE];

	/*
	**	Match results that the statistics packet reports per multiplayer slot (indexed from
	**	HOUSE_MULTI1): who resigned, who lost their connection and the cell each house started in.
	*/
	bool Resigned[SPAWNER_SLOT_COUNT];
	bool ConnectionLost[SPAWNER_SLOT_COUNT];
	int StartCell[SPAWNER_SLOT_COUNT];

	/*
	**	True if this machine detected that the match went out of sync.
	*/
	bool OutOfSync;

	/*
	**	True once the statistics have been written for this match.
	*/
	bool StatsWritten;

	/*
	**	True if the match was resumed from a save game (no statistics are reported for those).
	*/
	bool LoadedSave;
};

extern SpawnerSettingsType Spawner;


/*
**	Settings.
*/
void Spawner_Reset(void);
void Spawner_Read_Slot_Settings(INIClass const & ini);
void Spawner_Ignore_Color(int color);
bool Spawner_Is_Color_Ignored(int color);

/*
**	Chat. Messages from an ignored player are dropped, apart from a short list of quick call
**	outs ("go", "ally", "base" ...) and unless the sender is out of the game, in which case
**	everything from them is dropped.
*/
bool Spawner_Incoming_Message_Allowed(int color, char const * text);

/*
**	Houses. 'house' parameters are HousesType values.
*/
void Spawner_Apply_House_Overrides(void);
bool Spawner_Is_Spectator(int house);
int Spawner_Spectator_Count(void);
int Spawner_Get_Spawn_Location(int house);
bool Spawner_Skip_Scenario_Object_House(int house);

/*
**	Statistics. The CnCNet ladder reads the statistics packet that the game would normally send
**	to the Westwood servers from STATS.DMP.
*/
bool Spawner_Track_Stats(void);
void Spawner_Note_Resigned(int house);
void Spawner_Note_Connection_Lost(int house);
void Spawner_Note_Out_Of_Sync(void);
void Spawner_Note_Start_Cell(int house, int cell);
int Spawner_Get_Start_Cell(int house);
bool Spawner_Was_Resigned(int house);
bool Spawner_Was_Connection_Lost(int house);
bool Spawner_Write_Stats(void const * data, int length);

/*
**	Networking. The peer table maps a small token (1..SPAWNER_MAX_PEERS) -- which is what the
**	rest of the game stores as the player's address -- to the real IP address and port. This
**	keeps the game's address handling working when several peers share an IP address or when
**	every packet is relayed through a CnCNet tunnel server.
**
**	IP addresses and ports are in network byte order.
*/
void Spawner_Reset_Network(void);
int Spawner_Add_Peer(unsigned long ip, unsigned short port);
void Spawner_Set_Tunnel(unsigned long ip, unsigned short port, unsigned short id);
bool Spawner_Using_Tunnel(void);
void Spawner_Set_Game_Id(unsigned long game_id);
void Spawner_Set_Player_Id(int id);
void Spawner_Generate_Net_Key(char const * scenario_file, unsigned int salt);

/*
**	Packet transport. Both routines work with the IPXAddressClass sized (10 byte) address that
**	the Winsock interface classes pass around, where bytes 4..7 hold the peer token.
**
**	Spawner_Send_Packet returns a negative number if the socket reported an error (the caller
**	can use WSAGetLastError) and zero or more otherwise; packets to unknown peers are dropped.
**
**	Spawner_Receive_Packet returns the length of a valid, decoded packet, zero if the datagram
**	was not for us and should be ignored, or a negative number if the socket reported an error.
*/
#define	SPAWNER_ADDRESS_SIZE	10

int Spawner_Send_Packet(unsigned int sock, unsigned char const * address, void const * buffer, int length);
int Spawner_Receive_Packet(unsigned int sock, void * buffer, int buffer_size, unsigned char * address);

#endif
