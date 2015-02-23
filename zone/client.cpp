/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.org)

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
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// for windows compile
#ifndef _WINDOWS
	#include <stdarg.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include "../common/unix.h"
#endif

extern volatile bool RunLoops;

#include "../common/eqemu_logsys.h"
#include "../common/features.h"
#include "../common/spdat.h"
#include "../common/guilds.h"
#include "../common/rulesys.h"
#include "../common/string_util.h"
#include "../common/data_verification.h"
#include "position.h"
#include "net.h"
#include "worldserver.h"
#include "zonedb.h"
#include "petitions.h"
#include "command.h"
#include "string_ids.h"

#include "guild_mgr.h"
#include "quest_parser_collection.h"
#include "../common/crc32.h"
#include "../common/packet_dump_file.h"
#include "queryserv.h"

extern QueryServ* QServ;
extern EntityList entity_list;
extern Zone* zone;
extern volatile bool ZoneLoaded;
extern WorldServer worldserver;
extern uint32 numclients;
extern PetitionList petition_list;
bool commandlogged;
char entirecommand[255];

Client::Client(EQStreamInterface* ieqs)
: Mob("No name",	// name
	"",	// lastname
	0,	// cur_hp
	0,	// max_hp
	0,	// gender
	0,	// race
	0,	// class
	BT_Humanoid,	// bodytype
	0,	// deity
	0,	// level
	0,	// npctypeid
	0,	// size
	RuleR(Character, BaseRunSpeed),	// runspeed
	glm::vec4(),
	0,	// light - verified for client innate_light value
	0xFF,	// texture
	0xFF,	// helmtexture
	0,	// ac
	0,	// atk
	0,	// str
	0,	// sta
	0,	// dex
	0,	// agi
	0,	// int
	0,	// wis
	0,	// cha
	0,	// Luclin Hair Colour
	0,	// Luclin Beard Color
	0,	// Luclin Eye1
	0,	// Luclin Eye2
	0,	// Luclin Hair Style
	0,	// Luclin Face
	0,	// Luclin Beard
	0,	// Armor Tint
	0xff,	// AA Title
	0,	// see_invis
	0,	// see_invis_undead
	0,
	0,
	0,
	0,
	0,	// qglobal
	0,	// maxlevel
	0	// scalerate

	),
	//these must be listed in the order they appear in client.h
	position_timer(250),
	hpupdate_timer(1800),
	camp_timer(29000),
	process_timer(100),
	stamina_timer(40000),
	zoneinpacket_timer(3000),
	linkdead_timer(RuleI(Zone,ClientLinkdeadMS)),
	dead_timer(2000),
	global_channel_timer(1000),
	shield_timer(500),
	fishing_timer(8000),
	endupkeep_timer(1000),
	forget_timer(0),
	autosave_timer(RuleI(Character, AutosaveIntervalS)*1000),
	scanarea_timer(AIClientScanarea_delay),
	proximity_timer(ClientProximity_interval),
	charm_update_timer(6000),
	rest_timer(1),
	charm_class_attacks_timer(3000),
	charm_cast_timer(3500),
	qglobal_purge_timer(30000),
	TrackingTimer(2000),
	ItemTickTimer(10000),
	ItemQuestTimer(500),
	anon_toggle_timer(250),
	afk_toggle_timer(250),
	helm_toggle_timer(250),
	light_update_timer(600),
	position_update_timer(10000),
	m_Proximity(FLT_MAX, FLT_MAX, FLT_MAX), //arbitrary large number
	m_ZoneSummonLocation(-2.0f,-2.0f,-2.0f),
	m_AutoAttackPosition(0.0f, 0.0f, 0.0f, 0.0f),
	m_AutoAttackTargetLocation(0.0f, 0.0f, 0.0f)
{
	for(int cf=0; cf < _FilterCount; cf++)
		ClientFilters[cf] = FilterShow;
	character_id = 0;
	conn_state = NoPacketsReceived;
	client_data_loaded = false;
	feigned = false;
	berserk = false;
	dead = false;
	eqs = ieqs;
	ip = eqs->GetRemoteIP();
	port = ntohs(eqs->GetRemotePort());
	client_state = CLIENT_CONNECTING;
	Trader=false;
	Buyer = false;
	CustomerID = 0;
	TrackingID = 0;
	WID = 0;
	account_id = 0;
	admin = 0;
	lsaccountid = 0;
	shield_target = nullptr;
	SQL_log = nullptr;
	guild_id = GUILD_NONE;
	guildrank = 0;
	memset(lskey, 0, sizeof(lskey));
	strcpy(account_name, "");
	tellsoff = false;
	last_reported_mana = 0;
	last_reported_endur = 0;
	gmhideme = false;
	AFK = false;
	LFG = false;
	gmspeed = 0;
	playeraction = 0;
	SetTarget(0);
	auto_attack = false;
	auto_fire = false;
	linkdead_timer.Disable();
	zonesummon_id = 0;
	zonesummon_ignorerestrictions = 0;
	zoning = false;
	zone_mode = ZoneUnsolicited;
	casting_spell_id = 0;
	npcflag = false;
	npclevel = 0;
	pQueuedSaveWorkID = 0;
	position_timer_counter = 0;
	fishing_timer.Disable();
	shield_timer.Disable();
	dead_timer.Disable();
	camp_timer.Disable();
	autosave_timer.Disable();
	instalog = false;
	pLastUpdate = 0;
	pLastUpdateWZ = 0;
	m_pp.autosplit = false;
	// initialise haste variable
	m_tradeskill_object = nullptr;
	delaytimer = false;
	PendingRezzXP = -1;
	PendingRezzDBID = 0;
	PendingRezzSpellID = 0;
	numclients++;
	// emuerror;
	UpdateWindowTitle();
	horseId = 0;
	tgb = false;
	keyring.clear();
	bind_sight_target = nullptr;
	logging_enabled = CLIENT_DEFAULT_LOGGING_ENABLED;

	//for good measure:
	memset(&m_pp, 0, sizeof(m_pp));
	memset(&m_epp, 0, sizeof(m_epp));
	PendingTranslocate = false;
	PendingSacrifice = false;
	BoatID = 0;

	KarmaUpdateTimer = new Timer(RuleI(Chat, KarmaUpdateIntervalMS));
	GlobalChatLimiterTimer = new Timer(RuleI(Chat, IntervalDurationMS));
	AttemptedMessages = 0;
	TotalKarma = 0;
	ClientVersion = EQClientUnknown;
	ClientVersionBit = 0;
	AggroCount = 0;
	RestRegenHP = 0;
	RestRegenMana = 0;
	RestRegenEndurance = 0;
	XPRate = 100;
	cur_end = 0;

	m_TimeSinceLastPositionCheck = 0;
	m_DistanceSinceLastPositionCheck = 0.0f;
	m_ShadowStepExemption = 0;
	m_KnockBackExemption = 0;
	m_PortExemption = 0;
	m_SenseExemption = 0;
	m_AssistExemption = 0;
	m_CheatDetectMoved = false;
	CanUseReport = true;
	aa_los_them_mob = nullptr;
	los_status = false;
	los_status_facing = false;
	qGlobals = nullptr;
	HideCorpseMode = HideCorpseNone;
	PendingGuildInvitation = false;

	cur_end = 0;

	InitializeBuffSlots();

	LoadAccountFlags();

	initial_respawn_selection = 0;

	walkspeed = RuleR(Character, BaseWalkSpeed);

	EngagedRaidTarget = false;
	SavedRaidRestTimer = 0;

	interrogateinv_flag = false;

	active_light = innate_light;
	spell_light = equip_light = NOT_USED;
	has_zomm = false;
	client_position_update = false;
}

Client::~Client() {
	Mob* horse = entity_list.GetMob(this->CastToClient()->GetHorseId());
	if (horse)
		horse->Depop();

	if(Trader)
		database.DeleteTraderItem(this->CharacterID());

	if(conn_state != ClientConnectFinished) {
		Log.Out(Logs::General, Logs::None, "Client '%s' was destroyed before reaching the connected state:", GetName());
		ReportConnectingState();
	}

	if(m_tradeskill_object != nullptr) {
		m_tradeskill_object->Close();
		m_tradeskill_object = nullptr;
	}

	if(IsDueling() && GetDuelTarget() != 0) {
		Entity* entity = entity_list.GetID(GetDuelTarget());
		if(entity != nullptr && entity->IsClient()) {
			entity->CastToClient()->SetDueling(false);
			entity->CastToClient()->SetDuelTarget(0);
			entity_list.DuelMessage(entity->CastToClient(),this,true);
		}
	}

	if (shield_target) {
		for (int y = 0; y < 2; y++) {
			if (shield_target->shielder[y].shielder_id == GetID()) {
				shield_target->shielder[y].shielder_id = 0;
				shield_target->shielder[y].shielder_bonus = 0;
			}
		}
		shield_target = nullptr;
	}

	if(GetTarget())
		GetTarget()->IsTargeted(-1);

	//if we are in a group and we are not zoning, force leave the group
	if(isgrouped && !zoning && ZoneLoaded)
		LeaveGroup();

	UpdateWho(2);

	// we save right now, because the client might be zoning and the world
	// will need this data right away
	Save(2); // This fails when database destructor is called first on shutdown

	safe_delete(KarmaUpdateTimer);
	safe_delete(GlobalChatLimiterTimer);
	safe_delete(qGlobals);

	numclients--;
	UpdateWindowTitle();
	if(zone)
	zone->RemoveAuth(GetName());

	//let the stream factory know were done with this stream
	eqs->Close();
	eqs->ReleaseFromUse();

	UninitializeBuffSlots();
}

void Client::SendLogoutPackets() {

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_CancelTrade, sizeof(CancelTrade_Struct));
	CancelTrade_Struct* ct = (CancelTrade_Struct*) outapp->pBuffer;
	ct->fromid = GetID();
	ct->action = groupActUpdate;
	FastQueuePacket(&outapp);
}

void Client::ReportConnectingState() {
	switch(conn_state) {
	case NoPacketsReceived:		//havent gotten anything
		Log.Out(Logs::General, Logs::None, "Client has not sent us an initial zone entry packet.");
		break;
	case ReceivedZoneEntry:		//got the first packet, loading up PP
		Log.Out(Logs::General, Logs::None, "Client sent initial zone packet, but we never got their player info from the database.");
		break;
	case PlayerProfileLoaded:	//our DB work is done, sending it
		Log.Out(Logs::General, Logs::None, "We were sending the player profile, spawns, time and weather, but never finished.");
		break;
	case ZoneInfoSent:		//includes PP, spawns, time and weather
		Log.Out(Logs::General, Logs::None, "We successfully sent player info and spawns, waiting for client to request new zone.");
		break;
	case NewZoneRequested:	//received and sent new zone request
		Log.Out(Logs::General, Logs::None, "We received client's new zone request, waiting for client spawn request.");
		break;
	case ClientSpawnRequested:	//client sent ReqClientSpawn
		Log.Out(Logs::General, Logs::None, "We received the client spawn request, and were sending objects, doors, zone points and some other stuff, but never finished.");
		break;
	case ZoneContentsSent:		//objects, doors, zone points
		Log.Out(Logs::General, Logs::None, "The rest of the zone contents were successfully sent, waiting for client ready notification.");
		break;
	case ClientReadyReceived:	//client told us its ready, send them a bunch of crap like guild MOTD, etc
		Log.Out(Logs::General, Logs::None, "We received client ready notification, but never finished Client::CompleteConnect");
		break;
	case ClientConnectFinished:	//client finally moved to finished state, were done here
		Log.Out(Logs::General, Logs::None, "Client is successfully connected.");
		break;
	};
}

bool Client::SaveAA(){
	int first_entry = 0;
	std::string rquery;
	/* Save Player AA */
	int spentpoints = 0;
	for (int a = 0; a < MAX_PP_AA_ARRAY; a++) {
		uint32 points = aa[a]->value;
		if (points > HIGHEST_AA_VALUE) {
			aa[a]->value = HIGHEST_AA_VALUE;
			points = HIGHEST_AA_VALUE;
		}
		if (points > 0) {
			SendAA_Struct* curAA = zone->FindAA(aa[a]->AA - aa[a]->value + 1);
			if (curAA) {
				for (int rank = 0; rank<points; rank++) {
					std::map<uint32, AALevelCost_Struct>::iterator RequiredLevel = AARequiredLevelAndCost.find(aa[a]->AA - aa[a]->value + 1 + rank);
					if (RequiredLevel != AARequiredLevelAndCost.end()) {
						spentpoints += RequiredLevel->second.Cost;
					}
					else
						spentpoints += (curAA->cost + (curAA->cost_inc * rank));
				}
			}
		}
	}
	m_pp.aapoints_spent = spentpoints + m_epp.expended_aa;
	for (int a = 0; a < MAX_PP_AA_ARRAY; a++) {
		if (aa[a]->AA > 0 && aa[a]->value){
			if (first_entry != 1){
				rquery = StringFormat("REPLACE INTO `character_alternate_abilities` (id, slot, aa_id, aa_value)"
					" VALUES (%u, %u, %u, %u)", character_id, a, aa[a]->AA, aa[a]->value);
				first_entry = 1;
			}
			rquery = rquery + StringFormat(", (%u, %u, %u, %u)", character_id, a, aa[a]->AA, aa[a]->value);
		}
	}
	auto results = database.QueryDatabase(rquery);
	return true;
}

bool Client::Save(uint8 iCommitNow) {
	if(!ClientDataLoaded())
		return false;

	/* Wrote current basics to PP for saves */
	m_pp.x = m_Position.x;
	m_pp.y = m_Position.y;
	m_pp.z = m_Position.z;
	m_pp.guildrank = guildrank;
	m_pp.heading = m_Position.w;

	/* Mana and HP */
	if (GetHP() <= 0) {
		m_pp.cur_hp = GetMaxHP();
	}
	else {
		m_pp.cur_hp = GetHP();
	}

	m_pp.mana = cur_mana;
	m_pp.endurance = cur_end;

	/* Save Character Currency */
	database.SaveCharacterCurrency(CharacterID(), &m_pp);

	/* Save Current Bind Points */
	auto regularBindPosition = glm::vec4(m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, 0.0f);
	auto homeBindPosition = glm::vec4(m_pp.binds[4].x, m_pp.binds[4].y, m_pp.binds[4].z, 0.0f);
	database.SaveCharacterBindPoint(CharacterID(), m_pp.binds[0].zoneId, m_pp.binds[0].instance_id, regularBindPosition, 0); /* Regular bind */
	database.SaveCharacterBindPoint(CharacterID(), m_pp.binds[4].zoneId, m_pp.binds[4].instance_id, homeBindPosition, 1); /* Home Bind */

	/* Save Character Buffs */
	database.SaveBuffs(this);

	/* Total Time Played */
	TotalSecondsPlayed += (time(nullptr) - m_pp.lastlogin);
	m_pp.timePlayedMin = (TotalSecondsPlayed / 60);
	m_pp.RestTimer = rest_timer.GetRemainingTime() / 1000;

	m_pp.lastlogin = time(nullptr);

	if (GetPet() && !GetPet()->IsFamiliar() && GetPet()->CastToNPC()->GetPetSpellID() && !dead) {
		NPC *pet = GetPet()->CastToNPC();
		m_petinfo.SpellID = pet->CastToNPC()->GetPetSpellID();
		m_petinfo.HP = pet->GetHP();
		m_petinfo.Mana = pet->GetMana();
		pet->GetPetState(m_petinfo.Buffs, m_petinfo.Items, m_petinfo.Name);
		m_petinfo.petpower = pet->GetPetPower();
		m_petinfo.size = pet->GetSize();
	} else {
		memset(&m_petinfo, 0, sizeof(struct PetInfo));
	}
	database.SavePetInfo(this);

	p_timers.Store(&database);

	m_pp.hunger_level = EQEmu::Clamp(m_pp.hunger_level, 0, 50000);
	m_pp.thirst_level = EQEmu::Clamp(m_pp.thirst_level, 0, 50000);
	database.SaveCharacterData(this->CharacterID(), this->AccountID(), &m_pp, &m_epp); /* Save Character Data */

	return true;
}

void Client::SaveBackup() {
}

CLIENTPACKET::CLIENTPACKET()
{
	app = nullptr;
	ack_req = false;
}

CLIENTPACKET::~CLIENTPACKET()
{
	safe_delete(app);
}

//this assumes we do not own pApp, and clones it.
bool Client::AddPacket(const EQApplicationPacket *pApp, bool bAckreq) {
	if (!pApp)
		return false;
	if(!zoneinpacket_timer.Enabled()) {
		//drop the packet because it will never get sent.
		return(false);
	}
	CLIENTPACKET *c = new CLIENTPACKET;

	c->ack_req = bAckreq;
	c->app = pApp->Copy();

	clientpackets.Append(c);
	return true;
}

//this assumes that it owns the object pointed to by *pApp
bool Client::AddPacket(EQApplicationPacket** pApp, bool bAckreq) {
	if (!pApp || !(*pApp))
		return false;
	if(!zoneinpacket_timer.Enabled()) {
		//drop the packet because it will never get sent.
		return(false);
	}
	CLIENTPACKET *c = new CLIENTPACKET;

	c->ack_req = bAckreq;
	c->app = *pApp;
	*pApp = 0;

	clientpackets.Append(c);
	return true;
}

bool Client::SendAllPackets() {
	LinkedListIterator<CLIENTPACKET*> iterator(clientpackets);

	CLIENTPACKET* cp = 0;
	iterator.Reset();
	while(iterator.MoreElements()) {
		cp = iterator.GetData();
		if(eqs)
			eqs->FastQueuePacket((EQApplicationPacket **)&cp->app, cp->ack_req);
		iterator.RemoveCurrent();
		Log.Out(Logs::Moderate, Logs::Client_Server_Packet, "Transmitting a packet");
	}
	return true;
}

void Client::QueuePacket(const EQApplicationPacket* app, bool ack_req, CLIENT_CONN_STATUS required_state, eqFilterType filter) {
	if(filter!=FilterNone){
		//this is incomplete... no support for FilterShowGroupOnly or FilterShowSelfOnly
		if(GetFilter(filter) == FilterHide)
			return; //Client has this filter on, no need to send packet
	}

	if(client_state == PREDISCONNECTED)
		return;

	if(client_state != CLIENT_CONNECTED && required_state == CLIENT_CONNECTED){
		// save packets during connection state
		AddPacket(app, ack_req);
		return;
	}

	//Wait for the queue to catch up - THEN send the first available predisconnected (zonechange) packet!
	if (client_state == ZONING && required_state != ZONING)
	{
		// save packets in case this fails
		AddPacket(app, ack_req);
		return;
	}

	// if the program doesnt care about the status or if the status isnt what we requested
	if (required_state != CLIENT_CONNECTINGALL && client_state != required_state)
	{
		// todo: save packets for later use
		AddPacket(app, ack_req);
	}
	else
		if(eqs)
			eqs->QueuePacket(app, ack_req);
}

void Client::FastQueuePacket(EQApplicationPacket** app, bool ack_req, CLIENT_CONN_STATUS required_state) {
	// if the program doesnt care about the status or if the status isnt what we requested


	if(client_state == PREDISCONNECTED)
	{
		if (app && (*app))
			delete *app;
		return;
	}

		//Wait for the queue to catch up - THEN send the first available predisconnected (zonechange) packet!
	if (client_state == ZONING && required_state != ZONING)
	{
		// save packets in case this fails
		AddPacket(app, ack_req);
		return;
	}

	if (required_state != CLIENT_CONNECTINGALL && client_state != required_state) {
		// todo: save packets for later use
		AddPacket(app, ack_req);
		return;
	}
	else {
		if(eqs)
			eqs->FastQueuePacket((EQApplicationPacket **)app, ack_req);
		else if (app && (*app))
			delete *app;
		*app = 0;
	}
	return;
}

