/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2005 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/packet_dump_file.h"
#include "../common/rulesys.h"
#include "../common/string_util.h"

#include "queryserv.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"
#include "zone.h"

extern QueryServ* QServ;
extern WorldServer worldserver;
extern Zone* zone;


void Client::Handle_OP_ZoneChange(const EQApplicationPacket *app) {
	zoning = true;
	if (app->size != sizeof(ZoneChange_Struct)) {
		Log.Out(Logs::General, Logs::None, "Wrong size: OP_ZoneChange, size=%d, expected %d", app->size, sizeof(ZoneChange_Struct));
		return;
	}

#if EQDEBUG >= 5
	Log.Out(Logs::General, Logs::None, "Zone request from %s", GetName());
	DumpPacket(app);
#endif
	ZoneChange_Struct* zc=(ZoneChange_Struct*)app->pBuffer;

	uint16 target_zone_id = 0;
	uint16 target_instance_id = 0;
	ZonePoint* zone_point = nullptr;
	//figure out where they are going.
	//we should never trust the client's logic, however, the client's information coupled with the server's information can help us determine locations they should be going to.
	//try to figure it out for them.

	if(zc->zoneID == 0) {
		//client dosent know where they are going...
		//try to figure it out for them.

		switch(zone_mode) {
		case EvacToSafeCoords:
		case ZoneToSafeCoords:
			//going to safe coords, but client dosent know where?
			//assume it is this zone for now.
			target_zone_id = zone->GetZoneID();
			break;
		case GMSummon:
			target_zone_id = zonesummon_id;
			break;
		case GateToBindPoint:
			target_zone_id = m_pp.binds[0].zoneId;
			target_instance_id = m_pp.binds[0].instance_id;
			break;
		case ZoneToBindPoint:
			target_zone_id = m_pp.binds[0].zoneId;
			target_instance_id = m_pp.binds[0].instance_id;
			break;
		case ZoneSolicited: //we told the client to zone somewhere, so we know where they are going.
			target_zone_id = zonesummon_id;
			break;
		case ZoneUnsolicited: //client came up with this on its own.
			zone_point = zone->GetClosestZonePointWithoutZone(GetX(), GetY(), GetZ(), this, ZONEPOINT_NOZONE_RANGE);
			if(zone_point) {
				//we found a zone point, which is a reasonable distance away
				//assume that is the one were going with.
				target_zone_id = zone_point->target_zone_id;
				target_instance_id = zone_point->target_zone_instance;
			} else {
				//unable to find a zone point... is there anything else
				//that can be a valid un-zolicited zone request?

				Message(CC_Red, "Invalid unsolicited zone request.");
				Log.Out(Logs::General, Logs::Error, "Zoning %s: Invalid unsolicited zone request to zone id '%d'.", GetName(), target_zone_id);
				SendZoneCancel(zc);
				return;
			}
			break;
		default:
			break;
		};
	}
	else {
		// This is to allow both 6.2 and Titanium clients to perform a proper zoning of the client when evac/succor
		if(zone_mode == EvacToSafeCoords && zonesummon_id > 0)
			target_zone_id = zonesummon_id;
		else
			target_zone_id = zc->zoneID;

		//if we are zoning to a specific zone unsolicied,
		//then until otherwise determined, they must be zoning
		//on a zone line.
		if(zone_mode == ZoneUnsolicited)
		{

			zone_point = zone->GetClosestZonePoint(glm::vec3(GetPosition()), target_zone_id, this, ZONEPOINT_ZONE_RANGE);
			//if we didnt get a zone point, or its to a different zone,
			//then we assume this is invalid.
			if(!zone_point || zone_point->target_zone_id != target_zone_id) {
				Log.Out(Logs::General, Logs::Error, "Zoning %s: Invalid unsolicited zone request to zone id '%d'.", GetName(), target_zone_id);
				SendZoneCancel(zc);
				return;
			}
		}
	}

	if(target_instance_id > 0)
	{
		//make sure we are in it and it's unexpired.
		if(!database.VerifyInstanceAlive(target_instance_id, CharacterID()))
		{
			Message(CC_Red, "Instance ID was expired or you were not in it.");
			SendZoneCancel(zc);
			return;
		}

		if(!database.VerifyZoneInstance(target_zone_id, target_instance_id))
		{
			Message(CC_Red, "Instance ID was %u does not go with zone id %u", target_instance_id, target_zone_id);
			SendZoneCancel(zc);
			return;
		}
	}

	/* Check for Valid Zone */
	const char *target_zone_name = database.GetZoneName(target_zone_id);
	if(target_zone_name == nullptr) {
		//invalid zone...
		Message(CC_Red, "Invalid target zone ID.");
		Log.Out(Logs::General, Logs::Error, "Zoning %s: Unable to get zone name for zone id '%d'.", GetName(), target_zone_id);
		SendZoneCancel(zc);
		return;
	}

	/* Load up the Safe Coordinates, restrictions and verify the zone name*/
	float safe_x, safe_y, safe_z;
	int16 minstatus = 0;
	uint8 minlevel = 0, expansion = 0;
	char flag_needed[128];
	if(!database.GetSafePoints(target_zone_name, database.GetInstanceVersion(target_instance_id), &safe_x, &safe_y, &safe_z, &minstatus, &minlevel, flag_needed, &expansion)) {
		//invalid zone...
		Message(CC_Red, "Invalid target zone while getting safe points.");
		Log.Out(Logs::General, Logs::Error, "Zoning %s: Unable to get safe coordinates for zone '%s'.", GetName(), target_zone_name);
		SendZoneCancel(zc);
		return;
	}

	char buf[10];
	snprintf(buf, 9, "%d", target_zone_id);
	buf[9] = '\0';
	parse->EventPlayer(EVENT_ZONE, this, buf, 0);

	//handle circumvention of zone restrictions
	//we need the value when creating the outgoing packet as well.
	uint8 ignorerestrictions = zonesummon_ignorerestrictions;
	zonesummon_ignorerestrictions = 0;

	float dest_x=0, dest_y=0, dest_z=0, dest_h;
	dest_h = GetHeading();
	switch(zone_mode) {
	case EvacToSafeCoords:
	case ZoneToSafeCoords:
		Log.Out(Logs::General, Logs::None, "Zoning %s to safe coords (%f,%f,%f) in %s (%d)", GetName(), safe_x, safe_y, safe_z, target_zone_name, target_zone_id);
		dest_x = safe_x;
		dest_y = safe_y;
		dest_z = safe_z;
		break;
	case GMSummon:
		dest_x = m_ZoneSummonLocation.x;
		dest_y = m_ZoneSummonLocation.y;
		dest_z = m_ZoneSummonLocation.z;
		ignorerestrictions = 1;
		break;
	case GateToBindPoint:
		dest_x = m_pp.binds[0].x;
		dest_y = m_pp.binds[0].y;
		dest_z = m_pp.binds[0].z;
		dest_h = m_pp.binds[0].heading;
		break;
	case ZoneToBindPoint:
		dest_x = m_pp.binds[0].x;
		dest_y = m_pp.binds[0].y;
		dest_z = m_pp.binds[0].z;
		dest_h = m_pp.binds[0].heading;
		ignorerestrictions = 1;	//can always get to our bind point? seems exploitable
		break;
	case ZoneSolicited: //we told the client to zone somewhere, so we know where they are going.
		//recycle zonesummon variables
		dest_x = m_ZoneSummonLocation.x;
		dest_y = m_ZoneSummonLocation.y;
		dest_z = m_ZoneSummonLocation.z;
		break;
	case ZoneUnsolicited: //client came up with this on its own.
		//client requested a zoning... what are the cases when this could happen?

		//Handle zone point case:
		if(zone_point != nullptr) {
			//they are zoning using a valid zone point, figure out coords

			//999999 is a placeholder for 'same as where they were from'
			if(zone_point->target_x == 999999)
				dest_x = GetX();
			else
				dest_x = zone_point->target_x;
			if(zone_point->target_y == 999999)
				dest_y = GetY();
			else
				dest_y = zone_point->target_y;
			if(zone_point->target_z == 999999)
				dest_z=GetZ();
			else
				dest_z = zone_point->target_z;
			if(zone_point->target_heading == 999)
				dest_h = GetHeading();
			else
				dest_h = zone_point->target_heading;

			break;
		}

		//for now, there are no other cases...

		//could not find a valid reason for them to be zoning, stop it.
		CheatDetected(MQZoneUnknownDest, 0.0, 0.0, 0.0);
		Log.Out(Logs::General, Logs::Error, "Zoning %s: Invalid unsolicited zone request to zone id '%s'. Not near a zone point.", GetName(), target_zone_name);
		SendZoneCancel(zc);
		return;
	default:
		break;
	};

	//OK, now we should know where were going...

	//Check some rules first.
	int8 myerror = 1;		//1 is succes

	//not sure when we would use ZONE_ERROR_NOTREADY
	
	bool has_expansion = expansion & m_pp.expansions;
	if(!ignorerestrictions && Admin() < minStatusToIgnoreZoneFlags && expansion > ClassicEQ && !has_expansion)
	{
		myerror = ZONE_ERROR_NOEXPANSION;
		Log.Out(Logs::General, Logs::Error, "Zoning %s: Does not have the required expansion (%d) to enter %s. Player expansions: %d", GetName(), expansion, target_zone_name, m_pp.expansions);
	}

	//enforce min status and level
	if (!ignorerestrictions && (Admin() < minstatus || GetLevel() < minlevel))
	{
		myerror = ZONE_ERROR_NOEXPERIENCE;
	}

	if(!ignorerestrictions && flag_needed[0] != '\0') {
		//the flag needed string is not empty, meaning a flag is required.
		if(Admin() < minStatusToIgnoreZoneFlags && !HasZoneFlag(target_zone_id))
		{
			Message(CC_Red, "You do not have the flag to enter %s.", target_zone_name);
			myerror = ZONE_ERROR_NOEXPERIENCE;
		}
	}

	if(myerror == 1) 
	{
		//we have successfully zoned
		DoZoneSuccess(zc, target_zone_id, target_instance_id, dest_x, dest_y, dest_z, dest_h, ignorerestrictions);
		UpdateZoneChangeCount(target_zone_id);
	} 
	else 
	{
		Log.Out(Logs::General, Logs::Error, "Zoning %s: Rules prevent this char from zoning into '%s'", GetName(), target_zone_name);
		SendZoneError(zc, myerror);
	}
}

