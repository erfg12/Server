/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemu.org)

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
#include "masterentity.h"
#include "npc_ai.h"
#include "../common/packet_functions.h"
#include "../common/packet_dump.h"
#include "../common/string_util.h"
#include "worldserver.h"

extern EntityList entity_list;
extern WorldServer worldserver;

/*

note about how groups work:
A group contains 2 list, a list of pointers to members and a
list of member names. All members of a group should have their
name in the membername array, wether they are in the zone or not.
Only members in this zone will have non-null pointers in the
members array.

*/

//create a group which should allready exist in the database
Group::Group(uint32 gid)
: GroupIDConsumer(gid)
{
	leader = nullptr;
	memset(members,0,sizeof(Mob*) * MAX_GROUP_MEMBERS);
	uint32 i;
	for(i=0;i<MAX_GROUP_MEMBERS;i++)
	{
		memset(membername[i],0,64);
		MemberRoles[i] = 0;
	}

	if(gid != 0) {
		if(!LearnMembers())
			SetID(0);

		if(GetLeader() != nullptr)
		{
			SetOldLeaderName(GetLeaderName());
		}
	}
}

//creating a new group
Group::Group(Mob* leader)
: GroupIDConsumer()
{
	memset(members, 0, sizeof(members));
	members[0] = leader;
	leader->SetGrouped(true);
	SetLeader(leader);
	SetOldLeaderName(leader->GetName());
	Log.Out(Logs::Detail, Logs::Group, "Group:Group() Setting OldLeader to: %s and Leader to: %s", GetOldLeaderName(), leader->GetName());
	uint32 i;
	for(i=0;i<MAX_GROUP_MEMBERS;i++)
	{
		memset(membername[i],0,64);
		MemberRoles[i] = 0;
	}
	strcpy(membername[0],leader->GetName());

	if(leader->IsClient())
		strcpy(leader->CastToClient()->GetPP().groupMembers[0],leader->GetName());

}

Group::~Group()
{

}

//Cofruben:Split money used in OP_Split.
//Rewritten by Father Nitwit
void Group::SplitMoney(uint32 copper, uint32 silver, uint32 gold, uint32 platinum, Client *splitter) {
	//avoid unneeded work
	if(copper == 0 && silver == 0 && gold == 0 && platinum == 0)
		return;

	uint32 i;
	uint8 membercount = 0;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr) {
			membercount++;
		}
	}

	if (membercount == 0)
		return;

	uint32 mod;
	//try to handle round off error a little better
	if(membercount > 1) {
		mod = platinum % membercount;
		if((mod) > 0) {
			platinum -= mod;
			gold += 10 * mod;
		}
		mod = gold % membercount;
		if((mod) > 0) {
			gold -= mod;
			silver += 10 * mod;
		}
		mod = silver % membercount;
		if((mod) > 0) {
			silver -= mod;
			copper += 10 * mod;
		}
	}

	//calculate the splits
	//We can still round off copper pieces, but I dont care
	uint32 sc;
	uint32 cpsplit = copper / membercount;
	sc = copper % membercount;
	uint32 spsplit = silver / membercount;
	uint32 gpsplit = gold / membercount;
	uint32 ppsplit = platinum / membercount;

	char buf[128];
	buf[63] = '\0';
	std::string msg = "You receive";
	bool one = false;

	if(ppsplit > 0) {
		snprintf(buf, 63, " %u platinum", ppsplit);
		msg += buf;
		one = true;
	}
	if(gpsplit > 0) {
		if(one)
			msg += ",";
		snprintf(buf, 63, " %u gold", gpsplit);
		msg += buf;
		one = true;
	}
	if(spsplit > 0) {
		if(one)
			msg += ",";
		snprintf(buf, 63, " %u silver", spsplit);
		msg += buf;
		one = true;
	}
	if(cpsplit > 0) {
		if(one)
			msg += ",";
		//this message is not 100% accurate for the splitter
		//if they are receiving any roundoff
		snprintf(buf, 63, " %u copper", cpsplit);
		msg += buf;
		one = true;
	}
	msg += " as your split";

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr && members[i]->IsClient()) { // If Group Member is Client
				Client *c = members[i]->CastToClient();
			//I could not get MoneyOnCorpse to work, so we use this
			c->AddMoneyToPP(cpsplit, spsplit, gpsplit, ppsplit, true);
			c->Message(2, msg.c_str());
		}
	}
}

