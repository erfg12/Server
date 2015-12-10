/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2009 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/opcodemgr.h"
#include <iomanip>
#include <iostream>
#include <math.h>
#include <set>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#ifdef _WINDOWS
	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#else
	#include <pthread.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <unistd.h>
#endif

#include "../common/crc32.h"
#include "../common/data_verification.h"
#include "../common/faction.h"
#include "../common/guilds.h"
#include "../common/packet_dump_file.h"
#include "../common/rdtsc.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/string_util.h"
#include "../common/zone_numbers.h"
#include "event_codes.h"
#include "guild_mgr.h"
#include "mob.h"
#include "petitions.h"
#include "pets.h"
#include "queryserv.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "titles.h"
#include "water_map.h"
#include "worldserver.h"
#include "zone.h"
#include "remote_call_subscribe.h"
#include "remote_call_subscribe.h"
#include "../common/misc.h"
#include "position.h"

extern QueryServ* QServ;
extern Zone* zone;
extern volatile bool ZoneLoaded;
extern WorldServer worldserver;
extern PetitionList petition_list;
extern EntityList entity_list;
typedef void (Client::*ClientPacketProc)(const EQApplicationPacket *app);

//Use a map for connecting opcodes since it doesn't get used a lot and is sparse
std::map<uint32, ClientPacketProc> ConnectingOpcodes;
//Use a static array for connected, for speed
ClientPacketProc ConnectedOpcodes[_maxEmuOpcode];

void MapOpcodes()
{
	ConnectingOpcodes.clear();
	memset(ConnectedOpcodes, 0, sizeof(ConnectedOpcodes));

	// Now put all the opcodes into their home...
	// connecting opcode handler assignments:
	ConnectingOpcodes[OP_BazaarSearch] = &Client::Handle_OP_BazaarSearchCon;
	ConnectingOpcodes[OP_ClientError] = &Client::Handle_Connect_OP_ClientError;
	ConnectingOpcodes[OP_ClientUpdate] = &Client::Handle_Connect_OP_ClientUpdate;
	ConnectingOpcodes[OP_DataRate] = &Client::Handle_Connect_OP_SetDataRate;
	ConnectingOpcodes[OP_PetitionRefresh] = &Client::Handle_OP_PetitionRefresh;
	ConnectingOpcodes[OP_ReqClientSpawn] = &Client::Handle_Connect_OP_ReqClientSpawn;
	ConnectingOpcodes[OP_ReqNewZone] = &Client::Handle_Connect_OP_ReqNewZone;
	ConnectingOpcodes[OP_SendExpZonein] = &Client::Handle_Connect_OP_SendExpZonein;
	ConnectingOpcodes[OP_SetGuildMOTD] = &Client::Handle_OP_SetGuildMOTDCon;
	ConnectingOpcodes[OP_SetServerFilter] = &Client::Handle_Connect_OP_SetServerFilter;
	ConnectingOpcodes[OP_SpawnAppearance] = &Client::Handle_Connect_OP_SpawnAppearance;
	ConnectingOpcodes[OP_TGB] = &Client::Handle_Connect_OP_TGB;
	ConnectingOpcodes[OP_WearChange] = &Client::Handle_Connect_OP_WearChange;
	ConnectingOpcodes[OP_ZoneEntry] = &Client::Handle_Connect_OP_ZoneEntry;
	ConnectingOpcodes[OP_LFGCommand] = &Client::Handle_OP_LFGCommand;
	ConnectingOpcodes[OP_TargetMouse] = &Client::Handle_Connect_OP_TargetMouse;
	ConnectingOpcodes[OP_DeleteCharge] = &Client::Handle_Connect_OP_DeleteCharge;

	//temporary hack
	ConnectingOpcodes[OP_GetGuildsList] = &Client::Handle_OP_GetGuildsList;
	ConnectingOpcodes[OP_MoveItem] = &Client::Handle_OP_MoveItem;

	// connected opcode handler assignments:
	ConnectedOpcodes[OP_AAAction] = &Client::Handle_OP_AAAction;
	ConnectedOpcodes[OP_Action2] = &Client::Handle_OP_Action;
	ConnectedOpcodes[OP_Animation] = &Client::Handle_OP_Animation;
	ConnectedOpcodes[OP_ApplyPoison] = &Client::Handle_OP_ApplyPoison;
	ConnectedOpcodes[OP_Assist] = &Client::Handle_OP_Assist;
	ConnectedOpcodes[OP_AutoAttack] = &Client::Handle_OP_AutoAttack;
	ConnectedOpcodes[OP_AutoAttack2] = &Client::Handle_OP_AutoAttack2;
	ConnectedOpcodes[OP_BazaarSearch] = &Client::Handle_OP_BazaarSearch;
	ConnectedOpcodes[OP_Begging] = &Client::Handle_OP_Begging;
	ConnectedOpcodes[OP_Bind_Wound] = &Client::Handle_OP_Bind_Wound;
	ConnectedOpcodes[OP_BoardBoat] = &Client::Handle_OP_BoardBoat;
	ConnectedOpcodes[OP_Buff] = &Client::Handle_OP_Buff;
	ConnectedOpcodes[OP_Bug] = &Client::Handle_OP_Bug;
	ConnectedOpcodes[OP_Camp] = &Client::Handle_OP_Camp;
	ConnectedOpcodes[OP_CancelTrade] = &Client::Handle_OP_CancelTrade;
	ConnectedOpcodes[OP_CastSpell] = &Client::Handle_OP_CastSpell;
	ConnectedOpcodes[OP_ChannelMessage] = &Client::Handle_OP_ChannelMessage;
	ConnectedOpcodes[OP_ClickDoor] = &Client::Handle_OP_ClickDoor;
	ConnectedOpcodes[OP_ClickObject] = &Client::Handle_OP_ClickObject;
	ConnectedOpcodes[OP_ClickObjectAction] = &Client::Handle_OP_ClickObjectAction;
	ConnectedOpcodes[OP_ClientError] = &Client::Handle_OP_ClientError;
	ConnectedOpcodes[OP_ClientUpdate] = &Client::Handle_OP_ClientUpdate;
	ConnectedOpcodes[OP_CombatAbility] = &Client::Handle_OP_CombatAbility;
	ConnectedOpcodes[OP_Consent] = &Client::Handle_OP_Consent;
	ConnectedOpcodes[OP_Consider] = &Client::Handle_OP_Consider;
	ConnectedOpcodes[OP_ConsiderCorpse] = &Client::Handle_OP_ConsiderCorpse;
	ConnectedOpcodes[OP_Consume] = &Client::Handle_OP_Consume;
	ConnectedOpcodes[OP_ControlBoat] = &Client::Handle_OP_ControlBoat;
	ConnectedOpcodes[OP_CorpseDrag] = &Client::Handle_OP_CorpseDrag;
	ConnectedOpcodes[OP_Damage] = &Client::Handle_OP_Damage;
	ConnectedOpcodes[OP_Death] = &Client::Handle_OP_Death;
	ConnectedOpcodes[OP_DeleteCharge] = &Client::Handle_OP_DeleteCharge;
	ConnectedOpcodes[OP_DeleteSpawn] = &Client::Handle_OP_DeleteSpawn;
	ConnectedOpcodes[OP_DeleteSpell] = &Client::Handle_OP_DeleteSpell;
	ConnectedOpcodes[OP_DisarmTraps] = &Client::Handle_OP_DisarmTraps;
	ConnectedOpcodes[OP_Discipline] = &Client::Handle_OP_Discipline;
	ConnectedOpcodes[OP_DuelResponse] = &Client::Handle_OP_DuelResponse;
	ConnectedOpcodes[OP_DuelResponse2] = &Client::Handle_OP_DuelResponse2;
	ConnectedOpcodes[OP_Emote] = &Client::Handle_OP_Emote;
	ConnectedOpcodes[OP_EndLootRequest] = &Client::Handle_OP_EndLootRequest;
	ConnectedOpcodes[OP_EnvDamage] = &Client::Handle_OP_EnvDamage;
	ConnectedOpcodes[OP_FaceChange] = &Client::Handle_OP_FaceChange;
	ConnectedOpcodes[OP_FeignDeath] = &Client::Handle_OP_FeignDeath;
	ConnectedOpcodes[OP_Fishing] = &Client::Handle_OP_Fishing;
	ConnectedOpcodes[OP_Forage] = &Client::Handle_OP_Forage;
	ConnectedOpcodes[OP_FriendsWho] = &Client::Handle_OP_FriendsWho;
	ConnectedOpcodes[OP_GetGuildsList] = &Client::Handle_OP_GetGuildsList;
	ConnectedOpcodes[OP_GMBecomeNPC] = &Client::Handle_OP_GMBecomeNPC;
	ConnectedOpcodes[OP_GMDelCorpse] = &Client::Handle_OP_GMDelCorpse;
	ConnectedOpcodes[OP_GMEmoteZone] = &Client::Handle_OP_GMEmoteZone;
	ConnectedOpcodes[OP_GMEndTraining] = &Client::Handle_OP_GMEndTraining;
	ConnectedOpcodes[OP_GMFind] = &Client::Handle_OP_GMFind;
	ConnectedOpcodes[OP_GMGoto] = &Client::Handle_OP_GMGoto;
	ConnectedOpcodes[OP_GMHideMe] = &Client::Handle_OP_GMHideMe;
	ConnectedOpcodes[OP_GMKick] = &Client::Handle_OP_GMKick;
	ConnectedOpcodes[OP_GMKill] = &Client::Handle_OP_GMKill;
	ConnectedOpcodes[OP_GMLastName] = &Client::Handle_OP_GMLastName;
	ConnectedOpcodes[OP_GMNameChange] = &Client::Handle_OP_GMNameChange;
	ConnectedOpcodes[OP_GMSearchCorpse] = &Client::Handle_OP_GMSearchCorpse;
	ConnectedOpcodes[OP_GMServers] = &Client::Handle_OP_GMServers;
	ConnectedOpcodes[OP_GMSummon] = &Client::Handle_OP_GMSummon;
	ConnectedOpcodes[OP_GMToggle] = &Client::Handle_OP_GMToggle;
	ConnectedOpcodes[OP_GMTraining] = &Client::Handle_OP_GMTraining;
	ConnectedOpcodes[OP_GMTrainSkill] = &Client::Handle_OP_GMTrainSkill;
	ConnectedOpcodes[OP_GMZoneRequest] = &Client::Handle_OP_GMZoneRequest;
	ConnectedOpcodes[OP_GMZoneRequest2] = &Client::Handle_OP_GMZoneRequest2;
	ConnectedOpcodes[OP_GroundSpawn] = &Client::Handle_OP_CreateObject;
	ConnectedOpcodes[OP_GroupCancelInvite] = &Client::Handle_OP_GroupCancelInvite;
	ConnectedOpcodes[OP_GroupDisband] = &Client::Handle_OP_GroupDisband;
	ConnectedOpcodes[OP_GroupFollow] = &Client::Handle_OP_GroupFollow;
	ConnectedOpcodes[OP_GroupInvite] = &Client::Handle_OP_GroupInvite;
	ConnectedOpcodes[OP_GroupInvite2] = &Client::Handle_OP_GroupInvite2;
	ConnectedOpcodes[OP_GroupUpdate] = &Client::Handle_OP_GroupUpdate;
	ConnectedOpcodes[OP_GuildDelete] = &Client::Handle_OP_GuildDelete;
	ConnectedOpcodes[OP_GuildInvite] = &Client::Handle_OP_GuildInvite;
	ConnectedOpcodes[OP_GuildInviteAccept] = &Client::Handle_OP_GuildInviteAccept;
	ConnectedOpcodes[OP_GuildLeader] = &Client::Handle_OP_GuildLeader;
	ConnectedOpcodes[OP_GuildPeace] = &Client::Handle_OP_GuildPeace;
	ConnectedOpcodes[OP_GuildRemove] = &Client::Handle_OP_GuildRemove;
	ConnectedOpcodes[OP_GuildWar] = &Client::Handle_OP_GuildWar;
	ConnectedOpcodes[OP_Hide] = &Client::Handle_OP_Hide;
	ConnectedOpcodes[OP_Illusion] = &Client::Handle_OP_Illusion;
	ConnectedOpcodes[OP_InspectAnswer] = &Client::Handle_OP_InspectAnswer;
	ConnectedOpcodes[OP_InspectRequest] = &Client::Handle_OP_InspectRequest;
	ConnectedOpcodes[OP_InstillDoubt] = &Client::Handle_OP_InstillDoubt;
	ConnectedOpcodes[OP_ItemLinkResponse] = &Client::Handle_OP_ItemLinkResponse;
	ConnectedOpcodes[OP_Jump] = &Client::Handle_OP_Jump;
	ConnectedOpcodes[OP_LeaveBoat] = &Client::Handle_OP_LeaveBoat;
	ConnectedOpcodes[OP_Logout] = &Client::Handle_OP_Logout;
	ConnectedOpcodes[OP_LootItem] = &Client::Handle_OP_LootItem;
	ConnectedOpcodes[OP_LootRequest] = &Client::Handle_OP_LootRequest;
	ConnectedOpcodes[OP_ManaChange] = &Client::Handle_OP_ManaChange;
	ConnectedOpcodes[OP_MemorizeSpell] = &Client::Handle_OP_MemorizeSpell;
	ConnectedOpcodes[OP_Mend] = &Client::Handle_OP_Mend;
	ConnectedOpcodes[OP_MoveCoin] = &Client::Handle_OP_MoveCoin;
	ConnectedOpcodes[OP_MoveItem] = &Client::Handle_OP_MoveItem;
	ConnectedOpcodes[OP_PetCommands] = &Client::Handle_OP_PetCommands;
	ConnectedOpcodes[OP_Petition] = &Client::Handle_OP_Petition;
	ConnectedOpcodes[OP_PetitionCheckIn] = &Client::Handle_OP_PetitionCheckIn;
	ConnectedOpcodes[OP_PetitionCheckout] = &Client::Handle_OP_PetitionCheckout;
	ConnectedOpcodes[OP_PetitionDelete] = &Client::Handle_OP_PetitionDelete;
	ConnectedOpcodes[OP_PetitionRefresh] = &Client::Handle_OP_PetitionRefresh;
	ConnectedOpcodes[OP_PickPocket] = &Client::Handle_OP_PickPocket;
	ConnectedOpcodes[OP_RaidInvite] = &Client::Handle_OP_RaidCommand;
	ConnectedOpcodes[OP_RandomReq] = &Client::Handle_OP_RandomReq;
	ConnectedOpcodes[OP_ReadBook] = &Client::Handle_OP_ReadBook;
	ConnectedOpcodes[OP_Report] = &Client::Handle_OP_Report;
	ConnectedOpcodes[OP_RequestDuel] = &Client::Handle_OP_RequestDuel;
	ConnectedOpcodes[OP_RezzAnswer] = &Client::Handle_OP_RezzAnswer;
	ConnectedOpcodes[OP_Sacrifice] = &Client::Handle_OP_Sacrifice;
	ConnectedOpcodes[OP_SafeFallSuccess] = &Client::Handle_OP_SafeFallSuccess;
	ConnectedOpcodes[OP_SafePoint] = &Client::Handle_OP_SafePoint;
	ConnectedOpcodes[OP_Save] = &Client::Handle_OP_Save;
	ConnectedOpcodes[OP_SaveOnZoneReq] = &Client::Handle_OP_SaveOnZoneReq;
	ConnectedOpcodes[OP_SenseHeading] = &Client::Handle_OP_SenseHeading;
	ConnectedOpcodes[OP_SenseTraps] = &Client::Handle_OP_SenseTraps;
	ConnectedOpcodes[OP_SetGuildMOTD] = &Client::Handle_OP_SetGuildMOTD;
	ConnectedOpcodes[OP_SetRunMode] = &Client::Handle_OP_SetRunMode;
	ConnectedOpcodes[OP_SetServerFilter] = &Client::Handle_OP_SetServerFilter;
	ConnectedOpcodes[OP_SetTitle] = &Client::Handle_OP_SetTitle;
	ConnectedOpcodes[OP_Shielding] = &Client::Handle_OP_Shielding;
	ConnectedOpcodes[OP_ShopEnd] = &Client::Handle_OP_ShopEnd;
	ConnectedOpcodes[OP_ShopPlayerBuy] = &Client::Handle_OP_ShopPlayerBuy;
	ConnectedOpcodes[OP_ShopPlayerSell] = &Client::Handle_OP_ShopPlayerSell;
	ConnectedOpcodes[OP_ShopRequest] = &Client::Handle_OP_ShopRequest;
	ConnectedOpcodes[OP_Sneak] = &Client::Handle_OP_Sneak;
	ConnectedOpcodes[OP_SpawnAppearance] = &Client::Handle_OP_SpawnAppearance;
	ConnectedOpcodes[OP_Split] = &Client::Handle_OP_Split;
	ConnectedOpcodes[OP_Surname] = &Client::Handle_OP_Surname;
	ConnectedOpcodes[OP_SwapSpell] = &Client::Handle_OP_SwapSpell;
	ConnectedOpcodes[OP_TargetCommand] = &Client::Handle_OP_TargetCommand;
	ConnectedOpcodes[OP_TargetMouse] = &Client::Handle_OP_TargetMouse;
	ConnectedOpcodes[OP_Taunt] = &Client::Handle_OP_Taunt;
	ConnectedOpcodes[OP_TGB] = &Client::Handle_OP_TGB;
	ConnectedOpcodes[OP_Track] = &Client::Handle_OP_Track;
	ConnectedOpcodes[OP_TradeAcceptClick] = &Client::Handle_OP_TradeAcceptClick;
	ConnectedOpcodes[OP_Trader] = &Client::Handle_OP_Trader;
	ConnectedOpcodes[OP_TraderBuy] = &Client::Handle_OP_TraderBuy;
	ConnectedOpcodes[OP_TradeRequest] = &Client::Handle_OP_TradeRequest;
	ConnectedOpcodes[OP_TradeRequestAck] = &Client::Handle_OP_TradeRequestAck;
	ConnectedOpcodes[OP_TraderShop] = &Client::Handle_OP_TraderShop;
	ConnectedOpcodes[OP_TradeSkillCombine] = &Client::Handle_OP_TradeSkillCombine;
	ConnectedOpcodes[OP_Translocate] = &Client::Handle_OP_Translocate;
	ConnectedOpcodes[OP_WearChange] = &Client::Handle_OP_WearChange;
	ConnectedOpcodes[OP_WhoAllRequest] = &Client::Handle_OP_WhoAllRequest;
	ConnectedOpcodes[OP_YellForHelp] = &Client::Handle_OP_YellForHelp;
	ConnectedOpcodes[OP_ZoneChange] = &Client::Handle_OP_ZoneChange;
	ConnectedOpcodes[OP_ZoneEntryResend] = &Client::Handle_OP_ZoneEntryResend;
	ConnectedOpcodes[OP_LFGCommand] = &Client::Handle_OP_LFGCommand;
	ConnectedOpcodes[OP_Disarm] = &Client::Handle_OP_Disarm;
	ConnectedOpcodes[OP_Feedback] = &Client::Handle_OP_Feedback;
	ConnectedOpcodes[OP_SoulMarkUpdate] = &Client::Handle_OP_SoulMarkUpdate;
	ConnectedOpcodes[OP_SoulMarkList] = &Client::Handle_OP_SoulMarkList;
	ConnectedOpcodes[OP_SoulMarkAdd] = &Client::Handle_OP_SoulMarkAdd;
	ConnectedOpcodes[OP_MBRetrievalRequest] = &Client::Handle_OP_MBRetrievalRequest;
	ConnectedOpcodes[OP_MBRetrievalDetailRequest] = &Client::Handle_OP_MBRetrievalDetailRequest;
	ConnectedOpcodes[OP_MBRetrievalPostRequest] = &Client::Handle_OP_MBRetrievalPostRequest;
	ConnectedOpcodes[OP_MBRetrievalEraseRequest] = &Client::Handle_OP_MBRetrievalEraseRequest;
	ConnectedOpcodes[OP_Key] = &Client::Handle_OP_Key;
	ConnectedOpcodes[OP_TradeRefused] = &Client::Handle_OP_TradeRefused;
}

void ClearMappedOpcode(EmuOpcode op)
{
	if (op >= _maxEmuOpcode)
		return;

	ConnectedOpcodes[op] = nullptr;
	auto iter = ConnectingOpcodes.find(op);
	if (iter != ConnectingOpcodes.end()) {
		ConnectingOpcodes.erase(iter);
	}
}

// client methods
int Client::HandlePacket(const EQApplicationPacket *app)
{
	if (Log.log_settings[Logs::LogCategory::Netcode].is_category_enabled == 1) {
		char buffer[64];
		app->build_header_dump(buffer);
		Log.Out(Logs::Detail, Logs::Client_Server_Packet, "Dispatch opcode: %s", buffer);
	}

	if (Log.log_settings[Logs::Client_Server_Packet].is_category_enabled == 1)
		if (!RuleB(EventLog, SkipCommonPacketLogging) ||
			(RuleB(EventLog, SkipCommonPacketLogging) && app->GetOpcode() != OP_MobHealth && app->GetOpcode() != OP_MobUpdate && app->GetOpcode() != OP_ClientUpdate)){
			Log.Out(Logs::General, Logs::Client_Server_Packet, "[%s - 0x%04x] [Size: %u]", OpcodeManager::EmuToName(app->GetOpcode()), app->GetOpcode(), app->Size()-2);
		}
	
	if (Log.log_settings[Logs::Client_Server_Packet_With_Dump].is_category_enabled == 1)
		if (!RuleB(EventLog, SkipCommonPacketLogging) ||
			(RuleB(EventLog, SkipCommonPacketLogging) && app->GetOpcode() != OP_MobHealth && app->GetOpcode() != OP_MobUpdate && app->GetOpcode() != OP_ClientUpdate)){
			Log.Out(Logs::General, Logs::Client_Server_Packet_With_Dump, "[%s - 0x%04x] [Size: %u] %s", OpcodeManager::EmuToName(app->GetOpcode()), app->GetOpcode(), app->Size()-2, DumpPacketToString(app).c_str());
		}
	
	EmuOpcode opcode = app->GetOpcode();

	#if EQDEBUG >= 11
		std::cout << "Received 0x" << std::hex << std::setw(4) << std::setfill('0') << opcode << ", size=" << std::dec << app->size << std::endl;
	#endif

	#ifdef SOLAR
		if(0 && opcode != OP_ClientUpdate)
		{
			Log.LogDebug(Logs::General,"HandlePacket() OPCODE debug enabled client %s", GetName());
			std::cerr << "OPCODE: " << std::hex << std::setw(4) << std::setfill('0') << opcode << std::dec << ", size: " << app->size << std::endl;
			DumpPacket(app);
		}
	#endif

	switch(client_state) {
	case CLIENT_CONNECTING: {
		if(ConnectingOpcodes.count(opcode) != 1) {
			//Hate const cast but everything in lua needs to be non-const even if i make it non-mutable
			std::vector<EQEmu::Any> args;
			args.push_back(const_cast<EQApplicationPacket*>(app));
			parse->EventPlayer(EVENT_UNHANDLED_OPCODE, this, "", 1, &args);

#if EQDEBUG >= 10
			Log.Out(Logs::General, Logs::Error, "HandlePacket() Opcode error: Unexpected packet during CLIENT_CONNECTING: opcode:"
				" %s (#%d eq=0x%04x), size: %i", OpcodeNames[opcode], opcode, 0, app->size);
			//Log.Out(Logs::General, Logs::Error,"Received unknown EQApplicationPacket during CLIENT_CONNECTING");
			//DumpPacket(app);
#endif
			break;
		}

		ClientPacketProc p;
		p = ConnectingOpcodes[opcode];

		//call the processing routine
		(this->*p)(app);

		//special case where connecting code needs to boot client...
		if(client_state == CLIENT_KICKED) {
			return(false);
		}

		break;
	}
	case CLIENT_CONNECTED: {
		ClientPacketProc p;
		p = ConnectedOpcodes[opcode];
		if(p == nullptr) { 
			std::vector<EQEmu::Any> args;
			args.push_back(const_cast<EQApplicationPacket*>(app));
			parse->EventPlayer(EVENT_UNHANDLED_OPCODE, this, "", 0, &args);

			if (Log.log_settings[Logs::Client_Server_Packet_Unhandled].is_category_enabled == 1){
				char buffer[64];
				app->build_header_dump(buffer);
				Log.Out(Logs::General, Logs::Client_Server_Packet_Unhandled, "%s %s", buffer, DumpPacketToString(app).c_str());
			}
			break;
		}

		//call the processing routine
		(this->*p)(app);
		break;
	}
	case CLIENT_KICKED:
	case DISCONNECTED:
	case CLIENT_LINKDEAD:
	case PREDISCONNECTED:
	case ZONING:
		break;
	default:
		Log.Out(Logs::General, Logs::None, "Unknown client_state: %d\n", client_state);
		break;
	}

	return(true);
}

// Finish client connecting state
void Client::CompleteConnect()
{
	UpdateWho();
	client_state = CLIENT_CONNECTED;

	hpupdate_timer.Start();
	position_timer.Start();
	autosave_timer.Start();
	position_update_timer.Start();
	client_distance_timer.Start(2000, false);
	entity_list.UpdateNewClientDistances(this);
	SetDuelTarget(0);
	SetDueling(false);

	EnteringMessages(this);
	ZoneFlags.Clear();
	LoadZoneFlags(&ZoneFlags);
	consent_list.clear();
	LoadCharacterConsent();

	/* Sets GM Flag if needed & Sends Petition Queue */
	UpdateAdmin(false);

	for (uint32 spellInt = 0; spellInt < MAX_PP_SPELLBOOK; spellInt++)
	{
		if (m_pp.spell_book[spellInt] < 3 || m_pp.spell_book[spellInt] > 50000)
			m_pp.spell_book[spellInt] = 0xFFFFFFFF;
	}

	if (GetGM() && (GetHideMe() || GetGMSpeed() || GetGMInvul() || flymode != 0 || tellsoff))
	{
		std::string state = "currently ";

		if (GetHideMe()) state += "hidden to all clients, ";
		if (GetGMSpeed()) state += "running at GM speed, ";
		if (GetGMInvul()) state += "invulnerable to all damage, ";
		if (flymode == 1) state += "flying, ";
		else if (flymode == 2) state += "levitating, ";
		if (tellsoff) state += "ignoring tells, ";

		if (state.size () > 0) 
		{
			//Remove last two characters from the string
			state.resize (state.size () - 2);
			Message(CC_Red, "[GM] You are %s.", state.c_str());
		}
	}

	uint32 raidid = database.GetRaidID(GetName());
	Raid *raid = nullptr;
	if (raidid > 0){
		raid = entity_list.GetRaidByID(raidid);
		if (!raid){
			raid = new Raid(raidid);
			if (raid->GetID() != 0){
				entity_list.AddRaid(raid, raidid);
			}
			else
				raid = nullptr;
		}
		if (raid){
			SetRaidGrouped(true);
			raid->LearnMembers();
			raid->VerifyRaid();
			raid->GetRaidDetails();
			/*
			Only leader should get this; send to all for now till
			I figure out correct creation; can probably also send a no longer leader packet for non leaders
			but not important for now.
			*/
			raid->SendRaidCreate(this);
			raid->SendMakeLeaderPacketTo(raid->leadername, this);
			raid->SendRaidAdd(GetName(), this);
			raid->SendBulkRaid(this);
			raid->SendGroupUpdate(this);
			uint32 grpID = raid->GetGroup(GetName());
			if (grpID < 12){
				raid->SendRaidGroupRemove(GetName(), grpID);
				raid->SendRaidGroupAdd(GetName(), grpID);
			}
			if (raid->IsLocked())
				raid->SendRaidLockTo(this);
		}
	}

	//bulk raid send in here eventually

	//reapply some buffs
	uint32 buff_count = GetMaxTotalSlots();
	for (uint32 j1 = 0; j1 < buff_count; j1++) {
		if (!IsValidSpell(buffs[j1].spellid))
			continue;

		const SPDat_Spell_Struct &spell = spells[buffs[j1].spellid];

		for (int x1 = 0; x1 < EFFECT_COUNT; x1++) {
			switch (spell.effectid[x1]) {
			case SE_IllusionCopy:
			case SE_Illusion: {
				if (spell.base[x1] == -1) {
					if (gender == 1)
						gender = 0;
					else if (gender == 0)
						gender = 1;
					SendIllusionPacket(GetRace(), gender, 0xFF, 0xFF);
				}
				else if (spell.base[x1] == -2)
				{
					if (IsPlayableRace(GetRace()))
						SendIllusionPacket(GetRace(), GetGender(), spell.max[x1], spell.max[x1]);
				}
				else if (spell.max[x1] > 0)
				{
					SendIllusionPacket(spell.base[x1], 0xFF, spell.max[x1], spell.max[x1]);
				}
				else
				{
					SendIllusionPacket(spell.base[x1], 0xFF, 0xFF, 0xFF);
				}

				float realsize = GetPlayerHeight(spell.base[x1]);
				uint32 newsize = 6;
				newsize = floor(realsize + 0.5);
				SendAppearancePacket(AT_Size, newsize);
		
				break;
			}
			case SE_SummonHorse: {
				SummonHorse(buffs[j1].spellid);
				//hasmount = true;	//this was false, is that the correct thing?
				break;
			}
			case SE_Silence:
			{
				Silence(true);
				break;
			}
			case SE_Amnesia:
			{
				Amnesia(true);
				break;
			}
			case SE_DivineAura:
			{
				invulnerable = true;
				break;
			}
			case SE_Invisibility2:
			case SE_Invisibility:
			{
				invisible = true;
				SendAppearancePacket(AT_Invis, 1);
				break;
			}
			case SE_Levitate:
			{
				if (!zone->CanLevitate())
				{
					if (!GetGM())
					{
						SendAppearancePacket(AT_Levitate, 0);
						BuffFadeByEffect(SE_Levitate);
						Message(CC_Red, "You can't levitate in this zone.");
					}
				}
				else{
					SendAppearancePacket(AT_Levitate, 2);
				}
				break;
			}
			case SE_InvisVsUndead2:
			case SE_InvisVsUndead:
			{
				invisible_undead = true;
				break;
			}
			case SE_InvisVsAnimals:
			{
				invisible_animals = true;
				break;
			}
			case SE_AddMeleeProc:
			case SE_WeaponProc:
			{
				AddProcToWeapon(GetProcID(buffs[j1].spellid, x1), false, 100 + spells[buffs[j1].spellid].base2[x1], buffs[j1].spellid);
				break;
			}
			}
		}
	}

	// Disciplines don't survive zoning.
	if(GetActiveDisc() != 0)
		FadeDisc();

	/* Sends appearances for all mobs not doing anim_stand aka sitting, looting, playing dead */
	entity_list.SendZoneAppearance(this);

	client_data_loaded = true;
	int x;
	for (x = 0; x < 8; x++) {
		SendWearChange(x);
	}
	// added due to wear change above
	UpdateActiveLight();
	SendAppearancePacket(AT_Light, GetActiveLightType());

	Mob *pet = GetPet();
	if (pet != nullptr) {
		for (x = 0; x < 8; x++) {
			pet->SendWearChange(x);
		}
		// added due to wear change above
		pet->UpdateActiveLight();
		pet->SendAppearancePacket(AT_Light, pet->GetActiveLightType());
	}

	entity_list.SendTraders(this);

	zoneinpacket_timer.Start();

	conn_state = ClientConnectFinished;
	database.SetAccountActive(AccountID());

	if (GetGroup())
	{
		database.RefreshGroupFromDB(this);
	}

	//enforce some rules..
	if (!CanBeInZone()) {
		Log.Out(Logs::Detail, Logs::None, "[CLIENT] Kicking char from zone, not allowed here");
		GoToSafeCoords(database.GetZoneID("arena"), 0);
		return;
	}

	if (zone)
		zone->weatherSend();

	TotalKarma = database.GetKarma(AccountID());

	parse->EventPlayer(EVENT_ENTER_ZONE, this, "", 0);

	/* This sub event is for if a player logs in for the first time since entering world. */
	if (firstlogon == 1){
		parse->EventPlayer(EVENT_CONNECT, this, "", 0);
		/* QS: PlayerLogConnectDisconnect */
		if (RuleB(QueryServ, PlayerLogConnectDisconnect)){
			std::string event_desc = StringFormat("Connect :: Logged into zoneid:%i instid:%i", this->GetZoneID(), this->GetInstanceID());
			QServ->PlayerLogEvent(Player_Log_Connect_State, this->CharacterID(), event_desc);
		}
	}

	CalcItemScale();
	DoItemEnterZone();

	if (IsInAGuild()) {
		guild_mgr.RequestOnlineGuildMembers(this->CharacterID(), this->GuildID());
	}

	SendStaminaUpdate();
	SendClientVersion();
	FixClientXP();
	SendToBoat(true);
	worldserver.RequestTellQueue(GetName());
}


void Client::CheatDetected(CheatTypes CheatType, float x, float y, float z)
{ 
	//ToDo: Break warp down for special zones. Some zones have special teleportation pads or bad .map files which can trigger the detector without a legit zone request.

	switch (CheatType)
	{
	case MQWarp: //Some zones may still have issues. Database updates will eliminate most if not all problems.
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			if (GetBoatNPCID() == 0)
			{
				Message(CC_Red, "Large warp detected.");
				char hString[250];
				sprintf(hString, "/MQWarp with location %.2f, %.2f, %.2f", GetX(), GetY(), GetZ());
				database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			}
		}
		break;
	case MQWarpShadowStep:
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			char *hString = nullptr;
			MakeAnyLenString(&hString, "/MQWarp(SS) with location %.2f, %.2f, %.2f, the target was shadow step exempt but we still found this suspicious.", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			safe_delete_array(hString);
		}
		break;
	case MQWarpKnockBack:
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			char *hString = nullptr;
			MakeAnyLenString(&hString, "/MQWarp(KB) with location %.2f, %.2f, %.2f, the target was Knock Back exempt but we still found this suspicious.", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			safe_delete_array(hString);
		}
		break;

	case MQWarpLight:
		if (RuleB(Zone, EnableMQWarpDetector)
			&& ((this->Admin() < RuleI(Zone, MQWarpExemptStatus)
			|| (RuleI(Zone, MQWarpExemptStatus)) == -1)))
		{
			if (RuleB(Zone, MarkMQWarpLT))
			{
				char *hString = nullptr;
				MakeAnyLenString(&hString, "/MQWarp(LT) with location %.2f, %.2f, %.2f, running fast but not fast enough to get killed, possibly: small warp, speed hack, excessive lag, marked as suspicious.", GetX(), GetY(), GetZ());
				database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
				safe_delete_array(hString);
			}
		}
		break;

	case MQZone:
		if (RuleB(Zone, EnableMQZoneDetector) && ((this->Admin() < RuleI(Zone, MQZoneExemptStatus) || (RuleI(Zone, MQZoneExemptStatus)) == -1)))
		{
			char hString[250];
			sprintf(hString, "/MQZone used at %.2f, %.2f, %.2f to %.2f %.2f %.2f", GetX(), GetY(), GetZ(), x, y, z);
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		}
		break;
	case MQZoneUnknownDest:
		if (RuleB(Zone, EnableMQZoneDetector) && ((this->Admin() < RuleI(Zone, MQZoneExemptStatus) || (RuleI(Zone, MQZoneExemptStatus)) == -1)))
		{
			char hString[250];
			sprintf(hString, "/MQZone used at %.2f, %.2f, %.2f", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		}
		break;
	case MQGate:
		if (RuleB(Zone, EnableMQGateDetector) && ((this->Admin() < RuleI(Zone, MQGateExemptStatus) || (RuleI(Zone, MQGateExemptStatus)) == -1))) {
			Message(CC_Red, "Illegal gate request.");
			char hString[250];
			sprintf(hString, "/MQGate used at %.2f, %.2f, %.2f", GetX(), GetY(), GetZ());
			database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
			if (zone)
			{
				this->SetZone(this->GetZoneID(), zone->GetInstanceID()); //Prevent the player from zoning, place him back in the zone where he tried to originally /gate.
			}
			else
			{
				this->SetZone(this->GetZoneID(), 0); //Prevent the player from zoning, place him back in the zone where he tried to originally /gate.

			}
		}
		break;
	case MQGhost: //Not currently implemented, but the framework is in place - just needs detection scenarios identified
		if (RuleB(Zone, EnableMQGhostDetector) && ((this->Admin() < RuleI(Zone, MQGhostExemptStatus) || (RuleI(Zone, MQGhostExemptStatus)) == -1))) {
			database.SetMQDetectionFlag(this->account_name, this->name, "/MQGhost", zone->GetShortName());
		}
		break;
	default:
		char *hString = nullptr;
		MakeAnyLenString(&hString, "Unhandled HackerDetection flag with location %.2f, %.2f, %.2f.", GetX(), GetY(), GetZ());
		database.SetMQDetectionFlag(this->account_name, this->name, hString, zone->GetShortName());
		safe_delete_array(hString);
		break;
	}
}

void Client::Handle_Connect_OP_ClientError(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClientError_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size on OP_ClientError: Expected %i, Got %i",
			sizeof(ClientError_Struct), app->size);
		return;
	}

	return;
}

void Client::Handle_Connect_OP_ClientUpdate(const EQApplicationPacket *app)
{
	conn_state = ClientReadyReceived;

	CompleteConnect();
	SendHPUpdate();
}

void Client::Handle_Connect_OP_ReqClientSpawn(const EQApplicationPacket *app)
{
	conn_state = ClientSpawnRequested;
	if (GetClientVersion() != EQClientTrilogy)
	{
		EQApplicationPacket* outapp = new EQApplicationPacket;
		uint8 count = 0;
		if (entity_list.SendZoneDoorsBulk(outapp, this, count))
		{
			QueuePacket(outapp);
			if (count > 1)
				safe_delete(outapp);
		}

		entity_list.SendZoneObjects(this);
		outapp = new EQApplicationPacket(OP_SendExpZonein, 0);
		outapp->priority = 6;
		FastQueuePacket(&outapp);
	}
	SendZonePoints();

	conn_state = ZoneContentsSent;

	return;
}

void Client::Handle_Connect_OP_ReqNewZone(const EQApplicationPacket *app)
{
	conn_state = NewZoneRequested;

	EQApplicationPacket* outapp;

	/////////////////////////////////////
	// New Zone Packet
	outapp = new EQApplicationPacket(OP_NewZone, sizeof(NewZone_Struct));
	NewZone_Struct* nz = (NewZone_Struct*)outapp->pBuffer;
	memcpy(outapp->pBuffer, &zone->newzone_data, sizeof(NewZone_Struct));
	strcpy(nz->char_name, m_pp.name);
	Log.Out(Logs::Detail, Logs::Zone_Server, "NewZone data for %s (%i) successfully sent.", zone->newzone_data.zone_short_name, zone->newzone_data.zone_id);

	FastQueuePacket(&outapp);
	
	if (GetClientVersionBit() & EQClientTrilogy) {
		outapp = new EQApplicationPacket(OP_SendExpZonein, 0);
		outapp->priority = 6;
		FastQueuePacket(&outapp);
		entity_list.SendZoneSpawnsBulk(this);
		entity_list.SendZoneCorpsesBulk(this);
		outapp = new EQApplicationPacket;
		uint8 count = 0;
		if (entity_list.SendZoneDoorsBulkTrilogy(outapp, this, count))
		{
			QueuePacket(outapp);
			if (count > 1)
				safe_delete(outapp);
		}

		entity_list.SendZoneObjects(this);
	}
	return;
}