void Client::SendZoneCancel(ZoneChange_Struct *zc) {
	//effectively zone them right back to where they were
	//unless we find a better way to stop the zoning process.
	SetPortExemption(true);
	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_ZoneChange, sizeof(ZoneChange_Struct));
	ZoneChange_Struct *zc2 = (ZoneChange_Struct*)outapp->pBuffer;
	strcpy(zc2->char_name, zc->char_name);
	zc2->zoneID = zone->GetZoneID();
	zc2->success = ZONE_ERROR_NOTREADY;
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	//reset to unsolicited.
	zone_mode = ZoneUnsolicited;
}

void Client::SendZoneError(ZoneChange_Struct *zc, int8 err)
{
	Log.Out(Logs::General, Logs::Error, "Zone %i is not available because target wasn't found or character insufficent level", zc->zoneID);

	SetPortExemption(true);

	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_ZoneChange, sizeof(ZoneChange_Struct));
	ZoneChange_Struct *zc2 = (ZoneChange_Struct*)outapp->pBuffer;
	strcpy(zc2->char_name, zc->char_name);
	zc2->zoneID = zc->zoneID;
	zc2->success = err;
	outapp->priority = 6;
	FastQueuePacket(&outapp);

	//reset to unsolicited.
	zone_mode = ZoneUnsolicited;
}