bool Group::AddMember(Mob* newmember, const char *NewMemberName, uint32 CharacterID)
{
	bool InZone = true;

	// This method should either be passed a Mob*, if the new member is in this zone, or a nullptr Mob*
	// and the name and CharacterID of the new member, if they are out of zone.
	//
	if (!newmember && !NewMemberName)
		return false;

	if (GroupCount() >= MAX_GROUP_MEMBERS) //Sanity check for merging groups together.
		return false;

	if (!newmember)
		InZone = false;
	else
	{
		NewMemberName = newmember->GetCleanName();

		if (newmember->IsClient())
			CharacterID = newmember->CastToClient()->CharacterID();
	}

	uint32 i = 0;

	// See if they are already in the group
	//
	for (i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if (!strcasecmp(membername[i], NewMemberName))
			return false;

	// Put them in the group
	for (i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if (membername[i][0] == '\0')
		{
			if (InZone)
				members[i] = newmember;

			break;
		}
	}

	if (i == MAX_GROUP_MEMBERS)
		return false;

	strcpy(membername[i], NewMemberName);
	MemberRoles[i] = 0;

	int x = 1;

	//build the template join packet
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gj = (GroupJoin_Struct*)outapp->pBuffer;
	strcpy(gj->membername, NewMemberName);
	gj->action = groupActJoin;

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr && members[i] != newmember) {
			//fill in group join & send it
			strcpy(gj->yourname, members[i]->GetCleanName());
			if (members[i]->IsClient()) {
				members[i]->CastToClient()->QueuePacket(outapp);

				//put new member into existing person's list
				strcpy(members[i]->CastToClient()->GetPP().groupMembers[this->GroupCount() - 1], NewMemberName);
			}

			//put this existing person into the new member's list
			if (InZone && newmember->IsClient()) {
				if (IsLeader(members[i]))
					strcpy(newmember->CastToClient()->GetPP().groupMembers[0], members[i]->GetCleanName());
				else {
					strcpy(newmember->CastToClient()->GetPP().groupMembers[x], members[i]->GetCleanName());
					x++;
				}
			}
		}
	}

	if (InZone)
	{
		//put new member in his own list.
		newmember->SetGrouped(true);

		if (newmember->IsClient())
		{
			strcpy(newmember->CastToClient()->GetPP().groupMembers[x], NewMemberName);
			newmember->CastToClient()->Save();
			database.SetGroupID(NewMemberName, GetID(), newmember->CastToClient()->CharacterID());
		}
	}
	else
		database.SetGroupID(NewMemberName, GetID(), CharacterID);

	safe_delete(outapp);

	return true;
}

void Group::AddMember(const char *NewMemberName)
{
	// This method should be called when both the new member and the group leader are in a different zone to this one.
	//
	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if (!strcasecmp(membername[i], NewMemberName))
		{
			return;
		}

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if (membername[i][0] == '\0')
		{
			strcpy(membername[i], NewMemberName);
			MemberRoles[i] = 0;
			break;
		}
	}
}


void Group::QueuePacket(const EQApplicationPacket *app, bool ack_req)
{
	uint32 i;
	for(i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if(members[i] && members[i]->IsClient())
		{
			members[i]->CastToClient()->QueuePacket(app, ack_req);
		}
	}
}

