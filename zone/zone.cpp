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

#include <float.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WINDOWS
#define	snprintf	_snprintf
#define	vsnprintf	_vsnprintf
#else
#include <pthread.h>
#include "../common/unix.h"
#endif

#include "../common/global_define.h"
#include "../common/features.h"
#include "../common/rulesys.h"
#include "../common/seperator.h"
#include "../common/string_util.h"
#include "../common/eqemu_logsys.h"

#include "guild_mgr.h"
#include "map.h"
#include "net.h"
#include "npc.h"
#include "object.h"
#include "pathing.h"
#include "petitions.h"
#include "quest_parser_collection.h"
//#include "remote_call_subscribe.h"
//#include "remote_call_subscribe.h"
#include "spawn2.h"
#include "spawngroup.h"
#include "water_map.h"
#include "worldserver.h"
#include "zone.h"
#include "zone_config.h"

#include <time.h>
#include <ctime>
#include <iostream>

#ifdef _WINDOWS
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#endif



extern bool staticzone;
extern NetConnection net;
extern PetitionList petition_list;
extern QuestParserCollection* parse;
extern uint16 adverrornum;
extern uint32 numclients;
extern WorldServer worldserver;
extern Zone* zone;

Mutex MZoneShutdown;

volatile bool ZoneLoaded = false;
Zone* zone = 0;

bool Zone::Bootup(uint32 iZoneID, uint32 iInstanceID, bool iStaticZone) {
	const char* zonename = database.GetZoneName(iZoneID);

	if (iZoneID == 0 || zonename == 0)
		return false;
	if (zone != 0 || ZoneLoaded) {
		std::cerr << "Error: Zone::Bootup call when zone already booted!" << std::endl;
		worldserver.SetZone(0);
		return false;
	}

	Log.Out(Logs::General, Logs::Status, "Booting %s (%d:%d)", zonename, iZoneID, iInstanceID);

	numclients = 0;
	zone = new Zone(iZoneID, iInstanceID, zonename);

	//init the zone, loads all the data, etc
	if (!zone->Init(iStaticZone)) {
		safe_delete(zone);
		std::cerr << "Zone->Init failed" << std::endl;
		worldserver.SetZone(0);
		return false;
	}
	zone->zonemap = Map::LoadMapFile(zone->map_name);
	zone->watermap = WaterMap::LoadWaterMapfile(zone->map_name);
	zone->pathing = PathManager::LoadPathFile(zone->map_name);

	char tmp[10];
	if (database.GetVariable("loglevel",tmp, 9)) {
		int log_levels[4];
		if (atoi(tmp)>9){ //Server is using the new code
			for(int i=0;i<4;i++){
				if (((int)tmp[i]>=48) && ((int)tmp[i]<=57))
					log_levels[i]=(int)tmp[i]-48; //get the value to convert it to an int from the ascii value
				else
					log_levels[i]=0; //set to zero on a bogue char
			}
			zone->loglevelvar = log_levels[0];
			Log.Out(Logs::General, Logs::Status, "General logging level: %i", zone->loglevelvar);
			zone->merchantvar = log_levels[1];
			Log.Out(Logs::General, Logs::Status, "Merchant logging level: %i", zone->merchantvar);
			zone->tradevar = log_levels[2];
			Log.Out(Logs::General, Logs::Status, "Trade logging level: %i", zone->tradevar);
			zone->lootvar = log_levels[3];
			Log.Out(Logs::General, Logs::Status, "Loot logging level: %i", zone->lootvar);
		}
		else {
			zone->loglevelvar = uint8(atoi(tmp)); //continue supporting only command logging (for now)
			zone->merchantvar = 0;
			zone->tradevar = 0;
			zone->lootvar = 0;
		}
	}	

	ZoneLoaded = true;

	worldserver.SetZone(iZoneID, iInstanceID);

	Log.Out(Logs::General, Logs::Normal, "---- Zone server %s, listening on port:%i ----", zonename, ZoneConfig::get()->ZonePort);
	Log.Out(Logs::General, Logs::Status, "Zone Bootup: %s (%i: %i)", zonename, iZoneID, iInstanceID);
	parse->Init();
	UpdateWindowTitle();
	zone->GetTimeSync();

	/* Set Logging */

	Log.StartFileLogs(StringFormat("%s_version_%u_inst_id_%u_port_%u", zone->GetShortName(), zone->GetInstanceVersion(), zone->GetInstanceID(), ZoneConfig::get()->ZonePort));

	return true;
}

//this really loads the objects into entity_list
bool Zone::LoadZoneObjects() {

	std::string query = StringFormat("SELECT id, zoneid, xpos, ypos, zpos, heading, "
                                    "itemid, charges, objectname, type, icon, unknown08, "
                                    "unknown10, unknown20, unknown24, unknown76 FROM object "
                                    "WHERE zoneid = %i AND (version = %u OR version = -1)",
                                    zoneid, instanceversion);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
		Log.Out(Logs::General, Logs::Error, "Error Loading Objects from DB: %s",results.ErrorMessage().c_str());
		return false;
    }

    Log.Out(Logs::General, Logs::Status, "Loading Objects from DB...");
    for (auto row = results.begin(); row != results.end(); ++row) {
        if (atoi(row[9]) == 0)
        {
            // Type == 0 - Static Object
            const char* shortname = database.GetZoneName(atoi(row[1]), false); // zoneid -> zone_shortname

            if (!shortname)
                continue;

            Door d;
            memset(&d, 0, sizeof(d));

            strn0cpy(d.zone_name, shortname, sizeof(d.zone_name));
            d.db_id = 1000000000 + atoi(row[0]); // Out of range of normal use for doors.id
            d.door_id = -1; // Client doesn't care if these are all the same door_id
            d.pos_x = atof(row[2]); // xpos
            d.pos_y = atof(row[3]); // ypos
            d.pos_z = atof(row[4]); // zpos
            d.heading = atof(row[5]); // heading

            strn0cpy(d.door_name, row[8], sizeof(d.door_name)); // objectname
            // Strip trailing "_ACTORDEF" if present. Client won't accept it for doors.
            int len = strlen(d.door_name);
            if ((len > 9) && (memcmp(&d.door_name[len - 9], "_ACTORDEF", 10) == 0))
                d.door_name[len - 9] = '\0';

            memcpy(d.dest_zone, "NONE", 5);

            if ((d.size = atoi(row[11])) == 0) // unknown08 = optional size percentage
                d.size = 100;

            switch (d.opentype = atoi(row[12])) // unknown10 = optional request_nonsolid (0 or 1 or experimental number)
            {
                case 0:
                    d.opentype = 31;
                    break;
                case 1:
                    d.opentype = 9;
                    break;
            }

            d.incline = atoi(row[13]); // unknown20 = optional model incline value
            d.client_version_mask = 0xFFFFFFFF; //We should load the mask from the zone.

            Doors* door = new Doors(&d);
            entity_list.AddDoor(door);
        }

        Object_Struct data = {0};
        uint32 id = 0;
        uint32 icon = 0;
        uint32 type = 0;
        uint32 itemid = 0;
        uint32 idx = 0;
        int16 charges = 0;

        id	= (uint32)atoi(row[0]);
        data.zone_id = atoi(row[1]);
        data.x = atof(row[2]);
        data.y = atof(row[3]);
        data.z = atof(row[4]);
        data.heading = atof(row[5]);
		itemid = (uint32)atoi(row[6]);
		charges	= (int16)atoi(row[7]);
        strcpy(data.object_name, row[8]);
        type = (uint8)atoi(row[9]);
        icon = (uint32)atoi(row[10]);
		data.object_type = type;
		data.linked_list_addr[0] = 0;
        data.linked_list_addr[1] = 0;
		data.unknown010				= (uint32)atoi(row[11]);
		data.charges				= charges;
		data.maxcharges				= charges;
			

        ItemInst* inst = nullptr;
        //FatherNitwit: this dosent seem to work...
        //tradeskill containers do not have an itemid of 0... at least what I am seeing
        if (itemid == 0) {
            // Generic tradeskill container
            inst = new ItemInst(ItemInstWorldContainer);
        }
        else {
            // Groundspawn object
            inst = database.CreateItem(itemid);
        }

		// Load child objects if container
		if (inst && inst->IsType(ItemClassContainer)) {
			database.LoadWorldContainer(id, inst);
			for (uint8 i=0; i<10; i++) {
				const ItemInst* b_inst = inst->GetItem(i);
				if (b_inst) {
					data.itemsinbag[i] = b_inst->GetID();
				}
			}
		}

        Object* object = new Object(id, type, icon, data, inst);
        entity_list.AddObject(object, false);

        safe_delete(inst);
    }

	return true;
}

