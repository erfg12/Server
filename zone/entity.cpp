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
#include "../common/debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>

#ifdef _WINDOWS
#include <process.h>
#else
#include <pthread.h>
#include "../common/unix.h"
#endif

#include "../common/features.h"
#include "../common/guilds.h"

#include "guild_mgr.h"
#include "net.h"
#include "petitions.h"
#include "quest_parser_collection.h"
#include "raids.h"
#include "string_ids.h"
#include "worldserver.h"
#include "remote_call_subscribe.h"
#include "remote_call_subscribe.h"

#ifdef _WINDOWS
	#define snprintf	_snprintf
	#define strncasecmp	_strnicmp
	#define strcasecmp	_stricmp
#endif

extern Zone *zone;
extern volatile bool ZoneLoaded;
extern WorldServer worldserver;
extern NetConnection net;
extern uint32 numclients;
extern PetitionList petition_list;

extern char errorname[32];
extern uint16 adverrornum;

Entity::Entity()
{
	id = 0;
}

Entity::~Entity()
{

}

Client *Entity::CastToClient()
{
	if (this == 0x00) {
		std::cout << "CastToClient error (nullptr)" << std::endl;
		DebugBreak();
		return 0;
	}
#ifdef _EQDEBUG
	if (!IsClient()) {
		std::cout << "CastToClient error (not client?)" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<Client *>(this);
}

NPC *Entity::CastToNPC()
{
#ifdef _EQDEBUG
	if (!IsNPC()) {
		std::cout << "CastToNPC error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<NPC *>(this);
}

Mob *Entity::CastToMob()
{
#ifdef _EQDEBUG
	if (!IsMob()) {
		std::cout << "CastToMob error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<Mob *>(this);
}


Trap *Entity::CastToTrap()
{
#ifdef DEBUG
	if (!IsTrap()) {
		//std::cout << "CastToTrap error" << std::endl;
		return 0;
	}
#endif
	return static_cast<Trap *>(this);
}

Corpse *Entity::CastToCorpse()
{
#ifdef _EQDEBUG
	if (!IsCorpse()) {
		std::cout << "CastToCorpse error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<Corpse *>(this);
}

Object *Entity::CastToObject()
{
#ifdef _EQDEBUG
	if (!IsObject()) {
		std::cout << "CastToObject error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<Object *>(this);
}

/*Group* Entity::CastToGroup() {
#ifdef _EQDEBUG
	if(!IsGroup()) {
		std::cout << "CastToGroup error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<Group*>(this);
}*/

Doors *Entity::CastToDoors()
{
	return static_cast<Doors *>(this);
}

Beacon *Entity::CastToBeacon()
{
	return static_cast<Beacon *>(this);
}

const Client *Entity::CastToClient() const
{
	if (this == 0x00) {
		std::cout << "CastToClient error (nullptr)" << std::endl;
		DebugBreak();
		return 0;
	}
#ifdef _EQDEBUG
	if (!IsClient()) {
		std::cout << "CastToClient error (not client?)" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<const Client *>(this);
}

const NPC *Entity::CastToNPC() const
{
#ifdef _EQDEBUG
	if (!IsNPC()) {
		std::cout << "CastToNPC error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<const NPC *>(this);
}

const Mob *Entity::CastToMob() const
{
#ifdef _EQDEBUG
	if (!IsMob()) {
		std::cout << "CastToMob error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<const Mob *>(this);
}

const Trap *Entity::CastToTrap() const
{
#ifdef DEBUG
	if (!IsTrap()) {
		//std::cout << "CastToTrap error" << std::endl;
		return 0;
	}
#endif
	return static_cast<const Trap *>(this);
}

const Corpse *Entity::CastToCorpse() const
{
#ifdef _EQDEBUG
	if (!IsCorpse()) {
		std::cout << "CastToCorpse error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<const Corpse *>(this);
}

const Object *Entity::CastToObject() const
{
#ifdef _EQDEBUG
	if (!IsObject()) {
		std::cout << "CastToObject error" << std::endl;
		DebugBreak();
		return 0;
	}
#endif
	return static_cast<const Object *>(this);
}

const Doors *Entity::CastToDoors() const
{
	return static_cast<const Doors *>(this);
}

const Beacon* Entity::CastToBeacon() const
{
	return static_cast<const Beacon *>(this);
}

EntityList::EntityList()
{
	// set up ids between 1 and 1500
	// neither client or server performs well if you have
	// enough entities to exhaust this list
	for (uint16 i = 1; i <= 1500; i++)
		free_ids.push(i);
}

EntityList::~EntityList()
{
	//must call this before the list is destroyed, or else it will try to
	//delete the NPCs in the list, which it cannot do.
	RemoveAllLocalities();
}

bool EntityList::CanAddHateForMob(Mob *p)
{
	int count = 0;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *npc = it->second;
		if (npc->IsOnHatelist(p))
			count++;
		// no need to continue if we already hit the limit
		if (count > 3)
			return false;
		++it;
	}

	if (count <= 2)
		return true;
	return false;
}

void EntityList::AddClient(Client *client)
{
	client->SetID(GetFreeID());
	client_list.insert(std::pair<uint16, Client *>(client->GetID(), client));
	mob_list.insert(std::pair<uint16, Mob *>(client->GetID(), client));
}


void EntityList::TrapProcess()
{

	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->IsBoatZone())
	{
		if (numclients < 1 && !zone->idle_timer.Enabled() && !zone->idle)
		{
			zone->idle_timer.Start(RuleI(Zone, IdleTimer)); // idle timer from when the last player left the zone.
		}
		else if(numclients >= 1 && zone->idle)
		{
			if(zone->idle_timer.Enabled())
				zone->idle_timer.Disable();
			zone->idle = false;
		}

		if(zone->idle_timer.Check())
		{
			zone->idle_timer.Disable();
			zone->idle = true;
		}

		if(zone->idle)
		{
			return;
		}
	}

	if (trap_list.empty()) {
		net.trap_timer.Disable();
		return;
	}

	auto it = trap_list.begin();
	while (it != trap_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = trap_list.erase(it);
		} else {
			++it;
		}
	}
}


// Debug function -- checks to see if group_list has any nullptr entries.
// Meant to be called after each group-related function, in order
// to track down bugs.
void EntityList::CheckGroupList (const char *fname, const int fline)
{
	std::list<Group *>::iterator it;

	for (it = group_list.begin(); it != group_list.end(); ++it)
	{
		if (*it == nullptr)
		{
			Log.DebugCategory(EQEmuLogSys::General, EQEmuLogSys::Error, "nullptr group, %s:%i", fname, fline);
		}
	}
}

void EntityList::GroupProcess()
{
	if (numclients < 1)
		return;

	if (group_list.empty()) {
		net.group_timer.Disable();
		return;
	}

	for (auto &group : group_list)
		group->Process();

#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
}


void EntityList::RaidProcess()
{
	if (numclients < 1)
		return;

	if (raid_list.empty()) {
		net.raid_timer.Disable();
		return;
	}

	for (auto &raid : raid_list)
		raid->Process();
}

void EntityList::DoorProcess()
{

	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->IsBoatZone())
	{
		if (numclients < 1 && !zone->idle_timer.Enabled() && !zone->idle)
		{
			zone->idle_timer.Start(RuleI(Zone, IdleTimer)); // idle timer from when the last player left the zone.
		}
		else if(numclients >= 1 && zone->idle)
		{
			if(zone->idle_timer.Enabled())
				zone->idle_timer.Disable();
			zone->idle = false;
		}

		if(zone->idle_timer.Check())
		{
			zone->idle_timer.Disable();
			zone->idle = true;
		}

		if(zone->idle)
		{
			return;
		}
	}

	if (door_list.empty()) {
		net.door_timer.Disable();
		return;
	}

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = door_list.erase(it);
		}
		++it;
	}
}

void EntityList::ObjectProcess()
{

	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->IsBoatZone())
	{
		if (numclients < 1 && !zone->idle_timer.Enabled() && !zone->idle)
		{
			zone->idle_timer.Start(RuleI(Zone, IdleTimer)); // idle timer from when the last player left the zone.
		}
		else if(numclients >= 1 && zone->idle)
		{
			if(zone->idle_timer.Enabled())
				zone->idle_timer.Disable();
			zone->idle = false;
		}

		if(zone->idle_timer.Check())
		{
			zone->idle_timer.Disable();
			zone->idle = true;
		}

		if(zone->idle)
		{
			return;
		}
	}

	if (object_list.empty()) {
		net.object_timer.Disable();
		return;
	}

	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = object_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::CorpseProcess()
{
	if (corpse_list.empty()) {
		net.corpse_timer.Disable(); // No corpses in list
		return;
	}

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::MobProcess()
{

	if(zone && RuleB(Zone, IdleWhenEmpty) && !zone->IsBoatZone())
	{
		if (numclients < 1 && !zone->idle_timer.Enabled() && !zone->idle)
		{
			zone->idle_timer.Start(RuleI(Zone, IdleTimer)); // idle timer from when the last player left the zone.
			//_log(EQMAC__LOG, "Entity Process: Number of clients has dropped to 0. Setting idle timer.");
		}
		else if(numclients >= 1 && zone->idle)
		{
			if(zone->idle_timer.Enabled())
				zone->idle_timer.Disable();
			zone->idle = false;
			//_log(EQMAC__LOG, "Entity Process: A player has entered the zone, leaving idle state.");
		}

		if(zone->idle_timer.Check())
		{
			zone->idle_timer.Disable();
			zone->idle = true;
			//_log(EQMAC__LOG, "Entity Process: Idle timer has expired, zone will now idle.");
		}

		if(zone->idle)
		{
			return;
		}
	}


	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		uint16 id = it->first;
		Mob *mob = it->second;

		size_t sz = mob_list.size();
		bool p_val = mob->Process();
		size_t a_sz = mob_list.size();

		if(a_sz > sz) {
			//increased size can potentially screw with iterators so reset it to current value
			//if buckets are re-orderered we may skip a process here and there but since
			//process happens so often it shouldn't matter much
			it = mob_list.find(id);
			++it;
		} else {
			++it;
		}

		if(!p_val) {
			if(mob->IsNPC()) {
				entity_list.RemoveNPC(id);
			}
			else {
#ifdef _WINDOWS
				struct in_addr in;
				in.s_addr = mob->CastToClient()->GetIP();
				std::cout << "Dropping client: Process=false, ip=" << inet_ntoa(in) << ", port=" << mob->CastToClient()->GetPort() << std::endl;
#endif
				zone->StartShutdownTimer();
				Group *g = GetGroupByMob(mob);
				if(g) {
					Log.DebugCategory(EQEmuLogSys::General, EQEmuLogSys::Error, "About to delete a client still in a group.");
					g->DelMember(mob);
				}
				Raid *r = entity_list.GetRaidByClient(mob->CastToClient());
				if(r) {
					Log.DebugCategory(EQEmuLogSys::General, EQEmuLogSys::Error, "About to delete a client still in a raid.");
					r->MemberZoned(mob->CastToClient());
				}
				entity_list.RemoveClient(id);
			}

			entity_list.RemoveMob(id);
		}
	}
}

void EntityList::BeaconProcess()
{
	auto it = beacon_list.begin();
	while (it != beacon_list.end()) {
		if (!it->second->Process()) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = beacon_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::AddGroup(Group *group)
{
	if (group == nullptr)	//this seems to be happening somehow...
		return;

	uint32 gid = worldserver.NextGroupID();
	if (gid == 0) {
		Log.DebugCategory(EQEmuLogSys::General, EQEmuLogSys::Error, 
				"Unable to get new group ID from world server. group is going to be broken.");
		return;
	}

	AddGroup(group, gid);
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
}

void EntityList::AddGroup(Group *group, uint32 gid)
{
	group->SetID(gid);
	group_list.push_back(group);
	if (!net.group_timer.Enabled())
		net.group_timer.Start();
#if EQDEBUG >= 5
	CheckGroupList(__FILE__, __LINE__);
#endif
}

void EntityList::AddRaid(Raid *raid)
{
	if (raid == nullptr)
		return;

	uint32 gid = worldserver.NextGroupID();
	if (gid == 0) {
		Log.DebugCategory(EQEmuLogSys::General, EQEmuLogSys::Error, 
				"Unable to get new group ID from world server. group is going to be broken.");
		return;
	}

	AddRaid(raid, gid);
}

void EntityList::AddRaid(Raid *raid, uint32 gid)
{
	raid->SetID(gid);
	raid_list.push_back(raid);
	if (!net.raid_timer.Enabled())
		net.raid_timer.Start();
}


void EntityList::AddCorpse(Corpse *corpse, uint32 in_id)
{
	if (corpse == 0)
		return;

	if (in_id == 0xFFFFFFFF)
		corpse->SetID(GetFreeID());
	else
		corpse->SetID(in_id);

	corpse->CalcCorpseName();
	corpse_list.insert(std::pair<uint16, Corpse *>(corpse->GetID(), corpse));

	if (!net.corpse_timer.Enabled())
		net.corpse_timer.Start();
}

void EntityList::AddNPC(NPC *npc, bool SendSpawnPacket, bool dontqueue)
{
	npc->SetID(GetFreeID());
	//npc->SetMerchantProbability((uint8) zone->random.Int(0, 99));
	parse->EventNPC(EVENT_SPAWN, npc, nullptr, "", 0);

	/* Web Interface: NPC Spawn (Pop) */
	if (RemoteCallSubscriptionHandler::Instance()->IsSubscribed("NPC.Position")) {
		std::vector<std::string> params;
		params.push_back(std::to_string((long)npc->GetID()));
		params.push_back(npc->GetCleanName());
		params.push_back(std::to_string((float)npc->GetX()));
		params.push_back(std::to_string((float)npc->GetY()));
		params.push_back(std::to_string((float)npc->GetZ()));
		params.push_back(std::to_string((double)npc->GetHeading())); 
		params.push_back(std::to_string((double)npc->GetClass()));
		params.push_back(std::to_string((double)npc->GetRace()));
		RemoteCallSubscriptionHandler::Instance()->OnEvent("NPC.Position", params);
	}

	uint16 emoteid = npc->GetEmoteID();
	if (emoteid != 0)
		npc->DoNPCEmote(ONSPAWN, emoteid);

	if (SendSpawnPacket) {
		if (dontqueue) { // aka, SEND IT NOW BITCH!
			EQApplicationPacket *app = new EQApplicationPacket;
			npc->CreateSpawnPacket(app, npc);
			QueueClients(npc, app);
			safe_delete(app);
		} else {
			NewSpawn_Struct *ns = new NewSpawn_Struct;
			memset(ns, 0, sizeof(NewSpawn_Struct));
			npc->FillSpawnStruct(ns, 0);	// Not working on player newspawns, so it's safe to use a ForWho of 0
			AddToSpawnQueue(npc->GetID(), &ns);
			safe_delete(ns);
		}
	}

	npc_list.insert(std::pair<uint16, NPC *>(npc->GetID(), npc));
	mob_list.insert(std::pair<uint16, Mob *>(npc->GetID(), npc));
}

void EntityList::AddObject(Object *obj, bool SendSpawnPacket)
{
	obj->SetID(GetFreeID());

	if (SendSpawnPacket) {
		EQApplicationPacket app;
		obj->CreateSpawnPacket(&app);
		#if (EQDEBUG >= 6)
			DumpPacket(&app);
		#endif
		QueueClients(0, &app,false);
	}

	object_list.insert(std::pair<uint16, Object *>(obj->GetID(), obj));

	if (!net.object_timer.Enabled())
		net.object_timer.Start();
}

void EntityList::AddDoor(Doors *door)
{
	door->SetEntityID(GetFreeID());
	door_list.insert(std::pair<uint16, Doors *>(door->GetEntityID(), door));

	if (!net.door_timer.Enabled())
		net.door_timer.Start();
}

void EntityList::AddTrap(Trap *trap)
{
	trap->SetID(GetFreeID());
	trap_list.insert(std::pair<uint16, Trap *>(trap->GetID(), trap));
	if (!net.trap_timer.Enabled())
		net.trap_timer.Start();
}

void EntityList::AddBeacon(Beacon *beacon)
{
	beacon->SetID(GetFreeID());
	beacon_list.insert(std::pair<uint16, Beacon *>(beacon->GetID(), beacon));
}

void EntityList::AddToSpawnQueue(uint16 entityid, NewSpawn_Struct **ns)
{
	uint32 count;
	if ((count = (client_list.size())) == 0)
		return;
	SpawnQueue.Append(*ns);
	NumSpawnsOnQueue++;
	if (tsFirstSpawnOnQueue == 0xFFFFFFFF)
		tsFirstSpawnOnQueue = Timer::GetCurrentTime();
	*ns = nullptr;
}

void EntityList::CheckSpawnQueue()
{
	// Send the stuff if the oldest packet on the queue is older than 50ms -Quagmire
	if (tsFirstSpawnOnQueue != 0xFFFFFFFF && (Timer::GetCurrentTime() - tsFirstSpawnOnQueue) > 50) {
		LinkedListIterator<NewSpawn_Struct *> iterator(SpawnQueue);
		EQApplicationPacket *outapp = 0;

		iterator.Reset();
		while(iterator.MoreElements()) {
			outapp = new EQApplicationPacket;
			Mob::CreateSpawnPacket(outapp, iterator.GetData());
			QueueClients(0, outapp);
			safe_delete(outapp);
			iterator.RemoveCurrent();
		}
		tsFirstSpawnOnQueue = 0xFFFFFFFF;
		NumSpawnsOnQueue = 0;
	}
}

Doors *EntityList::FindDoor(uint8 door_id)
{
	if (door_id == 0 || door_list.empty())
		return nullptr;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (it->second->GetDoorID() == door_id)
			return it->second;
		++it;
	}

	return nullptr;
}

Object *EntityList::FindObject(uint32 object_id)
{
	if (object_id == 0 || object_list.empty())
		return nullptr;

	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (it->second->GetDBID() == object_id)
			return it->second;
		++it;
	}

	return nullptr;
}

Object *EntityList::FindNearbyObject(float x, float y, float z, float radius)
{
	if (object_list.empty())
		return nullptr;

	float ox;
	float oy;
	float oz;

	auto it = object_list.begin();
	while (it != object_list.end()) {
		Object *object = it->second;

		object->GetLocation(&ox, &oy, &oz);

		ox = (x < ox) ? (ox - x) : (x - ox);
		oy = (y < oy) ? (oy - y) : (y - oy);
		oz = (z < oz) ? (oz - z) : (z - oz);

		if ((ox <= radius) && (oy <= radius) && (oz <= radius))
			return object;

		++it;
	}

	return nullptr;
}

bool EntityList::SendZoneDoorsBulk(EQApplicationPacket* app, Client *client, uint8 &count)
{
	if (door_list.empty())
		return false;

	uint32 mask_test = client->GetClientVersionBit();
	uchar doorstruct[sizeof(OldDoor_Struct)];
	uchar packet[sizeof(OldDoor_Struct)*255];
	int16 length = 0;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if ((it->second->GetClientVersionMask() & mask_test) &&
				strlen(it->second->GetDoorName()) > 3)
			count++;
		++it;
	}

	if(count == 0 || count > 255) //doorid is uint8
		return false;

	count = 0;
	Doors *door;
	OldDoor_Struct* nd = (OldDoor_Struct*)doorstruct;
	memset(nd,0,sizeof(OldDoor_Struct));

	it = door_list.begin();
	while (it != door_list.end()) {
		door = it->second;
		if (door && (door->GetClientVersionMask() & mask_test) &&
				strlen(door->GetDoorName()) > 3) {
			memcpy(nd->name, door->GetDoorName(), 16);
			auto position = door->GetPosition();
			nd->xPos = position.x;
			nd->yPos = position.y;
			nd->zPos = position.z;
			nd->heading = position.w;			nd->incline = door->GetIncline();
			nd->size = door->GetSize();
			nd->doorid = door->GetDoorID();
			nd->opentype = door->GetOpenType();
			nd->doorIsOpen = door->GetInvertState() ? !door->IsDoorOpen() : door->IsDoorOpen();
			nd->inverted = door->GetInvertState();
			nd->parameter = door->GetDoorParam();
			
			memcpy(packet+length,doorstruct,sizeof(OldDoor_Struct));
			length += sizeof(OldDoor_Struct);

			count++;
		}
		++it;
	}
		
	int32 deflength = sizeof(OldDoor_Struct)*count;
	int buffer = 2;

	app->SetOpcode(OP_SpawnDoor);
	app->pBuffer = new uchar[sizeof(OldDoor_Struct)*count];
	app->size = buffer + DeflatePacket(packet,length,app->pBuffer+buffer,deflength+buffer);
	OldDoorSpawns_Struct* ds = (OldDoorSpawns_Struct*)app->pBuffer;

	ds->count = count;

	return true;
}

bool EntityList::MakeDoorSpawnPacket(EQApplicationPacket* app, Client *client)
{
	if (door_list.empty())
		return false;

	uint32 mask_test = client->GetClientVersionBit();
	int count = 0;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if ((it->second->GetClientVersionMask() & mask_test) &&
				strlen(it->second->GetDoorName()) > 3)
			count++;
		++it;
	}

	if(count == 0 || count > 500)
		return false;

	uint32 length = count * sizeof(Door_Struct);
	uchar *packet_buffer = new uchar[length];
	memset(packet_buffer, 0, length);
	uchar *ptr = packet_buffer;
	Doors *door;
	Door_Struct nd;

	it = door_list.begin();
	while (it != door_list.end()) {
		door = it->second;
		if (door && (door->GetClientVersionMask() & mask_test) &&
				strlen(door->GetDoorName()) > 3) {
			memset(&nd, 0, sizeof(nd));
			memcpy(nd.name, door->GetDoorName(), 32);
			auto position = door->GetPosition();
			nd.xPos = position.x;
			nd.yPos = position.y;
			nd.zPos = position.z;
			nd.heading = position.w;
			nd.incline = door->GetIncline();
			nd.size = door->GetSize();
			nd.doorId = door->GetDoorID();
			nd.opentype = door->GetOpenType();
			nd.state_at_spawn = door->GetInvertState() ? !door->IsDoorOpen() : door->IsDoorOpen();
			nd.invert_state = door->GetInvertState();
			nd.door_param = door->GetDoorParam();
			memcpy(ptr, &nd, sizeof(nd));
			ptr+=sizeof(nd);
			*(ptr-1)=0x01;
			*(ptr-3)=0x01;
		}
		++it;
	}

	app->SetOpcode(OP_SpawnDoor);
	app->size = length;
	app->pBuffer = packet_buffer;
	return true;
}

Entity *EntityList::GetEntityMob(uint16 id)
{
	return mob_list.count(id) ? mob_list.at(id) : nullptr;
}

Entity *EntityList::GetEntityMob(const char *name)
{
	if (name == 0 || mob_list.empty())
		return 0;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (strcasecmp(it->second->GetName(), name) == 0)
			return it->second;
		++it;
	}

	return nullptr;
}