// sends the rest of the group's hps to member. this is useful when
// someone first joins a group, but otherwise there shouldn't be a need to
// call it
void Group::SendHPPacketsTo(Mob *member)
{
	if(member && member->IsClient())
	{
		EQApplicationPacket hpapp;

		for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++)
		{
			if(members[i] && members[i] != member)
			{
				members[i]->CreateHPPacket(&hpapp);
				member->CastToClient()->QueuePacket(&hpapp, false);
			}
		}
	}
}

void Group::SendHPPacketsFrom(Mob *member)
{
	EQApplicationPacket hp_app;
	if(!member)
		return;

	member->CreateHPPacket(&hp_app);

	uint32 i;
	for(i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(members[i] && members[i] != member && members[i]->IsClient())
		{
			members[i]->CastToClient()->QueuePacket(&hp_app);
		}
	}
}

//updates a group member's client pointer when they zone in
//if the group was in the zone already
bool Group::UpdatePlayer(Mob* update){

	VerifyGroup();

	uint32 i=0;
	if(update->IsClient()) {
		//update their player profile
		PlayerProfile_Struct &pp = update->CastToClient()->GetPP();
		for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if(membername[i][0] == '\0')
				memset(pp.groupMembers[i], 0, 64);
			else
				strn0cpy(pp.groupMembers[i], membername[i], 64);
		}
	}

	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (!strcasecmp(membername[i],update->GetName()))
		{
			members[i] = update;
			members[i]->SetGrouped(true);
			return true;
		}
	}
	return false;
}


void Group::MemberZoned(Mob* removemob) {
	uint32 i;

	if (removemob == nullptr)
		return;

	if(removemob == GetLeader())
		SetLeader(nullptr);

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if (members[i] == removemob) {
				members[i] = nullptr;
				//should NOT clear the name, it is used for world communication.
				break;
			}
	}
}

void Group::SendGroupJoinOOZ(Mob* NewMember) {

	if (NewMember == nullptr)
	{
		return;
	}
	
	if (!NewMember->HasGroup())
	{
		return;
	}

	//send updates to clients out of zone...
	ServerPacket* pack = new ServerPacket(ServerOP_GroupJoin, sizeof(ServerGroupJoin_Struct));
	ServerGroupJoin_Struct* gj = (ServerGroupJoin_Struct*)pack->pBuffer;
	gj->gid = GetID();
	gj->zoneid = zone->GetZoneID();
	gj->instance_id = zone->GetInstanceID();
	strcpy(gj->member_name, NewMember->GetName());
	worldserver.SendPacket(pack);
	safe_delete(pack);

}

bool Group::DelMemberOOZ(const char *Name, bool checkleader) {

	if(!Name) return false;

	bool removed = false;
	// If a member out of zone has disbanded, clear out their name.
	for(unsigned int i = 0; i < MAX_GROUP_MEMBERS; i++) 
	{
		if(!strcasecmp(Name, membername[i]))
		{
			// This shouldn't be called if the member is in this zone.
			if(!members[i]) {
				memset(membername[i], 0, 64);
				MemberRoles[i] = 0;
				removed = true;
				Log.Out(Logs::Detail, Logs::Group, "DelMemberOOZ: Removed Member: %s", Name);
				break;
			}
		}
	}

	if(GroupCount() < 2)
	{
		DisbandGroup();
		return true;
	}

	if(checkleader)
	{
		Log.Out(Logs::Detail, Logs::Group, "DelMemberOOZ: Checking leader...");
		if (strcmp(GetOldLeaderName(),Name) == 0 && GroupCount() >= 2)
		{
			for(uint32 nl = 0; nl < MAX_GROUP_MEMBERS; nl++)
			{
				if(members[nl]) 
				{
					if (members[nl]->IsClient())
					{
						ChangeLeader(members[nl]);
						break;
					}
				}
			}
		}
	}

	return removed;
}