//this also just loads into entity_list, not really into zone
bool Zone::LoadGroundSpawns() {
	Ground_Spawns groundspawn;

	memset(&groundspawn, 0, sizeof(groundspawn));
	int gsindex=0;
	Log.Out(Logs::General, Logs::Status, "Loading Ground Spawns from DB...");
	database.LoadGroundSpawns(zoneid, GetInstanceVersion(), &groundspawn);
	uint32 ix=0;
	char* name=0;
	uint32 gsnumber=0;
	for(gsindex=0;gsindex<50;gsindex++){
		if(groundspawn.spawn[gsindex].item>0 && groundspawn.spawn[gsindex].item<500000){
			ItemInst* inst = nullptr;
			inst = database.CreateItem(groundspawn.spawn[gsindex].item);
			gsnumber=groundspawn.spawn[gsindex].max_allowed;
			ix=0;
			if(inst){
				name = groundspawn.spawn[gsindex].name;
				for(ix=0;ix<gsnumber;ix++){
					Object* object = new Object(inst,name,groundspawn.spawn[gsindex].max_x,groundspawn.spawn[gsindex].min_x,groundspawn.spawn[gsindex].max_y,groundspawn.spawn[gsindex].min_y,groundspawn.spawn[gsindex].max_z,groundspawn.spawn[gsindex].heading,groundspawn.spawn[gsindex].respawntimer);//new object with id of 10000+
					entity_list.AddObject(object, false);
				}
				safe_delete(inst);
			}
		}
	}
	return(true);
}

int Zone::SaveTempItem(uint32 merchantid, uint32 npcid, uint32 item, int32 charges, bool sold) {
	int freeslot = 0;
	std::list<MerchantList> merlist = merchanttable[merchantid];
	std::list<MerchantList>::const_iterator itr;
	uint32 i = 1;
	for (itr = merlist.begin(); itr != merlist.end(); ++itr) {
		MerchantList ml = *itr;
		if (ml.item == item)
			return 0;

		// Account for merchant lists with gaps in them.
		if (ml.slot >= i)
			i = ml.slot + 1;
	}
	std::list<TempMerchantList> tmp_merlist = tmpmerchanttable[npcid];
	std::list<TempMerchantList>::const_iterator tmp_itr;
	bool update_charges = false;
	TempMerchantList ml;
	while (freeslot == 0 && !update_charges) {
		freeslot = i;
		for (tmp_itr = tmp_merlist.begin(); tmp_itr != tmp_merlist.end(); ++tmp_itr) {
			ml = *tmp_itr;
			if (ml.item == item) {
				update_charges = true;
				freeslot = 0;
				break;
			}
			if ((ml.slot == i) || (ml.origslot == i)) {
				freeslot = 0;
			}
		}
		i++;
	}
	if (update_charges) {
		tmp_merlist.clear();
		std::list<TempMerchantList> oldtmp_merlist = tmpmerchanttable[npcid];
		for (tmp_itr = oldtmp_merlist.begin(); tmp_itr != oldtmp_merlist.end(); ++tmp_itr) {
			TempMerchantList ml2 = *tmp_itr;
			if(ml2.item != item)
				tmp_merlist.push_back(ml2);
		}
		if (sold)
			ml.charges = ml.charges + charges;
		else
			ml.charges = charges;
		if (!ml.origslot)
			ml.origslot = ml.slot;
		if (charges > 0) {
			database.SaveMerchantTemp(npcid, ml.origslot, item, ml.charges);
			tmp_merlist.push_back(ml);
		}
		else {
			database.DeleteMerchantTemp(npcid, ml.origslot);
		}
		tmpmerchanttable[npcid] = tmp_merlist;

		if (sold)
			return ml.slot;

	}
	if (freeslot) {
		if (charges < 0) //sanity check only, shouldnt happen
			charges = 0x7FFF;
		database.SaveMerchantTemp(npcid, freeslot, item, charges);
		tmp_merlist = tmpmerchanttable[npcid];
		TempMerchantList ml2;
		ml2.charges = charges;
		ml2.item = item;
		ml2.npcid = npcid;
		ml2.slot = freeslot;
		ml2.origslot = ml2.slot;
		tmp_merlist.push_back(ml2);
		tmpmerchanttable[npcid] = tmp_merlist;
	}
	return freeslot;
}

uint32 Zone::GetTempMerchantQuantity(uint32 NPCID, uint32 Slot) {

	std::list<TempMerchantList> TmpMerchantList = tmpmerchanttable[NPCID];
	std::list<TempMerchantList>::const_iterator Iterator;

	for (Iterator = TmpMerchantList.begin(); Iterator != TmpMerchantList.end(); ++Iterator)
		if ((*Iterator).slot == Slot)
			return (*Iterator).charges;

	return 0;
}

void Zone::LoadTempMerchantData() {
	Log.Out(Logs::General, Logs::Status, "Loading Temporary Merchant Lists...");
	std::string query = StringFormat(
		"SELECT								   "
		"DISTINCT ml.npcid,					   "
		"ml.slot,							   "
		"ml.charges,						   "
		"ml.itemid							   "
		"FROM								   "
		"merchantlist_temp ml,				   "
		"spawnentry se,						   "
		"spawn2 s2							   "
		"WHERE								   "
		"ml.npcid = se.npcid				   "
		"AND se.spawngroupid = s2.spawngroupid "
		"AND s2.zone = '%s' AND s2.version = %i "
		"ORDER BY ml.slot					   ", GetShortName(), GetInstanceVersion());
	auto results = database.QueryDatabase(query);
	if (!results.Success()) {
		return;
	}
	std::map<uint32, std::list<TempMerchantList> >::iterator cur;
	uint32 npcid = 0;
	for (auto row = results.begin(); row != results.end(); ++row) {
		TempMerchantList ml;
		ml.npcid = atoul(row[0]);
		if (npcid != ml.npcid){
			cur = tmpmerchanttable.find(ml.npcid);
			if (cur == tmpmerchanttable.end()) {
				std::list<TempMerchantList> empty;
				tmpmerchanttable[ml.npcid] = empty;
				cur = tmpmerchanttable.find(ml.npcid);
			}
			npcid = ml.npcid;
		}
		ml.slot = atoul(row[1]);
		ml.charges = atoul(row[2]);
		ml.item = atoul(row[3]);
		ml.origslot = ml.slot;
		cur->second.push_back(ml);
	}
	pQueuedMerchantsWorkID = 0;
}

void Zone::LoadNewMerchantData(uint32 merchantid) {

	std::list<MerchantList> merlist;
	std::string query = StringFormat("SELECT item, slot, faction_required, level_required, "
                                     "classes_required FROM merchantlist WHERE merchantid=%d ORDER BY slot", merchantid);
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for(auto row = results.begin(); row != results.end(); ++row) {
        MerchantList ml;
        ml.id = merchantid;
        ml.item = atoul(row[0]);
        ml.slot = atoul(row[1]);
        ml.faction_required = atoul(row[2]);
        ml.level_required = atoul(row[3]);
        ml.classes_required = atoul(row[4]);
       merlist.push_back(ml);
    }

    merchanttable[merchantid] = merlist;
}