Entity *EntityList::GetEntityDoor(uint16 id)
{
	return door_list.count(id) ? door_list.at(id) : nullptr;
}

Entity *EntityList::GetEntityCorpse(uint16 id)
{
	return corpse_list.count(id) ? corpse_list.at(id) : nullptr;
}

Entity *EntityList::GetEntityCorpse(const char *name)
{
	if (name == 0 || corpse_list.empty())
		return nullptr;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (strcasecmp(it->second->GetName(), name) == 0)
			return it->second;
		++it;
	}

	return nullptr;
}

Entity *EntityList::GetEntityTrap(uint16 id)
{
	return trap_list.count(id) ? trap_list.at(id) : nullptr;
}

Entity *EntityList::GetEntityObject(uint16 id)
{
	return object_list.count(id) ? object_list.at(id) : nullptr;
}

Entity *EntityList::GetEntityBeacon(uint16 id)
{
	return beacon_list.count(id) ? beacon_list.at(id) : nullptr;
}

Entity *EntityList::GetID(uint16 get_id)
{
	Entity *ent = 0;
	if ((ent = entity_list.GetEntityMob(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityDoor(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityCorpse(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityObject(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityTrap(get_id)) != 0)
		return ent;
	else if ((ent=entity_list.GetEntityBeacon(get_id)) != 0)
		return ent;
	else
		return 0;
}

NPC *EntityList::GetNPCByNPCTypeID(uint32 npc_id)
{
	if (npc_id == 0 || npc_list.empty())
		return nullptr;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->GetNPCTypeID() == npc_id)
			return it->second;
		++it;
	}

	return nullptr;
}

Mob *EntityList::GetMob(uint16 get_id)
{
	Entity *ent = nullptr;

	if (get_id == 0)
		return nullptr;

	if ((ent = entity_list.GetEntityMob(get_id)))
		return ent->CastToMob();
	else if ((ent = entity_list.GetEntityCorpse(get_id)))
		return ent->CastToMob();

	return nullptr;
}

Mob *EntityList::GetMob(const char *name)
{
	Entity* ent = nullptr;

	if (name == 0)
		return nullptr;

	if ((ent = entity_list.GetEntityMob(name)))
		return ent->CastToMob();
	else if ((ent = entity_list.GetEntityCorpse(name)))
		return ent->CastToMob();

	return nullptr;
}

Mob *EntityList::GetMobByNpcTypeID(uint32 get_id)
{
	if (get_id == 0 || mob_list.empty())
		return 0;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->GetNPCTypeID() == get_id)
			return it->second;
		++it;
	}
	return nullptr;
}

bool EntityList::IsMobSpawnedByNpcTypeID(uint32 get_id)
{
	if (get_id == 0 || npc_list.empty())
		return false;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		// Mobs will have a 0 as their GetID() if they're dead
		if (it->second->GetNPCTypeID() == get_id && it->second->GetID() != 0)
			return true;
		++it;
	}

	return false;
}

Object *EntityList::GetObjectByDBID(uint32 id)
{
	if (id == 0 || object_list.empty())
		return nullptr;

	auto it = object_list.begin();
	while (it != object_list.end()) {
		if (it->second->GetDBID() == id)
			return it->second;
		++it;
	}
	return nullptr;
}

Doors *EntityList::GetDoorsByDBID(uint32 id)
{
	if (id == 0 || door_list.empty())
		return nullptr;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (it->second->GetDoorDBID() == id)
			return it->second;
		++it;
	}

	return nullptr;
}

Doors *EntityList::GetDoorsByDoorID(uint32 id)
{
	if (id == 0 || door_list.empty())
		return nullptr;

	auto it = door_list.begin();
	while (it != door_list.end()) {
		if (it->second->CastToDoors()->GetDoorID() == id)
			return it->second;
		++it;
	}

	return nullptr;
}

uint16 EntityList::GetFreeID()
{
	if (free_ids.empty()) { // hopefully this will never be true
		// The client has a hard cap on entity count some where
		// Neither the client or server performs well with a lot entities either
		uint16 newid = 1500;
		while (true) {
			newid++;
			if (GetID(newid) == nullptr)
				return newid;
		}
	}

	uint16 newid = free_ids.front();
	free_ids.pop();
	return newid;
}

// if no language skill is specified, sent with 100 skill
void EntityList::ChannelMessage(Mob *from, uint8 chan_num, uint8 language, const char *message, ...)
{
	ChannelMessage(from, chan_num, language, 100, message);
}

