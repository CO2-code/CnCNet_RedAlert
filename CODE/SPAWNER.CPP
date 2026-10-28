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
 * SPAWNER.CPP -- CnCNet Spawner support.                                                      *
 *                                                                                             *
 * This is the source level counterpart of the Spawner binary patch (spawn.asm, the            *
 * house/spawn-location/spectator patches and nethack.c/netkey.c). The reading of the match    *
 * options themselves lives in INIT.CPP; this file holds the state that the options feed and  *
 * the code that acts on it.                                                                   *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Spawner_Reset -- Clears all spawner state.                                                *
 *   Spawner_Read_Slot_Settings -- Reads the per-slot sections of SPAWN.INI.                   *
 *   Spawner_Ignore_Color -- Marks a player colour as ignored for chat.                        *
 *   Spawner_Is_Color_Ignored -- Is chat from this colour ignored?                             *
 *   Spawner_Apply_House_Overrides -- Applies colours/countries/handicaps/spectators.          *
 *   Spawner_Is_Spectator -- Is this multiplayer house an observer?                            *
 *   Spawner_Spectator_Count -- Number of observers in the match.                              *
 *   Spawner_Get_Spawn_Location -- The waypoint a house has been told to start at.             *
 *   Spawner_Skip_Scenario_Object_House -- Should scenario objects of this house be skipped?   *
 *   Spawner_Reset_Network -- Clears the peer table and keys.                                  *
 *   Spawner_Add_Peer -- Registers a remote player.                                            *
 *   Spawner_Set_Tunnel -- Configures the tunnel server.                                       *
 *   Spawner_Using_Tunnel -- Is traffic routed through a tunnel?                               *
 *   Spawner_Set_Game_Id -- Sets the game id and derived packet header.                        *
 *   Spawner_Set_Player_Id -- Sets the id stamped on outgoing packets.                         *
 *   Spawner_Generate_Net_Key -- Builds the key that protects match traffic.                   *
 *   Spawner_Send_Packet -- Protects and sends a packet.                                       *
 *   Spawner_Receive_Packet -- Receives, verifies and decodes a packet.                        *
 *=============================================================================================*/

#include	"function.h"
#include	"diaglog.h"
#include	"spawner.h"
#include	"spawnsec.h"

#ifdef WINSOCK_IPX
#include	"WSProto.h"
#endif


/*
**	The global match settings.
*/
SpawnerSettingsType Spawner;

static char const * const SlotNames[SPAWNER_SLOT_COUNT] = {
	"Multi1", "Multi2", "Multi3", "Multi4", "Multi5", "Multi6", "Multi7", "Multi8"
};

static int SpectatorCount = 0;


/*
**	Packet layout, matching the original Spawner:
**
**		bytes 0..3		Tunnel mode: our tunnel id (2 bytes) then the destination's tunnel id
**							(2 bytes), both as stored in network order.
**							Direct mode: the game id derived packet header, little endian.
**		bytes 4..7		CRC-32 of the unprotected payload, seeded with the packet header.
**		byte  8			The sender's player id.
**		bytes 9..		The payload XORed with the repeating net key.
*/
#define	PACKET_HEADER_SIZE	9
#define	PACKET_MAX_PAYLOAD	1024

#ifdef WINSOCK_IPX
struct SpawnerPeerType {
	unsigned long Ip;				// Network order.
	unsigned short Port;			// Network order.
};

static SpawnerPeerType Peers[SPAWNER_MAX_PEERS];
static int PeerCount = 0;

static unsigned long TunnelIp = 0;
static unsigned short TunnelPort = 0;
static unsigned short TunnelId = 0;
static unsigned int PacketHeader = 0;
static unsigned char PlayerId = 0;
static unsigned char NetKey[SPAWNSEC_NET_KEY_SIZE];
#endif