void Client::Handle_Connect_OP_SendExpZonein(const EQApplicationPacket *app)
{
	//////////////////////////////////////////////////////
	// Spawn Appearance Packet
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)outapp->pBuffer;
	sa->type = AT_SpawnID;			// Is 0x10 used to set the player id?
	sa->parameter = GetID();	// Four bytes for this parameter...
	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	// Inform the world about the client
	outapp = new EQApplicationPacket();

	CreateSpawnPacket(outapp);
	outapp->priority = 6;
	if(GetHideMe())
		entity_list.QueueClientsStatus(this, outapp, true, Admin(), 255);
	else
		entity_list.QueueClients(this, outapp, true);
	safe_delete(outapp);

	if (GetClientVersion() == EQClientTrilogy) {
		outapp = new EQApplicationPacket(OP_LogServer, sizeof(LogServer_Struct));
		QueuePacket(outapp);
		safe_delete(outapp);
		SendHPUpdate();
		SendManaUpdatePacket();
		Log.Out(Logs::Detail, Logs::Zone_Server, "Detected as Trilogy - Sending OP_LogServer");
	}

	if (GetPVP())	//force a PVP update until we fix the spawn struct
		SendAppearancePacket(AT_PVP, GetPVP(), true, false);

	//Send AA Exp packet:
	if (GetLevel() >= 51)
	{
		SendAAStats();
	}

	// Send exp packets
	outapp = new EQApplicationPacket(OP_ExpUpdate, sizeof(ExpUpdate_Struct));
	ExpUpdate_Struct* eu = (ExpUpdate_Struct*)outapp->pBuffer;
	uint32 tmpxp1 = GetEXPForLevel(GetLevel() + 1);
	uint32 tmpxp2 = GetEXPForLevel(GetLevel());

	// Crash bug fix... Divide by zero when tmpxp1 and 2 equalled each other, most likely the error case from GetEXPForLevel() (invalid class, etc)
	if (tmpxp1 != tmpxp2 && tmpxp1 != 0xFFFFFFFF && tmpxp2 != 0xFFFFFFFF) {
		float tmpxp = (float)((float)m_pp.exp - tmpxp2) / ((float)tmpxp1 - tmpxp2);
		eu->exp = (uint32)(330.0f * tmpxp);
		outapp->priority = 6;
		QueuePacket(outapp);
	}
	safe_delete(outapp);

	if (GetLevel() >= 51)
	{
		SendAATimers();
	}

	outapp = new EQApplicationPacket(OP_SendExpZonein, 0);
	FastQueuePacket(&outapp);

	outapp = new EQApplicationPacket(OP_ZoneInAvatarSet, 1);
	QueuePacket(outapp);
	safe_delete(outapp);

	outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(ZoneInSendName_Struct));
	ZoneInSendName_Struct* zonesendname = (ZoneInSendName_Struct*)outapp->pBuffer;
	strcpy(zonesendname->name, m_pp.name);
	strcpy(zonesendname->name2, m_pp.name);
	zonesendname->unknown0 = 0x0A;
	QueuePacket(outapp);
	safe_delete(outapp);
	SendGuildMOTD();
	SendCursorItems();
	return;
}

void Client::Handle_Connect_OP_SetDataRate(const EQApplicationPacket *app)
{
	//Just ignore and prepare for ZoneEntry next for now.
	return;
}

void Client::Handle_Connect_OP_SetServerFilter(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SetServerFilter_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized OP_SetServerFilter");
		DumpPacket(app);
		return;
	}
	SetServerFilter_Struct* filter = (SetServerFilter_Struct*)app->pBuffer;
	ServerFilter(filter);
	return;
}

void Client::Handle_Connect_OP_SpawnAppearance(const EQApplicationPacket *app)
{
	return;
}

void Client::Handle_Connect_OP_TGB(const EQApplicationPacket *app)
{
	if (app->size != sizeof(uint32)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size on OP_TGB: Expected %i, Got %i",
			sizeof(uint32), app->size);
		return;
	}
	OPTGB(app);
	return;
}

void Client::Handle_Connect_OP_WearChange(const EQApplicationPacket *app)
{
	//not sure what these are supposed to mean to us.
	return;
}

void Client::Handle_Connect_OP_ZoneEntry(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClientZoneEntry_Struct))
		return;

	ClientZoneEntry_Struct *cze = (ClientZoneEntry_Struct *)app->pBuffer;

	conn_state = ReceivedZoneEntry;

	/* Antighost code
	tmp var is so the search doesnt find this object
	*/
	Client* client = entity_list.GetClientByName(cze->char_name);
	if (!zone->GetAuth(ip, cze->char_name, &WID, &account_id, &character_id, &admin, lskey, &tellsoff, &versionbit)) {
		Log.Out(Logs::General, Logs::Error, "GetAuth() returned false kicking client");
		if (client != 0) {
			client->Save();
			client->Kick();
		}
		//ret = false; // TODO: Can we tell the client to get lost in a good way
		client_state = CLIENT_KICKED;
		return;
	}

	ClientVersion = Connection()->ClientVersion();
	if(Connection()->ClientVersion() == EQClientMac)
	{
		ClientVersionBit = versionbit;
	}
	else
	{
		if(Connection()->ClientVersion() == EQClientTrilogy)
			ClientVersionBit = 1;
		else if(Connection()->ClientVersion() == EQClientEvolution)
			ClientVersionBit = 16;
		else
			ClientVersionBit = 0;
	}
	Log.Out(Logs::Detail, Logs::Zone_Server, "ClientVersionBit is: %i", ClientVersionBit);

	strcpy(name, cze->char_name);
	/* Check for Client Spoofing */
	if (client != 0) {
		struct in_addr ghost_addr;
		ghost_addr.s_addr = eqs->GetRemoteIP();

		Log.Out(Logs::General, Logs::Error, "Ghosting client: Account ID:%i Name:%s Character:%s IP:%s",
			client->AccountID(), client->AccountName(), client->GetName(), inet_ntoa(ghost_addr));
		client->Save();
		client->Disconnect();
	}

	uint32 pplen = 0;
	EQApplicationPacket* outapp = 0;
	MYSQL_RES* result = 0;
	bool loaditems = 0;
	uint32 i;
	std::string query;
	unsigned long* lengths;

	uint32 cid = CharacterID();
	character_id = cid; /* Global character_id reference */

	/* Flush and reload factions */
	database.RemoveTempFactions(this);
	database.LoadCharacterFactionValues(cid, factionvalues);

	/* Load Character Account Data: Temp until I move */
	query = StringFormat("SELECT `status`, `name`, `lsaccount_id`, `gmspeed`, `revoked`, `hideme`, `time_creation`, `gminvul`, `flymode`, `ignore_tells` FROM `account` WHERE `id` = %u", this->AccountID());
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		admin = atoi(row[0]);
		strncpy(account_name, row[1], 30);
		lsaccountid = atoi(row[2]);
		gmspeed = atoi(row[3]);
		revoked = atoi(row[4]);
		gmhideme = atoi(row[5]);
		account_creation = atoul(row[6]);
		gminvul = atoi(row[7]);
		flymode = atoi(row[8]);
		tellsoff = atoi(row[9]);
	}

	/* Load Character Data */
	query = StringFormat("SELECT `firstlogon`, `guild_id`, `rank` FROM `character_data` LEFT JOIN `guild_members` ON `id` = `char_id` WHERE `id` = %i", cid);
	results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {		
		if (row[1] && atoi(row[1]) > 0){
			guild_id = atoi(row[1]);
			if (row[2] != nullptr){ guildrank = atoi(row[2]); }
			else{ guildrank = GUILD_RANK_NONE; }
		}

		if (firstlogon){ firstlogon = atoi(row[0]); }
	}

	/* Do not write to the PP prior to this otherwise it will just be overwritten when it's loaded from the DB */
	loaditems = database.GetInventory(cid, &m_inv); /* Load Character Inventory */
	database.LoadCharacterBindPoint(cid, &m_pp); /* Load Character Bind */
	database.LoadCharacterMaterialColor(cid, &m_pp); /* Load Character Material */
	database.LoadCharacterCurrency(cid, &m_pp); /* Load Character Currency into PP */
	database.LoadCharacterData(cid, &m_pp, &m_epp); /* Load Character Data from DB into PP as well as E_PP */
	database.LoadCharacterSkills(cid, &m_pp); /* Load Character Skills */
	database.LoadCharacterSpellBook(cid, &m_pp); /* Load Character Spell Book */
	database.LoadCharacterMemmedSpells(cid, &m_pp);  /* Load Character Memorized Spells */
	database.LoadCharacterLanguages(cid, &m_pp); /* Load Character Languages */

	/* Set item material tint */
	for (int i = EmuConstants::MATERIAL_BEGIN; i <= EmuConstants::MATERIAL_END; i++)
		if (m_pp.item_tint[i].rgb.use_tint == 1 || m_pp.item_tint[i].rgb.use_tint == 255)
			m_pp.item_tint[i].rgb.use_tint = 0xFF;

	if (level){ level = m_pp.level; }

	/* If GM, not trackable */
	if (gmhideme) { trackable = false; }
	if (gminvul) { invulnerable = true; }
	if (flymode > 0) { SendAppearancePacket(AT_Levitate, flymode); } 
	/* Set Con State for Reporting */
	conn_state = PlayerProfileLoaded;

	m_pp.zone_id = zone->GetZoneID();
	m_pp.zoneInstance = 0;
	ignore_zone_count = false;
	
	
	SendToBoat();

	/* Load Character Key Ring */
	KeyRingLoad();
		
	/* Set Total Seconds Played */
	m_pp.lastlogin = time(nullptr);
	TotalSecondsPlayed = m_pp.timePlayedMin * 60;
	/* Set Max AA XP */
	max_AAXP = GetEXPForLevel(0, true);
	/* If we can maintain intoxication across zones, check for it */
	if (!RuleB(Character, MaintainIntoxicationAcrossZones))
		m_pp.intoxication = 0;

	strcpy(name, m_pp.name);
	strcpy(lastname, m_pp.last_name);
	/* If PP is set to weird coordinates */
	if ((m_pp.x == -1 && m_pp.y == -1 && m_pp.z == -1) || (m_pp.x == -2 && m_pp.y == -2 && m_pp.z == -2)) {
        auto safePoint = zone->GetSafePoint();
		m_pp.x = safePoint.x;
		m_pp.y = safePoint.y;
		m_pp.z = safePoint.z;
	}
	/* If too far below ground, then fix */
	// float ground_z = GetGroundZ(m_pp.x, m_pp.y, m_pp.z);
	// if (m_pp.z < (ground_z - 500))
	// 	m_pp.z = ground_z;

	/* Set Mob variables for spawn */
	class_ = m_pp.class_;
	level = m_pp.level;
	m_Position.x = m_pp.x;
	m_Position.y = m_pp.y;
	m_Position.z = m_pp.z;
	m_Position.w = m_pp.heading * 255.0f / 512.0f;
	race = m_pp.race;
	base_race = m_pp.race;
	gender = m_pp.gender;
	base_gender = m_pp.gender;
	deity = m_pp.deity;
	haircolor = m_pp.haircolor;
	beardcolor = m_pp.beardcolor;
	eyecolor1 = m_pp.eyecolor1;
	eyecolor2 = m_pp.eyecolor2;
	hairstyle = m_pp.hairstyle;
	luclinface = m_pp.face;
	beard = m_pp.beard;

	/* If GM not set in DB, and does not meet min status to be GM, reset */
	if (m_pp.gm && admin < minStatusToBeGM)
		m_pp.gm = 0;

	/* Load Guild */
	if (!IsInAGuild()) { m_pp.guild_id = GUILD_NONE; }
	else {
		m_pp.guild_id = GuildID();
		m_pp.guildrank = GuildRank();
	}

	size = GetPlayerHeight(race);
	base_size = size;
	m_pp.height = size;
	m_pp.width = size;

	z_offset = CalcZOffset();
	/* Initialize AA's : Move to function eventually */
	for (uint32 a = 0; a < MAX_PP_AA_ARRAY; a++){ aa[a] = &m_pp.aa_array[a]; }
	query = StringFormat(
		"SELECT								"
		"slot,							    "
		"aa_id,								"
		"aa_value							"
		"FROM								"
		"`character_alternate_abilities`    "
		"WHERE `id` = %u ORDER BY `slot`", this->CharacterID());
	results = database.QueryDatabase(query); i = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		i = atoi(row[0]);
		m_pp.aa_array[i].AA = atoi(row[1]);
		m_pp.aa_array[i].value = atoi(row[2]);
		aa[i]->AA = atoi(row[1]);
		aa[i]->value = atoi(row[2]);
	}
	for (uint32 a = 0; a < MAX_PP_AA_ARRAY; a++){
		uint32 id = aa[a]->AA;
		//watch for invalid AA IDs
		if (id == aaNone)
			continue;
		if (id >= aaHighestID) {
			aa[a]->AA = aaNone;
			aa[a]->value = 0;
			continue;
		}
		if (aa[a]->value == 0) {
			aa[a]->AA = aaNone;
			continue;
		}
		if (aa[a]->value > HIGHEST_AA_VALUE) {
			aa[a]->AA = aaNone;
			aa[a]->value = 0;
			continue;
		}

		if (aa[a]->value > 1)	/* hack in some stuff for sony's new AA method (where each level of each aa.has a seperate ID) */
			aa_points[(id - aa[a]->value + 1)] = aa[a]->value;
		else
			aa_points[id] = aa[a]->value;
	}

	/* Send Group Members via PP */
	uint32 groupid = database.GetGroupID(GetName());
	Group* group = nullptr;
	if (groupid > 0){
		group = entity_list.GetGroupByID(groupid);
		if (!group) {	//nobody from our is here... start a new group
			group = new Group(groupid);
			if (group->GetID() != 0)
				entity_list.AddGroup(group, groupid);
			else	//error loading group members...
			{
				delete group;
				group = nullptr;
			}
		}	//else, somebody from our group is already here...

		if (group)
			group->UpdatePlayer(this);
		else
			database.SetGroupID(GetName(), 0, CharacterID());	//cannot re-establish group, kill it

	}
	else {	//no group id
		//clear out the group junk in our PP
		uint32 xy = 0;
		for (xy = 0; xy < MAX_GROUP_MEMBERS; xy++)
			memset(m_pp.groupMembers[xy], 0, 64);
	}

	if (group){
		// If the group leader is not set, pull the group leader information from the database.
		if (!group->GetLeader()){
			char ln[64];
			memset(ln, 0, 64);
			strcpy(ln, database.GetGroupLeadershipInfo(group->GetID(), ln));
			Client *c = entity_list.GetClientByName(ln);
			if (c)
				group->SetLeader(c);
		}
	}
	if (SPDAT_RECORDS > 0)
	{
		database.LoadBuffs(this);
		for (uint32 z = 0; z<MAX_PP_MEMSPELL; z++)
		{
			if (m_pp.mem_spells[z] >= (uint32)SPDAT_RECORDS)
				UnmemSpell(z, false);
		}

	}

	CalcBonuses();

	if (m_pp.cur_hp <= 0)
	{
		m_pp.cur_hp = GetMaxHP();
	}

	SetHP(m_pp.cur_hp);

	Mob::SetMana(m_pp.mana); // mob function doesn't send the packet
	SetEndurance(m_pp.endurance, false);
	m_pp.fatigue = GetFatiguePercent();

	uint32 max_slots = GetMaxBuffSlots();
	bool stripbuffs = false;

	if(RuleB(AlKabor, StripBuffsOnLowHP) && GetHP() < itembonuses.HP)
		stripbuffs = true;

	for (int i = 0; i < max_slots; i++) 
	{
		if (buffs[i].spellid != SPELL_UNKNOWN && !stripbuffs)
		{
			m_pp.buffs[i].spellid = buffs[i].spellid;
			m_pp.buffs[i].bard_modifier = 10;
			m_pp.buffs[i].slotid = 2;
			m_pp.buffs[i].player_id = 0x2211;
			m_pp.buffs[i].level = buffs[i].casterlevel;
			m_pp.buffs[i].effect = 0;
			m_pp.buffs[i].duration = buffs[i].ticsremaining;
			m_pp.buffs[i].counters = buffs[i].counters;
		}
		else
		{
			m_pp.buffs[i].spellid = SPELLBOOK_UNKNOWN;
			m_pp.buffs[i].bard_modifier = 10;
			m_pp.buffs[i].slotid = 0;
			m_pp.buffs[i].player_id = 0;
			m_pp.buffs[i].level = 0;
			m_pp.buffs[i].effect = 0;
			m_pp.buffs[i].duration = 0;
			m_pp.buffs[i].counters = 0;
		}
	}

	if(stripbuffs)
	{
		Log.Out(Logs::General, Logs::EQMac, "Removing buffs. HP is: %i MaxHP is: %i BaseHP is: %i HP from items is: %i HP from spells is: %i", GetHP(), GetMaxHP(), GetBaseHP(), itembonuses.HP, spellbonuses.HP);
		BuffFadeAll();
	}

	if (m_pp.z <= zone->newzone_data.underworld) 
	{
		m_pp.x = zone->newzone_data.safe_x;
		m_pp.y = zone->newzone_data.safe_y;
		m_pp.z = zone->newzone_data.safe_z;
	}

	m_pp.expansions = database.GetExpansion(AccountID());

	p_timers.SetCharID(CharacterID());
	if (!p_timers.Load(&database)) {
		Log.Out(Logs::General, Logs::Error, "Unable to load ability timers from the database for %s (%i)!", GetCleanName(), CharacterID());
	}

	/* Load Spell Slot Refresh from Currently Memoried Spells */
	for (unsigned int i = 0; i < MAX_PP_MEMSPELL; ++i)
		if (IsValidSpell(m_pp.mem_spells[i]))
			m_pp.spellSlotRefresh[i] = p_timers.GetRemainingTime(pTimerSpellStart + m_pp.mem_spells[i]) * 1000;

	/* Ability slot refresh send SK/PAL */
	if (m_pp.class_ == SHADOWKNIGHT || m_pp.class_ == PALADIN) {
		uint32 abilitynum = 0;
		if (m_pp.class_ == SHADOWKNIGHT)
		{ 
			abilitynum = pTimerHarmTouch; 
		}
		else
		{ 
			abilitynum = pTimerLayHands;
		}

		uint32 remaining = p_timers.GetRemainingTime(abilitynum);
		if (remaining > 0 && remaining < 15300)
			m_pp.abilitySlotRefresh = remaining * 1000;
		else
			m_pp.abilitySlotRefresh = 0;
	}

#ifdef _EQDEBUG
	printf("Dumping inventory on load:\n");
	m_inv.dumpEntireInventory();
#endif

	/* Reset to max so they dont drown on zone in if its underwater */
	m_pp.air_remaining = 60;
	/* Check for PVP Zone status*/
	if (zone->IsPVPZone())
		m_pp.pvp = 1;
	/* Time entitled on Account: Move to account */
	m_pp.timeentitledonaccount = database.GetTotalTimeEntitledOnAccount(AccountID()) / 1440;

	FillPPItems();

	/* This checksum should disappear once dynamic structs are in... each struct strategy will do it */
	CRC32::SetEQChecksum((unsigned char*)&m_pp, sizeof(PlayerProfile_Struct) - 4);
	outapp = new EQApplicationPacket(OP_PlayerProfile, sizeof(PlayerProfile_Struct));

	PlayerProfile_Struct* pps = (PlayerProfile_Struct*) new uchar[sizeof(PlayerProfile_Struct) - 4];
	memcpy(pps, &m_pp, sizeof(PlayerProfile_Struct) - 4);
	pps->heading = m_pp.heading;
	pps->perAA = m_epp.perAA;
	int r = 0;
	for (r = 0; r < MAX_PP_AA_ARRAY; r++)
	{
		if (pps->aa_array[r].AA > 0)
		{
			pps->aa_array[r].AA = zone->EmuToEQMacAA(pps->aa_array[r].AA);
			pps->aa_array[r].value = pps->aa_array[r].value * 16;
		}
	}
	int s = 0;
	for (r = 0; s < HIGHEST_SKILL; s++)
	{
		SkillUseTypes currentskill = (SkillUseTypes)s;
		if (pps->skills[s] > 0)
			continue;
		else
		{
			int haveskill = GetMaxSkillAfterSpecializationRules(currentskill, MaxSkill(currentskill, GetClass(), RuleI(Character, MaxLevel)));
			if (haveskill > 0)
			{
				pps->skills[s] = 254;
				//If we never get the skill, value is 255. If we qualify for it AND do not need to train it it's 0, 
				//if we get it but don't yet qualify or it needs to be trained it's 254.
				uint16 t_level = SkillTrainLevel(currentskill, GetClass());
				if (t_level <= GetLevel())
				{
					if (t_level == 1)
						pps->skills[s] = 0;
				}
			}
			else
				pps->skills[s] = 255;
		}
	}
	
	if(m_pp.boatid > 0 && (zone->GetZoneID() == timorous || zone->GetZoneID() == firiona))
		pps->boat[0] = 0;

	// The entityid field in the Player Profile is used by the Client in relation to Group Leadership AA
	if(GetClientVersion() == EQClientTrilogy)
		strn0cpy(pps->current_zone, database.GetZoneName(zone->GetZoneID()),16);

	m_pp.entityid = GetID();
	memcpy(outapp->pBuffer, pps, outapp->size);
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	database.LoadPetInfo(this);

	/* Moved here so it's after where we load the pet data. */
	if (!GetAA(aaPersistentMinion))
		memset(&m_suspendedminion, 0, sizeof(PetInfo));

	/* Server Zone Entry Packet */
	outapp = new EQApplicationPacket(OP_ZoneEntry, sizeof(ServerZoneEntry_Struct));
	ServerZoneEntry_Struct* sze = (ServerZoneEntry_Struct*)outapp->pBuffer;
	memset(sze, 0, sizeof(ServerZoneEntry_Struct));
	
	strcpy(sze->name, name);
	strn0cpy(sze->Surname, lastname, 32);
	sze->zoneID = zone->GetZoneID();
	sze->x_pos = m_pp.x;
	sze->y_pos = m_pp.y;
	sze->z_pos = m_pp.z;
	sze->heading = m_pp.heading;
	sze->race = m_pp.race;
	sze->deity = m_pp.deity;
	sze->curHP = static_cast<uint32>(GetHPRatio());
	sze->max_hp = 100;
	sze->size = base_size;
	sze->width = base_size;
	
	sze->NPC = IsBecomeNPC() ? 1 : 10;
	sze->invis = invisible ? 1 : 0;
	sze->sneaking = sneaking;
	sze->animation = animation;
	
	sze->haircolor = m_pp.haircolor;
	sze->beardcolor = m_pp.beardcolor;
	sze->eyecolor1 = m_pp.eyecolor1;
	sze->eyecolor2 = m_pp.eyecolor2;
	sze->hairstyle = m_pp.hairstyle;
	sze->beard = m_pp.beard;
	sze->face = m_pp.face;
	sze->level = m_pp.level;
	for(int k = 0; k < 9; k++) 
	{
		sze->equipment[k] = GetEquipmentMaterial(k);
		if (armor_tint[k])
		{
			sze->equipcolors[k].color = armor_tint[k];
		}
		else
		{
			sze->equipcolors[k].color = GetEquipmentColor(i);
		}
	}
	sze->AFK = AFK;
	sze->anon = m_pp.anon;
	sze->title = 0;
	sze->anim_type = 0x64;
	sze->bodytexture = texture;
	sze->bodytype = bodytype;

	if(helmtexture && helmtexture != 0xFF)
	{
		sze->helm=helmtexture;
	} else {
		sze->helm = 0;
	}

	sze->GM = GetGM() ? 1 : 0;
	sze->GuildID = GuildID();
	if(sze->GuildID == 0)
		sze->GuildID = 0xFFFF;
	if (!IsInAGuild()) {
		sze->guildrank = 0xFFFF;
	} else {
		sze->guildrank = guild_mgr.GetDisplayedRank(GuildID(), GuildRank(), AccountID());
	}
	if(sze->guildrank == 0)
		sze->guildrank = 0xFFFF;
			
	sze->walkspeed = walkspeed;
	sze->runspeed = (gmspeed == 0) ? runspeed : 3.1f;
	sze->light = m_Light.Type.Active;
	sze->class_ = GetClass();

	sze->gender = m_pp.gender;
	sze->flymode = FindType(SE_Levitate) ? 2 : 0;
	sze->prev = 0xa0ae0e00;
	sze->next = 0xa0ae0e00;
	sze->view_height = 0x6666c640;
	sze->sprite_oheight = 0x00004840;
	sze->extra[10] = 0xFF;
	sze->extra[11] = 0xFF;
	sze->extra[12] = 0xFF;
	sze->extra[13] = 0xFF;
	sze->type = 0;
	sze->petOwnerId = ownerid;

	sze->curHP = 1;
	sze->NPC = 0;
	if(zone->zonemap)
	{
		// This prevents hopping on logging in.
		glm::vec3 loc(m_Position.x, m_Position.y, m_Position.z);
		if (!IsEncumbered() && m_pp.boatid == 0 && (!zone->HasWaterMap() || !zone->watermap->InLiquid(loc)) && 
			zone->GetZoneID() != hole && zone->GetZoneID() != freporte)
		{
			float bestz = zone->zonemap->FindBestZ(loc, nullptr);
			if(bestz > -5000.0f && size > 0)
				m_Position.z = bestz + (0.625f * (float)size);
		}
		sze->z_pos = m_Position.z;
	}

	outapp->priority = 6;
	FastQueuePacket(&outapp);

	/* Zone Spawns Packet */
	if (GetClientVersion() != EQClientTrilogy) {
		entity_list.SendZoneSpawnsBulk(this);
		entity_list.SendZoneCorpsesBulk(this);
	}
	//entity_list.SendZonePVPUpdates(this);	//hack until spawn struct is fixed.
	//_log(EQMAC__LOG, "Time");
	/* Time of Day packet */
	outapp = new EQApplicationPacket(OP_TimeOfDay, sizeof(TimeOfDay_Struct));
	TimeOfDay_Struct* tod = (TimeOfDay_Struct*)outapp->pBuffer;
	zone->zone_time.getEQTimeOfDay(time(0), tod);
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	/* LFG packet */
	// This tells the client who in the zone are LFG. Our own status is handled by a Connecting handle
	entity_list.SendLFG(this, true);

	/*
	Character Inventory Packet
	AK sent both individual item packets and a single bulk inventory packet on zonein
	The client requires cursor items to be sent in Handle_Connect_OP_SendExpZonein, should all items be moved there as well?
	*/
	if (loaditems) {  //Dont load if a length error occurs 
		BulkSendItems();
		BulkSendInventoryItems();
	}
	
	/*
	Weather Packet
	This shouldent be moved, this seems to be what the client
	uses to advance to the next state (sending ReqNewZone)
	*/
	outapp = new EQApplicationPacket(OP_Weather, sizeof(Weather_Struct));
	Weather_Struct* ws = (Weather_Struct*)outapp->pBuffer;
	ws->type = 0;
	ws->intensity = 0;

	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	SetAttackTimer();
	conn_state = ZoneInfoSent;

	return;
}

void Client::Handle_Connect_OP_TargetMouse(const EQApplicationPacket *app)
{
	// This can happen if you tab while zoning. Just handle the opcode and return.
	return;
}

void Client::Handle_Connect_OP_DeleteCharge(const EQApplicationPacket *app)
{
	//just send it back, if this happens while connecting.
	QueuePacket(app);
	return;
}

// connected opcode handlers

void Client::Handle_OP_AAAction(const EQApplicationPacket *app)
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	//Action packet is always 256 bytes, but fields have varying positions.
	if (app->size != 256)
	{
		Log.Out(Logs::Detail, Logs::Error, "Caught an invalid AAAction packet. Size is: %d", app->size);
		return;
	}

	if(Admin() < 95 && RuleB(Character, DisableAAs))
	{
		Message(CC_Yellow, "Alternate Abilities are currently disabled. You will continue to use traditional experience.");
		return;
	}

	if (strncmp((char *)app->pBuffer, "on ", 3) == 0)
	{
		if (m_epp.perAA == 0)
			Message_StringID(CC_Default, AA_ON); //ON
		m_epp.perAA = atoi((char *)&app->pBuffer[3]);
		SendAAStats();
		SendAATable();
	}
	else if (strcmp((char *)app->pBuffer, "off") == 0)
	{
		if (m_epp.perAA > 0)
			Message_StringID(CC_Default, AA_OFF); //OFF
		m_epp.perAA = 0;
		SendAAStats();
		SendAATable();
	}
	else if (strncmp((char *)app->pBuffer, "buy ", 4) == 0)
	{
		AA_Action *action = (AA_Action *)app->pBuffer;
		int aa = atoi((char *)&app->pBuffer[4]);

		int emuaa = database.GetMacToEmuAA(aa);
		SendAA_Struct* aa2 = zone->FindAA(emuaa);

		if (aa2 == nullptr)
		{
			Log.Out(Logs::Detail, Logs::Error, "Handle_OP_AAAction dun goofed. EQMacAAID is: %i, but no valid EmuAAID could be found.", aa);
			SendAATable(); // So client doesn't bug.
			SendAAStats();
			return;
		}

		uint32 cur_level = 0;
		if (aa2->id > 0)
			cur_level = GetAA(aa2->id);

		action->ability = emuaa + cur_level;
		action->action = 3;
		action->exp_value = m_epp.perAA;
		action->unknown08 = 0;

		Log.Out(Logs::Detail, Logs::AA, "Buying: EmuaaID: %i, MacaaID: %i, action: %i, exp: %i", action->ability, aa, action->action, action->exp_value);
		BuyAA(action);
	}
	else if (strncmp((char *)app->pBuffer, "activate ", 9) == 0)
	{
		if (GetBoatNPCID() > 0)
		{
			Message_StringID(CC_Red, TOO_DISTRACTED);
			return;
		}

		int aa = atoi((char *)&app->pBuffer[9]);
		AA_Action *action = (AA_Action *)app->pBuffer;

		action->ability = database.GetMacToEmuAA(aa);
		Log.Out(Logs::Detail, Logs::AA, "Activating AA %d", action->ability);
		ActivateAA((aaID)action->ability);
	}

	return;
}

void Client::Handle_OP_Action(const EQApplicationPacket *app) 
{
	//EQmac sends this when drowning.
}

void Client::Handle_OP_Animation(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Animation_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized "
			"OP_Animation: got %d, expected %d", app->size,
			sizeof(Animation_Struct));
		DumpPacket(app);
		return;
	}

	Animation_Struct *s = (Animation_Struct *)app->pBuffer;

	//might verify spawn ID, but it wouldent affect anything
	Animation action = static_cast<Animation>(s->action);
	DoAnim(action, s->value);

	return;
}

void Client::Handle_OP_ApplyPoison(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(ApplyPoison_Struct))
	{
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_ApplyPoison, size=%i, expected %i", app->size, sizeof(ApplyPoison_Struct));
		return;
	}

	uint32 ApplyPoisonSuccessResult = 0;
	ApplyPoison_Struct* ApplyPoisonData = (ApplyPoison_Struct*)app->pBuffer;
	const ItemInst* PrimaryWeapon = GetInv().GetItem(MainPrimary);
	const ItemInst* SecondaryWeapon = GetInv().GetItem(MainSecondary);
	const ItemInst* PoisonItemInstance = GetInv()[ApplyPoisonData->inventorySlot];

	bool IsPoison = PoisonItemInstance != nullptr && (PoisonItemInstance->GetItem()->ItemType == ItemTypePoison && PoisonItemInstance->GetCharges() > 0);

	if (!IsPoison)
	{
		Log.Out(Logs::General, Logs::Skills, "Item %s used to cast spell effect from a poison item was missing from inventory slot %d after casting, or is not a poison! Item type is %d", PoisonItemInstance->GetItem()->Name, ApplyPoisonData->inventorySlot, PoisonItemInstance->GetItem()->ItemType);
		Message_StringID(CC_Default, ITEM_OUT_OF_CHARGES);
	}
	else if (GetClass() == ROGUE)
	{
		if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0)
		{
			Message(CC_Default, "Error: You are not allowed to interact with the player world as GM.");
			ApplyPoisonSuccessResult = 0;
		}
		else if ((PrimaryWeapon && PrimaryWeapon->GetItem()->ItemType == ItemType1HPiercing) ||
			(SecondaryWeapon && SecondaryWeapon->GetItem()->ItemType == ItemType1HPiercing)) {
			float SuccessChance = (GetSkill(SkillApplyPoison) + GetLevel()) / 400.0f;
			double ChanceRoll = zone->random.Real(0, 1);

			if (ChanceRoll < SuccessChance) {
				ApplyPoisonSuccessResult = 1;
				// NOTE: Someone may want to tweak the chance to proc the poison effect that is added to the weapon here.
				// My thinking was that DEX should be apart of the calculation.
				AddProcToWeapon(PoisonItemInstance->GetItem()->Proc.Effect, false, (GetDEX() / 100) + 103);
			}

			float success = ApplyPoisonSuccessResult ? 1.0 : 2.0;
			CheckIncreaseSkill(SkillApplyPoison, nullptr, zone->skill_difficulty[SkillApplyPoison].difficulty, success);

			DeleteItemInInventory(ApplyPoisonData->inventorySlot, 1, true);

			Log.Out(Logs::General, Logs::Skills, "Chance to Apply Poison was %f. Roll was %f. Result is %u.", SuccessChance, ChanceRoll, ApplyPoisonSuccessResult);
		}
	}

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_ApplyPoison, nullptr, sizeof(ApplyPoison_Struct));
	ApplyPoison_Struct* ApplyPoisonResult = (ApplyPoison_Struct*)outapp->pBuffer;
	ApplyPoisonResult->success = ApplyPoisonSuccessResult;
	ApplyPoisonResult->inventorySlot = ApplyPoisonData->inventorySlot;

	FastQueuePacket(&outapp);
}

void Client::Handle_OP_Assist(const EQApplicationPacket *app)
{
	if (app->size != sizeof(EntityId_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_Assist expected %i got %i", sizeof(EntityId_Struct), app->size);
		return;
	}

	EntityId_Struct* eid = (EntityId_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetID(eid->entity_id);

	EQApplicationPacket* outapp = app->Copy();
	eid = (EntityId_Struct*)outapp->pBuffer;
	if (RuleB(Combat, AssistNoTargetSelf))
		eid->entity_id = GetID();
	else
		eid->entity_id = -1;
	if (entity && entity->IsMob()) {
		Mob *assistee = entity->CastToMob();
		if (assistee->GetTarget()) {
			Mob *new_target = assistee->GetTarget();
			if (new_target && (GetGM() ||
				Distance(m_Position, assistee->GetPosition()) <= TARGETING_RANGE)) {
				SetAssistExemption(true);
				eid->entity_id = new_target->GetID();
			}
		}
	}

	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_AutoAttack(const EQApplicationPacket *app)
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != 4) {
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_AutoAttack expected:4 got:%i", app->size);
		return;
	}

	if (app->pBuffer[0] == 0)
	{
		auto_attack = false;
		if (IsAIControlled())
			return;
		attack_timer.Disable();
		ranged_timer.Disable();
		attack_dw_timer.Disable();

		m_AutoAttackPosition = glm::vec4();
		m_AutoAttackTargetLocation = glm::vec3();
		aa_los_them_mob = nullptr;
	}
	else if (app->pBuffer[0] == 1)
	{
		auto_attack = true;
		auto_fire = false;
		if (IsAIControlled())
			return;
		SetAttackTimer();

		if (GetTarget())
		{
			aa_los_them_mob = GetTarget();
			m_AutoAttackPosition = GetPosition();
			m_AutoAttackTargetLocation = glm::vec3(aa_los_them_mob->GetPosition());
			los_status = CheckLosFN(aa_los_them_mob);
			los_status_facing = IsFacingMob(aa_los_them_mob);
		}
		else
		{
			m_AutoAttackPosition = GetPosition();
			m_AutoAttackTargetLocation = glm::vec3();
			aa_los_them_mob = nullptr;
			los_status = false;
			los_status_facing = false;
		}
	}
}

void Client::Handle_OP_BazaarSearch(const EQApplicationPacket *app)
{

	if (app->size == sizeof(BazaarSearch_Struct)) {

		BazaarSearch_Struct* bss = (BazaarSearch_Struct*)app->pBuffer;

		this->SendBazaarResults(bss->TraderID, bss->Class_, bss->Race, bss->ItemStat, bss->Slot, bss->Type,
			bss->Name, bss->MinPrice * 1000, bss->MaxPrice * 1000);
	}
	else if (app->size == sizeof(BazaarWelcome_Struct)) {

		BazaarWelcome_Struct* bws = (BazaarWelcome_Struct*)app->pBuffer;

		if (bws->Beginning.Action == BazaarWelcome)
			SendBazaarWelcome();
	}
	else {
		Log.Out(Logs::Detail, Logs::Bazaar, "Malformed BazaarSearch_Struct packe, Action %it received, ignoring...");
		Log.Out(Logs::General, Logs::Error, "Malformed BazaarSearch_Struct packet received, ignoring...\n");
	}

	return;
}

void Client::Handle_OP_BazaarSearchCon(const EQApplicationPacket *app)
{
	SendBazaarWelcome();
	return;
}

void Client::Handle_OP_Begging(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (!p_timers.Expired(&database, pTimerBeggingPickPocket, false))
	{
		Message(CC_Red, "Ability recovery time not yet met.");

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Begging, sizeof(BeggingResponse_Struct));
		BeggingResponse_Struct *brs = (BeggingResponse_Struct*)outapp->pBuffer;
		brs->Result = 0;
		FastQueuePacket(&outapp);
		return;
	}

	if (!HasSkill(SkillBegging) || !GetTarget())
		return;

	p_timers.Start(pTimerBeggingPickPocket, 8);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Begging, sizeof(BeggingResponse_Struct));
	BeggingResponse_Struct *brs = (BeggingResponse_Struct*)outapp->pBuffer;

	brs->target = GetTarget()->GetID();
	brs->begger = GetID();
	brs->skill = GetSkill(SkillBegging);
	brs->Result = 0; // Default, Fail.
	if (GetTarget() == this)
	{
		FastQueuePacket(&outapp);
		return;
	}

	int RandomChance = zone->random.Int(0, 100);

	int ChanceToAttack = 0;

	if (GetLevel() > GetTarget()->GetLevel())
		ChanceToAttack = zone->random.Int(0, 15);
	else
		ChanceToAttack = zone->random.Int(((this->GetTarget()->GetLevel() - this->GetLevel()) * 10) - 5, ((this->GetTarget()->GetLevel() - this->GetLevel()) * 10));

	if (ChanceToAttack < 0)
		ChanceToAttack = -ChanceToAttack;

	if (RandomChance < ChanceToAttack)
	{
		uint8 stringchance = zone->random.Int(0, 1);
		int stringid = BEG_FAIL1;
		if(stringchance == 1)
			stringid = BEG_FAIL2;
		Message_StringID(CC_Default, stringid);
		GetTarget()->Attack(this);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	uint16 CurrentSkill = GetSkill(SkillBegging);

	float ChanceToBeg = ((float)(CurrentSkill / 700.0f) + 0.15f) * 100;
	float success = 2.0;
	if (RandomChance < ChanceToBeg)
	{
		success = 1.0;
		Message_StringID(CC_Default, BEG_SUCCESS, GetName());
		brs->Amount = zone->random.Int(1, 10);
		// This needs some work to determine how much money they can beg, based on skill level etc.
		if (CurrentSkill < 50)
		{
			brs->Result = 4;	// Copper
			AddMoneyToPP(brs->Amount, false);
		}
		else
		{
			if(zone->random.Roll(2))
			{
				if(brs->Amount == 10)
				{
					brs->Result = 1;	// Plat
					AddMoneyToPP(1000, false);
				}
				else
				{
					brs->Result = 2;	// Gold
					AddMoneyToPP(brs->Amount * 100, false);
				}
			}
			else
			{
				brs->Result = 3;	// Silver
				AddMoneyToPP(brs->Amount * 10, false);
			}
		}

	}
	QueuePacket(outapp);
	safe_delete(outapp);
	CheckIncreaseSkill(SkillBegging, nullptr, zone->skill_difficulty[SkillBegging].difficulty, success);
}