void EntityList::ChannelMessage(Mob *from, uint8 chan_num, uint8 language,
		uint8 lang_skill, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	memcpy(buffer, message, 4096);

	auto it = client_list.begin();
	while(it != client_list.end()) {
		Client *client = it->second;
		eqFilterType filter = FilterNone;
		if (chan_num == 3) //shout
			filter = FilterShouts;
		else if (chan_num == 4) //auction
			filter = FilterAuctions;
		//
		// Only say is limited in range
		if (chan_num != 8 || Distance(client->GetPosition(), from->GetPosition()) < 200)
			if (filter == FilterNone || client->GetFilter(filter) != FilterHide)
				client->ChannelMessageSend(from->GetName(), 0, chan_num, language, lang_skill, buffer);
		++it;
	}
}

void EntityList::ChannelMessageSend(Mob *to, uint8 chan_num, uint8 language, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	memcpy(buffer, message, 4096);

	if (client_list.count(to->GetID()))
		client_list.at(to->GetID())->ChannelMessageSend(0, 0, chan_num, language, buffer);
}

void EntityList::SendZoneSpawns(Client *client)
{
	EQApplicationPacket *app;
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *ent = it->second;
		if (!(ent->InZone()) || (ent->IsClient())) {
			if (ent->CastToClient()->GMHideMe(client)) {
				++it;
				continue;
			}
		}

		app = new EQApplicationPacket;
		it->second->CastToMob()->CreateSpawnPacket(app); // TODO: Use zonespawns opcode instead
		client->QueuePacket(app, true, Client::CLIENT_CONNECTED);
		safe_delete(app);
		++it;
	}
}

void EntityList::SendZoneSpawnsBulk(Client *client)
{
	NewSpawn_Struct ns;
	Mob *spawn;
	uint32 maxspawns = 100;

	if (maxspawns > mob_list.size())
		maxspawns = mob_list.size();
	BulkZoneSpawnPacket *bzsp = new BulkZoneSpawnPacket(client, maxspawns);
	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		spawn = it->second;
		if (spawn && spawn->InZone()) {
			if (spawn->IsClient() && (spawn->CastToClient()->GMHideMe(client)))
				continue;
			memset(&ns, 0, sizeof(NewSpawn_Struct));
			spawn->FillSpawnStruct(&ns, client);
			bzsp->AddSpawn(&ns);
		}
	}
	safe_delete(bzsp);
}

//this is a hack to handle a broken spawn struct
void EntityList::SendZonePVPUpdates(Client *to)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		if(c->GetPVP())
			c->SendAppearancePacket(AT_PVP, c->GetPVP(), true, false, to);
		++it;
	}
}

void EntityList::SendZoneCorpses(Client *client)
{
	EQApplicationPacket *app;

	for (auto it = corpse_list.begin(); it != corpse_list.end(); ++it) {
		Corpse *ent = it->second;
		app = new EQApplicationPacket;
		ent->CreateSpawnPacket(app);
		client->QueuePacket(app, true, Client::CLIENT_CONNECTED);
		safe_delete(app);
	}
}

void EntityList::SendZoneCorpsesBulk(Client *client)
{
	NewSpawn_Struct ns;
	Corpse *spawn;
	uint32 maxspawns = 100;

	BulkZoneSpawnPacket *bzsp = new BulkZoneSpawnPacket(client, maxspawns);

	for (auto it = corpse_list.begin(); it != corpse_list.end(); ++it) {
		spawn = it->second;
		if (spawn && spawn->InZone()) {
			memset(&ns, 0, sizeof(NewSpawn_Struct));
			spawn->FillSpawnStruct(&ns, client);
			bzsp->AddSpawn(&ns);
		}
	}
	safe_delete(bzsp);
}

void EntityList::SendZoneObjects(Client *client)
{
	auto it = object_list.begin();
	while (it != object_list.end()) {
		EQApplicationPacket *app = new EQApplicationPacket;
		it->second->CreateSpawnPacket(app);
		client->FastQueuePacket(&app);
		++it;
	}
}

void EntityList::Save()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		it->second->Save();
		++it;
	}
}

void EntityList::ReplaceWithTarget(Mob *pOldMob, Mob *pNewTarget)
{
	if (!pNewTarget)
		return;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->IsAIControlled()) {
			// replace the old mob with the new one
			if (it->second->RemoveFromHateList(pOldMob))
					it->second->AddToHateList(pNewTarget, 1, 0);
		}
		++it;
	}
}

void EntityList::RemoveFromTargets(Mob *mob)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *m = it->second;
		++it;

		if (!m)
			continue;

		m->RemoveFromHateList(mob);
	}
}


void EntityList::QueueClientsByTarget(Mob *sender, const EQApplicationPacket *app,
		bool iSendToSender, Mob *SkipThisMob, bool ackreq, bool HoTT, uint32 ClientVersionBits)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		++it;

		Mob *Target = c->GetTarget();

		if (!Target)
			continue;

		Mob *TargetsTarget = nullptr;

		if (Target)
			TargetsTarget = Target->GetTarget();

		bool Send = false;

		if (c == SkipThisMob)
			continue;

		if (iSendToSender)
			if (c == sender)
				Send = true;

		if (c != sender) {
			if (Target == sender)
				Send = true;
			else if (HoTT)
				if (TargetsTarget == sender)
					Send = true;
		}

		if (Send && (c->GetClientVersionBit() & ClientVersionBits))
			c->QueuePacket(app, ackreq);
	}
}

void EntityList::QueueCloseClients(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, float dist, Mob *SkipThisMob, bool ackreq, eqFilterType filter)
{
	if (sender == nullptr) {
		QueueClients(sender, app, ignore_sender);
		return;
	}

	if (dist <= 0)
		dist = 600;
	float dist2 = dist * dist; //pow(dist, 2);

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if ((!ignore_sender || ent != sender) && (ent != SkipThisMob)) {
			eqFilterMode filter2 = ent->GetFilter(filter);
			if(ent->Connected() &&
				(filter == FilterNone
				|| filter2 == FilterShow
				|| (filter2 == FilterShowGroupOnly && (sender == ent ||
					(ent->GetGroup() && ent->GetGroup()->IsGroupMember(sender))))
				|| (filter2 == FilterShowSelfOnly && ent == sender))
			&& (DistanceSquared(ent->GetPosition(), sender->GetPosition()) <= dist2)) {
				ent->QueuePacket(app, ackreq, Client::CLIENT_CONNECTED);
			}
		}
		++it;
	}
}

//sender can be null
void EntityList::QueueClients(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, bool ackreq)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if ((!ignore_sender || ent != sender))
			ent->QueuePacket(app, ackreq, Client::CLIENT_CONNECTED);

		++it;
	}
}

void EntityList::QueueManaged(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, bool ackreq)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;

		if ((!ignore_sender || ent != sender))
			ent->QueuePacket(app, ackreq, Client::CLIENT_CONNECTED);

		++it;
	}
}


void EntityList::QueueClientsStatus(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, uint8 minstatus, uint8 maxstatus)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if ((!ignore_sender || it->second != sender) &&
				(it->second->Admin() >= minstatus && it->second->Admin() <= maxstatus))
			it->second->QueuePacket(app);

		++it;
	}
}

void EntityList::DuelMessage(Mob *winner, Mob *loser, bool flee)
{
	if (winner->GetLevelCon(winner->GetLevel(), loser->GetLevel()) > 2) {
		std::vector<EQEmu::Any> args;
		args.push_back(winner);
		args.push_back(loser);

		parse->EventPlayer(EVENT_DUEL_WIN, winner->CastToClient(), loser->GetName(), loser->CastToClient()->CharacterID(), &args);
		parse->EventPlayer(EVENT_DUEL_LOSE, loser->CastToClient(), winner->GetName(), winner->CastToClient()->CharacterID(), &args);
	}

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *cur = it->second;
		//might want some sort of distance check in here?
		if (cur != winner && cur != loser) {
			if (flee)
				cur->Message_StringID(CC_Yellow, DUEL_FLED, winner->GetName(),loser->GetName(),loser->GetName());
			else
				cur->Message_StringID(CC_Yellow, DUEL_FINISHED, winner->GetName(),loser->GetName());
		}
		++it;
	}
}

Client *EntityList::GetClientByName(const char *checkname)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (strcasecmp(it->second->GetName(), checkname) == 0)
			return it->second;
		++it;
	}
	return nullptr;
}

Client *EntityList::GetClientByCharID(uint32 iCharID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->CharacterID() == iCharID)
			return it->second;
		++it;
	}
	return nullptr;
}

Client *EntityList::GetClientByWID(uint32 iWID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->GetWID() == iWID) {
			return it->second;
		}
		++it;
	}
	return nullptr;
}

Client *EntityList::GetRandomClient(const glm::vec3& location, float Distance, Client *ExcludeClient)
{
	std::vector<Client *> ClientsInRange;


	for (auto it = client_list.begin();it != client_list.end(); ++it)
		if ((it->second != ExcludeClient) && (DistanceSquared(static_cast<glm::vec3>(it->second->GetPosition()), location) <= Distance))
			ClientsInRange.push_back(it->second);

	if (ClientsInRange.empty())
		return nullptr;

	return ClientsInRange[zone->random.Int(0, ClientsInRange.size() - 1)];
}

Corpse *EntityList::GetCorpseByOwner(Client *client)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->IsPlayerCorpse())
			if (strcasecmp(it->second->GetOwnerName(), client->GetName()) == 0)
				return it->second;
		++it;
	}
	return nullptr;
}

Corpse *EntityList::GetCorpseByOwnerWithinRange(Client *client, Mob *center, int range)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->IsPlayerCorpse())
			if (DistanceSquaredNoZ(center->GetPosition(), it->second->GetPosition()) < range &&
					strcasecmp(it->second->GetOwnerName(), client->GetName()) == 0)
				return it->second;
		++it;
	}
	return nullptr;
}

Corpse *EntityList::GetCorpseByDBID(uint32 dbid)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCorpseDBID() == dbid)
			return it->second;
		++it;
	}
	return nullptr;
}

Corpse *EntityList::GetCorpseByName(const char *name)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (strcmp(it->second->GetName(), name) == 0)
			return it->second;
		++it;
	}
	return nullptr;
}

Spawn2 *EntityList::GetSpawnByID(uint32 id)
{
	if (!zone || !zone->IsLoaded())
		return nullptr;

	LinkedListIterator<Spawn2 *> iterator(zone->spawn2_list);
	iterator.Reset();
	while(iterator.MoreElements())
	{
		if(iterator.GetData()->GetID() == id) {
			return iterator.GetData();
		}
		iterator.Advance();
	}

	return nullptr;
}

void EntityList::RemoveAllCorpsesByCharID(uint32 charid)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCharID() == charid) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		} else {
			++it;
		}
	}
}

void EntityList::RemoveCorpseByDBID(uint32 dbid)
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCorpseDBID() == dbid) {
			safe_delete(it->second);
			free_ids.push(it->first);
			it = corpse_list.erase(it);
		} else {
			++it;
		}
	}
}

int EntityList::RezzAllCorpsesByCharID(uint32 charid)
{
	int RezzExp = 0;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->GetCharID() == charid) {
			RezzExp += it->second->GetRezExp();
			it->second->IsRezzed(true);
			it->second->CompleteResurrection();
		}
		++it;
	}
	return RezzExp;
}

Group *EntityList::GetGroupByMob(Mob *mob)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if ((*iterator)->IsGroupMember(mob))
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Group *EntityList::GetGroupByLeaderName(const char *leader)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if (!strcmp((*iterator)->GetLeaderName(), leader))
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Group *EntityList::GetGroupByID(uint32 group_id)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if ((*iterator)->GetID() == group_id)
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Group *EntityList::GetGroupByClient(Client *client)
{
	std::list <Group *>::iterator iterator;

	iterator = group_list.begin();

	while (iterator != group_list.end()) {
		if ((*iterator)->IsGroupMember(client->CastToMob()))
			return *iterator;
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return nullptr;
}

Raid *EntityList::GetRaidByLeaderName(const char *leader)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		if ((*iterator)->GetLeader())
			if(strcmp((*iterator)->GetLeader()->GetName(), leader) == 0)
				return *iterator;
		++iterator;
	}
	return nullptr;
}

Raid *EntityList::GetRaidByID(uint32 id)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		if ((*iterator)->GetID() == id)
			return *iterator;
		++iterator;
	}
	return nullptr;
}

Raid *EntityList::GetRaidByClient(Client* client)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		for (int x = 0; x < MAX_RAID_MEMBERS; x++)
			if ((*iterator)->members[x].member)
				if((*iterator)->members[x].member == client)
					return *iterator;
		++iterator;
	}
	return nullptr;
}

Raid *EntityList::GetRaidByMob(Mob *mob)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while (iterator != raid_list.end()) {
		for(int x = 0; x < MAX_RAID_MEMBERS; x++) {
			// TODO: Implement support for Mob objects in Raid class
			/*if((*iterator)->members[x].member){
				if((*iterator)->members[x].member == mob)
					return *iterator;
			}*/
		}
		++iterator;
	}
	return nullptr;
}

Client *EntityList::GetClientByAccID(uint32 accid)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->AccountID() == accid)
			return it->second;
		++it;
	}
	return nullptr;
}

void EntityList::ChannelMessageFromWorld(const char *from, const char *to,
		uint8 chan_num, uint32 guild_id, uint8 language, const char *message)
{
	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		Client *client = it->second;
		if (chan_num == 0) {
			if (!client->IsInGuild(guild_id))
				continue;
			if (!guild_mgr.CheckPermission(guild_id, client->GuildRank(), GUILD_HEAR))
				continue;
			if (client->GetFilter(FilterGuildChat) == FilterHide)
				continue;
		} else if (chan_num == 5) {
			if (client->GetFilter(FilterOOC) == FilterHide)
				continue;
		}
		client->ChannelMessageSend(from, to, chan_num, language, message);
	}
}

void EntityList::Message(uint32 to_guilddbid, uint32 type, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4096, message, argptr);
	va_end(argptr);

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (to_guilddbid == 0 || client->IsInGuild(to_guilddbid))
			client->Message(type, buffer);
		++it;
	}
}