/***********************************************************************************************
 * Spawner_Reset -- Clears all spawner state.                                                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Reset(void)
{
	memset(&Spawner, 0, sizeof(Spawner));
	for (int slot = 0; slot < SPAWNER_SLOT_COUNT; slot++) {
		Spawner.StartCell[slot] = -1;
		Spawner.HouseColor[slot] = SPAWNER_NOT_SET;
		Spawner.HouseCountry[slot] = SPAWNER_NOT_SET;
		Spawner.HouseHandicap[slot] = SPAWNER_NOT_SET;
		Spawner.SpawnLocation[slot] = -1;
	}
	SpectatorCount = 0;
	Spawner_Reset_Network();
}


/***********************************************************************************************
 * Spawner_Read_Slot_Settings -- Reads the per-slot sections of SPAWN.INI.                     *
 *                                                                                             *
 * INPUT:   ini   -- The loaded SPAWN.INI.                                                     *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Values that are out of range are ignored (left as "not set") instead of being   *
 *             used as table indices.                                                          *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Read_Slot_Settings(INIClass const & ini)
{
	for (int slot = 0; slot < SPAWNER_SLOT_COUNT; slot++) {
		char const * name = SlotNames[slot];

		int color = ini.Get_Int("HouseColours", name, SPAWNER_NOT_SET);
		if (color >= 0 && color < MAX_MPLAYER_COLORS) {
			Spawner.HouseColor[slot] = (unsigned char)color;
		} else if (color != SPAWNER_NOT_SET) {
			RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: ignoring invalid HouseColours %s=%d", name, color);
		}

		int country = ini.Get_Int("HouseCountries", name, SPAWNER_NOT_SET);
		if (country >= 0 && country < HOUSE_COUNT) {
			Spawner.HouseCountry[slot] = (unsigned char)country;
		} else if (country != SPAWNER_NOT_SET) {
			RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: ignoring invalid HouseCountries %s=%d", name, country);
		}

		int handicap = ini.Get_Int("HouseHandicaps", name, SPAWNER_NOT_SET);
		if (handicap >= 0 && handicap < DIFF_COUNT) {
			Spawner.HouseHandicap[slot] = (unsigned char)handicap;
		} else if (handicap != SPAWNER_NOT_SET) {
			RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: ignoring invalid HouseHandicaps %s=%d", name, handicap);
		}

		int location = ini.Get_Int("SpawnLocations", name, -1);
		if (location >= 0 && location < WAYPT_COUNT) {
			Spawner.SpawnLocation[slot] = location;
		} else if (location != -1) {
			RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: ignoring invalid SpawnLocations %s=%d", name, location);
		}

		Spawner.Spectator[slot] = ini.Get_Bool("IsSpectator", name, false);
	}
}


/***********************************************************************************************
 * Spawner_Ignore_Color -- Marks a player colour as ignored for chat.                          *
 *                                                                                             *
 * INPUT:   color -- The player colour.                                                        *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Ignore_Color(int color)
{
	if (color >= 0 && color < 256) {
		Spawner.IgnoredColor[color] = true;
	}
}


/***********************************************************************************************
 * Spawner_Is_Color_Ignored -- Is chat from this colour ignored?                               *
 *                                                                                             *
 * INPUT:   color -- The player colour.                                                        *
 *                                                                                             *
 * OUTPUT:  Should messages from that colour be thrown away?                                   *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Is_Color_Ignored(int color)
{
	if (!Spawner.Active || color < 0 || color >= 256) {
		return(false);
	}
	return(Spawner.IgnoredColor[color]);
}


/***********************************************************************************************
 * Spawner_Incoming_Message_Allowed -- Should a chat message be shown?                         *
 *                                                                                             *
 * INPUT:   color -- The sender's colour as carried in the message.                            *
 *          text  -- The message text.                                                         *
 *                                                                                             *
 * OUTPUT:  true if the message should be displayed.                                           *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/28/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Incoming_Message_Allowed(int color, char const * text)
{
	static char const * const whitelist[] = {
		"f ", "b ", "fb ", "fb? ", "front ", "back ", "base ", "home ", "help ", "chinook ", "cy ",
		"nook ", "heli ", "south ", "north ", "east ", "west ", "bottom ", "top ", "left ", "right ",
		"ally ", "a ", "yellow ", "blue ", "red ", "green ", "orange ", "grey ", "gray ", "teal ", "brown ",
		"go ", "gogo ", "attack ", "ally me ", "go south ", "go north ", "i tank ", "i build ", "you tank ",
		"you build ", "make tanks ", "make helis ", "tank ", "build ", "ref ", "refinery ", "ore ", "ore space ",
		"ref space ", "space ", "make depot ", "depot ", "v2 ", "make v2 ", "make cy ", "take cy ", "take depot ",
		"repair ", "engi ", "use engi ", "send cy ", "need cy ", "wait ", "spec ", "specs ", "n ", "s ", "apc ",
		"transport ", "make boats ", "boats ", "cruisers ", "gg ", NULL
	};

	if (!Spawner_Is_Color_Ignored(color)) {
		return(true);
	}

	/*
	**	Everything from an ignored player who is no longer playing is dropped.
	*/
	for (int slot = 0; slot < SPAWNER_SLOT_COUNT; slot++) {
		HouseClass * hptr = HouseClass::As_Pointer((HousesType)(HOUSE_MULTI1 + slot));
		if (hptr != NULL && hptr->IsHuman && (int)hptr->RemapColor == color) {
			if (hptr->IsDefeated || Spawner.Spectator[slot]) {
				return(false);
			}
			break;
		}
	}

	/*
	**	Ignore the "(Allies):" prefix and any spaces around the text so that "gg", " gg" and
	**	"gg " are all the same call-out.
	*/
	char body[256];
	char const * source = text;
	if (strncmp(source, "(Allies):", 9) == 0) {
		source += 9;
	}
	while (*source == ' ') {
		source++;
	}
	strncpy(body, source, sizeof(body) - 1);
	body[sizeof(body) - 1] = 0;
	int length = strlen(body);
	while (length > 0 && body[length - 1] == ' ') {
		body[--length] = 0;
	}

	for (int index = 0; whitelist[index] != NULL; index++) {
		char entry[32];
		strncpy(entry, whitelist[index], sizeof(entry) - 1);
		entry[sizeof(entry) - 1] = 0;
		int entry_length = strlen(entry);
		while (entry_length > 0 && entry[entry_length - 1] == ' ') {
			entry[--entry_length] = 0;
		}
		if (stricmp(body, entry) == 0) {
			return(true);
		}
	}
	return(false);
}