bool Group::DelMember(Mob* oldmember,bool ignoresender)
{
	if (oldmember == nullptr)
	{
		return false;
	}

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] == oldmember)
		{
			members[i] = nullptr;
			membername[i][0] = '\0';
			memset(membername[i],0,64);
			MemberRoles[i] = 0;
			Log.Out(Logs::Detail, Logs::Group, "DelMember: Removed Member: %s", oldmember->GetCleanName());
			break;
		}
	}

	if(GroupCount() < 2)
	{
		DisbandGroup();
		return true;
	}

	// If the leader has quit and we have 2 or more players left in group, we want to first check the zone the old leader was in for a new leader. 
	// If a suitable replacement cannot be found, we need to go out of zone. If checkleader remains true after this method completes, another
	// loop will be run in DelMemberOOZ.
	bool checkleader = true;
	if (strcmp(GetOldLeaderName(),oldmember->GetCleanName()) == 0 && GroupCount() >= 2)
	{
		for(uint32 nl = 0; nl < MAX_GROUP_MEMBERS; nl++)
		{
			if(members[nl]) 
			{
				if (members[nl]->IsClient())
				{
					ChangeLeader(members[nl]);
					checkleader = false;
					break;
				}
			}
		}
	}
			
	ServerPacket* pack = new ServerPacket(ServerOP_GroupLeave, sizeof(ServerGroupLeave_Struct));
	ServerGroupLeave_Struct* gl = (ServerGroupLeave_Struct*)pack->pBuffer;
	gl->gid = GetID();
	gl->zoneid = zone->GetZoneID();
	gl->instance_id = zone->GetInstanceID();
	strcpy(gl->member_name, oldmember->GetName());
	gl->checkleader = checkleader;
	worldserver.SendPacket(pack);
	safe_delete(pack);

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gu = (GroupJoin_Struct*)outapp->pBuffer;
	gu->action = groupActLeave;
	strcpy(gu->membername, oldmember->GetCleanName());
	strcpy(gu->yourname, oldmember->GetCleanName());

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] == nullptr) {
			//if (DEBUG>=5) Log.Out(Logs::Detail, Logs::Group, "Group::DelMember() null member at slot %i", i);
			continue;
		}
		if (members[i] != oldmember) {
			strcpy(gu->yourname, members[i]->GetCleanName());
			if(members[i]->IsClient())
				members[i]->CastToClient()->QueuePacket(outapp);
		}
	}

	if (!ignoresender)
	{
		strcpy(gu->yourname,oldmember->GetCleanName());
		strcpy(gu->membername,oldmember->GetCleanName());
		gu->action = groupActLeave;

		if(oldmember->IsClient())
			oldmember->CastToClient()->QueuePacket(outapp);
	}

	if(oldmember->IsClient())
	{
		database.SetGroupID(oldmember->GetCleanName(), 0, oldmember->CastToClient()->CharacterID());
	}
	
	oldmember->SetGrouped(false);
	disbandcheck = true;

	safe_delete(outapp);

	return true;
}

// does the caster + group
void Group::CastGroupSpell(Mob* caster, uint16 spell_id) {
	uint32 z;
	float range, distance;

	if(!caster)
		return;

	castspell = true;
	range = caster->GetAOERange(spell_id);

	float range2 = range*range;
	float min_range2 = spells[spell_id].min_range * spells[spell_id].min_range;

//	caster->SpellOnTarget(spell_id, caster);

	for(z=0; z < MAX_GROUP_MEMBERS; z++)
	{
		if(members[z] == caster) {
			caster->SpellOnTarget(spell_id, caster);
#ifdef GROUP_BUFF_PETS
			if(spells[spell_id].targettype != ST_GroupNoPets && caster->GetPet() && caster->HasPetAffinity() && !caster->GetPet()->IsCharmed())
				caster->SpellOnTarget(spell_id, caster->GetPet());
#endif
		}
		else if(members[z] != nullptr)
		{
			distance = DistanceSquared(caster->GetPosition(), members[z]->GetPosition());
			if(distance <= range2 && distance >= min_range2) {
				members[z]->CalcSpellPowerDistanceMod(spell_id, distance);
				caster->SpellOnTarget(spell_id, members[z]);
#ifdef GROUP_BUFF_PETS
				if(spells[spell_id].targettype != ST_GroupNoPets && members[z]->GetPet() && members[z]->HasPetAffinity() && !members[z]->GetPet()->IsCharmed())
					caster->SpellOnTarget(spell_id, members[z]->GetPet());
#endif
			} else
				Log.Out(Logs::Detail, Logs::Spells, "Group spell: %s is out of range %f at distance %f from %s", members[z]->GetName(), range, distance, caster->GetName());
		}
	}

	castspell = false;
	disbandcheck = true;
}