void EntityList::QueueClientsGuild(Mob *sender, const EQApplicationPacket *app,
		bool ignore_sender, uint32 guild_id)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if (client->IsInGuild(guild_id))
			client->QueuePacket(app);
		++it;
	}
}

void EntityList::MessageStatus(uint32 to_guild_id, int to_minstatus, uint32 type, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4096, message, argptr);
	va_end(argptr);

	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *client = it->second;
		if ((to_guild_id == 0 || client->IsInGuild(to_guild_id)) && client->Admin() >= to_minstatus)
			client->Message(type, buffer);
		++it;
	}
}

// works much like MessageClose, but with formatted strings
void EntityList::MessageClose_StringID(Mob *sender, bool skipsender, float dist, uint32 type, uint32 string_id, const char* message1,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9)
{
	Client *c;
	float dist2 = dist * dist;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if(c && DistanceSquared(c->GetPosition(), sender->GetPosition()) <= dist2 && (!skipsender || c != sender))
			c->Message_StringID(type, string_id, message1, message2, message3, message4, message5, message6, message7, message8, message9);
	}
}

void EntityList::FilteredMessageClose_StringID(Mob *sender, bool skipsender,
		float dist, uint32 type, eqFilterType filter, uint32 string_id,
		const char *message1, const char *message2, const char *message3,
		const char *message4, const char *message5, const char *message6,
		const char *message7, const char *message8, const char *message9)
{
	Client *c;
	float dist2 = dist * dist;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if (c && DistanceSquared(c->GetPosition(), sender->GetPosition()) <= dist2 && (!skipsender || c != sender))
			c->FilteredMessage_StringID(sender, type, filter, string_id,
					message1, message2, message3, message4, message5,
					message6, message7, message8, message9);
	}
}

void EntityList::Message_StringID(Mob *sender, bool skipsender, uint32 type, uint32 string_id, const char* message1,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9)
{
	Client *c;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if(c && (!skipsender || c != sender))
			c->Message_StringID(type, string_id, message1, message2, message3, message4, message5, message6, message7, message8, message9);
	}
}

void EntityList::FilteredMessage_StringID(Mob *sender, bool skipsender,
		uint32 type, eqFilterType filter, uint32 string_id,
		const char *message1, const char *message2, const char *message3,
		const char *message4, const char *message5, const char *message6,
		const char *message7, const char *message8, const char *message9)
{
	Client *c;

	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if (c && (!skipsender || c != sender))
			c->FilteredMessage_StringID(sender, type, filter, string_id,
					message1, message2, message3, message4, message5, message6,
					message7, message8, message9);
	}
}

void EntityList::MessageClose(Mob* sender, bool skipsender, float dist, uint32 type, const char* message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4095, message, argptr);
	va_end(argptr);

	float dist2 = dist * dist;

	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (DistanceSquared(it->second->GetPosition(), sender->GetPosition()) <= dist2 && (!skipsender || it->second != sender))
			it->second->Message(type, buffer);
		++it;
	}
}

void EntityList::RemoveAllMobs()
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = mob_list.erase(it);
	}
}

void EntityList::RemoveAllClients()
{
	// doesn't clear the data
	client_list.clear();
}

void EntityList::RemoveAllNPCs()
{
	// doesn't clear the data
	npc_list.clear();
	npc_limit_list.clear();
}

void EntityList::RemoveAllGroups()
{
	while (!group_list.empty()) {
		safe_delete(group_list.front());
		group_list.pop_front();
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
}

void EntityList::RemoveAllRaids()
{
	while (!raid_list.empty()) {
		safe_delete(raid_list.front());
		raid_list.pop_front();
	}
}

void EntityList::RemoveAllDoors()
{
	auto it = door_list.begin();
	while (it != door_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = door_list.erase(it);
	}
	DespawnAllDoors();
}

void EntityList::DespawnAllDoors()
{
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_RemoveAllDoors, 0);
	this->QueueClients(0,outapp);
	safe_delete(outapp);
}

void EntityList::RespawnAllDoors()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second) {
			EQApplicationPacket *outapp = new EQApplicationPacket();
			MakeDoorSpawnPacket(outapp, it->second);
			it->second->FastQueuePacket(&outapp);
		}
		++it;
	}
}

void EntityList::RemoveAllCorpses()
{
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = corpse_list.erase(it);
	}
}

void EntityList::RemoveAllObjects()
{
	auto it = object_list.begin();
	while (it != object_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = object_list.erase(it);
	}
}

void EntityList::RemoveAllTraps(){
	auto it = trap_list.begin();
	while (it != trap_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		it = trap_list.erase(it);
	}
}

bool EntityList::RemoveMob(uint16 delete_id)
{
	if (delete_id == 0)
		return true;

	auto it = mob_list.find(delete_id);
	if (it != mob_list.end()) {
		if (npc_list.count(delete_id))
			entity_list.RemoveNPC(delete_id);
		else if (client_list.count(delete_id))
			entity_list.RemoveClient(delete_id);
		safe_delete(it->second);
		if (!corpse_list.count(delete_id))
			free_ids.push(it->first);
		mob_list.erase(it);
		return true;
	}
	return false;
}

// This is for if the ID is deleted for some reason
bool EntityList::RemoveMob(Mob *delete_mob)
{
	if (delete_mob == 0)
		return true;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second == delete_mob) {
			safe_delete(it->second);
			if (!corpse_list.count(it->first))
				free_ids.push(it->first);
			mob_list.erase(it);
			return true;
		}
		++it;
	}
	return false;
}

bool EntityList::RemoveNPC(uint16 delete_id)
{
	auto it = npc_list.find(delete_id);
	if (it != npc_list.end()) {
		// make sure its proximity is removed
		RemoveProximity(delete_id);
		// remove from the list
		npc_list.erase(it);
		// remove from limit list if needed
		if (npc_limit_list.count(delete_id))
			npc_limit_list.erase(delete_id);
		return true;
	}
	return false;
}

bool EntityList::RemoveClient(uint16 delete_id)
{
	auto it = client_list.find(delete_id);
	if (it != client_list.end()) {
		client_list.erase(it); // Already deleted
		return true;
	}
	return false;
}

// If our ID was deleted already
bool EntityList::RemoveClient(Client *delete_client)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second == delete_client) {
			client_list.erase(it);
			return true;
		}
		++it;
	}
	return false;
}

bool EntityList::RemoveObject(uint16 delete_id)
{
	auto it = object_list.find(delete_id);
	if (it != object_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		object_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveTrap(uint16 delete_id)
{
	auto it = trap_list.find(delete_id);
	if (it != trap_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		trap_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveDoor(uint16 delete_id)
{
	auto it = door_list.find(delete_id);
	if (it != door_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		door_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveCorpse(uint16 delete_id)
{
	auto it = corpse_list.find(delete_id);
	if (it != corpse_list.end()) {
		safe_delete(it->second);
		free_ids.push(it->first);
		corpse_list.erase(it);
		return true;
	}
	return false;
}

bool EntityList::RemoveGroup(uint32 delete_id)
{
	std::list<Group *>::iterator iterator;

	iterator = group_list.begin();

	while(iterator != group_list.end())
	{
		if((*iterator)->GetID() == delete_id) {
			safe_delete(*iterator);
			group_list.remove(*iterator);
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
			return true;
		}
		++iterator;
	}
#if EQDEBUG >= 5
	CheckGroupList (__FILE__, __LINE__);
#endif
	return false;
}

bool EntityList::RemoveRaid(uint32 delete_id)
{
	std::list<Raid *>::iterator iterator;

	iterator = raid_list.begin();

	while(iterator != raid_list.end())
	{
		if((*iterator)->GetID() == delete_id) {
			safe_delete(*iterator);
			raid_list.remove(*iterator);
			return true;
		}
		++iterator;
	}
	return false;
}

void EntityList::Clear()
{
	RemoveAllClients();
	entity_list.RemoveAllTraps(); //we can have child npcs so we go first
	entity_list.RemoveAllNPCs();
	entity_list.RemoveAllMobs();
	entity_list.RemoveAllCorpses();
	entity_list.RemoveAllGroups();
	entity_list.RemoveAllDoors();
	entity_list.RemoveAllObjects();
	entity_list.RemoveAllRaids();
	entity_list.RemoveAllLocalities();
}

void EntityList::UpdateWho(bool iSendFullUpdate)
{
	if ((!worldserver.Connected()) || !ZoneLoaded)
		return;
	uint32 tmpNumUpdates = numclients + 5;
	ServerPacket* pack = 0;
	ServerClientListKeepAlive_Struct* sclka = 0;
	if (!iSendFullUpdate) {
		pack = new ServerPacket(ServerOP_ClientListKA, sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4));
		sclka = (ServerClientListKeepAlive_Struct*) pack->pBuffer;
	}

	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->InZone()) {
			if (iSendFullUpdate) {
				it->second->UpdateWho();
			} else {
				if (sclka->numupdates >= tmpNumUpdates) {
					tmpNumUpdates += 10;
					uint8* tmp = pack->pBuffer;
					pack->pBuffer = new uint8[sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4)];
					memset(pack->pBuffer, 0, sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4));
					memcpy(pack->pBuffer, tmp, pack->size);
					pack->size = sizeof(ServerClientListKeepAlive_Struct) + (tmpNumUpdates * 4);
					safe_delete_array(tmp);
				}
				sclka->wid[sclka->numupdates] = it->second->GetWID();
				sclka->numupdates++;
			}
		}
		++it;
	}
	if (!iSendFullUpdate) {
		pack->size = sizeof(ServerClientListKeepAlive_Struct) + (sclka->numupdates * 4);
		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

void EntityList::RemoveEntity(uint16 id)
{
	if (id == 0)
		return;
	if (entity_list.RemoveMob(id))
		return;
	else if (entity_list.RemoveCorpse(id))
		return;
	else if (entity_list.RemoveDoor(id))
		return;
	else if (entity_list.RemoveGroup(id))
		return;
	else if (entity_list.RemoveTrap(id))
		return;
	else
		entity_list.RemoveObject(id);
}

void EntityList::Process()
{
	CheckSpawnQueue();
}

void EntityList::CountNPC(uint32 *NPCCount, uint32 *NPCLootCount, uint32 *gmspawntype_count)
{
	*NPCCount = 0;
	*NPCLootCount = 0;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		(*NPCCount)++;
		(*NPCLootCount) += it->second->CountLoot();
		if (it->second->GetNPCTypeID() == 0)
			(*gmspawntype_count)++;
		++it;
	}
}

void EntityList::Depop(bool StartSpawnTimer)
{
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *pnpc = it->second;
		if (pnpc) {
			Mob *own = pnpc->GetOwner();
			//do not depop player's pets...
			if (own && own->IsClient())
				continue;

			/* Web Interface Depop Entities */
		/*	std::vector<std::string> params;
			params.push_back(std::to_string((long)pnpc->GetID()));
			RemoteCallSubscriptionHandler::Instance()->OnEvent("NPC.Depop", params);*/

			pnpc->Depop(StartSpawnTimer);
		}
	}
}

void EntityList::DepopAll(int NPCTypeID, bool StartSpawnTimer)
{
	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *pnpc = it->second;
		if (pnpc && (pnpc->GetNPCTypeID() == (uint32)NPCTypeID)){
			pnpc->Depop(StartSpawnTimer); 

			/* Web Interface Depop Entities */
			/*std::vector<std::string> params;
			params.push_back(std::to_string((long)pnpc->GetID()));
			RemoteCallSubscriptionHandler::Instance()->OnEvent("NPC.Depop", params);*/
		}
	}
}

void EntityList::SendTraders(Client *client)
{
	Client *trader = nullptr;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		trader = it->second;
		if (trader->IsTrader())
			client->SendTraderPacket(trader);

		++it;
	}
}

void EntityList::RemoveFromHateLists(Mob *mob, bool settoone)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(mob)) {
			if (!settoone)
				it->second->RemoveFromHateList(mob);
			else
				it->second->SetHate(mob, 1);
		}
		++it;
	}
}

void EntityList::RemoveDebuffs(Mob *caster)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		it->second->BuffFadeDetrimentalByCaster(caster);
		++it;
	}
}

// Currently, a new packet is sent per entity.
// @todo: Come back and use FLAG_COMBINED to pack
// all updates into one packet.
void EntityList::SendPositionUpdates(Client *client, uint32 cLastUpdate,
		float range, Entity *alwayssend, bool iSendEvenIfNotChanged)
{
	range = range * range;

	EQApplicationPacket *outapp = 0;
	SpawnPositionUpdate_Struct *ppu = 0;
	Mob *mob = 0;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (outapp == 0) {
			outapp = new EQApplicationPacket(OP_ClientUpdate, sizeof(SpawnPositionUpdate_Struct));
			ppu = (SpawnPositionUpdate_Struct*)outapp->pBuffer;
		}
		mob = it->second;
		if (mob && !mob->IsCorpse() && (it->second != client)
			&& (mob->IsClient() || iSendEvenIfNotChanged || (mob->LastChange() >= cLastUpdate))
			&& (!it->second->IsClient() || !it->second->CastToClient()->GMHideMe(client))) {

			//bool Grouped = client->HasGroup() && mob->IsClient() && (client->GetGroup() == mob->CastToClient()->GetGroup());

			//if (range == 0 || (iterator.GetData() == alwayssend) || Grouped || (mob->DistNoRootNoZ(*client) <= range)) {
			if (range == 0 || (it->second == alwayssend) || mob->IsClient() || (DistanceSquared(mob->GetPosition(), client->GetPosition()) <= range)) {
				mob->MakeSpawnUpdate(ppu);
			}
			if(mob && mob->IsClient() && mob->GetID()>0) {
				client->QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
			}
		}
		safe_delete(outapp);
		outapp = 0;
		++it;
	}

	safe_delete(outapp);
}