void Zone::GetMerchantDataForZoneLoad() {
	Log.Out(Logs::General, Logs::Status, "Loading Merchant Lists...");
	std::string query = StringFormat(
		"SELECT																		   "
		"DISTINCT ml.merchantid,													   "
		"ml.slot,																	   "
		"ml.item,																	   "
		"ml.faction_required,														   "
		"ml.level_required,															   "
		"ml.classes_required													   "
		"FROM																		   "
		"merchantlist AS ml,														   "
		"npc_types AS nt,															   "
		"spawnentry AS se,															   "
		"spawn2 AS s2																   "
		"WHERE nt.merchant_id = ml.merchantid AND nt.id = se.npcid					   "
		"AND se.spawngroupid = s2.spawngroupid AND s2.zone = '%s' AND s2.version = %i  "
		"ORDER BY ml.slot															   ", GetShortName(), GetInstanceVersion());
	auto results = database.QueryDatabase(query);
	std::map<uint32, std::list<MerchantList> >::iterator cur;
	uint32 npcid = 0;
	if (results.RowCount() == 0) {
		Log.Out(Logs::General, Logs::None, "No Merchant Data found for %s.", GetShortName());
		return;
	}
	for (auto row = results.begin(); row != results.end(); ++row) {
		MerchantList ml;
		ml.id = atoul(row[0]);
		if (npcid != ml.id) {
			cur = merchanttable.find(ml.id);
			if (cur == merchanttable.end()) {
				std::list<MerchantList> empty;
				merchanttable[ml.id] = empty;
				cur = merchanttable.find(ml.id);
			}
			npcid = ml.id;
		}

		std::list<MerchantList>::iterator iter = cur->second.begin();
		bool found = false;
		while (iter != cur->second.end()) {
			if ((*iter).item == ml.id) {
				found = true;
				break;
			}
			++iter;
		}

		if (found) {
			continue;
		}

		ml.slot = atoul(row[1]);
		ml.item = atoul(row[2]);
		ml.faction_required = atoul(row[3]);
		ml.level_required = atoul(row[4]);
		ml.classes_required = atoul(row[5]);
		cur->second.push_back(ml);
	}

}

void Zone::LoadLevelEXPMods(){
	level_exp_mod.clear();
    const std::string query = "SELECT level, exp_mod, aa_exp_mod FROM level_exp_mods";
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        Log.Out(Logs::General, Logs::Error, "Error in ZoneDatabase::LoadEXPLevelMods()");
        return;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        uint32 index = atoi(row[0]);
		float exp_mod = atof(row[1]);
		float aa_exp_mod = atof(row[2]);
		level_exp_mod[index].ExpMod = exp_mod;
		level_exp_mod[index].AAExpMod = aa_exp_mod;
    }

}


bool Zone::IsLoaded() {
	return ZoneLoaded;
}

void Zone::Shutdown(bool quite)
{
	if (!ZoneLoaded)
		return;

	entity_list.StopMobAI();

	std::map<uint32,NPCType *>::iterator itr;
	while(!zone->npctable.empty()) {
		itr=zone->npctable.begin();
		delete itr->second;
		zone->npctable.erase(itr);
	}

	Log.Out(Logs::General, Logs::Status, "Zone Shutdown: %s (%i)", zone->GetShortName(), zone->GetZoneID());
	petition_list.ClearPetitions();
	zone->GotCurTime(false);
	if (!quite)
		Log.Out(Logs::General, Logs::Normal, "Zone shutdown: going to sleep");
	ZoneLoaded = false;

//	RemoteCallSubscriptionHandler::Instance()->ClearAllConnections();
	zone->ResetAuth();
	safe_delete(zone);
	entity_list.ClearAreas();
	parse->ReloadQuests(true);
	UpdateWindowTitle();

	Log.CloseFileLogs();
}

void Zone::LoadZoneDoors(const char* zone, int16 version)
{
	Log.Out(Logs::General, Logs::Status, "Loading doors for %s ...", zone);

	uint32 maxid;
	int32 count = database.GetDoorsCount(&maxid, zone, version);
	if(count < 1) {
		Log.Out(Logs::General, Logs::Status, "... No doors loaded.");
		return;
	}

	Door *dlist = new Door[count];

	if(!database.LoadDoors(count, dlist, zone, version)) {
		Log.Out(Logs::General, Logs::Error, "... Failed to load doors.");
		delete[] dlist;
		return;
	}

	int r;
	Door *d = dlist;
	for(r = 0; r < count; r++, d++) {
		Doors* newdoor = new Doors(d);
		entity_list.AddDoor(newdoor);
	}
	delete[] dlist;
}

Zone::Zone(uint32 in_zoneid, uint32 in_instanceid, const char* in_short_name)
:	initgrids_timer(10000),
	autoshutdown_timer((RuleI(Zone, AutoShutdownDelay))),
	clientauth_timer(AUTHENTICATION_TIMEOUT * 1000),
	spawn2_timer(1000),
	qglobal_purge_timer(30000),
	hotzone_timer(120000),
	m_SafePoint(0.0f,0.0f,0.0f),
	m_Graveyard(0.0f,0.0f,0.0f,0.0f)
{
	zoneid = in_zoneid;
	instanceid = in_instanceid;
	instanceversion = database.GetInstanceVersion(instanceid);
	pers_instance = false;
	zonemap = nullptr;
	watermap = nullptr;
	pathing = nullptr;
	qGlobals = nullptr;
	default_ruleset = 0;

	loglevelvar = 0;
	merchantvar = 0;
	tradevar = 0;
	lootvar = 0;

	short_name = strcpy(new char[strlen(in_short_name)+1], in_short_name);
	strlwr(short_name);
	memset(file_name, 0, sizeof(file_name));
	long_name = 0;
	aggroedmobs =0;
	pgraveyard_id = 0;
	pgraveyard_zoneid = 0;
	pMaxClients = 0;
	pQueuedMerchantsWorkID = 0;
	pvpzone = false;
	if(database.GetServerType() == 1)
		pvpzone = true;
	database.GetZoneLongName(short_name, &long_name, file_name, &m_SafePoint.x, &m_SafePoint.y, &m_SafePoint.z, &pgraveyard_id, &pMaxClients);
	if(graveyard_id() > 0)
	{
		Log.Out(Logs::General, Logs::None, "Graveyard ID is %i.", graveyard_id());
		bool GraveYardLoaded = database.GetZoneGraveyard(graveyard_id(), &pgraveyard_zoneid, &m_Graveyard.x, &m_Graveyard.y, &m_Graveyard.z, &m_Graveyard.w);
		if(GraveYardLoaded)
			Log.Out(Logs::General, Logs::None, "Loaded a graveyard for zone %s: graveyard zoneid is %u at %s.", short_name, graveyard_zoneid(), to_string(m_Graveyard).c_str());
		else
			Log.Out(Logs::General, Logs::Error, "Unable to load the graveyard id %i for zone %s.", graveyard_id(), short_name);
	}
	if (long_name == 0) {
		long_name = strcpy(new char[18], "Long zone missing");
	}
	autoshutdown_timer.Start(AUTHENTICATION_TIMEOUT * 1000, false);
	Weather_Timer = new Timer(60000);
	Weather_Timer->Start();
	Log.Out(Logs::General, Logs::None, "The next weather check for zone: %s will be in %i seconds.", short_name, Weather_Timer->GetRemainingTime()/1000);
	zone_weather = 0;
	weather_intensity = 0;
	blocked_spells = nullptr;
	totalBS = 0;
	aas = nullptr;
	totalAAs = 0;
	gottime = false;
	idle = false;

	Instance_Shutdown_Timer = nullptr;
	bool is_perma = false;
	if(instanceid > 0)
	{
		uint32 rem = database.GetTimeRemainingInstance(instanceid, is_perma);

		if(!is_perma)
		{
			if(rem < 150) //give some leeway to people who are zoning in 2.5 minutes to finish zoning in and get ported out
				rem = 150;
			Instance_Timer = new Timer(rem * 1000);
		}
		else
		{
			pers_instance = true;
			Instance_Timer = nullptr;
		}
	}
	else
	{
		Instance_Timer = nullptr;
	}
	map_name = nullptr;
	Instance_Warning_timer = nullptr;
	database.QGlobalPurge();

}