// does the caster + group
void Group::GroupBardPulse(Mob* caster, uint16 spell_id) {
	uint32 z;
	float range, distance;

	if(!caster)
		return;

	castspell = true;
	range = caster->GetAOERange(spell_id);

	float range2 = range*range;

	for(z=0; z < MAX_GROUP_MEMBERS; z++) {
		if(members[z] == caster) {
			caster->BardPulse(spell_id, caster);
#ifdef GROUP_BUFF_PETS
			if(caster->GetPet() && caster->HasPetAffinity() && !caster->GetPet()->IsCharmed())
				caster->BardPulse(spell_id, caster->GetPet());
#endif
		}
		else if(members[z] != nullptr)
		{
			distance = DistanceSquared(caster->GetPosition(), members[z]->GetPosition());
			if(distance <= range2) 
			{
				members[z]->BardPulse(spell_id, caster);
#ifdef GROUP_BUFF_PETS

				if(members[z]->GetPet() && members[z]->HasPetAffinity() && !members[z]->GetPet()->IsCharmed())
					members[z]->GetPet()->BardPulse(spell_id, caster);
#endif
			} 
			else
			{
				Log.Out(Logs::Detail, Logs::Spells, "Group bard pulse: %s is out of range %f at distance %f from %s", members[z]->GetName(), range, distance, caster->GetName());
			}
		}
	}
}

bool Group::IsGroupMember(Mob* client)
{
	bool Result = false;

	if(client) {
		for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if (members[i] == client)
				Result = true;
		}
	}

	return Result;
}

bool Group::IsGroupMember(const char *Name)
{
	if(Name)
		for(uint32 i = 0; i < MAX_GROUP_MEMBERS; i++)
			if((strlen(Name) == strlen(membername[i])) && !strncmp(membername[i], Name, strlen(Name)))
				return true;

	return false;
}

void Group::GroupMessage(Mob* sender, uint8 language, uint8 lang_skill, const char* message) {
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(!members[i])
			continue;

		if (members[i]->IsClient() && members[i]->CastToClient()->GetFilter(FilterGroupChat)!=0)
			members[i]->CastToClient()->ChannelMessageSend(sender->GetName(),members[i]->GetName(),2,language,lang_skill,message);
	}

	ServerPacket* pack = new ServerPacket(ServerOP_OOZGroupMessage, sizeof(ServerGroupChannelMessage_Struct) + strlen(message) + 1);
	ServerGroupChannelMessage_Struct* gcm = (ServerGroupChannelMessage_Struct*)pack->pBuffer;
	gcm->zoneid = zone->GetZoneID();
	gcm->groupid = GetID();
	gcm->instanceid = zone->GetInstanceID();
	strcpy(gcm->from, sender->GetName());
	strcpy(gcm->message, message);
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

uint32 Group::GetTotalGroupDamage(Mob* other) {
	uint32 total = 0;

	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(!members[i])
			continue;
		if (other->CheckAggro(members[i]))
			total += other->GetHateAmount(members[i],true);
	}
	return total;
}

