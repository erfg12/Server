/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "../common/database.h"
#include "../common/guilds.h"
#include "../common/string_util.h"

#include "guild_mgr.h"
#include "worldserver.h"

extern WorldServer worldserver;

void Client::SendGuildMOTD(bool GetGuildMOTDReply) {
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_GuildMOTD, sizeof(GuildMOTD_Struct));

	// When the Client gets an OP_GuildMOTD, it compares the text to the version it has previously stored.
	// If the text in the OP_GuildMOTD packet is the same, it does nothing. If not the same, it displays
	// the new MOTD and then stores the new text.
	//

	GuildMOTD_Struct *motd = (GuildMOTD_Struct *) outapp->pBuffer;
	motd->unknown0 = 0;
	strn0cpy(motd->name, m_pp.name, 64);

	if(IsInAGuild()) {
		if(!guild_mgr.GetGuildMOTD(GuildID(), motd->motd, motd->setby_name)) {
			motd->setby_name[0] = '\0';
			strcpy(motd->motd, "ERROR GETTING MOTD!");
		}
	} else {
		//we have to send them an empty MOTD anywyas.
		motd->motd[0] = '\0';	//just to be sure
		motd->setby_name[0] = '\0';	//just to be sure

	}

	Log.Out(Logs::Detail, Logs::Guilds, "Sending OP_GuildMOTD of length %d", outapp->size);

	FastQueuePacket(&outapp);
}

void Client::SendGuildSpawnAppearance() {
	if (!IsInAGuild()) {
		// clear guildtag
		SendAppearancePacket(AT_GuildID, GUILD_NONE);
		Log.Out(Logs::Detail, Logs::Guilds, "Sending spawn appearance for no guild tag.");
	} else {
		uint8 rank = guild_mgr.GetDisplayedRank(GuildID(), GuildRank(), CharacterID());
		Log.Out(Logs::Detail, Logs::Guilds, "Sending spawn appearance for guild %d at rank %d", GuildID(), rank);
		SendAppearancePacket(AT_GuildID, GuildID());
		SendAppearancePacket(AT_GuildRank, rank);
	}
	UpdateWho();
}

void Client::SendGuildList() {
	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_GuildsList);

	//ask the guild manager to build us a nice guild list packet
	 OldGuildsList_Struct* guildstruct = guild_mgr.MakeOldGuildList(outapp->size);
	outapp->pBuffer = reinterpret_cast<uchar*>(guildstruct);

	if(outapp->pBuffer == nullptr) {
		Log.Out(Logs::Detail, Logs::Guilds, "Unable to make guild list!");
		return;
	}

	Log.Out(Logs::Detail, Logs::Guilds, "Sending OP_GuildsList of length %d", outapp->size);

	FastQueuePacket(&outapp);
}

void Client::SendPlayerGuild() {
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_GuildAdded, sizeof(OldGuildUpdate_Struct));
	OldGuildUpdate_Struct* gu=(OldGuildUpdate_Struct*)outapp->pBuffer;

	int16 guid = this->GuildID();
	std::string tmp;
		
	if(guild_mgr.GetGuildNameByID(guid,tmp))
	{
		Log.Out(Logs::Detail, Logs::Guilds, "SendPlayerGuild(): GUID: %d Name: %s", guid, tmp.c_str());
		gu->guildID=guid;
		memcpy(gu->entry.name,tmp.c_str(),64);
		gu->entry.guildID=guid;
		gu->entry.exists=1;
	}
	else
	{
		gu->entry.guildID=0xFFFFFFFF;
		gu->entry.exists=0;
	}

	gu->entry.unknown1=0xFFFFFFFF;
	gu->entry.unknown3=0xFFFFFFFF;

	Log.Out(Logs::Detail, Logs::Guilds, "Sending OP_GuildAdded of length %d guildID %d", outapp->size, gu->entry.guildID);

	FastQueuePacket(&outapp);
}

void Client::RefreshGuildInfo()
{
	uint32 OldGuildID = guild_id;

	guildrank = GUILD_RANK_NONE;
	guild_id = GUILD_NONE;

	CharGuildInfo info;
	if(!guild_mgr.GetCharInfo(CharacterID(), info)) {
		Log.Out(Logs::Detail, Logs::Guilds, "Unable to obtain guild char info for %s (%d)", GetName(), CharacterID());
		return;
	}

	guildrank = info.rank;
	guild_id = info.guild_id;

	SendGuildSpawnAppearance();
}

void EntityList::SendGuildMOTD(uint32 guild_id) {
	if(guild_id == GUILD_NONE)
		return;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (client->GuildID() == guild_id) {
			client->SendGuildMOTD();
		}
		++it;
	}
}

void EntityList::SendGuildSpawnAppearance(uint32 guild_id) {
	if(guild_id == GUILD_NONE)
		return;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (client->GuildID() == guild_id) {
			client->SendGuildSpawnAppearance();
		}
		++it;
	}
}

void EntityList::RefreshAllGuildInfo(uint32 guild_id) {
	if(guild_id == GUILD_NONE)
		return;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (client->GuildID() == guild_id) {
			client->RefreshGuildInfo();
		}
		++it;
	}
}

void EntityList::SendGuildList() {
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		client->SendGuildList();
		++it;
	}
}