void Client::Handle_OP_Bind_Wound(const EQApplicationPacket *app) 
{
	if(!app)
		return;

	if (app->size != sizeof(BindWound_Struct)){
		Log.Out(Logs::General, Logs::Error, "Size mismatch for Bind wound packet");
		DumpPacket(app);
	}

	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) {
		return;
	}

	BindWound_Struct* bind_in = (BindWound_Struct*)app->pBuffer;

	if(!bind_in)
		return;

	Mob* bindmob = entity_list.GetMob(bind_in->to);
	if (!bindmob){
		Log.Out(Logs::General, Logs::Error, "Bindwound on non-exsistant mob from %s", this->GetName());
	}
	else {
		Log.Out(Logs::General, Logs::None, "BindWound in: to:\'%s\' from=\'%s\'", bindmob->GetName(), GetName());
		BindWound(bindmob, true);
	}
	return;
}

void Client::Handle_OP_BoardBoat(const EQApplicationPacket *app)
{

	if (app->size <= 7 || app->size > 32) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_BoardBoad. Expected greater than 7 no greater than 32, got %i", app->size);
		DumpPacket(app);
		return;
	}

	char boatname[32];
	memcpy(boatname, app->pBuffer, app->size);
	boatname[31] = '\0';

	Log.Out(Logs::Moderate, Logs::Boats, "%s is attempting to board boat %s", GetName(), boatname);

	Mob* boat = entity_list.GetMob(boatname);

	if (!boat || !boat->IsBoat())
		return;

	if (boat)
	{
		BuffFadeByEffect(SE_Levitate);
		BoatID = boat->GetID();	// set the client's BoatID to show that it's on this boat
		m_pp.boatid = boat->GetNPCTypeID(); //For EQMac's boat system.
		strcpy(m_pp.boat, boatname);

		char buf[24];
		snprintf(buf, 23, "%d", boat->GetNPCTypeID());
		buf[23] = '\0';
		parse->EventPlayer(EVENT_BOARD_BOAT, this, buf, 0);
	}

	return;
}

void Client::Handle_OP_Buff(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SpellBuffFade_Struct))
	{
		Log.Out(Logs::General, Logs::Error, "Size mismatch in OP_Buff. expected %i got %i", sizeof(SpellBuffFade_Struct), app->size);
		DumpPacket(app);
		return;
	}
	SpellBuffFade_Struct* sbf = (SpellBuffFade_Struct*)app->pBuffer;
	uint32 spid = sbf->spellid;
	Log.Out(Logs::Detail, Logs::Spells, "Client requested that buff with spell id %d be canceled. effect: %d slotid: %d slot: %d duration: %d level: %d", sbf->spellid, sbf->effect, sbf->slotid, sbf->slot, sbf->duration, sbf->level);

	//something about IsDetrimentalSpell() crashes this portion of code..
	//tbh we shouldn't use it anyway since this is a simple red vs blue buff check and
	//isdetrimentalspell() is much more complex
	if (spid == 0xFFFF || (IsValidSpell(spid) && (spells[spid].goodEffect == 0)))
		QueuePacket(app);
	else
		BuffFadeBySpellID(spid);

	return;
}

void Client::Handle_OP_Bug(const EQApplicationPacket *app)
{
	if (app->size != sizeof(BugStruct) && app->size != sizeof(IntelBugStruct))
	{
		printf("Wrong size of BugStruct got %d expected %zu!\n", app->size, sizeof(BugStruct));
		DumpPacket(app);
	}
	else
	{
		BugStruct* bug;
		if(app->size == sizeof(IntelBugStruct))
		{
			bug = new struct BugStruct;
			IntelBugStruct* intelbug = (IntelBugStruct*)app->pBuffer;
			strcpy(bug->name, intelbug->name);
			strcpy(bug->bug, intelbug->bug);
			bug->x = GetX();
			bug->y = GetY();
			bug->z = GetZ();
			bug->heading = GetHeading();
			memset(bug->target_name,0,64);
			if(GetTarget())
				strncpy(bug->target_name,GetTarget()->GetName(),64);
			bug->type = 0;
			if(intelbug->duplicate == 1)
				bug->type += 1;
			if(intelbug->crash == 1)
				bug->type += 2;
			strcpy(bug->chartype, "NA");
		}
		else if(app->size == sizeof(BugStruct))
		{
			bug = (BugStruct*)app->pBuffer;
		}
		Message(CC_Yellow, "Thank you for the bug submission, %s.", bug->name);
		uint32 clienttype = GetClientVersionBit();
		database.UpdateBug(bug, clienttype);
	}
	return;
}

void Client::Handle_OP_Camp(const EQApplicationPacket *app) 
{

	if (GetBoatNPCID() > 0)
	{
		Stand();
		return;
	}

	if (GetGM())
	{
		OnDisconnect(true);
		return;
	}
	camp_timer.Start(29000, true);
	return;
}

void Client::Handle_OP_CancelTrade(const EQApplicationPacket *app)
{
	if (app->size != sizeof(CancelTrade_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_CancelTrade, size=%i, expected %i", app->size, sizeof(CancelTrade_Struct));
		return;
	}
	Mob* with = trade->With();
	if (with && with->IsClient()) {
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;

		// Forward cancel packet to other client
		msg->fromid = with->GetID();
		//msg->action = 1;

		with->CastToClient()->QueuePacket(app);

		// Put trade items/cash back into inventory
		FinishTrade(this);
		trade->Reset();
	}
	else if (with){
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;
		msg->fromid = with->GetID();
		QueuePacket(app);

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_TradeReset, 0);
		QueuePacket(outapp);
		safe_delete(outapp);

		FinishTrade(this);
		trade->Reset();
	}
	else
	{
		//EQMac sends a second CancelTrade packet. Since "with" and the trade became invalid the first time around, this handles the second which prevents the client from bugging.
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;
		QueuePacket(app);
		Log.Out(Logs::Detail, Logs::Debug, "Cancelled second trade. from is: %i.", msg->fromid);
	}

	return;
}

void Client::Handle_OP_CastSpell(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(CastSpell_Struct)) {
		std::cout << "Wrong size: OP_CastSpell, size=" << app->size << ", expected " << sizeof(CastSpell_Struct) << std::endl;
		return;
	}
	if (IsAIControlled()) {
		this->Message_StringID(CC_Red, NOT_IN_CONTROL);
		//Message(CC_Red, "You cant cast right now, you arent in control of yourself!");
		return;
	}

	CastSpell_Struct* castspell = (CastSpell_Struct*)app->pBuffer;

	clicky_override = false;

	Log.Out(Logs::General, Logs::Spells, "OP CastSpell: slot=%d, spell=%d, target=%d, inv=%lx", castspell->slot, castspell->spell_id, castspell->target_id, (unsigned long)castspell->inventoryslot);

	if (m_pp.boatid != 0)
	{
		InterruptSpell(castspell->spell_id);
		return;
	}

	/* Memorized Spell */
	if (m_pp.mem_spells[castspell->slot] && m_pp.mem_spells[castspell->slot] == castspell->spell_id) {

		if ((Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0)) {
			InterruptSpell(castspell->spell_id);
			return;
		}

		uint16 spell_to_cast = 0;
		if (castspell->slot < MAX_PP_MEMSPELL) {
			spell_to_cast = m_pp.mem_spells[castspell->slot];
			if (spell_to_cast != castspell->spell_id) {
				InterruptSpell(castspell->spell_id); //CHEATER!!!
				return;
			}
		}
		else if (castspell->slot >= MAX_PP_MEMSPELL) {
			InterruptSpell();
			return;
		}

		CastSpell(spell_to_cast, castspell->target_id, castspell->slot);
	}
	/* Item Spell Slot or Potion Belt Slot */
	else if ((castspell->slot == USE_ITEM_SPELL_SLOT))	// ITEM or POTION cast
	{
		if (m_inv.SupportsClickCasting(castspell->inventoryslot))	// sanity check
		{
			if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0)
			{
				int guideClicks[] = { 30, 35, 36, 84, 119, 206, 214, 354, 595, 722, 778, 810, 811, 813, 814, 815, 816, 817, 818, 
										959, 970, 994, 1209, 1211, 1212, 1213, 1214, 1215, 1216, 1217, 1218, 1219, 1220, 1355, 3921 };
				int* found = std::find(guideClicks, std::end(guideClicks), castspell->spell_id);
				if (found == std::end(guideClicks))
				{
					InterruptSpell(castspell->spell_id);
					return;
				}
			}

			// packet field types will be reviewed as packet transistions occur -U
			const ItemInst* inst = m_inv[castspell->inventoryslot]; //slot values are int16, need to check packet on this field
			//bool cancast = true;
			if (inst && inst->IsType(ItemClassCommon))
			{
				const Item_Struct* item = inst->GetItem();
				if (item->Click.Effect != (uint32)castspell->spell_id)
				{
					database.SetMQDetectionFlag(account_name, name, "OP_CastSpell with item, tried to cast a different spell.", zone->GetShortName());
					InterruptSpell(castspell->spell_id);	//CHEATER!!
					return;
				}

				if ((item->Click.Type == ET_ClickEffect) || (item->Click.Type == ET_Expendable) || (item->Click.Type == ET_EquipClick) || (item->Click.Type == ET_ClickEffect2))
				{
					int16 casttime_ = item->CastTime;
					// Clickies with 0 casttime had no level or regeant requirement on AK. Also, -1 casttime was instant cast.
					if(casttime_ <= 0)
					{
						clicky_override = true;
						casttime_ = 0;
					}

					if (item->Click.Level2 > 0)
					{			
						if (GetLevel() >= item->Click.Level2 || clicky_override)
						{
							ItemInst* p_inst = (ItemInst*)inst;
							int i = parse->EventItem(EVENT_ITEM_CLICK_CAST, this, p_inst, nullptr, "", castspell->inventoryslot);

							if (i == 0) {
								CastSpell(item->Click.Effect, castspell->target_id, castspell->slot, casttime_, 0, 0, castspell->inventoryslot);
							}
							else {
								InterruptSpell(castspell->spell_id);
								return;
							}
						}
						else
						{
							database.SetMQDetectionFlag(account_name, name, "OP_CastSpell with item, did not meet req level.", zone->GetShortName());
							Message(CC_Default, "Error: level not high enough.", castspell->inventoryslot);
							InterruptSpell(castspell->spell_id);
						}
					}
					else
					{
						ItemInst* p_inst = (ItemInst*)inst;
						int i = parse->EventItem(EVENT_ITEM_CLICK_CAST, this, p_inst, nullptr, "", castspell->inventoryslot);

						if (i == 0) {
							CastSpell(item->Click.Effect, castspell->target_id, castspell->slot, casttime_, 0, 0, castspell->inventoryslot);
						}
						else {
							InterruptSpell(castspell->spell_id);
							return;
						}
					}
				}
				else
				{
					Message(CC_Default, "Error: unknown item->Click.Type (0x%02x)", item->Click.Type);
				}
			}
			else
			{
				Message(CC_Default, "Error: item not found in inventory slot #%i", castspell->inventoryslot);
				InterruptSpell(castspell->spell_id);
			}
		}
		else
		{
			Message(CC_Default, "Error: castspell->inventoryslot >= %i (0x%04x)", MainCursor, castspell->inventoryslot);
			InterruptSpell(castspell->spell_id);
		}
	}
	else if (castspell->slot == ABILITY_SPELL_SLOT) {	// ABILITY cast (LoH and Harm Touch)
		uint16 spell_to_cast = 0;

		//Reuse timers are handled by SpellFinished()
		if (castspell->spell_id == SPELL_LAY_ON_HANDS && GetClass() == PALADIN)
		{
			if (!p_timers.Expired(&database, pTimerLayHands)) {
				Message(CC_Red, "Ability recovery time not yet met.");
				InterruptSpell(castspell->spell_id);
				return;
			}
			spell_to_cast = SPELL_LAY_ON_HANDS;
		}
		else if ((castspell->spell_id == SPELL_HARM_TOUCH
			|| castspell->spell_id == SPELL_HARM_TOUCH2) && GetClass() == SHADOWKNIGHT) 
		{
			if (!p_timers.Expired(&database, pTimerHarmTouch)) 
			{
				Message(CC_Red, "Ability recovery time not yet met.");
				InterruptSpell(castspell->spell_id);
				return;
			}

			// determine which version of HT we are casting based on level
			if (GetLevel() <= 40)
				spell_to_cast = SPELL_HARM_TOUCH;
			else
				spell_to_cast = SPELL_HARM_TOUCH2;
		}

		// if we've matched LoH or HT, cast now
		if (spell_to_cast > 0)	
		{
			CastSpell(spell_to_cast, castspell->target_id, castspell->slot);

			if(HasInstantDisc(spell_to_cast))
			{
				FadeDisc();
			}
		}
	}
	else	// MEMORIZED SPELL (first confirm that it's a valid memmed spell slot, then validate that the spell is currently memorized)
	{
		uint16 spell_to_cast = 0;

		if (castspell->slot < MAX_PP_MEMSPELL)
		{
			spell_to_cast = m_pp.mem_spells[castspell->slot];
			if (spell_to_cast != castspell->spell_id)
			{
				InterruptSpell(castspell->spell_id); //CHEATER!!!
				return;
			}
		}
		else if (castspell->slot >= MAX_PP_MEMSPELL) {
			InterruptSpell();
			return;
		}

		CastSpell(spell_to_cast, castspell->target_id, castspell->slot);
	}
	return;
}

void Client::Handle_OP_ChannelMessage(const EQApplicationPacket *app)
{
	ChannelMessage_Struct* cm = (ChannelMessage_Struct*)app->pBuffer;

	if (app->size < sizeof(ChannelMessage_Struct)) {
		std::cout << "Wrong size " << app->size << ", should be " << sizeof(ChannelMessage_Struct) << "+ on 0x" << std::hex << std::setfill('0') << std::setw(4) << app->GetOpcode() << std::dec << std::endl;
		return;
	}
	if (IsAIControlled()) {
		Message(CC_Red, "You try to speak but cant move your mouth!");
		return;
	}

	ChannelMessageReceived(cm->chan_num, cm->language, cm->skill_in_language, cm->message, cm->targetname);
	return;
}

void Client::Handle_OP_ClickDoor(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClickDoor_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_ClickDoor, size=%i, expected %i", app->size, sizeof(ClickDoor_Struct));
		return;
	}
	ClickDoor_Struct* cd = (ClickDoor_Struct*)app->pBuffer;
	Doors* currentdoor = entity_list.FindDoor(cd->doorid);
	if (!currentdoor)
	{
		Message(CC_Default, "Unable to find door, please notify a GM (DoorID: %i).", cd->doorid);
		return;
	}

	char buf[20];
	snprintf(buf, 19, "%u", cd->doorid);
	buf[19] = '\0';
	std::vector<EQEmu::Any> args;
	args.push_back(currentdoor);
	parse->EventPlayer(EVENT_CLICK_DOOR, this, buf, 0, &args);

	currentdoor->HandleClick(this, 0);
	return;
}

void Client::Handle_OP_ClickObject(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(ClickObject_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size on ClickObject_Struct: Expected %i, Got %i",
			sizeof(ClickObject_Struct), app->size);
		return;
	}

	ClickObject_Struct* click_object = (ClickObject_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetID(click_object->drop_id);
	if (entity && entity->IsObject()) {
		Object* object = entity->CastToObject();

		object->HandleClick(this, click_object);

		std::vector<EQEmu::Any> args;
		args.push_back(object);

		char buf[10];
		snprintf(buf, 9, "%u", click_object->drop_id);
		buf[9] = '\0';
		parse->EventPlayer(EVENT_CLICK_OBJECT, this, buf, 0, &args);
	}

	return;
}

void Client::Handle_OP_ClickObjectAction(const EQApplicationPacket *app)
{

	if (app->size != sizeof(ClickObjectAction_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size on OP_ClickObjectAction: Expected %i, Got %i",
			sizeof(ClickObjectAction_Struct), app->size);
		return;
	}

	ClickObjectAction_Struct* oos = (ClickObjectAction_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetEntityObject(oos->drop_id);
	if (entity && entity->IsObject()) {
		Object* object = entity->CastToObject();
		if (oos->open == 0) {
			object->Close();
		}
		else {
			Log.Out(Logs::General, Logs::Error, "Unsupported action %d in OP_ClickObjectAction", oos->open);
		}
	}
	else {
		Log.Out(Logs::Detail, Logs::Error, "Invalid object %d in OP_ClickObjectAction", oos->drop_id);
	}


	SetTradeskillObject(nullptr);

	EQApplicationPacket end_trade1(OP_ClickObjectAction, 0);
	QueuePacket(&end_trade1);

	return;
}

void Client::Handle_OP_ClientError(const EQApplicationPacket *app)
{
	return;
}

void Client::Handle_OP_ClientUpdate(const EQApplicationPacket *app)
{
	if (IsAIControlled() && !has_zomm)
		return;

	if(dead)
		return;

	//currently accepting two sizes, one has an extra byte on the end
	if (app->size != sizeof(SpawnPositionUpdate_Struct)
	&& app->size != (sizeof(SpawnPositionUpdate_Struct)+1)
	) {
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_ClientUpdate expected:%i got:%i", sizeof(SpawnPositionUpdate_Struct), app->size);
		return;
	}
	SpawnPositionUpdate_Struct* ppu = (SpawnPositionUpdate_Struct*)app->pBuffer;

	if(client_position_update)
	{
		std::string name = GetName();
		if(ppu->spawn_id != GetID())
		{
			if(has_zomm)
				name = "Eye_Of_Zomm";
			else if(GetBoatID() != 0)
				name = GetBoatName();
		}

		Log.Out(Logs::Detail, Logs::EQMac, "PositionUpdate for %s(%d): loc: %d,%d,%d,%d delta: %d,%d,%d,%d anim: %d", name.c_str(), ppu->spawn_id, ppu->x_pos, ppu->y_pos, ppu->z_pos, ppu->heading, ppu->delta_x, ppu->delta_y, ppu->delta_z, ppu->delta_heading, ppu->anim_type);
		client_position_update = false;
	}

	/* Web Interface */
	if (IsClient() && RemoteCallSubscriptionHandler::Instance()->IsSubscribed("Client.Position")) {
		std::vector<std::string> params;
		params.push_back(std::to_string((long)GetID()));
		params.push_back(GetCleanName());
		params.push_back(std::to_string((double)ppu->x_pos));
		params.push_back(std::to_string((double)ppu->y_pos));
		params.push_back(std::to_string((double)ppu->z_pos));
		params.push_back(std::to_string((double)m_Position.w));
		params.push_back(std::to_string((double)GetClass()));
		params.push_back(std::to_string((double)GetRace())); 
		RemoteCallSubscriptionHandler::Instance()->OnEvent("Client.Position", params);
	}

	if(ppu->spawn_id != GetID()) 
	{
		if(has_zomm)
		{
			NPC* zommnpc = nullptr;
			if(entity_list.GetZommPet(this, zommnpc))
			{
				if(zommnpc)
				{
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
					SpawnPositionUpdates_Struct* ppus = (SpawnPositionUpdates_Struct*)outapp->pBuffer;
					zommnpc->SetSpawnUpdate(ppu, &ppus->spawn_update);
					ppus->num_updates = 1;
					entity_list.QueueCloseClients(zommnpc,outapp,true,500,this,false);
					safe_delete(outapp);

					pLastUpdate = Timer::GetCurrentTime();
					auto zommPos = glm::vec4(ppu->x_pos, ppu->y_pos, ppu->z_pos/10, ppu->heading);
					entity_list.OpenDoorsNearCoords(zommnpc, zommPos);
					return;
				}
			}
		}
		// check if the id is for a boat the player is controlling
		else if (ppu->spawn_id == BoatID) 
		{
			Mob* boat = entity_list.GetMob(BoatID);
			if (!boat || !boat->IsBoat()) 
			{	// if the boat ID is invalid, reset the id and abort
				BoatID = 0;
				return;
			}

			// set the boat's position deltas
			auto boatDelta = glm::vec4(ppu->delta_x, ppu->delta_y, ppu->delta_z, ppu->delta_heading);
			boat->SetDelta(boatDelta);
			// send an update to everyone nearby except the client controlling the boat
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
			SpawnPositionUpdates_Struct* ppus = (SpawnPositionUpdates_Struct*)outapp->pBuffer;
			boat->MakeSpawnUpdate(&ppus->spawn_update);
			ppus->num_updates = 1;
			entity_list.QueueCloseClients(boat,outapp,true,300,this,false);
			safe_delete(outapp);
			pLastUpdate = Timer::GetCurrentTime();
			// update the boat's position on the server, without sending an update
			boat->GMMove(ppu->x_pos, ppu->y_pos, ppu->z_pos, EQ19toFloat(ppu->heading), false);
			return;
		}
		else return;	// if not a boat, do nothing
	}

	// Send our original position to others so we don't disappear.
	if(has_zomm)
	{
		SendPosUpdate();
		pLastUpdate = Timer::GetCurrentTime();
		return;
	}

	m_EQPosition = glm::vec4(ppu->x_pos, ppu->y_pos, ppu->z_pos, ppu->heading);
	m_EQPosition.z = (float)ppu->z_pos / 10.0f;
	ppu->z_pos /= 10;

	float dist = 0;
	float tmp;
	tmp = m_Position.x - ppu->x_pos;
	dist += tmp*tmp;
	tmp = m_Position.y - ppu->y_pos;
	dist += tmp*tmp;
	dist = sqrt(dist);

	//the purpose of this first block may not be readily apparent
	//basically it's so people don't do a moderate warp every 2.5 seconds
	//letting it even out and basically getting the job done without triggering
	if(dist == 0)
	{
		if(m_DistanceSinceLastPositionCheck > 0.0)
		{
			uint32 cur_time = Timer::GetCurrentTime();
			if((cur_time - m_TimeSinceLastPositionCheck) > 0)
			{
				float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
				float runs = GetRunspeed();
				if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
				{
					if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
					{
						if(IsShadowStepExempted())
						{
							if(m_DistanceSinceLastPositionCheck > 800)
							{
								CheatDetected(MQWarpShadowStep, ppu->x_pos, ppu->y_pos, ppu->z_pos);
							}
						}
						else if(IsKnockBackExempted())
						{
							//still potential to trigger this if you're knocked back off a
							//HUGE fall that takes > 2.5 seconds
							if(speed > 30.0f)
							{
								CheatDetected(MQWarpKnockBack, ppu->x_pos, ppu->y_pos, ppu->z_pos);
							}
						}
						else if(!IsPortExempted())
						{
							if(!IsMQExemptedArea(zone->GetZoneID(), ppu->x_pos, ppu->y_pos, ppu->z_pos))
							{
								if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
								{
									m_TimeSinceLastPositionCheck = cur_time;
									m_DistanceSinceLastPositionCheck = 0.0f;
									CheatDetected(MQWarp, ppu->x_pos, ppu->y_pos, ppu->z_pos);
									//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
								}
								else
								{
									CheatDetected(MQWarpLight, ppu->x_pos, ppu->y_pos, ppu->z_pos);
								}
							}
						}
					}
				}
				SetShadowStepExemption(false);
				SetKnockBackExemption(false);
				SetPortExemption(false);
				m_TimeSinceLastPositionCheck = cur_time;
				m_DistanceSinceLastPositionCheck = 0.0f;
				m_CheatDetectMoved = false;
			}
		}
		else
		{
			m_TimeSinceLastPositionCheck = Timer::GetCurrentTime();
			m_CheatDetectMoved = false;
		}
	}
	else
	{
		m_DistanceSinceLastPositionCheck += dist;
		m_CheatDetectMoved = true;
		if(m_TimeSinceLastPositionCheck == 0)
		{
			m_TimeSinceLastPositionCheck = Timer::GetCurrentTime();
		}
		else
		{
			uint32 cur_time = Timer::GetCurrentTime();
			if((cur_time - m_TimeSinceLastPositionCheck) > 2500)
			{
				float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
				float runs = GetRunspeed();
				if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
				{
					if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
					{
						if(IsShadowStepExempted())
						{
							if(m_DistanceSinceLastPositionCheck > 800)
							{
								//if(!IsMQExemptedArea(zone->GetZoneID(), ppu->x_pos, ppu->y_pos, ppu->z_pos))
								//{
									CheatDetected(MQWarpShadowStep, ppu->x_pos, ppu->y_pos, ppu->z_pos);
									//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
								//}
							}
						}
						else if(IsKnockBackExempted())
						{
							//still potential to trigger this if you're knocked back off a
							//HUGE fall that takes > 2.5 seconds
							if(speed > 30.0f)
							{
								CheatDetected(MQWarpKnockBack, ppu->x_pos, ppu->y_pos, ppu->z_pos);
							}
						}
						else if(!IsPortExempted())
						{
							if(!IsMQExemptedArea(zone->GetZoneID(), ppu->x_pos, ppu->y_pos, ppu->z_pos))
							{
								if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
								{
									m_TimeSinceLastPositionCheck = cur_time;
									m_DistanceSinceLastPositionCheck = 0.0f;
									CheatDetected(MQWarp, ppu->x_pos, ppu->y_pos, ppu->z_pos);
									//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
								}
								else
								{
									CheatDetected(MQWarpLight, ppu->x_pos, ppu->y_pos, ppu->z_pos);
								}
							}
						}
					}
				}
				SetShadowStepExemption(false);
				SetKnockBackExemption(false);
				SetPortExemption(false);
				m_TimeSinceLastPositionCheck = cur_time;
				m_DistanceSinceLastPositionCheck = 0.0f;
			}
		}

		if(IsDraggingCorpse())
			DragCorpses();
	}

	//Check to see if PPU should trigger an update to the rewind position.
	float rewind_x_diff = 0;
	float rewind_y_diff = 0;

	rewind_x_diff = ppu->x_pos - m_RewindLocation.x;
	rewind_x_diff *= rewind_x_diff;
	rewind_y_diff = ppu->y_pos - m_RewindLocation.y;
	rewind_y_diff *= rewind_y_diff;

	//We only need to store updated values if the player has moved.
	//If the player has moved more than units for x or y, then we'll store
	//his pre-PPU x and y for /rewind, in case he gets stuck.
	if ((rewind_x_diff > 750) || (rewind_y_diff > 750))
		m_RewindLocation = glm::vec3(m_Position);

	//If the PPU was a large jump, such as a cross zone gate or Call of Hero,
	//just update rewind coords to the new ppu coords. This will prevent exploitation.

	if ((rewind_x_diff > 5000) || (rewind_y_diff > 5000))
		m_RewindLocation = glm::vec3(ppu->x_pos, ppu->y_pos, ppu->z_pos);

	if(proximity_timer.Check()) {
		entity_list.ProcessMove(this, glm::vec3(ppu->x_pos, ppu->y_pos, ppu->z_pos));

		m_Proximity = glm::vec3(ppu->x_pos, ppu->y_pos, ppu->z_pos);
	}

	bool send_update = (m_Position.w != ppu->heading && ppu->delta_heading == 0) || (ppu->delta_heading != m_Delta.w) || 
		(ppu->y_pos != m_Position.y || ppu->x_pos != m_Position.x || ppu->anim_type != animation);
	// Update internal state
	m_Delta = glm::vec4(static_cast<int16>((ppu->delta_x & 0x200) ? (0x3FF & ppu->delta_x) & 0xFC00 : (0x3FF & ppu->delta_x)),
						static_cast<int16>((ppu->delta_y & 0x400) ? (0x7FF & ppu->delta_y) & 0xF800 : (0x7FF & ppu->delta_y)),
						static_cast<int16>((ppu->delta_z & 0x400) ? (0x7FF & ppu->delta_z) & 0xF800 : (0x7FF & ppu->delta_z)), 
						ppu->delta_heading);
	uint8 sendtoself = 0;
	m_Position.w = ppu->heading;

	//Adjust the heading for the Paineel portal near the SK guild
	if(zone->GetZoneID() == paineel && 
		((GetX() > 519 && GetX() < 530) || (GetY() > 952 && GetY() < 965)))
	{
		float tmph = GetPortHeading(ppu->x_pos, ppu->y_pos);
		if(tmph > 0)
		{
			m_Position.w = tmph;
			sendtoself = 1;
		}

	}
	
	// Break Hide, Trader, and fishing mode if moving and set rewind timer
	if(ppu->y_pos != m_Position.y || ppu->x_pos != m_Position.x)
	{
		if((hidden || improved_hidden) && !sneaking)
		{
			hidden = false;
			improved_hidden = false;
			if(!invisible) 
			{
				EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
				SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
				sa_out->spawn_id = GetID();
				sa_out->type = 0x03;
				sa_out->parameter = 0;
				entity_list.QueueClients(this, outapp, true);
				safe_delete(outapp);
			}
		}
		if(Trader)
			Trader_EndTrader();

		if(fishing_timer.Enabled())
		{
			if(GetBoatNPCID() == 0 || (GetBoatNPCID() != 0 && ppu->anim_type != 0))
			{
				fishing_timer.Disable();
			}
		}

		rewind_timer.Start(30000, true);
	}

	float water_x = m_Position.x;
	float water_y = m_Position.y;

	m_Position.x = ppu->x_pos;
	m_Position.y = ppu->y_pos;
	m_Position.z = m_EQPosition.z;

	animation = ppu->anim_type;
	// No need to check for loc change, our client only sends this packet if it has actually moved in some way.
	if (send_update) {
		SendPosUpdate(sendtoself);
		pLastUpdate = Timer::GetCurrentTime();
	}
	
	if(zone->watermap)
	{
		auto water_pos = glm::vec3(m_Position.x, m_Position.y, m_Position.z - 0.5);
		if(zone->watermap->InLiquid(water_pos) && ((ppu->x_pos != water_x) || (ppu->y_pos != water_y)))
		{
			// Update packets happen so quickly, that we have to limit here or else swimming skillups are super fast.
			if(zone->random.Roll(40))
			{
				CheckIncreaseSkill(SkillSwimming, nullptr, zone->skill_difficulty[SkillSwimming].difficulty, 2.0);
			}
		}
	}

	return;
}

void Client::Handle_OP_CombatAbility(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(CombatAbility_Struct)) {
		std::cout << "Wrong size on OP_CombatAbility. Got: " << app->size << ", Expected: " << sizeof(CombatAbility_Struct) << std::endl;
		return;
	}

	/* Web Interface: Combat State */
	if (RemoteCallSubscriptionHandler::Instance()->IsSubscribed("Combat.States")) {
		bool wi_aa = false;
		if (app->pBuffer[0] == 0){ wi_aa = false; }
		else if (app->pBuffer[0] == 1){ wi_aa = true; }
		std::vector<std::string> params;
		params.push_back(std::to_string((long)GetID()));
		params.push_back(std::to_string((bool)wi_aa));
		RemoteCallSubscriptionHandler::Instance()->OnEvent("Combat.States", params);
	}

	//This came down in an web interface cherry pick. no clue what it does, but if it's important uncomment.
	/*
	if (app->pBuffer[0] == 0)
	{
		auto_attack = false;

		if (IsAIControlled())
			return;
		attack_timer.Disable();
		ranged_timer.Disable();
		attack_dw_timer.Disable();

        m_AutoAttackPosition = xyz_heading::Origin();
        m_AutoAttackTargetLocation = xyz_location::Origin();
		aa_los_them_mob = nullptr;
	}
	else if (app->pBuffer[0] == 1)
	{
		auto_attack = true;
		auto_fire = false;
		if (IsAIControlled())
			return;
		SetAttackTimer();

		if(GetTarget())
		{
			aa_los_them_mob = GetTarget();
			m_AutoAttackPosition = GetPosition();
			m_AutoAttackTargetLocation = aa_los_them_mob->GetPosition();
			los_status = CheckLosFN(aa_los_them_mob);
			los_status_facing = IsFacingMob(aa_los_them_mob);
		}
		else
		{
			m_AutoAttackPosition = GetPosition();
			m_AutoAttackTargetLocation = xyz_location::Origin();
			aa_los_them_mob = nullptr;
			los_status = false;
			los_status_facing = false;
		}
	}*/

	OPCombatAbility(app);
	return;
}

void Client::Handle_OP_AutoAttack2(const EQApplicationPacket *app)
{
	return;
}

void Client::Handle_OP_Consent(const EQApplicationPacket *app)
{
	if(app->size<64)
	{
		Consent_Struct* c = (Consent_Struct*)app->pBuffer;

		std::string cname = c->name;
		std::transform(cname.begin(), cname.end(), cname.begin(), ::tolower);
		std::string oname = GetName();
		std::transform(oname.begin(), oname.end(), oname.begin(), ::tolower);

		if(cname != oname)
		{
			//Offline consent
			if(database.GetAccountIDByChar(c->name) == AccountID())
			{
				uint32 charid = database.GetCharacterID(c->name);
				char ownername[64];
				strcpy(ownername, GetName());
				Consent(1, ownername, charid);
				Message_StringID(CC_Default, CONSENT_GIVEN, c->name);
			}
			else
			{
				ServerPacket* pack = new ServerPacket(ServerOP_Consent, sizeof(ServerOP_Consent_Struct));
				ServerOP_Consent_Struct* scs = (ServerOP_Consent_Struct*)pack->pBuffer;
				strcpy(scs->grantname, c->name);
				strcpy(scs->ownername, GetName());
				scs->message_string_id = 0;
				scs->permission = 1;
				scs->zone_id = zone->GetZoneID();
				scs->instance_id = zone->GetInstanceID();
				//consent_list.push_back(scs->grantname);
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
		}
		else 
		{
			Message_StringID(CC_Default, CONSENT_YOURSELF);
		}
	}
	return;
}

void Client::Handle_OP_Consider(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Consider_Struct))
	{
		Log.Out(Logs::General, Logs::None, "Size mismatch in Consider expected %i got %i", sizeof(Consider_Struct), app->size);
		return;
	}
	Consider_Struct* conin = (Consider_Struct*)app->pBuffer;
	Mob* tmob = entity_list.GetMob(conin->targetid);
	if (tmob == 0)
		return;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Consider, sizeof(Consider_Struct));
	Consider_Struct* con = (Consider_Struct*)outapp->pBuffer;
	con->playerid = GetID();
	con->targetid = conin->targetid;
	if (tmob->IsNPC())
		con->faction = GetFactionLevel(character_id, tmob->GetNPCTypeID(), race, class_, deity, (tmob->IsNPC()) ? tmob->CastToNPC()->GetPrimaryFaction() : 0, tmob); // Dec. 20, 2001; TODO: Send the players proper deity
	else
		con->faction = 1;
	con->level = GetLevelCon(tmob->GetLevel());
	if (zone->IsPVPZone()) {
		if (!tmob->IsNPC())
			con->pvpcon = tmob->CastToClient()->GetPVP();
	}

	// Mongrel: If we're feigned show NPC as indifferent
	if (tmob->IsNPC())
	{
		if (GetFeigned())
			con->faction = FACTION_INDIFFERENT;
	}

	if (!(con->faction == FACTION_SCOWLS))
	{
		if (tmob->IsNPC())
		{
			if (tmob->CastToNPC()->IsOnHatelist(this))
				con->faction = FACTION_THREATENLY;
		}
	}

	if (con->faction == FACTION_APPREHENSIVE) {
		con->faction = FACTION_SCOWLS;
	}
	else if (con->faction == FACTION_DUBIOUS) {
		con->faction = FACTION_THREATENLY;
	}
	else if (con->faction == FACTION_SCOWLS) {
		con->faction = FACTION_APPREHENSIVE;
	}
	else if (con->faction == FACTION_THREATENLY) {
		con->faction = FACTION_DUBIOUS;
	}

	mod_consider(tmob, con);

	QueuePacket(outapp);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_ConsiderCorpse(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Consider_Struct))
	{
		Log.Out(Logs::General, Logs::None, "Size mismatch in Consider corpse expected %i got %i", sizeof(Consider_Struct), app->size);
		return;
	}
	Consider_Struct* conin = (Consider_Struct*)app->pBuffer;
	Corpse* tcorpse = entity_list.GetCorpseByID(conin->targetid);
	if (tcorpse && tcorpse->IsNPCCorpse()) {
		uint32 min; uint32 sec; uint32 ttime;
		if ((ttime = tcorpse->GetDecayTime()) != 0) {
			sec = (ttime / 1000) % 60; // Total seconds
			min = (ttime / 60000) % 60; // Total seconds / 60 drop .00
			char val1[20] = { 0 };
			char val2[20] = { 0 };
			Message_StringID(CC_Default, CORPSE_DECAY1, ConvertArray(min, val1), ConvertArray(sec, val2));
		}
		else {
			Message_StringID(CC_Default, CORPSE_DECAY_NOW);
		}
	}
	else if (tcorpse && tcorpse->IsPlayerCorpse()) {
		uint32 day, hour, min, sec, ttime;
		if ((ttime = tcorpse->GetRemainingRezTime()) > 0)
		{
			sec = (ttime / 1000) % 60; // Total seconds
			min = (ttime / 60000) % 60; // Total seconds
			hour = (ttime / 3600000) % 24; // Total hours
			if (hour)
				Message(CC_Default, "This corpse's resurrection time will expire in %i hour(s) %i minute(s) and %i seconds.", hour, min, sec);
			else
				Message(CC_Default, "This corpse's resurrection time will expire in %i minute(s) and %i seconds.", min, sec);

			hour = 0;
		}
		else
			Message(CC_Default, "This corpse is too old to be resurrected.");

		if ((ttime = tcorpse->GetDecayTime()) != 0) {
			sec = (ttime / 1000) % 60; // Total seconds
			min = (ttime / 60000) % 60; // Total seconds
			hour = (ttime / 3600000) % 24; // Total hours
			day = ttime / 86400000; // Total Days
			if (day)
				Message(CC_Default, "This corpse will decay in %i day(s) %i hour(s) %i minute(s) and %i seconds.", day, hour, min, sec);
			else if (hour)
				Message(CC_Default, "This corpse will decay in %i hour(s) %i minute(s) and %i seconds.", hour, min, sec);
			else
				Message(CC_Default, "This corpse will decay in %i minute(s) and %i seconds.", min, sec);

			hour = 0;
		}
		else {
			Message_StringID(CC_Default, CORPSE_DECAY_NOW);
		}
	}
}