void Client::DoZoneSuccess(ZoneChange_Struct *zc, uint16 zone_id, uint32 instance_id, float dest_x, float dest_y, float dest_z, float dest_h, int8 ignore_r) {
	//this is called once the client is fully allowed to zone here
	//it takes care of all the activities which occur when a client zones out

	SendLogoutPackets();

	/* QS: PlayerLogZone */
	if (RuleB(QueryServ, PlayerLogZone)){
		std::string event_desc = StringFormat("Zoning :: zoneid:%u instid:%u x:%4.2f y:%4.2f z:%4.2f h:%4.2f zonemode:%d from zoneid:%u instid:%i", zone_id, instance_id, dest_x, dest_y, dest_z, dest_h, zone_mode, this->GetZoneID(), this->GetInstanceID());
		QServ->PlayerLogEvent(Player_Log_Zoning, this->CharacterID(), event_desc);
	}

	/* Dont clear aggro until the zone is successful */
	entity_list.RemoveFromHateLists(this);
	 
	if(this->GetPet())
		entity_list.RemoveFromHateLists(this->GetPet());

	Log.Out(Logs::General, Logs::Status, "Zoning '%s' to: %s (%i) - (%i) x=%f, y=%f, z=%f", m_pp.name, database.GetZoneName(zone_id), zone_id, instance_id, dest_x, dest_y, dest_z);

	//set the player's coordinates in the new zone so they have them
	//when they zone into it
	m_Position.x = dest_x; //these coordinates will now be saved when ~client is called
	m_Position.y = dest_y;
	m_Position.z = dest_z;
	m_Position.w = dest_h / 2.0f; // fix for zone heading
	m_pp.heading = dest_h;
	m_pp.zone_id = zone_id;
	m_pp.zoneInstance = instance_id;

	//Force a save so its waiting for them when they zone
	Save(2);

	// vesuvias - zoneing to another zone so we need to the let the world server
	//handle things with the client for a while
	SetZoningState();
	ServerPacket* pack = new ServerPacket(ServerOP_ZoneToZoneRequest, sizeof(ZoneToZone_Struct));
	ZoneToZone_Struct* ztz = (ZoneToZone_Struct*) pack->pBuffer;
	ztz->response = 0;
	ztz->current_zone_id = zone->GetZoneID();
	ztz->current_instance_id = zone->GetInstanceID();
	ztz->requested_zone_id = zone_id;
	ztz->requested_instance_id = instance_id;
	ztz->admin = admin;
	ztz->ignorerestrictions = ignore_r;
	strcpy(ztz->name, GetName());
	ztz->guild_id = GuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);

	//reset to unsolicited.
	zone_mode = ZoneUnsolicited;
	m_ZoneSummonLocation = glm::vec3();
	zonesummon_id = 0;
	zonesummon_ignorerestrictions = 0;
}