char *EntityList::MakeNameUnique(char *name)
{
	bool used[300];
	memset(used, 0, sizeof(used));
	name[61] = 0; name[62] = 0; name[63] = 0;


	int len = strlen(name);
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->IsMob()) {
			if (strncasecmp(it->second->CastToMob()->GetName(), name, len) == 0) {
				if (Seperator::IsNumber(&it->second->CastToMob()->GetName()[len])) {
					used[atoi(&it->second->CastToMob()->GetName()[len])] = true;
				}
			}
		}
		++it;
	}
	for (int i=0; i < 300; i++) {
		if (!used[i]) {
			#ifdef _WINDOWS
			snprintf(name, 64, "%s%03d", name, i);
			#else
			//glibc clears destination of snprintf
			//make a copy of name before snprintf--misanthropicfiend
			char temp_name[64];
			strn0cpy(temp_name, name, 64);
			snprintf(name, 64, "%s%03d", temp_name, i);
			#endif
			return name;
		}
	}
	Log.DebugCategory(EQEmuLogSys::General, EQEmuLogSys::Error, "Fatal error in EntityList::MakeNameUnique: Unable to find unique name for '%s'", name);
	char tmp[64] = "!";
	strn0cpy(&tmp[1], name, sizeof(tmp) - 1);
	strcpy(name, tmp);
	return MakeNameUnique(name);
}

char *EntityList::RemoveNumbers(char *name)
{
	char tmp[64];
	memset(tmp, 0, sizeof(tmp));
	int k = 0;
	for (unsigned int i=0; i<strlen(name) && i<sizeof(tmp); i++) {
		if (name[i] < '0' || name[i] > '9')
			tmp[k++] = name[i];
	}
	strn0cpy(name, tmp, sizeof(tmp));
	return name;
}

void EntityList::ListNPCs(Client* client, const char *arg1, const char *arg2, uint8 searchtype)
{
	if (arg1 == 0)
		searchtype = 0;
	else if (arg2 == 0 && searchtype >= 2)
		searchtype = 0;
	uint32 x = 0;
	uint32 z = 0;
	char sName[36];

	auto it = npc_list.begin();
	client->Message(0, "NPCs in the zone:");
	if (searchtype == 0) {
		while (it != npc_list.end()) {
			NPC *n = it->second;

			client->Message(0, "  %5d: %s (%.0f, %0.f, %.0f)", n->GetID(), n->GetName(), n->GetX(), n->GetY(), n->GetZ());
			x++;
			z++;
			++it;
		}
	} else if (searchtype == 1) {
		client->Message(0, "Searching by name method. (%s)",arg1);
		char* tmp = new char[strlen(arg1) + 1];
		strcpy(tmp, arg1);
		strupr(tmp);
		while (it != npc_list.end()) {
			z++;
			strcpy(sName, it->second->GetName());
			strupr(sName);
			if (strstr(sName, tmp)) {
				NPC *n = it->second;
				client->Message(0, "  %5d: %s (%.0f, %.0f, %.0f)", n->GetID(), n->GetName(), n->GetX(), n->GetY(), n->GetZ());
				x++;
			}
			++it;
		}
		safe_delete_array(tmp);
	} else if (searchtype == 2) {
		client->Message(0, "Searching by number method. (%s %s)",arg1,arg2);
		while (it != npc_list.end()) {
			z++;
			if ((it->second->GetID() >= atoi(arg1)) && (it->second->GetID() <= atoi(arg2)) && (atoi(arg1) <= atoi(arg2))) {
				client->Message(0, "  %5d: %s", it->second->GetID(), it->second->GetName());
				x++;
			}
			++it;
		}
	}
	client->Message(0, "%d npcs listed. There is a total of %d npcs in this zone.", x, z);
}

void EntityList::ListNPCCorpses(Client *client)
{
	uint32 x = 0;

	auto it = corpse_list.begin();
	client->Message(0, "NPC Corpses in the zone:");
	while (it != corpse_list.end()) {
		if (it->second->IsNPCCorpse()) {
			client->Message(0, "  %5d: %s", it->first, it->second->GetName());
			x++;
		}
		++it;
	}
	client->Message(0, "%d npc corpses listed.", x);
}

void EntityList::ListPlayerCorpses(Client *client)
{
	uint32 x = 0;

	auto it = corpse_list.begin();
	client->Message(0, "Player Corpses in the zone:");
	while (it != corpse_list.end()) {
		if (it->second->IsPlayerCorpse()) {
			client->Message(0, "  %5d: %s", it->first, it->second->GetName());
			x++;
		}
		++it;
	}
	client->Message(0, "%d player corpses listed.", x);
}

void EntityList::FindPathsToAllNPCs()
{
	if (!zone->pathing)
		return;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		glm::vec3 Node0 = zone->pathing->GetPathNodeCoordinates(0, false);
		glm::vec3 Dest(it->second->GetX(), it->second->GetY(), it->second->GetZ());
		std::deque<int> Route = zone->pathing->FindRoute(Node0, Dest);
		if (Route.size() == 0)
			printf("Unable to find a route to %s\n", it->second->GetName());
		else
			printf("Found a route to %s\n", it->second->GetName());
		++it;
	}

	fflush(stdout);
}

// returns the number of corpses deleted. A negative number indicates an error code.
int32 EntityList::DeleteNPCCorpses()
{
	int32 x = 0;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->IsNPCCorpse()) {
			it->second->DepopNPCCorpse();
			x++;
		}
		++it;
	}
	return x;
}

// returns the number of corpses deleted. A negative number indicates an error code.
int32 EntityList::DeletePlayerCorpses()
{
	int32 x = 0;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		if (it->second->IsPlayerCorpse()) {
			it->second->CastToCorpse()->Delete();
			x++;
		}
		++it;
	}
	return x;
}

void EntityList::SendPetitionToAdmins()
{
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_PetitionRefresh,sizeof(PetitionUpdate_Struct));
	PetitionUpdate_Struct *pcus = (PetitionUpdate_Struct*) outapp->pBuffer;
	pcus->petnumber = 0;		// Petition Number
	pcus->color = 0;
	pcus->status = 0xFFFFFFFF;
	pcus->senttime = 0;
	strcpy(pcus->accountid, "");
	strcpy(pcus->gmsenttoo, "");
	pcus->quetotal=0;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->CastToClient()->Admin() >= 80)
			it->second->CastToClient()->QueuePacket(outapp);
		++it;
	}
	safe_delete(outapp);
}

void EntityList::SendPetitionToAdmins(Petition *pet)
{
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_PetitionRefresh,sizeof(PetitionUpdate_Struct));
	PetitionUpdate_Struct *pcus = (PetitionUpdate_Struct*) outapp->pBuffer;
	pcus->petnumber = pet->GetID();		// Petition Number
	if (pet->CheckedOut()) {
		pcus->color = 0x00;
		pcus->status = 0xFFFFFFFF;
		pcus->senttime = pet->GetSentTime();
		strcpy(pcus->accountid, "");
		strcpy(pcus->gmsenttoo, "");
	} else {
		pcus->color = pet->GetUrgency();	// 0x00 = green, 0x01 = yellow, 0x02 = red
		pcus->status = pet->GetSentTime();
		pcus->senttime = pet->GetSentTime();			// 4 has to be 0x1F
		strcpy(pcus->accountid, pet->GetAccountName());
		strcpy(pcus->charname, pet->GetCharName());
	}
	pcus->quetotal = petition_list.GetTotalPetitions();
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->CastToClient()->Admin() >= 80) {
			if (pet->CheckedOut())
				strcpy(pcus->gmsenttoo, "");
			else
				strcpy(pcus->gmsenttoo, it->second->CastToClient()->GetName());
			it->second->CastToClient()->QueuePacket(outapp);
		}
		++it;
	}
	safe_delete(outapp);
}

void EntityList::WriteEntityIDs()
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		std::cout << "ID: " << it->first << "  Name: " << it->second->GetName() << std::endl;
		++it;
	}
}

BulkZoneSpawnPacket::BulkZoneSpawnPacket(Client *iSendTo, uint32 iMaxSpawnsPerPacket)
{
	data = 0;
	pSendTo = iSendTo;
	pMaxSpawnsPerPacket = iMaxSpawnsPerPacket;
}

BulkZoneSpawnPacket::~BulkZoneSpawnPacket()
{
	SendBuffer();
	safe_delete_array(data)
}

bool BulkZoneSpawnPacket::AddSpawn(NewSpawn_Struct *ns)
{
	if (!data) {
		data = new NewSpawn_Struct[pMaxSpawnsPerPacket];
		memset(data, 0, sizeof(NewSpawn_Struct) * pMaxSpawnsPerPacket);
		index = 0;
	}
	memcpy(&data[index], ns, sizeof(NewSpawn_Struct));
	index++;
	if (index >= pMaxSpawnsPerPacket) {
		SendBuffer();
		return true;
	}
	return false;
}

void BulkZoneSpawnPacket::SendBuffer() {
	if (!data)
		return;

	uint32 tmpBufSize = (index * sizeof(NewSpawn_Struct));
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_ZoneSpawns, (unsigned char *)data, tmpBufSize);

	if (pSendTo) {
		pSendTo->FastQueuePacket(&outapp);
	} else {
		entity_list.QueueClients(0, outapp);
		safe_delete(outapp);
	}
	memset(data, 0, sizeof(NewSpawn_Struct) * pMaxSpawnsPerPacket);
	index = 0;
}

void EntityList::DoubleAggro(Mob *who)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(who))
			it->second->SetHate(who, it->second->CastToNPC()->GetHateAmount(who),
					it->second->CastToNPC()->GetHateAmount(who) * 2);
		++it;
	}
}

void EntityList::HalveAggro(Mob *who)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CastToNPC()->CheckAggro(who))
			it->second->CastToNPC()->SetHate(who, it->second->CastToNPC()->GetHateAmount(who) / 2);
		++it;
	}
}

void EntityList::ReduceAggro(Mob *who)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CastToNPC()->CheckAggro(who))
			it->second->CastToNPC()->SetHate(who, 1);
		++it;
	}
}

void EntityList::Evade(Mob *who)
{
	uint32 flatval = who->GetLevel() * 13;
	int amt = 0;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CastToNPC()->CheckAggro(who)) {
			amt = it->second->CastToNPC()->GetHateAmount(who);
			amt -= flatval;
			if (amt > 0)
				it->second->CastToNPC()->SetHate(who, amt);
			else
				it->second->CastToNPC()->SetHate(who, 0);
		}
		++it;
	}
}

//removes "targ" from all hate lists, including feigned, in the zone
void EntityList::ClearAggro(Mob* targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(targ))
			it->second->RemoveFromHateList(targ);
		it->second->RemoveFromFeignMemory(targ->CastToClient()); //just in case we feigned
		++it;
	}
}

void EntityList::ClearFeignAggro(Mob *targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(targ)) {
			if (it->second->GetSpecialAbility(IMMUNE_FEIGN_DEATH)) {
				++it;
				continue;
			}

			if (targ->IsClient()) {
				std::vector<EQEmu::Any> args;
				args.push_back(it->second);
				int i = parse->EventPlayer(EVENT_FEIGN_DEATH, targ->CastToClient(), "", 0, &args);
				if (i != 0) {
					++it;
					continue;
				}

				if (it->second->IsNPC()) {
					int i = parse->EventNPC(EVENT_FEIGN_DEATH, it->second->CastToNPC(), targ, "", 0);
					if (i != 0) {
						++it;
						continue;
					}
				}
			}

			it->second->RemoveFromHateList(targ);
			if (targ->IsClient()) {
				if (it->second->GetLevel() >= 35 && zone->random.Roll(60))
					it->second->AddFeignMemory(targ->CastToClient());
			}
		}
		++it;
	}
}

void EntityList::ClearZoneFeignAggro(Client *targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		it->second->RemoveFromFeignMemory(targ);
		++it;
	}
}

void EntityList::AggroZone(Mob *who, int hate)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		it->second->AddToHateList(who, hate);
		++it;
	}
}

// Signal Quest command function
void EntityList::SignalMobsByNPCID(uint32 snpc, int signal_id)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *pit = it->second;
		if (pit->GetNPCTypeID() == snpc)
			pit->SignalNPC(signal_id);
		++it;
	}
}

void EntityList::MessageGroup(Mob *sender, bool skipclose, uint32 type, const char *message, ...)
{
	va_list argptr;
	char buffer[4096];

	va_start(argptr, message);
	vsnprintf(buffer, 4095, message, argptr);
	va_end(argptr);

	float dist2 = 100;

	if (skipclose)
		dist2 = 0;

	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second != sender &&
				(Distance(it->second->GetPosition(), sender->GetPosition()) <= dist2 || it->second->GetGroup() == sender->CastToClient()->GetGroup())) {
			it->second->Message(type, buffer);
		}
		++it;
	}
}

bool EntityList::Fighting(Mob *targ)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->CheckAggro(targ))
			return true;
		++it;
	}
	return false;
}

void EntityList::AddHealAggro(Mob *target, Mob *caster, uint16 thedam)
{
	NPC *cur = nullptr;
	uint16 count = 0;
	std::list<NPC *> npc_sub_list;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		cur = it->second;

		if (!cur->CheckAggro(target)) {
			++it;
			continue;
		}
		if (!cur->IsMezzed() && !cur->IsStunned() && !cur->IsFeared()) {
			npc_sub_list.push_back(cur);
			++count;
		}
		++it;
	}


	if (thedam > 1) {
		if (count > 0)
			thedam /= count;

		if (thedam < 1)
			thedam = 1;
	}

	cur = nullptr;
	auto sit = npc_sub_list.begin();
	while (sit != npc_sub_list.end()) {
		cur = *sit;

		if (cur->IsPet()) {
			if (caster) {
				if (cur->CheckAggro(caster)) {
					cur->AddToHateList(caster, thedam);
				}
			}
		} else {
			if (caster) {
				if (cur->CheckAggro(caster)) {
					cur->AddToHateList(caster, thedam);
				} else {
					cur->AddToHateList(caster, thedam * 0.33);
				}
			}
		}
		++sit;
	}
}