void Client::Handle_OP_Consume(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Consume_Struct))
	{
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_Consume expected:%i got:%i", sizeof(Consume_Struct), app->size);
		return;
	}
	Consume_Struct* pcs = (Consume_Struct*)app->pBuffer;
	Log.Out(Logs::Detail, Logs::Debug, "Hit Consume! How consumed: %i. Slot: %i. Type: %i", pcs->auto_consumed, pcs->slot, pcs->type);
	int value = RuleI(Character, ConsumptionValue);

	m_pp.fatigue = GetFatiguePercent();

	// We don't want to change the pp for hunger or thirst, since we want to be able to go past the client's maximium value internally.
	if (pcs->type == 0x01)
	{
		if (m_pp.hunger_level > value)
		{
			m_pp.hunger_level = value;
			EQApplicationPacket *outapp;
			outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
			Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
			sta->food = value;
			sta->water = m_pp.thirst_level> value ? value : m_pp.thirst_level;
			sta->fatigue = m_pp.fatigue;

			QueuePacket(outapp, false);
			safe_delete(outapp);
			return;
		}
	}
	else if (pcs->type == 0x02)
	{
		if (m_pp.thirst_level > value)
		{
			EQApplicationPacket *outapp;
			outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
			Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
			sta->food = m_pp.hunger_level > value ? value : m_pp.hunger_level;
			sta->water = value;
			sta->fatigue = m_pp.fatigue;

			QueuePacket(outapp, false);
			safe_delete(outapp);
			return;
		}
	}

	ItemInst *myitem = GetInv().GetItem(pcs->slot);
	if (myitem == nullptr) {
		Log.Out(Logs::General, Logs::Error, "Consuming from empty slot %d", pcs->slot);
		return;
	}

	const Item_Struct* eat_item = myitem->GetItem();
	if (pcs->type == 0x01) {
		Consume(eat_item, ItemTypeFood, pcs->slot, (pcs->auto_consumed == 0xffffffff));
	}
	else if (pcs->type == 0x02) {
		Consume(eat_item, ItemTypeDrink, pcs->slot, (pcs->auto_consumed == 0xffffffff));
	}
	else {
		Log.Out(Logs::General, Logs::Error, "OP_Consume: unknown type, type:%i", (int)pcs->type);
		return;
	}

	SendStaminaUpdate();
	return;
}

void Client::Handle_OP_ControlBoat(const EQApplicationPacket *app)
{
	if (app->size != sizeof(OldControlBoat_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_ControlBoat, size=%i, expected %i", app->size, sizeof(OldControlBoat_Struct));
		return;
	}

	bool TakeControl = false;
	Mob* boat = 0;
	int unknown5 = 0;
	int16 boatid = 0;
	OldControlBoat_Struct* cbs = (OldControlBoat_Struct*)app->pBuffer;
	boat = entity_list.GetMob(cbs->boatId);
	TakeControl = cbs->TakeControl;
	unknown5 = cbs->unknown5;
	boatid = cbs->boatId;

	if (boat == 0)
		return;	// do nothing if the boat isn't valid

	if (!boat->IsNPC() || (boat->GetRace() != CONTROLLED_BOAT))
	{
		char *hacked_string = nullptr;
		MakeAnyLenString(&hacked_string, "OP_Control Boat was sent against %s which is of race %u", boat->GetName(), boat->GetRace());
		database.SetMQDetectionFlag(this->AccountName(), this->GetName(), hacked_string, zone->GetShortName());
		safe_delete_array(hacked_string);
		return;
	}

	if (TakeControl) {
		// this uses the boat's target to indicate who has control of it. It has to check hate to make sure the boat isn't actually attacking anyone.
		if ((boat->GetTarget() == 0) || (boat->GetTarget() == this && boat->GetHateAmount(this) == 0)) {
			boat->SetTarget(this);
		}
		else {
			this->Message_StringID(CC_Red, IN_USE);
			return;
		}
	}
	else
		boat->SetTarget(0);

	EQApplicationPacket* outapp;
	outapp = new EQApplicationPacket(OP_ControlBoat, sizeof(OldControlBoat_Struct));
	OldControlBoat_Struct* cbs2 = (OldControlBoat_Struct*)outapp->pBuffer;
	cbs2->boatId = boatid;
	cbs2->TakeControl = TakeControl;
	cbs2->unknown5 = unknown5;

	FastQueuePacket(&outapp);
	// have the boat signal itself, so quests can be triggered by boat use
	boat->CastToNPC()->SignalNPC(0);
}

void Client::Handle_OP_CorpseDrag(const EQApplicationPacket *app)
{
	if (DraggedCorpses.size() >= (unsigned int)RuleI(Character, MaxDraggedCorpses))
	{
		Message_StringID(CC_Red, CORPSEDRAG_LIMIT);
		return;
	}

	VERIFY_PACKET_LENGTH(OP_CorpseDrag, app, CorpseDrag_Struct);

	CorpseDrag_Struct *cds = (CorpseDrag_Struct*)app->pBuffer;

	Mob* corpse = entity_list.GetMob(cds->CorpseName);

	if (!corpse || !corpse->IsPlayerCorpse() || corpse->CastToCorpse()->IsBeingLooted())
		return;

	Client *c = entity_list.FindCorpseDragger(corpse->GetID());

	if (c)
	{
		if (c == this)
			Message_StringID(MT_DefaultText, CORPSEDRAG_ALREADY, corpse->GetCleanName());
		else
			Message_StringID(MT_DefaultText, CORPSEDRAG_SOMEONE_ELSE, corpse->GetCleanName());

		return;
	}

	if (!corpse->CastToCorpse()->Summon(this, false, true))
		return;

	DraggedCorpses.push_back(std::pair<std::string, uint16>(cds->CorpseName, corpse->GetID()));

	Message_StringID(MT_DefaultText, CORPSEDRAG_BEGIN, cds->CorpseName);
}

void Client::Handle_OP_CreateObject(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	DropItem(MainCursor);
	return;
}

void Client::Handle_OP_Damage(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	Handle_OP_EnvDamage(app);
	return;
}

void Client::Handle_OP_Death(const EQApplicationPacket *app)
{
	bool EnvDeath = false;
	Log.Out(Logs::Detail, Logs::Debug, "Client hit OP_Death");
	if (app->size != sizeof(OldDeath_Struct))
	{
		Log.Out(Logs::Detail, Logs::Debug, "Handle_OP_Death: Struct is incorrect, expected %i got %i", sizeof(OldDeath_Struct), app->size);
		return;
	}
	OldDeath_Struct* ds = (OldDeath_Struct*)app->pBuffer;

	//Burning, Drowning, Falling, Freezing
	if (ds->attack_skill >= 250 && ds->attack_skill <= 255)
	{
		EnvDeath = true;
	}
	//I think this attack_skill value is really a value from SkillDamageTypes...
	else if (ds->attack_skill > HIGHEST_SKILL)
	{
		return;
	}

	Mob* killer = entity_list.GetMob(ds->killer_id);
	if (EnvDeath == true)
	{
		mod_client_death_env();
		Death(0, 32000, SPELL_UNKNOWN, SkillHandtoHand, Killed_ENV);
		return;
	}
	else
	{
		Death(killer, ds->damage, ds->spell_id, (SkillUseTypes)ds->attack_skill);
		return;
	}
}

void Client::Handle_OP_DeleteCharge(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MoveItem_Struct)) {
		std::cout << "Wrong size on OP_DeleteCharge. Got: " << app->size << ", Expected: " << sizeof(MoveItem_Struct) << std::endl;
		return;
	}

	MoveItem_Struct* alc = (MoveItem_Struct*)app->pBuffer;

	const ItemInst *inst = GetInv().GetItem(alc->from_slot);
	if (inst && inst->GetItem()->ItemType == ItemTypeAlcohol) {
		entity_list.MessageClose_StringID(this, true, 50, 0, DRINKING_MESSAGE, GetName(), inst->GetItem()->Name);
		CheckIncreaseSkill(SkillAlcoholTolerance, nullptr, zone->skill_difficulty[SkillAlcoholTolerance].difficulty);

		int16 AlcoholTolerance = GetSkill(SkillAlcoholTolerance);
		int16 IntoxicationIncrease;

		IntoxicationIncrease = (200 - AlcoholTolerance) * 30 / 200 + 10;

		if (IntoxicationIncrease < 0)
			IntoxicationIncrease = 1;

		m_pp.intoxication += IntoxicationIncrease;

		if (m_pp.intoxication > 200)
			m_pp.intoxication = 200;
	}

	if (inst)
		DeleteItemInInventory(alc->from_slot, 1);

	return;
}

void Client::Handle_OP_DeleteSpawn(const EQApplicationPacket *app)
{
	// The client will send this with his id when he zones, maybe when he disconnects too?
	//eqs->RemoveData(); // Flushing the queue of packet data to allow for proper zoning
	//hate_list.RemoveEnt(this->CastToMob());
	//Disconnect();
	return;
}

void Client::Handle_OP_DeleteSpell(const EQApplicationPacket *app)
{
	if (app->size != sizeof(DeleteSpell_Struct))
		return;

	EQApplicationPacket* outapp = app->Copy();
	DeleteSpell_Struct* dss = (DeleteSpell_Struct*)outapp->pBuffer;

	if (dss->spell_slot < 0 || dss->spell_slot > int(MAX_PP_SPELLBOOK))
		return;

	if (m_pp.spell_book[dss->spell_slot] != SPELLBOOK_UNKNOWN) 
	{
		database.DeleteCharacterSpell(this->CharacterID(), m_pp.spell_book[dss->spell_slot], dss->spell_slot);
		dss->success = 1;
	}
	else
		dss->success = 0;

	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_DisarmTraps(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (!HasSkill(SkillDisarmTraps))
		return;

	if (!p_timers.Expired(&database, pTimerDisarmTraps, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	int reuse = DisarmTrapsReuseTime;
	switch (GetAA(aaAdvTrapNegotiation)) {
	case 1:
		reuse -= 1;
		break;
	case 2:
		reuse -= 3;
		break;
	case 3:
		reuse -= 5;
		break;
	}
	p_timers.Start(pTimerDisarmTraps, reuse - 1);

	float success = 2.0;
	Trap* trap = entity_list.FindNearbyTrap(this, 60);
	if (trap && trap->detected)
	{
		int uskill = GetSkill(SkillDisarmTraps);
		if ((zone->random.Int(0, 49) + uskill) >= (zone->random.Int(0, 49) + trap->skill))
		{
			success = 1.0;
			Message(MT_Skills, "You disarm a trap.");
			trap->disarmed = true;
			Log.Out(Logs::General, Logs::Traps, "Trap %d is disarmed.", trap->trap_id);
			trap->UpdateTrap();
		}
		else
		{
			if (zone->random.Int(0, 99) < 25){
				Message(MT_Skills, "You set off the trap while trying to disarm it!");
				trap->Trigger(this);
			}
			else{
				Message(MT_Skills, "You failed to disarm a trap.");
			}
		}
		CheckIncreaseSkill(SkillDisarmTraps, nullptr, zone->skill_difficulty[SkillDisarmTraps].difficulty, success);
		return;
	}
	Message(MT_Skills, "You did not find any traps close enough to disarm.");
	return;
}

void Client::Handle_OP_Discipline(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	ClientDiscipline_Struct* cds = (ClientDiscipline_Struct*)app->pBuffer;

	uint32 remain = p_timers.GetRemainingTime(pTimerDisciplineReuseStart);
	if(remain > 0 && !GetGM())
	{
		char val1[20]={0};
		char val2[20]={0};
		Log.Out(Logs::General, Logs::Discs, "Disc reuse time not yet met. %d", remain);
		Message_StringID(CC_User_Disciplines, DISCIPLINE_CANUSEIN, ConvertArray((remain)/60,val1), ConvertArray(remain%60,val2));
		return;
	}

	if (cds->disc_id > 0)
	{
		Log.Out(Logs::General, Logs::Discs, "Attempting to cast Disc %d.", cds->disc_id);
		UseDiscipline(cds->disc_id);
	}
	else
	{
		Log.Out(Logs::General, Logs::Discs, "No disc used and reuse time is met.");
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_InterruptCast, sizeof(InterruptCast_Struct));
		InterruptCast_Struct* ic = (InterruptCast_Struct*)outapp->pBuffer;
		ic->entityid = GetID();
		ic->messageid = DISCIPLINE_RDY;
		ic->color = CC_User_Disciplines;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::Handle_OP_DuelResponse(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(DuelResponse_Struct))
		return;
	DuelResponse_Struct* ds = (DuelResponse_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetID(ds->target_id);
	Entity* initiator = entity_list.GetID(ds->entity_id);
	if (!entity->IsClient() || !initiator->IsClient())
		return;

	entity->CastToClient()->SetDuelTarget(0);
	entity->CastToClient()->SetDueling(false);
	initiator->CastToClient()->SetDuelTarget(0);
	initiator->CastToClient()->SetDueling(false);
	if (GetID() == initiator->GetID())
		entity->CastToClient()->Message_StringID(CC_Default, DUEL_DECLINE, initiator->GetName());
	else
		initiator->CastToClient()->Message_StringID(CC_Default, DUEL_DECLINE, entity->GetName());
	return;
}

void Client::Handle_OP_DuelResponse2(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(Duel_Struct))
		return;

	Duel_Struct* ds = (Duel_Struct*)app->pBuffer;
	Entity* entity = entity_list.GetID(ds->duel_target);
	Entity* initiator = entity_list.GetID(ds->duel_initiator);

	if (entity && initiator && entity == this && initiator->IsClient()) {
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_RequestDuel, sizeof(Duel_Struct));
		Duel_Struct* ds2 = (Duel_Struct*)outapp->pBuffer;

		ds2->duel_initiator = entity->GetID();
		ds2->duel_target = entity->GetID();
		initiator->CastToClient()->QueuePacket(outapp);

		outapp->SetOpcode(OP_DuelResponse2);
		ds2->duel_initiator = initiator->GetID();

		initiator->CastToClient()->QueuePacket(outapp);

		QueuePacket(outapp);
		SetDueling(true);
		initiator->CastToClient()->SetDueling(true);
		SetDuelTarget(ds->duel_initiator);
		safe_delete(outapp);

		if (IsCasting())
			InterruptSpell();
		if (initiator->CastToClient()->IsCasting())
			initiator->CastToClient()->InterruptSpell();
	}
	return;
}

void Client::Handle_OP_Emote(const EQApplicationPacket *app)
{
	if (app->size != sizeof(OldEmote_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized ""OP_Emote: got %d, expected %d", app->size, sizeof(OldEmote_Struct));
		DumpPacket(app);
		return;
	}

	// Calculate new packet dimensions
	OldEmote_Struct* in = (OldEmote_Struct*)app->pBuffer;
	in->message[1023] = '\0';

	const char* name = GetName();
	uint32 len_name = strlen(name);
	uint32 len_msg = strlen(in->message);
	// crash protection -- cheater
	if (len_msg > 512) {
		in->message[512] = '\0';
		len_msg = 512;
	}
	uint32 len_packet = sizeof(in->unknown01) + len_name
		+ len_msg + 1;

	// Construct outgoing packet
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Emote, len_packet);
	OldEmote_Struct* out = (OldEmote_Struct*)outapp->pBuffer;
	out->unknown01 = in->unknown01;
	memcpy(out->message, name, len_name);
	memcpy(&out->message[len_name], in->message, len_msg);

	entity_list.QueueCloseClients(this, outapp, true, 100, 0, true, FilterSocials);

	safe_delete(outapp);
	return;
}

void Client::Handle_OP_EndLootRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(uint16) && app->size != sizeof(uint32)) {
		std::cout << "Wrong size: OP_EndLootRequest, size=" << app->size << ", expected " << sizeof(uint16) << std::endl;
		return;
	}

	SetLooting(0);

	Entity* entity = entity_list.GetID(*((uint16*)app->pBuffer));
	if (entity == 0) {
		//Message(CC_Red, "Error: OP_EndLootRequest: Corpse not found (ent = 0)");
		Corpse::SendLootReqErrorPacket(this);
		return;
	}
	else if (!entity->IsCorpse()) {
		//Message(CC_Red, "Error: OP_EndLootRequest: Corpse not found (!entity->IsCorpse())");
		Corpse::SendLootReqErrorPacket(this);
		return;
	}
	else {
		entity->CastToCorpse()->EndLoot(this, app);
	}
	return;
}

void Client::Handle_OP_EnvDamage(const EQApplicationPacket *app)
{
	if (!ClientFinishedLoading() || (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0))
	{
		SetHP(GetHP() - 1);
		return;
	}

	if (app->size != sizeof(EnvDamage2_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized OP_EnvDamage: got %d, expected %d", app->size,
			sizeof(EnvDamage2_Struct));
		DumpPacket(app);
		return;
	}
	EnvDamage2_Struct* ed = (EnvDamage2_Struct*)app->pBuffer;

	int damage = ed->damage;
	if (ed->dmgtype == 252) {

		switch (GetAA(aaAcrobatics)) { //Don't know what acrobatics effect is yet but it should be done client side via aa effect.. till then
		case 1:
			damage = damage * 95 / 100;
			break;
		case 2:
			damage = damage * 90 / 100;
			break;
		case 3:
			damage = damage * 80 / 100;
			break;
		}
	}

	if (damage < 0)
		damage = 31337;

	if (admin >= minStatusToAvoidFalling && GetGM())
	{
		Message(CC_Red, "Your GM status protects you from %i points of type %i environmental damage.", ed->damage, ed->dmgtype);
		SetHP(GetHP() - 1);//needed or else the client wont acknowledge
		return;
	}
	else if (GetInvul()) 
	{
		Message(CC_Red, "Your invuln status protects you from %i points of type %i environmental damage.", ed->damage, ed->dmgtype);
		SetHP(GetHP() - 1);//needed or else the client wont acknowledge
		return;
	}

	else if (zone->GetZoneID() == tutorial || zone->GetZoneID() == load)
	{
		return;
	}
	else
	{
		SetHP(GetHP() - (damage * RuleR(Character, EnvironmentDamageMulipliter)));

		/* EVENT_ENVIRONMENTAL_DAMAGE */
		int final_damage = (damage * RuleR(Character, EnvironmentDamageMulipliter));
		char buf[24];
		snprintf(buf, 23, "%u %u %i", ed->damage, ed->dmgtype, final_damage); 
		parse->EventPlayer(EVENT_ENVIRONMENTAL_DAMAGE, this, buf, 0);
	}

	if (GetHP() <= 0) 
	{
		mod_client_death_env(); 
		Death(0, 32000, SPELL_UNKNOWN, SkillHandtoHand);
	}
	SendHPUpdate();
	return;
}

void Client::Handle_OP_FaceChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(FaceChange_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for OP_FaceChange: Expected: %i, Got: %i",
			sizeof(FaceChange_Struct), app->size);
		return;
	}

	// Notify other clients in zone
	entity_list.QueueClients(this, app, false);

	FaceChange_Struct* fc = (FaceChange_Struct*)app->pBuffer;
	m_pp.haircolor = fc->haircolor;
	m_pp.beardcolor = fc->beardcolor;
	m_pp.eyecolor1 = fc->eyecolor1;
	m_pp.eyecolor2 = fc->eyecolor2;
	m_pp.hairstyle = fc->hairstyle;
	m_pp.face = fc->face;
	m_pp.beard = fc->beard;
	Save();

	return;
}

void Client::Handle_OP_FeignDeath(const EQApplicationPacket *app)
{
	if (GetClass() != MONK)
		return;
	if (!p_timers.Expired(&database, pTimerFeignDeath, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}

	int reuse = FeignDeathReuseTime;
	switch (GetAA(aaRapidFeign))
	{
	case 1:
		reuse -= 1;
		break;
	case 2:
		reuse -= 2;
		break;
	case 3:
		reuse -= 5;
		break;
	}
	p_timers.Start(pTimerFeignDeath, reuse - 1);

	float feignbase = 120.0f;
	uint16 skill = GetSkill(SkillFeignDeath);
	float feignchance = 0.0f;

	if (skill > 100)
		feignchance = (int16)skill - 100.0f;

	feignchance = feignchance / 3.0f;

	float totalfeign = feignbase + feignchance;

	// We will always have at least a 5% chance of failure.
	if(totalfeign >= 143)
		totalfeign = 142.5f;

	float success = 2.0;
	if (auto_attack || zone->random.Real(0, 150) > totalfeign) {
		SetFeigned(false);
		entity_list.MessageClose_StringID(this, false, 200, 10, STRING_FEIGNFAILED, GetName());
	}
	else {
		success = 1.0;
		SetFeigned(true);
	}

	CheckIncreaseSkill(SkillFeignDeath, nullptr, zone->skill_difficulty[SkillFeignDeath].difficulty, success);
	return;
}

void Client::Handle_OP_Fishing(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (!p_timers.Expired(&database, pTimerFishing, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}

	if (CanFish()) {
		parse->EventPlayer(EVENT_FISH_START, this, "", 0);

		//these will trigger GoFish() after a delay if we're able to actually fish, and if not, we won't stop the client from trying again immediately (although we may need to tell it to repop the button)
		p_timers.Start(pTimerFishing, FishingReuseTime - 1);
		fishing_timer.Start();
	}
	return;
	// Changes made based on Bobs work on foraging. Now can set items in the forage database table to
	// forage for.
}

void Client::Handle_OP_Forage(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (!p_timers.Expired(&database, pTimerForaging, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}

	if (IsSitting())
	{
		Message_StringID(MT_Skills, FORAGE_STANDING);
		return;
	}

	if (IsStunned() || IsMezzed() || AutoAttackEnabled())
	{
		Message_StringID(MT_Skills, FORAGE_COMBAT);
		return;
	}

	p_timers.Start(pTimerForaging, ForagingReuseTime - 1);

	ForageItem();

	return;
}

void Client::Handle_OP_FriendsWho(const EQApplicationPacket *app)
{
	char *FriendsString = (char*)app->pBuffer;
	FriendsWho(FriendsString);
	return;
}

void Client::Handle_OP_GetGuildsList(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_GetGuildsList");
	SendPlayerGuild();
}

void Client::Handle_OP_GMBecomeNPC(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/becomenpc");
		return;
	}
	if (app->size != sizeof(BecomeNPC_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_GMBecomeNPC, size=%i, expected %i", app->size, sizeof(BecomeNPC_Struct));
		return;
	}
	//entity_list.QueueClients(this, app, false);
	BecomeNPC_Struct* bnpc = (BecomeNPC_Struct*)app->pBuffer;

	Mob* cli = (Mob*)entity_list.GetMob(bnpc->id);
	if (cli == nullptr)
		return;

	if (cli->IsClient())
	{
		Client* target = cli->CastToClient();
		target->QueuePacket(app);
		if(target->GetGM())
		{
			target->SetInvul(false);
			target->SetHideMe(false);
			target->SetGM(false);
		}

		cli->SendAppearancePacket(AT_NPCName, 1, true);
		target->SetBecomeNPC(true);
		target->SetBecomeNPCLevel(bnpc->maxlevel);
		cli->Message_StringID(CC_Default, TOGGLE_OFF);
		target->tellsoff = true;
		target->UpdateWho();
	}

	return;
}

void Client::Handle_OP_GMDelCorpse(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMDelCorpse_Struct))
		return;
	if (this->Admin() < commandEditPlayerCorpses) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/delcorpse");
		return;
	}
	GMDelCorpse_Struct* dc = (GMDelCorpse_Struct *)app->pBuffer;
	Mob* corpse = entity_list.GetMob(dc->corpsename);
	if (corpse == 0) {
		return;
	}
	if (corpse->IsCorpse() != true) {
		return;
	}
	corpse->CastToCorpse()->Delete();
	std::cout << name << " deleted corpse " << dc->corpsename << std::endl;
	Message(CC_Red, "Corpse %s deleted.", dc->corpsename);
	return;
}

void Client::Handle_OP_GMEmoteZone(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/emote");
		return;
	}
	if (app->size != sizeof(GMEmoteZone_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_GMEmoteZone, size=%i, expected %i", app->size, sizeof(GMEmoteZone_Struct));
		return;
	}
	GMEmoteZone_Struct* gmez = (GMEmoteZone_Struct*)app->pBuffer;
	char* newmessage = 0;
	if (strstr(gmez->text, "^") == 0)
		entity_list.Message(CC_Default, 15, gmez->text);
	else{
		for (newmessage = strtok((char*)gmez->text, "^"); newmessage != nullptr; newmessage = strtok(nullptr, "^"))
			entity_list.Message(CC_Default, 15, newmessage);
	}
	return;
}

void Client::Handle_OP_GMEndTraining(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMTrainEnd_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_GMEndTraining expected %i got %i", sizeof(GMTrainEnd_Struct), app->size);
		DumpPacket(app);
		return;
	}
	OPGMEndTraining(app);
	return;
}

void Client::Handle_OP_GMFind(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/find");
		return;
	}
	if (app->size != sizeof(GMSummon_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_GMFind, size=%i, expected %i", app->size, sizeof(GMSummon_Struct));
		return;
	}
	//Break down incoming
	GMSummon_Struct* request = (GMSummon_Struct*)app->pBuffer;
	//Create a new outgoing
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_GMFind, sizeof(GMSummon_Struct));
	GMSummon_Struct* foundplayer = (GMSummon_Struct*)outapp->pBuffer;
	//Copy the constants
	strcpy(foundplayer->charname, request->charname);
	strcpy(foundplayer->gmname, request->gmname);
	//Check if the NPC exits intrazone...
	Mob* gt = entity_list.GetMob(request->charname);
	if (gt != 0) {
		foundplayer->success = 1;
		foundplayer->x = (int32)gt->GetX();
		foundplayer->y = (int32)gt->GetY();

		foundplayer->z = (int32)gt->GetZ();
		foundplayer->zoneID = zone->GetZoneID();
	}
	//Send the packet...
	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_GMGoto(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMSummon_Struct)) {
		std::cout << "Wrong size on OP_GMGoto. Got: " << app->size << ", Expected: " << sizeof(GMSummon_Struct) << std::endl;
		return;
	}
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/goto");
		return;
	}
	GMSummon_Struct* gmg = (GMSummon_Struct*)app->pBuffer;
	Mob* gt = entity_list.GetMob(gmg->charname);
	if (gt != nullptr) {
		this->MovePC(zone->GetZoneID(), zone->GetInstanceID(), gt->GetX(), gt->GetY(), gt->GetZ(), gt->GetHeading());
	}
	else if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected.");
	else {
		ServerPacket* pack = new ServerPacket(ServerOP_GMGoto, sizeof(ServerGMGoto_Struct));
		memset(pack->pBuffer, 0, pack->size);
		ServerGMGoto_Struct* wsgmg = (ServerGMGoto_Struct*)pack->pBuffer;
		strcpy(wsgmg->myname, this->GetName());
		strcpy(wsgmg->gotoname, gmg->charname);
		wsgmg->admin = admin;
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
	return;
}

void Client::Handle_OP_GMHideMe(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/hideme");
		return;
	}
	if (app->size != sizeof(SpawnAppearance_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_GMHideMe, size=%i, expected %i", app->size, sizeof(SpawnAppearance_Struct));
		return;
	}
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)app->pBuffer;
	Message(CC_Red, "#: %i, %i", sa->type, sa->parameter);
	SetHideMe(!sa->parameter);
	return;

}

void Client::Handle_OP_GMKick(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMKick_Struct))
		return;
	if (this->Admin() < minStatusToKick) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/kick");
		return;
	}
	GMKick_Struct* gmk = (GMKick_Struct *)app->pBuffer;

	Client* client = entity_list.GetClientByName(gmk->name);
	if (client == 0) {
		if (!worldserver.Connected())
			Message(CC_Default, "Error: World server disconnected");
		else {
			ServerPacket* pack = new ServerPacket(ServerOP_KickPlayer, sizeof(ServerKickPlayer_Struct));
			ServerKickPlayer_Struct* skp = (ServerKickPlayer_Struct*)pack->pBuffer;
			strcpy(skp->adminname, gmk->gmname);
			strcpy(skp->name, gmk->name);
			skp->adminrank = this->Admin();
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
	}
	else {
		entity_list.QueueClients(this, app);
		//client->Kick();
	}
	return;
}

void Client::Handle_OP_GMKill(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/kill");
		return;
	}
	if (app->size != sizeof(GMKill_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_GMKill, size=%i, expected %i", app->size, sizeof(GMKill_Struct));
		return;
	}
	GMKill_Struct* gmk = (GMKill_Struct *)app->pBuffer;
	Mob* obj = entity_list.GetMob(gmk->name);
	Client* client = entity_list.GetClientByName(gmk->name);
	if (obj != 0) {
		if (client != 0) {
			entity_list.QueueClients(this, app);
		}
		else {
			obj->Kill();
		}
	}
	else {
		if (!worldserver.Connected())
			Message(CC_Default, "Error: World server disconnected");
		else {
			ServerPacket* pack = new ServerPacket(ServerOP_KillPlayer, sizeof(ServerKillPlayer_Struct));
			ServerKillPlayer_Struct* skp = (ServerKillPlayer_Struct*)pack->pBuffer;
			strcpy(skp->gmname, gmk->gmname);
			strcpy(skp->target, gmk->name);
			skp->admin = this->Admin();
			worldserver.SendPacket(pack);
			safe_delete(pack);
		}
	}
	return;
}

void Client::Handle_OP_GMLastName(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMLastName_Struct)) {
		std::cout << "Wrong size on OP_GMLastName. Got: " << app->size << ", Expected: " << sizeof(GMLastName_Struct) << std::endl;
		return;
	}
	GMLastName_Struct* gmln = (GMLastName_Struct*)app->pBuffer;
	if (strlen(gmln->lastname) >= 64) {
		Message(CC_Red, "/LastName: New last name too long. (max=63)");
	}
	else {
		Client* client = entity_list.GetClientByName(gmln->name);
		if (client == 0) {
			Message(CC_Red, "/LastName: %s not found", gmln->name);
		}
		else {
			if (this->Admin() < minStatusToUseGMCommands) {
				Message(CC_Red, "Your account has been reported for hacking.");
				database.SetHackerFlag(client->account_name, client->name, "/lastname");
				return;
			}
			else

				client->ChangeLastName(gmln->lastname);
		}
		gmln->unknown[0] = 1;
		gmln->unknown[1] = 1;
		gmln->unknown[2] = 1;
		gmln->unknown[3] = 1;
		entity_list.QueueClients(this, app, false);
	}
	return;
}

void Client::Handle_OP_GMNameChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMName_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_GMNameChange, size=%i, expected %i", app->size, sizeof(GMName_Struct));
		return;
	}
	const GMName_Struct* gmn = (const GMName_Struct *)app->pBuffer;
	if (this->Admin() < minStatusToUseGMCommands){
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/name");
		return;
	}
	Client* client = entity_list.GetClientByName(gmn->oldname);
	Log.Out(Logs::General, Logs::Status, "GM(%s) changeing players name. Old:%s New:%s", GetName(), gmn->oldname, gmn->newname);
	bool usedname = database.CheckUsedName((const char*)gmn->newname);
	if (client == 0) {
		Message(CC_Red, "%s not found for name change. Operation failed!", gmn->oldname);
		return;
	}
	if ((strlen(gmn->newname) > 63) || (strlen(gmn->newname) == 0)) {
		Message(CC_Red, "Invalid number of characters in new name (%s).", gmn->newname);
		return;
	}
	if (!usedname) {
		Message(CC_Red, "%s is already in use. Operation failed!", gmn->newname);
		return;

	}
	database.UpdateName(gmn->oldname, gmn->newname);
	strcpy(client->name, gmn->newname);
	client->Save();

	if (gmn->badname == 1) {
		database.AddToNameFilter(gmn->oldname);
	}
	EQApplicationPacket* outapp = app->Copy();
	GMName_Struct* gmn2 = (GMName_Struct*)outapp->pBuffer;
	gmn2->unknown[0] = 1;
	gmn2->unknown[1] = 1;
	gmn2->unknown[2] = 1;
	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);
	UpdateWho();
	return;
}

void Client::Handle_OP_GMSearchCorpse(const EQApplicationPacket *app)
{
	// Could make this into a rule, although there is a hard limit since we are using a popup, of 4096 bytes that can
	// be displayed in the window, including all the HTML formatting tags.
	//
	const int maxResults = 10;

	if (app->size < sizeof(GMSearchCorpse_Struct))
	{
		Log.Out(Logs::General, Logs::None, "OP_GMSearchCorpse size lower than expected: got %u expected at least %u",
			app->size, sizeof(GMSearchCorpse_Struct));
		DumpPacket(app);
		return;
	}

	GMSearchCorpse_Struct *gmscs = (GMSearchCorpse_Struct *)app->pBuffer;
	gmscs->Name[63] = '\0';

	char *escSearchString = new char[129];
	database.DoEscapeString(escSearchString, gmscs->Name, strlen(gmscs->Name));

	std::string query = StringFormat("SELECT charname, zoneid, x, y, z, time_of_death, rezzed, IsBurried "
		"FROM character_corpses WheRE charname LIKE '%%%s%%' ORDER BY charname LIMIT %i",
		escSearchString, maxResults);
	safe_delete_array(escSearchString);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	if (results.RowCount() == 0)
		return;

	if (results.RowCount() == maxResults)
		Message(clientMessageError, "Your search found too many results; some are not displayed.");
	else
		Message(clientMessageYellow, "There are %i corpse(s) that match the search string '%s'.", results.RowCount(), gmscs->Name);

	char charName[64], time_of_death[20];

	std::string popupText = "<table><tr><td>Name</td><td>Zone</td><td>X</td><td>Y</td><td>Z</td><td>Date</td><td>"
		"Rezzed</td><td>Buried</td></tr><tr><td>&nbsp</td><td></td><td></td><td></td><td></td><td>"
		"</td><td></td><td></td></tr>";

	for (auto row = results.begin(); row != results.end(); ++row) {

		strn0cpy(charName, row[0], sizeof(charName));

		uint32 ZoneID = atoi(row[1]);
		float CorpseX = atof(row[2]);
		float CorpseY = atof(row[3]);
		float CorpseZ = atof(row[4]);

		strn0cpy(time_of_death, row[5], sizeof(time_of_death));

		bool corpseRezzed = atoi(row[6]);
		bool corpseBuried = atoi(row[7]);

		popupText += StringFormat("<tr><td>%s</td><td>%s</td><td>%8.0f</td><td>%8.0f</td><td>%8.0f</td><td>%s</td><td>%s</td><td>%s</td></tr>",
			charName, StaticGetZoneName(ZoneID), CorpseX, CorpseY, CorpseZ, time_of_death,
			corpseRezzed ? "Yes" : "No", corpseBuried ? "Yes" : "No");

		if (popupText.size() > 4000) {
			Message(clientMessageError, "Unable to display all the results.");
			break;
		}

	}

	popupText += "</table>";


}

void Client::Handle_OP_GMServers(const EQApplicationPacket *app)
{
	if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	else {
		ServerPacket* pack = new ServerPacket(ServerOP_ZoneStatus, strlen(this->GetName()) + 2);
		memset(pack->pBuffer, (uint8)admin, 1);
		strcpy((char *)&pack->pBuffer[1], this->GetName());
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
	return;
}

void Client::Handle_OP_GMSummon(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMSummon_Struct)) {
		std::cout << "Wrong size on OP_GMSummon. Got: " << app->size << ", Expected: " << sizeof(GMSummon_Struct) << std::endl;
		return;
	}
	OPGMSummon(app);
	return;
}

void Client::Handle_OP_GMToggle(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMToggle_Struct)) {
		std::cout << "Wrong size on OP_GMToggle. Got: " << app->size << ", Expected: " << sizeof(GMToggle_Struct) << std::endl;
		return;
	}
	if (this->Admin() < minStatusToUseGMCommands) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/toggle");
		return;
	}
	GMToggle_Struct *ts = (GMToggle_Struct *)app->pBuffer;
	if (ts->toggle == 0) {
		database.SetGMIgnoreTells(AccountID(), 1);
		tellsoff = true;
	}
	else if (ts->toggle == 1) {
		database.SetGMIgnoreTells(AccountID(), 0);
		tellsoff = false;
	}
	else {
		Message(CC_Default, "Unkown value in /toggle packet");
	}
	UpdateWho();
	return;
}

void Client::Handle_OP_GMTraining(const EQApplicationPacket *app)
{
	if (app->size != sizeof(OldGMTrainee_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_GMTraining expected %i got %i", sizeof(OldGMTrainee_Struct), app->size);
		DumpPacket(app);
		return;
	}
	OPGMTraining(app);
	return;
}

void Client::Handle_OP_GMTrainSkill(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMSkillChange_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_GMTrainSkill expected %i got %i", sizeof(GMSkillChange_Struct), app->size);
		DumpPacket(app);
		return;
	}
	OPGMTrainSkill(app);
	return;
}