/***********************************************************************************************
 * Spawner_Apply_House_Overrides -- Applies colours/countries/handicaps/spectators.            *
 *                                                                                             *
 * This is the end-of-Assign_Houses step. Colour, country and handicap overrides apply to      *
 * every multiplayer slot. Slots flagged as spectators are marked defeated so that they take   *
 * no part in the match (no units, not counted when deciding whether the game is over), and if *
 * the local player is a spectator the same "see everything" state that a defeated player      *
 * gets is switched on from the start.                                                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Must be called after the houses have been assigned to players and PlayerPtr     *
 *             has been set.                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Apply_House_Overrides(void)
{
	if (!Spawner.Active) {
		return;
	}

	SpectatorCount = 0;
	for (int slot = 0; slot < SPAWNER_SLOT_COUNT; slot++) {
		HouseClass * hptr = HouseClass::As_Pointer((HousesType)(HOUSE_MULTI1 + slot));
		if (hptr == NULL) {
			continue;
		}

		if (Spawner.HouseColor[slot] != SPAWNER_NOT_SET) {
			hptr->RemapColor = (PlayerColorType)Spawner.HouseColor[slot];
		}
		if (Spawner.HouseCountry[slot] != SPAWNER_NOT_SET) {
			hptr->ActLike = (HousesType)Spawner.HouseCountry[slot];
		}
		if (Spawner.HouseHandicap[slot] != SPAWNER_NOT_SET) {
			hptr->Assign_Handicap((DiffType)Spawner.HouseHandicap[slot]);
		}

		if (Spawner.Spectator[slot]) {
			hptr->Become_Spectator();
			SpectatorCount++;
			RA_DiagLog_Write(RA_DIAG_LOG_INFO, "Spawner: house slot %d is a spectator", slot + 1);
		}
	}
}


/***********************************************************************************************
 * Spawner_Is_Spectator -- Is this multiplayer house an observer?                              *
 *                                                                                             *
 * INPUT:   house -- The HousesType to check.                                                  *
 *                                                                                             *
 * OUTPUT:  true if the house belongs to an observer.                                          *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Is_Spectator(int house)
{
	int slot = house - (int)HOUSE_MULTI1;
	if (!Spawner.Active || slot < 0 || slot >= SPAWNER_SLOT_COUNT) {
		return(false);
	}
	return(Spawner.Spectator[slot]);
}


/***********************************************************************************************
 * Spawner_Spectator_Count -- Number of observers in the match.                                *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  The number of spectator houses set up by Spawner_Apply_House_Overrides.            *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
int Spawner_Spectator_Count(void)
{
	return(SpectatorCount);
}


/***********************************************************************************************
 * Spawner_Get_Spawn_Location -- The waypoint a house has been told to start at.               *
 *                                                                                             *
 * INPUT:   house -- The HousesType to check.                                                  *
 *                                                                                             *
 * OUTPUT:  The waypoint number, or -1 if the game should choose.                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
int Spawner_Get_Spawn_Location(int house)
{
	int slot = house - (int)HOUSE_MULTI1;
	if (!Spawner.Active || slot < 0 || slot >= SPAWNER_SLOT_COUNT) {
		return(-1);
	}
	return(Spawner.SpawnLocation[slot]);
}


/***********************************************************************************************
 * Spawner_Skip_Scenario_Object_House -- Should scenario objects of this house be skipped?     *
 *                                                                                             *
 * Maps place starting units and structures for every multiplayer slot. Slots that are not     *
 * playing (spectators or unused slots) must not receive them.                                 *
 *                                                                                             *
 * INPUT:   house -- The HousesType named by the scenario entry.                               *
 *                                                                                             *
 * OUTPUT:  true if the entry should be ignored.                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Skip_Scenario_Object_House(int house)
{
	if (!Spawner.Active || house < (int)HOUSE_MULTI1 || house >= (int)HOUSE_COUNT) {
		return(false);
	}
	HouseClass * hptr = HouseClass::As_Pointer((HousesType)house);
	return(hptr != NULL && hptr->IsDefeated);
}


/***********************************************************************************************
 * Spawner_Track_Stats -- Should unit statistics be collected for this match?                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  true if the per-house unit trackers are in use.                                    *
 *                                                                                             *
 * WARNINGS:   Valid once the session type has been set up.                                    *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/28/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Track_Stats(void)
{
	if (Session.Type == GAME_INTERNET) {
		return(true);
	}
	return(Spawner.Active && Session.Type != GAME_NORMAL);
}


static int Slot_Of(int house)
{
	int slot = house - (int)HOUSE_MULTI1;
	if (slot < 0 || slot >= SPAWNER_SLOT_COUNT) {
		return(-1);
	}
	return(slot);
}


void Spawner_Note_Resigned(int house)
{
	int slot = Slot_Of(house);
	if (Spawner.Active && slot != -1) {
		Spawner.Resigned[slot] = true;
	}
}


void Spawner_Note_Connection_Lost(int house)
{
	int slot = Slot_Of(house);
	if (Spawner.Active && slot != -1) {
		Spawner.ConnectionLost[slot] = true;
	}
}


void Spawner_Note_Out_Of_Sync(void)
{
	if (Spawner.Active) {
		Spawner.OutOfSync = true;
	}
}


void Spawner_Note_Start_Cell(int house, int cell)
{
	int slot = Slot_Of(house);
	if (Spawner.Active && slot != -1) {
		Spawner.StartCell[slot] = cell;
	}
}


int Spawner_Get_Start_Cell(int house)
{
	int slot = Slot_Of(house);
	return(slot == -1 ? -1 : Spawner.StartCell[slot]);
}


bool Spawner_Was_Resigned(int house)
{
	int slot = Slot_Of(house);
	return(slot != -1 && Spawner.Resigned[slot]);
}


bool Spawner_Was_Connection_Lost(int house)
{
	int slot = Slot_Of(house);
	return(slot != -1 && Spawner.ConnectionLost[slot]);
}


/***********************************************************************************************
 * Spawner_Write_Stats -- Writes the statistics packet to STATS.DMP.                           *
 *                                                                                             *
 * INPUT:   data   -- The finished statistics packet.                                          *
 *          length -- Its size in bytes.                                                       *
 *                                                                                             *
 * OUTPUT:  true if the file was written.                                                      *
 *                                                                                             *
 * WARNINGS:   Only the first call in a match writes anything.                                 *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/28/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Write_Stats(void const * data, int length)
{
	if (!Spawner.Active || Spawner.StatsWritten) {
		return(false);
	}

	if (data == NULL || length <= 0) {
		RA_DiagLog_Write(RA_DIAG_LOG_WARN,
			"Spawner: not writing STATS.DMP (data=%p length=%d)", data, length);
		return(false);
	}

	/*
	**	Write straight to the game directory with a raw file. CCFileClass would also look
	**	inside the MIX files and its open can fail when the archive holds a stale
	**	STATS.DMP, which is exactly what must not happen here -- the client reads the file
	**	straight off the disk.
	*/
	RawFileClass file("STATS.DMP");
	if (!file.Open(WRITE)) {
		RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: cannot open STATS.DMP for writing");
		return(false);
	}

	long written = file.Write(data, length);
	file.Close();

	if (written != length) {
		RA_DiagLog_Write(RA_DIAG_LOG_WARN,
			"Spawner: short write to STATS.DMP (%ld of %d bytes)", written, length);
		return(false);
	}

	RA_DiagLog_Write(RA_DIAG_LOG_INFO, "Spawner: wrote STATS.DMP (%d bytes)", length);
	Spawner.StatsWritten = true;
	return(true);
}