void Client::MovePC(const char* zonename, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm) {
	ProcessMovePC(database.GetZoneID(zonename), 0, x, y, z, heading, ignorerestrictions, zm);
}

//designed for in zone moving
void Client::MovePC(float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm) {
	ProcessMovePC(zone->GetZoneID(), 0, x, y, z, heading, ignorerestrictions, zm);
}

void Client::MovePC(uint32 zoneID, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm) {
	ProcessMovePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
}

void Client::MovePC(uint32 zoneID, uint32 instanceID, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm){
	ProcessMovePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
}


void Client::ProcessMovePC(uint32 zoneID, uint32 instance_id, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm)
{
	// From what I have read, dragged corpses should stay with the player for Intra-zone summons etc, but we can implement that later.
	ClearDraggedCorpses();

	if(zoneID == 0)
		zoneID = zone->GetZoneID();

	if(zoneID == zone->GetZoneID()) {
		// TODO: Determine if this condition is necessary.
		if(IsAIControlled()) {
			GMMove(x, y, z);
			return;
		}

		if(GetPetID() != 0) {
			//if they have a pet and they are staying in zone, move with them
			Mob *p = GetPet();
			if(p != nullptr){
				p->SetPetOrder(SPO_Follow);
				p->GMMove(x+15, y, z);	//so it dosent have to run across the map.
			}
		}
	}

	switch(zm) {
		case GateToBindPoint:
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		case EvacToSafeCoords:
		case ZoneToSafeCoords:
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		case GMSummon:
			Message(CC_Yellow, "You have been summoned by a GM!");
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		case ZoneToBindPoint:
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		case ZoneSolicited:
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		case SummonPC:
			if(!GetGM())
				Message(CC_Yellow, "You have been summoned!");
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		case Rewind:
			Message(CC_Yellow, "Rewinding to previous location.");
			ZonePC(zoneID, 0, x, y, z, heading, ignorerestrictions, zm);
			break;
		default:
			Log.Out(Logs::General, Logs::Error, "Client::ProcessMovePC received a reguest to perform an unsupported client zone operation.");
			break;
	}
}