void Client::Handle_OP_GMZoneRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GMZoneRequest_Struct)) {
		std::cout << "Wrong size on OP_GMZoneRequest. Got: " << app->size << ", Expected: " << sizeof(GMZoneRequest_Struct) << std::endl;
		return;
	}
	if (this->Admin() < minStatusToBeGM) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/zone");
		return;
	}

	GMZoneRequest_Struct* gmzr = (GMZoneRequest_Struct*)app->pBuffer;
	float tarx = -1, tary = -1, tarz = -1;

	int16 minstatus = 0;
	uint8 minlevel = 0;
	char tarzone[32];
	uint16 zid = gmzr->zone_id;
	if (gmzr->zone_id == 0)
		zid = zonesummon_id;
	const char * zname = database.GetZoneName(zid);
	if (zname == nullptr)
		tarzone[0] = 0;
	else
		strcpy(tarzone, zname);

	// this both loads the safe points and does a sanity check on zone name
	if (!database.GetSafePoints(tarzone, 0, &tarx, &tary, &tarz, &minstatus, &minlevel)) {
		tarzone[0] = 0;
	}

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GMZoneRequest, sizeof(GMZoneRequest_Struct));
	GMZoneRequest_Struct* gmzr2 = (GMZoneRequest_Struct*)outapp->pBuffer;
	strcpy(gmzr2->charname, this->GetName());
	gmzr2->zone_id = gmzr->zone_id;
	gmzr2->x = tarx;
	gmzr2->y = tary;
	gmzr2->z = tarz;
	// Next line stolen from ZoneChange as well... - This gives us a nicer message than the normal "zone is down" message...
	if (tarzone[0] != 0 && admin >= minstatus && GetLevel() >= minlevel)
		gmzr2->success = 1;
	else {
		std::cout << "GetZoneSafeCoords failed. zoneid = " << gmzr->zone_id << "; czone = " << zone->GetZoneID() << std::endl;
		gmzr2->success = 0;
	}

	QueuePacket(outapp);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_GMZoneRequest2(const EQApplicationPacket *app)
{
	if (this->Admin() < minStatusToBeGM) {
		Message(CC_Red, "Your account has been reported for hacking.");
		database.SetHackerFlag(this->account_name, this->name, "/zone");
		return;
	}
	if (app->size < sizeof(uint32)) {
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_GMZoneRequest2 expected:%i got:%i", sizeof(uint32), app->size);
		return;
	}

	uint32 zonereq = *((uint32 *)app->pBuffer);
	GoToSafeCoords(zonereq, 0);
	return;
}

void Client::Handle_OP_GroupCancelInvite(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupCancel_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for OP_GroupCancelInvite: Expected: %i, Got: %i",
			sizeof(GroupCancel_Struct), app->size);
		return;
	}

	GroupCancel_Struct* gf = (GroupCancel_Struct*)app->pBuffer;
	Mob* inviter = entity_list.GetClientByName(gf->name1);

	if (inviter != nullptr)
	{
		if (inviter->IsClient())
			inviter->CastToClient()->QueuePacket(app);
	}
	else
	{
		ServerPacket* pack = new ServerPacket(ServerOP_GroupCancelInvite, sizeof(GroupCancel_Struct));
		memcpy(pack->pBuffer, gf, sizeof(GroupCancel_Struct));
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	return;
}

void Client::Handle_OP_GroupDisband(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupGeneric_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for GroupGeneric_Struct: Expected: %i, Got: %i",
			sizeof(GroupGeneric_Struct), app->size);
		return;
	}

	Log.Out(Logs::General, Logs::None, "Member Disband Request from %s\n", GetName());

	GroupGeneric_Struct* gd = (GroupGeneric_Struct*)app->pBuffer;

	Raid *raid = entity_list.GetRaidByClient(this);
	if (raid)
	{
		Mob* memberToDisband = nullptr;

		if (!raid->IsGroupLeader(GetName()))
			memberToDisband = this;
		else
			memberToDisband = GetTarget();

		if (!memberToDisband)
			memberToDisband = entity_list.GetMob(gd->name2);

		if (!memberToDisband)
			memberToDisband = this;

		if (!memberToDisband->IsClient())
			return;

		//we have a raid.. see if we're in a raid group
		uint32 grp = raid->GetGroup(memberToDisband->GetName());
		bool wasGrpLdr = raid->members[raid->GetPlayerIndex(memberToDisband->GetName())].IsGroupLeader;
		if (grp < 12){
			if (wasGrpLdr){
				raid->SetGroupLeader(memberToDisband->GetName(), false);
				for (int x = 0; x < MAX_RAID_MEMBERS; x++)
				{
					if (raid->members[x].GroupNumber == grp)
					{
						if (strlen(raid->members[x].membername) > 0 && strcmp(raid->members[x].membername, memberToDisband->GetName()) != 0)
						{
							raid->SetGroupLeader(raid->members[x].membername);
							break;
						}
					}
				}
			}
			raid->MoveMember(memberToDisband->GetName(), 0xFFFFFFFF);
			raid->GroupUpdate(grp); //break
			//raid->SendRaidGroupRemove(memberToDisband->GetName(), grp);
			//raid->SendGroupUpdate(memberToDisband->CastToClient());
			raid->SendGroupDisband(memberToDisband->CastToClient());
		}
		//we're done
		return;
	}

	Group* group = GetGroup();

	if (!group)
		return;

	if (group->GroupCount() <= 2 || (group->IsLeader(this) && GetTarget() == nullptr))
	{
		group->DisbandGroup();
	}
	else if (group->IsLeader(this) && GetTarget() == this)
	{
		LeaveGroup();
	}
	else
	{
		Mob* memberToDisband = nullptr;
		memberToDisband = GetTarget();

		if (!memberToDisband)
			memberToDisband = entity_list.GetMob(gd->name2);

		if (memberToDisband)
		{
			if (group->IsLeader(this))
			{
				// the group leader can kick other members out of the group...
				if (memberToDisband->IsClient())
				{
					group->DelMember(memberToDisband, false);
					Client* memberClient = memberToDisband->CastToClient();
					if (memberClient &&  group)
					{
						if (!memberClient->IsGrouped()) {
							Group *g = new Group(memberClient);

							entity_list.AddGroup(g);

							if (g->GetID() == 0) {
								safe_delete(g);
								return;
							}

							g->DisbandGroup(); //This sends the wrong message to the client but allows them to re-join a group without zoning.
						}
					}
				}
			}
			else
			{
				// ...but other members can only remove themselves
				group->DelMember(this, false);


				if (!IsGrouped()) {
					Group *g = new Group(this);

					if (!g) {
						delete g;
						g = nullptr;
						return;
					}

					entity_list.AddGroup(g);

					if (g->GetID() == 0) {
						safe_delete(g);
						return;
					}

					g->DisbandGroup(); //This sends the wrong message to the client but allows them to re-join a group without zoning.
				}
			}
		}
		else
		{
			Log.Out(Logs::General, Logs::Error, "Failed to remove player from group. Unable to find player named %s in player group", gd->name2);
		}
	}
	return;
}

void Client::Handle_OP_GroupFollow(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupGeneric_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for OP_GroupFollow: Expected: %i, Got: %i",
			sizeof(GroupGeneric_Struct), app->size);
		return;
	}

	GroupGeneric_Struct* gf = (GroupGeneric_Struct*)app->pBuffer;
	Mob* inviter = entity_list.GetClientByName(gf->name1);

	if (inviter != nullptr && inviter->IsClient()) {
		isgrouped = true;
		strn0cpy(gf->name1, inviter->GetName(), 64);
		strn0cpy(gf->name2, this->GetName(), 64);

		Raid* raid = entity_list.GetRaidByClient(inviter->CastToClient());
		Raid* iraid = entity_list.GetRaidByClient(this);

		//inviter has a raid don't do group stuff instead do raid stuff!
		if (raid){
			uint32 groupToUse = 0xFFFFFFFF;
			for (int x = 0; x < MAX_RAID_MEMBERS; x++){
				if (raid->members[x].member){ //this assumes the inviter is in the zone
					if (raid->members[x].member == inviter->CastToClient()){
						groupToUse = raid->members[x].GroupNumber;
						break;
					}
				}
			}
			if (iraid == raid){ //both in same raid
				uint32 ngid = raid->GetGroup(inviter->GetName());
				if (raid->GroupCount(ngid) < 6){
					raid->MoveMember(GetName(), ngid);
					raid->SendGroupDisband(this);
					//raid->SendRaidGroupAdd(GetName(), ngid);
					//raid->SendGroupUpdate(this);
					raid->GroupUpdate(ngid); //break
				}
				return;
			}
			if (raid->RaidCount() < MAX_RAID_MEMBERS){
				if (raid->GroupCount(groupToUse) < 6){
					raid->SendRaidCreate(this);
					raid->SendMakeLeaderPacketTo(raid->leadername, this);
					raid->AddMember(this, groupToUse);
					raid->SendBulkRaid(this);
					//raid->SendRaidGroupAdd(GetName(), groupToUse);
					//raid->SendGroupUpdate(this);
					raid->GroupUpdate(groupToUse); //break
					if (raid->IsLocked()) {
						raid->SendRaidLockTo(this);
					}
					return;
				}
				else{
					raid->SendRaidCreate(this);
					raid->SendMakeLeaderPacketTo(raid->leadername, this);
					raid->AddMember(this);
					raid->SendBulkRaid(this);
					if (raid->IsLocked()) {
						raid->SendRaidLockTo(this);
					}
					return;
				}
			}
		}

		Group* group = entity_list.GetGroupByClient(inviter->CastToClient());

		if (!group){
			//Make new group
			group = new Group(inviter);
			if (!group)
				return;
			entity_list.AddGroup(group);

			if (group->GetID() == 0) {
				Message(CC_Red, "Unable to get new group id. Cannot create group.");
				inviter->Message(CC_Red, "Unable to get new group id. Cannot create group.");
				return;
			}

			//now we have a group id, can set inviter's id
			database.SetGroupID(inviter->GetName(), group->GetID(), inviter->CastToClient()->CharacterID());
			database.SetGroupLeaderName(group->GetID(), inviter->GetName());

			//Invite the inviter into the group first.....dont ask
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
			GroupJoin_Struct* outgj = (GroupJoin_Struct*)outapp->pBuffer;
			strcpy(outgj->membername, inviter->GetName());
			strcpy(outgj->yourname, inviter->GetName());
			outgj->action = groupActInviteInitial; // 'You have formed the group'.
			inviter->CastToClient()->QueuePacket(outapp);
			safe_delete(outapp);

		}
		if (!group)
			return;

		inviter->CastToClient()->QueuePacket(app);//notify inviter the client accepted

		if (!group->AddMember(this))
			return;

		database.RefreshGroupFromDB(this);
		group->SendHPPacketsTo(this);

		//send updates to clients out of zone...
		ServerPacket* pack = new ServerPacket(ServerOP_GroupJoin, sizeof(ServerGroupJoin_Struct));
		ServerGroupJoin_Struct* gj = (ServerGroupJoin_Struct*)pack->pBuffer;
		gj->gid = group->GetID();
		gj->zoneid = zone->GetZoneID();
		gj->instance_id = zone->GetInstanceID();
		strcpy(gj->member_name, GetName());
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
	else if (inviter == nullptr)
	{
		// Inviter is in another zone - Remove merc from group now if any
		LeaveGroup();

		ServerPacket* pack = new ServerPacket(ServerOP_GroupFollow, sizeof(ServerGroupFollow_Struct));
		ServerGroupFollow_Struct *sgfs = (ServerGroupFollow_Struct *)pack->pBuffer;
		sgfs->CharacterID = CharacterID();
		strn0cpy(sgfs->gf.name1, gf->name1, sizeof(sgfs->gf.name1));
		strn0cpy(sgfs->gf.name2, gf->name2, sizeof(sgfs->gf.name2));
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Client::Handle_OP_GroupInvite(const EQApplicationPacket *app)
{
	//this seems to be the initial invite to form a group
	Handle_OP_GroupInvite2(app);
}

void Client::Handle_OP_GroupInvite2(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupInvite_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for OP_GroupInvite: Expected: %i, Got: %i",
			sizeof(GroupInvite_Struct), app->size);
		return;
	}

	GroupInvite_Struct* gis = (GroupInvite_Struct*)app->pBuffer;

	Mob *Invitee = entity_list.GetMob(gis->invitee_name);

	if (Invitee == this)
	{
		Message_StringID(clientMessageWhite, GROUP_INVITEE_SELF);
		return;
	}

	if (Invitee) {
		if (Invitee->IsClient()) {
			if ((!Invitee->IsGrouped() && !Invitee->IsRaidGrouped()) ||
				(Invitee->GetGroup() && Invitee->GetGroup()->GroupCount() == 2))
			{
				if (app->GetOpcode() == OP_GroupInvite2)
				{
					//Make a new packet using all the same information but make sure it's a fixed GroupInvite opcode so we
					//Don't have to deal with GroupFollow2 crap.
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupInvite, sizeof(GroupInvite_Struct));
					memcpy(outapp->pBuffer, app->pBuffer, outapp->size);
					Invitee->CastToClient()->QueuePacket(outapp);
					safe_delete(outapp);
					return;
				}
				else
				{
					//The correct opcode, no reason to bother wasting time reconstructing the packet
					Invitee->CastToClient()->QueuePacket(app);
				}
			}
		}
	}
	else
	{
		ServerPacket* pack = new ServerPacket(ServerOP_GroupInvite, sizeof(GroupInvite_Struct));
		memcpy(pack->pBuffer, gis, sizeof(GroupInvite_Struct));
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	return;
}

void Client::Handle_OP_GroupUpdate(const EQApplicationPacket *app)
{
	if (app->size != sizeof(GroupJoin_Struct)-252)
	{
		Log.Out(Logs::General, Logs::None, "Size mismatch on OP_GroupUpdate: got %u expected %u",
			app->size, sizeof(GroupJoin_Struct)-252);
		DumpPacket(app);
		return;
	}

	GroupJoin_Struct* gu = (GroupJoin_Struct*)app->pBuffer;

	switch (gu->action) 
	{
		case groupActMakeLeader:
		{
			Mob* newleader = entity_list.GetClientByName(gu->membername);
			Group* group = this->GetGroup();

			if (newleader && group) 
			{
				// the client only sends this if it's the group leader, but check anyway
				if (group->IsLeader(this))
					group->ChangeLeader(newleader);
				else 
				{
					Log.Out(Logs::General, Logs::None, "Group /makeleader request originated from non-leader member: %s", GetName());
					DumpPacket(app);
				}
			}
			break;
		}

		default:
		{
			Log.Out(Logs::General, Logs::None, "Received unhandled OP_GroupUpdate requesting action %u", gu->action);
			DumpPacket(app);
			return;
		}
	}
}

void Client::Handle_OP_GuildDelete(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_GuildDelete");

	if (!IsInAGuild() || !guild_mgr.IsGuildLeader(GuildID(), CharacterID()))
		Message(CC_Default, "You are not a guild leader or not in a guild.");
	else {
		Log.Out(Logs::Detail, Logs::Guilds, "Deleting guild %s (%d)", guild_mgr.GetGuildName(GuildID()), GuildID());
		if (!guild_mgr.DeleteGuild(GuildID()))
			Message(CC_Default, "Guild delete failed.");
		else {
			Message(CC_Default, "Guild successfully deleted.");
		}
	}
}

void Client::Handle_OP_GuildInvite(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_GuildInvite");

	if (app->size != sizeof(GuildCommand_Struct)) {
		std::cout << "Wrong size: OP_GuildInvite, size=" << app->size << ", expected " << sizeof(GuildCommand_Struct) << std::endl;
		return;
	}

	GuildCommand_Struct* gc = (GuildCommand_Struct*)app->pBuffer;

	if (!IsInAGuild())
		Message(CC_Default, "Error: You are not in a guild!");
	else if (gc->officer > GUILD_MAX_RANK)
		Message(CC_Red, "Invalid rank.");
	else if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	else {

		//ok, the invite is also used for changing rank as well.
		Mob* invitee = entity_list.GetMob(gc->othername);

		if (!invitee) {
			Message(CC_Red, "Prospective guild member %s must be in zone to preform guild operations on them.", gc->othername);
			return;
		}

		if (invitee->IsClient()) {
			Client* client = invitee->CastToClient();

			//ok, figure out what they are trying to do.
			if (client->GuildID() == GuildID()) {
				//they are already in this guild, must be a promotion or demotion
				if (gc->officer < client->GuildRank()) {
					//demotion
					if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_DEMOTE)) {
						Message(CC_Red, "You dont have permission to demote.");
						return;
					}

					//we could send this to the member and prompt them to see if they want to
					//be demoted (I guess), but I dont see a point in that.

					Log.Out(Logs::Detail, Logs::Guilds, "%s (%d) is demoting %s (%d) to rank %d in guild %s (%d)",
						GetName(), CharacterID(),
						client->GetName(), client->CharacterID(),
						gc->officer,
						guild_mgr.GetGuildName(GuildID()), GuildID());

					if (!guild_mgr.SetGuildRank(client->CharacterID(), gc->officer)) {
						Message(CC_Red, "There was an error during the demotion, DB may now be inconsistent.");
						return;
					}

				}
				else if (gc->officer > client->GuildRank()) {
					//promotion
					if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_PROMOTE)) {
						Message(CC_Red, "You dont have permission to demote.");
						return;
					}

					Log.Out(Logs::Detail, Logs::Guilds, "%s (%d) is asking to promote %s (%d) to rank %d in guild %s (%d)",
						GetName(), CharacterID(),
						client->GetName(), client->CharacterID(),
						gc->officer,
						guild_mgr.GetGuildName(GuildID()), GuildID());

					//record the promotion with guild manager so we know its valid when we get the reply
					guild_mgr.RecordInvite(client->CharacterID(), GuildID(), gc->officer);

					if (gc->guildeqid == 0)
						gc->guildeqid = GuildID();

					Log.Out(Logs::Detail, Logs::Guilds, "Sending OP_GuildInvite for promotion to %s, length %d", client->GetName(), app->size);
					client->QueuePacket(app);

				}
				else {
					Message(CC_Red, "That member is already that rank.");
					return;
				}
			}
			else if (!client->IsInAGuild()) {
				//they are not in this or any other guild, this is an invite
				//
				if (client->GetPendingGuildInvitation())
				{
					Message(CC_Red, "That person is already considering a guild invitation.");
					return;
				}

				if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_INVITE)) {
					Message(CC_Red, "You dont have permission to invite.");
					return;
				}

				Log.Out(Logs::Detail, Logs::Guilds, "Inviting %s (%d) into guild %s (%d)",
					client->GetName(), client->CharacterID(),
					guild_mgr.GetGuildName(GuildID()), GuildID());

				//record the invite with guild manager so we know its valid when we get the reply
				guild_mgr.RecordInvite(client->CharacterID(), GuildID(), gc->officer);

				if (gc->guildeqid == 0)
					gc->guildeqid = GuildID();

				Log.Out(Logs::Detail, Logs::Guilds, "Sending OP_GuildInvite for invite to %s, length %d", client->GetName(), app->size);
				client->SetPendingGuildInvitation(true);
				client->QueuePacket(app);

			}
			else {
				//they are in some other guild
				Message(CC_Red, "Player is in a guild.");
				return;
			}
		}
	}
}

void Client::Handle_OP_GuildInviteAccept(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_GuildInviteAccept");

	SetPendingGuildInvitation(false);

	if (app->size != sizeof(GuildInviteAccept_Struct)) {
		std::cout << "Wrong size: OP_GuildInviteAccept, size=" << app->size << ", expected " << sizeof(GuildInviteAccept_Struct) << std::endl;
		return;
	}

	GuildInviteAccept_Struct* gj = (GuildInviteAccept_Struct*)app->pBuffer;


	if (gj->response == 5 || gj->response == 4) {
		//dont care if the check fails (since we dont know the rank), just want to clear the entry.
		guild_mgr.VerifyAndClearInvite(CharacterID(), gj->guildeqid, gj->response);

		worldserver.SendEmoteMessage(gj->inviter, 0, 0, "%s has declined to join the guild.", this->GetName());

		return;
	}

	//uint32 tmpeq = gj->guildeqid;
	if (IsInAGuild() && gj->response == GuildRank())
		Message(CC_Default, "Error: You're already in a guild!");
	else if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	else {
		Log.Out(Logs::Detail, Logs::Guilds, "Guild Invite Accept: guild %d, response %d, inviter %s, person %s",
			gj->guildeqid, gj->response, gj->inviter, gj->newmember);

		//we dont really care a lot about what this packet means, as long as
		//it has been authorized with the guild manager
		if (!guild_mgr.VerifyAndClearInvite(CharacterID(), gj->guildeqid, gj->response)) {
			worldserver.SendEmoteMessage(gj->inviter, 0, 0, "%s has sent an invalid response to your invite!", GetName());
			Message(CC_Red, "Invalid invite response packet!");
			return;
		}

		if (gj->guildeqid == GuildID()) {
			//only need to change rank.

			Log.Out(Logs::Detail, Logs::Guilds, "Changing guild rank of %s (%d) to rank %d in guild %s (%d)",
				GetName(), CharacterID(),
				gj->response,
				guild_mgr.GetGuildName(GuildID()), GuildID());

			if (!guild_mgr.SetGuildRank(CharacterID(), gj->response)) {
				Message(CC_Red, "There was an error during the rank change, DB may now be inconsistent.");
				return;
			}
		}
		else {

			Log.Out(Logs::Detail, Logs::Guilds, "Adding %s (%d) to guild %s (%d) at rank %d",
				GetName(), CharacterID(),
				guild_mgr.GetGuildName(gj->guildeqid), gj->guildeqid,
				gj->response);

			//change guild and rank

			uint32 guildrank = gj->response;

			if (!guild_mgr.SetGuild(CharacterID(), gj->guildeqid, guildrank)) {
				Message(CC_Red, "There was an error during the invite, DB may now be inconsistent.");
				return;
			}
		}
	}
}

void Client::Handle_OP_GuildLeader(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_GuildLeader");

	if (app->size < 2) {
		Log.Out(Logs::Detail, Logs::Guilds, "Invalid length %d on OP_GuildLeader", app->size);
		return;
	}

	app->pBuffer[app->size - 1] = 0;
	GuildMakeLeader* gml = (GuildMakeLeader*)app->pBuffer;
	if (!IsInAGuild())
		Message(CC_Default, "Error: You arent in a guild!");
	else if (GuildRank() != GUILD_LEADER)
		Message(CC_Default, "Error: You arent the guild leader!");
	else if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	else {

		//NOTE: we could do cross-zone lookups here...
		char target[64];
		strcpy(target, gml->name);

		Client* newleader = entity_list.GetClientByName(target);
		if (newleader) {

			Log.Out(Logs::Detail, Logs::Guilds, "Transfering leadership of %s (%d) to %s (%d)",
				guild_mgr.GetGuildName(GuildID()), GuildID(),
				newleader->GetName(), newleader->CharacterID());

			if (guild_mgr.SetGuildLeader(GuildID(), newleader->CharacterID())){
				Message(CC_Default, "Successfully Transfered Leadership to %s.", target);
				newleader->Message(CC_Yellow, "%s has transfered the guild leadership into your hands.", GetName());
			}
			else
				Message(CC_Default, "Could not change leadership at this time.");
		}
		else
			Message(CC_Default, "Failed to change leader, could not find target.");
	}
	return;
}

void Client::Handle_OP_GuildPeace(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Got OP_GuildPeace of len %d", app->size);
	return;
}

void Client::Handle_OP_GuildRemove(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_GuildRemove");

	if (app->size != sizeof(GuildCommand_Struct)) {
		std::cout << "Wrong size: OP_GuildRemove, size=" << app->size << ", expected " << sizeof(GuildCommand_Struct) << std::endl;
		return;
	}
	GuildCommand_Struct* gc = (GuildCommand_Struct*)app->pBuffer;
	if (!IsInAGuild())
		Message(CC_Default, "Error: You arent in a guild!");
	// we can always remove ourself, otherwise, our rank needs remove permissions
	else if (strcasecmp(gc->othername, GetName()) != 0 &&
		!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_REMOVE))
		Message(CC_Default, "You dont have permission to remove guild members.");
	else if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	else {
		uint32 char_id;
		Client* client = entity_list.GetClientByName(gc->othername);
		Client* remover = entity_list.GetClientByName(gc->myname);

		if (client) {
			if (!client->IsInGuild(GuildID())) {
				Message(CC_Default, "You aren't in the same guild, what do you think you are doing?");
				return;
			}
			char_id = client->CharacterID();

			if (client->GuildRank() >= remover->GuildRank() && strcmp(client->GetName(), remover->GetName()) != 0){
				Message(CC_Default, "You can't remove a player from the guild with an equal or higher rank to you!");
				return;
			}

			Log.Out(Logs::Detail, Logs::Guilds, "Removing %s (%d) from guild %s (%d)",
				client->GetName(), client->CharacterID(),
				guild_mgr.GetGuildName(GuildID()), GuildID());
		}
		else {
			CharGuildInfo gci;
			CharGuildInfo gci_;
			if (!guild_mgr.GetCharInfo(gc->myname, gci_)) {
				Message(CC_Default, "Unable to find '%s'", gc->myname);
				return;
			}
			if (!guild_mgr.GetCharInfo(gc->othername, gci)) {
				Message(CC_Default, "Unable to find '%s'", gc->othername);
				return;
			}
			if (gci.guild_id != GuildID()) {
				Message(CC_Default, "You aren't in the same guild, what do you think you are doing?");
				return;
			}
			if (gci.rank >= gci_.rank) {
				Message(CC_Default, "You can't remove a player from the guild with an equal or higher rank to you!");
				return;
			}

			char_id = gci.char_id;

			Log.Out(Logs::Detail, Logs::Guilds, "Removing remote/offline %s (%d) into guild %s (%d)",
				gci.char_name.c_str(), gci.char_id, guild_mgr.GetGuildName(GuildID()), GuildID());
		}

		uint32 guid = remover->GuildID();
		if (!guild_mgr.SetGuild(char_id, GUILD_NONE, 0)) {
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_GuildRemove, sizeof(GuildRemove_Struct));
			GuildRemove_Struct* gm = (GuildRemove_Struct*)outapp->pBuffer;
			gm->guildeqid = guid;
			strcpy(gm->Removee, gc->othername);
			Message(CC_Default, "%s successfully removed from your guild.", gc->othername);
			entity_list.QueueClientsGuild(this, outapp, false, GuildID());
			safe_delete(outapp);
		}
	}
	return;
}

void Client::Handle_OP_GuildWar(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Got OP_GuildWar of len %d", app->size);
	return;
}

void Client::Handle_OP_Hide(const EQApplicationPacket *app)
{
	if (!HasSkill(SkillHide) && GetBaseRace() != HALFLING && GetBaseRace() != DARK_ELF && GetBaseRace() != WOOD_ELF)
	{
		return;
	}

	if (!p_timers.Expired(&database, pTimerHide, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	int reuse = HideReuseTime - GetAA(209);
	p_timers.Start(pTimerHide, reuse - 1);

	float hidechance = ((GetSkill(SkillHide) / 250.0f) + .25) * 100;
	float random = zone->random.Real(0, 100);
	float success = 2.0;
	if (random < hidechance) {
		if (GetAA(aaShroudofStealth)){
			improved_hidden = true;
		}
		hidden = true;
		SetInvisible(INVIS_HIDDEN, false); // We handle the apperance packet below based on if we failed or not.
		success = 1.0;
	}
	else
	{
		hidden = false;
	}

	if (GetClass() == ROGUE)
	{
		Mob *evadetar = GetTarget();
		if (!auto_attack && (evadetar && evadetar->CheckAggro(this)
			&& evadetar->IsNPC()))
		{
			if (zone->random.Int(0, 260) < (int)GetSkill(SkillHide))
			{
				Message_StringID(MT_Skills, EVADE_SUCCESS);
				RogueEvade(evadetar);
				success = 1.0;
			}
			else
			{
				Message_StringID(MT_Skills, EVADE_FAIL);
			}
		}
		else
		{
			if (hidden)
			{
				Message_StringID(MT_Skills, HIDE_SUCCESS);
				success = 1.0;
			}
			else
				Message_StringID(MT_Skills, HIDE_FAIL);
		}
	}

	CheckIncreaseSkill(SkillHide, nullptr, zone->skill_difficulty[SkillHide].difficulty, success);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
	sa_out->spawn_id = GetID();
	sa_out->type = AT_Invis;
	sa_out->parameter = hidden;

	if(!hidden && GetClass() != ROGUE)
		entity_list.QueueClients(this, outapp, true);
	else
		entity_list.QueueClients(this, outapp);
	safe_delete(outapp);

	return;
}

void Client::Handle_OP_Ignore(const EQApplicationPacket *app)
{
}

void Client::Handle_OP_Illusion(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Illusion_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized OP_Illusion: got %d, expected %d", app->size,
			sizeof(Illusion_Struct));
		DumpPacket(app);
		return;
	}

	if (!GetGM())
	{
		database.SetMQDetectionFlag(this->AccountName(), this->GetName(), "OP_Illusion sent by non Game Master.", zone->GetShortName());
		return;
	}

	Illusion_Struct* bnpc = (Illusion_Struct*)app->pBuffer;
	//these need to be implemented
	/*
	texture		= bnpc->texture;
	helmtexture	= bnpc->helmtexture;
	luclinface	= bnpc->luclinface;
	*/
	race = bnpc->race;
	size = 0;

	entity_list.QueueClients(this, app);
	return;
}

void Client::Handle_OP_InspectAnswer(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(OldInspectResponse_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_InspectAnswer, size=%i, expected %i", app->size, sizeof(OldInspectResponse_Struct));
		return;
	}

	EQApplicationPacket* outapp = app->Copy();
	OldInspectResponse_Struct* insr = (OldInspectResponse_Struct*)outapp->pBuffer;
	Mob* tmp = entity_list.GetMob(insr->TargetID);

	if (tmp != 0 && tmp->IsClient())
	{
		tmp->CastToClient()->QueuePacket(outapp);
	}

	return;
}

void Client::Handle_OP_InspectRequest(const EQApplicationPacket *app) 
{

	if (app->size != sizeof(Inspect_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_InspectRequest, size=%i, expected %i", app->size, sizeof(Inspect_Struct));
		return;
	}

	Inspect_Struct* ins = (Inspect_Struct*)app->pBuffer;
	Mob* tmp = entity_list.GetMob(ins->TargetID);

	if (tmp != 0 && tmp->IsClient()) {
		tmp->CastToClient()->QueuePacket(app);
	} // Send request to target

	return;
}

void Client::Handle_OP_InstillDoubt(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	//packet is empty as of 12/14/04

	if (!p_timers.Expired(&database, pTimerInstillDoubt, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerInstillDoubt, InstillDoubtReuseTime - 1);

	InstillDoubt(GetTarget());
	CheckIncreaseSkill(SkillIntimidation, GetTarget(), zone->skill_difficulty[SkillIntimidation].difficulty);
	return;
}

void Client::Handle_OP_ItemLinkResponse(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(ItemViewRequest_Struct)) {
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_ItemLinkResponse expected:%i got:%i", sizeof(ItemViewRequest_Struct), app->size);
		return;
	}

	ItemViewRequest_Struct* ivrs = (ItemViewRequest_Struct*)app->pBuffer;
	const Item_Struct* item = database.GetItem(ivrs->item_id);
	if (!item) {
		if (ivrs->item_id > 0 && ivrs->item_id  < 1001)
		{
			std::string response = "";
			int sayid = ivrs->item_id;
			bool silentsaylink = false;

			if (sayid > 500)	//Silent Saylink
			{
				sayid = sayid - 500;
				silentsaylink = true;
			}

			if (sayid > 0)
			{

				std::string query = StringFormat("SELECT `phrase` FROM saylink WHERE `id` = '%i'", sayid);
				auto results = database.QueryDatabase(query);
				if (!results.Success()) {
					Message(CC_Red, "Error: The saylink (%s) was not found in the database.", response.c_str());
					return;
				}

				if (results.RowCount() != 1) {
					Message(CC_Red, "Error: The saylink (%s) was not found in the database.", response.c_str());
					return;
				}

				auto row = results.begin();
				response = row[0];

			}

			if ((response).size() > 0)
			{
				if (!mod_saylink(response, silentsaylink)) { return; }

				if (GetTarget() && GetTarget()->IsNPC())
				{
					if (silentsaylink)
					{
						parse->EventNPC(EVENT_SAY, GetTarget()->CastToNPC(), this, response.c_str(), 0);
						parse->EventPlayer(EVENT_SAY, this, response.c_str(), 0);
					}
					else
					{
						Message(CC_Say, "You say, '%s'", response.c_str());
						ChannelMessageReceived(8, 0, 100, response.c_str());
					}
					return;
				}
				else
				{
					if (silentsaylink)
					{
						parse->EventPlayer(EVENT_SAY, this, response.c_str(), 0);
					}
					else
					{
						Message(CC_Say, "You say, '%s'", response.c_str());
						ChannelMessageReceived(8, 0, 100, response.c_str());
					}
					return;
				}
			}
			else
			{
				Message(CC_Red, "Error: Say Link not found or is too long.");
				return;
			}
		}
		else {
			Message(CC_Red, "Error: The item for the link you have clicked on does not exist!");
			return;
		}

	}

	ItemInst* inst = database.CreateItem(ivrs->item_id);
	if (inst) {
		SendItemPacket(0, inst, ItemPacketViewLink);
		safe_delete(inst);
	}
	return;
}

void Client::Handle_OP_Jump(const EQApplicationPacket *app)
{
	SetEndurance(GetEndurance() - (GetLevel()<20 ? (225 * GetLevel() / 100) : 50));
	return;
}

void Client::Handle_OP_LeaveBoat(const EQApplicationPacket *app)
{

	if(m_pp.boatid > 0)
	{
		Log.Out(Logs::Moderate, Logs::Boats, "%s is attempting to leave boat %s at %0.2f,%0.2f,%0.2f ", GetName(), m_pp.boat, GetX(), GetY(), GetZ());
	}
	else
	{
		Log.Out(Logs::Moderate, Logs::Boats, "%s recieved OP_LeaveBoat", GetName());
	}

	Mob* boat = entity_list.GetMob(this->BoatID);	// find the mob corresponding to the boat id
	if (boat) 
	{
		if ((boat->GetTarget() == this) && boat->GetHateAmount(this) == 0)	// if the client somehow left while still controlling the boat (and the boat isn't attacking them)
			boat->SetTarget(0);			// fix it to stop later problems

		char buf[24];
		snprintf(buf, 23, "%d", boat->GetNPCTypeID());
		buf[23] = '\0';
		parse->EventPlayer(EVENT_LEAVE_BOAT, this, buf, 0);
	}

	this->BoatID = 0;
	m_pp.boatid = 0;
	m_pp.boat[0] = 0;
	return;
}

void Client::Handle_OP_Logout(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Character, "%s sent a logout packet.", GetName());

	SendLogoutPackets();

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_LogoutReply);
	FastQueuePacket(&outapp);

	Disconnect();
	return;
}

void Client::Handle_OP_LootItem(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(LootingItem_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_LootItem, size=%i, expected %i", app->size, sizeof(LootingItem_Struct));
		return;
	}
	/*
	**	fixed the looting code so that it sends the correct opcodes
	**	and now correctly removes the looted item the player selected
	**	as well as gives the player the proper item.
	**	Also fixed a few UI lock ups that would occur.
	*/

	EQApplicationPacket* outapp = 0;
	Entity* entity = entity_list.GetID(*((uint16*)app->pBuffer));
	if (entity == 0) {
		//	Message(CC_Red, "Error: OP_LootItem: Corpse not found (ent = 0)");
		outapp = new EQApplicationPacket(OP_LootComplete, 0);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (entity->IsCorpse()) {
		entity->CastToCorpse()->LootItem(this, app);
		return;
	}
	else {
		//	Message(CC_Red, "Error: Corpse not found! (!ent->IsCorpse())");
		Corpse::SendEndLootErrorPacket(this);
	}

	return;
}

void Client::Handle_OP_LootRequest(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(uint16) && app->size != sizeof(uint32)) {
		std::cout << "Wrong size: OP_EndLootRequest, size=" << app->size << ", expected " << sizeof(uint16) << std::endl;
		return;
	}

	Entity* ent = entity_list.GetID(*((uint32*)app->pBuffer));
	if (ent == 0) {
		//	Message(CC_Red, "Error: OP_LootRequest: Corpse not found (ent = 0)");
		Corpse::SendLootReqErrorPacket(this);
		return;
	}
	if (ent->IsCorpse())
	{
		SetLooting(ent->GetID()); //store the entity we are looting
		Corpse *ent_corpse = ent->CastToCorpse();
		if (DistanceSquaredNoZ(m_Position, ent_corpse->GetPosition())  > 625)
		{
			Message(CC_Red, "Corpse too far away.");
			Corpse::SendLootReqErrorPacket(this);
			return;
		}

		if (invisible) {
			BuffFadeByEffect(SE_Invisibility);
			BuffFadeByEffect(SE_Invisibility2);
			invisible = false;
		}
		if (invisible_undead) {
			BuffFadeByEffect(SE_InvisVsUndead);
			BuffFadeByEffect(SE_InvisVsUndead2);
			invisible_undead = false;
		}
		if (invisible_animals){
			BuffFadeByEffect(SE_InvisVsAnimals);
			invisible_animals = false;
		}
		if (hidden || improved_hidden){
			hidden = false;
			improved_hidden = false;
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
			SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
			sa_out->spawn_id = GetID();
			sa_out->type = 0x03;
			sa_out->parameter = 0;
			entity_list.QueueClients(this, outapp, true);
			safe_delete(outapp);
		}
		ent->CastToCorpse()->MakeLootRequestPackets(this, app);
		return;
	}
	else {
		std::cout << "npc == 0 LOOTING FOOKED3" << std::endl;
		Message(CC_Red, "Error: OP_LootRequest: Corpse not a corpse?");
		Corpse::SendLootReqErrorPacket(this);
	}
	return;
}

void Client::Handle_OP_ManaChange(const EQApplicationPacket *app)
{
	if (app->size == 0) {
		// i think thats the sign to stop the songs
		if (IsBardSong(casting_spell_id) || bardsong != 0)
			InterruptSpell(SONG_ENDS, CC_User_SpellFailure);
		else
			InterruptSpell(INTERRUPT_SPELL, CC_User_SpellFailure);

		return;
	}
	else	// I don't think the client sends proper manachanges
	{			// with a length, just the 0 len ones for stopping songs
		//ManaChange_Struct* p = (ManaChange_Struct*)app->pBuffer;
		printf("OP_ManaChange from client:\n");
		//DumpPacket(app);
	}
	return;
}

#if 0	// I dont think there's an op for this now, and we check this
// when the client is sitting
void Client::Handle_OP_Medding(const EQApplicationPacket *app)
{
	if (app->pBuffer[0])
		medding = true;
	else
		medding = false;
	return;
}
#endif

void Client::Handle_OP_MemorizeSpell(const EQApplicationPacket *app) 
{
	OPMemorizeSpell(app);
	return;
}

void Client::Handle_OP_Mend(const EQApplicationPacket *app)
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (GetClass() != MONK)
		return;

	if (!p_timers.Expired(&database, pTimerMend, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerMend, MendReuseTime - 1);

	int mendhp = GetMaxHP() / 4;
	int currenthp = GetHP();
	float success = 2.0;
	if (zone->random.Int(0, 99) < (int)GetSkill(SkillMend)) {

		int criticalchance = spellbonuses.CriticalMend + itembonuses.CriticalMend + aabonuses.CriticalMend;

		if (zone->random.Int(0, 99) < criticalchance){
			mendhp *= 2;
			Message_StringID(CC_Blue, MEND_CRITICAL);
		}
		SetHP(GetHP() + mendhp);
		SendHPUpdate();
		Message_StringID(CC_Blue, MEND_SUCCESS);
		success = 1.0;
	}
	else {
		/* the purpose of the following is to make the chance to worsen wounds much less common,
		which is more consistent with the way eq live works.
		according to my math, this should result in the following probability:
		0 skill - 25% chance to worsen
		20 skill - 23% chance to worsen
		50 skill - 16% chance to worsen */
		if ((GetSkill(SkillMend) <= 50) && (zone->random.Int(GetSkill(SkillMend), 100) < 50) && (zone->random.Int(1, 3) == 1))
		{
			SetHP(currenthp > mendhp ? (GetHP() - mendhp) : 1);
			SendHPUpdate();
			Message_StringID(CC_Blue, MEND_WORSEN);
		}
		else
			Message_StringID(CC_Blue, MEND_FAIL);
	}

	CheckIncreaseSkill(SkillMend, nullptr, zone->skill_difficulty[SkillMend].difficulty, success);
	return;
}