Zone::~Zone() {
	spawn2_list.Clear();
	safe_delete(zonemap);
	safe_delete(watermap);
	safe_delete(pathing);
	if (worldserver.Connected()) {
		worldserver.SetZone(0);
	}
	safe_delete_array(short_name);
	safe_delete_array(long_name);
	safe_delete(Weather_Timer);
	NPCEmoteList.Clear();
	zone_point_list.Clear();
	entity_list.Clear();
	ClearBlockedSpells();

	safe_delete(Instance_Timer);
	safe_delete(Instance_Shutdown_Timer);
	safe_delete(Instance_Warning_timer);
	safe_delete(qGlobals);
	safe_delete_array(map_name);

	if(aas != nullptr) {
		int r;
		for(r = 0; r < totalAAs; r++) {
			uchar *data = (uchar *) aas[r];
			safe_delete_array(data);
		}
		safe_delete_array(aas);
	}

}

//Modified for timezones.
bool Zone::Init(bool iStaticZone) {
	SetStaticZone(iStaticZone);

	Log.Out(Logs::General, Logs::Status, "Loading spawn conditions...");
	if(!spawn_conditions.LoadSpawnConditions(short_name, instanceid)) {
		Log.Out(Logs::General, Logs::Error, "Loading spawn conditions failed, continuing without them.");
	}

	Log.Out(Logs::General, Logs::Status, "Loading static zone points...");
	if (!database.LoadStaticZonePoints(&zone_point_list, short_name, GetInstanceVersion())) {
		Log.Out(Logs::General, Logs::Error, "Loading static zone points failed.");
		return false;
	}

	Log.Out(Logs::General, Logs::Status, "Loading spawn groups...");
	if (!database.LoadSpawnGroups(short_name, GetInstanceVersion(), &spawn_group_list)) {
		Log.Out(Logs::General, Logs::Error, "Loading spawn groups failed.");
		return false;
	}

	Log.Out(Logs::General, Logs::Status, "Loading spawn2 points...");
	if (!database.PopulateZoneSpawnList(zoneid, spawn2_list, GetInstanceVersion()))
	{
		Log.Out(Logs::General, Logs::Error, "Loading spawn2 points failed.");
		return false;
	}

	Log.Out(Logs::General, Logs::Status, "Loading player corpses...");
	if (!database.LoadCharacterCorpses(zoneid, instanceid)) {
		Log.Out(Logs::General, Logs::Error, "Loading player corpses failed.");
		return false;
	}

	Log.Out(Logs::General, Logs::Status, "Loading traps...");
	if (!database.LoadTraps(short_name, GetInstanceVersion()))
	{
		Log.Out(Logs::General, Logs::Error, "Loading traps failed.");
		return false;
	}

	Log.Out(Logs::General, Logs::Status, "Loading ground spawns...");
	if (!LoadGroundSpawns())
	{
		Log.Out(Logs::General, Logs::Error, "Loading ground spawns failed. continuing.");
	}

	Log.Out(Logs::General, Logs::Status, "Loading World Objects from DB...");
	if (!LoadZoneObjects())
	{
		Log.Out(Logs::General, Logs::Error, "Loading World Objects failed. continuing.");
	}

	Log.Out(Logs::General, Logs::Status, "Flushing old respawn timers...");
	database.QueryDatabase("DELETE FROM `respawn_times` WHERE (`start` + `duration`) < UNIX_TIMESTAMP(NOW())");

	//load up the zone's doors (prints inside)
	zone->LoadZoneDoors(zone->GetShortName(), zone->GetInstanceVersion());
	zone->LoadBlockedSpells(zone->GetZoneID());

	//clear trader items if we are loading the bazaar
	if(strncasecmp(short_name,"bazaar",6)==0) {
		database.DeleteTraderItem(0);
	}

	zone->LoadNPCEmotes(&NPCEmoteList);

	//Load AA information
	adverrornum = 500;
	LoadAAs();

	//Load merchant data
	adverrornum = 501;
	zone->GetMerchantDataForZoneLoad();

	//Load temporary merchant data
	adverrornum = 502;
	zone->LoadTempMerchantData();

	if (RuleB(Zone, LevelBasedEXPMods))
		zone->LoadLevelEXPMods();

	adverrornum = 503;
	petition_list.ClearPetitions();
	petition_list.ReadDatabase();

	//load the zone config file.
	if (!LoadZoneCFG(zone->GetShortName(), zone->GetInstanceVersion(), true)) // try loading the zone name...
		LoadZoneCFG(zone->GetFileName(), zone->GetInstanceVersion()); // if that fails, try the file name, then load defaults

	if(RuleManager::Instance()->GetActiveRulesetID() != default_ruleset)
	{
		std::string r_name = RuleManager::Instance()->GetRulesetName(&database, default_ruleset);
		if(r_name.size() > 0)
		{
			RuleManager::Instance()->LoadRules(&database, r_name.c_str());
		}
	}

	Log.Out(Logs::General, Logs::Status, "Loading timezone data...");
	zone->zone_time.setEQTimeZone(database.GetZoneTZ(zoneid, GetInstanceVersion()));

	Log.Out(Logs::General, Logs::Status, "Init Finished: ZoneID = %d, Time Offset = %d", zoneid, zone->zone_time.getEQTimeZone());

	LoadTickItems();

	//MODDING HOOK FOR ZONE INIT
	mod_init();

	return true;
}

void Zone::ReloadStaticData() {
	Log.Out(Logs::General, Logs::Status, "Reloading Zone Static Data...");

	Log.Out(Logs::General, Logs::Status, "Reloading static zone points...");
	zone_point_list.Clear();
	if (!database.LoadStaticZonePoints(&zone_point_list, GetShortName(), GetInstanceVersion())) {
		Log.Out(Logs::General, Logs::Error, "Loading static zone points failed.");
	}

	Log.Out(Logs::General, Logs::Status, "Reloading traps...");
	entity_list.RemoveAllTraps();
	if (!database.LoadTraps(GetShortName(), GetInstanceVersion()))
	{
		Log.Out(Logs::General, Logs::Error, "Reloading traps failed.");
	}

	Log.Out(Logs::General, Logs::Status, "Reloading ground spawns...");
	if (!LoadGroundSpawns())
	{
		Log.Out(Logs::General, Logs::Error, "Reloading ground spawns failed. continuing.");
	}

	entity_list.RemoveAllObjects();
	Log.Out(Logs::General, Logs::Status, "Reloading World Objects from DB...");
	if (!LoadZoneObjects())
	{
		Log.Out(Logs::General, Logs::Error, "Reloading World Objects failed. continuing.");
	}

	entity_list.RemoveAllDoors();
	zone->LoadZoneDoors(zone->GetShortName(), zone->GetInstanceVersion());
	entity_list.RespawnAllDoors();

	NPCEmoteList.Clear();
	zone->LoadNPCEmotes(&NPCEmoteList);

	//load the zone config file.
	if (!LoadZoneCFG(zone->GetShortName(), zone->GetInstanceVersion(), true)) // try loading the zone name...
		LoadZoneCFG(zone->GetFileName(), zone->GetInstanceVersion()); // if that fails, try the file name, then load defaults

	Log.Out(Logs::General, Logs::Status, "Zone Static Data Reloaded.");
}