void Client::ChannelMessageReceived(uint8 chan_num, uint8 language, uint8 lang_skill, const char* orig_message, const char* targetname)
{
	char message[4096];
	strn0cpy(message, orig_message, sizeof(message));

	Log.Out(Logs::Detail, Logs::Zone_Server, "Client::ChannelMessageReceived() Channel:%i message:'%s'", chan_num, message);

	if (targetname == nullptr) {
		targetname = (!GetTarget()) ? "" : GetTarget()->GetName();
	}

	if(RuleB(Chat, EnableAntiSpam))
	{
		if(strcmp(targetname, "discard") != 0)
		{
			if(chan_num == 3 || chan_num == 4 || chan_num == 5 || chan_num == 7)
			{
				if(GlobalChatLimiterTimer)
				{
					if(GlobalChatLimiterTimer->Check(false))
					{
						GlobalChatLimiterTimer->Start(RuleI(Chat, IntervalDurationMS));
						AttemptedMessages = 0;
					}
				}

				uint32 AllowedMessages = RuleI(Chat, MinimumMessagesPerInterval) + TotalKarma;
				AllowedMessages = AllowedMessages > RuleI(Chat, MaximumMessagesPerInterval) ? RuleI(Chat, MaximumMessagesPerInterval) : AllowedMessages;

				if(RuleI(Chat, MinStatusToBypassAntiSpam) <= Admin())
					AllowedMessages = 10000;

				AttemptedMessages++;
				if(AttemptedMessages > AllowedMessages)
				{
					if(AttemptedMessages > RuleI(Chat, MaxMessagesBeforeKick))
					{
						Kick();
						return;
					}
					if(GlobalChatLimiterTimer)
					{
						Message(0, "You have been rate limited, you can send more messages in %i seconds.",
							GlobalChatLimiterTimer->GetRemainingTime() / 1000);
						return;
					}
					else
					{
						Message(0, "You have been rate limited, you can send more messages in 60 seconds.");
						return;
					}
				}
			}
		}
	}

	/* Logs Player Chat */
	if (RuleB(QueryServ, PlayerLogChat)) {
		ServerPacket* pack = new ServerPacket(ServerOP_Speech, sizeof(Server_Speech_Struct) + strlen(message) + 1);
		Server_Speech_Struct* sem = (Server_Speech_Struct*) pack->pBuffer;

		if(chan_num == 0)
			sem->guilddbid = GuildID();
		else
			sem->guilddbid = 0;

		strcpy(sem->message, message);
		sem->minstatus = this->Admin();
		sem->type = chan_num;
		if(targetname != 0)
			strcpy(sem->to, targetname);

		if(GetName() != 0)
			strcpy(sem->from, GetName());

		pack->Deflate();
		if(worldserver.Connected())
			worldserver.SendPacket(pack);
		safe_delete(pack);
	}

	//Return true to proceed, false to return
	if(!mod_client_message(message, chan_num)) { return; }

	// Garble the message based on drunkness, except for OOC and GM
	if (m_pp.intoxication > 0 && chan_num != 5 && !GetGM()) {
		GarbleMessage(message, (int)(m_pp.intoxication / 3));
		language = 0; // No need for language when drunk
	}

	switch(chan_num)
	{
	case 0: { /* Guild Chat */
		if (!IsInAGuild())
			Message_StringID(MT_DefaultText, GUILD_NOT_MEMBER2);	//You are not a member of any guild.
		else if (!guild_mgr.CheckPermission(GuildID(), GuildRank(), GUILD_SPEAK))
			Message(0, "Error: You dont have permission to speak to the guild.");
		else if (!worldserver.SendChannelMessage(this, targetname, chan_num, GuildID(), language, message))
			Message(0, "Error: World server disconnected");
		break;
	}
	case 2: { /* Group Chat */
		Raid* raid = entity_list.GetRaidByClient(this);
		if(raid) {
			raid->RaidGroupSay((const char*) message, this);
			break;
		}

		Group* group = GetGroup();
		if(group != nullptr) {
			group->GroupMessage(this,language,lang_skill,(const char*) message);
		}
		break;
	}
	case 15: { /* Raid Say */
		Raid* raid = entity_list.GetRaidByClient(this);
		if(raid){
			raid->RaidSay((const char*) message, this);
		}
		break;
	}
	case 3: { /* Shout */
		Mob *sender = this;
		if (GetPet() && GetPet()->FindType(SE_VoiceGraft))
			sender = GetPet();

		entity_list.ChannelMessage(sender, chan_num, language, lang_skill, message);
		break;
	}
	case 4: { /* Auction */
		if(RuleB(Chat, ServerWideAuction))
		{
			if(!global_channel_timer.Check())
			{
				if(strlen(targetname) == 0)
					ChannelMessageReceived(5, language, lang_skill, message, "discard"); //Fast typer or spammer??
				else
					return;
			}

			if(GetRevoked())
			{
				Message(0, "You have been revoked. You may not talk on Auction.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, GlobalChatLevelLimit))
				{
					Message(0, "You do not have permission to talk in Auction at this time.");
					return;
				}
			}

			if (!worldserver.SendChannelMessage(this, 0, 4, 0, language, message))
				Message(0, "Error: World server disconnected");
		}
		else if(!RuleB(Chat, ServerWideAuction)) {
			Mob *sender = this;

			if (GetPet() && GetPet()->FindType(SE_VoiceGraft))
			sender = GetPet();

		entity_list.ChannelMessage(sender, chan_num, language, message);
		}
		break;
	}
	case 5: { /* OOC */
		if(RuleB(Chat, ServerWideOOC))
		{
			if(!global_channel_timer.Check())
			{
				if(strlen(targetname) == 0)
					ChannelMessageReceived(5, language, lang_skill, message, "discard"); //Fast typer or spammer??
				else
					return;
			}
			if(worldserver.IsOOCMuted() && admin < 100)
			{
				Message(0,"OOC has been muted. Try again later.");
				return;
			}

			if(GetRevoked())
			{
				Message(0, "You have been revoked. You may not talk on OOC.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, GlobalChatLevelLimit))
				{
					Message(0, "You do not have permission to talk in OOC at this time.");
					return;
				}
			}

			if (!worldserver.SendChannelMessage(this, 0, 5, 0, language, message))
				Message(0, "Error: World server disconnected");
		}
		else if(!RuleB(Chat, ServerWideOOC))
		{
			Mob *sender = this;

			if (GetPet() && GetPet()->FindType(SE_VoiceGraft))
				sender = GetPet();

			entity_list.ChannelMessage(sender, chan_num, language, message);
		}
		break;
	}
	case 6: /* Broadcast */
	case 11: { /* GM Say */
		if (!(admin >= 80))
			Message(0, "Error: Only GMs can use this channel");
		else if (!worldserver.SendChannelMessage(this, targetname, chan_num, 0, language, message))
			Message(0, "Error: World server disconnected");
		break;
	}
	case 7: { /* Tell */
			if(!global_channel_timer.Check())
			{
				if(strlen(targetname) == 0)
					ChannelMessageReceived(7, language, lang_skill, message, "discard"); //Fast typer or spammer??
				else
					return;
			}

			if(GetRevoked())
			{
				Message(0, "You have been revoked. You may not send tells.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, GlobalChatLevelLimit))
				{
					Message(0, "You do not have permission to send tells at this time.");
					return;
				}
			}

			char target_name[64];

			if(targetname)
			{
				size_t i = strlen(targetname);
				int x;
				for(x = 0; x < i; ++x)
				{
					if(targetname[x] == '%')
					{
						target_name[x] = '/';
					}
					else
					{
						target_name[x] = targetname[x];
					}
				}
				target_name[x] = '\0';
			}

			if(!worldserver.SendChannelMessage(this, target_name, chan_num, 0, language, message))
				Message(0, "Error: World server disconnected");
		break;
	}
	case 8: { /* Say */
		if(message[0] == COMMAND_CHAR) {
			if(command_dispatch(this, message) == -2) {
				if(parse->PlayerHasQuestSub(EVENT_COMMAND)) {
					int i = parse->EventPlayer(EVENT_COMMAND, this, message, 0);
					if(i == 0 && !RuleB(Chat, SuppressCommandErrors)) {
						Message(CC_Red, "Command '%s' not recognized.", message);
					}
				} else {
					if(!RuleB(Chat, SuppressCommandErrors))
						Message(CC_Red, "Command '%s' not recognized.", message);
				}
			}
			break;
		}

		Mob* sender = this;
		if (GetPet() && GetPet()->FindType(SE_VoiceGraft))
			sender = GetPet();

		entity_list.ChannelMessage(sender, chan_num, language, lang_skill, message);
		parse->EventPlayer(EVENT_SAY, this, message, language);

		if (sender != this)
			break;

		if(quest_manager.ProximitySayInUse())
			entity_list.ProcessProximitySay(message, this, language);

		if (GetTarget() != 0 && GetTarget()->IsNPC()) {
			if(!GetTarget()->CastToNPC()->IsEngaged()) {
				CheckEmoteHail(GetTarget(), message);


				if(DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition()) <= 200 && !IsInvisible(this)) {
					NPC *tar = GetTarget()->CastToNPC();
					parse->EventNPC(EVENT_SAY, tar->CastToNPC(), this, message, language);
				}
			}
			else {
				if (DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition()) <= 200) {
					parse->EventNPC(EVENT_AGGRO_SAY, GetTarget()->CastToNPC(), this, message, language);
				}
			}

		}
		break;
	}
	case 20:
	{
		// UCS Relay for Underfoot and later.
		if(!worldserver.SendChannelMessage(this, 0, chan_num, 0, language, message))
			Message(0, "Error: World server disconnected");
		break;
	}
	case 22:
	{
		// Emotes for Underfoot and later.
		// crash protection -- cheater
		message[1023] = '\0';
		size_t msg_len = strlen(message);
		if (msg_len > 512)
			message[512] = '\0';

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Emote, 4 + msg_len + strlen(GetName()) + 2);
		Emote_Struct* es = (Emote_Struct*)outapp->pBuffer;
		char *Buffer = (char *)es;
		Buffer += 4;
		snprintf(Buffer, sizeof(Emote_Struct) - 4, "%s %s", GetName(), message);
		entity_list.QueueCloseClients(this, outapp, true, 100, 0, true, FilterSocials);
		safe_delete(outapp);
		break;
	}
	default: {
		Message(0, "Channel (%i) not implemented", (uint16)chan_num);
	}
	}
}

// if no language skill is specified, call the function with a skill of 100.
void Client::ChannelMessageSend(const char* from, const char* to, uint8 chan_num, uint8 language, const char* message, ...) {
	ChannelMessageSend(from, to, chan_num, language, 100, message);
}

void Client::ChannelMessageSend(const char* from, const char* to, uint8 chan_num, uint8 language, uint8 lang_skill, const char* message, ...) {
	if ((chan_num==11 && !(this->GetGM())) || (chan_num==10 && this->Admin()<80)) // dont need to send /pr & /petition to everybody
		return;
	va_list argptr;
	char buffer[4096];
	char message_sender[64];

	memcpy(buffer, message, 4096);

	EQApplicationPacket app(OP_ChannelMessage, sizeof(ChannelMessage_Struct)+strlen(buffer)+1);
	ChannelMessage_Struct* cm = (ChannelMessage_Struct*)app.pBuffer;

	if (from == 0)
		strcpy(cm->sender, "ZServer");
	else if (from[0] == 0)
		strcpy(cm->sender, "ZServer");
	else {
		CleanMobName(from, message_sender);
		strcpy(cm->sender, message_sender);
	}
	if (to != 0)
		strcpy((char *) cm->targetname, to);
	else if (chan_num == 7)
		strcpy(cm->targetname, m_pp.name);
	else
		cm->targetname[0] = 0;

	uint8 ListenerSkill;

	if (language < MAX_PP_LANGUAGE) {
		ListenerSkill = m_pp.languages[language];
		if (ListenerSkill == 0) {
			cm->language = (MAX_PP_LANGUAGE - 1); // in an unknown tongue
		}
		else {
			cm->language = language;
		}
	}
	else {
		ListenerSkill = m_pp.languages[0];
		cm->language = 0;
	}

	// set effective language skill = lower of sender and receiver skills
	int32 EffSkill = (lang_skill < ListenerSkill ? lang_skill : ListenerSkill);
	if (EffSkill > 100)	// maximum language skill is 100
		EffSkill = 100;
	cm->skill_in_language = EffSkill;

	// Garble the message based on listener skill
	if (ListenerSkill < 100) {
		GarbleMessage(buffer, (100 - ListenerSkill));
	}

	cm->chan_num = chan_num;
	strcpy(&cm->message[0], buffer);
	QueuePacket(&app);

	if ((chan_num == 2) && (ListenerSkill < 100)) {	// group message in unmastered language, check for skill up
		if ((m_pp.languages[language] <= lang_skill) && (from != this->GetName()))
			CheckLanguageSkillIncrease(language, lang_skill);
	}
}

void Client::Message(uint32 type, const char* message, ...) {
	if (GetFilter(FilterMeleeCrits) == FilterHide && type == MT_CritMelee) //98 is self...
		return;
	if (GetFilter(FilterSpellCrits) == FilterHide && type == MT_SpellCrits)
		return;

		va_list argptr;
		char *buffer = new char[4096];
		va_start(argptr, message);
		vsnprintf(buffer, 4096, message, argptr);
		va_end(argptr);

		size_t len = strlen(buffer);

		//client dosent like our packet all the time unless
		//we make it really big, then it seems to not care that
		//our header is malformed.
		//len = 4096 - sizeof(SpecialMesg_Struct);

		uint32 len_packet = sizeof(SpecialMesg_Struct)+len;
		EQApplicationPacket* app = new EQApplicationPacket(OP_SpecialMesg, len_packet);
		SpecialMesg_Struct* sm=(SpecialMesg_Struct*)app->pBuffer;
		sm->header[0] = 0x00; // Header used for #emote style messages..
		sm->header[1] = 0x00; // Play around with these to see other types
		sm->header[2] = 0x00;
		sm->msg_type = type;
		memcpy(sm->message, buffer, len+1);

		FastQueuePacket(&app);

		safe_delete_array(buffer);

}

void Client::QuestJournalledMessage(const char *npcname, const char* message) {

	// I assume there is an upper safe limit on the message length. Don't know what it is, but 4000 doesn't crash
	// the client.
	const int MaxMessageLength = 4000;
	char OutMessage[MaxMessageLength+1];

	// Apparently Visual C++ snprintf is not C99 compliant and doesn't put the null terminator
	// in if the formatted string >= the maximum length, so we put it in.
	//
	snprintf(OutMessage, MaxMessageLength, "%s", message); OutMessage[MaxMessageLength]='\0';

	//EQMac SpecialMesg works for Message but not here (end result of quest say) No clue why, but workaround for now.
		uint32 len_packet = sizeof(OldSpecialMesg_Struct) + strlen(OutMessage) + 1;
		EQApplicationPacket* app = new EQApplicationPacket(OP_OldSpecialMesg, len_packet);
		OldSpecialMesg_Struct* sm=(OldSpecialMesg_Struct*)app->pBuffer;

		sm->msg_type = 0x0a;
		memcpy(sm->message, OutMessage, strlen(OutMessage) + 1);

		QueuePacket(app);
		safe_delete(app);
}

void Client::SetMaxHP() {
	if(dead)
		return;
	SetHP(CalcMaxHP());
	SendHPUpdate();
	Save();
}

void Client::SetSkill(SkillUseTypes skillid, uint16 value) {
	if (skillid > HIGHEST_SKILL)
		return;
	m_pp.skills[skillid] = value; // We need to be able to #setskill 254 and 255 to reset skills

	database.SaveCharacterSkill(this->CharacterID(), skillid, value);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SkillUpdate, sizeof(SkillUpdate_Struct));
	SkillUpdate_Struct* skill = (SkillUpdate_Struct*)outapp->pBuffer;
	skill->skillId=skillid;
	skill->value=value;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::IncreaseLanguageSkill(int skill_id, int value) {

	if (skill_id >= MAX_PP_LANGUAGE)
		return; //Invalid lang id

	m_pp.languages[skill_id] += value;

	if (m_pp.languages[skill_id] > 100) //Lang skill above max
		m_pp.languages[skill_id] = 100;

	database.SaveCharacterLanguage(this->CharacterID(), skill_id, m_pp.languages[skill_id]);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SkillUpdate, sizeof(SkillUpdate_Struct));
	SkillUpdate_Struct* skill = (SkillUpdate_Struct*)outapp->pBuffer;
	skill->skillId = 100 + skill_id;
	skill->value = m_pp.languages[skill_id];
	QueuePacket(outapp);
	safe_delete(outapp);

	Message_StringID( MT_Skills, LANG_SKILL_IMPROVED ); //Notify client
}

void Client::AddSkill(SkillUseTypes skillid, uint16 value) {
	if (skillid > HIGHEST_SKILL)
		return;
	value = GetRawSkill(skillid) + value;
	uint16 max = GetMaxSkillAfterSpecializationRules(skillid, MaxSkill(skillid));
	if (value > max)
		value = max;
	SetSkill(skillid, value);
}

void Client::SendSound(){//Makes a sound. EQMac is 48 bytes.
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Sound, sizeof(QuestReward_Struct));
	unsigned char x[68];
	memset(x, 0, 68);
	x[0]=0x22;
	memset(&x[4],0x8002,sizeof(uint16));
	memset(&x[8],0x8624,sizeof(uint16));
	memset(&x[12],0x4A01,sizeof(uint16));
	x[16]=0x00; //copper 
	x[20]=0x00; //silver
	x[24]=0x00; //gold
	x[28]=0x00; //plat
	x[32]=0x00; //item int16
	memset(&x[40],0xFFFFFFFF,sizeof(uint32));
	memset(&x[44],0xFFFFFFFF,sizeof(uint32));
	memset(&x[48],0xFFFFFFFF,sizeof(uint32)); //EQMac client ends here.
	memset(&x[52],0xFFFFFFFF,sizeof(uint32));
	memset(&x[56],0xFFFFFFFF,sizeof(uint32));
	memset(&x[60],0xFFFFFFFF,sizeof(uint32));
	memset(&x[64],0xffffffff,sizeof(uint32));
	memcpy(outapp->pBuffer,x,outapp->size);
	QueuePacket(outapp);
	safe_delete(outapp);

}
void Client::UpdateWho(uint8 remove) {
	if (account_id == 0)
		return;
	if (!worldserver.Connected())
		return;
	ServerPacket* pack = new ServerPacket(ServerOP_ClientList, sizeof(ServerClientList_Struct));
	ServerClientList_Struct* scl = (ServerClientList_Struct*) pack->pBuffer;
	scl->remove = remove;
	scl->wid = this->GetWID();
	scl->IP = this->GetIP();
	scl->charid = this->CharacterID();
	strcpy(scl->name, this->GetName());

	scl->gm = GetGM();
	scl->Admin = this->Admin();
	scl->AccountID = this->AccountID();
	strcpy(scl->AccountName, this->AccountName());
	scl->LSAccountID = this->LSAccountID();
	strn0cpy(scl->lskey, lskey, sizeof(scl->lskey));
	scl->zone = zone->GetZoneID();
	scl->instance_id = zone->GetInstanceID();
	scl->race = this->GetRace();
	scl->class_ = GetClass();
	scl->level = GetLevel();
	if (m_pp.anon == 0)
		scl->anon = 0;
	else if (m_pp.anon == 1)
		scl->anon = 1;
	else if (m_pp.anon >= 2)
		scl->anon = 2;

	scl->ClientVersion = GetClientVersion();
	scl->tellsoff = tellsoff;
	scl->guild_id = guild_id;
	scl->LFG = this->LFG;
	scl->LD = this->IsLD();

	worldserver.SendPacket(pack);
	safe_delete(pack);
}