void Client::ZonePC(uint32 zoneID, uint32 instance_id, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm) {

	bool ReadyToZone = true;
	int iZoneNameLength = 0;
	const char*	pShortZoneName = nullptr;
	char* pZoneName = nullptr;

	pShortZoneName = database.GetZoneName(zoneID);
	database.GetZoneLongName(pShortZoneName, &pZoneName);

	SetPortExemption(true);

	if(!pZoneName) {
		Message(CC_Red, "Invalid zone number specified");
		safe_delete_array(pZoneName);
		return;
	}
	iZoneNameLength = strlen(pZoneName);
	glm::vec3 safePoint;

	switch(zm) {
		case EvacToSafeCoords:
		case ZoneToSafeCoords:
			safePoint = zone->GetSafePoint();
			x = safePoint.x;
			y = safePoint.y;
			z = safePoint.z;
			SetHeading(heading);
			break;
		case GMSummon:
			m_Position = glm::vec4(x, y, z, heading);
			m_ZoneSummonLocation = glm::vec3(m_Position);
			SetHeading(heading);

			zonesummon_id = zoneID;
			zonesummon_ignorerestrictions = 1;
			break;
		case ZoneSolicited:
			m_ZoneSummonLocation = glm::vec3(x,y,z);
			SetHeading(heading);

			zonesummon_id = zoneID;
			zonesummon_ignorerestrictions = ignorerestrictions;
			break;
		case GateToBindPoint:
			x = m_Position.x = m_pp.binds[0].x;
			y = m_Position.y = m_pp.binds[0].y;
			z = m_Position.z = m_pp.binds[0].z;
			heading = m_pp.binds[0].heading;
			m_Position.w = heading / 2.0f;
			break;
		case ZoneToBindPoint:
			x = m_Position.x = m_pp.binds[0].x;
			y = m_Position.y = m_pp.binds[0].y;
			z = m_Position.z = m_pp.binds[0].z;
			heading = m_pp.binds[0].heading;
			m_Position.w = heading / 2.0f;

			zonesummon_ignorerestrictions = 1;
			Log.Out(Logs::General, Logs::None, "Player %s has died and will be zoned to bind point in zone: %s at LOC x=%f, y=%f, z=%f, heading=%f", GetName(), pZoneName, m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, m_pp.binds[0].heading);
			break;
		case SummonPC:
			m_ZoneSummonLocation = glm::vec3(x, y, z);
			m_Position = glm::vec4(m_ZoneSummonLocation, 0.0f);
			SetHeading(heading);
			break;
		case Rewind:
			Log.Out(Logs::General, Logs::None, "%s has requested a /rewind from %f, %f, %f, to %f, %f, %f in %s", GetName(), m_Position.x, m_Position.y, m_Position.z, m_RewindLocation.x, m_RewindLocation.y, m_RewindLocation.z, zone->GetShortName());
			m_ZoneSummonLocation = glm::vec3(x, y, z);
			m_Position = glm::vec4(m_ZoneSummonLocation, 0.0f);
			SetHeading(heading);
			break;
		default:
			Log.Out(Logs::General, Logs::Error, "Client::ZonePC() received a reguest to perform an unsupported client zone operation.");
			ReadyToZone = false;
			break;
	}

	if (ReadyToZone)
	{
		//if client is looting, we need to send an end loot
		if (IsLooting())
		{
			Entity* entity = entity_list.GetID(entity_id_being_looted);
			if (entity == 0)
			{
				Message(CC_Red, "Error: OP_EndLootRequest: Corpse not found (ent = 0)");
				Corpse::SendLootReqErrorPacket(this);
			}
			else if (!entity->IsCorpse())
			{
				Message(CC_Red, "Error: OP_EndLootRequest: Corpse not found (!entity->IsCorpse())");
				Corpse::SendLootReqErrorPacket(this);
			}
			else
			{
				Corpse::SendEndLootErrorPacket(this);
				entity->CastToCorpse()->EndLoot(this, nullptr);
			}
			SetLooting(0);
		}
		if(Trader)
		{
			Trader_EndTrader();
		}

		zone_mode = zm;

		if(zm == ZoneSolicited || zm == ZoneToSafeCoords) {
			Log.Out(Logs::Detail, Logs::EQMac, "Zoning packet about to be sent (ZS/ZTS). We are headed to zone: %i, at %f, %f, %f", zoneID, x, y, z);
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_RequestClientZoneChange, sizeof(RequestClientZoneChange_Struct));
			RequestClientZoneChange_Struct* gmg = (RequestClientZoneChange_Struct*) outapp->pBuffer;

			gmg->zone_id = zoneID;

			gmg->x = x;
			gmg->y = y;
			gmg->z = z;
			gmg->heading = heading;
			gmg->instance_id = instance_id;
			gmg->type = 0x01;				//an observed value, not sure of meaning

			outapp->priority = 6;
			FastQueuePacket(&outapp);	
		}
		else if (zm == ZoneToBindPoint) {
			//TODO: Find a better packet that works with EQMac on death. Sending OP_RequestClientZoneChange here usually does not zone the
			//player correctly (it starts the zoning process, then disconnect.) OP_GMGoto seems to work 90% of the time. It's a hack, but it works...
			Log.Out(Logs::Detail, Logs::EQMac, "Zoning packet about to be sent (ZTB). We are headed to zone: %i, at %f, %f, %f", zoneID, x, y, z);
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_GMGoto, sizeof(GMGoto_Struct));
			GMGoto_Struct* gmg = (GMGoto_Struct*) outapp->pBuffer;
	
			strcpy(gmg->charname,this->name);
			strcpy(gmg->gmname,this->name);
			gmg->zoneID = zoneID;
			gmg->x = x;
			gmg->y = y;
			gmg->z = z;
			outapp->priority = 6;
			FastQueuePacket(&outapp);
		}
		else if (zm == GateToBindPoint) {			
			// we hide the real zoneid we want to evac/succor to here
			zonesummon_id = zoneID;
			zone_mode = zm;

			if(zoneID == GetZoneID()) {
				Log.Out(Logs::Detail, Logs::EQMac, "Zoning packet NOT being sent (GTB). We are headed to zone: %i, at %f, %f, %f", zoneID, x, y, z);
				//Not doing inter-zone for same zone gates. Client is supposed to handle these, based on PP info it is fed.
				//properly handle proximities
				entity_list.ProcessMove(this, glm::vec3(m_Position));
				m_Proximity = glm::vec3(m_Position);
				SendPosition();
			}
			else
			{
				zone_mode = zm;
				Log.Out(Logs::Detail, Logs::EQMac, "Zoning packet about to be sent (GTB). We are headed to zone: %i, at %f, %f, %f", zoneID, x, y, z);
				EQApplicationPacket* outapp = new EQApplicationPacket(OP_RequestClientZoneChange, sizeof(RequestClientZoneChange_Struct));
				RequestClientZoneChange_Struct* gmg = (RequestClientZoneChange_Struct*) outapp->pBuffer;

				gmg->zone_id = zoneID;
				gmg->x = x;
				gmg->y = y;
				gmg->z = z;
				gmg->heading = heading;
				gmg->instance_id = instance_id;
				gmg->type = 0x01;	//an observed value, not sure of meaning
				outapp->priority = 6;
				FastQueuePacket(&outapp);
			}
		}
		else if(zm == EvacToSafeCoords)
		{
			zone_mode = zm;
			Log.Out(Logs::Detail, Logs::EQMac, "Zoning packet about to be sent (ETSC). We are headed to zone: %i, at %f, %f, %f", zoneID, x, y, z);
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_RequestClientZoneChange, sizeof(RequestClientZoneChange_Struct));
			RequestClientZoneChange_Struct* gmg = (RequestClientZoneChange_Struct*) outapp->pBuffer;

			if(this->GetZoneID() == qeynos)
				gmg->zone_id = qeynos2;
			else if(this->GetZoneID() == qeynos2)
				gmg->zone_id = qeynos;
			else
				gmg->zone_id = qeynos;

			gmg->x = x;
			gmg->y = y;
			gmg->z = z;
			gmg->heading = heading;
			gmg->instance_id = instance_id;
			gmg->type = 0x01;				// '0x01' was an observed value for the type field, not sure of meaning

			// we hide the real zoneid we want to evac/succor to here
			zonesummon_id = zoneID;

			outapp->priority = 6;
			FastQueuePacket(&outapp);
		}
		else {
			if(zoneID == GetZoneID()) {
				Log.Out(Logs::Detail, Logs::EQMac, "Zoning packet about to be sent (GOTO). We are headed to zone: %i, at %f, %f, %f", zoneID, x, y, z);
				//properly handle proximities
				entity_list.ProcessMove(this, glm::vec3(m_Position));
				m_Proximity = glm::vec3(m_Position);

				//send out updates to people in zone.
				SendPosition();
			}

			EQApplicationPacket* outapp = new EQApplicationPacket(OP_RequestClientZoneChange, sizeof(RequestClientZoneChange_Struct));
			RequestClientZoneChange_Struct* gmg = (RequestClientZoneChange_Struct*) outapp->pBuffer;

			gmg->zone_id = zoneID;
			gmg->x = x;
			gmg->y = y;
			gmg->z = z;
			gmg->heading = heading;
			gmg->instance_id = instance_id;
			gmg->type = 0x01;	//an observed value, not sure of meaning
			outapp->priority = 6;
			FastQueuePacket(&outapp);
		}

		Log.Out(Logs::Detail, Logs::EQMac, "Player %s has requested a zoning to LOC x=%f, y=%f, z=%f, heading=%f in zoneid=%i and type=%i", GetName(), x, y, z, heading, zoneID, zm);
		//Clear zonesummon variables if we're zoning to our own zone
		//Client wont generate a zone change packet to the server in this case so
		//They aren't needed and it keeps behavior on next zone attempt from being undefined.
		if(zoneID == zone->GetZoneID() && instance_id == zone->GetInstanceID())
		{
			if(zm != EvacToSafeCoords && zm != ZoneToSafeCoords && zm != ZoneToBindPoint)
			{
				m_ZoneSummonLocation = glm::vec3();
				zonesummon_id = 0;
				zonesummon_ignorerestrictions = 0;
				zone_mode = ZoneUnsolicited;
			}
		}
	}

	safe_delete_array(pZoneName);
}