void Group::DisbandGroup() {
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupUpdate_Struct));

	GroupUpdate_Struct* gu = (GroupUpdate_Struct*) outapp->pBuffer;
	gu->action = groupActDisband;

	Client *Leader = nullptr;

	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] == nullptr)
		{
			continue;
		}

		if (members[i]->IsClient())
		{
			if(IsLeader(members[i]))
			{
				Leader = members[i]->CastToClient();
			}

			strcpy(gu->yourname, members[i]->GetName());
			database.SetGroupID(members[i]->GetName(), 0, members[i]->CastToClient()->CharacterID());
			members[i]->CastToClient()->QueuePacket(outapp);
		}
		
		members[i]->SetGrouped(false);
		members[i] = nullptr;
		membername[i][0] = '\0';
	}

	ServerPacket* pack = new ServerPacket(ServerOP_DisbandGroup, sizeof(ServerDisbandGroup_Struct));
	ServerDisbandGroup_Struct* dg = (ServerDisbandGroup_Struct*)pack->pBuffer;
	dg->zoneid = zone->GetZoneID();
	dg->groupid = GetID();
	dg->instance_id = zone->GetInstanceID();
	worldserver.SendPacket(pack);
	safe_delete(pack);

	entity_list.RemoveGroup(GetID());
	if(GetID() != 0)
	{
		database.ClearGroup(GetID());
	}

	safe_delete(outapp);
}

bool Group::Process() {
	if(disbandcheck && !GroupCount())
		return false;
	else if(disbandcheck && GroupCount())
		disbandcheck = false;
	return true;
}

void Group::SendUpdate(uint32 type, Mob* member)
{
	if(!member->IsClient())
		return;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupUpdate_Struct));
	GroupUpdate_Struct* gu = (GroupUpdate_Struct*)outapp->pBuffer;
	gu->action = type;
	strcpy(gu->yourname,member->GetName());

	int x = 0;

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if((members[i] != nullptr) && IsLeader(members[i]))
		{
			strcpy(gu->leadersname, members[i]->GetName());
			break;
		}

	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; ++i)
		if (members[i] != nullptr && members[i] != member)
			strcpy(gu->membername[x++], members[i]->GetName());

	member->CastToClient()->QueuePacket(outapp);

	safe_delete(outapp);
}

uint8 Group::GroupCount() {

	uint8 MemberCount = 0;

	for(uint8 i = 0; i < MAX_GROUP_MEMBERS; ++i)
	{
		if(membername[i][0])
		{
			++MemberCount;
		}
	}

	return MemberCount;
}

uint32 Group::GetHighestLevel()
{
uint32 level = 1;
uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i])
		{
			if(members[i]->GetLevel() > level)
				level = members[i]->GetLevel();
		}
	}
	return level;
}
uint32 Group::GetLowestLevel()
{
uint32 level = 255;
uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i])
		{
			if(members[i]->GetLevel() < level)
				level = members[i]->GetLevel();
		}
	}
	return level;
}

void Group::TeleportGroup(Mob* sender, uint32 zoneID, uint16 instance_id, float x, float y, float z, float heading)
{
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++)
	{
		if (members[i] != nullptr && members[i]->IsClient() && members[i] != sender)
		{
			members[i]->CastToClient()->MovePC(zoneID, instance_id, x, y, z, heading, 0, ZoneSolicited);
		}
	}
}

bool Group::LearnMembers() {
	std::string query = StringFormat("SELECT name FROM group_id WHERE groupid = %lu", (unsigned long)GetID());
	auto results = database.QueryDatabase(query);
	if (!results.Success())
        return false;

    if (results.RowCount() == 0) {
        Log.Out(Logs::General, Logs::Error, "Error getting group members for group %lu: %s", (unsigned long)GetID(), results.ErrorMessage().c_str());
			return false;
    }

	int memberIndex = 0;
    for(auto row = results.begin(); row != results.end(); ++row) {
		if(!row[0])
			continue;

		members[memberIndex] = nullptr;
		strn0cpy(membername[memberIndex], row[0], 64);

		memberIndex++;
	}

	return true;
}