void Client::WhoAll(Who_All_Struct* whom) {

	if (!worldserver.Connected())
		Message(0, "Error: World server disconnected");
	else {
		ServerPacket* pack = new ServerPacket(ServerOP_Who, sizeof(ServerWhoAll_Struct));
		ServerWhoAll_Struct* whoall = (ServerWhoAll_Struct*) pack->pBuffer;
		whoall->admin = this->Admin();
		whoall->fromid=this->GetID();
		strcpy(whoall->from, this->GetName());
		strn0cpy(whoall->whom, whom->whom, 64);
		whoall->lvllow = whom->lvllow;
		whoall->lvlhigh = whom->lvlhigh;
		whoall->gmlookup = whom->gmlookup;
		whoall->wclass = whom->wclass;
		whoall->wrace = whom->wrace;
		whoall->guildid = whom->guildid;
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Client::FriendsWho(char *FriendsString) {

	if (!worldserver.Connected())
		Message(0, "Error: World server disconnected");
	else {
		ServerPacket* pack = new ServerPacket(ServerOP_FriendsWho, sizeof(ServerFriendsWho_Struct) + strlen(FriendsString));
		ServerFriendsWho_Struct* FriendsWho = (ServerFriendsWho_Struct*) pack->pBuffer;
		FriendsWho->FromID = this->GetID();
		strcpy(FriendsWho->FromName, GetName());
		strcpy(FriendsWho->FriendsString, FriendsString);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}


void Client::UpdateAdmin(bool iFromDB) {
	int16 tmp = admin;
	if (iFromDB)
		admin = database.CheckStatus(account_id);
	if (tmp == admin && iFromDB)
		return;

	if(m_pp.gm)
	{
		Log.Out(Logs::Moderate, Logs::Zone_Server, "%s - %s is a GM", __FUNCTION__ , GetName());
// no need for this, having it set in pp you already start as gm
// and it's also set in your spawn packet so other people see it too
//		SendAppearancePacket(AT_GM, 1, false);
		petition_list.UpdateGMQueue();
	}

	UpdateWho();
}

const int32& Client::SetMana(int32 amount) {
	bool update = false;
	if (amount < 0)
		amount = 0;
	if (amount > GetMaxMana())
		amount = GetMaxMana();
	if (amount != cur_mana)
		update = true;
	cur_mana = amount;
	if (update)
		Mob::SetMana(amount);
	SendManaUpdatePacket();
	return cur_mana;
}

void Client::SendManaUpdatePacket() {
	if (!Connected() || IsCasting())
		return;

	if (last_reported_mana != cur_mana || last_reported_endur != cur_end) {



		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ManaChange, sizeof(ManaChange_Struct));
		ManaChange_Struct* manachange = (ManaChange_Struct*)outapp->pBuffer;
		manachange->new_mana = cur_mana;
		manachange->stamina = cur_end;
		manachange->spell_id = casting_spell_id;	//always going to be 0... since we check IsCasting()
		outapp->priority = 6;
		QueuePacket(outapp);
		safe_delete(outapp);

		last_reported_mana = cur_mana;
		last_reported_endur = cur_end;
	}
}

// sends mana update to self
void Client::SendManaUpdate()
{
	EQApplicationPacket* mana_app = new EQApplicationPacket(OP_ManaUpdate,sizeof(ManaUpdate_Struct));
	ManaUpdate_Struct* mus = (ManaUpdate_Struct*)mana_app->pBuffer;
	mus->cur_mana = GetMana();
	mus->max_mana = GetMaxMana();
	mus->spawn_id = GetID();
	QueuePacket(mana_app);
	safe_delete(mana_app);
}

void Client::SendStaminaUpdate()
{
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	int value = RuleI(Character,ConsumptionValue);
	sta->food = m_pp.hunger_level > value ? value : m_pp.hunger_level;
	sta->water = m_pp.thirst_level> value ? value : m_pp.thirst_level;
	sta->fatigue=GetFatiguePercent();
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho)
{
	Mob::FillSpawnStruct(ns, ForWho);

	// Populate client-specific spawn information
	ns->spawn.afk		= AFK;
	ns->spawn.anon		= m_pp.anon;
	ns->spawn.gm		= GetGM() ? 1 : 0;
	ns->spawn.guildID	= GuildID();
	ns->spawn.lfg		= LFG;
//	ns->spawn.linkdead	= IsLD() ? 1 : 0;
//	ns->spawn.pvp		= GetPVP() ? 1 : 0;


	strcpy(ns->spawn.title, m_pp.title);
	strcpy(ns->spawn.suffix, m_pp.suffix);

	if (IsBecomeNPC() == true)
		ns->spawn.NPC = 1;
	else if (ForWho == this)
		ns->spawn.NPC = 10;
	else
		ns->spawn.NPC = 0;
	ns->spawn.is_pet = 0;

	if (!IsInAGuild()) {
		ns->spawn.guildrank = 0xFF;
	} else {
		ns->spawn.guildrank = guild_mgr.GetDisplayedRank(GuildID(), GuildRank(), AccountID());
	}
	ns->spawn.size			= 0; // Changing size works, but then movement stops! (wth?)
	ns->spawn.runspeed		= (gmspeed == 0) ? runspeed : 3.125f;
	if (!m_pp.showhelm) ns->spawn.showhelm = 0;

	// pp also hold this info; should we pull from there or inventory?
	// (update: i think pp should do it, as this holds LoY dye - plus, this is ugly code with Inventory!)
	const Item_Struct* item = nullptr;
	const ItemInst* inst = nullptr;
	if ((inst = m_inv[MainHands]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialHands]	= item->Material;
		ns->spawn.colors[MaterialHands].color	= GetEquipmentColor(MaterialHands);
	}
	if ((inst = m_inv[MainHead]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialHead]	= item->Material;
		ns->spawn.colors[MaterialHead].color	= GetEquipmentColor(MaterialHead);
	}
	if ((inst = m_inv[MainArms]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialArms]	= item->Material;
		ns->spawn.colors[MaterialArms].color	= GetEquipmentColor(MaterialArms);
	}
	if ((inst = m_inv[MainWrist1]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialWrist]= item->Material;
		ns->spawn.colors[MaterialWrist].color	= GetEquipmentColor(MaterialWrist);
	}

	/*
	// non-live behavior
	if ((inst = m_inv[SLOT_BRACER02]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialWrist]= item->Material;
		ns->spawn.colors[MaterialWrist].color	= GetEquipmentColor(MaterialWrist);
	}
	*/

	if ((inst = m_inv[MainChest]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialChest]	= item->Material;
		ns->spawn.colors[MaterialChest].color	= GetEquipmentColor(MaterialChest);
	}
	if ((inst = m_inv[MainLegs]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialLegs]	= item->Material;
		ns->spawn.colors[MaterialLegs].color	= GetEquipmentColor(MaterialLegs);
	}
	if ((inst = m_inv[MainFeet]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		ns->spawn.equipment[MaterialFeet]	= item->Material;
		ns->spawn.colors[MaterialFeet].color	= GetEquipmentColor(MaterialFeet);
	}
	if ((inst = m_inv[MainPrimary]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		if (strlen(item->IDFile) > 2)
			ns->spawn.equipment[MaterialPrimary] = atoi(&item->IDFile[2]);
	}
	if ((inst = m_inv[MainSecondary]) && inst->IsType(ItemClassCommon)) {
		item = inst->GetItem();
		if (strlen(item->IDFile) > 2)
			ns->spawn.equipment[MaterialSecondary] = atoi(&item->IDFile[2]);
	}

	//these two may be related to ns->spawn.texture
	/*
	ns->spawn.npc_armor_graphic = texture;
	ns->spawn.npc_helm_graphic = helmtexture;
	*/

	//filling in some unknowns to make the client happy
//	ns->spawn.unknown0002[2] = 3;

	UpdateEquipLightValue();
	UpdateActiveLightValue();
	ns->spawn.light = active_light;
}

bool Client::GMHideMe(Client* client) {
	if (GetHideMe()) {
		if (client == 0)
			return true;
		else if (Admin() > client->Admin())
			return true;
		else
			return false;
	}
	else
		return false;
}

void Client::Duck() {
	SetAppearance(eaCrouching, false);
}

void Client::Stand() {
	SetAppearance(eaStanding, false);
}

void Client::ChangeLastName(const char* in_lastname) {
	memset(m_pp.last_name, 0, sizeof(m_pp.last_name));
	strn0cpy(m_pp.last_name, in_lastname, sizeof(m_pp.last_name));
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GMLastName, sizeof(GMLastName_Struct));
	GMLastName_Struct* gmn = (GMLastName_Struct*)outapp->pBuffer;
	strcpy(gmn->name, name);
	strcpy(gmn->gmname, name);
	strcpy(gmn->lastname, in_lastname);
	gmn->unknown[0]=1;
	gmn->unknown[1]=1;
	gmn->unknown[2]=1;
	gmn->unknown[3]=1;
	entity_list.QueueClients(this, outapp, false);
	// Send name update packet here... once know what it is
	safe_delete(outapp);
}

bool Client::ChangeFirstName(const char* in_firstname, const char* gmname)
{
	// check duplicate name
	bool usedname = database.CheckUsedName((const char*) in_firstname);
	if (!usedname) {
		return false;
	}

	// update character_
	if(!database.UpdateName(GetName(), in_firstname))
		return false;

	// update pp
	memset(m_pp.name, 0, sizeof(m_pp.name));
	snprintf(m_pp.name, sizeof(m_pp.name), "%s", in_firstname);
	strcpy(name, m_pp.name);
	Save();

	// send name update packet
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GMNameChange, sizeof(GMName_Struct));
	GMName_Struct* gmn=(GMName_Struct*)outapp->pBuffer;
	strn0cpy(gmn->gmname,gmname,64);
	strn0cpy(gmn->oldname,GetName(),64);
	strn0cpy(gmn->newname,in_firstname,64);
	gmn->unknown[0] = 1;
	gmn->unknown[1] = 1;
	gmn->unknown[2] = 1;
	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);

	// finally, update the /who list
	UpdateWho();

	// success
	return true;
}

void Client::SetGM(bool toggle) {
	m_pp.gm = toggle ? 1 : 0;
	Message(CC_Red, "You are %s a GM.", m_pp.gm ? "now" : "no longer");
	SendAppearancePacket(AT_GM, m_pp.gm);
	Save();
	UpdateWho();
}

void Client::SetAnon(bool toggle) {
	m_pp.anon = toggle ? 1 : 0;
	SendAppearancePacket(AT_Anon, m_pp.anon);
	Save();
	UpdateWho();
}

void Client::ReadBook(BookRequest_Struct *book) {
	char *txtfile = book->txtfile;

	if(txtfile[0] == '0' && txtfile[1] == '\0') {
		//invalid book... coming up on non-book items.
		return;
	}

	std::string booktxt2 = database.GetBook(txtfile);
	int length = booktxt2.length();

	if (booktxt2[0] != '\0') {
#if EQDEBUG >= 6
		Log.Out(Logs::General, Logs::Normal, "Client::ReadBook() textfile:%s Text:%s", txtfile, booktxt2.c_str());
#endif
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ReadBook, length + sizeof(BookText_Struct));

		BookText_Struct *out = (BookText_Struct *) outapp->pBuffer;
		out->window = book->window;
		out->type = book->type;
		out->invslot = book->invslot;
		memcpy(out->booktext, booktxt2.c_str(), length);

		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::QuestReadBook(const char* text, uint8 type) {
	std::string booktxt2 = text;
	int length = booktxt2.length();
	if (booktxt2[0] != '\0') {
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ReadBook, length + sizeof(BookText_Struct));
		BookText_Struct *out = (BookText_Struct *) outapp->pBuffer;
		out->window = 0xFF;
		out->type = type;
		out->invslot = 0;
		memcpy(out->booktext, booktxt2.c_str(), length);
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::SendClientMoneyUpdate(uint8 type,uint32 amount){
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_TradeMoneyUpdate,sizeof(TradeMoneyUpdate_Struct));
	TradeMoneyUpdate_Struct* mus= (TradeMoneyUpdate_Struct*)outapp->pBuffer;
	mus->amount=amount;
	mus->trader=0;
	mus->type=type;
	Log.Out(Logs::Detail, Logs::Debug, "Client::SendClientMoneyUpdate() %s added %i coin of type: %i.",
			GetName(), amount, type);
	QueuePacket(outapp);
	safe_delete(outapp);
}

bool Client::TakeMoneyFromPP(uint64 copper, bool updateclient) {
	int64 copperpp,silver,gold,platinum;
	copperpp = m_pp.copper;
	silver = static_cast<int64>(m_pp.silver) * 10;
	gold = static_cast<int64>(m_pp.gold) * 100;
	platinum = static_cast<int64>(m_pp.platinum) * 1000;

	int64 clienttotal = copperpp + silver + gold + platinum;

	clienttotal -= copper;
	if(clienttotal < 0)
	{
		return false; // Not enough money!
	}
	else
	{
		copperpp -= copper;
		if(copperpp <= 0)
		{
			copper = std::abs(copperpp);
			m_pp.copper = 0;
		}
		else
		{
			m_pp.copper = copperpp;
			SaveCurrency();
			return true;
		}

		silver -= copper;
		if(silver <= 0)
		{
			copper = std::abs(silver);
			m_pp.silver = 0;
		}
		else
		{
			m_pp.silver = silver/10;
			m_pp.copper += (silver-(m_pp.silver*10));
			SaveCurrency();
			return true;
		}

		gold -=copper;

		if(gold <= 0)
		{
			copper = std::abs(gold);
			m_pp.gold = 0;
		}
		else
		{
			m_pp.gold = gold/100;
			uint64 silvertest = (gold-(static_cast<uint64>(m_pp.gold)*100))/10;
			m_pp.silver += silvertest;
			uint64 coppertest = (gold-(static_cast<uint64>(m_pp.gold)*100+silvertest*10));
			m_pp.copper += coppertest;
			SaveCurrency();
			return true;
		}

		platinum -= copper;

		//Impossible for plat to be negative, already checked above

		m_pp.platinum = platinum/1000;
		uint64 goldtest = (platinum-(static_cast<uint64>(m_pp.platinum)*1000))/100;
		m_pp.gold += goldtest;
		uint64 silvertest = (platinum-(static_cast<uint64>(m_pp.platinum)*1000+goldtest*100))/10;
		m_pp.silver += silvertest;
		uint64 coppertest = (platinum-(static_cast<uint64>(m_pp.platinum)*1000+goldtest*100+silvertest*10));
		m_pp.copper = coppertest;
		RecalcWeight();
		SaveCurrency();
		return true;
	}
}

void Client::AddMoneyToPP(uint64 copper, bool updateclient){
	uint64 tmp;
	uint64 tmp2;
	tmp = copper;

	/* Add Amount of Platinum */
	tmp2 = tmp/1000;
	int32 new_val = m_pp.platinum + tmp2;
	if(new_val < 0) { m_pp.platinum = 0; }
	else { m_pp.platinum = m_pp.platinum + tmp2; }
	tmp-=tmp2*1000;
	if(updateclient)
		SendClientMoneyUpdate(3,tmp2);

	/* Add Amount of Gold */
	tmp2 = tmp/100;
	new_val = m_pp.gold + tmp2;
	if(new_val < 0) { m_pp.gold = 0; }
	else { m_pp.gold = m_pp.gold + tmp2; }

	tmp-=tmp2*100;
	if(updateclient)
		SendClientMoneyUpdate(2,tmp2);

	/* Add Amount of Silver */
	tmp2 = tmp/10;
	new_val = m_pp.silver + tmp2;
	if(new_val < 0) {
		m_pp.silver = 0;
	} else {
		m_pp.silver = m_pp.silver + tmp2;
	}
	tmp-=tmp2*10;
	if(updateclient)
		SendClientMoneyUpdate(1,tmp2);

	// Add Amount of Copper
	tmp2 = tmp;
	new_val = m_pp.copper + tmp2;
	if(new_val < 0) {
		m_pp.copper = 0;
	} else {
		m_pp.copper = m_pp.copper + tmp2;
	}
	if(updateclient)
		SendClientMoneyUpdate(0,tmp2);

	RecalcWeight();

	SaveCurrency();

	Log.Out(Logs::General, Logs::None, "Client::AddMoneyToPP() %s should have: plat:%i gold:%i silver:%i copper:%i", GetName(), m_pp.platinum, m_pp.gold, m_pp.silver, m_pp.copper);
}

void Client::EVENT_ITEM_ScriptStopReturn(){
	/* Set a timestamp in an entity variable for plugin check_handin.pl in return_items
		This will stopgap players from items being returned if global_npc.pl has a catch all return_items
	*/
	struct timeval read_time;
	char buffer[50];
	gettimeofday(&read_time, 0);
	sprintf(buffer, "%li.%li \n", read_time.tv_sec, read_time.tv_usec);
	this->SetEntityVariable("Stop_Return", buffer);
}

void Client::AddMoneyToPP(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, bool updateclient){
	this->EVENT_ITEM_ScriptStopReturn();

	int32 new_value = m_pp.platinum + platinum;
	if(new_value >= 0 && new_value > m_pp.platinum)
		m_pp.platinum += platinum;
	if(updateclient)
		SendClientMoneyUpdate(3,platinum);

	new_value = m_pp.gold + gold;
	if(new_value >= 0 && new_value > m_pp.gold)
		m_pp.gold += gold;
	if(updateclient)
		SendClientMoneyUpdate(2,gold);

	new_value = m_pp.silver + silver;
	if(new_value >= 0 && new_value > m_pp.silver)
		m_pp.silver += silver;
	if(updateclient)
		SendClientMoneyUpdate(1,silver);

	new_value = m_pp.copper + copper;
	if(new_value >= 0 && new_value > m_pp.copper)
		m_pp.copper += copper;
	if(updateclient)
		SendClientMoneyUpdate(0,copper);

	RecalcWeight();
	SaveCurrency();

#if (EQDEBUG>=5)
		Log.Out(Logs::General, Logs::None, "Client::AddMoneyToPP() %s should have: plat:%i gold:%i silver:%i copper:%i",
			GetName(), m_pp.platinum, m_pp.gold, m_pp.silver, m_pp.copper);
#endif
}

bool Client::HasMoney(uint64 Copper) {

	if ((static_cast<uint64>(m_pp.copper) +
		(static_cast<uint64>(m_pp.silver) * 10) +
		(static_cast<uint64>(m_pp.gold) * 100) +
		(static_cast<uint64>(m_pp.platinum) * 1000)) >= Copper)
		return true;

	return false;
}

uint64 Client::GetCarriedMoney() {

	return ((static_cast<uint64>(m_pp.copper) +
		(static_cast<uint64>(m_pp.silver) * 10) +
		(static_cast<uint64>(m_pp.gold) * 100) +
		(static_cast<uint64>(m_pp.platinum) * 1000)));
}

uint64 Client::GetAllMoney() {

	return (
		(static_cast<uint64>(m_pp.copper) +
		(static_cast<uint64>(m_pp.silver) * 10) +
		(static_cast<uint64>(m_pp.gold) * 100) +
		(static_cast<uint64>(m_pp.platinum) * 1000) +
		(static_cast<uint64>(m_pp.copper_bank) +
		(static_cast<uint64>(m_pp.silver_bank) * 10) +
		(static_cast<uint64>(m_pp.gold_bank) * 100) +
		(static_cast<uint64>(m_pp.platinum_bank) * 1000) +
		(static_cast<uint64>(m_pp.copper_cursor) +
		(static_cast<uint64>(m_pp.silver_cursor) * 10) +
		(static_cast<uint64>(m_pp.gold_cursor) * 100) +
		(static_cast<uint64>(m_pp.platinum_cursor) * 1000) +
		(static_cast<uint64>(m_pp.platinum_shared) * 1000)))));
}

bool Client::CheckIncreaseSkill(SkillUseTypes skillid, Mob *against_who, int chancemodi) {
	if (IsDead() || IsUnconscious())
		return false;
	if (IsAIControlled()) // no skillups while chamred =p
		return false;
	if (skillid > HIGHEST_SKILL)
		return false;
	int skillval = GetRawSkill(skillid);
	int maxskill = GetMaxSkillAfterSpecializationRules(skillid, MaxSkill(skillid));

	if(against_who)
	{
		if(against_who->GetSpecialAbility(IMMUNE_AGGRO) || against_who->IsClient() ||
			GetLevelCon(against_who->GetLevel()) == CON_GREEN)
		{
			Log.Out(Logs::Detail, Logs::Skills, "Skill %d at value %d failed to gain due to invalid target.", skillid, skillval);
			return false; 
		}
	}

	// Make sure we're not already at skill cap
	if (skillval < maxskill)
	{
		// the higher your current skill level, the harder it is
		int32 Chance = 10 + chancemodi + ((252 - skillval) / 20);

		Chance = (Chance * RuleI(Character, SkillUpModifier) / 100);

		Chance = mod_increase_skill_chance(Chance, against_who);

		if(Chance < 1)
			Chance = 1; // Make it always possible

		if(zone->random.Real(0, 99) < Chance)
		{
			SetSkill(skillid, GetRawSkill(skillid) + 1);
			Log.Out(Logs::Detail, Logs::Skills, "Skill %d at value %d successfully gain with %i percent chance (mod %d)", skillid, skillval, Chance, chancemodi);
			return true;
		} else {
			Log.Out(Logs::Detail, Logs::Skills, "Skill %d at value %d failed to gain with %i percent chance (mod %d)", skillid, skillval, Chance, chancemodi);
		}
	} else {
		Log.Out(Logs::Detail, Logs::Skills, "Skill %d at value %d cannot increase due to maximum %d", skillid, skillval, maxskill);
	}
	return false;
}

void Client::CheckLanguageSkillIncrease(uint8 langid, uint8 TeacherSkill) {
	if (IsDead() || IsUnconscious())
		return;
	if (IsAIControlled())
		return;
	if (langid >= MAX_PP_LANGUAGE)
		return;		// do nothing if langid is an invalid language

	int LangSkill = m_pp.languages[langid];		// get current language skill

	if (LangSkill < 100) {	// if the language isn't already maxed
		int32 Chance = 5 + ((TeacherSkill - LangSkill)/10);	// greater chance to learn if teacher's skill is much higher than yours
		Chance = (Chance * RuleI(Character, SkillUpModifier)/100);

		if(zone->random.Real(0,100) < Chance) {	// if they make the roll
			IncreaseLanguageSkill(langid);	// increase the language skill by 1
			Log.Out(Logs::Detail, Logs::Skills, "Language %d at value %d successfully gain with %.4f%%chance", langid, LangSkill, Chance);
		}
		else
			Log.Out(Logs::Detail, Logs::Skills, "Language %d at value %d failed to gain with %.4f%%chance", langid, LangSkill, Chance);
	}
}

bool Client::HasSkill(SkillUseTypes skill_id) const {
	/*if(skill_id == SkillMeditate)
	{
		if(SkillTrainLvl(skill_id, GetClass()) >= GetLevel())
			return true;
	}
	else*/
		return((GetSkill(skill_id) > 0) && CanHaveSkill(skill_id));
}

bool Client::CanHaveSkill(SkillUseTypes skill_id) const {
	return(database.GetSkillCap(GetClass(), skill_id, RuleI(Character, MaxLevel)) > 0);
	//if you don't have it by max level, then odds are you never will?
}

uint16 Client::MaxSkill(SkillUseTypes skillid, uint16 class_, uint16 level) const {
	return(database.GetSkillCap(class_, skillid, level));
}

uint8 Client::SkillTrainLevel(SkillUseTypes skillid, uint16 class_) {
	return(database.GetTrainLevel(class_, skillid, RuleI(Character, MaxLevel)));
}

uint8 Client::SkillTrainLvl(SkillUseTypes skillid, uint16 class_) const {
	return(database.GetTrainLevel(class_, skillid, RuleI(Character, MaxLevel)));
}

uint16 Client::GetMaxSkillAfterSpecializationRules(SkillUseTypes skillid, uint16 maxSkill)
{
	uint16 Result = maxSkill;

	uint16 PrimarySpecialization = 0, SecondaryForte = 0;

	uint16 PrimarySkillValue = 0, SecondarySkillValue = 0;

	uint16 MaxSpecializations = GetAA(aaSecondaryForte) ? 2 : 1;

	if(skillid >= SkillSpecializeAbjure && skillid <= SkillSpecializeEvocation)
	{
		bool HasPrimarySpecSkill = false;

		int NumberOfPrimarySpecSkills = 0;

		for(int i = SkillSpecializeAbjure; i <= SkillSpecializeEvocation; ++i)
		{
			if(m_pp.skills[i] > 50)
			{
				HasPrimarySpecSkill = true;
				NumberOfPrimarySpecSkills++;
			}
			if(m_pp.skills[i] > PrimarySkillValue)
			{
				if(PrimarySkillValue > SecondarySkillValue)
				{
					SecondarySkillValue = PrimarySkillValue;
					SecondaryForte = PrimarySpecialization;
				}

				PrimarySpecialization = i;
				PrimarySkillValue = m_pp.skills[i];
			}
			else if(m_pp.skills[i] > SecondarySkillValue)
			{
				SecondaryForte = i;
				SecondarySkillValue = m_pp.skills[i];
			}
		}

		if(SecondarySkillValue <=50)
			SecondaryForte = 0;

		if(HasPrimarySpecSkill)
		{
			if(NumberOfPrimarySpecSkills <= MaxSpecializations)
			{
				if(MaxSpecializations == 1)
				{
					if(skillid != PrimarySpecialization)
					{
						Result = 50;
					}
				}
				else
				{
					if((skillid != PrimarySpecialization) && ((skillid == SecondaryForte) || (SecondaryForte == 0)))
					{
						if((PrimarySkillValue > 100) || (!SecondaryForte))
							Result = 100;
					}
					else if(skillid != PrimarySpecialization)
					{
						Result = 50;
					}
				}
			}
			else
			{
				Message(CC_Red, "Your spell casting specializations skills have been reset. "
						"Only %i primary specialization skill is allowed.", MaxSpecializations);

				for(int i = SkillSpecializeAbjure; i <= SkillSpecializeEvocation; ++i)
					SetSkill((SkillUseTypes)i, 1);

				Save();

				Log.Out(Logs::General, Logs::Normal, "Reset %s's caster specialization skills to 1. "
								"Too many specializations skills were above 50.", GetCleanName());
			}

		}
	}
	// This should possibly be handled by bonuses rather than here.
	switch(skillid)
	{
		case SkillTracking:
		{
			Result += ((GetAA(aaAdvancedTracking) * 10) + (GetAA(aaTuneofPursuance) * 10));
			break;
		}

		default:
			break;
	}

	return Result;
}

void Client::SetPVP(bool toggle) {
	m_pp.pvp = toggle ? 1 : 0;

	if(GetPVP())
		this->Message_StringID(MT_Shout,PVP_ON);
	else
		Message(CC_Red, "You no longer follow the ways of discord.");

	SendAppearancePacket(AT_PVP, GetPVP());
	Save();
}

void Client::WorldKick() {
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GMKick, sizeof(GMKick_Struct));
	GMKick_Struct* gmk = (GMKick_Struct *)outapp->pBuffer;
	strcpy(gmk->name,GetName());
	QueuePacket(outapp);
	safe_delete(outapp);
	Kick();
}

void Client::GMKill() {
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GMKill, sizeof(GMKill_Struct));
	GMKill_Struct* gmk = (GMKill_Struct *)outapp->pBuffer;
	strcpy(gmk->name,GetName());
	QueuePacket(outapp);
	safe_delete(outapp);
}

bool Client::CheckAccess(int16 iDBLevel, int16 iDefaultLevel) {
	if ((admin >= iDBLevel) || (iDBLevel == 255 && admin >= iDefaultLevel))
		return true;
	else
		return false;
}

void Client::MemorizeSpell(uint32 slot,uint32 spellid,uint32 scribing){
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_MemorizeSpell,sizeof(MemorizeSpell_Struct));
	MemorizeSpell_Struct* mss=(MemorizeSpell_Struct*)outapp->pBuffer;
	mss->scribing=scribing;
	mss->slot=slot;
	mss->spell_id=spellid;
	outapp->priority = 5;
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SetFeigned(bool in_feigned) {
	if (in_feigned)
	{
		if(RuleB(Character, FeignKillsPet))
		{
			SetPet(0);
		}
		SetHorseId(0);
		entity_list.ClearFeignAggro(this);
		forget_timer.Start(FeignMemoryDuration);
	} else {
		forget_timer.Disable();
	}
	feigned=in_feigned;
 }

void Client::LogMerchant(Client* player, Mob* merchant, uint32 quantity, uint32 price, const Item_Struct* item, bool buying)
{
	if(!player || !merchant || !item)
		return;

	std::string LogText = "Qty: ";

	char Buffer[255];
	memset(Buffer, 0, sizeof(Buffer));

	snprintf(Buffer, sizeof(Buffer)-1, "%3i", quantity);
	LogText += Buffer;
	snprintf(Buffer, sizeof(Buffer)-1, "%10i", price);
	LogText += " TotalValue: ";
	LogText += Buffer;
	snprintf(Buffer, sizeof(Buffer)-1, " ItemID: %7i", item->ID);
	LogText += Buffer;
	LogText += " ";
	snprintf(Buffer, sizeof(Buffer)-1, " %s", item->Name);
	LogText += Buffer;

	if (buying==true) {
		database.logevents(player->AccountName(),player->AccountID(),player->admin,player->GetName(),merchant->GetName(),"Buying from Merchant",LogText.c_str(),2);
	}
	else {
		database.logevents(player->AccountName(),player->AccountID(),player->admin,player->GetName(),merchant->GetName(),"Selling to Merchant",LogText.c_str(),3);
	}
}