/***********************************************************************************************
 * Spawner_Reset_Network -- Clears the peer table and keys.                                    *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Reset_Network(void)
{
#ifdef WINSOCK_IPX
	memset(Peers, 0, sizeof(Peers));
	PeerCount = 0;
	TunnelIp = 0;
	TunnelPort = 0;
	TunnelId = 0;
	PacketHeader = 0;
	PlayerId = 0;
	memset(NetKey, 0, sizeof(NetKey));
#endif
}


/***********************************************************************************************
 * Spawner_Add_Peer -- Registers a remote player.                                              *
 *                                                                                             *
 * INPUT:   ip    -- The peer's IP address in network order (zero when tunnelling).            *
 *          port  -- The peer's port (or tunnel id) in network order.                          *
 *                                                                                             *
 * OUTPUT:  The token to store in the player's address (1..SPAWNER_MAX_PEERS), or -1 if the    *
 *          table is full.                                                                     *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
int Spawner_Add_Peer(unsigned long ip, unsigned short port)
{
#ifdef WINSOCK_IPX
	if (PeerCount >= SPAWNER_MAX_PEERS) {
		return(-1);
	}
	Peers[PeerCount].Ip = ip;
	Peers[PeerCount].Port = port;
	PeerCount++;
	return(PeerCount);
#else
	ip = ip;
	port = port;
	return(-1);
#endif
}


/***********************************************************************************************
 * Spawner_Set_Tunnel -- Configures the tunnel server.                                         *
 *                                                                                             *
 * INPUT:   ip    -- Tunnel server address (network order).                                    *
 *          port  -- Tunnel server port (network order); zero disables tunnelling.             *
 *          id    -- Our id on the tunnel (network order).                                     *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Set_Tunnel(unsigned long ip, unsigned short port, unsigned short id)
{
#ifdef WINSOCK_IPX
	TunnelIp = ip;
	TunnelPort = port;
	TunnelId = id;
#else
	ip = ip;
	port = port;
	id = id;
#endif
}


/***********************************************************************************************
 * Spawner_Using_Tunnel -- Is traffic routed through a tunnel?                                 *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  true if a tunnel server is configured.                                             *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
bool Spawner_Using_Tunnel(void)
{
#ifdef WINSOCK_IPX
	return(TunnelPort != 0);
#else
	return(false);
#endif
}


/***********************************************************************************************
 * Spawner_Set_Game_Id -- Sets the game id and derived packet header.                          *
 *                                                                                             *
 * INPUT:   game_id -- The launcher supplied game id.                                          *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Set_Game_Id(unsigned long game_id)
{
#ifdef WINSOCK_IPX
	/*
	**	The header is the game id rotated right by eight bits.
	*/
	unsigned int id = (unsigned int)(game_id & 0xFFFFFFFF);
	PacketHeader = (id >> 8) | (id << 24);
