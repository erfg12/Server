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
	0,	// scalerate
	0,
	0,
	0,
	0,
	0,
	0
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
	charm_class_attacks_timer(3000),
	charm_cast_timer(3500),
	qglobal_purge_timer(30000),
	TrackingTimer(2000),
	client_distance_timer(1000),
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
	CustomerID = 0;
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
	gminvul = false;
	playeraction = 0;
	SetTarget(0);
	auto_attack = false;
	auto_fire = false;
	runmode = true;
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

	client_distance_timer.Disable();

	cur_end = 0;

	InitializeBuffSlots();

	LoadAccountFlags();

	initial_respawn_selection = 0;

	//walkspeed = RuleR(Character, BaseWalkSpeed);

	EngagedRaidTarget = false;
	SavedRaidRestTimer = 0;

	interrogateinv_flag = false;

	has_zomm = false;
	client_position_update = false;
	ignore_zone_count = false;
	last_target = 0;
	clicky_override = false;
	active_disc = 0;
	active_disc_spell = 0;
	trapid = 0;
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

	// remove us from any feign memory
	entity_list.ClearZoneFeignAggro(this);

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
	m_pp.heading = m_Position.w * 2.0f;

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
	auto regularBindPosition = glm::vec4(m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, m_pp.binds[0].heading);
	auto homeBindPosition = glm::vec4(m_pp.binds[4].x, m_pp.binds[4].y, m_pp.binds[4].z, m_pp.binds[4].heading);
	database.SaveCharacterBindPoint(CharacterID(), m_pp.binds[0].zoneId, m_pp.binds[0].instance_id, regularBindPosition, 0); /* Regular bind */
	database.SaveCharacterBindPoint(CharacterID(), m_pp.binds[4].zoneId, m_pp.binds[4].instance_id, homeBindPosition, 1); /* Home Bind */

	/* Save Character Buffs */
	database.SaveBuffs(this);

	/* Total Time Played */
	TotalSecondsPlayed += (time(nullptr) - m_pp.lastlogin);
	m_pp.timePlayedMin = (TotalSecondsPlayed / 60);
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
		if (pApp && (*pApp))
			delete *pApp;
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
	else if (app != nullptr && *app != nullptr)
	{
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
						Message(CC_Default, "You have been rate limited, you can send more messages in %i seconds.",
							GlobalChatLimiterTimer->GetRemainingTime() / 1000);
						return;
					}
					else
					{
						Message(CC_Default, "You have been rate limited, you can send more messages in 60 seconds.");
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
			Message(CC_Default, "Error: You dont have permission to speak to the guild.");
		else if (!worldserver.SendChannelMessage(this, targetname, chan_num, GuildID(), language, message))
			Message(CC_Default, "Error: World server disconnected");
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
				Message(CC_Default, "You have been revoked. You may not talk on Auction.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, GlobalChatLevelLimit))
				{
					Message(CC_Default, "You do not have permission to talk in Auction at this time.");
					return;
				}
			}

			if (!worldserver.SendChannelMessage(this, 0, 4, 0, language, message))
				Message(CC_Default, "Error: World server disconnected");
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
				Message(CC_Default,"OOC has been muted. Try again later.");
				return;
			}

			if(GetRevoked())
			{
				Message(CC_Default, "You have been revoked. You may not talk on OOC.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, GlobalChatLevelLimit))
				{
					Message(CC_Default, "You do not have permission to talk in OOC at this time.");
					return;
				}
			}

			if (!worldserver.SendChannelMessage(this, 0, 5, 0, language, message))
				Message(CC_Default, "Error: World server disconnected");
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
			Message(CC_Default, "Error: Only GMs can use this channel");
		else if (!worldserver.SendChannelMessage(this, targetname, chan_num, 0, language, message))
			Message(CC_Default, "Error: World server disconnected");
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
				Message(CC_Default, "You have been revoked. You may not send tells.");
				return;
			}

			if(TotalKarma < RuleI(Chat, KarmaGlobalChatLimit))
			{
				if(GetLevel() < RuleI(Chat, GlobalChatLevelLimit))
				{
					Message(CC_Default, "You do not have permission to send tells at this time.");
					return;
				}
			}

			// allow tells to corpses
			if (targetname) {
				if (GetTarget() && GetTarget()->IsCorpse() && GetTarget()->CastToCorpse()->IsPlayerCorpse()) {
					if (strcasecmp(targetname,GetTarget()->CastToCorpse()->GetName()) == 0) {
						if (strcasecmp(GetTarget()->CastToCorpse()->GetOwnerName(),GetName()) == 0) {
							Message_StringID(MT_DefaultText, TALKING_TO_SELF);
							return;
						} else {
							targetname = GetTarget()->CastToCorpse()->GetOwnerName();
						}
					}
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
				Message(CC_Default, "Error: World server disconnected");
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
	default: {
		Message(CC_Default, "Channel (%i) not implemented", (uint16)chan_num);
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
		Message(CC_Default, "Error: World server disconnected");
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
		Message(CC_Default, "Error: World server disconnected");
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

	if (last_reported_mana != cur_mana) {



		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ManaChange, sizeof(ManaChange_Struct));
		ManaChange_Struct* manachange = (ManaChange_Struct*)outapp->pBuffer;
		manachange->new_mana = cur_mana;
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
	m_pp.fatigue = GetFatiguePercent();

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Stamina, sizeof(Stamina_Struct));
	Stamina_Struct* sta = (Stamina_Struct*)outapp->pBuffer;
	int value = RuleI(Character,ConsumptionValue);
	sta->food = m_pp.hunger_level > value ? value : m_pp.hunger_level;
	sta->water = m_pp.thirst_level> value ? value : m_pp.thirst_level;
	sta->fatigue = m_pp.fatigue;
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
	ns->spawn.size			= size;
	ns->spawn.runspeed		= (gmspeed == 0) ? runspeed : 3.1f;

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

	UpdateEquipmentLight();
	UpdateActiveLight();
	ns->spawn.light = m_Light.Type.Active;
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
		(static_cast<uint64>(m_pp.platinum_cursor) * 1000)))));
}