void Group::VerifyGroup() {
	/*
		The purpose of this method is to make sure that a group
		is in a valid state, to prevent dangling pointers.
		Only called every once in a while (on member re-join for now).
	*/

	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (membername[i][0] == '\0') {
#if EQDEBUG >= 7
	Log.Out(Logs::General, Logs::Group, "Group %lu: Verify %d: Empty.\n", (unsigned long)GetID(), i);
#endif
			members[i] = nullptr;
			continue;
		}

		//it should be safe to use GetClientByName, but Group is trying
		//to be generic, so we'll go for general Mob
		Mob *them = entity_list.GetMob(membername[i]);
		if(them == nullptr && members[i] != nullptr) {	//they arnt here anymore....
#if EQDEBUG >= 6
		Log.Out(Logs::General, Logs::Group, "Member of group %lu named '%s' has disappeared!!", (unsigned long)GetID(), membername[i]);
#endif
			membername[i][0] = '\0';
			members[i] = nullptr;
			continue;
		}

		if(them != nullptr && members[i] != them) {	//our pointer is out of date... not so good.
#if EQDEBUG >= 5
		Log.Out(Logs::General, Logs::Group, "Member of group %lu named '%s' had an out of date pointer!!", (unsigned long)GetID(), membername[i]);
#endif
			members[i] = them;
			continue;
		}
#if EQDEBUG >= 8
		Log.Out(Logs::General, Logs::Group, "Member of group %lu named '%s' is valid.", (unsigned long)GetID(), membername[i]);
#endif
	}
}


void Group::GroupMessage_StringID(Mob* sender, uint32 type, uint32 string_id, const char* message,const char* message2,const char* message3,const char* message4,const char* message5,const char* message6,const char* message7,const char* message8,const char* message9, uint32 distance) {
	uint32 i;
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if(members[i] == nullptr)
			continue;

		if(members[i] == sender)
			continue;

		members[i]->Message_StringID(type, string_id, message, message2, message3, message4, message5, message6, message7, message8, message9, 0);
	}
}



void Client::LeaveGroup() {
	Group *g = GetGroup();

	if(g)
	{
		int32 MemberCount = g->GroupCount();
		if(MemberCount <= 2)
		{
			g->DisbandGroup();
		}
		else
		{
			g->DelMember(this);
		}
	}
	else
	{
		//force things a little
		database.SetGroupID(GetName(), 0, CharacterID());

	}

	isgrouped = false;
}

void Group::HealGroup(uint32 heal_amt, Mob* caster, float range)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	float distance;
	float range2 = range*range;


	int numMem = 0;
	unsigned int gi = 0;
	for(; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				numMem += 1;
			}
		}
	}

	heal_amt /= numMem;
	for(gi = 0; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				members[gi]->HealDamage(heal_amt, caster);
				members[gi]->SendHPUpdate();
			}
		}
	}
}


void Group::BalanceHP(int32 penalty, float range, Mob* caster, int32 limit)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	int dmgtaken = 0, numMem = 0, dmgtaken_tmp = 0;

	float distance;
	float range2 = range*range;

	unsigned int gi = 0;
	for(; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){

				dmgtaken_tmp = members[gi]->GetMaxHP() - members[gi]->GetHP();
				if (limit && (dmgtaken_tmp > limit))
					dmgtaken_tmp = limit;

				dmgtaken += (dmgtaken_tmp);
				numMem += 1;
			}
		}
	}

	dmgtaken += dmgtaken * penalty / 100;
	dmgtaken /= numMem;
	for(gi = 0; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				if((members[gi]->GetMaxHP() - dmgtaken) < 1){ //this way the ability will never kill someone
					members[gi]->SetHP(1);					//but it will come darn close
					members[gi]->SendHPUpdate();
				}
				else{
					members[gi]->SetHP(members[gi]->GetMaxHP() - dmgtaken);
					members[gi]->SendHPUpdate();
				}
			}
		}
	}
}