#else
	game_id = game_id;
#endif
}


/***********************************************************************************************
 * Spawner_Set_Player_Id -- Sets the id stamped on outgoing packets.                           *
 *                                                                                             *
 * INPUT:   id -- The local player's id (their colour).                                        *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Set_Player_Id(int id)
{
#ifdef WINSOCK_IPX
	PlayerId = (unsigned char)id;
#else
	id = id;
#endif
}


#ifdef WINSOCK_IPX

/***********************************************************************************************
 * Hash_File -- Feeds the contents of a game file into a SHA-256 hash.                         *
 *                                                                                             *
 * INPUT:   sha  -- The hash to update.                                                        *
 *          name -- The file name (resolved through the normal game file search path).         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   A missing file contributes nothing, exactly like the original.                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
static void Hash_File(Sha256Class & sha, char const * name)
{
	CCFileClass file(name);
	if (!file.Is_Available()) {
		return;
	}
	if (!file.Open(READ)) {
		return;
	}

	unsigned char chunk[2048];
	long got;
	while ((got = file.Read(chunk, sizeof(chunk))) > 0) {
		sha.Update(chunk, (unsigned long)got);
	}
	file.Close();
}


/***********************************************************************************************
 * Hash_Executable -- Feeds the running program into a SHA-256 hash.                           *
 *                                                                                             *
 * The original patch hashed the code section of the loaded image. The image on disk is used   *
 * here instead because it is identical for identical builds even when Windows relocates the   *
 * loaded copy, and it ties a match to one exact build all the same.                           *
 *                                                                                             *
 * INPUT:   sha -- The hash to update.                                                         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
static void Hash_Executable(Sha256Class & sha)
{
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(NULL, path, sizeof(path));
	if (length == 0 || length >= sizeof(path)) {
		RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: cannot determine executable path for net key");
		return;
	}

	HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		RA_DiagLog_Write(RA_DIAG_LOG_WARN, "Spawner: cannot read executable for net key");
		return;
	}

	unsigned char chunk[4096];
	DWORD got = 0;
	while (ReadFile(handle, chunk, sizeof(chunk), &got, NULL) && got > 0) {
		sha.Update(chunk, got);
	}
	CloseHandle(handle);
}

#endif


/***********************************************************************************************
 * Spawner_Generate_Net_Key -- Builds the key that protects match traffic.                     *
 *                                                                                             *
 * Everything that must be identical on every machine for the match to stay in sync -- the     *
 * scenario, the spawner rules files and the executable itself -- goes into the key, so        *
 * packets from a player who differs in any of them fail to decode and are discarded.          *
 *                                                                                             *
 * INPUT:   scenario_file -- The scenario file name.                                           *
 *          salt          -- Value mixed into every block of the key.                          *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Call after the scenario has been located and before connections are created.    *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
void Spawner_Generate_Net_Key(char const * scenario_file, unsigned int salt)
{
#ifdef WINSOCK_IPX
	Sha256Class sha;
	unsigned char first[SPAWNSEC_SHA256_DIGEST_SIZE];
	unsigned char salt_bytes[4];

	Hash_File(sha, scenario_file);
	Hash_File(sha, "SPAWN.XDP");
	Hash_File(sha, "SPAWNAM.XDP");
	Hash_Executable(sha);

	salt_bytes[0] = (unsigned char)(salt);
	salt_bytes[1] = (unsigned char)(salt >> 8);
	salt_bytes[2] = (unsigned char)(salt >> 16);
	salt_bytes[3] = (unsigned char)(salt >> 24);
	sha.Update(salt_bytes, 4);
	sha.Final(first);

	Spawner_Expand_Net_Key(first, salt, NetKey);
	RA_DiagLog_Write(RA_DIAG_LOG_INFO, "Spawner: net key generated (salt=%u)", salt);
#else
	scenario_file = scenario_file;
	salt = salt;
#endif
}


#ifdef WINSOCK_IPX

static void Put_Le32(unsigned char * dest, unsigned int value)
{
	dest[0] = (unsigned char)(value);
	dest[1] = (unsigned char)(value >> 8);
	dest[2] = (unsigned char)(value >> 16);
	dest[3] = (unsigned char)(value >> 24);
}

static unsigned int Get_Le32(unsigned char const * source)
{
	return((unsigned int)source[0] |
			 ((unsigned int)source[1] << 8) |
			 ((unsigned int)source[2] << 16) |
			 ((unsigned int)source[3] << 24));
}

#endif


/***********************************************************************************************
 * Spawner_Send_Packet -- Protects and sends a packet.                                         *
 *                                                                                             *
 * INPUT:   sock    -- The UDP socket.                                                         *
 *          address -- The game's (IPXAddressClass sized) address; bytes 4..7 hold the peer    *
 *                     token handed out by Spawner_Add_Peer.                                   *
 *          buffer  -- The packet payload.                                                     *
 *          length  -- Payload length.                                                         *
 *                                                                                             *
 * OUTPUT:  Bytes sent. Packets that cannot be routed are dropped and reported as sent so the  *
 *          caller does not retry them forever. A negative number means the socket failed.     *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
int Spawner_Send_Packet(unsigned int sock, unsigned char const * address, void const * buffer, int length)
{
#ifdef WINSOCK_IPX
	unsigned char packet[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
	struct sockaddr_in dest;

	if (length < 0 || length > PACKET_MAX_PAYLOAD) {
		return(0);
	}

	/*
	**	The token in the address selects the peer. Anything else is not one of ours.
	*/
	int index = (int)Get_Le32(address + 4) - 1;
	if (index < 0 || index >= PeerCount) {
		return(0);
	}

	memset(packet, 0, PACKET_HEADER_SIZE);
	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;

	if (TunnelPort != 0) {
		/*
		**	Through a tunnel the header names both ends and the datagram goes to the server.
		*/
		memcpy(packet, &TunnelId, 2);
		memcpy(packet + 2, &Peers[index].Port, 2);
		dest.sin_port = TunnelPort;
		dest.sin_addr.s_addr = TunnelIp;
	} else {
		Put_Le32(packet, PacketHeader);
		dest.sin_port = Peers[index].Port;
		dest.sin_addr.s_addr = Peers[index].Ip;
	}

	Put_Le32(packet + 4, Spawner_Crc32(PacketHeader, buffer, (unsigned long)length));
	packet[8] = PlayerId;
	Spawner_Xor_Key_Stream(packet + PACKET_HEADER_SIZE, buffer, (unsigned long)length, NetKey, sizeof(NetKey));

	int sent = sendto((SOCKET)sock, (char const *)packet, length + PACKET_HEADER_SIZE, 0, (struct sockaddr *)&dest, sizeof(dest));
	if (sent == SOCKET_ERROR) {
		return(-1);
	}
	return(length);