void Client::Handle_OP_MoveCoin(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MoveCoin_Struct)){
		Log.Out(Logs::General, Logs::Error, "Wrong size on OP_MoveCoin. Got: %i, Expected: %i", app->size, sizeof(MoveCoin_Struct));
		DumpPacket(app);
		return;
	}

	OPMoveCoin(app);

	return;
}

void Client::Handle_OP_MoveItem(const EQApplicationPacket *app)
{

	if (!CharacterID())
	{
		return;
	}

	if (app->size != sizeof(MoveItem_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_MoveItem, size=%i, expected %i", app->size, sizeof(MoveItem_Struct));
		return;
	}

	MoveItem_Struct* mi = (MoveItem_Struct*)app->pBuffer;
	Log.Out(Logs::Detail, Logs::Inventory, "Moveitem from_slot: %i, to_slot: %i, number_in_stack: %i", mi->from_slot, mi->to_slot, mi->number_in_stack);

	if (spellend_timer.Enabled() && casting_spell_id && !IsBardSong(casting_spell_id) && mi->from_slot != MainCursor)
	{
		if (mi->from_slot != mi->to_slot && (mi->from_slot <= EmuConstants::GENERAL_END || mi->from_slot > 39) && IsValidSlot(mi->from_slot) && IsValidSlot(mi->to_slot))
		{
			char *detect = nullptr;
			const ItemInst *itm_from = GetInv().GetItem(mi->from_slot);
			const ItemInst *itm_to = GetInv().GetItem(mi->to_slot);
			MakeAnyLenString(&detect, "Player issued a move item from %u(item id %u) to %u(item id %u) while casting %u.",
				mi->from_slot,
				itm_from ? itm_from->GetID() : 0,
				mi->to_slot,
				itm_to ? itm_to->GetID() : 0,
				casting_spell_id);
			database.SetMQDetectionFlag(AccountName(), GetName(), detect, zone->GetShortName());
			safe_delete_array(detect);
			Kick(); // Kick client to prevent client and server from getting out-of-sync inventory slots
			return;
		}
	}

	// Illegal bagslot useage checks. Currently, user only receives a message if this check is triggered.
	bool mi_hack = false;

	if (mi->from_slot >= EmuConstants::GENERAL_BAGS_BEGIN && mi->from_slot <= EmuConstants::CURSOR_BAG_END) {
		if (mi->from_slot >= EmuConstants::CURSOR_BAG_BEGIN) { mi_hack = true; }
		else {
			int16 from_parent = m_inv.CalcSlotId(mi->from_slot);
			if (!m_inv[from_parent]) { mi_hack = true; }
			else if (!m_inv[from_parent]->IsType(ItemClassContainer)) { mi_hack = true; }
			else if (m_inv.CalcBagIdx(mi->from_slot) >= m_inv[from_parent]->GetItem()->BagSlots) { mi_hack = true; }
		}
	}

	if (mi->to_slot >= EmuConstants::GENERAL_BAGS_BEGIN && mi->to_slot <= EmuConstants::CURSOR_BAG_END) {
		if (mi->to_slot >= EmuConstants::CURSOR_BAG_BEGIN) { mi_hack = true; }
		else {
			int16 to_parent = m_inv.CalcSlotId(mi->to_slot);
			if (!m_inv[to_parent]) { mi_hack = true; }
			else if (!m_inv[to_parent]->IsType(ItemClassContainer)) { mi_hack = true; }
			else if (m_inv.CalcBagIdx(mi->to_slot) >= m_inv[to_parent]->GetItem()->BagSlots) { mi_hack = true; }
		}
	}

	if (mi_hack) { Message(CC_Yellow, "Caution: Illegal use of inaccessable bag slots!"); }

	if (IsValidSlot(mi->from_slot) && IsValidSlot(mi->to_slot)) {
		bool si = SwapItem(mi);
		if (!si)
		{
			Log.Out(Logs::Detail, Logs::Inventory, "WTF Some shit failed. SwapItem: %i, IsValidSlot (from): %i, IsValidSlot (to): %i", si, IsValidSlot(mi->from_slot), IsValidSlot(mi->to_slot));
			SwapItemResync(mi);

			bool error = false;
			InterrogateInventory(this, false, true, false, error, false);
			if (error)
				InterrogateInventory(this, true, false, true, error);
		}
	}

	return;
}

void Client::Handle_OP_PetCommands(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(PetCommand_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_PetCommands, size=%i, expected %i", app->size, sizeof(PetCommand_Struct));
		return;
	}
	char val1[20] = { 0 };
	PetCommand_Struct* pet = (PetCommand_Struct*)app->pBuffer;
	Mob* mypet = this->GetPet();
	Mob *target = entity_list.GetMob(pet->target);

	if (!mypet || pet->command == PET_LEADER)
	{
		if (pet->command == PET_LEADER)
		{
			if (mypet && (!GetTarget() || GetTarget() == mypet))
			{
				mypet->Say_StringID(PET_LEADERIS, GetName());
			}
			else if ((mypet = GetTarget()))
			{
				Mob *Owner = mypet->GetOwner();
				if (Owner)
					mypet->Say_StringID(PET_LEADERIS, Owner->GetCleanName());
				else
					mypet->Say_StringID(I_FOLLOW_NOONE);
			}
		}

		return;
	}

	if (mypet->GetPetType() == petAnimation && (pet->command != PET_HEALTHREPORT && pet->command != PET_GETLOST) && !GetAA(aaAnimationEmpathy))
		return;

	// just let the command "/pet get lost" work for familiars
	if (mypet->GetPetType() == petFamiliar && pet->command != PET_GETLOST)
		return;

	uint32 PetCommand = pet->command;

	switch (PetCommand)
	{
	case PET_ATTACK: {
		if (!target)
			break;
		if (target->IsMezzed()) {
			Message_StringID(CC_Default, CANNOT_WAKE, mypet->GetCleanName(), target->GetCleanName());
			break;
		}
		if (mypet->IsFeared())
			break; //prevent pet from attacking stuff while feared

		if (!mypet->IsAttackAllowed(target)) {
			mypet->Say_StringID(NOT_LEGAL_TARGET);
			break;
		}

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 2) || mypet->GetPetType() != petAnimation) {
			if (target != this && DistanceSquaredNoZ(mypet->GetPosition(), target->GetPosition()) <= (RuleR(Pets, AttackCommandRange)*RuleR(Pets, AttackCommandRange))) {
				if (mypet->IsHeld()) {
					if (!mypet->IsFocused()) {
						mypet->SetHeld(false); //break the hold and guard if we explicitly tell the pet to attack.
						if (mypet->GetPetOrder() != SPO_Guard)
							mypet->SetPetOrder(SPO_Follow);
					}
					else {
						mypet->SetTarget(target);
					}
				}
				zone->AddAggroMob();
				mypet->AddToHateList(target, 1);
				Message_StringID(MT_PetResponse, PET_ATTACKING, mypet->GetCleanName(), target->GetCleanName());
			}
		}
		break;
	}
	case PET_BACKOFF: {
		if (mypet->IsFeared()) break; //keeps pet running while feared

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(MT_PetResponse, PET_CALMING);
			mypet->WipeHateList();
			mypet->SetTarget(nullptr);
			tar_ndx = 20;
		}
		break;
	}
	case PET_HEALTHREPORT: {
		Message_StringID(MT_PetResponse, PET_REPORT_HP, ConvertArrayF(mypet->GetHPRatio(), val1));
		mypet->ShowBuffList(this);
		//Message(CC_Default,"%s tells you, 'I have %d percent of my hit points left.'",mypet->GetName(),(uint8)mypet->GetHPRatio());
		break;
	}
	case PET_GETLOST: {
		// Cant tell a charmed pet to get lost
		if (mypet->GetPetType() == PetType::petCharmed) {
			break;
		}

		mypet->Say_StringID(MT_PetResponse, PET_GETLOST_STRING);
		mypet->CastToNPC()->Depop();

		//Oddly, the client (Titanium) will still allow "/pet get lost" command despite me adding the code below. If someone can figure that out, you can uncomment this code and use it.
		/*
		if((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 2) || mypet->GetPetType() != petAnimation) {
		mypet->Say_StringID(PET_GETLOST_STRING);
		mypet->CastToNPC()->Depop();
		}
		*/

		break;
	}
	case PET_GUARDHERE: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 1) || mypet->GetPetType() != petAnimation) {
			if (mypet->IsNPC()) {
				mypet->SetHeld(false);
				mypet->Say_StringID(MT_PetResponse, PET_GUARDINGLIFE);
				mypet->SetPetOrder(SPO_Guard);
				mypet->CastToNPC()->SaveGuardSpot();
			}
		}
		break;
	}
	case PET_FOLLOWME: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 1) || mypet->GetPetType() != petAnimation) {
			mypet->SetHeld(false);
			mypet->Say_StringID(MT_PetResponse, PET_FOLLOWING);
			mypet->SetPetOrder(SPO_Follow);
			mypet->SendAppearancePacket(AT_Anim, ANIM_STAND);
		}
		break;
	}
	case PET_TAUNT: {
		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			if(mypet->CastToNPC()->IsTaunting())
			{
				Message_StringID(MT_PetResponse, PET_NO_TAUNT);
				mypet->CastToNPC()->SetTaunting(false);
			}
			else
			{
				Message_StringID(MT_PetResponse, PET_DO_TAUNT);
				mypet->CastToNPC()->SetTaunting(true);
			}
		}
		break;
	}
	case PET_NOTAUNT: {
		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			Message_StringID(MT_PetResponse, PET_NO_TAUNT);
			mypet->CastToNPC()->SetTaunting(false);
		}
		break;
	}
	case PET_GUARDME: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 1) || mypet->GetPetType() != petAnimation) {
			mypet->SetHeld(false);
			mypet->Say_StringID(MT_PetResponse, PET_GUARDME_STRING);
			mypet->SetPetOrder(SPO_Follow);
			mypet->SendAppearancePacket(AT_Anim, ANIM_STAND);
		}
		break;
	}
	case PET_SITDOWN: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(MT_PetResponse, PET_SIT_STRING);
			mypet->SetPetOrder(SPO_Sit);
			mypet->SetRunAnimSpeed(0);
			if (!mypet->UseBardSpellLogic())	//maybe we can have a bard pet
				mypet->InterruptSpell(); //No cast 4 u. //i guess the pet should start casting
			mypet->SendAppearancePacket(AT_Anim, ANIM_SIT);
		}
		break;
	}
	case PET_STANDUP: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if ((mypet->GetPetType() == petAnimation && GetAA(aaAnimationEmpathy) >= 3) || mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(MT_PetResponse, PET_SIT_STRING);
			mypet->SetPetOrder(SPO_Follow);
			mypet->SendAppearancePacket(AT_Anim, ANIM_STAND);
		}
		break;
	}
	case PET_SLUMBER: {
		if (mypet->IsFeared()) break; //could be exploited like PET_BACKOFF

		if (mypet->GetPetType() != petAnimation) {
			mypet->Say_StringID(MT_PetResponse, PET_SIT_STRING);
			mypet->SetPetOrder(SPO_Sit);
			mypet->SetRunAnimSpeed(0);
			if (!mypet->UseBardSpellLogic())	//maybe we can have a bard pet
				mypet->InterruptSpell(); //No cast 4 u. //i guess the pet should start casting
			mypet->SendAppearancePacket(AT_Anim, ANIM_DEATH);
		}
		break;
	}
	case PET_HOLD: {
		if (GetAA(aaPetDiscipline) && mypet->IsNPC()){
			if (mypet->IsFeared())
				break; //could be exploited like PET_BACKOFF

			mypet->Say_StringID(MT_PetResponse, PET_ON_HOLD);
			mypet->WipeHateList();
			mypet->SetHeld(true);
		}
		break;
	}
	case PET_HOLD_ON: {
		if (GetAA(aaPetDiscipline) && mypet->IsNPC() && !mypet->IsHeld()) {
			if (mypet->IsFeared())
				break; //could be exploited like PET_BACKOFF

			mypet->Say_StringID(MT_PetResponse, PET_ON_HOLD);
			mypet->WipeHateList();
			mypet->SetHeld(true);
		}
		break;
	}
	case PET_HOLD_OFF: {
		if (GetAA(aaPetDiscipline) && mypet->IsNPC() && mypet->IsHeld())
			mypet->SetHeld(false);
		break;
	}
	case PET_NOCAST: {
		if (GetAA(aaAdvancedPetDiscipline) == 2 && mypet->IsNPC()) {
			if (mypet->IsFeared())
				break;
			if (mypet->IsNoCast()) {
				mypet->CastToNPC()->SetNoCast(false);
			}
			else {
				mypet->CastToNPC()->SetNoCast(true);
			}
		}
		break;
	}
	case PET_FOCUS: {
		if (GetAA(aaAdvancedPetDiscipline) >= 1 && mypet->IsNPC()) {
			if (mypet->IsFeared())
				break;
			if (mypet->IsFocused()) {
				mypet->CastToNPC()->SetFocused(false);
			}
			else {
				mypet->CastToNPC()->SetFocused(true);
			}
		}
		break;
	}
	case PET_FOCUS_ON: {
		if (GetAA(aaAdvancedPetDiscipline) >= 1 && mypet->IsNPC()) {
			if (mypet->IsFeared())
				break;
			if (!mypet->IsFocused()) {
				mypet->CastToNPC()->SetFocused(true);
			}
		}
		break;
	}
	case PET_FOCUS_OFF: {
		if (GetAA(aaAdvancedPetDiscipline) >= 1 && mypet->IsNPC()) {
			if (mypet->IsFeared())
				break;
			if (mypet->IsFocused()) {
				mypet->CastToNPC()->SetFocused(false);
			}
		}
		break;
	}
	default:
		printf("Client attempted to use a unknown pet command:\n");
		break;
	}
}

void Client::Handle_OP_Petition(const EQApplicationPacket *app)
{
	if (app->size <= 1)
		return;
	if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	/*else if(petition_list.FindPetitionByAccountName(this->AccountName()))
	{
	Message(CC_Default,"You already have a petition in queue, you cannot petition again until this one has been responded to or you have deleted the petition.");
	return;
	}*/
	else
	{
		if (petition_list.FindPetitionByAccountName(AccountName()))
		{
			Message(CC_Default, "You already have a petition in the queue, you must wait for it to be answered or use /deletepetition to delete it.");
			return;
		}
		Petition* pet = new Petition(CharacterID());
		pet->SetAName(this->AccountName());
		pet->SetClass(this->GetClass());
		pet->SetLevel(this->GetLevel());
		pet->SetCName(this->GetName());
		pet->SetRace(this->GetRace());
		pet->SetLastGM("");
		pet->SetCName(this->GetName());
		pet->SetPetitionText((char*)app->pBuffer);
		pet->SetZone(zone->GetZoneID());
		pet->SetUrgency(0);
		petition_list.AddPetition(pet);
		database.InsertPetitionToDB(pet);
		petition_list.UpdateGMQueue();
		petition_list.UpdateZoneListQueue();
		worldserver.SendEmoteMessage(0, 0, 80, 15, "%s has made a petition. #%i", GetName(), pet->GetID());
	}
	return;
}

void Client::Handle_OP_PetitionCheckIn(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Petition_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_PetitionCheckIn, size=%i, expected %i", app->size, sizeof(Petition_Struct));
		return;
	}
	Petition_Struct* inpet = (Petition_Struct*)app->pBuffer;

	Petition* pet = petition_list.GetPetitionByID(inpet->petnumber);
	//if (inpet->urgency != pet->GetUrgency())
	pet->SetUrgency(inpet->urgency);
	pet->SetLastGM(this->GetName());
	pet->SetGMText(inpet->gmtext);

	pet->SetCheckedOut(false);
	petition_list.UpdatePetition(pet);
	petition_list.UpdateGMQueue();
	petition_list.UpdateZoneListQueue();
	return;
}

void Client::Handle_OP_PetitionCheckout(const EQApplicationPacket *app)
{
	if (app->size != sizeof(uint32)) {
		std::cout << "Wrong size: OP_PetitionCheckout, size=" << app->size << ", expected " << sizeof(uint32) << std::endl;
		return;
	}
	if (!worldserver.Connected())
		Message(CC_Default, "Error: World server disconnected");
	else {
		uint32 getpetnum = *((uint32*)app->pBuffer);
		Petition* getpet = petition_list.GetPetitionByID(getpetnum);
		if (getpet != 0) {
			getpet->AddCheckout();
			getpet->SetCheckedOut(true);
			getpet->SendPetitionToPlayer(this->CastToClient());
			petition_list.UpdatePetition(getpet);
			petition_list.UpdateGMQueue();
			petition_list.UpdateZoneListQueue();
		}
	}
	return;
}

void Client::Handle_OP_PetitionDelete(const EQApplicationPacket *app)
{
	if (app->size != sizeof(PetitionUpdate_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_PetitionDelete, size=%i, expected %i", app->size, sizeof(PetitionUpdate_Struct));
		return;
	}
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_PetitionRefresh, sizeof(PetitionUpdate_Struct));
	PetitionUpdate_Struct* pet = (PetitionUpdate_Struct*)outapp->pBuffer;
	pet->petnumber = *((int*)app->pBuffer);
	pet->color = 0x00;
	pet->status = 0xFFFFFFFF;
	pet->senttime = 0;
	strcpy(pet->accountid, "");
	strcpy(pet->gmsenttoo, "");
	pet->quetotal = petition_list.GetTotalPetitions();
	strcpy(pet->charname, "");
	FastQueuePacket(&outapp);

	if (petition_list.DeletePetition(pet->petnumber) == -1)
		std::cout << "Something is borked with: " << pet->petnumber << std::endl;
	petition_list.ClearPetitions();
	petition_list.UpdateGMQueue();
	petition_list.ReadDatabase();
	petition_list.UpdateZoneListQueue();
	return;
}

void Client::Handle_OP_PetitionRefresh(const EQApplicationPacket *app)
{
	// This is When Client Asks for Petition Again and Again...
	// break is here because it floods the zones and causes lag if it
	// Were to actually do something:P We update on our own schedule now.
	return;
}

void Client::Handle_OP_PickPocket(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(PickPocket_Struct))
	{
		Log.Out(Logs::General, Logs::Error, "Size mismatch for Pick Pocket packet");
		DumpPacket(app);
	}

	if (!HasSkill(SkillPickPockets))
	{
		return;
	}

	if (!p_timers.Expired(&database, pTimerBeggingPickPocket, false))
	{
		Message(CC_Red, "Ability recovery time not yet met.");
		database.SetMQDetectionFlag(this->AccountName(), this->GetName(), "OP_PickPocket was sent again too quickly.", zone->GetShortName());
		return;
	}

	p_timers.Start(pTimerBeggingPickPocket, 8);

	PickPocket_Struct* pick_in = (PickPocket_Struct*)app->pBuffer;
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_PickPocket, sizeof(PickPocket_Struct));
	PickPocket_Struct* pick_out = (PickPocket_Struct*)outapp->pBuffer;

	Mob* victim = entity_list.GetMob(pick_in->to);
	if (!victim || m_inv.GetItem(MainCursor) != nullptr)
	{
		pick_out->coin = 0;
		pick_out->from = 0;
		pick_out->to = GetID();
		pick_out->myskill = GetSkill(SkillPickPockets);
		pick_out->type = 0;
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}

	if (victim->IsNPC())
	{
		victim->CastToNPC()->PickPocket(this);
		safe_delete(outapp);
		return;
	}

	pick_out->coin = 0;
	pick_out->from = victim->GetID();
	pick_out->to = GetID();
	pick_out->myskill = GetSkill(SkillPickPockets);
	pick_out->type = 0;

	if (victim == this){
		Message_StringID(CC_User_Skills, STEAL_FROM_SELF);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	else if(victim->IsClient() || (victim->GetOwner() && victim->GetOwner()->IsClient()))
	{
		Message_StringID(CC_User_Skills, STEAL_PLAYERS);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	else if(victim->IsCorpse())
	{
		Message_StringID(CC_User_Skills, STEAL_CORPSES);
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	else
	{
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
}

void Client::Handle_OP_RaidCommand(const EQApplicationPacket *app)
{
	if (app->size < sizeof(RaidGeneral_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_RaidCommand, size=%i, expected at least %i", app->size, sizeof(RaidGeneral_Struct));
		DumpPacket(app);
		return;
	}

	RaidGeneral_Struct *ri = (RaidGeneral_Struct*)app->pBuffer;
	switch (ri->action)
	{
	case RaidCommandInviteIntoExisting:
	case RaidCommandInvite: {
		Client *i = entity_list.GetClientByName(ri->player_name);
		if (i){
			Group *g = i->GetGroup();
			if (g){
				if (g->IsLeader(i) == false)
					Message(CC_Red, "You can only invite an ungrouped player or group leader to join your raid.");
				else{
					//This sends an "invite" to the client in question.
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
					RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
					strn0cpy(rg->leader_name, ri->leader_name, 64);
					strn0cpy(rg->player_name, ri->player_name, 64);

					rg->parameter = 0;
					rg->action = 20;
					i->QueuePacket(outapp);
					safe_delete(outapp);
				}
			}
			else{
				//This sends an "invite" to the client in question.
				EQApplicationPacket* outapp = new EQApplicationPacket(OP_RaidUpdate, sizeof(RaidGeneral_Struct));
				RaidGeneral_Struct *rg = (RaidGeneral_Struct*)outapp->pBuffer;
				strn0cpy(rg->leader_name, ri->leader_name, 64);
				strn0cpy(rg->player_name, ri->player_name, 64);

				rg->parameter = 0;
				rg->action = 20;
				i->QueuePacket(outapp);
				safe_delete(outapp);
			}
		}
		break;
	}
	case RaidCommandAcceptInvite: {
		Client *i = entity_list.GetClientByName(ri->player_name);
		if (i){
			if (IsRaidGrouped()){
				i->Message_StringID(CC_Default, ALREADY_IN_RAID, GetName()); //group failed, must invite members not in raid...
				return;
			}
			Raid *r = entity_list.GetRaidByClient(i);
			if (r){
				r->VerifyRaid();
				Group *g = GetGroup();
				if (g){
					if (g->GroupCount() + r->RaidCount() > MAX_RAID_MEMBERS)
					{
						i->Message(CC_Red, "Invite failed, group invite would create a raid larger than the maximum number of members allowed.");
						return;
					}
				}
				else{
					if (1 + r->RaidCount() > MAX_RAID_MEMBERS)
					{
						i->Message(CC_Red, "Invite failed, member invite would create a raid larger than the maximum number of members allowed.");
						return;
					}
				}
				if (g){//add us all
					uint32 freeGroup = r->GetFreeGroup();
					Client *addClient = nullptr;
					for (int x = 0; x < 6; x++)
					{
						if (g->members[x]){
							Client *c = nullptr;
							if (g->members[x]->IsClient())
								c = g->members[x]->CastToClient();
							else
								continue;

							if (!addClient)
							{
								addClient = c;
								r->SetGroupLeader(addClient->GetName());
							}

							r->SendRaidCreate(c);
							r->SendMakeLeaderPacketTo(r->leadername, c);
							if (g->IsLeader(g->members[x]))
								r->AddMember(c, freeGroup, false, true);
							else
								r->AddMember(c, freeGroup);
							r->SendBulkRaid(c);
							if (r->IsLocked()) {
								r->SendRaidLockTo(c);
							}
						}
					}
					g->DisbandGroup();
					r->GroupUpdate(freeGroup);
				}
				else{
					r->SendRaidCreate(this);
					r->SendMakeLeaderPacketTo(r->leadername, this);
					r->AddMember(this);
					r->SendBulkRaid(this);
					if (r->IsLocked()) {
						r->SendRaidLockTo(this);
					}
				}
			}
			else
			{
				Group *ig = i->GetGroup();
				Group *g = GetGroup();
				if (g) //if our target has a group
				{
					r = new Raid(i);
					entity_list.AddRaid(r);
					r->SetRaidDetails();

					uint32 groupFree = r->GetFreeGroup(); //get a free group
					if (ig){ //if we already have a group then cycle through adding us...
						Client *addClientig = nullptr;
						for (int x = 0; x < 6; x++)
						{
							if (ig->members[x]){
								if (!addClientig){
									if (ig->members[x]->IsClient()){
										addClientig = ig->members[x]->CastToClient();
										r->SetGroupLeader(addClientig->GetName());
									}
								}
								if (ig->IsLeader(ig->members[x])){
									Client *c = nullptr;
									if (ig->members[x]->IsClient())
										c = ig->members[x]->CastToClient();
									else
										continue;
									r->SendRaidCreate(c);
									r->SendMakeLeaderPacketTo(r->leadername, c);
									r->AddMember(c, groupFree, true, true, true);
									r->SendBulkRaid(c);
									if (r->IsLocked()) {
										r->SendRaidLockTo(c);
									}
								}
								else{
									Client *c = nullptr;
									if (ig->members[x]->IsClient())
										c = ig->members[x]->CastToClient();
									else
										continue;
									r->SendRaidCreate(c);
									r->SendMakeLeaderPacketTo(r->leadername, c);
									r->AddMember(c, groupFree);
									r->SendBulkRaid(c);
									if (r->IsLocked()) {
										r->SendRaidLockTo(c);
									}
								}
							}
						}
						ig->DisbandGroup();
						r->GroupUpdate(groupFree);
						groupFree = r->GetFreeGroup();
					}
					else{ //else just add the inviter
						r->SendRaidCreate(i);
						r->AddMember(i, 0xFFFFFFFF, true, false, true);
					}

					Client *addClient = nullptr;
					//now add the existing group
					for (int x = 0; x < 6; x++)
					{
						if (g->members[x]){
							if (!addClient)
							{
								if (g->members[x]->IsClient()){
									addClient = g->members[x]->CastToClient();
									r->SetGroupLeader(addClient->GetName());
								}
							}
							if (g->IsLeader(g->members[x]))
							{
								Client *c = nullptr;
								if (g->members[x]->IsClient())
									c = g->members[x]->CastToClient();
								else
									continue;
								r->SendRaidCreate(c);
								r->SendMakeLeaderPacketTo(r->leadername, c);
								r->AddMember(c, groupFree, false, true);
								r->SendBulkRaid(c);
								if (r->IsLocked()) {
									r->SendRaidLockTo(c);
								}
							}
							else
							{
								Client *c = nullptr;
								if (g->members[x]->IsClient())
									c = g->members[x]->CastToClient();
								else
									continue;
								r->SendRaidCreate(c);
								r->SendMakeLeaderPacketTo(r->leadername, c);
								r->AddMember(c, groupFree);
								r->SendBulkRaid(c);
								if (r->IsLocked()) {
									r->SendRaidLockTo(c);
								}
							}
						}
					}
					g->DisbandGroup();
					r->GroupUpdate(groupFree);
				}
				else
				{
					if (ig){
						r = new Raid(i);
						entity_list.AddRaid(r);
						r->SetRaidDetails();
						Client *addClientig = nullptr;
						for (int x = 0; x < 6; x++)
						{
							if (ig->members[x])
							{
								if (!addClientig){
									if (ig->members[x]->IsClient()){
										addClientig = ig->members[x]->CastToClient();
										r->SetGroupLeader(addClientig->GetName());
									}
								}
								if (ig->IsLeader(ig->members[x]))
								{
									Client *c = nullptr;
									if (ig->members[x]->IsClient())
										c = ig->members[x]->CastToClient();
									else
										continue;

									r->SendRaidCreate(c);
									r->SendMakeLeaderPacketTo(r->leadername, c);
									r->AddMember(c, 0, true, true, true);
									r->SendBulkRaid(c);
									if (r->IsLocked()) {
										r->SendRaidLockTo(c);
									}
								}
								else
								{
									Client *c = nullptr;
									if (ig->members[x]->IsClient())
										c = ig->members[x]->CastToClient();
									else
										continue;

									r->SendRaidCreate(c);
									r->SendMakeLeaderPacketTo(r->leadername, c);
									r->AddMember(c, 0);
									r->SendBulkRaid(c);
									if (r->IsLocked()) {
										r->SendRaidLockTo(c);
									}
								}
							}
						}
						r->SendRaidCreate(this);
						r->SendMakeLeaderPacketTo(r->leadername, this);
						r->SendBulkRaid(this);
						r->AddMember(this);
						ig->DisbandGroup();
						r->GroupUpdate(0);
						if (r->IsLocked()) {
							r->SendRaidLockTo(this);
						}
					}
					else{
						r = new Raid(i);
						entity_list.AddRaid(r);
						r->SetRaidDetails();
						r->SendRaidCreate(i);
						r->SendRaidCreate(this);
						r->SendMakeLeaderPacketTo(r->leadername, this);
						r->AddMember(i, 0xFFFFFFFF, true, false, true);
						r->SendBulkRaid(this);
						r->AddMember(this);
						if (r->IsLocked()) {
							r->SendRaidLockTo(this);
						}
					}
				}
			}
		}
		break;
	}
	case RaidCommandDisband: {
		Raid *r = entity_list.GetRaidByClient(this);
		if (r){
			//if(this == r->GetLeader()){
			uint32 grp = r->GetGroup(ri->leader_name);

			if (grp < 12){
				uint32 i = r->GetPlayerIndex(ri->leader_name);
				if (r->members[i].IsGroupLeader){ //assign group leader to someone else
					for (int x = 0; x < MAX_RAID_MEMBERS; x++){
						if (strlen(r->members[x].membername) > 0 && i != x){
							if (r->members[x].GroupNumber == grp){
								r->SetGroupLeader(ri->leader_name, false);
								r->SetGroupLeader(r->members[x].membername);
								break;
							}
						}
					}

				}
				if (r->members[i].IsRaidLeader){
					for (int x = 0; x < MAX_RAID_MEMBERS; x++){
						if (strlen(r->members[x].membername) > 0 && strcmp(r->members[x].membername, r->members[i].membername) != 0)
						{
							r->SetRaidLeader(r->members[i].membername, r->members[x].membername);
							break;
						}
					}
				}
			}

			r->RemoveMember(ri->leader_name);
			Client *c = entity_list.GetClientByName(ri->leader_name);
			if (c)
				r->SendGroupDisband(c);
			else{
				ServerPacket *pack = new ServerPacket(ServerOP_RaidGroupDisband, sizeof(ServerRaidGeneralAction_Struct));
				ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
				rga->rid = GetID();
				rga->zoneid = zone->GetZoneID();
				rga->instance_id = zone->GetInstanceID();
				strn0cpy(rga->playername, ri->leader_name, 64);
				worldserver.SendPacket(pack);
				safe_delete(pack);
			}
			//r->SendRaidGroupRemove(ri->leader_name, grp);
			r->GroupUpdate(grp);// break
			//}
		}
		break;
	}
	case RaidCommandMoveGroup:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			if (ri->parameter < 12) //moving to a group
			{
				uint8 grpcount = r->GroupCount(ri->parameter);

				if (grpcount < 6)
				{
					Client *c = entity_list.GetClientByName(ri->leader_name);
					uint32 oldgrp = r->GetGroup(ri->leader_name);
					if (ri->parameter == oldgrp) //don't rejoin grp if we order to join same group.
						break;

					if (r->members[r->GetPlayerIndex(ri->leader_name)].IsGroupLeader)
					{
						r->SetGroupLeader(ri->leader_name, false);
						if (oldgrp < 12){ //we were the leader of our old grp
							for (int x = 0; x < MAX_RAID_MEMBERS; x++) //assign a new grp leader if we can
							{
								if (r->members[x].GroupNumber == oldgrp)
								{
									if (strcmp(ri->leader_name, r->members[x].membername) != 0 && strlen(ri->leader_name) > 0)
									{
										r->SetGroupLeader(r->members[x].membername);
										Client *cgl = entity_list.GetClientByName(r->members[x].membername);
										if (cgl){
											r->SendRaidRemove(r->members[x].membername, cgl);
											r->SendRaidCreate(cgl);
											r->SendMakeLeaderPacketTo(r->leadername, cgl);
											r->SendRaidAdd(r->members[x].membername, cgl);
											r->SendBulkRaid(cgl);
											if (r->IsLocked()) {
												r->SendRaidLockTo(cgl);
											}
										}
										else{
											ServerPacket *pack = new ServerPacket(ServerOP_RaidChangeGroup, sizeof(ServerRaidGeneralAction_Struct));
											ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
											rga->rid = r->GetID();
											strn0cpy(rga->playername, r->members[x].membername, 64);
											rga->zoneid = zone->GetZoneID();
											rga->instance_id = zone->GetInstanceID();
											worldserver.SendPacket(pack);
											safe_delete(pack);
										}
										break;
									}
								}
							}
						}
					}
					if (grpcount == 0)
						r->SetGroupLeader(ri->leader_name);

					r->MoveMember(ri->leader_name, ri->parameter);
					if (c){
						r->SendGroupDisband(c);
					}
					else{
						ServerPacket *pack = new ServerPacket(ServerOP_RaidGroupDisband, sizeof(ServerRaidGeneralAction_Struct));
						ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
						rga->rid = r->GetID();
						rga->zoneid = zone->GetZoneID();
						rga->instance_id = zone->GetInstanceID();
						strn0cpy(rga->playername, ri->leader_name, 64);
						worldserver.SendPacket(pack);
						safe_delete(pack);
					}
					//r->SendRaidGroupAdd(ri->leader_name, ri->parameter);
					//r->SendRaidGroupRemove(ri->leader_name, oldgrp);
					//r->SendGroupUpdate(c);
					//break
					r->GroupUpdate(ri->parameter); //send group update to our new group
					if (oldgrp < 12) //if our old was a group send update there too
						r->GroupUpdate(oldgrp);

					//r->SendMakeGroupLeaderPacketAll();
				}
			}
			else //moving to ungrouped
			{
				Client *c = entity_list.GetClientByName(ri->leader_name);
				uint32 oldgrp = r->GetGroup(ri->leader_name);
				if (r->members[r->GetPlayerIndex(ri->leader_name)].IsGroupLeader){
					r->SetGroupLeader(ri->leader_name, false);
					for (int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						if (strlen(r->members[x].membername) > 0 && strcmp(r->members[x].membername, ri->leader_name) != 0)
						{
							r->SetGroupLeader(r->members[x].membername);
							Client *cgl = entity_list.GetClientByName(r->members[x].membername);
							if (cgl){
								r->SendRaidRemove(r->members[x].membername, cgl);
								r->SendRaidCreate(cgl);
								r->SendMakeLeaderPacketTo(r->leadername, cgl);
								r->SendRaidAdd(r->members[x].membername, cgl);
								r->SendBulkRaid(cgl);
								if (r->IsLocked()) {
									r->SendRaidLockTo(cgl);
								}
							}
							else{
								ServerPacket *pack = new ServerPacket(ServerOP_RaidChangeGroup, sizeof(ServerRaidGeneralAction_Struct));
								ServerRaidGeneralAction_Struct *rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
								rga->rid = r->GetID();
								strn0cpy(rga->playername, r->members[x].membername, 64);
								rga->zoneid = zone->GetZoneID();
								rga->instance_id = zone->GetInstanceID();
								worldserver.SendPacket(pack);
								safe_delete(pack);
							}
							break;
						}
					}
				}
				r->MoveMember(ri->leader_name, 0xFFFFFFFF);
				if (c){
					r->SendGroupDisband(c);
				}
				else{
					ServerPacket *pack = new ServerPacket(ServerOP_RaidGroupDisband, sizeof(ServerRaidGeneralAction_Struct));
					ServerRaidGeneralAction_Struct* rga = (ServerRaidGeneralAction_Struct*)pack->pBuffer;
					rga->rid = r->GetID();
					rga->zoneid = zone->GetZoneID();
					rga->instance_id = zone->GetInstanceID();
					strn0cpy(rga->playername, ri->leader_name, 64);
					worldserver.SendPacket(pack);
					safe_delete(pack);
				}
				//r->SendRaidGroupRemove(ri->leader_name, oldgrp);
				r->GroupUpdate(oldgrp);
				//r->SendMakeGroupLeaderPacketAll();
			}
		}
		break;
	}
	case RaidCommandRaidLock:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			if (!r->IsLocked())
				r->LockRaid(true);
			else
				r->SendRaidLockTo(this);
		}
		break;
	}
	case RaidCommandRaidUnlock:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			if (r->IsLocked())
				r->LockRaid(false);
			else
				r->SendRaidUnlockTo(this);
		}
		break;
	}
	case RaidCommandLootType2:
	case RaidCommandLootType:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			Message(CC_Yellow, "Loot type changed to: %d.", ri->parameter);
			r->ChangeLootType(ri->parameter);
		}
		break;
	}

	case RaidCommandAddLooter2:
	case RaidCommandAddLooter:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			Message(CC_Yellow, "Adding %s as a raid looter.", ri->leader_name);
			r->AddRaidLooter(ri->leader_name);
		}
		break;
	}

	case RaidCommandRemoveLooter2:
	case RaidCommandRemoveLooter:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			Message(CC_Yellow, "Removing %s as a raid looter.", ri->leader_name);
			r->RemoveRaidLooter(ri->leader_name);
		}
		break;
	}

	case RaidCommandMakeLeader:
	{
		Raid *r = entity_list.GetRaidByClient(this);
		if (r)
		{
			if (strcmp(r->leadername, GetName()) == 0){
				r->SetRaidLeader(GetName(), ri->leader_name);
			}
		}
		break;
	}

	default: {
		Message(CC_Red, "Raid command (%d) NYI", ri->action);
		break;
	}
	}
}