bool Client::CheckIncreaseSkill(SkillUseTypes skillid, Mob *against_who, float difficulty, float success) {
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
		Log.Out(Logs::General, Logs::Skills, "Skill %d at value %d %s. difficulty: %0.2f", skillid, skillval, success == 1.0 ? "succeeded" : "failed", difficulty);
		float stat = GetSkillStat(skillid);
		float skillup_modifier = stat > 135 ? RuleR(Skills, HighStatSkillUpModifier) : RuleR(Skills, SkillUpModifier);

		if(difficulty < 1)
			difficulty = 1.0f;
		if(difficulty > 15)
			difficulty = 15.0f;

		float chance1 = (stat / (difficulty * success)) * skillup_modifier;
		if(chance1 > 95)
			chance1 = 95.0;

		if(zone->random.Real(0, 99) < chance1)
		{

			Log.Out(Logs::Detail, Logs::Skills, "Skill %d at value %d passed first roll with %0.2f percent chance (diff %0.2f)", skillid, skillval, chance1, difficulty);

			float skillvalue = skillval / 2.0f;
			if(skillvalue > 95)
				skillvalue = 95.0f;

			float chance2 = 100.0f - skillvalue;
			mod_increase_skill_chance(chance2, against_who);

			if(zone->random.Real(0, 99) < chance2)
			{
				SetSkill(skillid, GetRawSkill(skillid) + 1);
				Log.Out(Logs::General, Logs::Skills, "Skill %d at value %d using stat %0.2f successfully gained a point with %0.2f percent chance (diff %0.2f) first roll chance was: %0.2f", skillid, skillval, stat, chance2, difficulty, chance1);
				return true;
			}
			else
			{
				Log.Out(Logs::General, Logs::Skills, "Skill %d at value %d failed second roll with %0.2f percent chance (diff %0.2f)", skillid, skillval, chance2, difficulty);
			}
		} 
		else 
		{
			Log.Out(Logs::Detail, Logs::Skills, "Skill %d at value %d failed first roll with %0.2f percent chance (diff %0.2f)", skillid, skillval, chance1, difficulty);
		}
	} 
	else 
	{
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
		Chance = (Chance * RuleI(Skills, LangSkillUpModifier)/100);

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
	bool returned = false;
	if(!fail) 
	{
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
				returned = false;
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
				returned = false;
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
					returned = false; //Not really a failure, but we shouldn't get a skillup check here.
				}
				else if (bindmob->IsAIControlled() && bindmob != this ){
					bindmob->CastToClient()->Message_StringID(CC_User_Skills, STAY_STILL); // Tell IPC to stand still?
				}
			}
		} 
		else 
		{
		// finish bind
			// disable complete timer
			bindwound_timer.Disable();
			bindwound_target = 0;
			if(!bindmob)
			{
				// send "bindmob gone" to client
				bind_out->type = 5; // not in zone
				QueuePacket(outapp);
				bind_out->type = 0;
				returned = false;
			}
			else 
			{
				if (!GetFeigned() && (DistanceSquared(bindmob->GetPosition(), m_Position)  <= 400)) 
				{
					// send bindmob bind done
					if(!bindmob->IsAIControlled() && bindmob != this ) {

					}
					else if(bindmob->IsAIControlled() && bindmob != this ) {
					// Tell IPC to resume??
					}

					// Send client bind done
					bind_out->type = 1;
					QueuePacket(outapp);
					bind_out->type = 0;
					returned = true;

					int maxHPBonus = spellbonuses.MaxBindWound + itembonuses.MaxBindWound + aabonuses.MaxBindWound;

					int max_percent = 50 + 10 * maxHPBonus;

					if(GetClass() == MONK && GetSkill(SkillBindWound) > 200) {
						max_percent = 70 + 10 * maxHPBonus;
					}

					max_percent = mod_bindwound_percent(max_percent, bindmob);

					int max_hp = bindmob->GetMaxHP()*max_percent/100;

					// send bindmob new hp's
					if (bindmob->GetHP() < bindmob->GetMaxHP() && bindmob->GetHP() <= (max_hp)-1)
					{
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
					else 
					{
						//I dont have the real, live
						if(bindmob->IsClient() && bindmob != this)
							bindmob->CastToClient()->Message(CC_Yellow, "You cannot have your wounds bound above %d%% hitpoints.", max_percent);
						else
							Message(CC_Yellow, "You cannot bind wounds above %d%% hitpoints.", max_percent);

						returned = false;
					}
					Stand();
				}
				else 
				{
					// Send client bind failed
					if(bindmob != this)
						bind_out->type = 6; // They moved
					else
						bind_out->type = 7; // Bandager moved

					QueuePacket(outapp);
					bind_out->type = 0;
					returned = false;
				}
			}
		}
	}
	else if (bindwound_timer.Enabled()) 
	{
		// You moved
		outapp = new EQApplicationPacket(OP_Bind_Wound, sizeof(BindWound_Struct));
		BindWound_Struct* bind_out = (BindWound_Struct*) outapp->pBuffer;
		bindwound_timer.Disable();
		bindwound_target = 0;
		bind_out->type = 7;
		QueuePacket(outapp);
		bind_out->type = 3;
		QueuePacket(outapp);
		return false;
	}
	safe_delete(outapp);
	return returned;
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
		tellsoff = true;
	}
	else
	{
		SetAnon(false);
		database.SetHideMe(AccountID(),false);
		CreateSpawnPacket(&app);
		trackable = true;
		tellsoff = false;
	}

	entity_list.QueueClientsStatus(this, &app, true, 0, Admin()-1);
	UpdateWho();
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
	client_distance_timer.Disable();
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

		client->Message(CC_Default,"%s",ptr);
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

			BuffFadeAll();
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