bool Client::BindWound(Mob* bindmob, bool start, bool fail){
	EQApplicationPacket* outapp = 0;
	if(!fail) {
		outapp = new EQApplicationPacket(OP_Bind_Wound, sizeof(BindWound_Struct));
		BindWound_Struct* bind_out = (BindWound_Struct*) outapp->pBuffer;
		// Start bind
		if(!bindwound_timer.Enabled()) {
			//make sure we actually have a bandage... and consume it.
			int16 bslot = m_inv.HasItemByUse(ItemTypeBandage, 1, invWhereWorn|invWherePersonal);
			if (bslot == INVALID_INDEX) {
				bind_out->type = 3;
				QueuePacket(outapp);
				bind_out->type = 0;	//this is the wrong message, dont know the right one.
				QueuePacket(outapp);
				return(true);
			}
			DeleteItemInInventory(bslot, 1, true);	//do we need client update?

			// start complete timer
			bindwound_timer.Start(10000);
			bindwound_target = bindmob;

			// Send client unlock
			bind_out->type = 3;
			QueuePacket(outapp);
			bind_out->type = 0;
			// Client Unlocked
			if(!bindmob) {
				// send "bindmob dead" to client
				bind_out->type = 4;
				QueuePacket(outapp);
				bind_out->type = 0;
				bindwound_timer.Disable();
				bindwound_target = 0;
			}
			else {
				// send bindmob "stand still"
				if(!bindmob->IsAIControlled() && bindmob != this ) {
					bind_out->type = 2; // ?
					//bind_out->type = 3; // ?
					bind_out->to = GetID(); // ?
					bindmob->CastToClient()->QueuePacket(outapp);
					bind_out->type = 0;
					bind_out->to = 0;
				}
				else if (bindmob->IsAIControlled() && bindmob != this ){
					bindmob->CastToClient()->Message_StringID(CC_User_Skills, STAY_STILL); // Tell IPC to stand still?
				}
				else {
					Message_StringID(CC_User_Skills, STAY_STILL); // Binding self
				}
			}
		} else {
		// finish bind
			// disable complete timer
			bindwound_timer.Disable();
			bindwound_target = 0;
			if(!bindmob){
					// send "bindmob gone" to client
					bind_out->type = 5; // not in zone
					QueuePacket(outapp);
					bind_out->type = 0;
			}

			else {
				if (!GetFeigned() && (DistanceSquared(bindmob->GetPosition(), m_Position)  <= 400)) {
					// send bindmob bind done
					if(!bindmob->IsAIControlled() && bindmob != this ) {

					}
					else if(bindmob->IsAIControlled() && bindmob != this ) {
					// Tell IPC to resume??
					}
					else {
					// Binding self
					}
					// Send client bind done

					bind_out->type = 1; // Done
					QueuePacket(outapp);
					bind_out->type = 0;
					CheckIncreaseSkill(SkillBindWound, nullptr, 5);

					int maxHPBonus = spellbonuses.MaxBindWound + itembonuses.MaxBindWound + aabonuses.MaxBindWound;

					int max_percent = 50 + 10 * maxHPBonus;

					if(GetClass() == MONK && GetSkill(SkillBindWound) > 200) {
						max_percent = 70 + 10 * maxHPBonus;
					}

					max_percent = mod_bindwound_percent(max_percent, bindmob);

					int max_hp = bindmob->GetMaxHP()*max_percent/100;

					// send bindmob new hp's
					if (bindmob->GetHP() < bindmob->GetMaxHP() && bindmob->GetHP() <= (max_hp)-1){
						// 0.120 per skill point, 0.60 per skill level, minimum 3 max 30
						int bindhps = 3;


						if (GetSkill(SkillBindWound) > 200) {
							bindhps += GetSkill(SkillBindWound)*4/10;
						} else if (GetSkill(SkillBindWound) >= 10) {
							bindhps += GetSkill(SkillBindWound)/4;
						}

						//Implementation of aaMithanielsBinding is a guess (the multiplier)
						int bindBonus = spellbonuses.BindWound + itembonuses.BindWound + aabonuses.BindWound;

						bindhps += bindhps*bindBonus / 100;

						bindhps = mod_bindwound_hp(bindhps, bindmob);

						//if the bind takes them above the max bindable
						//cap it at that value. Dont know if live does it this way
						//but it makes sense to me.
						int chp = bindmob->GetHP() + bindhps;
						if(chp > max_hp)
							chp = max_hp;

						bindmob->SetHP(chp);
						bindmob->SendHPUpdate();
					}
					else {
						//I dont have the real, live
						if(bindmob->IsClient() && bindmob != this)
							bindmob->CastToClient()->Message(CC_Yellow, "You cannot have your wounds bound above %d%% hitpoints.", max_percent);
						else
							Message(CC_Yellow, "You cannot bind wounds above %d%% hitpoints.", max_percent);
					}
					Stand();
				}
				else {
					// Send client bind failed
					if(bindmob != this)
						bind_out->type = 6; // They moved
					else
						bind_out->type = 7; // Bandager moved

					QueuePacket(outapp);
					bind_out->type = 0;
				}
			}
		}
	}
	else if (bindwound_timer.Enabled()) {
		// You moved
		outapp = new EQApplicationPacket(OP_Bind_Wound, sizeof(BindWound_Struct));
		BindWound_Struct* bind_out = (BindWound_Struct*) outapp->pBuffer;
		bindwound_timer.Disable();
		bindwound_target = 0;
		bind_out->type = 7;
		QueuePacket(outapp);
		bind_out->type = 3;
		QueuePacket(outapp);
	}
	safe_delete(outapp);
	return true;
}

void Client::SetMaterial(int16 in_slot, uint32 item_id) {
	const Item_Struct* item = database.GetItem(item_id);
	if (item && (item->ItemClass==ItemClassCommon)) {
		if (in_slot==MainHead)
			m_pp.item_material[MaterialHead]		= item->Material;
		else if (in_slot==MainChest)
			m_pp.item_material[MaterialChest]		= item->Material;
		else if (in_slot==MainArms)
			m_pp.item_material[MaterialArms]		= item->Material;
		else if (in_slot==MainWrist1)
			m_pp.item_material[MaterialWrist]		= item->Material;
		else if (in_slot == MainWrist2)
			m_pp.item_material[MaterialWrist]		= item->Material;
		else if (in_slot==MainHands)
			m_pp.item_material[MaterialHands]		= item->Material;
		else if (in_slot==MainLegs)
			m_pp.item_material[MaterialLegs]		= item->Material;
		else if (in_slot==MainFeet)
			m_pp.item_material[MaterialFeet]		= item->Material;
		else if (in_slot == MainPrimary)
			m_pp.item_material[MaterialPrimary] = atoi(item->IDFile + 2);
		else if (in_slot == MainSecondary)
			m_pp.item_material[MaterialSecondary] = atoi(item->IDFile + 2);
	}
}

void Client::ServerFilter(SetServerFilter_Struct* filter){

/*	this code helps figure out the filter IDs in the packet if needed
	static SetServerFilter_Struct ssss;
	int r;
	uint32 *o = (uint32 *) &ssss;
	uint32 *n = (uint32 *) filter;
	for(r = 0; r < (sizeof(SetServerFilter_Struct)/4); r++) {
		if(*o != *n)
			Log.Out(Logs::Detail, Logs::Debug, "Filter %d changed from %d to %d", r, *o, *n);
		o++; n++;
	}
	memcpy(&ssss, filter, sizeof(SetServerFilter_Struct));
*/
#define Filter0(type) \
	if(filter->filters[type] == 1) \
		ClientFilters[type] = FilterShow; \
	else \
		ClientFilters[type] = FilterHide;
#define Filter1(type) \
	if(filter->filters[type] == 0) \
		ClientFilters[type] = FilterShow; \
	else \
		ClientFilters[type] = FilterHide;

	Filter1(FilterNone);
	Filter0(FilterGuildChat);
	Filter0(FilterSocials);
	Filter0(FilterGroupChat);
	Filter0(FilterShouts);
	Filter0(FilterAuctions);
	Filter0(FilterOOC);

	if(filter->filters[FilterPCSpells] == 0)
		ClientFilters[FilterPCSpells] = FilterShow;
	else if(filter->filters[FilterPCSpells] == 1)
		ClientFilters[FilterPCSpells] = FilterHide;
	else
		ClientFilters[FilterPCSpells] = FilterShowGroupOnly;

	Filter1(FilterNPCSpells);

	if(filter->filters[FilterBardSongs] == 0)
		ClientFilters[FilterBardSongs] = FilterShow;
	else if(filter->filters[FilterBardSongs] == 1)
		ClientFilters[FilterBardSongs] = FilterShowSelfOnly;
	else if(filter->filters[FilterBardSongs] == 2)
		ClientFilters[FilterBardSongs] = FilterShowGroupOnly;
	else
		ClientFilters[FilterBardSongs] = FilterHide;

	if(filter->filters[FilterSpellCrits] == 0)
		ClientFilters[FilterSpellCrits] = FilterShow;
	else if(filter->filters[FilterSpellCrits] == 1)
		ClientFilters[FilterSpellCrits] = FilterShowSelfOnly;
	else
		ClientFilters[FilterSpellCrits] = FilterHide;

	if (filter->filters[FilterMeleeCrits] == 0)
		ClientFilters[FilterMeleeCrits] = FilterShow;
	else if (filter->filters[FilterMeleeCrits] == 1)
		ClientFilters[FilterMeleeCrits] = FilterShowSelfOnly;
	else
		ClientFilters[FilterMeleeCrits] = FilterHide;

	Filter0(FilterMyMisses);
	Filter0(FilterOthersMiss);
	Filter0(FilterOthersHit);
	Filter0(FilterMissedMe);
	Filter1(FilterDamageShields);
}

// this version is for messages with no parameters
void Client::Message_StringID(uint32 type, uint32 string_id, uint32 distance)
{
	if (GetFilter(FilterMeleeCrits) == FilterHide && type == MT_CritMelee) //98 is self...
		return;
	if (GetFilter(FilterSpellCrits) == FilterHide && type == MT_SpellCrits)
		return;
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_FormattedMessage, 12);
	OldFormattedMessage_Struct *fm = (OldFormattedMessage_Struct *)outapp->pBuffer;
	fm->string_id = string_id;
	fm->type = type;

	if(distance>0)
		entity_list.QueueCloseClients(this,outapp,false,distance);
	else
		QueuePacket(outapp);
	safe_delete(outapp);

}

//
// this list of 9 args isn't how I want to do it, but to use va_arg
// you have to know how many args you're expecting, and to do that we have
// to load the eqstr file and count them in the string.
// This hack sucks but it's gonna work for now.
//
void Client::Message_StringID(uint32 type, uint32 string_id, const char* message1,
	const char* message2,const char* message3,const char* message4,
	const char* message5,const char* message6,const char* message7,
	const char* message8,const char* message9, uint32 distance)
{
	if (GetFilter(FilterMeleeCrits) == FilterHide && type == MT_CritMelee) //98 is self...
		return;
	if (GetFilter(FilterSpellCrits) == FilterHide && type == MT_SpellCrits)
		return;

	int i, argcount, length;
	char *bufptr;
	const char *message_arg[9] = {0};

	if(type==MT_Emote)
		type=4;

	if(!message1)
	{
		Message_StringID(type, string_id);	// use the simple message instead
		return;
	}

	i = 0;
	message_arg[i++] = message1;
	message_arg[i++] = message2;
	message_arg[i++] = message3;
	message_arg[i++] = message4;
	message_arg[i++] = message5;
	message_arg[i++] = message6;
	message_arg[i++] = message7;
	message_arg[i++] = message8;
	message_arg[i++] = message9;

	for(argcount = length = 0; message_arg[argcount]; argcount++)
    {
		length += strlen(message_arg[argcount]) + 1;
    }

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_FormattedMessage, length+13);
	OldFormattedMessage_Struct *fm = (OldFormattedMessage_Struct *)outapp->pBuffer;
	fm->string_id = string_id;
	fm->type = type;
	bufptr = fm->message;

	// since we're moving the pointer the 0 offset is correct
	bufptr[0] = '\0';

    for(i = 0; i < argcount; i++)
    {
        strcpy(bufptr, message_arg[i]);
        bufptr += strlen(message_arg[i]) + 1;
    }


    if(distance>0)
        entity_list.QueueCloseClients(this,outapp,false,distance);
    else
        QueuePacket(outapp);
    safe_delete(outapp);

}

// helper function, returns true if we should see the message
bool Client::FilteredMessageCheck(Mob *sender, eqFilterType filter)
{
	eqFilterMode mode = GetFilter(filter);
	// easy ones first
	if (mode == FilterShow)
		return true;
	else if (mode == FilterHide)
		return false;

	if (!sender && mode == FilterHide) {
		return false;
	} else if (sender) {
		if (this == sender) {
			if (mode == FilterHide) // don't need to check others
				return false;
		} else if (mode == FilterShowSelfOnly) { // we know sender isn't us
			return false;
		} else if (mode == FilterShowGroupOnly) {
			Group *g = GetGroup();
			Raid *r = GetRaid();
			if (g) {
				if (g->IsGroupMember(sender))
					return true;
			} else if (r && sender->IsClient()) {
				uint32 rgid1 = r->GetGroup(this);
				uint32 rgid2 = r->GetGroup(sender->CastToClient());
				if (rgid1 != 0xFFFFFFFF && rgid1 == rgid2)
					return true;
			} else {
				return false;
			}
		}
	}

	// we passed our checks
	return true;
}

void Client::FilteredMessage_StringID(Mob *sender, uint32 type,
		eqFilterType filter, uint32 string_id)
{
	if (!FilteredMessageCheck(sender, filter))
		return;

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_Animation, 12);
	SimpleMessage_Struct *sms = (SimpleMessage_Struct *)outapp->pBuffer;
	sms->color = type;
	sms->string_id = string_id;

	sms->unknown8 = 0;

	QueuePacket(outapp);
	safe_delete(outapp);

	return;
}

void Client::FilteredMessage_StringID(Mob *sender, uint32 type, eqFilterType filter, uint32 string_id,
		const char *message1, const char *message2, const char *message3,
		const char *message4, const char *message5, const char *message6,
		const char *message7, const char *message8, const char *message9)
{
	if (!FilteredMessageCheck(sender, filter))
		return;

	int i, argcount, length;
	char *bufptr;
	const char *message_arg[9] = {0};

	if (type == MT_Emote)
		type = 4;

	if (!message1) {
		FilteredMessage_StringID(sender, type, filter, string_id);	// use the simple message instead
		return;
	}

	i = 0;
	message_arg[i++] = message1;
	message_arg[i++] = message2;
	message_arg[i++] = message3;
	message_arg[i++] = message4;
	message_arg[i++] = message5;
	message_arg[i++] = message6;
	message_arg[i++] = message7;
	message_arg[i++] = message8;
	message_arg[i++] = message9;

	for (argcount = length = 0; message_arg[argcount]; argcount++)
    {
		length += strlen(message_arg[argcount]) + 1;
    }

    EQApplicationPacket* outapp = new EQApplicationPacket(OP_FormattedMessage, length+13);
	OldFormattedMessage_Struct *fm = (OldFormattedMessage_Struct *)outapp->pBuffer;
	fm->string_id = string_id;
	fm->type = type;
	bufptr = fm->message;


	for (i = 0; i < argcount; i++) {
		strcpy(bufptr, message_arg[i]);
		bufptr += strlen(message_arg[i]) + 1;
	}

	// since we're moving the pointer the 0 offset is correct
	bufptr[0] = '\0';

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::Tell_StringID(uint32 string_id, const char *who, const char *message)
{
	char string_id_str[10];
	snprintf(string_id_str, 10, "%d", string_id);

	Message_StringID(MT_TellEcho, TELL_QUEUED_MESSAGE, who, string_id_str, message);
}

void Client::SetTint(int16 in_slot, uint32 color) {
	Color_Struct new_color;
	new_color.color = color;
	SetTint(in_slot, new_color);
	database.SaveCharacterMaterialColor(this->CharacterID(), in_slot, color);
}

// Still need to reconcile bracer01 versus bracer02
void Client::SetTint(int16 in_slot, Color_Struct& color) {
	if (in_slot==MainHead)
		m_pp.item_tint[MaterialHead].color=color.color;
	else if (in_slot==MainArms)
		m_pp.item_tint[MaterialArms].color=color.color;
	else if (in_slot==MainWrist1)
		m_pp.item_tint[MaterialWrist].color=color.color;
	else if (in_slot == MainWrist2)
		m_pp.item_tint[MaterialWrist].color=color.color;
	else if (in_slot==MainHands)
		m_pp.item_tint[MaterialHands].color=color.color;
	else if (in_slot==MainPrimary)
		m_pp.item_tint[MaterialPrimary].color=color.color;
	else if (in_slot==MainSecondary)
		m_pp.item_tint[MaterialSecondary].color=color.color;
	else if (in_slot==MainChest)
		m_pp.item_tint[MaterialChest].color=color.color;
	else if (in_slot==MainLegs)
		m_pp.item_tint[MaterialLegs].color=color.color;
	else if (in_slot==MainFeet)
		m_pp.item_tint[MaterialFeet].color=color.color;

	database.SaveCharacterMaterialColor(this->CharacterID(), in_slot, color.color);
}

void Client::SetHideMe(bool flag)
{
	EQApplicationPacket app;

	gmhideme = flag;

	if(gmhideme)
	{
		if(!GetGM())
			SetGM(true);
		if(GetAnon() != 1) // 1 is anon, 2 is role
			SetAnon(true);
		database.SetHideMe(AccountID(),true);
		CreateDespawnPacket(&app, false);
		entity_list.RemoveFromTargets(this);
		trackable = false;
	}
	else
	{
		SetAnon(false);
		database.SetHideMe(AccountID(),false);
		CreateSpawnPacket(&app);
		trackable = true;
	}

	entity_list.QueueClientsStatus(this, &app, true, 0, Admin()-1);
}

void Client::SetLanguageSkill(int langid, int value)
{
	if (langid >= MAX_PP_LANGUAGE)
		return; //Invalid Language

	if (value > 100)
		value = 100; //Max lang value

	m_pp.languages[langid] = value;
	database.SaveCharacterLanguage(this->CharacterID(), langid, value);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SkillUpdate, sizeof(SkillUpdate_Struct));
	SkillUpdate_Struct* skill = (SkillUpdate_Struct*)outapp->pBuffer;
	skill->skillId = 100 + langid;
	skill->value = m_pp.languages[langid];
	QueuePacket(outapp);
	safe_delete(outapp);

	Message_StringID( MT_Skills, LANG_SKILL_IMPROVED ); //Notify the client
}

void Client::LinkDead()
{
	if (GetGroup())
	{
		entity_list.MessageGroup(this,true,15,"%s has gone linkdead.",GetName());
		GetGroup()->DelMember(this);
	}
	Raid *raid = entity_list.GetRaidByClient(this);
	if(raid){
		raid->MemberZoned(this);
	}
//	save_timer.Start(2500);
	linkdead_timer.Start(RuleI(Zone,ClientLinkdeadMS));
	SendAppearancePacket(AT_Linkdead, 1);
	client_state = CLIENT_LINKDEAD;
	AI_Start(CLIENT_LD_TIMEOUT);
	UpdateWho();
}

uint8 Client::SlotConvert(uint8 slot,bool bracer){
	uint8 slot2=0; // why are we returning MainCursor instead of INVALID_INDEX? (must be a pre-charm segment...)
	if(bracer)
		return MainWrist2;
	switch(slot){
		case MaterialHead:
			slot2=MainHead;
			break;
		case MaterialChest:
			slot2=MainChest;
			break;
		case MaterialArms:
			slot2=MainArms;
			break;
		case MaterialWrist:
			slot2=MainWrist1;
			break;
		case MaterialHands:
			slot2=MainHands;
			break;
		case MaterialLegs:
			slot2=MainLegs;
			break;
		case MaterialFeet:
			slot2=MainFeet;
			break;
		}
	return slot2;
}

uint8 Client::SlotConvert2(uint8 slot){
	uint8 slot2=0; // same as above...
	switch(slot){
		case MainHead:
			slot2=MaterialHead;
			break;
		case MainChest:
			slot2=MaterialChest;
			break;
		case MainArms:
			slot2=MaterialArms;
			break;
		case MainWrist1:
			slot2=MaterialWrist;
			break;
		case MainHands:
			slot2=MaterialHands;
			break;
		case MainLegs:
			slot2=MaterialLegs;
			break;
		case MainFeet:
			slot2=MaterialFeet;
			break;
		}
	return slot2;
}

void Client::Escape()
{
	entity_list.RemoveFromTargets(this);
	SetInvisible(INVIS_NORMAL);
	Message_StringID(MT_Skills, ESCAPE);
}

float Client::CalcPriceMod(Mob* other, bool reverse)
{
	float chaformula = 0;
	if (other)
	{
		int factionlvl = GetFactionLevel(CharacterID(), other->CastToNPC()->GetNPCTypeID(), GetRace(), GetClass(), GetDeity(), other->CastToNPC()->GetPrimaryFaction(), other);
		if (factionlvl >= FACTION_APPREHENSIVE) // Apprehensive or worse.
		{
			if (GetCHA() > 103)
			{
				chaformula = (GetCHA() - 103)*((-(RuleR(Merchant, ChaBonusMod))/100)*(RuleI(Merchant, PriceBonusPct))); // This will max out price bonus.
				if (chaformula < -1*(RuleI(Merchant, PriceBonusPct)))
					chaformula = -1*(RuleI(Merchant, PriceBonusPct));
			}
			else if (GetCHA() < 103)
			{
				chaformula = (103 - GetCHA())*(((RuleR(Merchant, ChaPenaltyMod))/100)*(RuleI(Merchant, PricePenaltyPct))); // This will bottom out price penalty.
				if (chaformula > 1*(RuleI(Merchant, PricePenaltyPct)))
					chaformula = 1*(RuleI(Merchant, PricePenaltyPct));
			}
		}
		if (factionlvl <= FACTION_INDIFFERENT) // Indifferent or better.
		{
			if (GetCHA() > 75)
			{
				chaformula = (GetCHA() - 75)*((-(RuleR(Merchant, ChaBonusMod))/100)*(RuleI(Merchant, PriceBonusPct))); // This will max out price bonus.
				if (chaformula < -1*(RuleI(Merchant, PriceBonusPct)))
					chaformula = -1*(RuleI(Merchant, PriceBonusPct));
			}
			else if (GetCHA() < 75)
			{
				chaformula = (75 - GetCHA())*(((RuleR(Merchant, ChaPenaltyMod))/100)*(RuleI(Merchant, PricePenaltyPct))); // Faction modifier keeps up from reaching bottom price penalty.
				if (chaformula > 1*(RuleI(Merchant, PricePenaltyPct)))
					chaformula = 1*(RuleI(Merchant, PricePenaltyPct));
			}
		}
	}

	if (reverse)
		chaformula *= -1; //For selling
	//Now we have, for example, 10
	chaformula /= 100; //Convert to 0.10
	chaformula += 1; //Convert to 1.10;
	return chaformula; //Returns 1.10, expensive stuff!
}

void Client::EnteringMessages(Client* client)
{
	//server rules
	char *rules;
	rules = new char [4096];

	if(database.GetVariable("Rules", rules, 4096))
	{
		uint8 flag = database.GetAgreementFlag(client->AccountID());
		if(!flag)
		{
			client->Message(CC_Red,"You must agree to the Rules, before you can move. (type #serverrules to view the rules)");
			client->Message(CC_Red,"You must agree to the Rules, before you can move. (type #serverrules to view the rules)");
			client->Message(CC_Red,"You must agree to the Rules, before you can move. (type #serverrules to view the rules)");
			client->SendAppearancePacket(AT_Anim, ANIM_FREEZE);
		}
	}
	safe_delete_array(rules);
}

void Client::SendRules(Client* client)
{
	char *rules;
	rules = new char [4096];
	char *ptr;

	database.GetVariable("Rules", rules, 4096);

	ptr = strtok(rules, "\n");
	while(ptr != nullptr)
	{

		client->Message(0,"%s",ptr);
		ptr = strtok(nullptr, "\n");
	}
	safe_delete_array(rules);
}

void Client::SetEndurance(int32 newEnd)
{
	/*Endurance can't be less than 0 or greater than max*/
	if(newEnd < 0)
		newEnd = 0;
	else if(newEnd > GetMaxEndurance()){
		newEnd = GetMaxEndurance();
	}

	cur_end = newEnd;
	SendStaminaUpdate();
}

void Client::SacrificeConfirm(Client *caster) {

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Sacrifice, sizeof(Sacrifice_Struct));
	Sacrifice_Struct *ss = (Sacrifice_Struct*)outapp->pBuffer;

	if(!caster || PendingSacrifice) return;

	if(GetLevel() < RuleI(Spells, SacrificeMinLevel)){
		caster->Message_StringID(CC_Red, SAC_TOO_LOW);	//This being is not a worthy sacrifice.
		return;
	}
	if (GetLevel() > RuleI(Spells, SacrificeMaxLevel)) {
		caster->Message_StringID(CC_Red, SAC_TOO_HIGH);
		return;
	}

	ss->CasterID = caster->GetID();
	ss->TargetID = GetID();
	ss->Confirm = 0;
	QueuePacket(outapp);
	safe_delete(outapp);
	// We store the Caster's name, because when the packet comes back, it only has the victim's entityID in it,
	// not the caster.
	SacrificeCaster += caster->GetName();
	PendingSacrifice = true;
}