void Client::Handle_OP_RandomReq(const EQApplicationPacket *app)
{
	if (app->size != sizeof(RandomReq_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_RandomReq, size=%i, expected %i", app->size, sizeof(RandomReq_Struct));
		return;
	}
	const RandomReq_Struct* rndq = (const RandomReq_Struct*)app->pBuffer;
	uint32 randLow = rndq->low > rndq->high ? rndq->high : rndq->low;
	uint32 randHigh = rndq->low > rndq->high ? rndq->low : rndq->high;
	uint32 randResult;

	if (randLow == 0 && randHigh == 0)
	{	// defaults
		randLow = 0;
		randHigh = 100;
	}
	randResult = zone->random.Int(randLow, randHigh);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_RandomReply, sizeof(RandomReply_Struct));
	RandomReply_Struct* rr = (RandomReply_Struct*)outapp->pBuffer;
	rr->low = randLow;
	rr->high = randHigh;
	rr->result = randResult;
	strcpy(rr->name, GetName());
	entity_list.QueueCloseClients(this, outapp, false, 400);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_ReadBook(const EQApplicationPacket *app)
{
	if (app->size != sizeof(BookRequest_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_ReadBook, size=%i, expected %i", app->size, sizeof(BookRequest_Struct));
		return;
	}
	BookRequest_Struct* book = (BookRequest_Struct*)app->pBuffer;
	ReadBook(book);
	return;
}

void Client::Handle_OP_Report(const EQApplicationPacket *app)
{
	if (!CanUseReport)
	{
		Message_StringID(MT_System, REPORT_ONCE);
		return;
	}

	uint32 size = app->size;
	uint32 current_point = 0;
	std::string reported, reporter;
	std::string current_string;
	int mode = 0;

	while (current_point < size)
	{
		if (mode < 2)
		{
			if (app->pBuffer[current_point] == '|')
			{
				mode++;
			}
			else
			{
				if (mode == 0)
				{
					reported += app->pBuffer[current_point];
				}
				else
				{
					reporter += app->pBuffer[current_point];
				}
			}
			current_point++;
		}
		else
		{
			if (app->pBuffer[current_point] == 0x0a)
			{
				current_string += '\n';
			}
			else if (app->pBuffer[current_point] == 0x00)
			{
				CanUseReport = false;
				database.AddReport(reporter, reported, current_string);
				return;
			}
			else
			{
				current_string += app->pBuffer[current_point];
			}
			current_point++;
		}
	}

	CanUseReport = false;
	database.AddReport(reporter, reported, current_string);
}

void Client::Handle_OP_RequestDuel(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(Duel_Struct))
		return;

	EQApplicationPacket* outapp = app->Copy();
	Duel_Struct* ds = (Duel_Struct*)outapp->pBuffer;
	uint32 duel = ds->duel_initiator;
	ds->duel_initiator = ds->duel_target;
	ds->duel_target = duel;
	Entity* entity = entity_list.GetID(ds->duel_target);
	if (GetID() != ds->duel_target && entity->IsClient() && (entity->CastToClient()->IsDueling() && entity->CastToClient()->GetDuelTarget() != 0)) {
		Message_StringID(CC_Default, DUEL_CONSIDERING, entity->GetName());
		safe_delete(outapp);
		return;
	}
	if (IsDueling()) {
		Message_StringID(CC_Default, DUEL_INPROGRESS);
		safe_delete(outapp);
		return;
	}

	if (GetID() != ds->duel_target && entity->IsClient() && GetDuelTarget() == 0 && !IsDueling() && !entity->CastToClient()->IsDueling() && entity->CastToClient()->GetDuelTarget() == 0) {
		SetDuelTarget(ds->duel_target);
		entity->CastToClient()->SetDuelTarget(GetID());
		ds->duel_target = ds->duel_initiator;
		entity->CastToClient()->FastQueuePacket(&outapp);
		entity->CastToClient()->SetDueling(false);
		SetDueling(false);
		return;
	}
	
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_RezzAnswer(const EQApplicationPacket *app)
{
	VERIFY_PACKET_LENGTH(OP_RezzAnswer, app, Resurrect_Struct);

	const Resurrect_Struct* ra = (const Resurrect_Struct*)app->pBuffer;

	Log.Out(Logs::Detail, Logs::Spells, "Received OP_RezzAnswer from client. Pendingrezzexp is %i, action is %s",
		PendingRezzXP, ra->action ? "ACCEPT" : "DECLINE");


	OPRezzAnswer(ra->action, ra->spellid, ra->zone_id, ra->instance_id, ra->x, ra->y, ra->z);

	if (ra->action == 1)
	{
		if(GetPet() && !GetPet()->IsCharmed())
			DepopPet();

		EQApplicationPacket* outapp = app->Copy();
		// Send the OP_RezzComplete to the world server. This finds it's way to the zone that
		// the rezzed corpse is in to mark the corpse as rezzed.
		outapp->SetOpcode(OP_RezzComplete);
		worldserver.RezzPlayer(outapp, 0, 0, OP_RezzComplete);
		safe_delete(outapp);
	}
	return;
}

void Client::Handle_OP_Sacrifice(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) {
		DumpPacket(app);
		return;
	}

	if (app->size != sizeof(Sacrifice_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_Sacrifice expected %i got %i", sizeof(Sacrifice_Struct), app->size);
		DumpPacket(app);
		return;
	}
	Sacrifice_Struct *ss = (Sacrifice_Struct*)app->pBuffer;

	if (!PendingSacrifice) {
		Log.Out(Logs::General, Logs::Error, "Unexpected OP_Sacrifice reply");
		DumpPacket(app);
		return;
	}

	if (ss->Confirm) {
		Client *Caster = entity_list.GetClientByName(SacrificeCaster.c_str());
		if (Caster) Sacrifice(Caster);
	}
	PendingSacrifice = false;
	SacrificeCaster.clear();
}

void Client::Handle_OP_SafeFallSuccess(const EQApplicationPacket *app)	// bit of a misnomer, sent whenever safe fall is used (success of fail)
{
	if (HasSkill(SkillSafeFall)) //this should only get called if the client has safe fall, but just in case...
		CheckIncreaseSkill(SkillSafeFall, nullptr, zone->skill_difficulty[SkillSafeFall].difficulty, 1.0); //check for skill up
}

void Client::Handle_OP_SafePoint(const EQApplicationPacket *app)
{
}

void Client::Handle_OP_Save(const EQApplicationPacket *app)
{
	// The payload is 192 bytes - Not sure what is contained in payload
	Save();
	return;
}

void Client::Handle_OP_SaveOnZoneReq(const EQApplicationPacket *app)
{
	Handle_OP_Save(app);
}

void Client::Handle_OP_SenseHeading(const EQApplicationPacket *app)
{
	CheckIncreaseSkill(SkillSenseHeading, nullptr, zone->skill_difficulty[SkillSenseHeading].difficulty, 1.0);
	return;
}

void Client::Handle_OP_SenseTraps(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (!HasSkill(SkillSenseTraps))
		return;

	if (!p_timers.Expired(&database, pTimerSenseTraps, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	int reuse = SenseTrapsReuseTime;
	switch (GetAA(aaAdvTrapNegotiation)) {
	case 1:
		reuse -= 1;
		break;
	case 2:
		reuse -= 3;
		break;
	case 3:
		reuse -= 5;
		break;
	}
	p_timers.Start(pTimerSenseTraps, reuse - 1);

	Trap* trap = entity_list.FindNearbyTrap(this, 800);	
	float success = 2.0;
	if (trap && trap->skill > 0) {
		int uskill = GetSkill(SkillSenseTraps);
		if ((zone->random.Int(0, 99) + uskill) >= (zone->random.Int(0, 99) + trap->skill*0.75))
		{
			auto diff = trap->m_Position - glm::vec3(GetPosition());

			if (diff.x == 0 && diff.y == 0)
				Message(MT_Skills, "You sense a trap right under your feet!");
			else if (diff.x > 10 && diff.y > 10)
				Message(MT_Skills, "You sense a trap to the NorthWest.");
			else if (diff.x < -10 && diff.y > 10)
				Message(MT_Skills, "You sense a trap to the NorthEast.");
			else if (diff.y > 10)
				Message(MT_Skills, "You sense a trap to the North.");
			else if (diff.x > 10 && diff.y < -10)
				Message(MT_Skills, "You sense a trap to the SouthWest.");
			else if (diff.x < -10 && diff.y < -10)
				Message(MT_Skills, "You sense a trap to the SouthEast.");
			else if (diff.y < -10)
				Message(MT_Skills, "You sense a trap to the South.");
			else if (diff.x > 10)
				Message(MT_Skills, "You sense a trap to the West.");
			else
				Message(MT_Skills, "You sense a trap to the East.");
			trap->detected = true;

			float angle = CalculateHeadingToTarget(trap->m_Position.x, trap->m_Position.y);

			if (angle < 0)
				angle = (256 + angle);

			angle *= 2;
			MovePC(zone->GetZoneID(), zone->GetInstanceID(), GetX(), GetY(), GetZ(), angle);
			success = 1.0;
		}
	}

	if(success == 2)
		Message(MT_Skills, "You did not find any traps nearby.");

	CheckIncreaseSkill(SkillSenseTraps, nullptr, zone->skill_difficulty[SkillSenseTraps].difficulty, success);
}

void Client::Handle_OP_SetGuildMOTD(const EQApplicationPacket *app)
{
	Log.Out(Logs::Detail, Logs::Guilds, "Received OP_SetGuildMOTD");

	if (app->size != sizeof(GuildMOTD_Struct)) {
		// client calls for a motd on login even if they arent in a guild
		Log.Out(Logs::Detail, Logs::Guilds, "Error: app size of %i != size of GuildMOTD_Struct of %zu\n", app->size, sizeof(GuildMOTD_Struct));
		return;
	}
	if (!IsInAGuild()) {
		Message(CC_Red, "You are not in a guild!");
		return;
	}
	if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_MOTD)) {
		return;
	}

	GuildMOTD_Struct* gmotd = (GuildMOTD_Struct*)app->pBuffer;
	if (gmotd->motd[0] == 0)
	{
		Log.Out(Logs::Detail, Logs::Guilds, "Client is trying to remove MOTD. This may be intentional but still will be prevented for now.");
		return; //EQMac sends this on login, and it overwrites the existing MOTD. Need to figure out a better way to handle it
	}

	Log.Out(Logs::Detail, Logs::Guilds, "Setting MOTD for %s (%d) to: %s - %s",
		guild_mgr.GetGuildName(GuildID()), GuildID(), GetName(), gmotd->motd);

	if (!guild_mgr.SetGuildMOTD(GuildID(), gmotd->motd, GetName())) {
		Message(CC_Default, "Motd update failed.");
	}

	return;
}

void Client::Handle_OP_SetGuildMOTDCon(const EQApplicationPacket *app)
{
	//EQMac sends this while connecting, and it will overwrite the MOTD if the player has permission.
	return;
}

void Client::Handle_OP_SetRunMode(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SetRunMode_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized "
		"OP_SetRunMode: got %d, expected %d", app->size,
		sizeof(SetRunMode_Struct));
		DumpPacket(app);
		return;
	}
	SetRunMode_Struct* rms = (SetRunMode_Struct*)app->pBuffer;
	if (rms->mode)
		runmode = true;
	else
		runmode = false;
}

void Client::Handle_OP_SetServerFilter(const EQApplicationPacket *app)
{

	if (app->size != sizeof(SetServerFilter_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Received invalid sized "
			"OP_SetServerFilter: got %d, expected %d", app->size,
			sizeof(SetServerFilter_Struct));
		DumpPacket(app);
		return;
	}
	SetServerFilter_Struct* filter = (SetServerFilter_Struct*)app->pBuffer;
	ServerFilter(filter);
	return;
}

void Client::Handle_OP_SetTitle(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SetTitle_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_SetTitle expected %i got %i", sizeof(SetTitle_Struct), app->size);
		DumpPacket(app);
		return;
	}

	SetTitle_Struct *sts = (SetTitle_Struct *)app->pBuffer;

	std::string Title;

	if (!sts->is_suffix)
	{
		Title = title_manager.GetPrefix(sts->title_id);
		SetAATitle(Title.c_str());
	}
	else
	{
		Title = title_manager.GetSuffix(sts->title_id);
		SetTitleSuffix(Title.c_str());
	}
}

void Client::Handle_OP_Shielding(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(Shielding_Struct)) {
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_Shielding expected:%i got:%i", sizeof(Shielding_Struct), app->size);
		return;

	}
	if (GetClass() != WARRIOR)
	{
		return;
	}

	if (shield_target)
	{
		entity_list.MessageClose_StringID(this, false, 100, 0,
			END_SHIELDING, GetName(), shield_target->GetName());
		for (int y = 0; y < 2; y++)
		{
			if (shield_target->shielder[y].shielder_id == GetID())
			{
				shield_target->shielder[y].shielder_id = 0;
				shield_target->shielder[y].shielder_bonus = 0;
			}
		}
	}
	Shielding_Struct* shield = (Shielding_Struct*)app->pBuffer;
	shield_target = entity_list.GetMob(shield->target_id);
	bool ack = false;
	ItemInst* inst = GetInv().GetItem(MainSecondary);
	if (!shield_target)
		return;
	if (inst)
	{
		const Item_Struct* shield = inst->GetItem();
		if (shield && shield->ItemType == ItemTypeShield)
		{
			for (int x = 0; x < 2; x++)
			{
				if (shield_target->shielder[x].shielder_id == 0)
				{
					entity_list.MessageClose_StringID(this, false, 100, 0,
						START_SHIELDING, GetName(), shield_target->GetName());
					shield_target->shielder[x].shielder_id = GetID();
					int shieldbonus = shield->AC * 2;
					switch (GetAA(197))
					{
					case 1:
						shieldbonus = shieldbonus * 115 / 100;
						break;
					case 2:
						shieldbonus = shieldbonus * 125 / 100;
						break;
					case 3:
						shieldbonus = shieldbonus * 150 / 100;
						break;
					}
					shield_target->shielder[x].shielder_bonus = shieldbonus;
					shield_timer.Start();
					ack = true;
					break;
				}
			}
		}
		else
		{
			Message(CC_Default, "You must have a shield equipped to shield a target!");
			shield_target = 0;
			return;
		}
	}
	else
	{
		Message(CC_Default, "You must have a shield equipped to shield a target!");
		shield_target = 0;
		return;
	}
	if (!ack)
	{
		Message_StringID(CC_Default, ALREADY_SHIELDED);
		shield_target = 0;
		return;
	}
	return;
}

void Client::Handle_OP_ShopEnd(const EQApplicationPacket *app)
{
	SendMerchantEnd();
}

void Client::Handle_OP_ShopPlayerBuy(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Merchant_Sell_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size on OP_ShopPlayerBuy: Expected %i, Got %i",
			sizeof(Merchant_Sell_Struct), app->size);
		return;
	}
	RDTSC_Timer t1;
	t1.start();
	Merchant_Sell_Struct* mp = (Merchant_Sell_Struct*)app->pBuffer;
#if EQDEBUG >= 11
	Log.Out(Logs::General, Logs::None, "%s, purchase item..", GetName());
	DumpPacket(app);
#endif

	int merchantid;
	bool tmpmer_used = false;
	Mob* tmp = entity_list.GetMob(mp->npcid);

	if (tmp == 0 || !tmp->IsNPC() || tmp->GetClass() != MERCHANT)
		return;

	if (mp->quantity < 1) return;

	//you have to be somewhat close to them to be properly using them
	if (DistanceSquared(m_Position, tmp->GetPosition()) > USE_NPC_RANGE2)
		return;

	merchantid = tmp->CastToNPC()->MerchantType;

	uint32 item_id = 0;
	uint8 quantity_left = 0;
	std::list<MerchantList> merlist = zone->merchanttable[merchantid];
	std::list<MerchantList>::const_iterator itr;
	for (itr = merlist.begin(); itr != merlist.end(); ++itr){
		MerchantList ml = *itr;
		if (GetLevel() < ml.level_required) {
			continue;
		}

		int32 fac = tmp->GetPrimaryFaction();
		if (fac != 0 && GetModCharacterFactionLevel(fac) < ml.faction_required) {
			continue;
		}

		if(ml.quantity > 0 && ml.qty_left <= 0)
		{
			continue;
		}

		if (mp->itemslot == ml.slot){
			item_id = ml.item;
			if(ml.quantity > 0 && ml.qty_left > 0)
			{
				quantity_left = ml.qty_left;
			}
			break;
		}
	}
	const Item_Struct* item = nullptr;
	uint32 prevcharges = 0;
	if (item_id == 0) { //check to see if its on the temporary table
		std::list<TempMerchantList> tmp_merlist = zone->tmpmerchanttable[tmp->GetNPCTypeID()];
		std::list<TempMerchantList>::const_iterator tmp_itr;
		TempMerchantList ml;
		for (tmp_itr = tmp_merlist.begin(); tmp_itr != tmp_merlist.end(); ++tmp_itr){
			ml = *tmp_itr;
			if (mp->itemslot == ml.slot){
				item_id = ml.item;
				tmpmer_used = true;
				prevcharges = ml.charges;
				break;
			}
		}
	}
	item = database.GetItem(item_id);
	if (!item){
		//error finding item, client didnt get the update packet for whatever reason, roleplay a tad
		Message(CC_Default, "%s tells you 'Sorry, that item is for display purposes only.' as they take the item off the shelf.", tmp->GetCleanName());
		entity_list.SendMerchantInventory(tmp, mp->itemslot, true);
		return;
	}
	if (CheckLoreConflict(item))
	{
		Message(CC_Yellow, "You can only have one of a lore item.");
		return;
	}

	// This makes sure the vendor deletes charged items from their lists properly.
	uint8 tmp_qty = 0;
	// Temp merchantlist
	if(tmpmer_used)
		tmp_qty = prevcharges;
	// Regular merchantlist with limited supplies
	else if(quantity_left > 0)
		tmp_qty = quantity_left;

	if ((tmpmer_used || quantity_left > 0) && (mp->quantity > tmp_qty || database.ItemQuantityType(item_id) == Quantity_Charges))
	{
		if (database.ItemQuantityType(item_id) == Quantity_Charges && tmpmer_used)
			mp->quantity = zone->GetTempMerchantQtyNoSlot(tmp->GetNPCTypeID(), item_id);	
		else
			mp->quantity = tmp_qty;
	}
	else if(database.ItemQuantityType(item_id) == Quantity_Charges && !tmpmer_used)
		mp->quantity = item->MaxCharges;

	uint8 quantity = mp->quantity;
	//This makes sure we don't overflow the quantity in our packet.
	if(database.ItemQuantityType(item_id) == Quantity_Stacked && quantity > 20)
		quantity = 20; 

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopPlayerBuy, sizeof(Merchant_Sell_Struct));
	Merchant_Sell_Struct* mpo = (Merchant_Sell_Struct*)outapp->pBuffer;
	mpo->quantity = (item->MaxCharges > 1 ? 1 : quantity);
	mpo->playerid = mp->playerid;
	mpo->npcid = mp->npcid;
	mpo->itemslot = mp->itemslot;

	int16 freeslotid = INVALID_INDEX;

	ItemInst* inst = database.CreateItem(item, quantity);

	int SinglePrice = 0;
	if (RuleB(Merchant, UsePriceMod))
		SinglePrice = (item->Price * (RuleR(Merchant, SellCostMod)) * item->SellRate * Client::CalcPriceMod(tmp, false));
	else
		SinglePrice = (item->Price * (RuleR(Merchant, SellCostMod)) * item->SellRate);

	if (item->MaxCharges > 1)
		mpo->price = SinglePrice;
	else
		mpo->price = SinglePrice * mp->quantity;
	if (mpo->price < 0)
	{
		safe_delete(outapp);
		safe_delete(inst);
		return;
	}

	// this area needs some work..two inventory insertion check failure points
	// below do not return player's money..is this the intended behavior?

	if (!TakeMoneyFromPP(mpo->price))
	{
		char *hacker_str = nullptr;
		MakeAnyLenString(&hacker_str, "Vendor Cheat: attempted to buy %i of %i: %s that cost %d cp but only has %d pp %d gp %d sp %d cp\n",
			mpo->quantity, item->ID, item->Name,
			mpo->price, m_pp.platinum, m_pp.gold, m_pp.silver, m_pp.copper);
		database.SetMQDetectionFlag(AccountName(), GetName(), hacker_str, zone->GetShortName());
		safe_delete_array(hacker_str);
		safe_delete(outapp);
		safe_delete(inst);
		return;
	}

	bool stacked = TryStacking(inst);
	bool bag = false;
	bool cursor = true;
	if (inst->IsType(ItemClassContainer))
	{
		bag = true;
		cursor = false;
	}

	//If stacked returns true, the items have already been placed in the inventory. No need to check for available slot
	//or if the player inventory is full.
	if (!stacked)
	{
		freeslotid = m_inv.FindFreeSlot(bag, cursor, item->Size);

		//make sure we are not completely full...
		if (freeslotid == MainCursor || freeslotid == INVALID_INDEX) {
			if (m_inv.GetItem(MainCursor) != nullptr || freeslotid == INVALID_INDEX) {
				Message(CC_Red, "You do not have room for any more items.");
				entity_list.CreateGroundObject(inst->GetID(), glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime));
				QueuePacket(outapp);
				safe_delete(outapp);
				safe_delete(inst);
				return;
			}
		}
	}

	if (!stacked && inst) {
		if(PutItemInInventory(freeslotid, *inst))
		{
			if (freeslotid == MainCursor)
				SendItemPacket(freeslotid, inst, ItemPacketSummonItem);
			else
				SendItemPacket(freeslotid, inst, ItemPacketTrade);
		}
	}
	else if (!stacked){
		Log.Out(Logs::General, Logs::Error, "OP_ShopPlayerBuy: item->ItemClass Unknown! Type: %i", item->ItemClass);
	}

	QueuePacket(outapp);

	if (inst && (tmpmer_used || quantity_left > 0)){
		int32 new_charges = 0;
		if(tmpmer_used)
		{
			new_charges = prevcharges - mp->quantity;
			zone->SaveTempItem(merchantid, tmp->GetNPCTypeID(), item_id, new_charges);
		}
		else if(quantity_left > 0)
		{
			new_charges = quantity_left - mp->quantity;
			zone->SaveMerchantItem(merchantid,item_id, new_charges, mp->itemslot);
		}

		int8 new_quantity = zone->GetTempMerchantQtyNoSlot(tmp->GetNPCTypeID(), item_id);
		if ((database.ItemQuantityType(item_id) == Quantity_Charges && new_quantity < 0) ||
			(database.ItemQuantityType(item_id) != Quantity_Charges && new_charges <= 0))
		{
			entity_list.SendMerchantInventory(tmp, mp->itemslot, true);
		}
		else 
		{
			inst->SetCharges(new_charges);
			inst->SetPrice(SinglePrice);
			inst->SetMerchantSlot(mp->itemslot);
			inst->SetMerchantCount(new_charges);
			entity_list.SendMerchantInventory(tmp);
		}
	}
	safe_delete(inst);
	safe_delete(outapp);

	// start QS code
	// stacking purchases not supported at this time - entire process will need some work to catch them properly
	if (RuleB(QueryServ, PlayerLogMerchantTransactions))
	{
		QServ->QSMerchantTransactions(character_id, zone->GetZoneID(), freeslotid == INVALID_INDEX ? 0 : freeslotid,
			item->ID, mpo->quantity, tmp->CastToNPC()->MerchantType, 0, 0, 0, 0, 1,
			mpo->price / 1000, (mpo->price / 100) % 10, (mpo->price / 10) % 10, mpo->price % 10, 0);
	}
	// end QS code

	if (RuleB(EventLog, RecordBuyFromMerchant))
		LogMerchant(this, tmp, mpo->quantity, mpo->price, item, true);

	if ((RuleB(Character, EnableDiscoveredItems)))
	{
		if (!GetGM() && !IsDiscovered(item_id))
			DiscoverItem(item_id);
	}

	t1.stop();
	return;
}

void Client::Handle_OP_ShopPlayerSell(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin()<= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(Merchant_Purchase_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size on OP_ShopPlayerSell: Expected %i, Got %i",
			sizeof(Merchant_Purchase_Struct), app->size);
		return;
	}
	RDTSC_Timer t1(true);
	Merchant_Purchase_Struct* mp = (Merchant_Purchase_Struct*)app->pBuffer;

	Mob* vendor = entity_list.GetMob(mp->npcid);

	if (vendor == 0 || !vendor->IsNPC() || vendor->GetClass() != MERCHANT)
		return;

	//you have to be somewhat close to them to be properly using them
	if (DistanceSquared(m_Position, vendor->GetPosition()) > USE_NPC_RANGE2)
		return;

	uint32 price = 0;
	uint32 itemid = GetItemIDAt(mp->itemslot);
	if (itemid == 0)
		return;
	const Item_Struct* item = database.GetItem(itemid);
	ItemInst* inst = GetInv().GetItem(mp->itemslot);
	if (!item || !inst){
		Message(CC_Red, "You seemed to have misplaced that item..");
		return;
	}
	if (mp->quantity > 1)
	{
		if ((inst->GetCharges() < 0) || (mp->quantity > (uint32)inst->GetCharges()))
			return;
	}

	if (!item->NoDrop) {
		return;
	}

	int cost_quantity = mp->quantity;
	if (database.ItemQuantityType(inst->GetID()) == Quantity_Charges)
		int cost_quantity = 1; //Always sell items to merchants for base cost

	if (RuleB(Merchant, UsePriceMod))
		price = (int)((item->Price*cost_quantity)*(RuleR(Merchant, BuyCostMod))*Client::CalcPriceMod(vendor, true) + 0.5); // need to round up, because client does it automatically when displaying price
	else
		price = (int)((item->Price*cost_quantity)*(RuleR(Merchant, BuyCostMod)) + 0.5);
	AddMoneyToPP(price, false);

	if (inst->IsStackable())
	{
		unsigned int i_quan = inst->GetCharges();
		if (mp->quantity > i_quan)
			mp->quantity = i_quan;
	}
	else if(database.ItemQuantityType(inst->GetID()) == Quantity_Charges)
	{
		int8 final_charges = zone->GetTempMerchantQtyNoSlot(vendor->GetNPCTypeID(), item->ID);
		mp->quantity = (final_charges < 0 ? inst->GetCharges() : final_charges);
	}
	else
	{
		mp->quantity = 1;
	}

	if (RuleB(EventLog, RecordSellToMerchant))
		LogMerchant(this, vendor, mp->quantity, price, item, false);

	int charges = mp->quantity;
	int freeslot = 0;
	if (charges >= 0 && (freeslot = zone->SaveTempItem(vendor->CastToNPC()->MerchantType, vendor->GetNPCTypeID(), itemid, charges, true)) > 0){
		ItemInst* inst2 = inst->Clone();
		if (RuleB(Merchant, UsePriceMod)){
			inst2->SetPrice(item->Price*(RuleR(Merchant, SellCostMod))*item->SellRate*Client::CalcPriceMod(vendor, false));
		}
		else
			inst2->SetPrice(item->Price*(RuleR(Merchant, SellCostMod))*item->SellRate);
		inst2->SetMerchantSlot(freeslot);

		uint32 MerchantQuantity = zone->GetTempMerchantQuantity(vendor->GetNPCTypeID(), freeslot);

		if (inst2->IsStackable()) {
			inst2->SetCharges(MerchantQuantity);
		}
		inst2->SetMerchantCount(MerchantQuantity);

		entity_list.SendMerchantInventory(vendor);

		safe_delete(inst2);
	}

	// start QS code
	if (RuleB(QueryServ, PlayerLogMerchantTransactions))
	{
		QServ->QSMerchantTransactions(character_id, zone->GetZoneID(), mp->itemslot,
			itemid, charges, vendor->CastToNPC()->MerchantType, price / 1000, (price / 100) % 10,
			(price / 10) % 10, price % 10, 0, 0, 0, 0, 0, 1);
	}
	// end QS code

	// Now remove the item from the player, this happens regardless of outcome
	if (!inst->IsStackable())
		this->DeleteItemInInventory(mp->itemslot, 0, false);
	else
		this->DeleteItemInInventory(mp->itemslot, mp->quantity, false);

	//This forces the price to show up correctly for charged items.
	if (database.ItemQuantityType(inst->GetID()) == Quantity_Charges)
		mp->quantity = 1;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopPlayerSell, sizeof(OldMerchant_Purchase_Struct));
	OldMerchant_Purchase_Struct* mco = (OldMerchant_Purchase_Struct*)outapp->pBuffer;

	mco->itemslot = mp->itemslot;
	mco->npcid = vendor->GetID();
	mco->quantity = mp->quantity;
	mco->price = price;
	mco->playerid = this->GetID();
	QueuePacket(outapp);
	safe_delete(outapp);
	Save(1);

	return;
}

void Client::Handle_OP_ShopRequest(const EQApplicationPacket *app) 
{
	if (app->size != sizeof(Merchant_Click_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_ShopRequest, size=%i, expected %i", app->size, sizeof(Merchant_Click_Struct));
		return;
	}

	Merchant_Click_Struct* mc = (Merchant_Click_Struct*)app->pBuffer;

	// Send back opcode OP_ShopRequest - tells client to open merchant window.
	//EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopRequest, sizeof(Merchant_Click_Struct));
	//Merchant_Click_Struct* mco=(Merchant_Click_Struct*)outapp->pBuffer;
	int merchantid = 0;
	Mob* tmp = entity_list.GetMob(mc->npcid);

	if (tmp == 0 || !tmp->IsNPC() || tmp->GetClass() != MERCHANT)
		return;

	//you have to be somewhat close to them to be properly using them
	if (DistanceSquared(m_Position, tmp->GetPosition()) > USE_NPC_RANGE2)
		return;

	merchantid = tmp->CastToNPC()->MerchantType;

	int action = 1;
	if (merchantid == 0) {
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopRequest, sizeof(Merchant_Click_Struct));
		Merchant_Click_Struct* mco = (Merchant_Click_Struct*)outapp->pBuffer;
		mco->npcid = mc->npcid;
		mco->playerid = 0;
		mco->command = 1;		//open...
		mco->rate = 1.0;
		QueuePacket(outapp);
		safe_delete(outapp);
		return;
	}
	if (tmp->IsEngaged()){
		this->Message_StringID(CC_Default, MERCHANT_BUSY);
		action = 0;
	}
	if (GetFeigned() || IsInvisible())
	{
		Message(CC_Default, "You cannot use a merchant right now.");
		action = 0;
	}
	int primaryfaction = tmp->CastToNPC()->GetPrimaryFaction();
	int factionlvl = GetFactionLevel(CharacterID(), tmp->CastToNPC()->GetNPCTypeID(), GetRace(), GetClass(), GetDeity(), primaryfaction, tmp);
	if (factionlvl >= 7) {
		MerchantRejectMessage(tmp, primaryfaction);
		action = 0;
	}

	if (tmp->Charmed())
		action = 0;

	// 1199 I don't have time for that now. etc
	if (!tmp->CastToNPC()->IsMerchantOpen()) {
		tmp->Say_StringID(zone->random.Int(1199, 1202));
		action = 0;
	}

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopRequest, sizeof(Merchant_Click_Struct));
	Merchant_Click_Struct* mco = (Merchant_Click_Struct*)outapp->pBuffer;

	mco->npcid = mc->npcid;
	mco->playerid = 0;
	mco->command = action; // Merchant command 0x01 = open
	if (RuleB(Merchant, UsePriceMod)){
		mco->rate = 1 / ((RuleR(Merchant, BuyCostMod))*Client::CalcPriceMod(tmp, true)); // works
	}
	else
		mco->rate = 1 / (RuleR(Merchant, BuyCostMod));

	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	if (action == 1)
		BulkSendMerchantInventory(merchantid, tmp->GetNPCTypeID());

	MerchantSession = tmp->GetID();

	return;
}

void Client::Handle_OP_Sneak(const EQApplicationPacket *app)
{
	if (!HasSkill(SkillSneak) && GetClass() != ROGUE && GetBaseRace() != HALFLING && GetBaseRace() != VAHSHIR) {
		return;
	}

	if (!p_timers.Expired(&database, pTimerSneak, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerSneak, SneakReuseTime - 1);

	bool checkskill = true;
	bool was = sneaking;
	if (sneaking){
		checkskill = false;
		sneaking = false;
		hidden = false;
		improved_hidden = false;

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
		SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
		sa_out->spawn_id = GetID();
		sa_out->type = AT_Invis;
		sa_out->parameter = 0;
		entity_list.QueueClients(this, outapp, true);
		safe_delete(outapp);

	}

	float success = 2.0;
	float hidechance = ((GetSkill(SkillSneak) / 300.0f) + .25) * 100;
	float random = zone->random.Real(0, 99);
	if (!was && random < hidechance) {
		sneaking = true;
		success = 1.0;
	}
	else
	{
		sneaking = false;
	}

	if (GetClass() == ROGUE){
		if (sneaking){
			Message_StringID(MT_Skills, SNEAK_SUCCESS);
		}
		else {
			Message_StringID(MT_Skills, SNEAK_FAIL);
		}
	}

	if(checkskill)
		CheckIncreaseSkill(SkillSneak, nullptr, zone->skill_difficulty[SkillSneak].difficulty, success);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
	sa_out->spawn_id = GetID();
	sa_out->type = AT_Sneak;
	sa_out->parameter = sneaking;
	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);

	return;
}

void Client::Handle_OP_SpawnAppearance(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SpawnAppearance_Struct)) {
		std::cout << "Wrong size on OP_SpawnAppearance. Got: " << app->size << ", Expected: " << sizeof(SpawnAppearance_Struct) << std::endl;
		return;
	}
	SpawnAppearance_Struct* sa = (SpawnAppearance_Struct*)app->pBuffer;

	if (sa->spawn_id != GetID())
		return;

	if (sa->type == AT_Invis) {
		if (sa->parameter != 0)
		{
			if (!HasSkill(SkillHide) && GetSkill(SkillHide) == 0)
			{
				char *hack_str = nullptr;
				MakeAnyLenString(&hack_str, "Player sent OP_SpawnAppearance with AT_Invis: %i", sa->parameter);
				database.SetMQDetectionFlag(this->account_name, this->name, hack_str, zone->GetShortName());
				safe_delete_array(hack_str);
			}
			return;
		}
		invisible = false;
		hidden = false;
		improved_hidden = false;
		entity_list.QueueClients(this, app, true);
		return;
	}
	else if (sa->type == AT_Anim) {
		if (IsAIControlled())
			return;

		if (sa->parameter == ANIM_STAND) {
			SetAppearance(eaStanding);
			playeraction = 0;
			SetFeigned(false);
			BindWound(this, false, true);
			camp_timer.Disable();
		}
		else if (sa->parameter == ANIM_SIT) {
			SetAppearance(eaSitting);
			playeraction = 1;
			if (!UseBardSpellLogic())
				InterruptSpell();
			SetFeigned(false);
			BindWound(this, false, true);
		}
		else if (sa->parameter == ANIM_CROUCH) {
			if (!UseBardSpellLogic())
				InterruptSpell();
			SetAppearance(eaCrouching);
			playeraction = 2;
			SetFeigned(false);
		}
		else if (sa->parameter == ANIM_DEATH) { // feign death too
			SetAppearance(eaDead);
			playeraction = 3;
			InterruptSpell();
		}
		else if (sa->parameter == ANIM_LOOT) {
			SetAppearance(eaLooting);
			playeraction = 4;
			SetFeigned(false);
		}

		else {
			std::cerr << "Client " << name << " unknown apperance " << (int)sa->parameter << std::endl;
			return;
		}

		entity_list.QueueClients(this, app, true);
	}
	else if (sa->type == AT_Anon) {
		if(!anon_toggle_timer.Check()) {
			return;
		}

		// For Anon/Roleplay
		if (sa->parameter == 1) { // Anon
			m_pp.anon = 1;
		}
		else if ((sa->parameter == 2) || (sa->parameter == 3)) { // This is Roleplay, or anon+rp
			m_pp.anon = 2;
		}
		else if (sa->parameter == 0) { // This is Non-Anon
			m_pp.anon = 0;
		}
		else {
			std::cerr << "Client " << name << " unknown Anon/Roleplay Switch " << (int)sa->parameter << std::endl;
			return;
		}
		entity_list.QueueClients(this, app, true);
		UpdateWho();
	}
	else if ((sa->type == AT_HP) && (dead == 0)) {
		return;
	}
	else if (sa->type == AT_AFK) {
		if(afk_toggle_timer.Check()) {
			AFK = (sa->parameter == 1);
			entity_list.QueueClients(this, app, true);
		}
	}
	else if (sa->type == AT_Split) {
		m_pp.autosplit = (sa->parameter == 1);
	}
	else if (sa->type == AT_Sneak) {
		if(sneaking == 0)
			return;

		if (sa->parameter != 0)
		{
			if (!HasSkill(SkillSneak))
			{
				char *hack_str = nullptr;
				MakeAnyLenString(&hack_str, "Player sent OP_SpawnAppearance with AT_Sneak: %i", sa->parameter);
				database.SetMQDetectionFlag(this->account_name, this->name, hack_str, zone->GetShortName());
				safe_delete_array(hack_str);
			}
			return;
		}
		sneaking = 0;
		entity_list.QueueClients(this, app, true);
	}
	else if (sa->type == AT_Size)
	{
		char *hack_str = nullptr;
		MakeAnyLenString(&hack_str, "Player sent OP_SpawnAppearance with AT_Size: %i", sa->parameter);
		database.SetMQDetectionFlag(this->account_name, this->name, hack_str, zone->GetShortName());
		safe_delete_array(hack_str);
	}
	else if (sa->type == AT_Light)	// client emitting light (lightstone, shiny shield)
	{
		//don't do anything with this
	}
	else if (sa->type == AT_Levitate)
	{
		// don't do anything with this, we tell the client when it's
		// levitating, not the other way around
	}
	else {
		std::cout << "Unknown SpawnAppearance type: 0x" << std::hex << std::setw(4) << std::setfill('0') << sa->type << std::dec
			<< " value: 0x" << std::hex << std::setw(8) << std::setfill('0') << sa->parameter << std::dec << std::endl;
	}
	return;
}

void Client::Handle_OP_Split(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(Split_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_Split, size=%i, expected %i", app->size, sizeof(Split_Struct));
		return;
	}
	// The client removes the money on its own, but we have to
	// update our state anyway, and make sure they had enough to begin
	// with.
	Split_Struct *split = (Split_Struct *)app->pBuffer;
	//Per the note above, Im not exactly sure what to do on error
	//to notify the client of the error...
	if (!isgrouped) {
		Message(CC_Red, "You can not split money if your not in a group.");
		return;
	}
	Group *cgroup = GetGroup();
	if (cgroup == nullptr) {
		//invalid group, not sure if we should say more...
		Message(CC_Red, "You can not split money if your not in a group.");
		return;
	}

	if (!TakeMoneyFromPP(static_cast<uint64>(split->copper) +
		10 * static_cast<uint64>(split->silver) +
		100 * static_cast<uint64>(split->gold) +
		1000 * static_cast<uint64>(split->platinum))) {
		Message(CC_Red, "You do not have enough money to do that split.");
		return;
	}
	cgroup->SplitMoney(split->copper, split->silver, split->gold, split->platinum);

	return;

}