void EntityList::OpenDoorsNear(NPC *who)
{

	for (auto it = door_list.begin();it != door_list.end(); ++it) {
		Doors *cdoor = it->second;
		if (!cdoor || cdoor->IsDoorOpen())
			continue;

		auto diff = who->GetPosition() - cdoor->GetPosition();

		float curdist = diff.x * diff.x + diff.y * diff.y;

		if (diff.z * diff.z < 10 && curdist <= 100)
			cdoor->NPCOpen(who);
	}
}

void EntityList::SendAlarm(Trap *trap, Mob *currenttarget, uint8 kos)
{
	float preSquareDistance = trap->effectvalue * trap->effectvalue;

	for (auto it = npc_list.begin();it != npc_list.end(); ++it) {
		NPC *cur = it->second;

		auto diff = glm::vec3(cur->GetPosition()) - trap->m_Position;
		float curdist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

		if (cur->GetOwner() || cur->IsEngaged() || curdist > preSquareDistance )
			continue;

		if (kos) {
			uint8 factioncon = currenttarget->GetReverseFactionCon(cur);
			if (factioncon == FACTION_THREATENLY || factioncon == FACTION_SCOWLS) {
				cur->AddToHateList(currenttarget,1);
			}
		}
		else
			cur->AddToHateList(currenttarget,1);
	}
}

void EntityList::AddProximity(NPC *proximity_for)
{
	RemoveProximity(proximity_for->GetID());

	proximity_list.push_back(proximity_for);

	proximity_for->proximity = new NPCProximity; // deleted in NPC::~NPC
}

bool EntityList::RemoveProximity(uint16 delete_npc_id)
{
	auto it = std::find_if(proximity_list.begin(), proximity_list.end(),
			[delete_npc_id](const NPC *a) { return a->GetID() == delete_npc_id; });
	if (it == proximity_list.end())
		return false;

	proximity_list.erase(it);
	return true;
}

void EntityList::RemoveAllLocalities()
{
	proximity_list.clear();
}

struct quest_proximity_event {
	QuestEventID event_id;
	Client *client;
	NPC *npc;
	int area_id;
	int area_type;
};

void EntityList::ProcessMove(Client *c, const glm::vec3& location)
{
	float last_x = c->ProximityX();
	float last_y = c->ProximityY();
	float last_z = c->ProximityZ();

	std::list<quest_proximity_event> events;
	for (auto iter = proximity_list.begin(); iter != proximity_list.end(); ++iter) {
		NPC *d = (*iter);
		NPCProximity *l = d->proximity;
		if (l == nullptr)
			continue;

		//check both bounding boxes, if either coords pairs
		//cross a boundary, send the event.
		bool old_in = true;
		bool new_in = true;
		if (last_x < l->min_x || last_x > l->max_x ||
				last_y < l->min_y || last_y > l->max_y ||
				last_z < l->min_z || last_z > l->max_z) {
			old_in = false;
		}
		if (location.x < l->min_x || location.x > l->max_x ||
				location.y < l->min_y || location.y > l->max_y ||
				location.z < l->min_z || location.z > l->max_z) {
			new_in = false;
		}

		if (old_in && !new_in) {
			quest_proximity_event evt;
			evt.event_id = EVENT_EXIT;
			evt.client = c;
			evt.npc = d;
			evt.area_id = 0;
			evt.area_type = 0;
			events.push_back(evt);
		} else if (new_in && !old_in) {
			quest_proximity_event evt;
			evt.event_id = EVENT_ENTER;
			evt.client = c;
			evt.npc = d;
			evt.area_id = 0;
			evt.area_type = 0;
			events.push_back(evt);
		}
	}

	for (auto iter = area_list.begin(); iter != area_list.end(); ++iter) {
		Area& a = (*iter);
		bool old_in = true;
		bool new_in = true;
		if (last_x < a.min_x || last_x > a.max_x ||
				last_y < a.min_y || last_y > a.max_y ||
				last_z < a.min_z || last_z > a.max_z) {
			old_in = false;
		}

		if (location.x < a.min_x || location.x > a.max_x ||
				location.y < a.min_y || location.y > a.max_y ||
				location.z < a.min_z || location.z > a.max_z ) {
			new_in = false;
		}

		if (old_in && !new_in) {
			//were in but are no longer.
			quest_proximity_event evt;
			evt.event_id = EVENT_LEAVE_AREA;
			evt.client = c;
			evt.npc = nullptr;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		} else if (!old_in && new_in) {
			//were not in but now are
			quest_proximity_event evt;
			evt.event_id = EVENT_ENTER_AREA;
			evt.client = c;
			evt.npc = nullptr;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		}
	}

	for (auto iter = events.begin(); iter != events.end(); ++iter) {
		quest_proximity_event& evt = (*iter);
		if (evt.npc) {
			std::vector<EQEmu::Any> args;
			parse->EventNPC(evt.event_id, evt.npc, evt.client, "", 0, &args);
		} else {
			std::vector<EQEmu::Any> args;
			args.push_back(&evt.area_id);
			args.push_back(&evt.area_type);
			parse->EventPlayer(evt.event_id, evt.client, "", 0, &args);
		}
	}
}

void EntityList::ProcessMove(NPC *n, float x, float y, float z)
{
	float last_x = n->GetX();
	float last_y = n->GetY();
	float last_z = n->GetZ();

	std::list<quest_proximity_event> events;
	for (auto iter = area_list.begin(); iter != area_list.end(); ++iter) {
		Area& a = (*iter);
		bool old_in = true;
		bool new_in = true;
		if (last_x < a.min_x || last_x > a.max_x ||
				last_y < a.min_y || last_y > a.max_y ||
				last_z < a.min_z || last_z > a.max_z) {
			old_in = false;
		}

		if (x < a.min_x || x > a.max_x ||
				y < a.min_y || y > a.max_y ||
				z < a.min_z || z > a.max_z) {
			new_in = false;
		}

		if (old_in && !new_in) {
			//were in but are no longer.
			quest_proximity_event evt;
			evt.event_id = EVENT_LEAVE_AREA;
			evt.client = nullptr;
			evt.npc = n;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		} else if (!old_in && new_in) {
			//were not in but now are
			quest_proximity_event evt;
			evt.event_id = EVENT_ENTER_AREA;
			evt.client = nullptr;
			evt.npc = n;
			evt.area_id = a.id;
			evt.area_type = a.type;
			events.push_back(evt);
		}
	}

	for (auto iter = events.begin(); iter != events.end(); ++iter) {
		quest_proximity_event& evt = (*iter);
		std::vector<EQEmu::Any> args;
		args.push_back(&evt.area_id);
		args.push_back(&evt.area_type);
		parse->EventNPC(evt.event_id, evt.npc, evt.client, "", 0, &args);
	}
}

void EntityList::AddArea(int id, int type, float min_x, float max_x, float min_y,
		float max_y, float min_z, float max_z)
{
	RemoveArea(id);
	Area a;
	a.id = id;
	a.type = type;
	if (min_x > max_x) {
		a.min_x = max_x;
		a.max_x = min_x;
	} else {
		a.min_x = min_x;
		a.max_x = max_x;
	}

	if (min_y > max_y) {
		a.min_y = max_y;
		a.max_y = min_y;
	} else {
		a.min_y = min_y;
		a.max_y = max_y;
	}

	if (min_z > max_z) {
		a.min_z = max_z;
		a.max_z = min_z;
	} else {
		a.min_z = min_z;
		a.max_z = max_z;
	}

	area_list.push_back(a);
}

void EntityList::RemoveArea(int id)
{
	auto it = std::find_if(area_list.begin(), area_list.end(),
			[id](const Area &a) { return a.id == id; });
	if (it == area_list.end())
		return;

	area_list.erase(it);
}

void EntityList::ClearAreas()
{
	area_list.clear();
}

void EntityList::ProcessProximitySay(const char *Message, Client *c, uint8 language)
{
	if (!Message || !c)
		return;

	auto iter = proximity_list.begin();
	for (; iter != proximity_list.end(); ++iter) {
		NPC *d = (*iter);
		NPCProximity *l = d->proximity;
		if (l == nullptr || !l->say)
			continue;

		if (c->GetX() < l->min_x || c->GetX() > l->max_x
				|| c->GetY() < l->min_y || c->GetY() > l->max_y
				|| c->GetZ() < l->min_z || c->GetZ() > l->max_z)
			continue;

		parse->EventNPC(EVENT_PROXIMITY_SAY, d, c, Message, language);
	}
}

bool EntityList::IsMobInZone(Mob *who)
{
	//We don't use mob_list.find(who) because this code needs to be able to handle dangling pointers for the quest code.
	auto it = mob_list.begin();
	while(it != mob_list.end()) {
		if(it->second == who) {
			return true;
		}
		++it;
	}
	return false;
}

/*
Code to limit the amount of certain NPCs in a given zone.
Primarily used to make a named mob unique within the zone, but written
to be more generic allowing limits larger than 1.

Maintain this stuff in a seperate list since the number
of limited NPCs will most likely be much smaller than the number
of NPCs in the entire zone.
*/
void EntityList::LimitAddNPC(NPC *npc)
{
	if (!npc)
		return;

	SpawnLimitRecord r;

	uint16 eid = npc->GetID();
	r.spawngroup_id = npc->GetSp2();
	r.npc_type = npc->GetNPCTypeID();

	npc_limit_list[eid] = r;
}

void EntityList::LimitRemoveNPC(NPC *npc)
{
	if (!npc)
		return;

	uint16 eid = npc->GetID();
	npc_limit_list.erase(eid);
}

//check a limit over the entire zone.
//returns true if the limit has not been reached
bool EntityList::LimitCheckType(uint32 npc_type, int count)
{
	if (count < 1)
		return true;

	std::map<uint16, SpawnLimitRecord>::iterator cur,end;
	cur = npc_limit_list.begin();
	end = npc_limit_list.end();

	for (; cur != end; ++cur) {
		if (cur->second.npc_type == npc_type) {
			count--;
			if (count == 0) {
				return false;
			}
		}
	}
	return true;
}

//check limits on an npc type in a given spawn group.
//returns true if the limit has not been reached
bool EntityList::LimitCheckGroup(uint32 spawngroup_id, int count)
{
	if (count < 1)
		return true;

	std::map<uint16, SpawnLimitRecord>::iterator cur,end;
	cur = npc_limit_list.begin();
	end = npc_limit_list.end();

	for (; cur != end; ++cur) {
		if (cur->second.spawngroup_id == spawngroup_id) {
			count--;
			if (count == 0) {
				return false;
			}
		}
	}
	return true;
}

//check limits on an npc type in a given spawn group, and
//checks limits on the entire zone in one pass.
//returns true if neither limit has been reached
bool EntityList::LimitCheckBoth(uint32 npc_type, uint32 spawngroup_id, int group_count, int type_count)
{
	if (group_count < 1 && type_count < 1)
		return true;

	std::map<uint16, SpawnLimitRecord>::iterator cur,end;
	cur = npc_limit_list.begin();
	end = npc_limit_list.end();

	for (; cur != end; ++cur) {
		if (cur->second.npc_type == npc_type) {
			type_count--;
			if (type_count == 0) {
				return false;
			}
		}
		if (cur->second.spawngroup_id == spawngroup_id) {
			group_count--;
			if (group_count == 0) {
				return false;
			}
		}
	}
	return true;
}

bool EntityList::LimitCheckName(const char *npc_name)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* npc = it->second;
		if (npc)
			if (strcasecmp(npc_name, npc->GetRawNPCTypeName()) == 0)
				return false;
		++it;
	}
	return true;
}

void EntityList::DestroyTempPets(Mob *owner)
{
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID()) {
				n->Depop();
			}
		}
		++it;
	}
}

int16 EntityList::CountTempPets(Mob *owner)
{
	int16 count = 0;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID()) {
				count++;
			}
		}
		++it;
	}

	owner->SetTempPetCount(count);

	return count;
}

void EntityList::AddTempPetsToHateList(Mob *owner, Mob* other, bool bFrenzy)
{
	if (!other || !owner)
		return;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC* n = it->second;
		if (n->GetSwarmInfo()) {
			if (n->GetSwarmInfo()->owner_id == owner->GetID()) {
				n->CastToNPC()->hate_list.Add(other, 0, 0, bFrenzy);
			}
		}
		++it;
	}
}

bool Entity::CheckCoordLosNoZLeaps(float cur_x, float cur_y, float cur_z,
		float trg_x, float trg_y, float trg_z, float perwalk)
{
	if (zone->zonemap == nullptr)
		return true;

	glm::vec3 myloc;
	glm::vec3 oloc;
	glm::vec3 hit;

	myloc.x = cur_x;
	myloc.y = cur_y;
	myloc.z = cur_z+5;

	oloc.x = trg_x;
	oloc.y = trg_y;
	oloc.z = trg_z+5;

	if (myloc.x == oloc.x && myloc.y == oloc.y && myloc.z == oloc.z)
		return true;

	if (!zone->zonemap->LineIntersectsZoneNoZLeaps(myloc,oloc,perwalk,&hit))
		return true;
	return false;
}

void EntityList::QuestJournalledSayClose(Mob *sender, Client *QuestInitiator,
		float dist, const char* mobname, const char* message)
{
	Client *c = nullptr;
	float dist2 = dist * dist;

	// Send the message to the quest initiator such that the client will enter it into the NPC Quest Journal
	if (QuestInitiator) {
		char *buf = new char[strlen(mobname) + strlen(message) + 10];
		sprintf(buf, "%s says, '%s'", mobname, message);
		QuestInitiator->QuestJournalledMessage(mobname, buf);
		safe_delete_array(buf);
	}
	// Use the old method for all other nearby clients
	for (auto it = client_list.begin(); it != client_list.end(); ++it) {
		c = it->second;
		if(c && (c != QuestInitiator) && DistanceSquared(c->GetPosition(), sender->GetPosition()) <= dist2)
			c->Message_StringID(CC_Default, GENERIC_SAY, mobname, message);
	}
}