//Essentially a special case death function
void Client::Sacrifice(Client *caster)
{
	if(GetLevel() >= RuleI(Spells, SacrificeMinLevel) && GetLevel() <= RuleI(Spells, SacrificeMaxLevel)){
		int exploss = (int)(GetLevel() * (GetLevel() / 18.0) * 12000);
		if(exploss < GetEXP()){
			SetEXP(GetEXP()-exploss, GetAAXP());
			SendLogoutPackets();

			// make death packet
			EQApplicationPacket app(OP_Death, sizeof(Death_Struct));
			Death_Struct* d = (Death_Struct*)app.pBuffer;
			d->spawn_id = GetID();
			d->killer_id = caster ? caster->GetID() : 0;
			d->bindzoneid = GetPP().binds[0].zoneId;
			d->spell_id = SPELL_UNKNOWN;
			d->attack_skill = 0xe7;
			d->damage = 0;
			app.priority = 6;
			entity_list.QueueClients(this, &app);

			BuffFadeAll(true);
			UnmemSpellAll();
			Group *g = GetGroup();
			if(g){
				g->MemberZoned(this);
			}
			Raid *r = entity_list.GetRaidByClient(this);
			if(r){
				r->MemberZoned(this);
			}
			ClearAllProximities();
			if(RuleB(Character, LeaveCorpses)){
				Corpse *new_corpse = new Corpse(this, 0);
				entity_list.AddCorpse(new_corpse, GetID());
				SetID(0);
			}
			Save();
			GoToDeath();
			caster->SummonItem(RuleI(Spells, SacrificeItemID));
		}
	}
	else{
		caster->Message_StringID(CC_Red, SAC_TOO_LOW);	//This being is not a worthy sacrifice.
	}
}

void Client::SendOPTranslocateConfirm(Mob *Caster, uint16 SpellID) {

	if (!Caster || PendingTranslocate)
		return;

	const SPDat_Spell_Struct &Spell = spells[SpellID];

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Translocate, sizeof(Translocate_Struct));
	Translocate_Struct *ts = (Translocate_Struct*)outapp->pBuffer;

	strcpy(ts->Caster, Caster->GetName());
	PendingTranslocateData.spell_id = ts->SpellID = SpellID;

	if ((SpellID == 1422) || (SpellID == 1334) || (SpellID == 3243)) {
		PendingTranslocateData.zone_id = ts->ZoneID = m_pp.binds[0].zoneId;
		PendingTranslocateData.instance_id = m_pp.binds[0].instance_id;
		PendingTranslocateData.x = ts->x = m_pp.binds[0].x;
		PendingTranslocateData.y = ts->y = m_pp.binds[0].y;
		PendingTranslocateData.z = ts->z = m_pp.binds[0].z;
		PendingTranslocateData.heading = m_pp.binds[0].heading;
	}
	else {
		PendingTranslocateData.zone_id = ts->ZoneID = database.GetZoneID(Spell.teleport_zone);
		PendingTranslocateData.instance_id = 0;
		PendingTranslocateData.y = ts->y = Spell.base[0];
		PendingTranslocateData.x = ts->x = Spell.base[1];
		PendingTranslocateData.z = ts->z = Spell.base[2];
		PendingTranslocateData.heading = 0.0;
	}

	ts->unknown008 = 0;
	ts->Complete = 0;

	PendingTranslocate = true;
	TranslocateTime = time(nullptr);

	QueuePacket(outapp);
	safe_delete(outapp);

	return;
}

void Client::SendPickPocketResponse(Mob *from, uint32 amt, int type, const Item_Struct* item){
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_PickPocket, sizeof(sPickPocket_Struct));
		sPickPocket_Struct* pick_out = (sPickPocket_Struct*) outapp->pBuffer;
		pick_out->coin = amt;
		pick_out->from = GetID();
		pick_out->to = from->GetID();
		pick_out->myskill = GetSkill(SkillPickPockets);

		if((type >= PickPocketPlatinum) && (type <= PickPocketCopper) && (amt == 0))
			type = PickPocketFailed;

		pick_out->type = type;
		if(item)
			strcpy(pick_out->itemname, item->Name);
		else
			pick_out->itemname[0] = '\0';
		//if we do not send this packet the client will lock up and require the player to relog.
		QueuePacket(outapp);
		safe_delete(outapp);
}

void Client::KeyRingLoad()
{
	std::string query = StringFormat("SELECT item_id FROM keyring "
                                    "WHERE char_id = '%i' ORDER BY item_id", character_id);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
		keyring.push_back(atoi(row[0]));

}

void Client::KeyRingAdd(uint32 item_id)
{
	return;
}

bool Client::KeyRingCheck(uint32 item_id)
{
	return false;
}

void Client::KeyRingList()
{
	Message(CC_Blue,"Keys on Keyring:");
	const Item_Struct *item = 0;
	for(std::list<uint32>::iterator iter = keyring.begin();
		iter != keyring.end();
		++iter)
	{
		if ((item = database.GetItem(*iter))!=nullptr) {
			Message(CC_Blue,item->Name);
		}
	}
}

bool Client::IsDiscovered(uint32 itemid) {

	std::string query = StringFormat("SELECT count(*) FROM discovered_items WHERE item_id = '%lu'", itemid);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

	auto row = results.begin();
	if (!atoi(row[0]))
		return false;

	return true;
}

void Client::DiscoverItem(uint32 itemid) {

	std::string query = StringFormat("INSERT INTO discovered_items "
									"SET item_id = %lu, char_name = '%s', "
									"discovered_date = UNIX_TIMESTAMP(), account_status = %i",
									itemid, GetName(), Admin());
	auto results = database.QueryDatabase(query);

	parse->EventPlayer(EVENT_DISCOVER_ITEM, this, "", itemid);
}

uint16 Client::GetPrimarySkillValue()
{
	SkillUseTypes skill = HIGHEST_SKILL; //because nullptr == 0, which is 1H Slashing, & we want it to return 0 from GetSkill
	bool equiped = m_inv.GetItem(MainPrimary);

	if (!equiped)
		skill = SkillHandtoHand;

	else {

		uint8 type = m_inv.GetItem(MainPrimary)->GetItem()->ItemType; //is this the best way to do this?

		switch (type)
		{
			case ItemType1HSlash: // 1H Slashing
			{
				skill = Skill1HSlashing;
				break;
			}
			case ItemType2HSlash: // 2H Slashing
			{
				skill = Skill2HSlashing;
				break;
			}
			case ItemType1HPiercing: // Piercing
			{
				skill = Skill1HPiercing;
				break;
			}
			case ItemType1HBlunt: // 1H Blunt
			{
				skill = Skill1HBlunt;
				break;
			}
			case ItemType2HBlunt: // 2H Blunt
			{
				skill = Skill2HBlunt;
				break;
			}
			case ItemType2HPiercing: // 2H Piercing
			{
				skill = Skill1HPiercing; // change to Skill2HPiercing once activated
				break;
			}
			case ItemTypeMartial: // Hand to Hand
			{
				skill = SkillHandtoHand;
				break;
			}
			default: // All other types default to Hand to Hand
			{
				skill = SkillHandtoHand;
				break;
			}
		}
	}

	return GetSkill(skill);
}

uint32 Client::GetTotalATK()
{
	uint32 AttackRating = 0;
	uint32 WornCap = itembonuses.ATK;

	if(IsClient()) {
		AttackRating = ((WornCap * 1.342) + (GetSkill(SkillOffense) * 1.345) + ((GetSTR() - 66) * 0.9) + (GetPrimarySkillValue() * 2.69));
		AttackRating += aabonuses.ATK;

		if (AttackRating < 10)
			AttackRating = 10;
	}
	else
		AttackRating = GetATK();

	AttackRating += spellbonuses.ATK;

	return AttackRating;
}

uint32 Client::GetATKRating()
{
	uint32 AttackRating = 0;
	if(IsClient()) {
		AttackRating = (GetSkill(SkillOffense) * 1.345) + ((GetSTR() - 66) * 0.9) + (GetPrimarySkillValue() * 2.69);

		if (AttackRating < 10)
			AttackRating = 10;
	}
	return AttackRating;
}

void Client::VoiceMacroReceived(uint32 Type, char *Target, uint32 MacroNumber) {

	uint32 GroupOrRaidID = 0;

	switch(Type) {

		case VoiceMacroGroup: {

			Group* g = GetGroup();

			if(g)
				GroupOrRaidID = g->GetID();
			else
				return;

			break;
		}

		case VoiceMacroRaid: {

			Raid* r = GetRaid();

			if(r)
				GroupOrRaidID = r->GetID();
			else
				return;

			break;
		}
	}

	if(!worldserver.SendVoiceMacro(this, Type, Target, MacroNumber, GroupOrRaidID))
		Message(0, "Error: World server disconnected");
}

int Client::GetAggroCount() {
	return AggroCount;
}

void Client::IncrementAggroCount() {

	// This method is called when a client is added to a mob's hate list. It turns the clients aggro flag on so
	// rest state regen is stopped, and for SoF, it sends the opcode to show the crossed swords in-combat indicator.
	//
	//
	AggroCount++;

	if(!RuleI(Character, RestRegenPercent))
		return;

	// If we already had aggro before this method was called, the combat indicator should already be up for SoF clients,
	// so we don't need to send it again.
	//
	if(AggroCount > 1)
		return;
}

void Client::DecrementAggroCount() {

	// This should be called when a client is removed from a mob's hate list (it dies or is memblurred).
	// It checks whether any other mob is aggro on the player, and if not, starts the rest timer.
	// For SoF, the opcode to start the rest state countdown timer in the UI is sent.
	//

	// If we didn't have aggro before, this method should not have been called.
	if(!AggroCount)
		return;

	AggroCount--;

	if(!RuleI(Character, RestRegenPercent))
		return;

	// Something else is still aggro on us, can't rest yet.
	if(AggroCount) return;

	uint32 time_until_rest;
	if (GetEngagedRaidTarget()) {
		time_until_rest = RuleI(Character, RestRegenRaidTimeToActivate) * 1000;
		SetEngagedRaidTarget(false);
	} else {
		if (SavedRaidRestTimer > (RuleI(Character, RestRegenTimeToActivate) * 1000)) {
			time_until_rest = SavedRaidRestTimer;
			SavedRaidRestTimer = 0;
		} else {
			time_until_rest = RuleI(Character, RestRegenTimeToActivate) * 1000;
		}
	}

	rest_timer.Start(time_until_rest);

}

void Client::SendDisciplineTimers()
{

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_DisciplineTimer, sizeof(DisciplineTimer_Struct));
	DisciplineTimer_Struct *dts = (DisciplineTimer_Struct *)outapp->pBuffer;

	for(unsigned int i = 0; i < MAX_DISCIPLINE_TIMERS; ++i)
	{
		uint32 RemainingTime = p_timers.GetRemainingTime(pTimerDisciplineReuseStart + i);

		if(RemainingTime > 0)
		{
			dts->TimerID = i;
			dts->Duration = RemainingTime;
			QueuePacket(outapp);
		}
	}

	safe_delete(outapp);
}

void Client::SummonAndRezzAllCorpses()
{
	PendingRezzXP = -1;

	ServerPacket *Pack = new ServerPacket(ServerOP_DepopAllPlayersCorpses, sizeof(ServerDepopAllPlayersCorpses_Struct));

	ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct*)Pack->pBuffer;

	sdapcs->CharacterID = CharacterID();
	sdapcs->ZoneID = zone->GetZoneID();
	sdapcs->InstanceID = zone->GetInstanceID();

	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveAllCorpsesByCharID(CharacterID());

	int CorpseCount = database.SummonAllCharacterCorpses(CharacterID(), zone->GetZoneID(), zone->GetInstanceID(), GetPosition());
	if(CorpseCount <= 0)
	{
		Message(clientMessageYellow, "You have no corpses to summnon.");
		return;
	}

	int RezzExp = entity_list.RezzAllCorpsesByCharID(CharacterID());

	if(RezzExp > 0)
		SetEXP(GetEXP() + RezzExp, GetAAXP(), true);

	Message(clientMessageYellow, "All your corpses have been summoned to your feet and have received a 100% resurrection.");
}

void Client::SummonAllCorpses(const glm::vec4& position)
{
	auto summonLocation = position;
	if(IsOrigin(position) && position.w == 0.0f)
		summonLocation = GetPosition();

	ServerPacket *Pack = new ServerPacket(ServerOP_DepopAllPlayersCorpses, sizeof(ServerDepopAllPlayersCorpses_Struct));

	ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct*)Pack->pBuffer;

	sdapcs->CharacterID = CharacterID();
	sdapcs->ZoneID = zone->GetZoneID();
	sdapcs->InstanceID = zone->GetInstanceID();

	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveAllCorpsesByCharID(CharacterID());

	database.SummonAllCharacterCorpses(CharacterID(), zone->GetZoneID(), zone->GetInstanceID(), summonLocation);
}

void Client::DepopAllCorpses()
{
	ServerPacket *Pack = new ServerPacket(ServerOP_DepopAllPlayersCorpses, sizeof(ServerDepopAllPlayersCorpses_Struct));

	ServerDepopAllPlayersCorpses_Struct *sdapcs = (ServerDepopAllPlayersCorpses_Struct*)Pack->pBuffer;

	sdapcs->CharacterID = CharacterID();
	sdapcs->ZoneID = zone->GetZoneID();
	sdapcs->InstanceID = zone->GetInstanceID();

	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveAllCorpsesByCharID(CharacterID());
}

void Client::DepopPlayerCorpse(uint32 dbid)
{
	ServerPacket *Pack = new ServerPacket(ServerOP_DepopPlayerCorpse, sizeof(ServerDepopPlayerCorpse_Struct));

	ServerDepopPlayerCorpse_Struct *sdpcs = (ServerDepopPlayerCorpse_Struct*)Pack->pBuffer;

	sdpcs->DBID = dbid;
	sdpcs->ZoneID = zone->GetZoneID();
	sdpcs->InstanceID = zone->GetInstanceID();

	worldserver.SendPacket(Pack);

	safe_delete(Pack);

	entity_list.RemoveCorpseByDBID(dbid);
}

void Client::BuryPlayerCorpses()
{
	database.BuryAllCharacterCorpses(CharacterID());
}

void Client::NotifyNewTitlesAvailable()
{
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_SetTitle, 0);
	QueuePacket(outapp);
	safe_delete(outapp);
}

uint32 Client::GetStartZone()
{
	return m_pp.binds[4].zoneId;
}

void Client::ShowSkillsWindow()
{
	const char *WindowTitle = "Skills";
	std::string WindowText;
	// using a map for easy alphabetizing of the skills list
	std::map<std::string, SkillUseTypes> Skills;
	std::map<std::string, SkillUseTypes>::iterator it;

	// this list of names must keep the same order as that in common/skills.h
	const char* SkillName[] = {"1H Blunt","1H Slashing","2H Blunt","2H Slashing","Abjuration","Alteration","Apply Poison","Archery",
		"Backstab","Bind Wound","Bash","Block","Brass Instruments","Channeling","Conjuration","Defense","Disarm","Disarm Traps","Divination",
		"Dodge","Double Attack","Dragon Punch","Dual Wield","Eagle Strike","Evocation","Feign Death","Flying Kick","Forage","Hand to Hand",
		"Hide","Kick","Meditate","Mend","Offense","Parry","Pick Lock","Piercing","Ripost","Round Kick","Safe Fall","Sense Heading",
		"Singing","Sneak","Specialize Abjuration","Specialize Alteration","Specialize Conjuration","Specialize Divination","Specialize Evocation","Pick Pockets",
		"Stringed Instruments","Swimming","Throwing","Tiger Claw","Tracking","Wind Instruments","Fishing","Make Poison","Tinkering","Research",
		"Alchemy","Baking","Tailoring","Sense Traps","Blacksmithing","Fletching","Brewing","Alcohol Tolerance","Begging","Jewelry Making",
		"Pottery","Percussion Instruments","Intimidation","Berserking","Taunt","Frenzy"};
	for(int i = 0; i <= (int)HIGHEST_SKILL; i++)
		Skills[SkillName[i]] = (SkillUseTypes)i;

	// print out all available skills
	for(it = Skills.begin(); it != Skills.end(); ++it) {
		if(GetSkill(it->second) > 0 || MaxSkill(it->second) > 0) {
			WindowText += it->first;
			// line up the values
			WindowText += itoa(this->GetSkill(it->second));
			if (MaxSkill(it->second) > 0) {
				WindowText += "/";
				WindowText += itoa(this->GetMaxSkillAfterSpecializationRules(it->second,this->MaxSkill(it->second)));
			}
			WindowText += "<br>";
		}
	}
	this->Message(0,"%s",WindowText.c_str());
}


void Client::SetShadowStepExemption(bool v)
{
	if(v == true)
	{
		uint32 cur_time = Timer::GetCurrentTime();
		if((cur_time - m_TimeSinceLastPositionCheck) > 1000)
		{
			float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
			float runs = GetRunspeed();
			if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
			{
				printf("%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
					m_DistanceSinceLastPositionCheck, (cur_time - m_TimeSinceLastPositionCheck), speed);
				if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
				{
					if(IsShadowStepExempted())
					{
						if(m_DistanceSinceLastPositionCheck > 800)
						{
							CheatDetected(MQWarpShadowStep, GetX(), GetY(), GetZ());
						}
					}
					else if(IsKnockBackExempted())
					{
						//still potential to trigger this if you're knocked back off a
						//HUGE fall that takes > 2.5 seconds
						if(speed > 30.0f)
						{
							CheatDetected(MQWarpKnockBack, GetX(), GetY(), GetZ());
						}
					}
					else if(!IsPortExempted())
					{
						if(!IsMQExemptedArea(zone->GetZoneID(), GetX(), GetY(), GetZ()))
						{
							if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
							{
								CheatDetected(MQWarp, GetX(), GetY(), GetZ());
								m_TimeSinceLastPositionCheck = cur_time;
								m_DistanceSinceLastPositionCheck = 0.0f;
								//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
							}
							else
							{
								CheatDetected(MQWarpLight, GetX(), GetY(), GetZ());
							}
						}
					}
				}
			}
		}
		m_TimeSinceLastPositionCheck = cur_time;
		m_DistanceSinceLastPositionCheck = 0.0f;
	}
	m_ShadowStepExemption = v;
}

void Client::SetKnockBackExemption(bool v)
{
	if(v == true)
	{
		uint32 cur_time = Timer::GetCurrentTime();
		if((cur_time - m_TimeSinceLastPositionCheck) > 1000)
		{
			float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
			float runs = GetRunspeed();
			if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
			{
				if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
				{
					printf("%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
					m_DistanceSinceLastPositionCheck, (cur_time - m_TimeSinceLastPositionCheck), speed);
					if(IsShadowStepExempted())
					{
						if(m_DistanceSinceLastPositionCheck > 800)
						{
							CheatDetected(MQWarpShadowStep, GetX(), GetY(), GetZ());
						}
					}
					else if(IsKnockBackExempted())
					{
						//still potential to trigger this if you're knocked back off a
						//HUGE fall that takes > 2.5 seconds
						if(speed > 30.0f)
						{
							CheatDetected(MQWarpKnockBack, GetX(), GetY(), GetZ());
						}
					}
					else if(!IsPortExempted())
					{
						if(!IsMQExemptedArea(zone->GetZoneID(), GetX(), GetY(), GetZ()))
						{
							if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
							{
								m_TimeSinceLastPositionCheck = cur_time;
								m_DistanceSinceLastPositionCheck = 0.0f;
								CheatDetected(MQWarp, GetX(), GetY(), GetZ());
								//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
							}
							else
							{
								CheatDetected(MQWarpLight, GetX(), GetY(), GetZ());
							}
						}
					}
				}
			}
		}
		m_TimeSinceLastPositionCheck = cur_time;
		m_DistanceSinceLastPositionCheck = 0.0f;
	}
	m_KnockBackExemption = v;
}

void Client::SetPortExemption(bool v)
{
	if(v == true)
	{
		uint32 cur_time = Timer::GetCurrentTime();
		if((cur_time - m_TimeSinceLastPositionCheck) > 1000)
		{
			float speed = (m_DistanceSinceLastPositionCheck * 100) / (float)(cur_time - m_TimeSinceLastPositionCheck);
			float runs = GetRunspeed();
			if(speed > (runs * RuleR(Zone, MQWarpDetectionDistanceFactor)))
			{
				if(!GetGMSpeed() && (runs >= GetBaseRunspeed() || (speed > (GetBaseRunspeed() * RuleR(Zone, MQWarpDetectionDistanceFactor)))))
				{
					printf("%s %i moving too fast! moved: %.2f in %ims, speed %.2f\n", __FILE__, __LINE__,
					m_DistanceSinceLastPositionCheck, (cur_time - m_TimeSinceLastPositionCheck), speed);
					if(IsShadowStepExempted())
					{
						if(m_DistanceSinceLastPositionCheck > 800)
						{
								CheatDetected(MQWarpShadowStep, GetX(), GetY(), GetZ());
						}
					}
					else if(IsKnockBackExempted())
					{
						//still potential to trigger this if you're knocked back off a
						//HUGE fall that takes > 2.5 seconds
						if(speed > 30.0f)
						{
							CheatDetected(MQWarpKnockBack, GetX(), GetY(), GetZ());
						}
					}
					else if(!IsPortExempted())
					{
						if(!IsMQExemptedArea(zone->GetZoneID(), GetX(), GetY(), GetZ()))
						{
							if(speed > (runs * 2 * RuleR(Zone, MQWarpDetectionDistanceFactor)))
							{
								m_TimeSinceLastPositionCheck = cur_time;
								m_DistanceSinceLastPositionCheck = 0.0f;
								CheatDetected(MQWarp, GetX(), GetY(), GetZ());
								//Death(this, 10000000, SPELL_UNKNOWN, _1H_BLUNT);
							}
							else
							{
								CheatDetected(MQWarpLight, GetX(), GetY(), GetZ());
							}
						}
					}
				}
			}
		}
		m_TimeSinceLastPositionCheck = cur_time;
		m_DistanceSinceLastPositionCheck = 0.0f;
	}
	m_PortExemption = v;
}

void Client::Signal(uint32 data)
{
	char buf[32];
	snprintf(buf, 31, "%d", data);
	buf[31] = '\0';
	parse->EventPlayer(EVENT_SIGNAL, this, buf, 0);
}

const bool Client::IsMQExemptedArea(uint32 zoneID, float x, float y, float z) const
{
	float max_dist = 90000;
	switch(zoneID)
	{
	case 2:
		{
			float delta = (x-(-713.6));
			delta *= delta;
			float distance = delta;
			delta = (y-(-160.2));
			delta *= delta;
			distance += delta;
			delta = (z-(-12.8));
			delta *= delta;
			distance += delta;

			if(distance < max_dist)
				return true;

			delta = (x-(-153.8));
			delta *= delta;
			distance = delta;
			delta = (y-(-30.3));
			delta *= delta;
			distance += delta;
			delta = (z-(8.2));
			delta *= delta;
			distance += delta;

			if(distance < max_dist)
				return true;

			break;
		}
	case 9:
	{
		float delta = (x-(-682.5));
		delta *= delta;
		float distance = delta;
		delta = (y-(147.0));
		delta *= delta;
		distance += delta;
		delta = (z-(-9.9));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-655.4));
		delta *= delta;
		distance = delta;
		delta = (y-(10.5));
		delta *= delta;
		distance += delta;
		delta = (z-(-51.8));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		break;
	}
	case 62:
	case 75:
	case 114:
	case 209:
	{
		//The portals are so common in paineel/felwitheb that checking
		//distances wouldn't be worth it cause unless you're porting to the
		//start field you're going to be triggering this and that's a level of
		//accuracy I'm willing to sacrifice
		return true;
		break;
	}

	case 24:
	{
		float delta = (x-(-183.0));
		delta *= delta;
		float distance = delta;
		delta = (y-(-773.3));
		delta *= delta;
		distance += delta;
		delta = (z-(54.1));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-8.8));
		delta *= delta;
		distance = delta;
		delta = (y-(-394.1));
		delta *= delta;
		distance += delta;
		delta = (z-(41.1));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-310.3));
		delta *= delta;
		distance = delta;
		delta = (y-(-1411.6));
		delta *= delta;
		distance += delta;
		delta = (z-(-42.8));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		delta = (x-(-183.1));
		delta *= delta;
		distance = delta;
		delta = (y-(-1409.8));
		delta *= delta;
		distance += delta;
		delta = (z-(37.1));
		delta *= delta;
		distance += delta;

		if(distance < max_dist)
			return true;

		break;
	}

	case 110:
	case 34:
	case 96:
	case 93:
	case 68:
	case 84:
		{
			if(GetBoatID() != 0)
				return true;
			break;
		}
	default:
		break;
	}
	return false;
}