#else
	sock = sock;
	address = address;
	buffer = buffer;
	length = length;
	return(-1);
#endif
}


/***********************************************************************************************
 * Spawner_Receive_Packet -- Receives, verifies and decodes a packet.                          *
 *                                                                                             *
 * INPUT:   sock        -- The UDP socket.                                                     *
 *          buffer      -- Receives the decoded payload.                                       *
 *          buffer_size -- Size of 'buffer'.                                                   *
 *          address     -- Receives the game address (SPAWNER_ADDRESS_SIZE bytes) of the       *
 *                         sender, with the peer token in bytes 4..7.                          *
 *                                                                                             *
 * OUTPUT:  Payload length; zero if the datagram should be ignored (wrong game, our own        *
 *          echo, bad checksum, unknown sender); negative if the socket reported an error.     *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/27/2026 : Created.                                                                     *
 *=============================================================================================*/
int Spawner_Receive_Packet(unsigned int sock, void * buffer, int buffer_size, unsigned char * address)
{
#ifdef WINSOCK_IPX
	unsigned char packet[PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD];
	struct sockaddr_in from;
	int from_length = sizeof(from);

	memset(&from, 0, sizeof(from));
	int received = recvfrom((SOCKET)sock, (char *)packet, sizeof(packet), 0, (struct sockaddr *)&from, &from_length);
	if (received == SOCKET_ERROR) {
		return(-1);
	}
	if (received <= PACKET_HEADER_SIZE) {
		return(0);
	}

	/*
	**	Ignore our own packets.
	*/
	if (packet[8] == PlayerId) {
		return(0);
	}

	unsigned long source_ip;
	unsigned short source_port;
	if (TunnelPort != 0) {
		unsigned short sender_id;
		unsigned short target_id;

		memcpy(&sender_id, packet, 2);
		memcpy(&target_id, packet + 2, 2);
		if (target_id != TunnelId) {
			return(0);
		}
		source_ip = 0;
		source_port = sender_id;
	} else {
		if (Get_Le32(packet) != PacketHeader) {
			return(0);
		}
		source_ip = from.sin_addr.s_addr;
		source_port = from.sin_port;
	}

	int payload = received - PACKET_HEADER_SIZE;
	if (payload > buffer_size) {
		return(0);
	}
	Spawner_Xor_Key_Stream(buffer, packet + PACKET_HEADER_SIZE, (unsigned long)payload, NetKey, sizeof(NetKey));
	if (Get_Le32(packet + 4) != Spawner_Crc32(PacketHeader, buffer, (unsigned long)payload)) {
		return(0);
	}

	/*
	**	Translate the sender back into the token the game knows them by.
	*/
	for (int index = 0; index < PeerCount; index++) {
		if (Peers[index].Ip == source_ip && Peers[index].Port == source_port) {
			memset(address, 0, SPAWNER_ADDRESS_SIZE);
			Put_Le32(address + 4, (unsigned int)(index + 1));
			return(payload);
		}
	}
	return(0);
#else
	sock = sock;
	buffer = buffer;
	buffer_size = buffer_size;
	address = address;
	return(-1);
#endif
}