void Client::SendPickPocketResponse(Mob *from, uint32 amt, int type, int16 slotid, ItemInst* inst, bool skipskill)
{
	if(!skipskill)
	{
		float success = 2.0;
		if(type > PickPocketFailed)
			success = 1.0;

		CheckIncreaseSkill(SkillPickPockets, nullptr, zone->skill_difficulty[SkillPickPockets].difficulty, success);
	}

	if(type == PickPocketItem && inst)
	{
		SendItemPacket(slotid, inst, ItemPacketStolenItem, GetID(), from->GetID(), GetSkill(SkillPickPockets));
		return;
	}
	else
	{

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_PickPocket, sizeof(PickPocket_Struct));
		PickPocket_Struct* pick_out = (PickPocket_Struct*) outapp->pBuffer;
		pick_out->coin = amt;
		pick_out->from = GetID();
		pick_out->to = from->GetID();
		pick_out->myskill = GetSkill(SkillPickPockets);

		if(amt == 0)
			type = PickPocketFailed;

		pick_out->type = type;

		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

bool Client::GetPickPocketSlot(ItemInst* inst, int16& freeslotid)
{
	bool is_arrow = (inst->GetItem()->ItemType == ItemTypeArrow) ? true : false;
	freeslotid = m_inv.FindFreeSlot(false, true, inst->GetItem()->Size, is_arrow);

	//make sure we are not completely full...
	if ((freeslotid == MainCursor && m_inv.GetItem(MainCursor) != nullptr) || freeslotid == INVALID_INDEX)
	{
		freeslotid == INVALID_INDEX;
		return false;
	}
	else
	{
		PutItemInInventory(freeslotid, *inst);
		return true;
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
	this->Message(CC_Default,"%s",WindowText.c_str());
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
		Message(CC_Default,"Unable to find pet!");
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

void Client::SendStats(Client* client)
{

	int shield_ac = 0;
	GetRawACNoShield(shield_ac);

	client->Message(CC_Yellow, "~~~~~ %s %s ~~~~~", GetCleanName(), GetLastName());
	client->Message(CC_Default, " Level: %i Class: %i Race: %i RaceBit: %i Size: %1.1f BaseSize: %1.1f Weight: %.1f/%d  ", GetLevel(), GetClass(), GetRace(), GetRaceBitmask(GetRace()), GetSize(), GetBaseSize(), (float)CalcCurrentWeight() / 10.0f, GetSTR());
	client->Message(CC_Default, " HP: %i/%i  HP Regen: %i/%i",GetHP(), GetMaxHP(), CalcHPRegen()+RestRegenHP, CalcHPRegenCap());
	client->Message(CC_Default, " AC: %i ( Mit.: %i + Avoid.: %i + Spell: %i ) | Shield AC: %i DS: %i", CalcAC(), GetMitigation(), GetAvoidance(), spellbonuses.AC, shield_ac, GetDS());
	client->Message(CC_Default, " AFK: %i LFG: %i Anon: %i PVP: %i GM: %i Flymode: %i GMSpeed: %i Hideme: %i GMInvul: %d LD: %i ClientVersion: %i TellsOff: %i", AFK, LFG, GetAnon(), GetPVP(), GetGM(), flymode, GetGMSpeed(), GetHideMe(), GetGMInvul(), IsLD(), GetClientVersionBit(), tellsoff);
	if(CalcMaxMana() > 0)
		client->Message(CC_Default, " Mana: %i/%i  Mana Regen: %i/%i", GetMana(), GetMaxMana(), CalcManaRegen()+RestRegenMana, CalcManaRegenCap());
	client->Message(CC_Default, "  X: %0.2f Y: %0.2f Z: %0.2f", GetX(), GetY(), GetZ());
	client->Message(CC_Default, " End.: %i/%i  End. Regen: %i/%i",GetEndurance(), GetMaxEndurance(), CalcEnduranceRegen()+RestRegenEndurance, CalcEnduranceRegenCap());
	client->Message(CC_Default, " Offense: %i  To-Hit: %i  ATK: %i  Worn/Spell ATK %i/%i  Server Side ATK: %i", GetOffense(), GetToHit(), GetTotalATK(), RuleI(Character, ItemATKCap), GetATKBonus(), GetATK());
	client->Message(CC_Default, " Haste: %i / %i (Item: %i + Spell: %i + Over: %i)", GetHaste(), RuleI(Character, HasteCap), itembonuses.haste, spellbonuses.haste + spellbonuses.hastetype2, spellbonuses.hastetype3 + ExtraHaste);
	client->Message(CC_Default, " STR: %i  STA: %i  AGI: %i DEX: %i  WIS: %i INT: %i  CHA: %i", GetSTR(), GetSTA(), GetAGI(), GetDEX(), GetWIS(), GetINT(), GetCHA());
	client->Message(CC_Default, " PR: %i MR: %i  DR: %i FR: %i  CR: %i  ", GetPR(), GetMR(), GetDR(), GetFR(), GetCR());
	client->Message(CC_Default, " Shielding: %i  Spell Shield: %i  DoT Shielding: %i Stun Resist: %i  Strikethrough: %i  Avoidance: %i  Accuracy: %i  Combat Effects: %i", GetShielding(), GetSpellShield(), GetDoTShield(), GetStunResist(), GetStrikeThrough(), GetAvoidanceMod(), GetAccuracy(), GetCombatEffects());
	client->Message(CC_Default, " Runspeed: %0.1f  Walkspeed: %0.1f Hunger: %i Thirst: %i Famished: %i Encumbered: %i Boat: %s (Ent %i : NPC %i)", GetRunspeed(), GetWalkspeed(), GetHunger(), GetThirst(), GetFamished(), IsEncumbered(), GetBoatName(), GetBoatID(), GetBoatNPCID());
	if(GetClass() == WARRIOR)
		client->Message(CC_Default, "HasShield: %i KickDmg: %i BashDmg: %i", HasShieldEquiped(), GetKickDamage(), GetBashDamage());
	else if(GetClass() == RANGER || GetClass() == BEASTLORD)
		client->Message(CC_Default, "HasShield: %i KickDmg: %i", HasShieldEquiped(), GetKickDamage());
	else if(GetClass() == PALADIN || GetClass() == SHADOWKNIGHT)
		client->Message(CC_Default, "HasShield: %i BashDmg: %i", HasShieldEquiped(), GetBashDamage());
	else
		client->Message(CC_Default, "HasShield: %i", HasShieldEquiped());
	if(GetClass() == BARD)
		client->Message(CC_Default, " Singing: %i  Brass: %i  String: %i Percussion: %i Wind: %i", GetSingMod(), GetBrassMod(), GetStringMod(), GetPercMod(), GetWindMod());
	if(HasGroup())
	{
		Group* g = GetGroup();
		client->Message(CC_Default, " GroupID: %i Count: %i GroupLeader: %s GroupLeaderCached: %s", g->GetID(), g->GroupCount(), g->GetLeaderName(), g->GetOldLeaderName());
	}
	client->Message(CC_Default, " Hidden: %i ImpHide: %i Sneaking: %i Invisible: %i InvisVsUndead: %i InvisVsAnimals: %i", hidden, improved_hidden, sneaking, invisible, invisible_undead, invisible_animals);
	client->Message(CC_Default, " Feigned: %i Invulnerable: %i SeeInvis: %i HasZomm: %i Disc: %i/%i", feigned, invulnerable, see_invis, has_zomm, GetActiveDisc(), GetActiveDiscSpell());

	Extra_Info:

	client->Message(CC_Default, " BaseRace: %i  Gender: %i  BaseGender: %i Texture: %i  HelmTexture: %i", GetBaseRace(), GetGender(), GetBaseGender(), GetTexture(), GetHelmTexture());
	if (client->Admin() >= 100) {
		client->Message(CC_Default, "  CharID: %i  EntityID: %i  PetID: %i  OwnerID: %i  AIControlled: %i  Targetted: %i", CharacterID(), GetID(), GetPetID(), GetOwnerID(), IsAIControlled(), targeted);
		uint32 xpneeded = GetEXPForLevel(GetLevel()+1) - GetEXP();
		int exploss;
		GetExpLoss(nullptr,0,exploss);
		if(GetLevel() < 10)
			exploss = 0;
		client->Message(CC_Default, "  CurrentXP: %i XP Needed: %i ExpLoss: %i CurrentAA: %i", GetEXP(), xpneeded, exploss, GetAAXP());
		client->Message(CC_Default, "  Last Update: %d Current Time: %d Difference: %d Zone Count: %d", pLastUpdate, Timer::GetCurrentTime(), Timer::GetCurrentTime() - pLastUpdate, GetZoneChangeCount());
	}
}

void Client::SendQuickStats(Client* client)
{
	client->Message(CC_Yellow, "~~~~~ %s %s ~~~~~", GetCleanName(), GetLastName());
	client->Message(CC_Default, " Level: %i Class: %i Race: %i Size: %1.1f BaseSize: %1.1f Weight: %.1f/%d  ", GetLevel(), GetClass(), GetRace(), GetSize(), GetBaseSize(), (float)CalcCurrentWeight() / 10.0f, GetSTR());
	client->Message(CC_Default, " HP: %i/%i  HP Regen: %i/%i End.: %i/%i  End. Regen: %i/%i",GetHP(), GetMaxHP(), CalcHPRegen()+RestRegenHP, CalcHPRegenCap(),GetEndurance(), GetMaxEndurance(), CalcEnduranceRegen()+RestRegenEndurance, CalcEnduranceRegenCap());
	if(CalcMaxMana() > 0)
		client->Message(CC_Default, " Mana: %i/%i  Mana Regen: %i/%i", GetMana(), GetMaxMana(), CalcManaRegen()+RestRegenMana, CalcManaRegenCap());
	client->Message(CC_Default, " AC: %i ATK: %i Haste: %i / %i", CalcAC(), GetTotalATK(), GetHaste(), RuleI(Character, HasteCap));
	client->Message(CC_Default, " STR: %i  STA: %i  AGI: %i DEX: %i  WIS: %i INT: %i  CHA: %i", GetSTR(), GetSTA(), GetAGI(), GetDEX(), GetWIS(), GetINT(), GetCHA());
	client->Message(CC_Default, " PR: %i MR: %i  DR: %i FR: %i  CR: %i  ", GetPR(), GetMR(), GetDR(), GetFR(), GetCR());
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

	// Don't garble # commands
	if (message[0] == '#')
		return;

	for (size_t i = 0; i < strlen(message); i++) {
		uint8 chance = (uint8)zone->random.Int(0, 115); // variation just over worst possible scrambling
		if (isalpha((unsigned char)message[i]) && (chance <= variance)) {
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
		if(database.GetFactionData(&fmods, p_class, p_race, p_deity, pFaction, GetTexture(), GetGender()))
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
	int32 current_value;
	bool repair = false;

	// Get the npc faction list
	if (!database.GetNPCFactionList(npc_id, faction_id, npc_value, temp))
		return;
	for (int i = 0; i < MAX_NPC_FACTIONS; i++)
	{
		int32 faction_before_hit;
		int32 faction_to_use_for_messaging;
		FactionMods fm;
		int32 this_faction_max;
		int32 this_faction_min;

		if (faction_id[i] <= 0)
			continue;

		// Find out starting faction for this faction
		// It needs to be used to adj max and min personal
		// The range is still the same, 1200-3000(4200), but adjusted for base
		database.GetFactionData(&fm, GetClass(), GetRace(), GetDeity(),
			faction_id[i], GetTexture(), GetGender());

		if (quest)
		{
			//The ole switcheroo
			if (npc_value[i] > 0)
				npc_value[i] = -abs(npc_value[i]);
			else if (npc_value[i] < 0)
				npc_value[i] = abs(npc_value[i]);
		}

		// Adjust the amount you can go up or down so the resulting range
		// is PERSONAL_MAX - PERSONAL_MIN
		//
		// Adjust these values for cases where starting faction is below
		// min or above max by not allowing any earn in those directions.
		this_faction_min = MIN_PERSONAL_FACTION - fm.base;
		this_faction_min = std::min(0, this_faction_min);
		this_faction_max = MAX_PERSONAL_FACTION - fm.base;
		this_faction_max = std::max(0, this_faction_max);

		// Get the characters current value with that faction
		current_value = GetCharacterFactionLevel(faction_id[i], true);
		faction_before_hit = current_value;

		UpdatePersonalFaction(char_id, npc_value[i], faction_id[i], &current_value, temp[i], this_faction_min, this_faction_max);

		Log.Out(Logs::General, Logs::Faction, "Min(%d) Max(%d) Before(%d), After(%d)\n", this_faction_min, this_faction_max, faction_before_hit, current_value);

		SendFactionMessage(npc_value[i], faction_id[i], faction_before_hit, current_value, temp[i], this_faction_min, this_faction_max);
	}

	return;
}

void Client::SetFactionLevel2(uint32 char_id, int32 faction_id, uint8 char_class, uint8 char_race, uint8 char_deity, int32 value, uint8 temp)
{
	int32 current_value;

	//Get the npc faction list
	if(faction_id > 0 && value != 0) {
		int32 faction_before_hit;
		FactionMods fm;
		int32 this_faction_max;
		int32 this_faction_min;

		// Find out starting faction for this faction
		// It needs to be used to adj max and min personal
		// The range is still the same, 1200-3000(4200), but adjusted for base
		database.GetFactionData(&fm, GetClass(), GetRace(), GetDeity(), 
			faction_id, GetTexture(), GetGender());

		// Adjust the amount you can go up or down so the resulting range
		// is PERSONAL_MAX - PERSONAL_MIN
		//
		// Adjust these values for cases where starting faction is below
		// min or above max by not allowing any earn/loss in those directions.
		// At least one faction starts out way below min, so we don't want
		// to allow loses in those cases, just massive gains.
		this_faction_min = MIN_PERSONAL_FACTION - fm.base;
		this_faction_min = std::min(0, this_faction_min);
		this_faction_max = MAX_PERSONAL_FACTION - fm.base;
		this_faction_max = std::max(0, this_faction_max);

		//Get the faction modifiers
		current_value = GetCharacterFactionLevel(faction_id, true);
		faction_before_hit = current_value;

		UpdatePersonalFaction(char_id, value, faction_id, &current_value, temp, this_faction_min, this_faction_max);

		Log.Out(Logs::General, Logs::Faction, "Min(%d) Max(%d) Before(%d), After(%d)\n", this_faction_min, this_faction_max, faction_before_hit, current_value);

		SendFactionMessage(value, faction_id, faction_before_hit, current_value, temp, this_faction_min, this_faction_max);
	}

	return;
}

int32 Client::GetCharacterFactionLevel(int32 faction_id, bool updating)
{
	int32 faction = 0;
	if (!updating && (faction_id <= 0 || GetRaceBitmask(GetRace()) == 0))
	{
		Log.Out(Logs::Detail, Logs::Faction, "Race: %d is non base, ignoring character faction. This is %s.", GetRace(), updating ? "a save" : "not a save");
		return 0;
	}
	faction_map::iterator res;
	res = factionvalues.find(faction_id);
	if (res == factionvalues.end())
		return 0;
	faction = res->second;

	Log.Out(Logs::Detail, Logs::Faction, "%s has %d personal faction with %d This is %s.", GetName(), faction, faction_id, updating ? "a save" : "not a save");
	return faction;
}

// Common code to set faction level.
// Applies HeroicCHA is it applies
// Checks for bottom out and max faction and old faction db entries
// Updates the faction if we are not minned, maxed or we need to repair

void Client::UpdatePersonalFaction(int32 char_id, int32 npc_value, int32 faction_id, int32 *current_value, int32 temp, int32 this_faction_min, int32 this_faction_max)
{
	bool repair = false;
	bool change = false;

	// Set flag when to update db
	// Repair needed, as db changes could modify a base value for a faction
	// and we need to auto correct when that happens.
	if (*current_value > this_faction_max)
	{
		*current_value = this_faction_max;
		repair = true;
	}
	else if (*current_value < this_faction_min)
	{
		*current_value = this_faction_min;
		repair = true;
	}
	else if ((m_pp.gm != 1) && (npc_value != 0) &&
		((npc_value > 0 && *current_value != this_faction_max) ||
		((npc_value < 0 && *current_value != this_faction_min))))
		change = true;

	if (change || repair)
	{
		*current_value += npc_value;

		if (*current_value > this_faction_max)
			*current_value = this_faction_max;
		else if (*current_value < this_faction_min)
			*current_value = this_faction_min;

		database.SetCharacterFactionLevel(char_id, faction_id, *current_value, temp, factionvalues);
	}

return;
}

// returns the character's faction level, adjusted for racial, class, and deity modifiers
int32 Client::GetModCharacterFactionLevel(int32 faction_id) {
	int32 Modded = GetCharacterFactionLevel(faction_id);
	FactionMods fm;
	if (database.GetFactionData(&fm, GetClass(), GetRace(), GetDeity(), faction_id, GetTexture(), GetGender()))
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
		if (database.GetFactionData(&fmod, GetClass(), GetRace(), GetDeity(), primaryfaction, GetTexture(), GetGender())) {
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
void Client::SendFactionMessage(int32 tmpvalue, int32 faction_id, int32 faction_before_hit, int32 totalvalue, uint8 temp, int32 this_faction_min, int32 this_faction_max)
{
	char name[50];
	int32 faction_value;

	// If we're dropping from MAX or raising from MIN or repairing, 
	// we should base the message on the new updated value so we don't show
	// a min MAX message
	//
	// If we're changing any other place, we use the value before the
	// hit.  For example, if we go from 1199 to 1200 which is the MAX
	// we still want to say faction got better this time around.
	
	if ( (faction_before_hit >= this_faction_max) ||
	     (faction_before_hit <= this_faction_min))
		faction_value = totalvalue;
	else
		faction_value = faction_before_hit;

	// default to Faction# if we couldn't get the name from the ID
	if (database.GetFactionName(faction_id, name, sizeof(name)) == false)
		snprintf(name, sizeof(name), "Faction%i", faction_id);

	bool gained = true;
	if (tmpvalue == 0 || temp == 1 || temp == 2)
		return;
	else if (faction_value >= this_faction_max)
		Message_StringID(CC_Default, FACTION_BEST, name);
	else if (faction_value <= this_faction_min)
	{
		gained = false;
		Message_StringID(CC_Default, FACTION_WORST, name);
	}
	else if (tmpvalue > 0 && faction_value < this_faction_max)
		Message_StringID(CC_Default, FACTION_BETTER, name);
	else if (tmpvalue < 0 && faction_value > this_faction_min)
	{
		gained = false;
		Message_StringID(CC_Default, FACTION_WORSE, name);
	}

	//We need to get total faction here, including racial, class, and deity modifiers.
	int32 total = GetModCharacterFactionLevel(faction_id);
	int32 new_value = faction_value + tmpvalue;
	std::string type = "gained";

	if(!gained)
		type = "lost";
	Log.Out(Logs::General, Logs::Faction, "You have %s %d faction with %s (%d)! Your total now including bonuses is %d (Personal: %d)", type.c_str(), tmpvalue, name, faction_id, total, new_value);
	// Log.Out(Logs::General, Logs::Faction, "Your faction standing with %s has been adjusted by %d", name, tmpvalue);
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
	m_pp.hunger_level = in_hunger;
	SendStaminaUpdate();
}

void Client::SetThirst(int32 in_thirst)
{
	m_pp.thirst_level = in_thirst;
	SendStaminaUpdate();
}

void Client::SetConsumption(int32 in_hunger, int32 in_thirst)
{
	m_pp.hunger_level = in_hunger;
	m_pp.thirst_level = in_thirst;
	SendStaminaUpdate();
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

void Client::SetBoatID(uint32 boatid)
{
	m_pp.boatid = boatid;
}

void Client::SetBoatName(const char* boatname)
{
	strncpy(m_pp.boat, boatname, 32);
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
		SummonItem(itemid, 0, false, MainQuest);

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
		Message(CC_Default, "You must wait before using #rewind again.");
	}
	else {
		MovePC(zone->GetZoneID(), zone->GetInstanceID(), m_RewindLocation.x, m_RewindLocation.y, m_RewindLocation.z, 0, 2, Rewind);
		rewind_timer.Start(30000, true);
	}
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
	if (target != nullptr)
	{
		if (this == target)
		{
			return true;
		}
		else if (this->IsRaidGrouped() && target->IsRaidGrouped())
		{
			return this->GetRaid() == target->GetRaid();
		}
		else if (this->IsGrouped() && target->IsGrouped())
		{
			return this->GetGroup() == target->GetGroup();
		}
	}
	return false;
}

uint8 Client::Disarm(Client* client, float chance)
{
	ItemInst* weapon = client->m_inv.GetItem(MainPrimary);
	if(weapon)
	{
		float roll = zone->random.Real(0, 150);
		if (roll < chance) 
		{
			uint8 charges = weapon->GetCharges();
			uint16 freeslotid = client->m_inv.FindFreeSlot(false, true, weapon->GetItem()->Size);
			if(freeslotid != INVALID_INDEX)
			{
				client->DeleteItemInInventory(MainPrimary,0,true);
				client->SummonItem(weapon->GetID(),charges,false,freeslotid);
				client->WearChange(MaterialPrimary,0,0);

				return 2;
			}
		}
		else
		{
			return 1;
		}
	}

	return 0;
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

void Client::SendClientVersion()
{
	if(GetClientVersion() == EQClientMac)
	{
		std::string string("Mac");
		std::string type;
		if(GetClientVersionBit() == BIT_MacIntel)
			type = "Intel";
		else if(GetClientVersionBit() == BIT_MacPPC)
			type = "PowerPC";
		else if(GetClientVersionBit() == BIT_MacPC)
			type = "PC";
		else
			type = "Invalid";

		if(GetGM())
			Message(CC_Yellow, "[GM Debug] Your client version is: %s (%i). Your client type is: %s.", string.c_str(), GetClientVersion(), type.c_str());
		else
			Log.Out(Logs::Detail, Logs::Debug, "%s: Client version is: %s. The client type is: %s.", GetName(), string.c_str(), type.c_str());

	}
	else
	{
		std::string string;
		if(GetClientVersion() == EQClientEvolution)
			string = "Evolution";
		if(GetClientVersion() == EQClientUnused)
			string = "Unused";
		else
			string = "Unknown";

		if(GetGM())
			Message(CC_Yellow, "[GM Debug] Your client version is: %s (%i).", string.c_str(), GetClientVersion());	
		else
			Log.Out(Logs::Detail, Logs::Debug, "%s: Client version is: %s.", GetName(), string.c_str());
	}
}

void Client::FixClientXP()
{
	//This is only necessary when the XP formula changes. However, it should be left for toons that have not been converted.

	uint16 level = GetLevel();
	uint32 totalrequiredxp = GetEXPForLevel(level);
	float currentxp = GetEXP();
	uint32 currentaa = GetAAXP();

	if(currentxp < totalrequiredxp)
	{
		if(Admin() == 0 && level > 1)
		{
			Message(CC_Red, "Error: Your current XP (%0.2f) is lower than your current level (%i)! It needs to be at least %i", currentxp, level, totalrequiredxp);
			SetEXP(totalrequiredxp, currentaa);
			Save();
			Kick();
		}
		else if(Admin() > 0 && level > 1)
			Message(CC_Red, "Error: Your current XP (%0.2f) is lower than your current level (%i)! It needs to be at least %i. Use #level or #addxp to correct it and logout!", currentxp, level, totalrequiredxp);
	}
}

void Client::KeyRingLoad()
{
	std::string query = StringFormat("SELECT item_id FROM character_keyring "
									"WHERE id = '%i' ORDER BY item_id", character_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
		keyring.push_back(atoi(row[0]));

}

void Client::KeyRingAdd(uint32 item_id)
{
	if(0==item_id)
		return;

	bool found = KeyRingCheck(item_id);
	if (found)
		return;

	std::string query = StringFormat("INSERT INTO character_keyring(id, item_id) VALUES(%i, %i)", character_id, item_id);
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}

	if(Admin() >= 10)
		Message(CC_Blue,"Added to keyring.");

	keyring.push_back(item_id);
}

bool Client::KeyRingCheck(uint32 item_id)
{
	for(std::list<uint32>::iterator iter = keyring.begin();
		iter != keyring.end();
		++iter)
	{
		if(*iter == item_id)
			return true;
	}
	return false;
}

void Client::KeyRingList(Client* notifier)
{
	notifier->Message(CC_Default,"Keys on %s's Keyring:", GetName());
	const Item_Struct *item = 0;
	for(std::list<uint32>::iterator iter = keyring.begin();
		iter != keyring.end();
		++iter)
	{
		if ((item = database.GetItem(*iter))!=nullptr) {
			notifier->Message(CC_Default,item->Name);
		}
	}
}

void Client::ZoneFlagList(Client* notifier)
{
	// Todo: Figure out how to get the client to spit these messages out for us.
	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		uint32 zoneid = zfs->zoneid;
		uint8 key = zfs->key;

		const char *short_name = database.GetZoneName(zoneid);

		float safe_x, safe_y, safe_z;
		int16 minstatus = 0;
		uint8 minlevel = 0;
		char flag_name[128];
		// Unfortunately, we have to hit the DB here. We can't assume the target zone will be booted to check its zone_data.
		if(database.GetSafePoints(short_name, 0, &safe_x, &safe_y, &safe_z, &minstatus, &minlevel, flag_name)) 
		{
			if(zoneid == vexthal)
			{
				if(key == 0 || key == 1)
					notifier->Message(CC_Yellow, "%s", flag_name); //Flag to enter zone.
				if(key == 1)
					notifier->Message(CC_Yellow, "North Tower Key (Vex Thal)"); //Flag within the zone.
			}
			if(zoneid == ssratemple)
				notifier->Message(CC_Yellow, "Ring of the Shissar"); //Flag within the zone.
			if(zoneid == frozenshadow)
				notifier->Message(CC_Yellow, "Tower of Frozen Shadows: %d", key); //Flag within the zone.
			if(zoneid == bothunder)
			{
				if(key == 0 || key == 1)
					notifier->Message(CC_Yellow, "%s", flag_name); //Flag to enter zone.
				if(key == 1)
					notifier->Message(CC_Yellow, "Enchanted Ring of Torden"); //Flag within the zone.
			}
			else
				notifier->Message(CC_Yellow, "%s", flag_name); //All other flags
		}

		iterator.Advance();
	}
}

void Client::SendToBoat(bool messageonly)
{
	// Sometimes, the client doesn't send OP_LeaveBoat, so the boat values don't get cleared.
	// This can lead difficulty entering the zone, since some people's client's don't like
	// the boat timeout period.
	if(!zone->IsBoatZone())
	{
		m_pp.boatid = 0;
		m_pp.boat[0] = 0;
		return;
	}
	else
	{
		if(m_pp.boatid > 0)
		{
			Log.Out(Logs::Moderate, Logs::Boats, "%s's boatid is %d boatname is %s", GetName(), m_pp.boatid, m_pp.boat);

			if(messageonly)
			{
				Mob* boat = entity_list.GetNPCByNPCTypeID(m_pp.boatid);
				if(boat && boat->IsBoat())
				{
					Log.Out(Logs::Moderate, Logs::Boats, "%s's boat %s (%d) location is %0.2f,%0.2f,%0.2f", GetName(), boat->GetCleanName(), m_pp.boatid, boat->GetX(), boat->GetY(), boat->GetZ());
					Log.Out(Logs::Moderate, Logs::Boats, "%s's location is: %0.2f,%0.2f,%0.2f", GetName(), m_Position.x, m_Position.y, m_Position.z);
				}
				if(!boat)
				{
					Log.Out(Logs::Moderate, Logs::Boats, "%s's boat is not spawned.", GetName());
				}

				return;
			}

			Mob* boat = entity_list.GetNPCByNPCTypeID(m_pp.boatid);
			if(!boat || !boat->IsBoat())
			{
				Log.Out(Logs::Moderate, Logs::Boats, "Boat %d is not spawned. Sending %s to safe points.", m_pp.boatid, GetName());
				auto safePoint = zone->GetSafePoint();
				m_pp.boatid = 0;
				m_pp.boat[0] = 0;
				m_pp.x = safePoint.x;
				m_pp.y = safePoint.y;
				m_pp.z = safePoint.z;
			}
			else
			{
				//The Kunark zones force the client to the wrong coords if boat name is set in PP, this is the workaround
				if(zone->GetZoneID() == timorous || zone->GetZoneID() == firiona)
				{
					auto PPPos = glm::vec4(m_pp.x, m_pp.y, m_pp.z, m_pp.heading);
					float distance = DistanceNoZ(PPPos, boat->GetPosition());
					if(distance >= RuleI(Zone,BoatDistance))
					{
						float z_mod = 0.0f;
						if(m_pp.boatid == Maidens_Voyage)
							z_mod = 76.0f;
						else if(m_pp.boatid == Bloated_Belly)
							z_mod = 20.0f;
						m_pp.x = boat->GetX();
						m_pp.y = boat->GetY();
						m_pp.z = boat->GetZ() + z_mod;
						Log.Out(Logs::Moderate, Logs::Boats, "Kunark boat %s found at %0.2f,%0.2f,%0.2f! %s's location changed to match.", boat->GetName(), boat->GetX(), boat->GetY(), boat->GetZ(), GetName());
						return;
					}
				}

				Log.Out(Logs::Moderate, Logs::Boats, "Boat %s found at %0.2f,%0.2f,%0.2f! %s's location (%0.2f,%0.2f,%0.2f) unchanged.", boat->GetName(), boat->GetX(), boat->GetY(), boat->GetZ(), GetName(), m_pp.x, m_pp.y, m_pp.z);
			}
		}
	}
}

bool Client::HasInstantDisc(uint16 skill_type)
{
	if(GetClass() == MONK)
	{
		if((skill_type == SkillFlyingKick && GetActiveDisc() == disc_thunderkick) || 
			(skill_type == SkillEagleStrike && GetActiveDisc() == disc_ashenhand) || 
			(skill_type == SkillDragonPunch && GetActiveDisc() == disc_silentfist))
			return true;
	}
	else if(GetClass() == SHADOWKNIGHT)
	{
		if(GetActiveDisc() == disc_unholyaura && (skill_type == SPELL_HARM_TOUCH || skill_type == SPELL_HARM_TOUCH2))
			return true;
	}

	return false;
}

void Client::SendMerchantEnd()
{
	MerchantSession = 0;
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopEndConfirm, 2);
	outapp->pBuffer[0] = 0x0a;
	outapp->pBuffer[1] = 0x66;
	QueuePacket(outapp);
	safe_delete(outapp);
	Save();
}

void Client::Consent(uint8 permission, char name[64], uint32 offline_charid)
{
	uint32 charid = CharacterID();
	if(offline_charid > 0)
		charid = offline_charid;

	if(permission == 1)
	{
		//Add Consent
		if(offline_charid == 0)
			consent_list.push_back(name);
		database.SaveCharacterConsent(charid, name);
	}
	else
	{
		//Remove Consent
		if(offline_charid == 0)
			consent_list.remove(name);
		database.DeleteCharacterConsent(charid, name);
	}
}

bool Client::LoadCharacterConsent()
{
	consent_list.clear();
	std::string query = StringFormat("SELECT consented_name FROM `character_consent` WHERE `id` = %u GROUP by consented_name", CharacterID());
	auto results = database.QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) 
	{
		char name[64];
		strncpy(name, row[0], 64);
		consent_list.push_back(name);
	}
	return true;
}

float Client::GetPortHeading(uint16 newx, uint16 newy)
{
	if(zone->GetZoneID() == paineel)
	{
		// To Bank
		if(GetX() > 519 && GetX() < 530 && newx > 540 && newx < 560)
		{
			return 64.0f;
		}
		// To SK guild
		else if(GetY() > 952 && GetY() < 965 && newy >= 900 && newy <= 920)
		{
			return 120.0f;
		}
	}

	return 0.0f;
}