void Client::Handle_OP_Surname(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Surname_Struct))
	{
		Log.Out(Logs::General, Logs::None, "Size mismatch in Surname expected %i got %i", sizeof(Surname_Struct), app->size);
		return;
	}

	if (!p_timers.Expired(&database, pTimerSurnameChange, false) && !GetGM())
	{
		Message(CC_Yellow, "You may only change surnames once every 7 days, your /surname is currently on cooldown.");
		return;
	}

	if (GetLevel() < 20)
	{
		Message_StringID(CC_Yellow, SURNAME_LEVEL);
		return;
	}

	Surname_Struct* surname = (Surname_Struct*)app->pBuffer;

	char *c = nullptr;
	bool first = true;
	for (c = surname->lastname; *c; c++)
	{
		if (first)
		{
			*c = toupper(*c);
			first = false;
		}
		else
		{
			*c = tolower(*c);
		}
	}

	if (strlen(surname->lastname) >= 20) {
		Message_StringID(CC_Yellow, SURNAME_TOO_LONG);
		return;
	}

	if (!database.CheckNameFilter(surname->lastname, true))
	{
		Message_StringID(CC_Yellow, SURNAME_REJECTED);
		return;
	}

	ChangeLastName(surname->lastname);
	p_timers.Start(pTimerSurnameChange, 604800);

	EQApplicationPacket* outapp = app->Copy();
	outapp = app->Copy();
	surname = (Surname_Struct*)outapp->pBuffer;
	surname->unknown0064 = 1;
	FastQueuePacket(&outapp);
	return;
}

void Client::Handle_OP_SwapSpell(const EQApplicationPacket *app)
{
	if (app->size != sizeof(SwapSpell_Struct)) {
		std::cout << "Wrong size on OP_SwapSpell. Got: " << app->size << ", Expected: " << sizeof(SwapSpell_Struct) << std::endl;
		return;
	}
	const SwapSpell_Struct* swapspell = (const SwapSpell_Struct*)app->pBuffer;
	int swapspelltemp;

	if (swapspell->from_slot < 0 || swapspell->from_slot > MAX_PP_SPELLBOOK || swapspell->to_slot < 0 || swapspell->to_slot > MAX_PP_SPELLBOOK)
		return;

	swapspelltemp = m_pp.spell_book[swapspell->from_slot];
	m_pp.spell_book[swapspell->from_slot] = m_pp.spell_book[swapspell->to_slot];
	m_pp.spell_book[swapspell->to_slot] = swapspelltemp;

	/* Save Spell Swaps */
	if (!database.SaveCharacterSpell(this->CharacterID(), m_pp.spell_book[swapspell->from_slot], swapspell->from_slot)){
		database.DeleteCharacterSpell(this->CharacterID(), m_pp.spell_book[swapspell->from_slot], swapspell->from_slot);
	}
	if (!database.SaveCharacterSpell(this->CharacterID(), swapspelltemp, swapspell->to_slot)){
		database.DeleteCharacterSpell(this->CharacterID(), swapspelltemp, swapspell->to_slot);
	}

	QueuePacket(app);
	return;
}

void Client::Handle_OP_TargetCommand(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ClientTarget_Struct)) {
		Log.Out(Logs::General, Logs::Error, "OP size error: OP_TargetMouse expected:%i got:%i", sizeof(ClientTarget_Struct), app->size);
		return;
	}
	if (IsAIControlled() && !has_zomm)
		return;

	ClientTarget_Struct* ct = (ClientTarget_Struct*)app->pBuffer;
	pClientSideTarget = ct->new_target;

	bool tar_cmd = (app->GetOpcode() == OP_TargetCommand);

	// cache current target
	Mob *cur_tar = GetTarget();

	//Message(0, "Target: %d OP_TargetCommand: %s", ct->new_target, tar_cmd ? "true" : "false");
	if (ct->new_target == 0)
	{
		if (tar_cmd)
		{
			// searched for a target with /target (name), and not found.  Client sends over a new_target == 0
			// dont do anything else, client keeps track of own current target.
			Message_StringID(CC_Default, TARGET_NOT_FOUND2);
		} 
		else
		{
			// cleared target
			if (cur_tar) {
				cur_tar->IsTargeted(-1);
				last_target = cur_tar->GetID();
			}
			SetTarget(nullptr);
			QueuePacket(app);
		}
		return;
	}

	bool send_tar = false;

	// Locate and cache new target
	Mob *new_tar = entity_list.GetMob(ct->new_target);

	// For /target, send reject or success packet
	if (tar_cmd) {
		if (new_tar) {
			if (!new_tar->CastToMob()->IsInvisible(this) && (DistanceSquared(m_Position, new_tar->GetPosition())  <= TARGETING_RANGE*TARGETING_RANGE || GetGM())) {
				if (new_tar->GetBodyType() == BT_NoTarget2 || new_tar->GetBodyType() == BT_Special
					|| new_tar->GetBodyType() == BT_NoTarget)
				{
					//Targeting something we shouldn't with /target
					//but the client allows this without MQ so you don't flag it
					if (!cur_tar) 
					{
						SendTargetCommand(0);
						SetTarget(nullptr);
						pClientSideTarget = 0;
						last_target = 0;
					}
					return;
				}
				// have a valid target
				// send target packet later if pass validation checks
				send_tar = true;
			}
			else
			{
				Message_StringID(CC_Red,TARGET_NOT_FOUND2);
				SendTargetCommand(cur_tar ? cur_tar->GetID() : 0);
				if (!cur_tar) {
					pClientSideTarget = 0;
					SetTarget(nullptr);
				}
				return;
			}
		} else {
			// no target found
			Message_StringID(CC_Red,TARGET_NOT_FOUND2);
			SendTargetCommand(cur_tar ? cur_tar->GetID() : 0);
			if (!cur_tar) {
				SetTarget(nullptr);
				pClientSideTarget = 0;
			}
			return;
		}
	}

	// validate target
	if (new_tar)
	{
		if (GetGM())
		{
			// always allow GM's to target
		} 
		else if (new_tar->GetBodyType() == BT_NoTarget2 || new_tar->GetBodyType() == BT_Special
			|| new_tar->GetBodyType() == BT_NoTarget)
		{
			char *hacker_str = nullptr;
			MakeAnyLenString(&hacker_str, "%s attempting to target something untargetable, %s bodytype: %i\n",
				GetName(), new_tar->GetName(), (int)new_tar->GetBodyType());
			database.SetMQDetectionFlag(AccountName(), GetName(), hacker_str, zone->GetShortName());
			safe_delete_array(hacker_str);
			if (cur_tar)
			{
				SendTargetCommand(cur_tar->GetID());
				SetTarget(cur_tar);
			} else {
				SendTargetCommand(0);
				SetTarget(nullptr);
			}
			return;
		}
		else if (IsAssistExempted())
		{
			SetAssistExemption(false);
		}
		else if (new_tar->IsClient())
		{
			if (new_tar != cur_tar && new_tar != this)
			{
				if (new_tar)
				{
					EQApplicationPacket hp_app;
					new_tar->CreateHPPacket(&hp_app);
					QueuePacket(&hp_app);
				}
			}
		}
		else if (IsPortExempted())
		{

		}
		else if (IsSenseExempted())
		{
			SetSenseExemption(false);
		}
		else if (GetBindSightTarget())
		{
			if (DistanceSquared(GetBindSightTarget()->GetPosition(), new_tar->GetPosition()) > (zone->newzone_data.maxclip*zone->newzone_data.maxclip))
			{
				if (DistanceSquared(m_Position, new_tar->GetPosition()) > (zone->newzone_data.maxclip*zone->newzone_data.maxclip + 40000))
				{
					char *hacker_str = nullptr;
					MakeAnyLenString(&hacker_str, "%s attempting to target something beyond the clip plane of %.2f units,"
						" from (%.2f, %.2f, %.2f) to %s (%.2f, %.2f, %.2f)", GetName(),
						(zone->newzone_data.maxclip*zone->newzone_data.maxclip),
						GetX(), GetY(), GetZ(), new_tar->GetName(), new_tar->GetX(), new_tar->GetY(), new_tar->GetZ());
					database.SetMQDetectionFlag(AccountName(), GetName(), hacker_str, zone->GetShortName());
					safe_delete_array(hacker_str);
					SendTargetCommand(0);
					SetTarget(nullptr);
					return;
				}
			}
		}
		else if (DistanceSquared(m_Position, new_tar->GetPosition()) > (zone->newzone_data.maxclip*zone->newzone_data.maxclip + 40000))
		{ // client will allow targeting something just beyond max clip just out of sight, so add another 200 for that.
			char *hacker_str = nullptr;
			MakeAnyLenString(&hacker_str, "%s attempting to target something beyond the clip plane of %.2f units,"
				" from (%.2f, %.2f, %.2f) to %s (%.2f, %.2f, %.2f)", GetName(),
				(zone->newzone_data.maxclip*zone->newzone_data.maxclip),
				GetX(), GetY(), GetZ(), new_tar->GetName(), new_tar->GetX(), new_tar->GetY(), new_tar->GetZ());
			database.SetMQDetectionFlag(AccountName(), GetName(), hacker_str, zone->GetShortName());
			safe_delete_array(hacker_str);
		}
		SetTarget(new_tar);
		if (cur_tar) {
			if(cur_tar != new_tar) {
				cur_tar->IsTargeted(-1);
				new_tar->IsTargeted(1);
			}
		} else {
			new_tar->IsTargeted(1);
		}
		if (send_tar)
			QueuePacket(app);
	} else {
		SendTargetCommand(0);
		SetTarget(nullptr);
	}
	return;
}

void Client::Handle_OP_TargetMouse(const EQApplicationPacket *app)
{
	Handle_OP_TargetCommand(app);
}

void Client::Handle_OP_Taunt(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(ClientTarget_Struct)) {
		std::cout << "Wrong size on OP_Taunt. Got: " << app->size << ", Expected: " << sizeof(ClientTarget_Struct) << std::endl;
		return;
	}

	if (!p_timers.Expired(&database, pTimerTaunt, false)) {
		Message(CC_Red, "Ability recovery time not yet met.");
		return;
	}
	p_timers.Start(pTimerTaunt, TauntReuseTime - 1);

	if (GetTarget() == nullptr || !GetTarget()->IsNPC())
		return;

	Taunt(GetTarget()->CastToNPC(), false);
	return;
}

void Client::Handle_OP_TGB(const EQApplicationPacket *app) 
{
	OPTGB(app);
	return;
}

void Client::Handle_OP_Track(const EQApplicationPacket *app)
{
	if (GetClass() != RANGER && GetClass() != DRUID && GetClass() != BARD)
	{
		Kick(); //The client handles tracking for us, simply returning is not enough if they are cheating.
		return;
	}

	if (GetSkill(SkillTracking) == 0)
		SetSkill(SkillTracking, 1);
	else
		CheckIncreaseSkill(SkillTracking, nullptr, zone->skill_difficulty[SkillTracking].difficulty);

	return;
}

void Client::Handle_OP_TradeAcceptClick(const EQApplicationPacket *app)
{
	Mob* with = trade->With();
	trade->state = TradeAccepted;

	if (with && with->IsClient()) {
		//finish trade...
		// Have both accepted?
		Client* other = with->CastToClient();
		other->QueuePacket(app);

		if (other->trade->state == trade->state) {
			other->trade->state = TradeCompleting;
			trade->state = TradeCompleting;

			// should we do this for NoDrop items as well?
			if (CheckTradeLoreConflict(other) || other->CheckTradeLoreConflict(this)) {
				Message_StringID(CC_Red, TRADE_CANCEL_LORE);
				other->Message_StringID(CC_Red, TRADE_CANCEL_LORE);
				this->FinishTrade(this);
				other->FinishTrade(other);
				other->trade->Reset();
				trade->Reset();
			}
			else {
				// Audit trade to database for both trade streams
				other->trade->LogTrade();
				trade->LogTrade();

				// start QS code
				if (RuleB(QueryServ, PlayerLogTrades)) {
					QSPlayerLogTrade_Struct event_entry;
					std::list<void*> event_details;

					memset(&event_entry, 0, sizeof(QSPlayerLogTrade_Struct));

					// Perform actual trade
					this->FinishTrade(other, true, &event_entry, &event_details);
					other->FinishTrade(this, false, &event_entry, &event_details);

					event_entry._detail_count = event_details.size();

					ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLogTrades, sizeof(QSPlayerLogTrade_Struct) + (sizeof(QSTradeItems_Struct)* event_entry._detail_count));
					QSPlayerLogTrade_Struct* QS = (QSPlayerLogTrade_Struct*)pack->pBuffer;

					memcpy(QS, &event_entry, sizeof(QSPlayerLogTrade_Struct));

					int offset = 0;

					for (std::list<void*>::iterator iter = event_details.begin(); iter != event_details.end(); ++iter, ++offset) {
						QSTradeItems_Struct* detail = reinterpret_cast<QSTradeItems_Struct*>(*iter);
						QS->items[offset] = *detail;
						safe_delete(detail);
					}

					event_details.clear();

					pack->Deflate();

					if (worldserver.Connected())
						worldserver.SendPacket(pack);

					safe_delete(pack);
					// end QS code
				}
				else {
					this->FinishTrade(other);
					other->FinishTrade(this);
				}

				other->trade->Reset();
				trade->Reset();
			}
			// All done
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_FinishTrade, 0);
			other->QueuePacket(outapp);
			this->FastQueuePacket(&outapp);
		}
	}
	// Trading with a Mob object that is not a Client.
	else if (with) {

		if(with->IsNPC() && with->IsEngaged())
		{
			SendCancelTrade(with);
			Log.Out(Logs::General, Logs::Trading, "Cancelled in-progress trade due to %s being in combat.", with->GetCleanName());
			return;
		}

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_FinishTrade, 0);
		QueuePacket(outapp);
		safe_delete(outapp);

		outapp = new EQApplicationPacket(OP_TradeReset, 0);
		QueuePacket(outapp);
		safe_delete(outapp);

		if (with->IsNPC()) {
			// Audit trade to database for player trade stream
			if (RuleB(QueryServ, PlayerLogHandins)) {
				QSPlayerLogHandin_Struct event_entry;
				std::list<void*> event_details;

				memset(&event_entry, 0, sizeof(QSPlayerLogHandin_Struct));

				FinishTrade(with->CastToNPC(), false, &event_entry, &event_details);

				event_entry._detail_count = event_details.size();

				ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLogHandins, sizeof(QSPlayerLogHandin_Struct) + (sizeof(QSHandinItems_Struct)* event_entry._detail_count));
				QSPlayerLogHandin_Struct* QS = (QSPlayerLogHandin_Struct*)pack->pBuffer;

				memcpy(QS, &event_entry, sizeof(QSPlayerLogHandin_Struct));

				int offset = 0;

				for (std::list<void*>::iterator iter = event_details.begin(); iter != event_details.end(); ++iter, ++offset)
				{
					QSHandinItems_Struct* detail = reinterpret_cast<QSHandinItems_Struct*>(*iter);
					if (detail != nullptr)
					{
						QS->items[offset] = *detail;
						safe_delete(detail);
					}
				}

				event_details.clear();

				pack->Deflate();

				if (worldserver.Connected())
					worldserver.SendPacket(pack);

				safe_delete(pack);
			}
			else {
				FinishTrade(with->CastToNPC());
			}
		}
		trade->Reset();
	}


	return;
}

void Client::Handle_OP_Trader(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) {
		Trader_EndTrader();
		return;
	}

	if(zone->GetZoneID() != bazaar)
		return;

	uint32 max_items = 80;

	//Show Items
	if (app->size == sizeof(Trader_ShowItems_Struct))
	{
		Trader_ShowItems_Struct* sis = (Trader_ShowItems_Struct*)app->pBuffer;

		switch (sis->Code)
		{
			case BazaarTrader_EndTraderMode: {
				Trader_EndTrader();
				break;
			}
			case BazaarTrader_EndTransaction: {

				Client* c = entity_list.GetClientByID(sis->TraderID);
				if (c)
					c->WithCustomer(0);
				else
					Log.Out(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_Trader: Null Client Pointer");

				break;
			}
			case BazaarTrader_ShowItems: {
				Trader_ShowItems();
				break;
			}
			default: {
				Log.Out(Logs::Detail, Logs::Bazaar, "Unhandled action code in OP_Trader ShowItems_Struct");
				break;
			}
		}
	}
	else if (app->size == sizeof(Trader_Struct) || app->size == sizeof(TraderStatus_Struct))
	{
		Trader_Struct* ints = (Trader_Struct*)app->pBuffer;

		if (ints->Code == BazaarTrader_StartTraderMode && app->size == sizeof(Trader_Struct))
		{
			GetItems_Struct* gis = GetTraderItems();

			// Verify there are no NODROP or items with a zero price
			bool TradeItemsValid = true;

			/* In this loop, we will compare the items sent by the client in OP_Trader with the items the server has in the player's trader satchels.
			It is important we force the client to sync with the data the server has, because otherwise we will have no way to track stackable
			items as no serial or slot number is sent by the client. In a default setting, both client and server will check the satchel's items 
			in the same order. However, if items are moved around in the satchel and the player has kept the trader window up the client will tack 
			those items to the end of its check, while the server will continue to check them in the order they appear in the bags. Closing the 
			trader will sync server and client. */
			for (uint32 i = 0; i < max_items; i++) 
			{
				// We've reached the end of the server's item list.
				if (gis->Items[i] == 0) break;

				// Our client will not send an item if the cost has never been set by the player. If the item was previously set with a cost
				// but the player has since cleared it, the item will be sent with cost 1.

				// This will happen if an item with no cost is the last one the client checks. `Item` gets its value from the next 
				// offset in the packet which is usually garbage as our client does not memset.
				const Item_Struct *Item = database.GetItem(ints->Items[i]);
				if (!Item) 
				{
					Message(CC_Red, "Make sure all your items have a price, close your trader window, and start again.");
					TradeItemsValid = false;
					break;
				}

				// No drop item.
				if (Item->NoDrop == 0) 
				{
					Message(CC_Red, "NODROP Item in Trader Satchel. Unable to start trader mode.");
					TradeItemsValid = false;
					break;
				}

				// We should in theory never reach this logic, but keep it here just in case I am wrong.
				if (ints->ItemCost[i] == 0) 
				{
					Message(CC_Red, "Item in Trader Satchel with no price. Unable to start trader mode.");
					TradeItemsValid = false;
					break;
				}

				// This will happen if an item with no cost is in the middle of the client's check. The client will skip it
				// but server does not. 
				// It can also happen if the player moves items around in their satchel while their trader window is still up.
				// Finally if `Item` is a garbage value that just happens to be a valid itemid this will catch that. 
				// We either try to re-sync, or fail and send the player an error message.
				if(ints->Items[i] != gis->Items[i])
				{
					GetItem_Struct* gi = RefetchItem(ints->Items[i]);
					if(gi->Items > 0)
					{
						gis->Items[i] = gi->Items;
						gis->Charges[i] = gi->Charges;
						gis->SerialNumber[i] = gi->SerialNumber;
					}
					else
					{
						Message(CC_Red, "Make sure all your items have a price, close your trader window, and start again.");
						TradeItemsValid = false;
						break;
					}
				}

				Log.Out(Logs::Detail, Logs:: Bazaar, "(%d) Item: %s added to trader list with cost: %d and charges: %d", i, Item->Name, ints->ItemCost[i], gis->Charges[i]);
			}

			if (!TradeItemsValid) 
			{
				Trader_EndTrader();
				return;
			}

			for (uint32 i = 0; i < max_items; i++) 
			{
				if (database.GetItem(ints->Items[i])) 
				{
					database.SaveTraderItem(this->CharacterID(), ints->Items[i], gis->SerialNumber[i],
						gis->Charges[i], ints->ItemCost[i], i);
				}
				else 
				{
					break;
				}

			}
			safe_delete(gis);

			this->Trader_StartTrader();

		}
		else if (app->size == sizeof(TraderStatus_Struct))
		{
			TraderStatus_Struct* tss = (TraderStatus_Struct*)app->pBuffer;

			if (tss->Code == BazaarTrader_ShowItems)
			{
				Trader_ShowItems();
			}
			else if (tss->Code == BazaarTrader_EndTraderMode)
			{
				Trader_EndTrader();
			}
			else if (tss->Code == BazaarTrader_EndTransaction)
			{
				Client* c = entity_list.GetClientByID(tss->TraderID);
				if (c)
					c->WithCustomer(0);
				else
					Log.Out(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_Trader: Null Client Pointer");

				SendMerchantEnd();
			}
		}
		else {
			Log.Out(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_Trader: Unknown TraderStruct code of: %i\n",
				ints->Code);

			Log.Out(Logs::General, Logs::Error, "Unknown TraderStruct code of: %i\n", ints->Code);
		}
	}

	else if (app->size == sizeof(TraderPriceUpdate_Struct))
	{
		HandleTraderPriceUpdate(app);
	}
	else {
		Log.Out(Logs::Detail, Logs::Bazaar, "Unknown size for OP_Trader: %i\n", app->size);
		Log.Out(Logs::General, Logs::Error, "Unknown size for OP_Trader: %i\n", app->size);
		DumpPacket(app);
		return;
	}

	return;
}

void Client::Handle_OP_TraderBuy(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	// Bazaar Trader:
	//
	// Client has elected to buy an item from a Trader
	//

	if (app->size == sizeof(TraderBuy_Struct)){

		TraderBuy_Struct* tbs = (TraderBuy_Struct*)app->pBuffer;

		if (Client* Trader = entity_list.GetClientByID(tbs->TraderID)){

			BuyTraderItem(tbs, Trader, app);
		}
		else {
			Log.Out(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_TraderBuy: Null Client Pointer");
		}
	}
	else {
		Log.Out(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_TraderBuy: Struct size mismatch");

	}
	return;
}

void Client::Handle_OP_TradeRequest(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) {
		Handle_OP_CancelTrade(app);
		return;
	}

	if (app->size != sizeof(TradeRequest_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_TradeRequest, size=%i, expected %i", app->size, sizeof(TradeRequest_Struct));
		return;
	}
	// Client requesting a trade session from an npc/client
	// Trade session not started until OP_TradeRequestAck is sent

	CommonBreakInvisible();

	// Pass trade request on to recipient
	TradeRequest_Struct* msg = (TradeRequest_Struct*)app->pBuffer;
	Mob* tradee = entity_list.GetMob(msg->to_mob_id);

	if (tradee && tradee->IsClient()) 
	{
		tradee->CastToClient()->QueuePacket(app);
	}
	else if (tradee && tradee->IsNPC()) 
	{
		
		if(tradee->IsEngaged())
		{
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
			CancelTrade_Struct* ct = (CancelTrade_Struct*) outapp->pBuffer;
			ct->fromid = tradee->GetID();
			FastQueuePacket(&outapp);
			Log.Out(Logs::General, Logs::Trading, "Cancelled trade request due to %s being in combat.", tradee->GetCleanName());
			return;
		}

		//npcs always accept
		trade->Start(msg->to_mob_id);

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_TradeReset, 0);
		FastQueuePacket(&outapp);

		outapp = new EQApplicationPacket(OP_TradeRequestAck, sizeof(TradeRequest_Struct));
		TradeRequest_Struct* acc = (TradeRequest_Struct*)outapp->pBuffer;
		acc->from_mob_id = msg->to_mob_id;
		acc->to_mob_id = msg->from_mob_id;
		FastQueuePacket(&outapp);
	}
	return;
}

void Client::Handle_OP_TradeRequestAck(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) {
		Handle_OP_CancelTrade(app);
		return;
	}

	if (app->size != sizeof(TradeRequest_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_TradeRequestAck, size=%i, expected %i", app->size, sizeof(TradeRequest_Struct));
		return;
	}
	// Trade request recipient is acknowledging they are able to trade
	// After this, the trade session has officially started
	// Send ack on to trade initiator if client
	TradeRequest_Struct* msg = (TradeRequest_Struct*)app->pBuffer;
	Mob* tradee = entity_list.GetMob(msg->to_mob_id);

	if (tradee && tradee->IsClient()) {
		trade->Start(msg->to_mob_id);
		tradee->CastToClient()->QueuePacket(app);
	}
	return;
}

void Client::Handle_OP_TraderShop(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	// Bazaar Trader:
	//
	// This is when a potential purchaser right clicks on this client who is in Trader mode to
	// browse their goods.
	//

	TraderClick_Struct* tcs = (TraderClick_Struct*)app->pBuffer;

	if (app->size != sizeof(TraderClick_Struct)) {

		Log.Out(Logs::Detail, Logs::Error, "Client::Handle_OP_TraderShop: Returning due to struct size mismatch");

		return;
	}

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_TraderShop, sizeof(TraderClick_Struct));
	TraderClick_Struct* outtcs = (TraderClick_Struct*)outapp->pBuffer;
	Client* Trader = entity_list.GetClientByID(tcs->TraderID);

	if (Trader)
		outtcs->Approval = Trader->WithCustomer(GetID());
	else {
		Log.Out(Logs::Detail, Logs::Bazaar, "Client::Handle_OP_TraderShop: entity_list.GetClientByID(tcs->traderid)"
			" returned a nullptr pointer");
		return;
	}

	outtcs->TraderID = tcs->TraderID;
	outtcs->Unknown008 = 0x3f800000;

	QueuePacket(outapp);


	if (outtcs->Approval) {
		this->BulkSendTraderInventory(Trader->CharacterID());
		Trader->Trader_CustomerBrowsing(this);
	}
	else
		Message_StringID(clientMessageYellow, TRADER_BUSY);

	safe_delete(outapp);

	return;
}

void Client::Handle_OP_TradeSkillCombine(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(NewCombine_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for NewCombine_Struct: Expected: %i, Got: %i",
			sizeof(NewCombine_Struct), app->size);
		return;
	}
	/*if (m_tradeskill_object == nullptr) {
	Message(CC_Red, "Error: Server is not aware of the tradeskill container you are attempting to use");
	return;
	}*/

	//fixed this to work for non-world objects

	// Delegate to tradeskill object to perform combine
	NewCombine_Struct* in_combine = (NewCombine_Struct*)app->pBuffer;
	Object::HandleCombine(this, in_combine, m_tradeskill_object);

	return;
}

void Client::Handle_OP_Translocate(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;


	if (app->size != sizeof(Translocate_Struct)) {
		Log.Out(Logs::General, Logs::None, "Size mismatch in OP_Translocate expected %i got %i", sizeof(Translocate_Struct), app->size);
		DumpPacket(app);
		return;
	}
	Translocate_Struct *its = (Translocate_Struct*)app->pBuffer;

	if (!PendingTranslocate)
		return;

	if ((RuleI(Spells, TranslocateTimeLimit) > 0) && (time(nullptr) > (TranslocateTime + RuleI(Spells, TranslocateTimeLimit)))) {
		Message(CC_Red, "You did not accept the Translocate within the required time limit.");
		PendingTranslocate = false;
		return;
	}

	if (its->Complete == 1) {

		int SpellID = PendingTranslocateData.spell_id;
		int i = parse->EventSpell(EVENT_SPELL_EFFECT_TRANSLOCATE_COMPLETE, nullptr, this, SpellID, 0);

		if (i == 0)
		{
			// If the spell has a translocate to bind effect, AND we are already in the zone the client
			// is bound in, use the GoToBind method. If we send OP_Translocate in this case, the client moves itself
			// to the bind coords it has from the PlayerProfile, but with the X and Y reversed. I suspect they are
			// reversed in the pp, and since spells like Gate are handled serverside, this has not mattered before.
			if (((SpellID == 1422) || (SpellID == 1334) || (SpellID == 3243)) &&
				(zone->GetZoneID() == PendingTranslocateData.zone_id &&
				zone->GetInstanceID() == PendingTranslocateData.instance_id))
			{
				PendingTranslocate = false;
				GoToBind();
				return;
			}

			////Was sending the packet back to initiate client zone...
			////but that could be abusable, so lets go through proper channels
			MovePC(PendingTranslocateData.zone_id, PendingTranslocateData.instance_id,
				PendingTranslocateData.x, PendingTranslocateData.y,
				PendingTranslocateData.z, PendingTranslocateData.heading, 0, ZoneSolicited);
		}
	}

	PendingTranslocate = false;
}

void Client::Handle_OP_WearChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(WearChange_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size: OP_WearChange, size %i expected %i", app->size, sizeof(WearChange_Struct));
		return;
	}

	WearChange_Struct* wc = (WearChange_Struct*)app->pBuffer;
	if (wc->spawn_id != GetID())
		return;

	// we could maybe ignore this and just send our own from moveitem
	entity_list.QueueClients(this, app, true);
	return;
}

void Client::Handle_OP_WhoAllRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Who_All_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Wrong size on OP_WhoAll. Got: %i, Expected: %i", app->size, sizeof(Who_All_Struct));
		return;
	}
	Who_All_Struct* whoall = (Who_All_Struct*)app->pBuffer;

	WhoAll(whoall);

	return;
}

void Client::Handle_OP_YellForHelp(const EQApplicationPacket *app)
{
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_YellForHelp, 4);
	*(uint32 *)outapp->pBuffer = GetID();
	entity_list.QueueCloseClients(this, outapp, true, 100.0);
	safe_delete(outapp);
	return;
}

void Client::Handle_OP_ZoneEntryResend(const EQApplicationPacket *app)
{
	//EQMac doesn't reply to this according to ShowEQ captures.
	return;
}

void Client::Handle_OP_LFGCommand(const EQApplicationPacket *app)
{
	if (app->size != sizeof(LFG_Struct)) {
		Log.Out(Logs::General, Logs::Error, "Invalid size for LFG_Struct: Expected: %i, Got: %i", sizeof(LFG_Struct), app->size);
		return;
	}

	LFG_Struct* lfg = (LFG_Struct*) app->pBuffer;
	if(lfg->value == 1)
	{
		LFG = true;
	}
	else if(lfg->value == 0)
	{
		LFG = false;
	}
	else
	{
		Log.Out(Logs::Detail, Logs::Error, "Invalid LFG value sent. %i", lfg->value);
	}

	UpdateWho();

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_LFGCommand, sizeof(LFG_Appearance_Struct));
	LFG_Appearance_Struct* lfga = (LFG_Appearance_Struct*)outapp->pBuffer;
	lfga->entityid = GetID();
	lfga->value = (int8)LFG;

	entity_list.QueueClients(this, outapp, true);
	return;
}

void Client::Handle_OP_Disarm(const EQApplicationPacket *app) 
{
	if (Admin() >= RuleI(GM, NoCombatLow) && Admin() <= RuleI(GM, NoCombatHigh) && Admin() != 0) return;

	if (app->size != sizeof(Disarm_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for Disarm_Struct: Expected: %i, Got: %i", sizeof(Disarm_Struct), app->size);
		return;
	}

	Disarm_Struct* disin = (Disarm_Struct*)app->pBuffer;
	Mob* target = entity_list.GetMob(disin->target);

	if(!target)
		return;

	if(target != GetTarget())
		return;

	// It looks like the client does its own check, but let's double check.
	if(target->IsCorpse() || !IsAttackAllowed(target))
	{
		Message(CC_Red, "You cannot disarm this target.");
		return;
	}

	int8 level_diff = 0;
	uint16 skill = GetSkill(SkillDisarm);
	level_diff = GetLevel()-target->GetLevel();
	float disarmbase = (int16)skill/1.2f;
	float disarmchance = 0.0f;

	if (skill > 100)
		disarmchance = (int16)skill - 100.0f;

	disarmchance = disarmchance / 3.0f;

	float totaldisarm = disarmbase + disarmchance + level_diff;

	// We will always have at least a 5% chance of failure.
	if(totaldisarm >= 143)
		totaldisarm = 142.5f;

	if(totaldisarm <= 0)
		totaldisarm = 0.1f;

	bool success = 0;
	int8 disarm_result = 0;
	
	if(target->IsClient())
	{
		disarm_result = Disarm(target->CastToClient(), totaldisarm);
		if (disarm_result < 2)
			success = false;
		else
			success = true;
	}
	else if(target->IsNPC())
	{
		disarm_result = target->Disarm(totaldisarm);
		if (disarm_result < 2)
			success = false;
		else
			success = true;

		if(!GetGM())
			target->AddToHateList(this, 1);
	}

	if(disarm_result > 0)
	{
		float skillsuccess = disarm_result == 2 ? 1.0 : 2.0;
		CheckIncreaseSkill(SkillDisarm, target, zone->skill_difficulty[SkillDisarm].difficulty, skillsuccess);
	}

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_Disarm, sizeof(Disarm_Struct));
	Disarm_Struct* dis = (Disarm_Struct*)outapp->pBuffer;
	dis->entityid = disin->target;// These are reversed on the out packet.
	dis->target = disin->entityid;
	dis->skill = disin->skill;
	dis->status = success;

	QueuePacket(outapp);
	safe_delete(outapp);

	return;
}

void Client::Handle_OP_Feedback(const EQApplicationPacket *app)
{
	if (app->size != sizeof(Feedback_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for Feedback_Struct: Expected: %i, Got: %i", sizeof(Feedback_Struct), app->size);
		return;
	}

	Feedback_Struct* in = (Feedback_Struct*)app->pBuffer;
	database.UpdateFeedback(in);

	Message(CC_Yellow, "Thank you, %s. Your feedback has been recieved.", in->name);
	return;
}


void Client::Handle_OP_SoulMarkList(const EQApplicationPacket *app)
{
	if(Admin() < 80)
		return;

	if (app->size != sizeof(SoulMarkList_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for SoulMarkList_Struct: Expected: %i, Got: %i", sizeof(SoulMarkList_Struct), app->size);
		return;
	}
	SoulMarkList_Struct* in = (SoulMarkList_Struct*)app->pBuffer;

	uint32 charid = 0;
	if(strlen(in->interrogatename) > 0)
	{
		charid = database.GetCharacterID(in->interrogatename);
	}

	if(charid != 0) 
	{
		int max = database.RemoveSoulMark(charid);
		uint32 i = 0;
		for(i = 0; i < max; i++)
		{
			if(in->entries[i].type != 0)
			database.AddSoulMark(charid, in->entries[i].name, in->entries[i].accountname,  in->entries[i].gmname, in->entries[i].gmaccountname, in->entries[i].unix_timestamp, in->entries[i].type, in->entries[i].description);
		}
	}

	return;
}


void Client::Handle_OP_SoulMarkAdd(const EQApplicationPacket *app)
{
	if(Admin() < 80)
		return;

	if (app->size != sizeof(SoulMarkEntry_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for SoulMarkEntry_Struct: Expected: %i, Got: %i", sizeof(SoulMarkEntry_Struct), app->size);
		return;
	}
	SoulMarkEntry_Struct* in = (SoulMarkEntry_Struct*)app->pBuffer;

	uint32 charid = 0;
	if(strlen(in->name) > 0)
	{
		charid = database.GetCharacterID(in->name);
	}
	if(charid != 0)
	{
		database.AddSoulMark(charid, in->name, in->accountname, in->gmname, in->gmaccountname, std::time(nullptr), in->type, in->description);
	}

	return;
}

void Client::Handle_OP_SoulMarkUpdate(const EQApplicationPacket *app)
{
	if(Admin() < 80)
		return;

	if (app->size != sizeof(SoulMarkUpdate_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for SoulMarkUpdate_Struct: Expected: %i, Got: %i", sizeof(SoulMarkList_Struct), app->size);
		return;
	}
	SoulMarkUpdate_Struct* in = (SoulMarkUpdate_Struct*)app->pBuffer;

	ServerPacket* pack = new ServerPacket(ServerOP_Soulmark, sizeof(ServerRequestSoulMark_Struct));
	ServerRequestSoulMark_Struct* scs = (ServerRequestSoulMark_Struct*)pack->pBuffer;
	strcpy(scs->name, GetCleanName());
	strcpy(scs->entry.interrogatename, in->charname);
	worldserver.SendPacket(pack);
	safe_delete(pack);
	return;
}

void Client::Handle_OP_MBRetrievalRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MBRetrieveMessages_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for MBRetrieveMessages_Struct: Expected: %i, Got: %i", sizeof(MBRetrieveMessages_Struct), app->size);
		return;
	}
	MBRetrieveMessages_Struct* in = (MBRetrieveMessages_Struct*)app->pBuffer;	
	std::vector<MBMessageRetrievalGen_Struct> messageList;

	database.RetrieveMBMessages(in->category, messageList);

	for (int i=0; i<messageList.size();i++) {   

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_MBRetrievalResponse, sizeof(MBMessageRetrievalGen_Struct));             
		memcpy(outapp->pBuffer, &messageList[i], sizeof(MBMessageRetrievalGen_Struct));
		QueuePacket(outapp);
		safe_delete(outapp);
	}

	
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_MBRetrievalFin, 0);
	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_OP_MBRetrievalDetailRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MBModifyRequest_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for MBModifyRequest_Struct: Expected: %i, Got: %i", sizeof(MBModifyRequest_Struct), app->size);
		return;
	}
	MBModifyRequest_Struct* in = (MBModifyRequest_Struct*)app->pBuffer;	
	char messageText[2048] = "";	

	
	if(database.ViewMBMessage(in->id, messageText))
	{
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_MBRetrievalDetailResponse, strlen(messageText) + 1);             
		memcpy(outapp->pBuffer, messageText, strlen(messageText));
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	else
	{
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_MBRetrievalDetailResponse, 0);
		QueuePacket(outapp);
		safe_delete(outapp);
		
	}

	//No fin sent for this packet. Just data.

	return;
}



void Client::Handle_OP_MBRetrievalPostRequest(const EQApplicationPacket *app)
{
	if (app->size > sizeof(MBMessageRetrievalGen_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for MBRetrieveMessages_Struct: Expected: at most %i, Got: %i", sizeof(MBMessageRetrievalGen_Struct), app->size);
		return;
	}
	MBMessageRetrievalGen_Struct* in = (MBMessageRetrievalGen_Struct*)app->pBuffer;
	database.PostMBMessage(CharacterID(), GetCleanName(), in);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_MBRetrievalFin, 0);
	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_OP_MBRetrievalEraseRequest(const EQApplicationPacket *app)
{
	if (app->size != sizeof(MBEraseRequest_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for MBRetrieveMessages_Struct: Expected: %i, Got: %i", sizeof(MBEraseRequest_Struct), app->size);
		return;
	}
	MBEraseRequest_Struct* in = (MBEraseRequest_Struct*)app->pBuffer;	
	database.EraseMBMessage(in->id, CharacterID());
	
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_MBRetrievalFin, 0);
	FastQueuePacket(&outapp);

	return;
}

void Client::Handle_OP_Key(const EQApplicationPacket *app)
{
	if (app->size != 4) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for OP_Key: Expected: %i, Got: %i", 4, app->size);
		return;
	}

	ZoneFlagList(this);

	return;
}

void Client::Handle_OP_TradeRefused(const EQApplicationPacket *app)
{
	if (app->size != sizeof(RefuseTrade_Struct)) {
		Log.Out(Logs::Detail, Logs::Error, "Invalid size for OP_TradeRefused: Expected: %i, Got: %i", sizeof(RefuseTrade_Struct), app->size);
		return;
	}

	RefuseTrade_Struct* in = (RefuseTrade_Struct*)app->pBuffer;	
	Client* client = entity_list.GetClientByID(in->fromid);

	if(client)
	{
		CancelTrade_Struct* msg = (CancelTrade_Struct*)app->pBuffer;
		client->QueuePacket(app);
	}

	return;
}