void Client::GoToSafeCoords(uint16 zone_id, uint16 instance_id) {
	if(zone_id == 0)
		zone_id = zone->GetZoneID();

	MovePC(zone_id, instance_id, 0.0f, 0.0f, 0.0f, 0.0f, 0, ZoneToSafeCoords);
}


void Mob::Gate() {
	GoToBind();
}

void Client::Gate() 
{
	Mob::Gate();
}

void NPC::Gate() {
	entity_list.MessageClose_StringID(this, true, 200, MT_Spells, GATES, GetCleanName());
	
	if (GetHPRatio() < 25)
	{
		SetHP(GetMaxHP() / 4);
	}

	Mob::Gate();
}

void Client::SetBindPoint(int to_zone, int to_instance, const glm::vec3& location) {
	if (to_zone == -1) {
		m_pp.binds[0].zoneId = zone->GetZoneID();
		m_pp.binds[0].instance_id = (zone->GetInstanceID() != 0 && zone->IsInstancePersistent()) ? zone->GetInstanceID() : 0;
		m_pp.binds[0].x = m_Position.x;
		m_pp.binds[0].y = m_Position.y;
		m_pp.binds[0].z = m_Position.z;
		m_pp.binds[0].heading = m_Position.w * 2.0f;
	}
	else {
		m_pp.binds[0].zoneId = to_zone;
		m_pp.binds[0].instance_id = to_instance;
		m_pp.binds[0].x = location.x;
		m_pp.binds[0].y = location.y;
		m_pp.binds[0].z = location.z;
		m_pp.binds[0].heading = m_Position.w * 2.0f;
	}
	auto regularBindPoint = glm::vec4(m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, m_pp.binds[0].heading);
	database.SaveCharacterBindPoint(this->CharacterID(), m_pp.binds[0].zoneId, m_pp.binds[0].instance_id, regularBindPoint, 0);
}