void Client::SuspendMinion()
{
	NPC *CurrentPet = GetPet()->CastToNPC();

	int AALevel = GetAA(aaSuspendedMinion);

	if(AALevel == 0)
		return;

	if(GetLevel() < 62)
		return;

	if(!CurrentPet)
	{
		if(m_suspendedminion.SpellID > 0)
		{
			MakePoweredPet(m_suspendedminion.SpellID, spells[m_suspendedminion.SpellID].teleport_zone,
				m_suspendedminion.petpower, m_suspendedminion.Name, m_suspendedminion.size);

			CurrentPet = GetPet()->CastToNPC();

			if(!CurrentPet)
			{
				Message(CC_Red, "Failed to recall suspended minion.");
				return;
			}

			if(AALevel >= 2)
			{
				CurrentPet->SetPetState(m_suspendedminion.Buffs, m_suspendedminion.Items);

			}
			CurrentPet->CalcBonuses();

			CurrentPet->SetHP(m_suspendedminion.HP);

			CurrentPet->SetMana(m_suspendedminion.Mana);

			Message_StringID(clientMessageTell, SUSPEND_MINION_UNSUSPEND, CurrentPet->GetCleanName());

			memset(&m_suspendedminion, 0, sizeof(struct PetInfo));
		}
		else
			return;

	}
	else
	{
		uint16 SpellID = CurrentPet->GetPetSpellID();

		if(SpellID)
		{
			if(m_suspendedminion.SpellID > 0)
			{
				Message_StringID(clientMessageError,ONLY_ONE_PET);

				return;
			}
			else if(CurrentPet->IsEngaged())
			{
				Message_StringID(clientMessageError,SUSPEND_MINION_FIGHTING);

				return;
			}
			else if(entity_list.Fighting(CurrentPet))
			{
				Message_StringID(clientMessageBlue,SUSPEND_MINION_HAS_AGGRO);
			}
			else
			{
				m_suspendedminion.SpellID = SpellID;

				m_suspendedminion.HP = CurrentPet->GetHP();

				m_suspendedminion.Mana = CurrentPet->GetMana();
				m_suspendedminion.petpower = CurrentPet->GetPetPower();
				m_suspendedminion.size = CurrentPet->GetSize();

				if(AALevel >= 2)
					CurrentPet->GetPetState(m_suspendedminion.Buffs, m_suspendedminion.Items, m_suspendedminion.Name);
				else
					strn0cpy(m_suspendedminion.Name, CurrentPet->GetName(), 64); // Name stays even at rank 1

				Message_StringID(clientMessageTell, SUSPEND_MINION_SUSPEND, CurrentPet->GetCleanName());

				CurrentPet->Depop(false);

				SetPetID(0);
			}
		}
		else
		{
			Message_StringID(clientMessageError, ONLY_SUMMONED_PETS);

			return;
		}
	}
}

// Processes a client request to inspect a SoF+ client's equipment.
void Client::ProcessInspectRequest(Client* requestee, Client* requester) {
	if(requestee && requester) {
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_InspectAnswer, sizeof(InspectResponse_Struct));
		InspectResponse_Struct* insr = (InspectResponse_Struct*) outapp->pBuffer;
		insr->TargetID = requester->GetID();
		insr->playerid = requestee->GetID();

		const Item_Struct* item = nullptr;
		const ItemInst* inst = nullptr;
		for(int16 L = 0; L <= 20; L++) {
			inst = requestee->GetInv().GetItem(L);

			if(inst) {
				item = inst->GetItem();
				if(item) {
					strcpy(insr->itemnames[L], item->Name);
					insr->itemicons[L] = item->Icon;
				}
				else
					insr->itemicons[L] = 0xFFFFFFFF;
			}
		}

		inst = requestee->GetInv().GetItem(MainPowerSource);

		if(inst) {
			item = inst->GetItem();
			if(item) {
				strcpy(insr->itemnames[21], item->Name);
				insr->itemicons[21] = item->Icon;
			}
			else
				insr->itemicons[21] = 0xFFFFFFFF;
		}

		inst = requestee->GetInv().GetItem(21);

		if(inst) {
			item = inst->GetItem();
			if(item) {
				strcpy(insr->itemnames[22], item->Name);
				insr->itemicons[22] = item->Icon;
			}
			else
				insr->itemicons[22] = 0xFFFFFFFF;
		}

		// There could be an OP for this..or not... (Ti clients are not processed here..this message is generated client-side)
		if(requestee->IsClient() && (requestee != requester)) { requestee->Message(0, "%s is looking at your equipment...", requester->GetName()); }

		requester->QueuePacket(outapp); // Send answer to requester
		safe_delete(outapp);
	}
}

void Client::CheckEmoteHail(Mob *target, const char* message)
{
	if(
		(message[0] != 'H'	&&
		message[0] != 'h')	||
		message[1] != 'a'	||
		message[2] != 'i'	||
		message[3] != 'l'){
		return;
	}

	if(!target || !target->IsNPC())
	{
		return;
	}

	if(target->GetOwnerID() != 0)
	{
		return;
	}
	uint16 emoteid = target->GetEmoteID();
	if(emoteid != 0)
		target->CastToNPC()->DoNPCEmote(HAILED,emoteid);
}

void Client::SendZonePoints()
{
	int count = 0;
	LinkedListIterator<ZonePoint*> iterator(zone->zone_point_list);
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* data = iterator.GetData();
		if(GetClientVersionBit() & data->client_version_mask)
		{
			count++;
		}
		iterator.Advance();
	}

	uint32 zpsize = sizeof(ZonePoints) + ((count + 1) * sizeof(ZonePoint_Entry));
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SendZonepoints, zpsize);
	ZonePoints* zp = (ZonePoints*)outapp->pBuffer;
	zp->count = count;

	int i = 0;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* data = iterator.GetData();
		data->target_zone_instance = 0;
		if(GetClientVersionBit() & data->client_version_mask)
		{
			zp->zpe[i].iterator = data->number;
			zp->zpe[i].x = data->target_x;
			zp->zpe[i].y = data->target_y;
			zp->zpe[i].z = data->target_z;
			zp->zpe[i].heading = data->target_heading;
			zp->zpe[i].zoneid = data->target_zone_id;
			zp->zpe[i].zoneinstance = data->target_zone_instance;
			i++;
		}
		iterator.Advance();
	}
	FastQueuePacket(&outapp);
}

void Client::SendTargetCommand(uint32 EntityID)
{
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_TargetCommand, sizeof(ClientTarget_Struct));
	ClientTarget_Struct *cts = (ClientTarget_Struct*)outapp->pBuffer;
	cts->new_target = EntityID;
	FastQueuePacket(&outapp);
}

void Client::LocateCorpse()
{
	Corpse *ClosestCorpse = nullptr;
	if(!GetTarget())
		ClosestCorpse = entity_list.GetClosestCorpse(this, nullptr);
	else if(GetTarget()->IsCorpse())
		ClosestCorpse = entity_list.GetClosestCorpse(this, GetTarget()->CastToCorpse()->GetOwnerName());
	else
		ClosestCorpse = entity_list.GetClosestCorpse(this, GetTarget()->GetCleanName());

	if(ClosestCorpse)
	{
		Message_StringID(MT_Spells, SENSE_CORPSE_DIRECTION);
		SetHeading(CalculateHeadingToTarget(ClosestCorpse->GetX(), ClosestCorpse->GetY()));
		SetTarget(ClosestCorpse);
		SendTargetCommand(ClosestCorpse->GetID());
		SendPosUpdate(2);
	}
	else if(!GetTarget())
		Message_StringID(clientMessageError, SENSE_CORPSE_NONE);
	else
		Message_StringID(clientMessageError, SENSE_CORPSE_NOT_NAME);
}

void Client::NPCSpawn(NPC *target_npc, const char *identifier, uint32 extra)
{
	if (!target_npc || !identifier)
		return;

	std::string id = identifier;
	for(int i = 0; i < id.length(); ++i)
	{
		id[i] = tolower(id[i]);
	}

	if (id == "create") {
		// extra tries to create the npc_type ID within the range for the current zone (zone_id * 1000)
		database.NPCSpawnDB(0, zone->GetShortName(), zone->GetInstanceVersion(), this, target_npc->CastToNPC(), extra);
	}
	else if (id == "add") {
		// extra sets the respawn timer for add
		database.NPCSpawnDB(1, zone->GetShortName(), zone->GetInstanceVersion(), this, target_npc->CastToNPC(), extra);
	}
	else if (id == "update") {
		database.NPCSpawnDB(2, zone->GetShortName(), zone->GetInstanceVersion(), this, target_npc->CastToNPC());
	}
	else if (id == "remove") {
		database.NPCSpawnDB(3, zone->GetShortName(), zone->GetInstanceVersion(), this, target_npc->CastToNPC());
		target_npc->Depop(false);
	}
	else if (id == "delete") {
		database.NPCSpawnDB(4, zone->GetShortName(), zone->GetInstanceVersion(), this, target_npc->CastToNPC());
		target_npc->Depop(false);
	}
	else {
		return;
	}
}

bool Client::IsDraggingCorpse(uint16 CorpseID)
{
	for (auto It = DraggedCorpses.begin(); It != DraggedCorpses.end(); ++It) {
		if (It->second == CorpseID)
			return true;
	}

	return false;
}

void Client::DragCorpses()
{
	for (auto It = DraggedCorpses.begin(); It != DraggedCorpses.end(); ++It) {
		Mob *corpse = entity_list.GetMob(It->second);

		if (corpse && corpse->IsPlayerCorpse() &&
				(DistanceSquaredNoZ(m_Position, corpse->GetPosition()) <= RuleR(Character, DragCorpseDistance)))
			continue;

		if (!corpse || !corpse->IsPlayerCorpse() ||
				corpse->CastToCorpse()->IsBeingLooted() ||
				!corpse->CastToCorpse()->Summon(this, false, false)) {
			Message_StringID(MT_DefaultText, CORPSEDRAG_STOP);
			It = DraggedCorpses.erase(It);
		}
	}
}

void Client::Doppelganger(uint16 spell_id, Mob *target, const char *name_override, int pet_count, int pet_duration)
{
	if(!target || !IsValidSpell(spell_id) || this->GetID() == target->GetID())
		return;

	PetRecord record;
	if(!database.GetPetEntry(spells[spell_id].teleport_zone, &record))
	{
		Log.Out(Logs::General, Logs::Error, "Unknown doppelganger spell id: %d, check pets table", spell_id);
		Message(CC_Red, "Unable to find data for pet %s", spells[spell_id].teleport_zone);
		return;
	}

	AA_SwarmPet pet;
	pet.count = pet_count;
	pet.duration = pet_duration;
	pet.npc_id = record.npc_type;

	NPCType *made_npc = nullptr;

	const NPCType *npc_type = database.GetNPCType(pet.npc_id);
	if(npc_type == nullptr) {
		Log.Out(Logs::General, Logs::Error, "Unknown npc type for doppelganger spell id: %d", spell_id);
		Message(0,"Unable to find pet!");
		return;
	}
	// make a custom NPC type for this
	made_npc = new NPCType;
	memcpy(made_npc, npc_type, sizeof(NPCType));

	strcpy(made_npc->name, name_override);
	made_npc->level = GetLevel();
	made_npc->race = GetRace();
	made_npc->gender = GetGender();
	made_npc->size = GetSize();
	made_npc->AC = GetAC();
	made_npc->STR = GetSTR();
	made_npc->STA = GetSTA();
	made_npc->DEX = GetDEX();
	made_npc->AGI = GetAGI();
	made_npc->MR = GetMR();
	made_npc->FR = GetFR();
	made_npc->CR = GetCR();
	made_npc->DR = GetDR();
	made_npc->PR = GetPR();
	made_npc->Corrup = GetCorrup();
	// looks
	made_npc->texture = GetEquipmentMaterial(MaterialChest);
	made_npc->helmtexture = GetEquipmentMaterial(MaterialHead);
	made_npc->haircolor = GetHairColor();
	made_npc->beardcolor = GetBeardColor();
	made_npc->eyecolor1 = GetEyeColor1();
	made_npc->eyecolor2 = GetEyeColor2();
	made_npc->hairstyle = GetHairStyle();
	made_npc->luclinface = GetLuclinFace();
	made_npc->beard = GetBeard();
	made_npc->d_melee_texture1 = GetEquipmentMaterial(MaterialPrimary);
	made_npc->d_melee_texture2 = GetEquipmentMaterial(MaterialSecondary);
	for (int i = EmuConstants::MATERIAL_BEGIN; i <= EmuConstants::MATERIAL_END; i++)	{
		made_npc->armor_tint[i] = GetEquipmentColor(i);
	}
	made_npc->loottable_id = 0;

	npc_type = made_npc;

	int summon_count = 0;
	summon_count = pet.count;

	if(summon_count > MAX_SWARM_PETS)
		summon_count = MAX_SWARM_PETS;

	static const glm::vec2 swarmPetLocations[MAX_SWARM_PETS] = {
		glm::vec2(5, 5), glm::vec2(-5, 5), glm::vec2(5, -5), glm::vec2(-5, -5),
		glm::vec2(10, 10), glm::vec2(-10, 10), glm::vec2(10, -10), glm::vec2(-10, -10),
		glm::vec2(8, 8), glm::vec2(-8, 8), glm::vec2(8, -8), glm::vec2(-8, -8)
	};

	while(summon_count > 0) {
		NPCType *npc_dup = nullptr;
		if(made_npc != nullptr) {
			npc_dup = new NPCType;
			memcpy(npc_dup, made_npc, sizeof(NPCType));
		}

		NPC* npca = new NPC(
				(npc_dup!=nullptr)?npc_dup:npc_type,	//make sure we give the NPC the correct data pointer
				0,
				GetPosition() + glm::vec4(swarmPetLocations[summon_count], 0.0f, 0.0f),
				FlyMode3);

		if(!npca->GetSwarmInfo()){
			AA_SwarmPetInfo* nSI = new AA_SwarmPetInfo;
			npca->SetSwarmInfo(nSI);
			npca->GetSwarmInfo()->duration = new Timer(pet_duration*1000);
		}
		else{
			npca->GetSwarmInfo()->duration->Start(pet_duration*1000);
		}

		npca->GetSwarmInfo()->owner_id = GetID();

		// Give the pets alittle more agro than the caster and then agro them on the target
		target->AddToHateList(npca, (target->GetHateAmount(this) + 100), (target->GetDamageAmount(this) + 100));
		npca->AddToHateList(target, 1000, 1000);
		npca->GetSwarmInfo()->target = target->GetID();

		//we allocated a new NPC type object, give the NPC ownership of that memory
		if(npc_dup != nullptr)
			npca->GiveNPCTypeData(npc_dup);

		entity_list.AddNPC(npca);
		summon_count--;
	}
}

void Client::AssignToInstance(uint16 instance_id)
{
	database.AddClientToInstance(instance_id, CharacterID());
}