Corpse *EntityList::GetClosestCorpse(Mob *sender, const char *Name)
{
	if (!sender)
		return nullptr;

	uint32 CurrentDistance, ClosestDistance = 4294967295u;
	Corpse *CurrentCorpse, *ClosestCorpse = nullptr;

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		CurrentCorpse = it->second;

		++it;

		if (Name && strcasecmp(CurrentCorpse->GetOwnerName(), Name))
			continue;

		CurrentDistance = ((CurrentCorpse->GetY() - sender->GetY()) * (CurrentCorpse->GetY() - sender->GetY())) +
					((CurrentCorpse->GetX() - sender->GetX()) * (CurrentCorpse->GetX() - sender->GetX()));

		if (CurrentDistance < ClosestDistance) {
			ClosestDistance = CurrentDistance;

			ClosestCorpse = CurrentCorpse;

		}
	}
	return ClosestCorpse;
}

void EntityList::ForceGroupUpdate(uint32 gid)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second){
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					database.RefreshGroupFromDB(it->second);
				}
			}
		}
		++it;
	}
}

void EntityList::SendGroupLeave(uint32 gid, const char *name, bool checkleader)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		if (c) {
			Group *g = nullptr;
			g = c->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					strcpy(gj->membername, name);
					gj->action = groupActLeave;
					strcpy(gj->yourname, c->GetName());
					Mob *Leader = g->GetLeader();
					c->QueuePacket(outapp);
					safe_delete(outapp);
					g->DelMemberOOZ(name, checkleader);
				}
			}
		}
		++it;
	}
}

void EntityList::SendGroupLeader(uint32 gid, const char *lname, const char *oldlname)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second){
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					gj->action = groupActMakeLeader;
					strcpy(gj->membername, lname);
					strcpy(gj->yourname, oldlname);
					it->second->QueuePacket(outapp);
					Log.DebugCategory(EQEmuLogSys::Detail, EQEmuLogSys::General, "SendGroupLeader(): Entity loop leader update packet sent to: %s .", it->second->GetName());
					safe_delete(outapp);
				}
			}
		}
		++it;
	}
}

void EntityList::SendGroupJoin(uint32 gid, const char *name)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second){
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid) {
					EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupJoin_Struct));
					GroupJoin_Struct* gj = (GroupJoin_Struct*) outapp->pBuffer;
					strcpy(gj->membername, name);
					gj->action = groupActJoin;
					strcpy(gj->yourname, it->second->GetName());
					Mob *Leader = g->GetLeader();
					it->second->QueuePacket(outapp);
					safe_delete(outapp);
				}
			}
		}
		++it;
	}
}

void EntityList::GroupMessage(uint32 gid, const char *from, const char *message)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second) {
			Group *g = nullptr;
			g = it->second->GetGroup();
			if (g) {
				if (g->GetID() == gid)
					it->second->ChannelMessageSend(from, it->second->GetName(), 2, 0, message);
			}
		}
		++it;
	}
}

uint16 EntityList::CreateGroundObject(uint32 itemid, const glm::vec4& position, uint32 decay_time)
{
	const Item_Struct *is = database.GetItem(itemid);
	if (!is)
		return 0;

	ItemInst *i = new ItemInst(is, is->MaxCharges);
	if (!i)
		return 0;

	Object *object = new Object(i, position.x, position.y, position.z, position.w,decay_time);
	entity_list.AddObject(object, true);

	safe_delete(i);
	if (!object)
		return 0;

	return object->GetID();
}

uint16 EntityList::CreateGroundObjectFromModel(const char *model, const glm::vec4& position, uint8 type, uint32 decay_time)
{
	if (!model)
		return 0;

	Object *object = new Object(model, position.x, position.y, position.z, position.w, type);
	entity_list.AddObject(object, true);

	if (!object)
		return 0;

	return object->GetID();
}

uint16 EntityList::CreateDoor(const char *model, const glm::vec4& position, uint8 opentype, uint16 size)
{
	if (!model)
		return 0; // fell through everything, this is bad/incomplete from perl

	Doors *door = new Doors(model, position, opentype, size);
	RemoveAllDoors();
	zone->LoadZoneDoors(zone->GetShortName(), zone->GetInstanceVersion());
	entity_list.AddDoor(door);
	entity_list.RespawnAllDoors();

	if (door)
		return door->GetEntityID();

	return 0; // fell through everything, this is bad/incomplete from perl
}


Mob *EntityList::GetTargetForMez(Mob *caster)
{
	if (!caster)
		return nullptr;

	auto it = mob_list.begin();
	//TODO: make this smarter and not mez targets being damaged by dots
	while (it != mob_list.end()) {
		Mob *d = it->second;
		if (d) {
			if (d == caster) { //caster can't pick himself
				++it;
				continue;
			}

			if (caster->GetTarget() == d) { //caster can't pick his target
				++it;
				continue;
			}

			if (!caster->CheckAggro(d)) { //caster can't pick targets that aren't aggroed on himself
				++it;
				continue;
			}

			if (DistanceSquared(caster->GetPosition(), d->GetPosition()) > 22250) { //only pick targets within 150 range
				++it;
				continue;
			}

			if (!caster->CheckLosFN(d)) {	//this is wasteful but can't really think of another way to do it
				++it;						//that wont have us trying to los the same target every time
				continue;					//it's only in combat so it's impact should be minimal.. but stil.
			}
			return d;
		}
		++it;
	}
	return nullptr;
}

void EntityList::SendZoneAppearance(Client *c)
{
	if (!c)
		return;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *cur = it->second;

		if (cur) {
			if (cur == c) {
				++it;
				continue;
			}
			if (cur->GetAppearance() != eaStanding) {
				cur->SendAppearancePacket(AT_Anim, cur->GetAppearanceValue(cur->GetAppearance()), false, true, c);
			}
			if (cur->GetSize() != cur->GetBaseSize()) {
				cur->SendAppearancePacket(AT_Size, (uint32)cur->GetSize(), false, true, c);
			}
		}
		++it;
	}
}

void EntityList::SendNimbusEffects(Client *c)
{
	if (!c)
		return;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *cur = it->second;

		if (cur) {
			if (cur == c) {
				++it;
				continue;
			}
			if (cur->GetNimbusEffect1() != 0) {
				cur->SendSpellEffect(cur->GetNimbusEffect1(), 1000, 0, 1, 3000, false, c);
			}
			if (cur->GetNimbusEffect2() != 0) {
				cur->SendSpellEffect(cur->GetNimbusEffect2(), 2000, 0, 1, 3000, false, c);
			}
			if (cur->GetNimbusEffect3() != 0) {
				cur->SendSpellEffect(cur->GetNimbusEffect3(), 3000, 0, 1, 3000, false, c);
			}
		}
		++it;
	}
}

void EntityList::SendUntargetable(Client *c)
{
	if (!c)
		return;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *cur = it->second;

		if (cur) {
			if (cur == c) {
				++it;
				continue;
			}
			if (!cur->IsTargetable())
				cur->SendTargetable(false, c);
		}
		++it;
	}
}

void EntityList::ZoneWho(Client *c, Who_All_Struct *Who)
{
	// This is only called for SoF clients, as regular /who is now handled server-side for that client. Remove later, seems to not be used on eqmac.
	uint32 PacketLength = 0;
	uint32 Entries = 0;
	uint8 WhomLength = strlen(Who->whom);

	std::list<Client *> client_sub_list;
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ClientEntry = it->second;
		++it;

		if (ClientEntry) {
			if (ClientEntry->GMHideMe(c))
				continue;
			if ((Who->wrace != 0xFFFFFFFF) && (ClientEntry->GetRace() != Who->wrace))
				continue;
			if ((Who->wclass != 0xFFFFFFFF) && (ClientEntry->GetClass() != Who->wclass))
				continue;
			if ((Who->lvllow != 0xFFFFFFFF) && (ClientEntry->GetLevel() < Who->lvllow))
				continue;
			if ((Who->lvlhigh != 0xFFFFFFFF) && (ClientEntry->GetLevel() > Who->lvlhigh))
				continue;
			if (Who->guildid != 0xFFFFFFFF) {
				if ((Who->guildid == 0xFFFFFFFC) && !ClientEntry->IsTrader())
					continue;
				if ((Who->guildid == 0xFFFFFFFB) && !ClientEntry->IsBuyer())
					continue;
				if (Who->guildid != ClientEntry->GuildID())
					continue;
			}
			if (WhomLength && strncasecmp(Who->whom, ClientEntry->GetName(), WhomLength) &&
					strncasecmp(guild_mgr.GetGuildName(ClientEntry->GuildID()), Who->whom, WhomLength))
				continue;

			Entries++;
			client_sub_list.push_back(ClientEntry);

			PacketLength = PacketLength + strlen(ClientEntry->GetName());

			if (strlen(guild_mgr.GetGuildName(ClientEntry->GuildID())) > 0)
				PacketLength = PacketLength + strlen(guild_mgr.GetGuildName(ClientEntry->GuildID())) + 2;
		}
	}

	PacketLength = PacketLength + sizeof(WhoAllReturnStruct) + (47 * Entries);
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_WhoAllResponse, PacketLength);
	char *Buffer = (char *)outapp->pBuffer;
	WhoAllReturnStruct *WARS = (WhoAllReturnStruct *)Buffer;
	WARS->id = 0;
	WARS->playerineqstring = 5001;
	strncpy(WARS->line, "---------------------------", sizeof(WARS->line));
	WARS->unknown35 = 0x0a;
	WARS->unknown36 = 0;

	switch(Entries) {
		case 0:
			WARS->playersinzonestring = 5029;
			break;
		case 1:
			WARS->playersinzonestring = 5028; // 5028 There is %1 player in EverQuest.
			break;
		default:
			WARS->playersinzonestring = 5036; // 5036 There are %1 players in EverQuest.
	}

	WARS->unknown44[0] = 0;
	WARS->unknown44[1] = 0;
	WARS->unknown44[2] = 0;

	WARS->unknown44[3] = 0;

	WARS->unknown44[4] = 0;

	WARS->unknown52 = Entries;
	WARS->unknown56 = Entries;
	WARS->playercount = Entries;
	Buffer += sizeof(WhoAllReturnStruct);

	auto sit = client_sub_list.begin();
	while (sit != client_sub_list.end()) {
		Client *ClientEntry = *sit;
		++sit;

		if (ClientEntry) {
			if (ClientEntry->GMHideMe(c))
				continue;
			if ((Who->wrace != 0xFFFFFFFF) && (ClientEntry->GetRace() != Who->wrace))
				continue;
			if ((Who->wclass != 0xFFFFFFFF) && (ClientEntry->GetClass() != Who->wclass))
				continue;
			if ((Who->lvllow != 0xFFFFFFFF) && (ClientEntry->GetLevel() < Who->lvllow))
				continue;
			if ((Who->lvlhigh != 0xFFFFFFFF) && (ClientEntry->GetLevel() > Who->lvlhigh))
				continue;
			if (Who->guildid != 0xFFFFFFFF) {
				if ((Who->guildid == 0xFFFFFFFC) && !ClientEntry->IsTrader())
					continue;
				if ((Who->guildid == 0xFFFFFFFB) && !ClientEntry->IsBuyer())
					continue;
				if (Who->guildid != ClientEntry->GuildID())
					continue;
			}
			if (WhomLength && strncasecmp(Who->whom, ClientEntry->GetName(), WhomLength) &&
					strncasecmp(guild_mgr.GetGuildName(ClientEntry->GuildID()), Who->whom, WhomLength))
				continue;
			std::string GuildName;
			if ((ClientEntry->GuildID() != GUILD_NONE) && (ClientEntry->GuildID() > 0)) {
				GuildName = "<";
				GuildName += guild_mgr.GetGuildName(ClientEntry->GuildID());
				GuildName += ">";
			}
			uint32 FormatMSGID = 5025; // 5025 %T1[%2 %3] %4 (%5) %6 %7 %8 %9
			if (ClientEntry->GetAnon() == 1)
				FormatMSGID = 5024; // 5024 %T1[ANONYMOUS] %2 %3
			else if (ClientEntry->GetAnon() == 2)
				FormatMSGID = 5023; // 5023 %T1[ANONYMOUS] %2 %3 %4
			uint32 PlayerClass = 0;
			uint32 PlayerLevel = 0;
			uint32 PlayerRace = 0;
			uint32 ZoneMSGID = 0xFFFFFFFF;

			if (ClientEntry->GetAnon()==0) {
				PlayerClass = ClientEntry->GetClass();
				PlayerLevel = ClientEntry->GetLevel();
				PlayerRace = ClientEntry->GetRace();
			}

			WhoAllPlayerPart1* WAPP1 = (WhoAllPlayerPart1*)Buffer;
			WAPP1->FormatMSGID = FormatMSGID;
			WAPP1->PIDMSGID = 0xFFFFFFFF;
			strcpy(WAPP1->Name, ClientEntry->GetName());
			Buffer += sizeof(WhoAllPlayerPart1) + strlen(WAPP1->Name);
			WhoAllPlayerPart2* WAPP2 = (WhoAllPlayerPart2*)Buffer;

			if (ClientEntry->IsTrader())
				WAPP2->RankMSGID = 12315;
			else if (ClientEntry->IsBuyer())
				WAPP2->RankMSGID = 6056;
			else if (ClientEntry->Admin() >= 10)
				WAPP2->RankMSGID = 12312;
			else
				WAPP2->RankMSGID = 0xFFFFFFFF;

			strcpy(WAPP2->Guild, GuildName.c_str());
			Buffer += sizeof(WhoAllPlayerPart2) + strlen(WAPP2->Guild);
			WhoAllPlayerPart3* WAPP3 = (WhoAllPlayerPart3*)Buffer;
			WAPP3->Unknown80[0] = 0xFFFFFFFF;

			if (ClientEntry->IsLD())
				WAPP3->Unknown80[1] = 12313; // LinkDead
			else
				WAPP3->Unknown80[1] = 0xFFFFFFFF;

			WAPP3->ZoneMSGID = ZoneMSGID;
			WAPP3->Zone = 0;
			WAPP3->Class_ = PlayerClass;
			WAPP3->Level = PlayerLevel;
			WAPP3->Race = PlayerRace;
			WAPP3->Account[0] = 0;
			Buffer += sizeof(WhoAllPlayerPart3);
			WhoAllPlayerPart4* WAPP4 = (WhoAllPlayerPart4*)Buffer;
			WAPP4->Unknown100 = 0;
			Buffer += sizeof(WhoAllPlayerPart4);
		}

	}

	c->QueuePacket(outapp);

	safe_delete(outapp);
}