bool Zone::LoadZoneCFG(const char* filename, uint16 instance_id, bool DontLoadDefault)
{
	memset(&newzone_data, 0, sizeof(NewZone_Struct));
	if(instance_id == 0)
	{
		map_name = nullptr;
		if(!database.GetZoneCFG(database.GetZoneID(filename), 0, &newzone_data, can_bind,
			can_combat, can_levitate, can_castoutdoor, is_city, is_hotzone, zone_type, default_ruleset, &map_name, can_bind_others, skip_los))
		{
			Log.Out(Logs::General, Logs::Error, "Error loading the Zone Config.");
			return false;
		}
	}
	else
	{
		//Fall back to base zone if we don't find the instance version.
		map_name = nullptr;
		if(!database.GetZoneCFG(database.GetZoneID(filename), instance_id, &newzone_data, can_bind,
			can_combat, can_levitate, can_castoutdoor, is_city, is_hotzone,  zone_type, default_ruleset, &map_name, can_bind_others, skip_los))
		{
			safe_delete_array(map_name);
			if(!database.GetZoneCFG(database.GetZoneID(filename), 0, &newzone_data, can_bind,
			can_combat, can_levitate, can_castoutdoor, is_city, is_hotzone,  zone_type, default_ruleset, &map_name, can_bind_others, skip_los))
			{
				Log.Out(Logs::General, Logs::Error, "Error loading the Zone Config.");
				return false;
			}
		}
	}

	//overwrite with our internal variables
	strcpy(newzone_data.zone_short_name, GetShortName());
	strcpy(newzone_data.zone_long_name, GetLongName());
	strcpy(newzone_data.zone_short_name2, GetShortName());

	Log.Out(Logs::General, Logs::Status, "Successfully loaded Zone Config.");
	return true;
}

bool Zone::SaveZoneCFG() {
	return database.SaveZoneCFG(GetZoneID(), GetInstanceVersion(), &newzone_data);
}

void Zone::AddAuth(ServerZoneIncomingClient_Struct* szic) {
	ZoneClientAuth_Struct* zca = new ZoneClientAuth_Struct;
	memset(zca, 0, sizeof(ZoneClientAuth_Struct));
	zca->ip = szic->ip;
	zca->wid = szic->wid;
	zca->accid = szic->accid;
	zca->admin = szic->admin;
	zca->charid = szic->charid;
	zca->tellsoff = szic->tellsoff;
	strn0cpy(zca->charname, szic->charname, sizeof(zca->charname));
	strn0cpy(zca->lskey, szic->lskey, sizeof(zca->lskey));
	zca->stale = false;
	client_auth_list.Insert(zca);
	zca->version = szic->version;
}

void Zone::RemoveAuth(const char* iCharName)
{
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (strcasecmp(zca->charname, iCharName) == 0) {
		iterator.RemoveCurrent();
		return;
		}
		iterator.Advance();
	}
}

void Zone::ResetAuth()
{
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		iterator.RemoveCurrent();
	}
}

bool Zone::GetAuth(uint32 iIP, const char* iCharName, uint32* oWID, uint32* oAccID, uint32* oCharID, int16* oStatus, char* oLSKey, bool* oTellsOff, uint32* oVersionbit) {
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		ZoneClientAuth_Struct* zca = iterator.GetData();
		if (strcasecmp(zca->charname, iCharName) == 0) {
				if(oWID)
				*oWID = zca->wid;
				if(oAccID)
				*oAccID = zca->accid;
				if(oCharID)
				*oCharID = zca->charid;
				if(oStatus)
				*oStatus = zca->admin;
				if(oTellsOff)
				*oTellsOff = zca->tellsoff;
				zca->stale = true;
				if(oVersionbit)
				*oVersionbit = zca->version;
			return true;
		}
		iterator.Advance();
	}
	return false;
}

uint32 Zone::CountAuth() {
	LinkedListIterator<ZoneClientAuth_Struct*> iterator(client_auth_list);

	int x = 0;
	iterator.Reset();
	while (iterator.MoreElements()) {
		x++;
		iterator.Advance();
	}
	return x;
}

bool Zone::Process() {
	spawn_conditions.Process();

	if(spawn2_timer.Check()) {
		LinkedListIterator<Spawn2*> iterator(spawn2_list);

		Inventory::CleanDirty();

		iterator.Reset();
		while (iterator.MoreElements()) {
			if (iterator.GetData()->Process()) {
				iterator.Advance();
			}
			else {
				iterator.RemoveCurrent();
			}
		}
	}
	if(initgrids_timer.Check()) {
		//delayed grid loading stuff.
		initgrids_timer.Disable();
		LinkedListIterator<Spawn2*> iterator(spawn2_list);

		iterator.Reset();
		while (iterator.MoreElements()) {
			iterator.GetData()->LoadGrid();
			iterator.Advance();
		}
	}

	if(!staticzone) {
		if (autoshutdown_timer.Check()) {
			StartShutdownTimer();
			if (numclients == 0) {
				return false;
			}
		}
	}

	if(GetInstanceID() > 0)
	{
		if(Instance_Timer != nullptr && Instance_Shutdown_Timer == nullptr)
		{
			if(Instance_Timer->Check())
			{
				entity_list.GateAllClients();
				database.DeleteInstance(GetInstanceID());
				Instance_Shutdown_Timer = new Timer(20000); //20 seconds
			}
		}
		else if(Instance_Shutdown_Timer != nullptr)
		{
			if(Instance_Shutdown_Timer->Check())
			{
				StartShutdownTimer();
				return false;
			}
		}
	}

	if(Weather_Timer->Check())
	{
		Weather_Timer->Disable();
		this->ChangeWeather();
	}

	if(qGlobals)
	{
		if(qglobal_purge_timer.Check())
		{
			qGlobals->PurgeExpiredGlobals();
		}
	}

	if (clientauth_timer.Check()) {
		LinkedListIterator<ZoneClientAuth_Struct*> iterator2(client_auth_list);

		iterator2.Reset();
		while (iterator2.MoreElements()) {
			if (iterator2.GetData()->stale)
				iterator2.RemoveCurrent();
			else {
				iterator2.GetData()->stale = true;
				iterator2.Advance();
			}
		}
	}

	if(hotzone_timer.Check()) { UpdateHotzone(); }

	return true;
}

void Zone::ChangeWeather()
{
	if(!HasWeather())
	{
		Weather_Timer->Disable();
		return;
	}

	int chance = zone->random.Int(0, 3);
	uint8 rainchance = zone->newzone_data.rain_chance[chance];
	uint8 rainduration = zone->newzone_data.rain_duration[chance];
	uint8 snowchance = zone->newzone_data.snow_chance[chance];
	uint8 snowduration = zone->newzone_data.snow_duration[chance];
	uint32 weathertimer = 0;
	uint16 tmpweather = zone->random.Int(0, 100);
	uint8 duration = 0;
	uint8 tmpOldWeather = zone->zone_weather;
	bool changed = false;

	if(tmpOldWeather == 0)
	{
		if(rainchance > 0 || snowchance > 0)
		{
			uint8 intensity = zone->random.Int(1, 3);
			if((rainchance > snowchance) || (rainchance == snowchance))
			{
				//It's gunna rain!
				if(rainchance >= tmpweather)
				{
					if(rainduration == 0)
						duration = 1;
					else
						duration = rainduration*3; //Duration is 1 EQ hour which is 3 earth minutes.

					weathertimer = (duration*60)*1000;
					Weather_Timer->Start(weathertimer);
					zone->zone_weather = 1;
					zone->weather_intensity = intensity;
					changed = true;
				}
			}
			else
			{
				//It's gunna snow!
				if(snowchance >= tmpweather)
				{
					if(snowduration == 0)
						duration = 1;
					else
						duration = snowduration*3;
					weathertimer = (duration*60)*1000;
					Weather_Timer->Start(weathertimer);
					zone->zone_weather = 2;
					zone->weather_intensity = intensity;
					changed = true;
				}
			}
		}
	}
	else
	{
		changed = true;
		//We've had weather, now taking a break
		if(tmpOldWeather == 1)
		{
			if(rainduration == 0)
				duration = 1;
			else
				duration = rainduration*3; //Duration is 1 EQ hour which is 3 earth minutes.

			weathertimer = (duration*60)*1000;
			Weather_Timer->Start(weathertimer);
			zone->zone_weather = 0;
			zone->weather_intensity = 0;
		}
		else if(tmpOldWeather == 2)
		{
			if(snowduration == 0)
				duration = 1;
			else
				duration = snowduration*3; //Duration is 1 EQ hour which is 3 earth minutes.

			weathertimer = (duration*60)*1000;
			Weather_Timer->Start(weathertimer);
			zone->zone_weather = 0;
			zone->weather_intensity = 0;
		}
	}

	if(changed == false)
	{
		if(weathertimer == 0)
		{
			uint32 weatherTimerRule = RuleI(Zone, WeatherTimer);
			weathertimer = weatherTimerRule*1000;
			Weather_Timer->Start(weathertimer);
		}
		Log.Out(Logs::General, Logs::None, "The next weather check for zone: %s will be in %i seconds.", zone->GetShortName(), Weather_Timer->GetRemainingTime()/1000);
	}
	else
	{
		Log.Out(Logs::General, Logs::None, "The weather for zone: %s has changed. Old weather was = %i. New weather is = %i The next check will be in %i seconds. Rain chance: %i, Rain duration: %i, Snow chance %i, Snow duration: %i", zone->GetShortName(), tmpOldWeather, zone_weather,Weather_Timer->GetRemainingTime()/1000,rainchance,rainduration,snowchance,snowduration);
		this->weatherSend();
	}
}