void Client::SendStatsWindow(Client* client, bool use_window)
{
	// Define the types of page breaks we need
	std::string indP = "&nbsp;";
	std::string indS = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
	std::string indM = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
	std::string indL = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
	std::string div = " | ";

	std::string color_red = "<c \"#993333\">";
	std::string color_blue = "<c \"#9999FF\">";
	std::string color_green = "<c \"#33FF99\">";
	std::string bright_green = "<c \"#7CFC00\">";
	std::string bright_red = "<c \"#FF0000\">";
	std::string heroic_color = "<c \"#d6b228\"> +";

	// Set Class
	std::string class_Name = itoa(GetClass());
	std::string class_List[] = { "WAR", "CLR", "PAL", "RNG", "SK", "DRU", "MNK", "BRD", "ROG", "SHM", "NEC", "WIZ", "MAG", "ENC", "BST", "BER" };

	if(GetClass() < 17 && GetClass() > 0) { class_Name = class_List[GetClass()-1]; }

	// Race
	std::string race_Name = itoa(GetRace());
	switch(GetRace())
	{
		case 1: race_Name = "Human";		break;
		case 2:	race_Name = "Barbarian";	break;
		case 3:	race_Name = "Erudite";		break;
		case 4:	race_Name = "Wood Elf";		break;
		case 5:	race_Name = "High Elf";		break;
		case 6:	race_Name = "Dark Elf";		break;
		case 7:	race_Name = "Half Elf";		break;
		case 8:	race_Name = "Dwarf";		break;
		case 9:	race_Name = "Troll";		break;
		case 10: race_Name = "Ogre";		break;
		case 11: race_Name = "Halfing";		break;
		case 12: race_Name = "Gnome";		break;
		case 128: race_Name = "Iksar";		break;
		case 130: race_Name = "Vah Shir";	break;
		case 330: race_Name = "Froglok";	break;
		default: break;
	}
	/*##########################################################
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		H/M/E String
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	##########################################################*/
	std::string HME_row = "";
	//Loop Variables
	/*===========================*/
	std::string		cur_field = "";
	std::string		total_field = "";
	std::string		cur_name = "";
	std::string		cur_spacing = "";
	std::string		cur_color = "";

	int				hme_rows = 3; // Rows in display
	int				max_HME_value_len = 9; // 9 digits in the displayed value

	for(int hme_row_counter = 0; hme_row_counter < hme_rows; hme_row_counter++)
	{
		switch(hme_row_counter) {
			case 0: {
				cur_name = " H: ";
				cur_field = itoa(GetHP());
				total_field = itoa(GetMaxHP());
				break;
			}
			case 1: {
				if(CalcMaxMana() > 0) {
					cur_name = " M: ";
					cur_field = itoa(GetMana());
					total_field = itoa(CalcMaxMana());
				}
				else { continue; }

				break;
			}
			case 2: {
				cur_name = " E: ";
				cur_field = itoa(GetEndurance());
				total_field = itoa(GetMaxEndurance());
				break;
			}
			default: { break; }
		}
		if(cur_field.compare(total_field) == 0) { cur_color = bright_green; }
		else { cur_color = bright_red; }

		cur_spacing.clear();
		for(int a = cur_field.size(); a < max_HME_value_len; a++) { cur_spacing += " ."; }

		HME_row += indM + cur_name + cur_spacing + cur_color + cur_field + "</c> / " + total_field + "<br>";
	}
	/*##########################################################
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		Regen String
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	##########################################################*/
	std::string regen_string;
	//Loop Variables
	/*===========================*/
	std::string		regen_row_header = "";
	std::string		regen_row_color = "";
		std::string		base_regen_field = "";
	std::string		base_regen_spacing = "";
	std::string		item_regen_field = "";
	std::string		item_regen_spacing = "";
	std::string		cap_regen_field = "";
	std::string		cap_regen_spacing = "";
	std::string		spell_regen_field = "";
	std::string		spell_regen_spacing = "";
	std::string		aa_regen_field = "";
	std::string		aa_regen_spacing = "";
	std::string		total_regen_field = "";
	int		regen_rows = 3; // Number of rows
	int		max_regen_value_len = 5; // 5 digits in the displayed value(larger values will not get cut off, this is just a baseline)

	for(int regen_row_counter = 0; regen_row_counter < regen_rows; regen_row_counter++)
	{
		switch(regen_row_counter)
		{
			case 0: {
				regen_row_header = "H: ";
				regen_row_color = color_red;

				base_regen_field = itoa(LevelRegen());
				item_regen_field = itoa(itembonuses.HPRegen);
				cap_regen_field = itoa(CalcHPRegenCap());
				spell_regen_field = itoa(spellbonuses.HPRegen);
				aa_regen_field = itoa(aabonuses.HPRegen);
				total_regen_field = itoa(CalcHPRegen());
				break;
			}
			case 1: {
				if(CalcMaxMana() > 0) {
					regen_row_header = "M: ";
					regen_row_color = color_blue;

					base_regen_field = itoa(CalcBaseManaRegen());
					item_regen_field = itoa(itembonuses.ManaRegen);
					cap_regen_field = itoa(CalcManaRegenCap());
					spell_regen_field = itoa(spellbonuses.ManaRegen);
					aa_regen_field = itoa(aabonuses.ManaRegen);
					total_regen_field = itoa(CalcManaRegen());
				}
				else { continue; }
				break;
			}
			case 2: {
				regen_row_header = "E: ";
				regen_row_color = color_green;

				base_regen_field = itoa(((GetLevel() * 4 / 10) + 2));
				item_regen_field = itoa(itembonuses.EnduranceRegen);
				cap_regen_field = itoa(CalcEnduranceRegenCap());
				spell_regen_field = itoa(spellbonuses.EnduranceRegen);
				aa_regen_field = itoa(aabonuses.EnduranceRegen);
				total_regen_field = itoa(CalcEnduranceRegen());
				break;
			}
			default: { break; }
		}

		base_regen_spacing.clear();
		item_regen_spacing.clear();
		cap_regen_spacing.clear();
		spell_regen_spacing.clear();
		aa_regen_spacing.clear();

		for(int b = base_regen_field.size(); b < max_regen_value_len; b++) { base_regen_spacing += " ."; }
		for(int b = item_regen_field.size(); b < max_regen_value_len; b++) { item_regen_spacing += " ."; }
		for(int b = cap_regen_field.size(); b < max_regen_value_len; b++) { cap_regen_spacing += " ."; }
		for(int b = spell_regen_field.size(); b < max_regen_value_len; b++) { spell_regen_spacing += " ."; }
		for(int b = aa_regen_field.size(); b < max_regen_value_len; b++) { aa_regen_spacing += " ."; }

		regen_string += indS + regen_row_color + regen_row_header + base_regen_spacing + base_regen_field;
		regen_string += div + item_regen_spacing + item_regen_field + " (" + cap_regen_field;
		regen_string += ") " + cap_regen_spacing + div + spell_regen_spacing + spell_regen_field;
		regen_string += div + aa_regen_spacing + aa_regen_field + div + total_regen_field + "</c><br>";
	}
	/*##########################################################
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		Stat String
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	##########################################################*/
	std::string stat_field = "";
	//Loop Variables
	/*===========================*/
	//first field(stat)
	std::string		a_stat = "";;
	std::string		a_stat_name = "";
	std::string		a_stat_spacing = "";
	//second field(heroic stat)
	std::string		h_stat = "";
	std::string		h_stat_spacing = "";
	//third field(resist)
	std::string		a_resist = "";
	std::string		a_resist_name = "";
	std::string		a_resist_spacing = "";
	//fourth field(heroic resist)
	std::string		h_resist_field = "";

	int				stat_rows = 7; // Number of rows
	int				max_stat_value_len = 3; // 3 digits in the displayed value

	for(int stat_row_counter = 0; stat_row_counter < stat_rows; stat_row_counter++)
	{
		switch(stat_row_counter) {
			case 0: {
				a_stat_name = " STR: ";
				a_resist_name = "MR: ";
				a_stat = itoa(GetSTR());
				h_stat = itoa(GetHeroicSTR());
				a_resist = itoa(GetMR());
				h_resist_field = itoa(GetHeroicMR());
				break;
			}
			case 1: {
				a_stat_name = " STA: ";
				a_resist_name = "CR: ";
				a_stat = itoa(GetSTA());
				h_stat = itoa(GetHeroicSTA());
				a_resist = itoa(GetCR());
				h_resist_field = itoa(GetHeroicCR());
				break;
			}
			case 2: {
				a_stat_name = " AGI : ";
				a_resist_name = "FR: ";
				a_stat = itoa(GetAGI());
				h_stat = itoa(GetHeroicAGI());
				a_resist = itoa(GetFR());
				h_resist_field = itoa(GetHeroicFR());
				break;
			}
			case 3: {
				a_stat_name = " DEX: ";
				a_resist_name = "PR: ";
				a_stat = itoa(GetDEX());
				h_stat = itoa(GetHeroicDEX());
				a_resist = itoa(GetPR());
				h_resist_field = itoa(GetHeroicPR());
				break;
			}
			case 4: {
				a_stat_name = " INT : ";
				a_resist_name = "DR: ";
				a_stat = itoa(GetINT());
				h_stat = itoa(GetHeroicINT());
				a_resist = itoa(GetDR());
				h_resist_field = itoa(GetHeroicDR());
				break;
			}
			case 5: {
				a_stat_name = " WIS: ";
				a_resist_name = "Cp: ";
				a_stat = itoa(GetWIS());
				h_stat = itoa(GetHeroicWIS());
				a_resist = itoa(GetCorrup());
				h_resist_field = itoa(GetHeroicCorrup());
				break;
			}
			case 6: {
				a_stat_name = " CHA: ";
				a_stat = itoa(GetCHA());
				h_stat = itoa(GetHeroicCHA());
				break;
			}
			default: { break; }
		}

		a_stat_spacing.clear();
		h_stat_spacing.clear();
		a_resist_spacing.clear();

		for(int a = a_stat.size(); a < max_stat_value_len; a++) { a_stat_spacing += " . "; }
		for(int h = h_stat.size(); h < 20; h++) { h_stat_spacing += " . "; }
		for(int h = a_resist.size(); h < max_stat_value_len; h++) { a_resist_spacing += " . "; }

		stat_field += indP + a_stat_name + a_stat_spacing + a_stat + heroic_color + h_stat + "</c>";
		if(stat_row_counter < 6) {
			stat_field += h_stat_spacing + a_resist_name + a_resist_spacing + a_resist + heroic_color + h_resist_field + "</c><br>";
		}
	}
	/*##########################################################
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		Mod2 String
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	##########################################################*/
	std::string mod2_field = "";
	//Loop Variables
	/*===========================*/
	std::string		mod2a = "";
	std::string		mod2a_name = "";
	std::string		mod2a_spacing = "";
	std::string		mod2a_cap = "";
	std::string		mod_row_spacing = "";
	std::string		mod2b = "";
	std::string		mod2b_name = "";
	std::string		mod2b_spacing = "";
	std::string		mod2b_cap = "";
	int				mod2a_space_count;
	int				mod2b_space_count;

	int		mod2_rows = 4;
	int		max_mod2_value_len = 3; // 3 digits in the displayed value

	for(int mod2_row_counter = 0; mod2_row_counter < mod2_rows; mod2_row_counter++)
	{
		switch (mod2_row_counter)
		{
			case 0: {
				mod2a_name = "Avoidance: ";
				mod2b_name = "Combat Effects: ";
				mod2a = itoa(GetAvoidance());
				mod2a_cap = itoa(RuleI(Character, ItemAvoidanceCap));
				mod2b = itoa(GetCombatEffects());
				mod2b_cap = itoa(RuleI(Character, ItemCombatEffectsCap));
				mod2a_space_count = 2;
				mod2b_space_count = 0;
				break;
			}
			case 1: {
				mod2a_name = "Accuracy: ";
				mod2b_name = "Strike Through: ";
				mod2a = itoa(GetAccuracy());
				mod2a_cap = itoa(RuleI(Character, ItemAccuracyCap));
				mod2b = itoa(GetStrikeThrough());
				mod2b_cap = itoa(RuleI(Character, ItemStrikethroughCap));
				mod2a_space_count = 3;
				mod2b_space_count = 1;
				break;
			}
			case 2: {
				mod2a_name = "Shielding: ";
				mod2b_name = "Spell Shielding: ";
				mod2a = itoa(GetShielding());
				mod2a_cap = itoa(RuleI(Character, ItemShieldingCap));
				mod2b = itoa(GetSpellShield());
				mod2b_cap = itoa(RuleI(Character, ItemSpellShieldingCap));
				mod2a_space_count = 2;
				mod2b_space_count = 1;
				break;
			}
			case 3: {
				mod2a_name = "Stun Resist: ";
				mod2b_name = "DoT Shielding: ";
				mod2a = itoa(GetStunResist());
				mod2a_cap = itoa(RuleI(Character, ItemStunResistCap));
				mod2b = itoa(GetDoTShield());
				mod2b_cap = itoa(RuleI(Character, ItemDoTShieldingCap));
				mod2a_space_count = 0;
				mod2b_space_count = 2;
				break;
			}
		}

		mod2a_spacing.clear();
		mod_row_spacing.clear();
		mod2b_spacing.clear();

		for(int a = mod2a.size(); a < (max_mod2_value_len + mod2a_space_count); a++) { mod2a_spacing += " . "; }
		for(int a = mod2a_cap.size(); a < 6 ; a++) { mod_row_spacing += " . "; }
		for(int a = mod2b.size(); a < (max_mod2_value_len + mod2b_space_count); a++) { mod2b_spacing += " . "; }

		mod2_field += indP + mod2a_name + mod2a_spacing + mod2a + " / " + mod2a_cap + mod_row_spacing;
		mod2_field += mod2b_name + mod2b_spacing + mod2b + " / " + mod2b_cap + "<br>";
	}

	uint32 rune_number = 0;
	uint32 magic_rune_number = 0;
	uint32 buff_count = GetMaxTotalSlots();
	for (int i=0; i < buff_count; i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			if (buffs[i].melee_rune > 0) { rune_number += buffs[i].melee_rune; }

			if (buffs[i].magic_rune > 0) { magic_rune_number += buffs[i].magic_rune; }
		}
	}

	int shield_ac = 0;
	GetRawACNoShield(shield_ac);

	std::string skill_list[] = {
		"1H Blunt","1H Slashing","2H Blunt","2H Slashing","Abjuration","Alteration","Apply Poison","Archery","Backstab","Bind Wound","Bash","Block","Brass Instruments","Channeling","Conjuration",
		"Defense","Disarm","Disarm Traps","Divination","Dodge","Double Attack","Dragon Punch","Dual Wield","Eagle Strike","Evocation","Feign Death","Flying Kick","Forage","Hand To Hand","Hide","Kick",
		"Meditate","Mend","Offense","Parry","Pick Lock","Piercing","Riposte","Round Kick","Safe Fall","Sense Heading","Singing","Sneak","Specialize Abjuration","Specialize Alteration","Specialize Conjuration",
		"Specialize Divination","Specialize Evocation","Pick Pockets","Stringed_Instruments","Swimming","Throwing","Tiger Claw","Tracking","Wind Instruments","Fishing","Make Poison","Tinkering","Research","Alchemy",
		"Baking","Tailoring","Sense Traps","Blacksmithing","Fletching","Brewing","Alcohol_Tolerance","Begging","Jewelry Making","Pottery","Percussion Instruments","Intimidation","Berserking","Taunt","Frenzy"
	};

	std::string skill_mods = "";
	for(int j = 0; j <= HIGHEST_SKILL; j++) {
		if(itembonuses.skillmod[j] > 0)
			skill_mods += indP + skill_list[j] + " : +" + itoa(itembonuses.skillmod[j]) + "%<br>";
		else if(itembonuses.skillmod[j] < 0)
			skill_mods += indP + skill_list[j] + " : -" + itoa(itembonuses.skillmod[j]) + "%<br>";
	}

	std::string skill_dmgs = "";
	for(int j = 0; j <= HIGHEST_SKILL; j++) {
		if((itembonuses.SkillDamageAmount[j] + spellbonuses.SkillDamageAmount[j]) > 0)
			skill_dmgs += indP + skill_list[j] + " : +" + itoa(itembonuses.SkillDamageAmount[j] + spellbonuses.SkillDamageAmount[j]) + "<br>";
		else if((itembonuses.SkillDamageAmount[j] + spellbonuses.SkillDamageAmount[j]) < 0)
			skill_dmgs += indP + skill_list[j] + " : -" + itoa(itembonuses.SkillDamageAmount[j] + spellbonuses.SkillDamageAmount[j]) + "<br>";
	}

	std::string faction_item_string = "";
	char faction_buf[256];

	for(std::map <uint32, int32>::iterator iter = item_faction_bonuses.begin();
		iter != item_faction_bonuses.end();
		++iter)
	{
		memset(&faction_buf, 0, sizeof(faction_buf));

		if(!database.GetFactionName((int32)((*iter).first), faction_buf, sizeof(faction_buf)))
			strcpy(faction_buf, "Not in DB");

		if((*iter).second > 0) {
			faction_item_string += indP + faction_buf + " : +" + itoa((*iter).second) + "<br>";
		}
		else if((*iter).second < 0) {
			faction_item_string += indP + faction_buf + " : -" + itoa((*iter).second) + "<br>";
		}
	}

	std::string bard_info = "";
	if(GetClass() == BARD) {
		bard_info = indP + "Singing: " + itoa(GetSingMod()) + "<br>" +
					indP + "Brass: " + itoa(GetBrassMod()) + "<br>" +
					indP + "String: " + itoa(GetStringMod()) + "<br>" +
					indP + "Percussion: " + itoa(GetPercMod()) + "<br>" +
					indP + "Wind: " + itoa(GetWindMod()) + "<br>";
	}

	std::ostringstream final_string;
					final_string <<
	/*	C/L/R	*/	indP << "Class: " << class_Name << indS << "Level: " << static_cast<int>(GetLevel()) << indS << "Race: " << race_Name << "<br>" <<
	/*	Runes	*/	indP << "Rune: " << rune_number << indL << indS << "Spell Rune: " << magic_rune_number << "<br>" <<
	/*	HP/M/E	*/	HME_row <<
	/*	DS		*/	indP << "DS: " << (itembonuses.DamageShield + spellbonuses.DamageShield*-1) << " (Spell: " << (spellbonuses.DamageShield*-1) << " + Item: " << itembonuses.DamageShield << " / " << RuleI(Character, ItemDamageShieldCap) << ")<br>" <<
	/*	Atk		*/	indP << "<c \"#CCFF00\">ATK: " << GetTotalATK() << "</c><br>" <<
	/*	Atk2	*/	indP << "- Base: " << GetATKRating() << " | Item: " << itembonuses.ATK << " (" << RuleI(Character, ItemATKCap) << ")~Used: " << (itembonuses.ATK * 1.342) << " | Spell: " << spellbonuses.ATK << "<br>" <<
	/*	AC		*/	indP << "<c \"#CCFF00\">AC: " << CalcAC() << "</c><br>" <<
	/*	AC2		*/	indP << "- Mit: " << GetACMit() << " | Avoid: " << GetACAvoid() << " | Spell: " << spellbonuses.AC << " | Shield: " << shield_ac << "<br>" <<
	/*	Haste	*/	indP << "<c \"#CCFF00\">Haste: " << GetHaste() << "</c><br>" <<
	/*	Haste2	*/	indP << " - Item: " << itembonuses.haste << " + Spell: " << (spellbonuses.haste + spellbonuses.hastetype2) << " (Cap: " << RuleI(Character, HasteCap) << ") | Over: " << (spellbonuses.hastetype3 + ExtraHaste) << "<br><br>" <<
	/* RegenLbl	*/	indL << indS << "Regen<br>" << indS << indP << indP << " Base | Items (Cap) " << indP << " | Spell | A.A.s | Total<br>" <<
	/*	Regen	*/	regen_string << "<br>" <<
	/*	Stats	*/	stat_field << "<br><br>" <<
	/*	Mod2s	*/	mod2_field << "<br>" <<
	/*	HealAmt	*/	indP << "Heal Amount: " << GetHealAmt() << " / " << RuleI(Character, ItemHealAmtCap) << "<br>" <<
	/*	SpellDmg*/	indP << "Spell Dmg: " << GetSpellDmg() << " / " << RuleI(Character, ItemSpellDmgCap) << "<br>" <<
	/*	Clair	*/	indP << "Clairvoyance: " << GetClair() << " / " << RuleI(Character, ItemClairvoyanceCap) << "<br>" <<
	/*	DSMit	*/	indP << "Dmg Shld Mit: " << GetDSMit() << " / " << RuleI(Character, ItemDSMitigationCap) << "<br><br>";
	if(GetClass() == BARD)
		final_string << bard_info << "<br>";
	if(skill_mods.size() > 0)
		final_string << skill_mods << "<br>";
	if(skill_dmgs.size() > 0)
		final_string << skill_dmgs << "<br>";
	if(faction_item_string.size() > 0)
		final_string << faction_item_string;

	std::string final_stats = final_string.str();

	if(use_window) {
		if(final_stats.size() < 4096)
		{
			uint32 Buttons = 1;
			goto Extra_Info;
		}
		else {
			client->Message(CC_Yellow, "The window has exceeded its character limit, displaying stats to chat window:");
		}
	}

	client->Message(CC_Yellow, "~~~~~ %s %s ~~~~~", GetCleanName(), GetLastName());
	client->Message(0, " Level: %i Class: %i Race: %i RaceBit: %i DS: %i/%i Size: %1.1f BaseSize: %1.1f Weight: %.1f/%d  ", GetLevel(), GetClass(), GetRace(), GetRaceBitmask(GetRace()), GetDS(), RuleI(Character, ItemDamageShieldCap), GetSize(), GetBaseSize(), (float)CalcCurrentWeight() / 10.0f, GetSTR());
	client->Message(0, " HP: %i/%i  HP Regen: %i/%i",GetHP(), GetMaxHP(), CalcHPRegen(), CalcHPRegenCap());
	client->Message(0, " AC: %i ( Mit.: %i + Avoid.: %i + Spell: %i ) | Shield AC: %i", CalcAC(), GetACMit(), GetACAvoid(), spellbonuses.AC, shield_ac);
	client->Message(0, " AFK: %i LFG: %i Anon: %i GM: %i Flymode: %i GMSpeed: %i Hideme: %i LD: %i ClientVersion: %i", AFK, LFG, GetAnon(), GetGM(), flymode, GetGMSpeed(), GetHideMe(), IsLD(), GetClientVersionBit());
	if(CalcMaxMana() > 0)
		client->Message(0, " Mana: %i/%i  Mana Regen: %i/%i", GetMana(), GetMaxMana(), CalcManaRegen(), CalcManaRegenCap());
	client->Message(0, "  X: %0.2f Y: %0.2f Z: %0.2f", GetX(), GetY(), GetZ());
	client->Message(0, " End.: %i/%i  End. Regen: %i/%i",GetEndurance(), GetMaxEndurance(), CalcEnduranceRegen(), CalcEnduranceRegenCap());
	client->Message(0, " ATK: %i  Worn/Spell ATK %i/%i  Server Side ATK: %i", GetTotalATK(), RuleI(Character, ItemATKCap), GetATKBonus(), GetATK());
	client->Message(0, " Haste: %i / %i (Item: %i + Spell: %i + Over: %i)", GetHaste(), RuleI(Character, HasteCap), itembonuses.haste, spellbonuses.haste + spellbonuses.hastetype2, spellbonuses.hastetype3 + ExtraHaste);
	client->Message(0, " STR: %i  STA: %i  DEX: %i  AGI: %i  INT: %i  WIS: %i  CHA: %i", GetSTR(), GetSTA(), GetDEX(), GetAGI(), GetINT(), GetWIS(), GetCHA());
	client->Message(0, " hSTR: %i  hSTA: %i  hDEX: %i  hAGI: %i  hINT: %i  hWIS: %i  hCHA: %i", GetHeroicSTR(), GetHeroicSTA(), GetHeroicDEX(), GetHeroicAGI(), GetHeroicINT(), GetHeroicWIS(), GetHeroicCHA());
	client->Message(0, " MR: %i  PR: %i  FR: %i  CR: %i  DR: %i Corruption: %i", GetMR(), GetPR(), GetFR(), GetCR(), GetDR(), GetCorrup());
	client->Message(0, " hMR: %i  hPR: %i  hFR: %i  hCR: %i  hDR: %i hCorruption: %i", GetHeroicMR(), GetHeroicPR(), GetHeroicFR(), GetHeroicCR(), GetHeroicDR(), GetHeroicCorrup());
	client->Message(0, " Shielding: %i  Spell Shield: %i  DoT Shielding: %i Stun Resist: %i  Strikethrough: %i  Avoidance: %i  Accuracy: %i  Combat Effects: %i", GetShielding(), GetSpellShield(), GetDoTShield(), GetStunResist(), GetStrikeThrough(), GetAvoidance(), GetAccuracy(), GetCombatEffects());
	client->Message(0, " Heal Amt.: %i  Spell Dmg.: %i  Clairvoyance: %i DS Mitigation: %i", GetHealAmt(), GetSpellDmg(), GetClair(), GetDSMit());
	client->Message(0, " Runspeed: %0.1f  Walkspeed: %0.1f Hunger: %i Thirst: %i Famished: %i Boat: %s (Ent %i : NPC %i)", GetRunspeed(), GetWalkspeed(), GetHunger(), GetThirst(), GetFamished(), GetBoatName(), GetBoatID(), GetBoatNPCID());
	if(GetClass() == WARRIOR)
		client->Message(0, "HasShield: %i KickDmg: %i BashDmg: %i", HasShieldEquiped(), GetKickDamage(), GetBashDamage());
	else if(GetClass() == RANGER || GetClass() == BEASTLORD)
		client->Message(0, "HasShield: %i KickDmg: %i", HasShieldEquiped(), GetKickDamage());
	else if(GetClass() == PALADIN || GetClass() == SHADOWKNIGHT)
		client->Message(0, "HasShield: %i BashDmg: %i", HasShieldEquiped(), GetBashDamage());
	else
		client->Message(0, "HasShield: %i", HasShieldEquiped());
	if(GetClass() == BARD)
		client->Message(0, " Singing: %i  Brass: %i  String: %i Percussion: %i Wind: %i", GetSingMod(), GetBrassMod(), GetStringMod(), GetPercMod(), GetWindMod());
	if(HasGroup())
	{
		Group* g = GetGroup();
		client->Message(0, " GroupID: %i Count: %i GroupLeader: %s GroupLeaderCached: %s", g->GetID(), g->GroupCount(), g->GetLeaderName(), g->GetOldLeaderName());
	}
	client->Message(0, " Hidden: %i ImpHide: %i Sneaking: %i Invisible: %i InvisVsUndead: %i InvisVsAnimals: %i", hidden, improved_hidden, sneaking, invisible, invisible_undead, invisible_animals);
	client->Message(0, " Feigned: %i Invulnerable: %i SeeInvis: %i HasZomm: %i", feigned, invulnerable, see_invis, has_zomm);

	Extra_Info:

	client->Message(0, " BaseRace: %i  Gender: %i  BaseGender: %i Texture: %i  HelmTexture: %i", GetBaseRace(), GetGender(), GetBaseGender(), GetTexture(), GetHelmTexture());
	if (client->Admin() >= 100) {
		client->Message(0, "  CharID: %i  EntityID: %i  PetID: %i  OwnerID: %i  AIControlled: %i  Targetted: %i", CharacterID(), GetID(), GetPetID(), GetOwnerID(), IsAIControlled(), targeted);
		uint32 xpneeded = GetEXPForLevel(GetLevel()+1) - GetEXP();
		int exploss;
		GetExpLoss(nullptr,0,exploss);
		if(GetLevel() < 10)
			exploss = 0;
		client->Message(0, "  CurrentXP: %i XP Needed: %i ExpLoss: %i CurrentAA: %i", GetEXP(), xpneeded, exploss, GetAAXP());
		client->Message(0, "  Last Update: %d Current Time: %d Difference: %d", pLastUpdate, Timer::GetCurrentTime(), Timer::GetCurrentTime() - pLastUpdate);
	}
}

const char* Client::GetRacePlural(Client* client) {

	switch (client->CastToMob()->GetRace()) {
		case HUMAN:
			return "Humans"; break;
		case BARBARIAN:
			return "Barbarians"; break;
		case ERUDITE:
			return "Erudites"; break;
		case WOOD_ELF:
			return "Wood Elves"; break;
		case HIGH_ELF:
			return "High Elves"; break;
		case DARK_ELF:
			return "Dark Elves"; break;
		case HALF_ELF:
			return "Half Elves"; break;
		case DWARF:
			return "Dwarves"; break;
		case TROLL:
			return "Trolls"; break;
		case OGRE:
			return "Ogres"; break;
		case HALFLING:
			return "Halflings"; break;
		case GNOME:
			return "Gnomes"; break;
		case IKSAR:
			return "Iksar"; break;
		case VAHSHIR:
			return "Vah Shir"; break;
		case FROGLOK:
			return "Frogloks"; break;
		default:
			return "Races"; break;
	}
}

const char* Client::GetClassPlural(Client* client) {

	switch (client->CastToMob()->GetClass()) {
		case WARRIOR:
			return "Warriors"; break;
		case CLERIC:
			return "Clerics"; break;
		case PALADIN:
			return "Paladins"; break;
		case RANGER:
			return "Rangers"; break;
		case SHADOWKNIGHT:
			return "Shadowknights"; break;
		case DRUID:
			return "Druids"; break;
		case MONK:
			return "Monks"; break;
		case BARD:
			return "Bards"; break;
		case ROGUE:
			return "Rogues"; break;
		case SHAMAN:
			return "Shamen"; break;
		case NECROMANCER:
			return "Necromancers"; break;
		case WIZARD:
			return "Wizards"; break;
		case MAGICIAN:
			return "Magicians"; break;
		case ENCHANTER:
			return "Enchanters"; break;
		case BEASTLORD:
			return "Beastlords"; break;
		default:
			return "Classes"; break;
	}
}

void Client::DuplicateLoreMessage(uint32 ItemID)
{
	Message_StringID(CC_Default, PICK_LORE);
	return;

	const Item_Struct *item = database.GetItem(ItemID);

	if(!item)
		return;

	Message_StringID(CC_Default, PICK_LORE, item->Name);
}

