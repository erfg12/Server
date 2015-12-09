/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2008 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/eqemu_logsys.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <limits.h>
#include <ctype.h>
#include <assert.h>
#include <map>
#include <vector>

// Disgrace: for windows compile
#ifdef _WINDOWS
#include <windows.h>
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#else
#include "../common/unix.h"
#include <netinet/in.h>
#endif

#include "database.h"
#include "../common/eq_packet_structs.h"
#include "../common/string_util.h"
#include "../common/servertalk.h"

Database::Database ()
{
	DBInitVars();
}

/*
 * Establish a connection to a mysql database with the supplied parameters
 */
Database::Database(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	DBInitVars();
	Connect(host, user, passwd, database, port);
}

bool Database::Connect(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	uint32 errnum= 0;
	char errbuf[MYSQL_ERRMSG_SIZE];
	if (!Open(host, user, passwd, database, port, &errnum, errbuf))
	{
		Log.Out(Logs::General, Logs::Error, "Failed to connect to database: Error: %s", errbuf);
		HandleMysqlError(errnum);
		return false;
	}
	else
	{
		Log.Out(Logs::General, Logs::Status, "Using database '%s' at %s:%d",database,host,port);
		return true;
	}
}

void Database::DBInitVars() {}

void Database::HandleMysqlError(uint32 errnum) {}

/*
 * Close the connection to the database
 */
Database::~Database() {}