bool Zone::HasWeather()
{
	uint8 rain1 = zone->newzone_data.rain_chance[0];
	uint8 rain2 = zone->newzone_data.rain_chance[1];
	uint8 rain3 = zone->newzone_data.rain_chance[2];
	uint8 rain4 = zone->newzone_data.rain_chance[3];
	uint8 snow1 = zone->newzone_data.snow_chance[0];
	uint8 snow2 = zone->newzone_data.snow_chance[1];
	uint8 snow3 = zone->newzone_data.snow_chance[2];
	uint8 snow4 = zone->newzone_data.snow_chance[3];

	if(rain1 == 0 && rain2 == 0 && rain3 == 0 && rain4 == 0 && snow1 == 0 && snow2 == 0 && snow3 == 0 && snow4 == 0)
		return false;
	else
		return true;
}

void Zone::StartShutdownTimer(uint32 set_time) {
	if (set_time > autoshutdown_timer.GetRemainingTime()) {
		if (set_time == (RuleI(Zone, AutoShutdownDelay)))
		{
			set_time = database.getZoneShutDownDelay(GetZoneID(), GetInstanceVersion());
		}
		autoshutdown_timer.Start(set_time, false);
	}
}

bool Zone::Depop(bool StartSpawnTimer) {
std::map<uint32,NPCType *>::iterator itr;
	entity_list.Depop(StartSpawnTimer);

#ifdef DEPOP_INVALIDATES_NPC_TYPES_CACHE
	// Refresh npctable, getting current info from database.
	while(!npctable.empty()) {
		itr=npctable.begin();
		delete itr->second;
		npctable.erase(itr);
	}
#endif

	return true;
}

void Zone::ClearNPCTypeCache(int id) {
	if (id <= 0) {
		auto iter = npctable.begin();
		while (iter != npctable.end()) {
			delete iter->second;
			++iter;
		}
		npctable.clear();
	}
	else {
		auto iter = npctable.begin();
		while (iter != npctable.end()) {
			if (iter->first == (uint32)id) {
				delete iter->second;
				npctable.erase(iter);
				return;
			}
			++iter;
		}
	}
}

void Zone::Repop(uint32 delay) {

	if(!Depop())
		return;

	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while (iterator.MoreElements()) {
		iterator.RemoveCurrent();
	}

	quest_manager.ClearAllTimers();

	if (!database.PopulateZoneSpawnList(zoneid, spawn2_list, GetInstanceVersion(), delay))
		Log.Out(Logs::General, Logs::None, "Error in Zone::Repop: database.PopulateZoneSpawnList failed");

	initgrids_timer.Start();

	//MODDING HOOK FOR REPOP
	mod_repop();
}