void Client::GoToBind(uint8 bindnum) {
	// if the bind number is invalid, use the primary bind
	if(bindnum > 4)
		bindnum = 0;

	// move the client, which will zone them if needed.
	// ignore restrictions on the zone request..?
	if(bindnum == 0)
		MovePC(m_pp.binds[0].zoneId, m_pp.binds[0].instance_id, 0.0f, 0.0f, 0.0f, 0.0f, 1, GateToBindPoint);
	else
		MovePC(m_pp.binds[bindnum].zoneId, m_pp.binds[bindnum].instance_id, m_pp.binds[bindnum].x, m_pp.binds[bindnum].y, m_pp.binds[bindnum].z, m_pp.binds[bindnum].heading, 1);
}

void Client::GoToDeath() {
	//Client will request a zone in EQMac era clients, but let's make sure they get there:
	zone_mode = ZoneToBindPoint;
	MovePC(m_pp.binds[0].zoneId, m_pp.binds[0].instance_id, 0.0f, 0.0f, 0.0f, 0.0f, 1, ZoneToBindPoint);
}

void Client::SetZoneFlag(uint32 zone_id, uint8 key) {
	if(HasZoneFlag(zone_id, key))
		return;

	ClearZoneFlag(zone_id);

	ZoneFlags_Struct* zfs = new ZoneFlags_Struct;
	zfs->zoneid = zone_id;
	zfs->key = key;
	ZoneFlags.Insert(zfs);

	std::string query = StringFormat("INSERT INTO character_zone_flags (id,zoneID,key_) VALUES(%d,%d,%d)", CharacterID(), zone_id, key);
	auto results = database.QueryDatabase(query);
	if(!results.Success())
		Log.Out(Logs::General, Logs::Error, "MySQL Error while trying to set zone flag for %s: %s", GetName(), results.ErrorMessage().c_str());
}

void Client::ClearZoneFlag(uint32 zone_id) {
	if(!HasZoneFlag(zone_id))
		return;

	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		if (zfs->zoneid == zone_id)
		{
			iterator.RemoveCurrent(true);
		}
		iterator.Advance();
	}

	std::string query = StringFormat("DELETE FROM character_zone_flags WHERE id=%d AND zoneID=%d", CharacterID(), zone_id);
	auto results = database.QueryDatabase(query);
	if(!results.Success())
		Log.Out(Logs::General, Logs::Error, "MySQL Error while trying to clear zone flag for %s: %s", GetName(), results.ErrorMessage().c_str());

}

