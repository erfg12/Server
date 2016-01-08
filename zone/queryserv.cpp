/*	EQEMu: Everquest Server Emulator
Copyright (C) 2001-2014 EQEMu Development Team (http://eqemulator.net)

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

#include "../common/global_define.h"
#include "../common/servertalk.h"
#include "../common/string_util.h"
#include "queryserv.h"
#include "worldserver.h"
#include "net.h"

extern WorldServer worldserver;
extern QueryServ* QServ;

QueryServ::QueryServ() {}

QueryServ::~QueryServ() {}

void QueryServ::SendQuery(std::string Query)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSSendQuery, static_cast<uint32> (Query.length() + 5));
	pack->WriteUInt32(static_cast<uint32> (Query.length())); /* Pack Query String Size so it can be dynamically broken out at queryserv */
	pack->WriteString(Query.c_str()); /* Query */
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void QueryServ::PlayerLogEvent(int Event_Type, int Character_ID, std::string Event_Desc)
{
	std::string query = StringFormat(
		"INSERT INTO `qs_player_events` (event, char_id, event_desc, time) VALUES (%i, %i, '%s', now())",
		Event_Type, Character_ID, EscapeString(Event_Desc).c_str());
	SendQuery(query);
}

void QueryServ::QSQGlobalUpdate(uint32 char_id, uint32 zone_id, int32 instance_id, const char* varname, const char* newvalue)
{
	char action[8];
	if (strcmp(newvalue, "deleted") == 0) { strncpy(action, "Deleted", 8); }
	else { strncpy(action, "Updated", 8); }

	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerQGlobalUpdates, sizeof(QSPlayerQGlobalUpdate_Struct));
	QSPlayerQGlobalUpdate_Struct* QS = (QSPlayerQGlobalUpdate_Struct*)pack->pBuffer;
	QS->charid = char_id;
	strncpy(QS->action, action, 8);
	QS->zone_id = zone_id;
	QS->instance_id = instance_id;
	strncpy(QS->varname, varname, 32);
	strncpy(QS->newvalue, newvalue, 32);
	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSAAPurchases(uint32 char_id, uint32 zone_id, int32 instance_id, char aa_type[8], char aa_name[128], uint32 aa_id, uint32 cost)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerAAPurchase, sizeof(QSPlayerAAPurchase_Struct));
	QSPlayerAAPurchase_Struct* QS = (QSPlayerAAPurchase_Struct*)pack->pBuffer;
	QS->charid = char_id;
	strncpy(QS->aatype, aa_type, 8);
	strncpy(QS->aaname, aa_name, 128);
	QS->aaid = aa_id;
	QS->cost = cost;
	QS->zone_id = zone_id;
	QS->instance_id = instance_id;
	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSAARate(uint32 char_id, uint32 aapoints, uint32 last_unspentAA)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerAARateHourly, sizeof(QSPlayerAARateHourly_Struct));
	QSPlayerAARateHourly_Struct* QS = (QSPlayerAARateHourly_Struct*)pack->pBuffer;
	QS->charid = char_id;
	QS->add_points = aapoints - last_unspentAA;
	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSDeathBy(uint32 char_id, uint32 zone_id, int32 instance_id, char killed_by[128], uint16 spell, int32 damage)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerDeathBy, sizeof(QSPlayerDeathBy_Struct));
	QSPlayerDeathBy_Struct* QS = (QSPlayerDeathBy_Struct*)pack->pBuffer;
	QS->charid = char_id;
	QS->zone_id = zone_id;
	QS->instance_id = instance_id;
	strncpy(QS->killed_by, killed_by, 128);
	QS->spell = spell;
	QS->damage = damage;
	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSTSEvents(uint32 char_id, uint32 zone_id, int32 instance_id, const char results[8], uint32 recipe, uint32 tradeskill, uint16 trivial, float chance)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerTSEvents, sizeof(QSPlayerTSEvents_Struct));
	QSPlayerTSEvents_Struct* QS = (QSPlayerTSEvents_Struct*)pack->pBuffer;
	QS->charid = char_id;
	QS->zone_id = zone_id;
	QS->instance_id = instance_id;
	strncpy(QS->results, results, 8);
	QS->recipe_id = recipe;
	QS->tradeskill = tradeskill;
	QS->trivial = trivial;
	QS->chance = chance;
	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSMerchantTransactions(uint32 char_id, uint32 zone_id, int16 slot_id, uint32 item_id, uint8 charges,
	uint32 merchant_id, int32 merchant_plat, int32 merchant_gold, int32 merchant_silver, int32 merchant_copper, uint16 merchant_count,
	int32 char_plat, int32 char_gold, int32 char_silver, int32 char_copper, uint16 char_count)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLogMerchantTransactions, sizeof(QSMerchantLogTransaction_Struct));
	QSMerchantLogTransaction_Struct* QS = (QSMerchantLogTransaction_Struct*)pack->pBuffer;

	QS->char_id = char_id;
	QS->char_slot = slot_id == INVALID_INDEX ? 0 : slot_id;
	QS->item_id = item_id;
	QS->charges = charges;
	QS->zone_id = zone_id;
	QS->merchant_id = merchant_id;
	QS->merchant_money.platinum = merchant_plat;
	QS->merchant_money.gold = merchant_gold;
	QS->merchant_money.silver = merchant_silver;
	QS->merchant_money.copper = merchant_copper;
	QS->merchant_count = merchant_count;
	QS->char_money.platinum = char_plat;
	QS->char_money.gold = char_gold;
	QS->char_money.silver = char_silver;
	QS->char_money.copper = char_copper;
	QS->char_count = char_count;

	if (slot_id == INVALID_INDEX) {}

	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

void QueryServ::QSLootRecords(uint32 char_id, const char* corpsename, const char* type, uint32 zone_id, uint32 item_id, const char* item_name, int16 charges, int32 platinum, int32 gold, int32 silver, int32 copper)
{
	ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLootRecords, sizeof(QSPlayerLootRecords_struct));
	QSPlayerLootRecords_struct* QS = (QSPlayerLootRecords_struct*)pack->pBuffer;
	QS->charid = char_id;
	strncpy(QS->corpse_name, corpsename, 64);
	strncpy(QS->type, type, 12),
	QS->zone_id = zone_id;
	QS->item_id = item_id;
	strncpy(QS->item_name, item_name, 64);
	QS->charges = charges;
	QS->money.platinum = platinum;
	QS->money.gold = gold;
	QS->money.silver = silver;
	QS->money.copper = copper;
	pack->Deflate();
	if (worldserver.Connected())
	{
		worldserver.SendPacket(pack);
	}
	safe_delete(pack);
}

//TODO: Needs to cover raid/group and solo. Have to figure out how to pass that all into this function.
void QueryServ::QSNPCKills() {}

//TODO: Need to figure out how to convert the trade reporting to this function.
void QueryServ::QSTradeItems() {}

//TODO: Need to figure out how to convert the handin reporting to this function.
void QueryServ::QSHandinItems() {}

//TODO: Need to figure out how to convert the delete reporting to this function.
void QueryServ::QSDeleteItems() {}

//TODO: Need to figure out how to convert the move reporting to this function.
void QueryServ::QSMoveItems() {}