void Zone::GetTimeSync()
{
	if (worldserver.Connected() && !gottime) {
		ServerPacket* pack = new ServerPacket(ServerOP_GetWorldTime, 0);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Zone::SetDate(uint16 year, uint8 month, uint8 day, uint8 hour, uint8 minute)
{
	if (worldserver.Connected()) {
		ServerPacket* pack = new ServerPacket(ServerOP_SetWorldTime, sizeof(eqTimeOfDay));
		eqTimeOfDay* eqtod = (eqTimeOfDay*)pack->pBuffer;
		eqtod->start_eqtime.minute=minute;
		eqtod->start_eqtime.hour=hour;
		eqtod->start_eqtime.day=day;
		eqtod->start_eqtime.month=month;
		eqtod->start_eqtime.year=year;
		eqtod->start_realtime=time(0);
		printf("Setting master date on world server to: %d-%d-%d %d:%d (%d)\n", year, month, day, hour, minute, (int)eqtod->start_realtime);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void Zone::SetTime(uint8 hour, uint8 minute)
{
	if (worldserver.Connected()) {
		ServerPacket* pack = new ServerPacket(ServerOP_SetWorldTime, sizeof(eqTimeOfDay));
		eqTimeOfDay* eqtod = (eqTimeOfDay*)pack->pBuffer;
		zone_time.getEQTimeOfDay(time(0), &eqtod->start_eqtime);
		eqtod->start_eqtime.minute=minute;
		eqtod->start_eqtime.hour=hour;
		eqtod->start_realtime=time(0);
		printf("Setting master time on world server to: %d:%d (%d)\n", hour, minute, (int)eqtod->start_realtime);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

ZonePoint* Zone::GetClosestZonePoint(const glm::vec3& location, uint32 to, Client* client, float max_distance) {
	LinkedListIterator<ZonePoint*> iterator(zone_point_list);
	ZonePoint* closest_zp = 0;
	float closest_dist = FLT_MAX;
	float max_distance2 = max_distance * max_distance;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* zp = iterator.GetData();
		uint32 mask_test = client->GetClientVersionBit();
		if(!(zp->client_version_mask & mask_test))
		{
			iterator.Advance();
			continue;
		}

		if (zp->target_zone_id == to)
		{
            auto dist = Distance(glm::vec2(zp->x, zp->y), glm::vec2(location));
			if ((zp->x == -BEST_Z_INVALID || zp->x == BEST_Z_INVALID) && (zp->y == -BEST_Z_INVALID || zp->y == BEST_Z_INVALID))
				dist = 0;

			if (dist < closest_dist)
			{
				closest_zp = zp;
				closest_dist = dist;
			}
		}
		iterator.Advance();
	}

	if(closest_dist > 400.0f && closest_dist < max_distance2)
	{
		if(client)
			client->CheatDetected(MQZoneUnknownDest, location.x, location.y, location.z); // Someone is trying to use /zone
		Log.Out(Logs::General, Logs::Status, "WARNING: Closest zone point for zone id %d is %f, you might need to update your zone_points table if you dont arrive at the right spot.", to, closest_dist);
		Log.Out(Logs::General, Logs::Status, "<Real Zone Points>. %s", to_string(location).c_str());
	}

	if(closest_dist > max_distance2)
		closest_zp = nullptr;

	if(!closest_zp)
		closest_zp = GetClosestZonePointWithoutZone(location.x, location.y, location.z, client);

	return closest_zp;
}

ZonePoint* Zone::GetClosestZonePoint(const glm::vec3& location, const char* to_name, Client* client, float max_distance) {
	if(to_name == nullptr)
		return GetClosestZonePointWithoutZone(location.x, location.y, location.z, client, max_distance);
	return GetClosestZonePoint(location, database.GetZoneID(to_name), client, max_distance);
}

ZonePoint* Zone::GetClosestZonePointWithoutZone(float x, float y, float z, Client* client, float max_distance) {
	LinkedListIterator<ZonePoint*> iterator(zone_point_list);
	ZonePoint* closest_zp = 0;
	float closest_dist = FLT_MAX;
	float max_distance2 = max_distance*max_distance;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		ZonePoint* zp = iterator.GetData();
		uint32 mask_test = client->GetClientVersionBit();

		if(!(zp->client_version_mask & mask_test))
		{
			iterator.Advance();
			continue;
		}

		float delta_x = zp->x - x;
		float delta_y = zp->y - y;
		if(zp->x == -BEST_Z_INVALID || zp->x == BEST_Z_INVALID)
			delta_x = 0;
		if(zp->y == -BEST_Z_INVALID || zp->y == BEST_Z_INVALID)
			delta_y = 0;

		float dist = delta_x*delta_x+delta_y*delta_y;///*+(zp->z-z)*(zp->z-z)*/;
		if (dist < closest_dist)
		{
			closest_zp = zp;
			closest_dist = dist;
		}
		iterator.Advance();
	}
	if(closest_dist > max_distance2)
		closest_zp = nullptr;

	return closest_zp;
}

bool ZoneDatabase::LoadStaticZonePoints(LinkedList<ZonePoint*>* zone_point_list, const char* zonename, uint32 version)
{

	zone_point_list->Clear();
	zone->numzonepoints = 0;
	std::string query = StringFormat("SELECT x, y, z, target_x, target_y, "
                                    "target_z, target_zone_id, heading, target_heading, "
                                    "number, target_instance, client_version_mask "
                                    "FROM zone_points WHERE zone='%s' AND (version=%i OR version=-1) "
                                    "ORDER BY number", zonename, version);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

    for (auto row = results.begin(); row != results.end(); ++row) {
        ZonePoint* zp = new ZonePoint;

        zp->x = atof(row[0]);
        zp->y = atof(row[1]);
        zp->z = atof(row[2]);
        zp->target_x = atof(row[3]);
        zp->target_y = atof(row[4]);
        zp->target_z = atof(row[5]);
        zp->target_zone_id = atoi(row[6]);
        zp->heading = atof(row[7]);
        zp->target_heading = atof(row[8]);
        zp->number = atoi(row[9]);
        zp->target_zone_instance = atoi(row[10]);
        zp->client_version_mask = (uint32)strtoul(row[11], nullptr, 0);

        zone_point_list->Insert(zp);

        zone->numzonepoints++;
    }

	return true;
}

void Zone::SpawnStatus(Mob* client) {
	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	uint32 x = 0;
	iterator.Reset();
	while(iterator.MoreElements())
	{
		if (iterator.GetData()->timer.GetRemainingTime() == 0xFFFFFFFF)
			client->Message(0, "  %d: %1.1f, %1.1f, %1.1f: disabled", iterator.GetData()->GetID(), iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ());
		else
			client->Message(0, "  %d: %1.1f, %1.1f, %1.1f: %1.2f", iterator.GetData()->GetID(), iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ(), (float)iterator.GetData()->timer.GetRemainingTime() / 1000);

		x++;
		iterator.Advance();
	}
	client->Message(0, "%i spawns listed.", x);
}

void Zone::ShowEnabledSpawnStatus(Mob* client)
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	int x = 0;
	int iEnabledCount = 0;

	iterator.Reset();

	while(iterator.MoreElements())
	{
		if (iterator.GetData()->timer.GetRemainingTime() != 0xFFFFFFFF)
		{
			client->Message(0, "  %d: %1.1f, %1.1f, %1.1f: %1.2f", iterator.GetData()->GetID(), iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ(), (float)iterator.GetData()->timer.GetRemainingTime() / 1000);
			iEnabledCount++;
		}

		x++;
		iterator.Advance();
	}

	client->Message(0, "%i of %i spawns listed.", iEnabledCount, x);
}

void Zone::ShowDisabledSpawnStatus(Mob* client)
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	int x = 0;
	int iDisabledCount = 0;

	iterator.Reset();

	while(iterator.MoreElements())
	{
		if (iterator.GetData()->timer.GetRemainingTime() == 0xFFFFFFFF)
		{
			client->Message(0, "  %d: %1.1f, %1.1f, %1.1f: disabled", iterator.GetData()->GetID(), iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ());
			iDisabledCount++;
		}

		x++;
		iterator.Advance();
	}

	client->Message(0, "%i of %i spawns listed.", iDisabledCount, x);
}

void Zone::ShowSpawnStatusByID(Mob* client, uint32 spawnid)
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);
	int x = 0;
	int iSpawnIDCount = 0;

	iterator.Reset();

	while(iterator.MoreElements())
	{
		if (iterator.GetData()->GetID() == spawnid)
		{
			if (iterator.GetData()->timer.GetRemainingTime() == 0xFFFFFFFF)
				client->Message(0, "  %d: %1.1f, %1.1f, %1.1f: disabled", iterator.GetData()->GetID(), iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ());
			else
				client->Message(0, "  %d: %1.1f, %1.1f, %1.1f: %1.2f", iterator.GetData()->GetID(), iterator.GetData()->GetX(), iterator.GetData()->GetY(), iterator.GetData()->GetZ(), (float)iterator.GetData()->timer.GetRemainingTime() / 1000);

			iSpawnIDCount++;

			break;
		}

		x++;
		iterator.Advance();
	}

	if(iSpawnIDCount > 0)
		client->Message(0, "%i of %i spawns listed.", iSpawnIDCount, x);
	else
		client->Message(0, "No matching spawn id was found in this zone.");
}


bool Zone::RemoveSpawnEntry(uint32 spawnid)
{
	LinkedListIterator<Spawn2*> iterator(spawn2_list);


	iterator.Reset();
	while(iterator.MoreElements())
	{
		if(iterator.GetData()->GetID() == spawnid)
		{
			iterator.RemoveCurrent();
			return true;
		}
		else
		iterator.Advance();
	}
return false;
}

bool Zone::RemoveSpawnGroup(uint32 in_id) {
	if(spawn_group_list.RemoveSpawnGroup(in_id))
		return true;
	else
		return false;
}


// Added By Hogie
bool ZoneDatabase::GetDecayTimes(npcDecayTimes_Struct* npcCorpseDecayTimes) {

	const std::string query = "SELECT varname, value FROM variables WHERE varname LIKE 'decaytime%%' ORDER BY varname";
	auto results = QueryDatabase(query);
	if (!results.Success())
        return false;

	int index = 0;
    for (auto row = results.begin(); row != results.end(); ++row, ++index) {
        Seperator sep(row[0]);
        npcCorpseDecayTimes[index].minlvl = atoi(sep.arg[1]);
        npcCorpseDecayTimes[index].maxlvl = atoi(sep.arg[2]);

        if (atoi(row[1]) > 7200)
            npcCorpseDecayTimes[index].seconds = 720;
        else
            npcCorpseDecayTimes[index].seconds = atoi(row[1]);
    }

	return true;
}

void Zone::weatherSend()
{
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Weather, sizeof(Weather_Struct));
	Weather_Struct* ws = (Weather_Struct*)outapp->pBuffer;

	if(zone_weather>0)
		ws->type = zone_weather-1;
	if(zone_weather>0)
		ws->intensity = zone->weather_intensity;
	entity_list.QueueClients(0, outapp);
	safe_delete(outapp);
}

bool Zone::HasGraveyard() {
	bool Result = false;

	if(graveyard_zoneid() > 0)
		Result = true;

	return Result;
}

void Zone::SetGraveyard(uint32 zoneid, const glm::vec4& graveyardPosition) {
	pgraveyard_zoneid = zoneid;
	m_Graveyard = graveyardPosition;
}