void Client::LoadZoneFlags(LinkedList<ZoneFlags_Struct*>* ZoneFlags) 
{
	ZoneFlags->Clear();
	std::string query = StringFormat("SELECT zoneID, key_ from character_zone_flags WHERE id=%d order by zoneID", CharacterID());
	auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        Log.Out(Logs::General, Logs::Error, "MySQL Error while trying to load zone flags for %s: %s", GetName(), results.ErrorMessage().c_str());
        return;
    }

	for(auto row = results.begin(); row != results.end(); ++row)
	{
		ZoneFlags_Struct* zfs = new ZoneFlags_Struct;
		zfs->zoneid = atoi(row[0]);
		zfs->key = atoi(row[1]);
		ZoneFlags->Insert(zfs);
	}
}

bool Client::HasZoneFlag(uint32 zone_id, uint8 key) {

	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		if (zfs->zoneid == zone_id && zfs->key >= key)
		{
			return true;
		}
		iterator.Advance();
	}
	return false;
}

uint8 Client::GetZoneFlagKey(uint32 zone_id) {

	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		if (zfs->zoneid == zone_id)
		{
			return zfs->key;
		}
		iterator.Advance();
	}

	return 0;
}

void Client::SendZoneFlagInfo(Client *to) {
	if(ZoneFlags.Count() == 0) {
		to->Message(CC_Default, "%s has no zone flags.", GetName());
		return;
	}

	to->Message(CC_Default, "Flags for %s:", GetName());
	char empty[1] = { '\0' };
	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		uint32 zoneid = zfs->zoneid;
		uint8 key = zfs->key;

		const char *short_name = database.GetZoneName(zoneid);

		char *long_name = nullptr;
		database.GetZoneLongName(short_name, &long_name);
		if(long_name == nullptr)
			long_name = empty;

		float safe_x, safe_y, safe_z;
		int16 minstatus = 0;
		uint8 minlevel = 0;
		char flag_name[128];
		if(!database.GetSafePoints(short_name, 0, &safe_x, &safe_y, &safe_z, &minstatus, &minlevel, flag_name)) {
			strcpy(flag_name, "(ERROR GETTING NAME)");
		}

		to->Message(CC_Default, "Has Flag %s for zone %s (%d,%s) Key: %d", flag_name, long_name, zoneid, short_name, key);
		if(long_name != empty)
			delete[] long_name;

		iterator.Advance();
	}
}

bool Client::CanBeInZone() {
	//check some critial rules to see if this char needs to be booted from the zone
	//only enforce rules here which are serious enough to warrant being kicked from
	//the zone

	if(Admin() >= RuleI(GM, MinStatusToZoneAnywhere))
		return(true);

	float safe_x, safe_y, safe_z;
	int16 minstatus = 0;
	uint8 minlevel = 0, expansion = 0;
	char flag_needed[128];
	if(!database.GetSafePoints(zone->GetShortName(), zone->GetInstanceVersion(), &safe_x, &safe_y, &safe_z, &minstatus, &minlevel, flag_needed, &expansion)) {
		//this should not happen...
		Log.Out(Logs::Detail, Logs::Character, "[CLIENT] Unable to query zone info for ourself '%s'", zone->GetShortName());
		return(false);
	}

	if(GetLevel() < minlevel) {
		Log.Out(Logs::Detail, Logs::Character, "[CLIENT] Character does not meet min level requirement (%d < %d)!", GetLevel(), minlevel);
		return(false);
	}
	if(Admin() < minstatus) {
		Log.Out(Logs::Detail, Logs::Character, "[CLIENT] Character does not meet min status requirement (%d < %d)!", Admin(), minstatus);
		return(false);
	}

	if(flag_needed[0] != '\0') {
		//the flag needed string is not empty, meaning a flag is required.
		if(Admin() < minStatusToIgnoreZoneFlags && !HasZoneFlag(zone->GetZoneID())) {
			Log.Out(Logs::Detail, Logs::Character, "[CLIENT] Character does not have the flag to be in this zone (%s)!", flag_needed);
			return(false);
		}
	}

	bool has_expansion = expansion & m_pp.expansions;
	if(Admin() < minStatusToIgnoreZoneFlags && expansion > ClassicEQ && !has_expansion)
	{
		Log.Out(Logs::Detail, Logs::Character, "[CLIENT] Character does not have the required expansion (%d ~ %d)!", m_pp.expansions, expansion);
		return(false);
	}

	return(true);
}

void Client::UpdateZoneChangeCount(uint32 zoneID)
{
	if(zoneID != GetZoneID() && !ignore_zone_count)
	{
		++m_pp.zone_change_count;
		ignore_zone_count = true;
	}

}