void Client::GarbleMessage(char *message, uint8 variance)
{
	// Garble message by variance%
	const char alpha_list[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; // only change alpha characters for now

	for (size_t i = 0; i < strlen(message); i++) {
		uint8 chance = (uint8)zone->random.Int(0, 115); // variation just over worst possible scrambling
		if (isalpha(message[i]) && (chance <= variance)) {
			uint8 rand_char = (uint8)zone->random.Int(0,51); // choose a random character from the alpha list
			message[i] = alpha_list[rand_char];
		}
	}
}

// returns what Other thinks of this
FACTION_VALUE Client::GetReverseFactionCon(Mob* iOther) {
	if (GetOwnerID()) {
		return GetOwnerOrSelf()->GetReverseFactionCon(iOther);
	}

	iOther = iOther->GetOwnerOrSelf();

	if (iOther->GetPrimaryFaction() < 0)
		return GetSpecialFactionCon(iOther);

	if (iOther->GetPrimaryFaction() == 0)
		return FACTION_INDIFFERENT;

	return GetFactionLevel(CharacterID(), 0, GetRace(), GetClass(), GetDeity(), iOther->GetPrimaryFaction(), iOther);
}

//o--------------------------------------------------------------
//| Name: GetFactionLevel; Dec. 16, 2001
//o--------------------------------------------------------------
//| Notes: Gets the characters faction standing with the specified NPC.
//|			Will return Indifferent on failure.
//o--------------------------------------------------------------
FACTION_VALUE Client::GetFactionLevel(uint32 char_id, uint32 npc_id, uint32 p_race, uint32 p_class, uint32 p_deity, int32 pFaction, Mob* tnpc)
{
	if (pFaction < 0)
		return GetSpecialFactionCon(tnpc);
	FACTION_VALUE fac = FACTION_INDIFFERENT;
	int32 tmpFactionValue;
	FactionMods fmods;

	// few optimizations
	if (GetFeigned())
		return FACTION_INDIFFERENT;
	if (invisible_undead && tnpc && !tnpc->SeeInvisibleUndead())
		return FACTION_INDIFFERENT;
	if (IsInvisible(tnpc))
		return FACTION_INDIFFERENT;
	if (tnpc && tnpc->GetOwnerID() != 0) // pets con amiably to owner and indiff to rest
	{
		if (tnpc->GetOwner()->IsClient() && char_id == tnpc->GetOwner()->CastToClient()->CharacterID())
			return FACTION_AMIABLE;
		else
			return FACTION_INDIFFERENT;
	}

	//First get the NPC's Primary faction
	if(pFaction > 0)
	{
		//Get the faction data from the database
		if(database.GetFactionData(&fmods, p_class, p_race, p_deity, pFaction))
		{
			//Get the players current faction with pFaction
			tmpFactionValue = GetCharacterFactionLevel(pFaction);
			//Tack on any bonuses from Alliance type spell effects
			tmpFactionValue += GetFactionBonus(pFaction);
			tmpFactionValue += GetItemFactionBonus(pFaction);
			//Return the faction to the client
			fac = CalculateFaction(&fmods, tmpFactionValue);
		}
	}
	else
	{
		return(FACTION_INDIFFERENT);
	}

	// merchant fix
	if (tnpc && tnpc->IsNPC() && tnpc->CastToNPC()->MerchantType && (fac == FACTION_THREATENLY || fac == FACTION_SCOWLS))
		fac = FACTION_DUBIOUS;

	if (tnpc != 0 && fac != FACTION_SCOWLS && tnpc->CastToNPC()->CheckAggro(this))
		fac = FACTION_THREATENLY;

	return fac;
}

//Sets the characters faction standing with the specified NPC.
void Client::SetFactionLevel(uint32 char_id, uint32 npc_id, uint8 char_class, uint8 char_race, uint8 char_deity, bool quest)
{
	int32 faction_id[MAX_NPC_FACTIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int32 npc_value[MAX_NPC_FACTIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	uint8 temp[MAX_NPC_FACTIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int32 mod;
	int32 tmpValue;
	int32 current_value;
	FactionMods fm;
	bool change = false;
	bool repair = false;

	// Get the npc faction list
	if (!database.GetNPCFactionList(npc_id, faction_id, npc_value, temp))
		return;
	for (int i = 0; i < MAX_NPC_FACTIONS; i++)
	{
		if (faction_id[i] <= 0)
			continue;

		// Get the faction modifiers
		if (database.GetFactionData(&fm, char_class, char_race, char_deity, faction_id[i]))
		{
			// Get the characters current value with that faction
			current_value = GetCharacterFactionLevel(faction_id[i]);

			if (quest)
			{
				//The ole switcheroo
				if (npc_value[i] > 0)
					npc_value[i] = -abs(npc_value[i]);
				else if (npc_value[i] < 0)
					npc_value[i] = abs(npc_value[i]);
			}

			if (this->itembonuses.HeroicCHA)
			{
				int faction_mod = itembonuses.HeroicCHA / 5;
				// If our result isn't truncated, then just do that
				if (npc_value[i] * faction_mod / 100 != 0)
					npc_value[i] += npc_value[i] * faction_mod / 100;
				// If our result is truncated, then double a mob's value every once and a while to equal what they would have got
				else
				{
					if (zone->random.Int(0, 100) < faction_mod)
						npc_value[i] *= 2;
				}
			}
			// Set flag when to update db
			if (!quest)
			{
				if (current_value > MAX_PERSONAL_FACTION)
				{
					current_value = MAX_PERSONAL_FACTION;
					repair = true;
				}
				else if (current_value < MIN_PERSONAL_FACTION)
				{
					current_value = MIN_PERSONAL_FACTION;
					repair = true;
				}
				else if ((m_pp.gm != 1) && (npc_value[i] != 0) && ((current_value != MAX_PERSONAL_FACTION) || (current_value != MIN_PERSONAL_FACTION)))
					change = true;
			}

			current_value += npc_value[i];

			if (current_value > MAX_PERSONAL_FACTION)
				current_value = MAX_PERSONAL_FACTION;
			else if (current_value < MIN_PERSONAL_FACTION)
				current_value = MIN_PERSONAL_FACTION;

			if (change || repair)
			{
				database.SetCharacterFactionLevel(char_id, faction_id[i], current_value, temp[i], factionvalues);

				if (change)
				{
					mod = fm.base + fm.class_mod + fm.race_mod + fm.deity_mod;
					tmpValue = current_value + mod + npc_value[i];
					SendFactionMessage(npc_value[i], faction_id[i], tmpValue, temp[i]);
				}
			}
		}
	}
	return;
}

void Client::SetFactionLevel2(uint32 char_id, int32 faction_id, uint8 char_class, uint8 char_race, uint8 char_deity, int32 value, uint8 temp)
{
	int32 current_value;
	//Get the npc faction list
	if(faction_id > 0 && value != 0) {
		//Get the faction modifiers
		current_value = GetCharacterFactionLevel(faction_id) + value;
		if(!(database.SetCharacterFactionLevel(char_id, faction_id, current_value, temp, factionvalues)))
			return;

		SendFactionMessage(value, faction_id, current_value, temp);
	}
	return;
}

int32 Client::GetCharacterFactionLevel(int32 faction_id)
{
	if (faction_id <= 0 || GetRaceBitmask(GetRace()) == 0)
	{
		Log.Out(Logs::Detail, Logs::Faction, "Race: %d is non base, ignoring character faction.", GetRace());
		return 0;
	}
	faction_map::iterator res;
	res = factionvalues.find(faction_id);
	if (res == factionvalues.end())
		return 0;
	return res->second;
}

// returns the character's faction level, adjusted for racial, class, and deity modifiers
int32 Client::GetModCharacterFactionLevel(int32 faction_id) {
	int32 Modded = GetCharacterFactionLevel(faction_id);
	FactionMods fm;
	if (database.GetFactionData(&fm, GetClass(), GetRace(), GetDeity(), faction_id))
		Modded += fm.base + fm.class_mod + fm.race_mod + fm.deity_mod;

	return Modded;
}

void Client::MerchantRejectMessage(Mob *merchant, int primaryfaction)
{
	int messageid = 0;
	int32 tmpFactionValue = 0;
	int32 lowestvalue = 0;
	FactionMods fmod;

	// If a faction is involved, get the data.
	if (primaryfaction > 0) {
		if (database.GetFactionData(&fmod, GetClass(), GetRace(), GetDeity(), primaryfaction)) {
			tmpFactionValue = GetCharacterFactionLevel(primaryfaction);
			lowestvalue = std::min(tmpFactionValue, std::min(fmod.class_mod, fmod.race_mod));
		}
	}
	// If no primary faction or biggest influence is your faction hit
	// Hack to get Shadowhaven messages correct :I
	if (GetZoneID() != shadowhaven && (primaryfaction <= 0 || lowestvalue == tmpFactionValue)) 
	{
		merchant->Say_StringID(zone->random.Int(WONT_SELL_DEEDS1, WONT_SELL_DEEDS6));
	} 
	//class biggest
	else if (lowestvalue == fmod.class_mod) 
	{
		merchant->Say_StringID(0, zone->random.Int(WONT_SELL_CLASS1, WONT_SELL_CLASS5), itoa(GetClassStringID()));
	}
	// race biggest/default
	else
	{ 
		// Non-standard race (ex. illusioned to wolf)
		if (GetRace() > PLAYER_RACE_COUNT) 
		{
			messageid = zone->random.Int(1, 3); // these aren't sequential StringIDs :(
			switch (messageid) {
			case 1:
				messageid = WONT_SELL_NONSTDRACE1;
				break;
			case 2:
				messageid = WONT_SELL_NONSTDRACE2;
				break;
			case 3:
				messageid = WONT_SELL_NONSTDRACE3;
				break;
			default: // w/e should never happen
				messageid = WONT_SELL_NONSTDRACE1;
				break;
			}
			merchant->Say_StringID(messageid);
		} 
		else 
		{ // normal player races
			messageid = zone->random.Int(1, 4);
			switch (messageid) {
			case 1:
				messageid = WONT_SELL_RACE1;
				break;
			case 2:
				messageid = WONT_SELL_RACE2;
				break;
			case 3:
				messageid = WONT_SELL_RACE3;
				break;
			case 4:
				messageid = WONT_SELL_RACE4;
				break;
			default: // w/e should never happen
				messageid = WONT_SELL_RACE1;
				break;
			}
			merchant->Say_StringID(0, messageid, itoa(GetRaceStringID()));
		}
	}
	return;
}

//o--------------------------------------------------------------
//| Name: SendFactionMessage
//o--------------------------------------------------------------
//| Purpose: Send faction change message to client
//o--------------------------------------------------------------
void Client::SendFactionMessage(int32 tmpvalue, int32 faction_id, int32 totalvalue, uint8 temp)
{
	char name[50];

	// default to Faction# if we couldn't get the name from the ID
	if (database.GetFactionName(faction_id, name, sizeof(name)) == false)
		snprintf(name, sizeof(name), "Faction%i", faction_id);

	//We need to get total faction here, including racial, class, and deity modifiers.
	int32 fac = GetModCharacterFactionLevel(faction_id) + tmpvalue;
	totalvalue = fac;

	if (tmpvalue == 0 || temp == 1 || temp == 2)
		return;
	else if (totalvalue >= MAX_PERSONAL_FACTION)
		Message_StringID(CC_Default, FACTION_BEST, name);
	else if (tmpvalue > 0 && totalvalue < MAX_PERSONAL_FACTION)
		Message_StringID(CC_Default, FACTION_BETTER, name);
	else if (tmpvalue < 0 && totalvalue > MIN_PERSONAL_FACTION)
		Message_StringID(CC_Default, FACTION_WORSE, name);
	else if (totalvalue <= MIN_PERSONAL_FACTION)
		Message_StringID(CC_Default, FACTION_WORST, name);

	return;
}

void Client::LoadAccountFlags()
{

	accountflags.clear();
	std::string query = StringFormat("SELECT p_flag, p_value "
                                    "FROM account_flags WHERE p_accid = '%d'",
                                    account_id);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

	for (auto row = results.begin(); row != results.end(); ++row)
		accountflags[row[0]] = row[1];
}

void Client::SetAccountFlag(std::string flag, std::string val) {

	std::string query = StringFormat("REPLACE INTO account_flags (p_accid, p_flag, p_value) "
									"VALUES( '%d', '%s', '%s')",
									account_id, flag.c_str(), val.c_str());
	auto results = database.QueryDatabase(query);
	if(!results.Success()) {
		return;
	}

	accountflags[flag] = val;
}

std::string Client::GetAccountFlag(std::string flag)
{
	return(accountflags[flag]);
}

void Client::TickItemCheck()
{
	int i;

	if(zone->tick_items.empty()) { return; }

	//Scan equip slots for items  + cursor
	for(i = MainCursor; i <= EmuConstants::EQUIPMENT_END; i++)
	{
		TryItemTick(i);
	}
	//Scan main inventory
	for (i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_END; i++)
	{
		TryItemTick(i);
	}
	//Scan bags
	for(i = EmuConstants::GENERAL_BAGS_BEGIN; i <= EmuConstants::CURSOR_BAG_END; i++)
	{
		TryItemTick(i);
	}
}

void Client::TryItemTick(int slot)
{
	int iid = 0;
	const ItemInst* inst = m_inv[slot];
	if(inst == 0) { return; }

	iid = inst->GetID();

	if(zone->tick_items.count(iid) > 0)
	{
		if( GetLevel() >= zone->tick_items[iid].level && zone->random.Int(0, 100) >= (100 - zone->tick_items[iid].chance) && (zone->tick_items[iid].bagslot || slot <= EmuConstants::EQUIPMENT_END) )
		{
			ItemInst* e_inst = (ItemInst*)inst;
			parse->EventItem(EVENT_ITEM_TICK, this, e_inst, nullptr, "", slot);
		}
	}

	//Only look at augs in main inventory
	if(slot > EmuConstants::EQUIPMENT_END) { return; }

}

void Client::ItemTimerCheck()
{
	int i;
	for(i = MainCursor; i <= EmuConstants::EQUIPMENT_END; i++)
	{
		TryItemTimer(i);
	}

	for (i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_END; i++)
	{
		TryItemTimer(i);
	}

	for(i = EmuConstants::GENERAL_BAGS_BEGIN; i <= EmuConstants::CURSOR_BAG_END; i++)
	{
		TryItemTimer(i);
	}
}

void Client::TryItemTimer(int slot)
{
	ItemInst* inst = m_inv.GetItem(slot);
	if(!inst) {
		return;
	}

	auto item_timers = inst->GetTimers();
	auto it_iter = item_timers.begin();
	while(it_iter != item_timers.end()) {
		if(it_iter->second.Check()) {
			parse->EventItem(EVENT_TIMER, this, inst, nullptr, it_iter->first, 0);
		}
		++it_iter;
	}

	if(slot > EmuConstants::EQUIPMENT_END) {
		return;
	}
}

void Client::RefundAA() {
	int cur = 0;
	bool refunded = false;

	for(int x = 0; x < aaHighestID; x++) {
		cur = GetAA(x);
		if(cur > 0){
			SendAA_Struct* curaa = zone->FindAA(x);
			if(cur){
				SetAA(x, 0);
				for(int j = 0; j < cur; j++) {
					m_pp.aapoints += curaa->cost + (curaa->cost_inc * j);
					refunded = true;
				}
			}
			else
			{
				m_pp.aapoints += cur;
				SetAA(x, 0);
				refunded = true;
			}
		}
	}

	if(refunded) {
		SaveAA();
		Save();
		// Kick();
	}
}

void Client::IncrementAA(int aa_id) {
	SendAA_Struct* aa2 = zone->FindAA(aa_id);

	if(aa2 == nullptr)
		return;

	if(GetAA(aa_id) == aa2->max_level)
		return;

	SetAA(aa_id, GetAA(aa_id) + 1);

	SaveAA();

	SendAATable();
	SendAAStats();
	CalcBonuses();
}

void Client::SendItemScale(ItemInst *inst) {
	int slot = m_inv.GetSlotByItemInst(inst);
	if(slot != -1) {
		inst->ScaleItem();
		SendItemPacket(slot, inst, ItemPacketCharmUpdate);
		CalcBonuses();
	}
}

void Client::SetHunger(int32 in_hunger)
{
	int value = RuleI(Character,ConsumptionValue);

	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	sta->food = in_hunger;
	sta->water = m_pp.thirst_level > value ? value : m_pp.thirst_level;
	sta->fatigue=GetFatiguePercent();

	m_pp.hunger_level = in_hunger;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SetThirst(int32 in_thirst)
{
	int value = RuleI(Character,ConsumptionValue);

	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	sta->food = m_pp.hunger_level > value ? value : m_pp.hunger_level;
	sta->water = in_thirst;
	sta->fatigue=GetFatiguePercent();

	m_pp.thirst_level = in_thirst;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SetConsumption(int32 in_hunger, int32 in_thirst)
{
	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	sta->food = in_hunger;
	sta->water = in_thirst;
	sta->fatigue=GetFatiguePercent();

	m_pp.hunger_level = in_hunger;
	m_pp.thirst_level = in_thirst;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::Consume(const Item_Struct *item, uint8 type, int16 slot, bool auto_consume)
{
	/* item->CastTime is the time in real world minutes the food/drink will last when clicked. If it is 
	auto-consumed, that number is doubled.

	EQ stomach is 6000 units.

	Baseline characters (Humans, no mods) consume food/drink at a rate of about 50% (3000 units) of the 
	"stomach" per hour, or about 0.83% (50 units) per minute. Once the player's "stomach" reaches 50% 
	food/drink is auto-consumed by the client and we end up here. Server side, in Client::DoStaminaUpdate()
	we subtract 75 units of food/water every time the race specific timer hits.

	All penalities and mods including race, horses, deserts, and AAs are handled in Client::DoStaminaUpdate().

	EQ stomach is 6000 units, which means for auto-consume we want to multiply CastTime by 100. 

	Example: Bread Cakes has a CastTime of 30, which equates to 60 minutes auto-consumed or HALF
	of the total stomach (since the stomach will go from 6000 to 0 in 2 hours.) 30*100 is 3000,
	or half of 6000. Anything higher than CastTime 60 will overfill the stomach. We want to keep
	track of the correct value in the pp (so those foods will have their full value), but before 
	we send the packet it needs to be adjusted to 6000 max.
	
	There are rumors that some items like plains pebble or bale of hay will reduce food consumption,
	but I found no hard evidence of that so I left them out. */
	
	if(!item) { return; }

	int value = RuleI(Character,ConsumptionValue);
	if(type == ItemTypeFood)
	{
		int32 food = item->CastTime*100;

		if(!auto_consume)
			food = food/2;

		m_pp.hunger_level += food;
		DeleteItemInInventory(slot, 1, false);

		//Message(CC_Yellow, "%s consumed. Added %i to hunger (%i)", item->Name, food, m_pp.hunger_level);
		if(!auto_consume) //no message if the client consumed for us
			entity_list.MessageClose_StringID(this, true, 50, 0, EATING_MESSAGE, GetName(), item->Name);

#if EQDEBUG >= 5
       Log.Out(Logs::General, Logs::None, "Eating from slot:%i", (int)slot);
#endif
	}
	else
	{
		int32 drink = item->CastTime*100;

		if(!auto_consume)
			drink = drink/2;

		m_pp.thirst_level += drink;
		DeleteItemInInventory(slot, 1, false);

		//Message(CC_Yellow, "%s consumed. Added %i to thirst (%i)", item->Name, drink, m_pp.thirst_level);
        if(!auto_consume) //no message if the client consumed for us
			entity_list.MessageClose_StringID(this, true, 50, 0, DRINKING_MESSAGE, GetName(), item->Name);

#if EQDEBUG >= 5
        Log.Out(Logs::General, Logs::None, "Drinking from slot:%i", (int)slot);
#endif
   }
}

void Client::SendMarqueeMessage(uint32 type, uint32 priority, uint32 fade_in, uint32 fade_out, uint32 duration, std::string msg)
{
	if(duration == 0 || msg.length() == 0) {
		return;
	}

	EQApplicationPacket outapp(OP_Unknown, sizeof(ClientMarqueeMessage_Struct) + msg.length());
	ClientMarqueeMessage_Struct *cms = (ClientMarqueeMessage_Struct*)outapp.pBuffer;

	cms->type = type;
	cms->unk04 = 10;
	cms->priority = priority;
	cms->fade_in_time = fade_in;
	cms->fade_out_time = fade_out;
	cms->duration = duration;
	strcpy(cms->msg, msg.c_str());

	QueuePacket(&outapp);
}

void Client::Starve()
{
	m_pp.hunger_level = 0;
	m_pp.thirst_level = 0;

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	sta->food = m_pp.hunger_level;
	sta->water = m_pp.thirst_level;
	sta->fatigue=GetFatiguePercent();

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SetBoatID(uint32 boatid)
{
	m_pp.boatid = boatid;
}

void Client::SetBoatName(const char* boatname)
{
	strncpy(m_pp.boat, boatname, 16);
}

void Client::QuestReward(Mob* target, uint32 copper, uint32 silver, uint32 gold, uint32 platinum, uint32 itemid, uint32 exp, bool faction) {

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Sound, sizeof(QuestReward_Struct));
	memset(outapp->pBuffer, 0, sizeof(outapp->pBuffer));
	QuestReward_Struct* qr = (QuestReward_Struct*)outapp->pBuffer;

	qr->mob_id = target->GetID();		// Entity ID for the from mob name
	qr->target_id = GetID();			// The Client ID (this)
	qr->copper = copper;
	qr->silver = silver;
	qr->gold = gold;
	qr->platinum = platinum;
	qr->item_id = itemid;
	qr->exp_reward = exp;

	if (copper > 0 || silver > 0 || gold > 0 || platinum > 0)
		AddMoneyToPP(copper, silver, gold, platinum, false);

	if (itemid > 0)
		SummonItem(itemid, 1, false, MainQuest);

	if (faction)
	{
		if (target->IsNPC())
		{
			int32 nfl_id = target->CastToNPC()->GetNPCFactionID();
			SetFactionLevel(CharacterID(), nfl_id, GetBaseClass(), GetBaseRace(), GetDeity(), true);
			qr->faction = target->CastToNPC()->GetPrimaryFaction();
			qr->faction_mod = 1; // Too lazy to get real value, not sure if this is even used by client anyhow.
		}
	}

	if (exp > 0)
		AddQuestEXP(exp);

	QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
	safe_delete(outapp);
}

void Client::RewindCommand()
{
	if ((rewind_timer.GetRemainingTime() > 1 && rewind_timer.Enabled())) {
		Message(0, "You must wait before using #rewind again.");
	}
	else {
		MovePC(zone->GetZoneID(), zone->GetInstanceID(), m_RewindLocation.x, m_RewindLocation.y, m_RewindLocation.z, 0, 2, Rewind);
		rewind_timer.Start(30000, true);
	}
}

void Client::ShowNumHits()
{
	uint32 buffcount = GetMaxTotalSlots();
	for (uint32 buffslot = 0; buffslot < buffcount; buffslot++) {
		const Buffs_Struct &curbuff = buffs[buffslot];
		if (curbuff.spellid != SPELL_UNKNOWN && curbuff.numhits)
			Message(0, "You have %d hits left on %s", curbuff.numhits, GetSpellName(curbuff.spellid));
	}
	return;
}

float Client::GetQuiverHaste()
{
	float quiver_haste = 0;
	for (int r = EmuConstants::GENERAL_BEGIN; r <= EmuConstants::GENERAL_END; r++) {
		const ItemInst *pi = GetInv().GetItem(r);
		if (!pi)
			continue;
		if (pi->IsType(ItemClassContainer) && pi->GetItem()->BagType == BagTypeQuiver) {
			float temp_wr = (pi->GetItem()->BagWR / RuleI(Combat, QuiverWRHasteDiv));
			quiver_haste = std::max(temp_wr, quiver_haste);
		}
	}
	if (quiver_haste > 0)
		quiver_haste = 1.0f / (1.0f + static_cast<float>(quiver_haste) / 100.0f);
	return quiver_haste;
}

bool Client::IsTargetInMyGroup(Client* target)
{
	if (this == target)
	{
		return true;
	}
	else if (this->IsRaidGrouped() && target->IsRaidGrouped())
	{
		return this->GetRaid() == target->GetRaid();
	}
	else if (this->IsGrouped() && target->IsGrouped()){

		return this->GetGroup() == target->GetGroup();
	}

	return false;
}

bool Client::Disarm(Client* client)
{
	ItemInst* weapon = client->m_inv.GetItem(MainPrimary);
	if(weapon)
	{
		uint8 charges = weapon->GetCharges();
		uint16 freeslotid = client->m_inv.FindFreeSlot(false, true, weapon->GetItem()->Size);
		if(freeslotid != INVALID_INDEX)
		{
			client->DeleteItemInInventory(MainPrimary,0,true);
			client->SummonItem(weapon->GetID(),charges,false,freeslotid);
			client->WearChange(MaterialPrimary,0,0);

			return true;
		}
	}
	return false;
}


void Client::SendSoulMarks(SoulMarkList_Struct* SMS)
{
	if(Admin() <= 80)
		return;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SoulMarkUpdate, sizeof(SoulMarkList_Struct));
	memset(outapp->pBuffer, 0, sizeof(outapp->pBuffer));
	SoulMarkList_Struct* soulmarks = (SoulMarkList_Struct*)outapp->pBuffer;
	memcpy(&soulmarks->entries, SMS->entries, 12 * sizeof(SoulMarkEntry_Struct));
	strncpy(soulmarks->interrogatename, SMS->interrogatename, 64);
	QueuePacket(outapp);
	safe_delete(outapp);	
}

bool Client::FoodFamished()
{
	if(GetGM())
	{
		return false;
	}
	else
	{
		if(m_pp.hunger_level <= 0 && m_pp.famished >= RuleI(Character, FamishedLevel))
			return true;
	}

	return false;
}

bool Client::WaterFamished()
{
	if(GetGM())
	{
		return false;
	}
	else
	{
		if(m_pp.thirst_level <= 0 && m_pp.famished >= RuleI(Character, FamishedLevel))
			return true;
	}

	return false;
}