void Zone::LoadBlockedSpells(uint32 zoneid)
{
	if(!blocked_spells)
	{
		totalBS = database.GetBlockedSpellsCount(zoneid);
		if(totalBS > 0){
			blocked_spells = new ZoneSpellsBlocked[totalBS];
			if(!database.LoadBlockedSpells(totalBS, blocked_spells, zoneid))
			{
				Log.Out(Logs::General, Logs::Error, "... Failed to load blocked spells.");
				ClearBlockedSpells();
			}
		}
	}
}

void Zone::ClearBlockedSpells()
{
	if(blocked_spells){
		safe_delete_array(blocked_spells);
		totalBS = 0;
	}
}

bool Zone::IsSpellBlocked(uint32 spell_id, const glm::vec3& location)
{
	if (blocked_spells)
	{
		bool exception = false;
		bool block_all = false;
		for (int x = 0; x < totalBS; x++)
		{
			if (blocked_spells[x].spellid == spell_id)
			{
				exception = true;
			}

			if (blocked_spells[x].spellid == 0)
			{
				block_all = true;
			}
		}

		for (int x = 0; x < totalBS; x++)
		{
			// If spellid is 0, block all spells in the zone
			if (block_all)
			{
				// If the same zone has entries other than spellid 0, they act as exceptions and are allowed
				if (exception)
				{
					return false;
				}
				else
				{
					return true;
				}
			}
			else
			{
				if (spell_id != blocked_spells[x].spellid)
				{
					continue;
				}

				switch (blocked_spells[x].type)
				{
					case 1:
					{
						return true;
						break;
					}
					case 2:
					{
						if (IsWithinAxisAlignedBox(location, blocked_spells[x].m_Location - blocked_spells[x].m_Difference, blocked_spells[x].m_Location + blocked_spells[x].m_Difference))
							return true;
						break;
					}
					default:
					{
						continue;
						break;
					}
				}
			}
		}
	}
	return false;
}

const char* Zone::GetSpellBlockedMessage(uint32 spell_id, const glm::vec3& location)
{
	if(blocked_spells)
	{
		for(int x = 0; x < totalBS; x++)
		{
			if(spell_id != blocked_spells[x].spellid && blocked_spells[x].spellid != 0)
				continue;

			switch(blocked_spells[x].type)
			{
				case 1:
				{
					return blocked_spells[x].message;
					break;
				}
				case 2:
				{
					if(IsWithinAxisAlignedBox(location, blocked_spells[x].m_Location - blocked_spells[x].m_Difference, blocked_spells[x].m_Location + blocked_spells[x].m_Difference))
						return blocked_spells[x].message;
					break;
				}
				default:
				{
					continue;
					break;
				}
			}
		}
	}
	return "Error: Message String Not Found\0";
}

void Zone::SetInstanceTimer(uint32 new_duration)
{
	if(Instance_Timer)
	{
		Instance_Timer->Start(new_duration * 1000);
	}
}

void Zone::UpdateQGlobal(uint32 qid, QGlobal newGlobal)
{
	if(newGlobal.npc_id != 0)
		return;

	if(newGlobal.char_id != 0)
		return;

	if(newGlobal.zone_id == GetZoneID() || newGlobal.zone_id == 0)
	{
		if(qGlobals)
		{
			qGlobals->AddGlobal(qid, newGlobal);
		}
		else
		{
			qGlobals = new QGlobalCache();
			qGlobals->AddGlobal(qid, newGlobal);
		}
	}
}

void Zone::DeleteQGlobal(std::string name, uint32 npcID, uint32 charID, uint32 zoneID)
{
	if(qGlobals)
	{
		qGlobals->RemoveGlobal(name, npcID, charID, zoneID);
	}
}

void Zone::LoadNPCEmotes(LinkedList<NPC_Emote_Struct*>* NPCEmoteList)
{

	NPCEmoteList->Clear();
    const std::string query = "SELECT emoteid, event_, type, text FROM npc_emotes";
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for (auto row = results.begin(); row != results.end(); ++row)
    {
        NPC_Emote_Struct* nes = new NPC_Emote_Struct;
        nes->emoteid = atoi(row[0]);
        nes->event_ = atoi(row[1]);
        nes->type = atoi(row[2]);
        strn0cpy(nes->text, row[3], sizeof(nes->text));
        NPCEmoteList->Insert(nes);
    }

}

void Zone::ReloadWorld(uint32 Option){
	if (Option == 0) {
		entity_list.ClearAreas();
		parse->ReloadQuests();
		RuleManager::Instance()->LoadRules(&database, RuleManager::Instance()->GetActiveRuleset());
		zone->Repop(0);
	}
}

void Zone::LoadTickItems()
{
	tick_items.clear();

    const std::string query = "SELECT it_itemid, it_chance, it_level, it_qglobal, it_bagslot FROM item_tick";
    auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        return;
    }


    for (auto row = results.begin(); row != results.end(); ++row) {
        if(atoi(row[0]) == 0)
            continue;

        item_tick_struct ti_tmp;
		ti_tmp.itemid = atoi(row[0]);
		ti_tmp.chance = atoi(row[1]);
		ti_tmp.level = atoi(row[2]);
		ti_tmp.bagslot = (int16)atoi(row[4]);
		ti_tmp.qglobal = std::string(row[3]);
		tick_items[atoi(row[0])] = ti_tmp;

    }

}

uint32 Zone::GetSpawnKillCount(uint32 in_spawnid) {
	LinkedListIterator<Spawn2*> iterator(spawn2_list);

	iterator.Reset();
	while(iterator.MoreElements())
	{
		if(iterator.GetData()->GetID() == in_spawnid)
		{
			return(iterator.GetData()->killcount);
		}
		iterator.Advance();
	}
	return 0;
}

void Zone::UpdateHotzone()
{
    std::string query = StringFormat("SELECT hotzone FROM zone WHERE short_name = '%s'", GetShortName());
    auto results = database.QueryDatabase(query);
    if (!results.Success())
        return;

    if (results.RowCount() == 0)
        return;

    auto row = results.begin();

    is_hotzone = atoi(row[0]) == 0 ? false: true;
}

bool Zone::IsBoatZone()
{
	// This only returns true for zones that contain actual boats. It should not be used for zones that only have 
	// controllable boats, or the Halas raft.

	static const int16 boatzones[] = { qeynos, freporte, erudnext, butcher, oot, erudsxing };

	int8 boatzonessize = sizeof(boatzones) / sizeof(boatzones[0]);
	for (int i = 0; i < boatzonessize; i++) {
		if (GetZoneID() == boatzones[i]) {
			return true;
		}
	}

	return false;
}

bool Zone::IsDesertZone()
{

	if(GetZoneID() != oot && GetZoneID() < codecay)
	{
		if(!HasWeather() && CanCastOutdoor() && !IsCity())
			return true;

		// This are zones that do get weather, but with a very little chance (<10%).
		static const int16 desertzones[] = { nro, sro, oasis, lavastorm, shadeweaver, skyfire };

		int8 desertzonessize = sizeof(desertzones) / sizeof(desertzones[0]);
		for (int i = 0; i < desertzonessize; i++) {
			if (GetZoneID() == desertzones[i]) {
				return true;
			}
		}
	}

	return false;
}

bool Zone::IsBindArea(float x_coord, float y_coord)
{
	if(CanBindOthers())
	{
		// NK gypsies
		if(GetZoneID() == northkarana)
		{
			if(x_coord >= -215 && x_coord <= -109 &&  y_coord >= -688 && y_coord <= -600)
				return true;
			else
				return false;
		}
		// RM gypsies
		else if(GetZoneID() == rathemtn)
		{
			if(x_coord >= 1395 && x_coord <= 1474 && y_coord >= 3918 && y_coord <= 4008)
				return true;
			else
				return false;
		}
		else
			return true;
	}

	return false;
}

bool Zone::IsWaterZone()
{
	if(GetZoneID() == kedge || GetZoneID() == powater)
		return true;

	return false;
}