uint32 EntityList::CheckNPCsClose(Mob *center)
{
	uint32 count = 0;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *cur = it->second;
		if (!cur || cur == center || cur->IsPet() || cur->GetBodyType() == BT_NoTarget || cur->GetBodyType() == BT_Special) {
			++it;
			continue;
		}

		float xDiff = cur->GetX() - center->GetX();
		float yDiff = cur->GetY() - center->GetY();
		float zDiff = cur->GetZ() - center->GetZ();
		float dist = ((xDiff * xDiff) + (yDiff * yDiff) + (zDiff * zDiff));

		++it;
	}
	return count;
}

void EntityList::GateAllClients()
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *c = it->second;
		if (c)
			c->GoToBind();
		++it;
	}
}

void EntityList::SignalAllClients(uint32 data)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		Client *ent = it->second;
		if (ent)
			ent->Signal(data);
		++it;
	}
}

uint16 EntityList::GetClientCount(){
	uint16 ClientCount = 0;
	std::list<Client*> client_list;
	entity_list.GetClientList(client_list);
	std::list<Client*>::iterator iter = client_list.begin();
	while (iter != client_list.end()) {
		Client *entry = (*iter);
		entry->GetCleanName();
		ClientCount++;
		iter++;
	}
	return ClientCount;
}

void EntityList::GetMobList(std::list<Mob *> &m_list)
{
	m_list.clear();
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		m_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetNPCList(std::list<NPC *> &n_list)
{
	n_list.clear();
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		n_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetClientList(std::list<Client *> &c_list)
{
	c_list.clear();
	auto it = client_list.begin();
	while (it != client_list.end()) {
		c_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetCorpseList(std::list<Corpse *> &c_list)
{
	c_list.clear();
	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		c_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetObjectList(std::list<Object *> &o_list)
{
	o_list.clear();
	auto it = object_list.begin();
	while (it != object_list.end()) {
		o_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetDoorsList(std::list<Doors*> &o_list)
{
	o_list.clear();
	auto it = door_list.begin();
	while (it != door_list.end()) {
		o_list.push_back(it->second);
		++it;
	}
}

void EntityList::GetSpawnList(std::list<Spawn2*> &o_list)
{
	o_list.clear();
	if(zone) {
		LinkedListIterator<Spawn2*> iterator(zone->spawn2_list);
		iterator.Reset();
		while(iterator.MoreElements())
		{
			Spawn2 *ent = iterator.GetData();
			o_list.push_back(ent);
			iterator.Advance();
		}
	}
}

void EntityList::UpdateQGlobal(uint32 qid, QGlobal newGlobal)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *ent = it->second;

		if (ent->IsClient()) {
			QGlobalCache *qgc = ent->CastToClient()->GetQGlobals();
			if (qgc) {
				uint32 char_id = ent->CastToClient()->CharacterID();
				if (newGlobal.char_id == char_id && newGlobal.npc_id == 0)
					qgc->AddGlobal(qid, newGlobal);
			}
		} else if (ent->IsNPC()) {
			QGlobalCache *qgc = ent->CastToNPC()->GetQGlobals();
			if (qgc) {
				uint32 npc_id = ent->GetNPCTypeID();
				if (newGlobal.npc_id == npc_id)
					qgc->AddGlobal(qid, newGlobal);
			}
		}
		++it;
	}
}

void EntityList::DeleteQGlobal(std::string name, uint32 npcID, uint32 charID, uint32 zoneID)
{
	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *ent = it->second;

		if (ent->IsClient()) {
			QGlobalCache *qgc = ent->CastToClient()->GetQGlobals();
			if (qgc)
				qgc->RemoveGlobal(name, npcID, charID, zoneID);
		} else if (ent->IsNPC()) {
			QGlobalCache *qgc = ent->CastToNPC()->GetQGlobals();
			if (qgc)
				qgc->RemoveGlobal(name, npcID, charID, zoneID);
		}
		++it;
	}
}

void EntityList::HideCorpses(Client *c, uint8 CurrentMode, uint8 NewMode)
{
	if (!c)
		return;

	if (NewMode == HideCorpseNone) {
		SendZoneCorpses(c);
		return;
	}

	Group *g = nullptr;

	if (NewMode == HideCorpseAllButGroup) {
		g = c->GetGroup();

		if (!g)
			NewMode = HideCorpseAll;
	}

	auto it = corpse_list.begin();
	while (it != corpse_list.end()) {
		Corpse *b = it->second;

		if (b && (b->GetCharID() != c->CharacterID())) {
			if ((NewMode == HideCorpseAll) || ((NewMode == HideCorpseNPC) && (b->IsNPCCorpse()))) {
				EQApplicationPacket outapp;
					b->CreateDespawnPacket(&outapp, false);
				c->QueuePacket(&outapp);
			} else if(NewMode == HideCorpseAllButGroup) {
				if (!g->IsGroupMember(b->GetOwnerName())) {
					EQApplicationPacket outapp;
						b->CreateDespawnPacket(&outapp, false);
					c->QueuePacket(&outapp);
				} else if((CurrentMode == HideCorpseAll)) {
					EQApplicationPacket outapp;
						b->CreateSpawnPacket(&outapp);
					c->QueuePacket(&outapp);
				}
			}
		}
		++it;
	}
}

void EntityList::AddLootToNPCS(uint32 item_id, uint32 count)
{
	if (count == 0)
		return;

	int npc_count = 0;
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (!it->second->IsPet()
				&& it->second->GetBodyType() != BT_NoTarget
				&& it->second->GetBodyType() != BT_NoTarget2
				&& it->second->GetBodyType() != BT_Special)
			npc_count++;
		++it;
	}

	if (npc_count == 0)
		return;

	NPC **npcs = new NPC*[npc_count];
	int *counts = new int[npc_count];
	bool *marked = new bool[npc_count];
	memset(counts, 0, sizeof(int) * npc_count);
	memset(marked, 0, sizeof(bool) * npc_count);

	int i = 0;
	it = npc_list.begin();
	while (it != npc_list.end()) {
		if (!it->second->IsPet()
				&& it->second->GetBodyType() != BT_NoTarget
				&& it->second->GetBodyType() != BT_NoTarget2
				&& it->second->GetBodyType() != BT_Special)
			npcs[i++] = it->second;
		++it;
	}

	while (count > 0) {
		std::vector<int> selection;
		selection.reserve(npc_count);
		for (int j = 0; j < npc_count; ++j)
			selection.push_back(j);

		while (selection.size() > 0 && count > 0) {
			int k = zone->random.Int(0, selection.size() - 1);
			counts[selection[k]]++;
			count--;
			selection.erase(selection.begin() + k);
		}
	}

	for (int j = 0; j < npc_count; ++j)
		if (counts[j] > 0)
			for (int k = 0; k < counts[j]; ++k)
				npcs[j]->AddItem(item_id, 1);

	safe_delete_array(npcs);
	safe_delete_array(counts);
	safe_delete_array(marked);
}

NPC *EntityList::GetClosestBanker(Mob *sender, uint32 &distance)
{
	if (!sender)
		return nullptr;

	distance = 4294967295u;
	NPC *nc = nullptr;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		if (it->second->GetClass() == BANKER) {
			uint32 nd = ((it->second->GetY() - sender->GetY()) * (it->second->GetY() - sender->GetY())) +
				((it->second->GetX() - sender->GetX()) * (it->second->GetX() - sender->GetX()));
			if (nd < distance){
				distance = nd;
				nc = it->second;
			}
		}
		++it;
	}
	return nc;
}

Mob *EntityList::GetClosestMobByBodyType(Mob *sender, bodyType BodyType)
{

	if (!sender)
		return nullptr;

	uint32 CurrentDistance, ClosestDistance = 4294967295u;
	Mob *CurrentMob, *ClosestMob = nullptr;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		CurrentMob = it->second;
		++it;

		if (CurrentMob->GetBodyType() != BodyType)
			continue;

		CurrentDistance = ((CurrentMob->GetY() - sender->GetY()) * (CurrentMob->GetY() - sender->GetY())) +
					((CurrentMob->GetX() - sender->GetX()) * (CurrentMob->GetX() - sender->GetX()));

		if (CurrentDistance < ClosestDistance) {
			ClosestDistance = CurrentDistance;
			ClosestMob = CurrentMob;
		}
	}
	return ClosestMob;
}

Mob *EntityList::GetClosestClient(Mob *sender, uint32 &distance)
{
	if (!sender)
		return nullptr;

	distance = 4294967295u;
	Mob *nc = nullptr;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		if (it->second->IsClient() || (it->second->GetOwner() && it->second->GetOwner()->IsClient())) {
			uint32 nd = ((it->second->GetY() - sender->GetY()) * (it->second->GetY() - sender->GetY())) +
				((it->second->GetX() - sender->GetX()) * (it->second->GetX() - sender->GetX()));
			if (nd < distance){
				distance = nd;
				nc = it->second;
			}
		}
		++it;
	}
	return nc;
}

void EntityList::GetTargetsForConeArea(Mob *start, float min_radius, float radius, float height, std::list<Mob*> &m_list)
{
	auto it = mob_list.begin();
	while (it !=  mob_list.end()) {
		Mob *ptr = it->second;
		if (ptr == start) {
			++it;
			continue;
		}
		float x_diff = ptr->GetX() - start->GetX();
		float y_diff = ptr->GetY() - start->GetY();
		float z_diff = ptr->GetZ() - start->GetZ();

		x_diff *= x_diff;
		y_diff *= y_diff;
		z_diff *= z_diff;

		if ((x_diff + y_diff) <= (radius * radius) && (x_diff + y_diff) >= (min_radius * min_radius))
			if(z_diff <= (height * height))
				m_list.push_back(ptr);

		++it;
	}
}

Client *EntityList::FindCorpseDragger(uint16 CorpseID)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->IsDraggingCorpse(CorpseID))
			return it->second;
		++it;
	}
	return nullptr;
}

Mob *EntityList::GetTargetForVirus(Mob *spreader, int range)
{
	int max_spread_range = RuleI(Spells, VirusSpreadDistance);

	if (range)
		max_spread_range = range;

	std::vector<Mob *> TargetsInRange;

	auto it = mob_list.begin();
	while (it != mob_list.end()) {
		Mob *cur = it->second;
		// Make sure the target is in range, has los and is not the mob doing the spreading
		if ((cur->GetID() != spreader->GetID()) &&
				(cur->CalculateDistance(spreader->GetX(), spreader->GetY(),
					spreader->GetZ()) <= max_spread_range) &&
				(spreader->CheckLosFN(cur))) {
			// If the spreader is an npc it can only spread to other npc controlled mobs
			if (spreader->IsNPC() && !spreader->IsPet() && !spreader->CastToNPC()->GetSwarmOwner() && cur->IsNPC()) {
				TargetsInRange.push_back(cur);
			}
			// If the spreader is an npc controlled pet it can spread to any other npc or an npc controlled pet
			else if (spreader->IsNPC() && spreader->IsPet() && spreader->GetOwner()->IsNPC()) {
				if (cur->IsNPC() && !cur->IsPet()) {
					TargetsInRange.push_back(cur);
				} else if (cur->IsNPC() && cur->IsPet() && cur->GetOwner()->IsNPC()) {
					TargetsInRange.push_back(cur);
				}
				else if (cur->IsNPC() && cur->CastToNPC()->GetSwarmOwner() && cur->GetOwner()->IsNPC()) {
					TargetsInRange.push_back(cur);
				}
			}
			// if the spreader is anything else(bot, pet, etc) then it should spread to everything but non client controlled npcs
			else if (!spreader->IsNPC() && !cur->IsNPC()) {
				TargetsInRange.push_back(cur);
			}
			// if its a pet we need to determine appropriate targets(pet to client, pet to pet, pet to bot, etc)
			else if (spreader->IsNPC() && (spreader->IsPet() || spreader->CastToNPC()->GetSwarmOwner()) && !spreader->GetOwner()->IsNPC()) {
				if (!cur->IsNPC()) {
					TargetsInRange.push_back(cur);
				}
				else if (cur->IsNPC() && (cur->IsPet() || cur->CastToNPC()->GetSwarmOwner()) && !cur->GetOwner()->IsNPC()) {
					TargetsInRange.push_back(cur);
				}
			}
		}
		++it;
	}

	if(TargetsInRange.size() == 0)
		return nullptr;

	return TargetsInRange[zone->random.Int(0, TargetsInRange.size() - 1)];
}

void EntityList::SendLFG(Client* client, bool lfg)
{
	auto it = client_list.begin();
	while (it != client_list.end()) {
		if (it->second->IsLFG() == lfg) 
		{
			EQApplicationPacket	*outapp = new EQApplicationPacket(OP_LFGCommand, sizeof(LFG_Appearance_Struct));
			LFG_Appearance_Struct* lfga = (LFG_Appearance_Struct*)outapp->pBuffer;
			lfga->entityid = it->second->GetID();
			lfga->value = it->second->IsLFG();

			client->QueuePacket(outapp);
			safe_delete(outapp);
		}
		++it;
	}
}
void EntityList::StopMobAI()
{
	for (auto &mob : mob_list) {
		mob.second->AI_Stop();
		mob.second->AI_ShutDown();
	}
}