void Group::BalanceMana(int32 penalty, float range, Mob* caster, int32 limit)
{
	if (!caster)
		return;

	if (!range)
		range = 200;

	float distance;
	float range2 = range*range;

	int manataken = 0, numMem = 0, manataken_tmp = 0;
	unsigned int gi = 0;
	for(; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi] && (members[gi]->GetMaxMana() > 0)){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){

				manataken_tmp = members[gi]->GetMaxMana() - members[gi]->GetMana();
				if (limit && (manataken_tmp > limit))
					manataken_tmp = limit;

				manataken += (manataken_tmp);
				numMem += 1;
			}
		}
	}

	manataken += manataken * penalty / 100;
	manataken /= numMem;

	if (limit && (manataken > limit))
		manataken = limit;

	for(gi = 0; gi < MAX_GROUP_MEMBERS; gi++)
	{
		if(members[gi]){
			distance = DistanceSquared(caster->GetPosition(), members[gi]->GetPosition());
			if(distance <= range2){
				if((members[gi]->GetMaxMana() - manataken) < 1){
					members[gi]->SetMana(1);
					if (members[gi]->IsClient())
						members[gi]->CastToClient()->SendManaUpdate();
				}
				else{
					members[gi]->SetMana(members[gi]->GetMaxMana() - manataken);
					if (members[gi]->IsClient())
						members[gi]->CastToClient()->SendManaUpdate();
				}
			}
		}
	}
}

uint16 Group::GetAvgLevel()
{
	double levelHolder = 0;
	uint8 i = 0;
	uint8 numMem = 0;
	while(i < MAX_GROUP_MEMBERS)
	{
		if (members[i])
		{
			numMem++;
			levelHolder = levelHolder + (members[i]->GetLevel());
		}
		i++;
	}
	levelHolder = ((levelHolder/numMem)+.5); // total levels divided by num of characters
	return (uint16(levelHolder));
}

int8 Group::GetNumberNeedingHealedInGroup(int8 hpr, bool includePets) {
	int8 needHealed = 0;

	for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
		if(members[i] && !members[i]->qglobal) {

			if(members[i]->GetHPRatio() <= hpr)
				needHealed++;

			if(includePets) {
				if(members[i]->GetPet() && members[i]->GetPet()->GetHPRatio() <= hpr) {
					needHealed++;
				}
			}
		}
	}


	return needHealed;
}

void Group::ChangeLeader(Mob* newleader)
{
	// this changes the current group leader, notifies other members, and updates leadship AA

	// if the new leader is invalid, do nothing
	if (!newleader)
		return;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_GroupUpdate,sizeof(GroupJoin_Struct));
	GroupJoin_Struct* gu = (GroupJoin_Struct*) outapp->pBuffer;
	gu->action = groupActMakeLeader;

	strcpy(gu->membername, newleader->GetName());
	strcpy(gu->yourname, GetOldLeaderName());
	SetLeader(newleader);
	database.SetGroupLeaderName(GetID(), newleader->GetName());
	for (uint32 i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] && members[i]->IsClient())
		{
			members[i]->CastToClient()->QueuePacket(outapp);
			Log.Out(Logs::Detail, Logs::Group, "ChangeLeader(): Local leader update packet sent to: %s .", members[i]->GetName());
		}
	}
	safe_delete(outapp);

	Log.Out(Logs::Detail, Logs::Group, "ChangeLeader(): Old Leader is: %s New leader is: %s", GetOldLeaderName(), newleader->GetName());

	ServerPacket* pack = new ServerPacket(ServerOP_ChangeGroupLeader, sizeof(ServerGroupLeader_Struct));
	ServerGroupLeader_Struct* fgu = (ServerGroupLeader_Struct*)pack->pBuffer;
	fgu->zoneid = zone->GetZoneID();
	fgu->gid = GetID();
	strcpy(fgu->leader_name, newleader->GetName());
	strcpy(fgu->oldleader_name, GetOldLeaderName());
	worldserver.SendPacket(pack);
	//safe_delete(pack);

	SetOldLeaderName(newleader->GetName());
}

const char *Group::GetClientNameByIndex(uint8 index)
{
	return membername[index];
}