void Database::LogPlayerTrade(QSPlayerLogTrade_Struct* QS, uint32 detailCount)
{
	if (detailCount == 0)
	{
		return;
	}

    for(uint32 i = 0; i < detailCount; i++)
	{
		std::string query = StringFormat(
			"INSERT INTO `qs_player_trade_log` SET "
				"`char1_id` = '%i', "
				"`char1_pp` = '%i', "
				"`char1_gp` = '%i', "
				"`char1_sp` = '%i', "
				"`char1_cp` = '%i', "
				"`char1_items` = '%i', "
				"`char2_id` = '%i', "
				"`char2_pp` = '%i', "
				"`char2_gp` = '%i', "
				"`char2_sp` = '%i', "
				"`char2_cp` = '%i', "
				"`char2_items` = '%i', "
				"`from_id` = '%i', "
				"`from_slot` = '%i', "
				"`to_id` = '%i', "
				"`to_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`time` = NOW();",
				QS->char1_id,
				QS->char1_money.platinum,
				QS->char1_money.gold,
				QS->char1_money.silver,
				QS->char1_money.copper,
				QS->char1_count,
				QS->char2_id,
				QS->char2_money.platinum,
				QS->char2_money.gold,
				QS->char2_money.silver,
				QS->char2_money.copper,
				QS->char2_count,
				QS->items[i].from_id,
				QS->items[i].from_slot,
				QS->items[i].to_id,
				QS->items[i].to_slot,
				QS->items[i].item_id,
				QS->items[i].charges);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log.Out(Logs::Detail, Logs::QS_Server, "Failed Trade Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void Database::LogPlayerHandin(QSPlayerLogHandin_Struct* QS, uint32 detailCount)
{
	if (detailCount == 0)
	{
		return;
	}

    for(uint32 i = 0; i < detailCount; i++)
	{
        std::string query = StringFormat(
			"INSERT INTO `qs_player_handin_log` SET "
				"`char_id` = '%i', "
				"`action_type` = '%s', "
				"`quest_id` = '%i', "
				"`char_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`char_pp` = '%i', "
				"`char_gp` = '%i', "
				"`char_sp` = '%i', "
				"`char_cp` = '%i', "
				"`char_items` = '%i', "
				"`npc_id` = '%i', "
				"`npc_pp` = '%i', "
				"`npc_gp` = '%i', "
				"`npc_sp` = '%i', "
				"`npc_cp` = '%i', "
				"`npc_items` = '%i', "
				"`time` = now()",
				QS->char_id,
				QS->items[i].action_type,
				QS->quest_id,
				QS->items[i].char_slot,
				QS->items[i].item_id,
				QS->items[i].charges,
				QS->char_money.platinum,
				QS->char_money.gold,
				QS->char_money.silver,
				QS->char_money.copper,
				QS->char_count,
				QS->npc_id,
				QS->npc_money.platinum,
				QS->npc_money.gold,
				QS->npc_money.silver,
				QS->npc_money.copper,
				QS->npc_count);

		auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log.Out(Logs::Detail, Logs::QS_Server, "Failed Hand in Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void Database::LogPlayerNPCKill(QSPlayerLogNPCKill_Struct* QS, uint32 members)
{
	if (members == 0)
	{
		return;
	}

	for (uint32 i = 0; i < members; i++)
	{
		std::string query = StringFormat(
			"INSERT INTO `qs_player_npc_kill_log` SET "
				"`char_id` = '%i', "
				"`npc_id` = '%i', "
				"`type` = '%i', "
				"`zone_id` = '%i', "
				"`time` = now()",
				QS->Chars[i].char_id,
				QS->s1.NPCID,
				QS->s1.Type,
				QS->s1.ZoneID);

		auto results = QueryDatabase(query);
		if(!results.Success())
		{
			Log.Out(Logs::Detail, Logs::QS_Server, "Failed NPC Kill Log Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
		}
	}
}

void Database::LogPlayerItemDelete(QSPlayerLogItemDelete_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

    for(uint32 i = 0; i < items; i++)
	{
		std::string query = StringFormat(
			"INSERT INTO `qs_player_item_delete_log` SET "
				"`char_id` = '%i', "
				"`char_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`stack_size` = '%i', "
				"`char_items` = '%i', "
				"`time` = now();",
				QS->char_id,
				QS->char_slot,
				QS->item_id,
				QS->charges,
				QS->stack_size,
				QS->char_count);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log.Out(Logs::Detail, Logs::QS_Server, "Failed Delete Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void Database::LogPlayerItemMove(QSPlayerLogItemMove_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

    for(uint32 i = 0; i < items; i++)
	{
        std::string query = StringFormat(
			"INSERT INTO `qs_player_item_move_log` SET "
				"`char_id` = '%i', "
				"`from_slot` = '%i', "
				"`to_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`stack_size` = '%i', "
				"`char_items` = '%i', "
				"`postaction` = '%i', "
				"`time` = now()",
				QS->char_id,
				QS->items[i].from_slot,
				QS->items[i].to_slot,
				QS->items[i].item_id,
				QS->items[i].charges,
				QS->stack_size,
				QS->char_count,
				QS->postaction);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log.Out(Logs::Detail, Logs::QS_Server, "Failed Move Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void Database::LogMerchantTransaction(QSMerchantLogTransaction_Struct* QS, uint32 items)
{
	/* Merchant transactions are from the perspective of the merchant, not the player -U */
	if (items == 0)
	{
		return;
	}

    for(uint32 i = 0; i < items; i++)
	{
		std::string query = StringFormat(
			"INSERT INTO `qs_merchant_transaction_log` SET "
				"`char_id` = '%i', "
				"`char_slot` = '%i', "
				"`item_id` = '%i', "
				"`charges` = '%i', "
				"`zone_id` = '%i', "
				"`merchant_id` = '%i', "
				"`merchant_pp` = '%i', "
				"`merchant_gp` = '%i', "
				"`merchant_sp` = '%i', "
				"`merchant_cp` = '%i', "
				"`merchant_items` = '%i', "
				"`char_pp` = '%i', "
				"`char_gp` = '%i', "
				"`char_sp` = '%i', "
				"`char_cp` = '%i', "
				"`char_items` = '%i', "
				"`time` = NOW()",
				QS->char_id,
				QS->char_slot,
				QS->item_id,
				QS->charges,
				QS->zone_id,
				QS->merchant_id,
				QS->merchant_money.platinum,
				QS->merchant_money.gold,
				QS->merchant_money.silver,
				QS->merchant_money.copper,
				QS->merchant_count,
				QS->char_money.platinum,
				QS->char_money.gold,
				QS->char_money.silver,
				QS->char_money.copper,
				QS->char_count);

        auto results = QueryDatabase(query);
        if(!results.Success())
		{
			Log.Out(Logs::Detail, Logs::QS_Server, "Failed Transaction Log Record Entry Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
        }
    }
}

void Database::LogPlayerAARateHourly(QSPlayerAARateHourly_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_aa_rate_hourly` (char_id, aa_count, hour_time) "
			"VALUES "
			"(%i, %i, UNIX_TIMESTAMP() - MOD(UNIX_TIMESTAMP(), 3600)) "
			"ON DUPLICATE KEY UPDATE "
			"`aa_count` = `aa_count` + %i",
			QS->charid,
			QS->add_points,
			QS->add_points);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::Detail, Logs::QS_Server, "Failed AA Rate Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void Database::LogPlayerAAPurchase(QSPlayerAAPurchase_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_aa_purchase_log` SET "
			"`char_id` = '%i', "
			"`aa_type` = '%s', "
			"`aa_name` = '%s', "
			"`aa_id` = '%i', "
			"`aa_cost` = '%i', "
			"`zone_id` = '%i', "
			"`instance_id` = '%i', "
			"`time` = now()",
			QS->charid,
			QS->aatype,
			QS->aaname,
			QS->aaid,
			QS->cost,
			QS->zone_id,
			QS->instance_id);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::Detail, Logs::QS_Server, "Failed AA Purchase Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void Database::LogPlayerDeathBy(QSPlayerDeathBy_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_killed_by_log` SET "
		"`char_id` = '%i', "
		"`zone_id` = '%i', "
		"`instance_id` = '%i', "
		"`killed_by` = '%s', "
		"`spell` = '%i', "
		"`damage` = '%i', "
		"`time` = now()",
		QS->charid,
		QS->zone_id,
		QS->instance_id,
		QS->killed_by,
		QS->spell,
		QS->damage);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::Detail, Logs::QS_Server, "Failed Death Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void Database::LogPlayerTSEvents(QSPlayerTSEvents_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_ts_event_log` SET "
			"`char_id` = '%i', "
			"`zone_id` = '%i', "
			"`instance_id` = '%i', "
			"`results` = '%s', "
			"`recipe_id` = '%i', "
			"`tradeskill` = '%i', "
			"`trivial` = '%i', "
			"`chance` = '%f', "
			"`time` = now()",
			QS->charid,
			QS->zone_id,
			QS->instance_id,
			QS->results,
			QS->recipe_id,
			QS->tradeskill,
			QS->trivial,
			QS->chance);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::Detail, Logs::QS_Server, "Failed TS Event Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void Database::LogPlayerQGlobalUpdates(QSPlayerQGlobalUpdate_Struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	std::string query = StringFormat(
		"INSERT INTO `qs_player_qglobal_updates_log` SET "
			"`char_id` = '%i', "
			"`action` = '%s', "
			"`zone_id` = '%i', "
			"`instance_id` = '%i', "
			"`varname` = '%s', "
			"`newvalue` = '%s', "
			"`time` = now()",
			QS->charid,
			QS->action,
			QS->zone_id,
			QS->instance_id,
			QS->varname,
			QS->newvalue);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::Detail, Logs::QS_Server, "Failed QGlobal Update Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void Database::LogPlayerLootRecords(QSPlayerLootRecords_struct* QS, uint32 items)
{
	if (items == 0)
	{
		return;
	}

	Log.Out(Logs::General, Logs::QS_Server, "Inserting loot record");
	std::string query = StringFormat(
		"INSERT INTO `qs_player_loot_records_log` SET "
		"`char_id` = '%i', "
		"`corpse_name` = '%s', "
		"`type` = '%s', "
		"`zone_id` = '%i', "
		"`item_id` = '%i', "
		"`item_name` = '%s', "
		"`charges` = '%i', "
		"`platinum` = '%i', "
		"`gold` = '%i', "
		"`silver` = '%i', "
		"`copper` = '%i', "
		"`time` = now()",
		QS->charid,
		QS->corpse_name,
		QS->type,
		QS->zone_id,
		QS->item_id,
		QS->item_name,
		QS->charges,
		QS->money.platinum,
		QS->money.gold,
		QS->money.silver,
		QS->money.copper);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::General, Logs::QS_Server, "Failed Loot Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
}

void Database::GeneralQueryReceive(ServerPacket *pack)
{
	/*
	 * These are general queries passed from anywhere in zone instead of packing structures and breaking them down again and again
	 */
	char *queryBuffer = new char[pack->ReadUInt32() + 1];
	pack->ReadString(queryBuffer);

	std::string query(queryBuffer);
	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		Log.Out(Logs::Detail, Logs::QS_Server, "Failed Delete Log Record Insert: %s\n%s", results.ErrorMessage().c_str(), query.c_str());
	}
	safe_delete(pack);
	safe_delete_array(queryBuffer);
}

void Database::LoadLogSettings(EQEmuLogSys::LogSettings* log_settings)
{
	std::string query =
		"SELECT "
		"log_category_id, "
		"log_category_description, "
		"log_to_console, "
		"log_to_file, "
		"log_to_gmsay "
		"FROM "
		"logsys_categories "
		"ORDER BY log_category_id";

	auto results = QueryDatabase(query);

	int log_category = 0;
	Log.file_logs_enabled = false;

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		log_category = atoi(row[0]);
		log_settings[log_category].log_to_console = atoi(row[2]);
		log_settings[log_category].log_to_file = atoi(row[3]);
		log_settings[log_category].log_to_gmsay = atoi(row[4]);

		/* Determine if any output method is enabled for the category
		 * and set it to 1 so it can used to check if category is enabled.
		 */
		const bool log_to_console = log_settings[log_category].log_to_console > 0;
		const bool log_to_file = log_settings[log_category].log_to_file > 0;
		const bool log_to_gmsay = log_settings[log_category].log_to_gmsay > 0;
		const bool is_category_enabled = log_to_console || log_to_file || log_to_gmsay;

		if (is_category_enabled)
		{
			log_settings[log_category].is_category_enabled = 1;
		}

		/*
		 * This determines whether or not the process needs to actually file log anything.
		 * If we go through this whole loop and nothing is set to any debug level, there is no point to create a file or keep anything open
		 */
		if (log_settings[log_category].log_to_file > 0)
		{
			Log.file_logs_enabled = true;
		}
	}
}
