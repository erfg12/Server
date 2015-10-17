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

#include "../common/global_define.h"
#include "../common/features.h"
#include "../common/rulesys.h"
#include "../common/string_util.h"

#include "client.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"

#include "queryserv.h"
#include "quest_parser_collection.h"
#include "string_ids.h"

extern QueryServ* QServ;


float Mob::GetBaseEXP()
{
	float exp = EXP_FORMULA;

	float zemmod = 75.0;
	if(zone->newzone_data.zone_exp_multiplier >= 0)
	{
		zemmod = zone->newzone_data.zone_exp_multiplier * 100;
	}

	float totalmod = 1.0;
	if(zone->IsHotzone())
	{
		totalmod += RuleR(Zone, HotZoneBonus);
	}

	// AK had a permanent 20% XP increase.
	totalmod += 0.20;
	
	float basexp = exp * zemmod * totalmod;
	float logged_xp = basexp;

	if(player_damage == 0)
	{
		basexp *= 0.25f;
		Log.Out(Logs::General, Logs::EQMac, "%s was not damaged by a player. Full XP was: %0.2f Earned XP is: %0.2f", GetName(), logged_xp, basexp);
	}
	else if(dire_pet_damage > 0)
	{
		float percentage = static_cast<float>(dire_pet_damage)/total_damage;
		percentage *= 100.0f;
		if(percentage > 50 && percentage <= 74)
			basexp *= 0.75f;
		else if(percentage >= 75)
			basexp *= 0.50f;

		Log.Out(Logs::General, Logs::EQMac, "%s was %0.2f percent damaged by a dire charmed pet (%d/%d). Full XP was: %0.2f Earned XP is: %0.2f", GetName(), percentage, dire_pet_damage, total_damage, logged_xp, basexp);
	}

	return basexp;
}

void Client::AddEXP(uint32 in_add_exp, uint8 conlevel, bool resexp) {

	this->EVENT_ITEM_ScriptStopReturn();

	if(conlevel == CON_GREEN)
		return;

	uint32 add_exp = in_add_exp;

	if(!resexp && (XPRate != 0))
		add_exp = static_cast<uint32>(in_add_exp * (static_cast<float>(XPRate) / 100.0f));

	if (m_epp.perAA<0 || m_epp.perAA>100)
		m_epp.perAA=0;	// stop exploit with sanity check

	uint32 add_aaxp;
	if(resexp) {
		add_aaxp = 0;
	} else {

		//figure out how much of this goes to AAs
		add_aaxp = add_exp * m_epp.perAA / 100;
		//take that ammount away from regular exp
		add_exp -= add_aaxp;

		float totalmod = 1.0;
		if(RuleR(Character, ExpMultiplier) >= 0){
			totalmod *= RuleR(Character, ExpMultiplier);
		}

		add_exp = uint32(float(add_exp) * totalmod);
	}

	if(RuleB(Zone, LevelBasedEXPMods)){
		if(zone->level_exp_mod[GetLevel()].ExpMod){
			add_exp *= zone->level_exp_mod[GetLevel()].ExpMod;
			add_aaxp *= zone->level_exp_mod[GetLevel()].AAExpMod;
		}
	}

	uint32 requiredxp = GetEXPForLevel(GetLevel() + 1) - GetEXPForLevel(GetLevel());
	float xp_cap = (float)requiredxp * 0.13f; //13% of total XP is our cap

	if(add_exp > xp_cap)
		add_exp = xp_cap;

	if(add_aaxp > xp_cap)
		add_aaxp = xp_cap;

	uint32 exp = GetEXP() + add_exp;
	uint32 aaexp = (uint32)(RuleR(Character, AAExpMultiplier) * add_aaxp);
	uint32 had_aaexp = GetAAXP();
	aaexp += had_aaexp;
	if(aaexp < had_aaexp)
		aaexp = had_aaexp;	//watch for wrap

	uint32 neededxp = GetEXPForLevel(GetLevel()+1) - (GetEXP() + add_exp);
	if (admin>=100 && GetGM()) {
		Message(CC_Yellow, "[GM] You have gained %d (%d) AXP and %d (%d) EXP. %d more EXP is needed for Level %d", add_aaxp, GetAAXP() + add_aaxp, add_exp, GetEXP() + add_exp, neededxp, GetLevel()+1);
	}
	//Message(CC_Yellow, "AddExp: XP awarded: %i (%i) Required XP is: %i Cap: %0.2f Race: %i Class: %i Zoneid: %i", add_exp, GetEXP() + add_exp, requiredxp, xp_cap, GetBaseRace(), GetClass(), zone->GetZoneID());
	SetEXP(exp, aaexp, resexp);
}

void Client::AddEXPPercent(uint8 percent, uint8 level) {

	if(percent < 0)
		percent = 1;
	if(percent > 100)
		percent = 100;

	uint32 requiredxp = GetEXPForLevel(level+1) - GetEXPForLevel(level);
	float tmpxp = requiredxp * (percent/100.0);
	uint32 newxp = (uint32)tmpxp;
	AddQuestEXP(newxp);
}

void Client::AddQuestEXP(uint32 in_add_exp) {

	// Quest handle method. This divides up AA XP, but does not apply bonuses/modifiers. The quest writer will do that.

	this->EVENT_ITEM_ScriptStopReturn();

	uint32 add_exp = in_add_exp;

	if (m_epp.perAA<0 || m_epp.perAA>100)
		m_epp.perAA=0;	// stop exploit with sanity check

	uint32 add_aaxp;

	//figure out how much of this goes to AAs
	add_aaxp = add_exp * m_epp.perAA / 100;
	//take that ammount away from regular exp
	add_exp -= add_aaxp;

	uint32 exp = GetEXP() + add_exp;
	uint32 aaexp = add_aaxp;
	uint32 had_aaexp = GetAAXP();
	aaexp += had_aaexp;
	if(aaexp < had_aaexp)
		aaexp = had_aaexp;	//watch for wrap

	SetEXP(exp, aaexp, false);

}


void Client::SetEXP(uint32 set_exp, uint32 set_aaxp, bool isrezzexp) {
	Log.Out(Logs::Detail, Logs::None, "Attempting to Set Exp for %s (XP: %u, AAXP: %u, Rez: %s)", this->GetCleanName(), set_exp, set_aaxp, isrezzexp ? "true" : "false");
	//max_AAXP = GetEXPForLevel(52) - GetEXPForLevel(51);	//GetEXPForLevel() doesn't depend on class/race, just level, so it shouldn't change between Clients
	max_AAXP = max_AAXP = GetEXPForLevel(0, true);	//this may be redundant since we're doing this in Client::FinishConnState2()
	if (max_AAXP == 0 || GetEXPForLevel(GetLevel()) == 0xFFFFFFFF) {
		Message(CC_Red, "Error in Client::SetEXP. EXP not set.");
		return; // Must be invalid class/race
	}

	if ((set_exp + set_aaxp) > (m_pp.exp+m_pp.expAA)) {
		if (isrezzexp)
			this->Message_StringID(CC_Yellow, REZ_REGAIN);
		else{
			if(this->IsGrouped())
				this->Message_StringID(CC_Yellow, GAIN_GROUPXP);
			else if(IsRaidGrouped())
				Message_StringID(CC_Yellow, GAIN_RAIDEXP);
			else
				this->Message_StringID(CC_Yellow, GAIN_XP);
		}
	}
	else if((set_exp + set_aaxp) < (m_pp.exp+m_pp.expAA)){ //only loss message if you lose exp, no message if you gained/lost nothing.
		Message(CC_Yellow, "You have lost experience.");
	}

	//check_level represents the level we should be when we have
	//this ammount of exp (once these loops complete)
	uint16 check_level = GetLevel()+1;
	//see if we gained any levels
	bool level_increase = true;
	int8 level_count = 0;

	while (set_exp >= GetEXPForLevel(check_level)) 
	{
		check_level++;
		if (check_level > 127)
		{	//hard level cap
			check_level = 127;
			break;
		}
		level_count++;
	}
	//see if we lost any levels
	while (set_exp < GetEXPForLevel(check_level-1))
	{
		check_level--;
		if (check_level < 2)
		{	//hard level minimum
			check_level = 2;
			break;
		}
		level_increase = false;
	}
	check_level--;


	//see if we gained any AAs
	if (set_aaxp >= max_AAXP) {
		/*
			Note: AA exp is stored differently than normal exp.
			Exp points are only stored in m_pp.expAA until you
			gain a full AA point, once you gain it, a point is
			added to m_pp.aapoints and the ammount needed to gain
			that point is subtracted from m_pp.expAA

			then, once they spend an AA point, it is subtracted from
			m_pp.aapoints. In theory it then goes into m_pp.aapoints_spent,
			but im not sure if we have that in the right spot.
		*/
		//record how many points we have
		uint32 last_unspentAA = m_pp.aapoints;

		//figure out how many AA points we get from the exp were setting
		m_pp.aapoints = set_aaxp / max_AAXP;
		Log.Out(Logs::Detail, Logs::None, "Calculating additional AA Points from AAXP for %s: %u / %u = %.1f points", this->GetCleanName(), set_aaxp, max_AAXP, (float)set_aaxp / (float)max_AAXP);

		//get remainder exp points, set in PP below
		set_aaxp = set_aaxp - (max_AAXP * m_pp.aapoints);

		//add in how many points we had
		m_pp.aapoints += last_unspentAA;
		//set_aaxp = m_pp.expAA % max_AAXP;

		//figure out how many points were actually gained
		/*uint32 gained = m_pp.aapoints - last_unspentAA;*/	//unused

		//Message(CC_Yellow, "You have gained %d skill points!!", m_pp.aapoints - last_unspentAA);
		char val1[20]={0};
		Message_StringID(CC_Yellow, GAIN_ABILITY_POINT,ConvertArray(m_pp.aapoints, val1),m_pp.aapoints == 1 ? "" : "(s)");	//You have gained an ability point! You now have %1 ability point%2.
		
		/* QS: PlayerLogAARate */
		if (RuleB(QueryServ, PlayerLogAARate)){
			int add_points = (m_pp.aapoints - last_unspentAA);
			std::string query = StringFormat("INSERT INTO `qs_player_aa_rate_hourly` (char_id, aa_count, hour_time) VALUES (%i, %i, UNIX_TIMESTAMP() - MOD(UNIX_TIMESTAMP(), 3600)) ON DUPLICATE KEY UPDATE `aa_count` = `aa_count` + %i", this->CharacterID(), add_points, add_points);
			QServ->SendQuery(query.c_str());
		}

		//Message(CC_Yellow, "You now have %d skill points available to spend.", m_pp.aapoints);
	}

	uint8 maxlevel = RuleI(Character, MaxExpLevel) + 1;

	if(maxlevel <= 1)
		maxlevel = RuleI(Character, MaxLevel) + 1;

	if(check_level > maxlevel) {
		check_level = maxlevel;

		if(RuleB(Character, KeepLevelOverMax)) {
			set_exp = GetEXPForLevel(GetLevel()+1);
		}
		else {
			set_exp = GetEXPForLevel(maxlevel);
		}
	}

	if(RuleB(Character, PerCharacterQglobalMaxLevel)){
		uint32 MaxLevel = GetCharMaxLevelFromQGlobal();
		if(MaxLevel){
			if(GetLevel() >= MaxLevel){
				uint32 expneeded = GetEXPForLevel(MaxLevel);
				if(set_exp > expneeded) {
					set_exp = expneeded;
				}
			}
		}
	}

	if ((GetLevel() != check_level) && !(check_level >= maxlevel))
	{
		char val1[20] = { 0 };
		if (level_increase)
		{
			if (level_count == 1)
			{
				Message_StringID(CC_Yellow, GAIN_LEVEL, ConvertArray(check_level, val1));
				/* Message(CC_Yellow, "You have gained a level! Welcome to level %i!", check_level); */
			}
			else
				Message(CC_Yellow, "Welcome to level %i!", check_level);

			if (check_level == RuleI(Character, DeathItemLossLevel))
				Message_StringID(CC_Yellow, CORPSE_ITEM_LOST);

			if (check_level == RuleI(Character, DeathExpLossLevel))
				Message_StringID(CC_Yellow, CORPSE_EXP_LOST);
		}
		else 
		{
			Message_StringID(CC_Yellow, LOSE_LEVEL, ConvertArray(check_level, val1));
			/* Message(CC_Yellow, "You lost a level! You are now level %i!", check_level); */
		}
		SetLevel(check_level);
	}

	//If were at max level then stop gaining experience if we make it to the cap
	if(GetLevel() == maxlevel - 1){
		uint32 expneeded = GetEXPForLevel(maxlevel);
		if(set_exp > expneeded) {
			set_exp = expneeded;
		}
	}

	//set the client's EXP and AAEXP
	m_pp.exp = set_exp;
	m_pp.expAA = set_aaxp;

	if (GetLevel() < 51) {
		m_epp.perAA = 0;	// turn off aa exp if they drop below 51
	} else
		SendAAStats();	//otherwise, send them an AA update

	//send the expdata in any case so the xp bar isnt stuck after leveling
	uint32 tmpxp1 = GetEXPForLevel(GetLevel()+1);
	uint32 tmpxp2 = GetEXPForLevel(GetLevel());
	// Quag: crash bug fix... Divide by zero when tmpxp1 and 2 equalled each other, most likely the error case from GetEXPForLevel() (invalid class, etc)
	if (tmpxp1 != tmpxp2 && tmpxp1 != 0xFFFFFFFF && tmpxp2 != 0xFFFFFFFF) {
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ExpUpdate, sizeof(ExpUpdate_Struct));
		ExpUpdate_Struct* eu = (ExpUpdate_Struct*)outapp->pBuffer;
		float tmpxp = (float) ( (float) set_exp-tmpxp2 ) / ( (float) tmpxp1-tmpxp2 );
		eu->exp = (uint32)(330.0f * tmpxp);
		FastQueuePacket(&outapp);
	}

/*	if (admin>=100 && GetGM()) {
		char val1[20]={0};
		char val2[20]={0};
		char val3[20]={0};
		Message_StringID(CC_Yellow, GM_GAINXP,ConvertArray(set_aaxp,val1),ConvertArray(set_exp,val2),ConvertArray(GetEXPForLevel(GetLevel()+1),val3)); //[GM] You have gained %1 AXP and %2 EXP (%3).
		//Message(CC_Yellow, "[GM] You have gained %d AXP and %d EXP (%d)", set_aaxp, set_exp, GetEXPForLevel(GetLevel()+1));
		//Message(CC_Yellow, "[GM] You have gained %d AXP and %d EXP (%d)", set_aaxp, set_exp, GetEXPForLevel(GetLevel()+1));
		//Message(CC_Yellow, "[GM] You now have %d / %d EXP and %d / %d AA exp.", set_exp, GetEXPForLevel(GetLevel()+1), set_aaxp, max_AAXP);
	}*/
}

void Client::SetLevel(uint8 set_level, bool command)
{
	if (GetEXPForLevel(set_level) == 0xFFFFFFFF) {
		Log.Out(Logs::General, Logs::Error, "Client::SetLevel() GetEXPForLevel(%i) = 0xFFFFFFFF", set_level);
		return;
	}

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_LevelUpdate, sizeof(LevelUpdate_Struct));
	LevelUpdate_Struct* lu = (LevelUpdate_Struct*)outapp->pBuffer;
	lu->level = set_level;
	if(m_pp.level2 != 0)
		lu->level_old = m_pp.level2;
	else
		lu->level_old = level;

	level = set_level;

	if(IsRaidGrouped()) {
		Raid *r = this->GetRaid();
		if(r){
			r->UpdateLevel(GetName(), set_level);
		}
	}
	if(set_level > m_pp.level2) {
		if(m_pp.level2 == 0)
			m_pp.points += 5;
		else
			m_pp.points += (5 * (set_level - m_pp.level2));

		m_pp.level2 = set_level;
	}
	if(set_level > m_pp.level) {
		parse->EventPlayer(EVENT_LEVEL_UP, this, "", 0);
		/* QS: PlayerLogLevels */
		if (RuleB(QueryServ, PlayerLogLevels)){
			std::string event_desc = StringFormat("Leveled UP :: to Level:%i from Level:%i in zoneid:%i instid:%i", set_level, m_pp.level, this->GetZoneID(), this->GetInstanceID());
			QServ->PlayerLogEvent(Player_Log_Levels, this->CharacterID(), event_desc); 
		}
	}
	else if (set_level < m_pp.level){
		/* QS: PlayerLogLevels */
		if (RuleB(QueryServ, PlayerLogLevels)){
			std::string event_desc = StringFormat("Leveled DOWN :: to Level:%i from Level:%i in zoneid:%i instid:%i", set_level, m_pp.level, this->GetZoneID(), this->GetInstanceID());
			QServ->PlayerLogEvent(Player_Log_Levels, this->CharacterID(), event_desc);
		}
	}

	m_pp.level = set_level;
	if (command){
		m_pp.exp = GetEXPForLevel(set_level);
		Message(CC_Yellow, "Welcome to level %i!", set_level);
		lu->exp = 0;
	}
	else {
		float tmpxp = (float) ( (float) m_pp.exp - GetEXPForLevel( GetLevel() )) / ( (float) GetEXPForLevel(GetLevel()+1) - GetEXPForLevel(GetLevel()));
		lu->exp = (uint32)(330.0f * tmpxp);
	}
	QueuePacket(outapp);
	safe_delete(outapp);
	this->SendAppearancePacket(AT_WhoLevel, set_level); // who level change

	Log.Out(Logs::General, Logs::Normal, "Setting Level for %s to %i", GetName(), set_level);

	CalcBonuses();

	if(!RuleB(Character, HealOnLevel)) {
		int mhp = CalcMaxHP();
		if(GetHP() > mhp)
			SetHP(mhp);
	}
	else {
		SetHP(CalcMaxHP()); // Why not, lets give them a free heal
	}
	if(!RuleB(Character, ManaOnLevel))
	{
		int mp = CalcMaxMana();
		if(GetMana() > mp)
			SetMana(mp);
	}
	else
	{
		SetMana(CalcMaxMana()); // Why not, lets give them a free heal
	}

	SendHPUpdate();
	SendManaUpdate();
	UpdateWho();
	Save();
}

uint32 Client::GetEXPForLevel(uint16 check_level, bool aa)
{

	// Warning: Changing anything in this method WILL cause levels to change in-game the first time a player
	// gains or loses XP. 

	if(aa)
	{
		if(m_epp.perAA > 99)
			return (RuleI(AA, ExpPerPoint));
		else
			check_level = 52;
	}

	check_level -= 1;
	float base = (check_level)*(check_level)*(check_level);

	// Classes: In the XP formula AK used, they WERE calculated in. This was due to Sony not being able to change their XP
	// formula drastically (see above comment.) Instead, they gave the penalized classes a bonus on gain. We've decided to go
	// the easy route, and simply not use a class mod at all.

	float playermod = 10;
	uint8 race = GetBaseRace();
	if(race == HALFLING)
		playermod *= 95.0;
	else if(race == DARK_ELF || race == DWARF || race == ERUDITE || race == GNOME || 
		race == HALF_ELF || race == HIGH_ELF || race == HUMAN || race == WOOD_ELF ||
		race == VAHSHIR)
		playermod *= 100.0;
	else if(race == BARBARIAN)
		playermod *= 105.0;
	else if(race == OGRE)
		playermod *= 115.0;
	else if(race == IKSAR || race == TROLL)
		playermod *= 120.0;

	float mod;
	if (check_level <= 29)
		mod = 1.0;
	else if (check_level <= 34)
		mod = 1.1;
	else if (check_level <= 39)
		mod = 1.2;
	else if (check_level <= 44)
		mod = 1.3;
	else if (check_level <= 50)
		mod = 1.4;
	else if (check_level == 51)
		mod = 1.5;
	else if (check_level == 52)
		mod = 1.6;
	else if (check_level == 53)
		mod = 1.7;
	else if (check_level == 54)
		mod = 1.9;
	else if (check_level == 55)
		mod = 2.1;
	else if (check_level == 56)
		mod = 2.3;
	else if (check_level == 57)
		mod = 2.5;
	else if (check_level == 58)
		mod = 2.7;
	else if (check_level == 59)
		mod = 3.0;
	else if (check_level == 60)
		mod = 3.1;
	else if (check_level == 61)
		mod = 3.2;
	else if (check_level == 62)
		mod = 3.3;
	else if (check_level == 63)
		mod = 3.4;
	else if (check_level == 64)
		mod = 3.5;
	else
		mod = 3.6;

	uint32 finalxp = uint32(base * playermod * mod);
	if(aa)
	{
		uint32 aaxp;
		aaxp = finalxp - GetEXPForLevel(51);
		return aaxp;
	}

	finalxp = mod_client_xp_for_level(finalxp, check_level);
	return finalxp;
}

void Client::AddLevelBasedExp(uint8 exp_percentage, uint8 max_level) { 
	if (exp_percentage > 100) { exp_percentage = 100; } 
	if (!max_level || GetLevel() < max_level) { max_level = GetLevel(); } 
	uint32 newexp = GetEXP() + ((GetEXPForLevel(max_level + 1) - GetEXPForLevel(max_level)) * exp_percentage / 100); 
	SetEXP(newexp, GetAAXP());
}

void Group::SplitExp(uint32 exp, Mob* other) {
	if( other->CastToNPC()->MerchantType != 0 ) // Ensure NPC isn't a merchant
		return;

	if(other->GetOwner() && other->GetOwner()->IsClient()) // Ensure owner isn't pc
		return;

	unsigned int i;
	int8 membercount = 0;
	int8 close_membercount = 0;
	uint8 maxlevel = 1;
	uint16 weighted_levels = 0;
	uint8 minlevel = 65;

	// This loop grabs the max player level for the group level check further down, the min level to subtract the 6th group member, adds up all
	// the player levels for the weighted split, and does a preliminary count of the group members for the group bonus calc. 
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr  && members[i]->IsClient()) {
			Client *cmember = members[i]->CastToClient();
			if(cmember->GetZoneID() == zone->GetZoneID())
			{
				if(cmember->GetLevel() > maxlevel)
					maxlevel = cmember->GetLevel();

				if(cmember->GetLevel() < minlevel)
					minlevel = cmember->GetLevel();

				if(cmember->GetLevelCon(other->GetLevel()) != CON_GREEN)
				{
					++membercount;
					if(cmember->IsInRange(other))
					{
						weighted_levels += cmember->GetLevel();
						++close_membercount;
					}
				}
			}
		}
	}

	// If the NPC is green to the whole group or they are all out of the kill zone (wipe?) this will return.
	if (membercount <= 0 || close_membercount <= 0)
		return;

	// We don't need this variable set if we don't have a 6th member to subtract from the split.
	if(close_membercount < 6)
		minlevel = 0;

	bool isgreen = false;
	int conlevel = Mob::GetLevelCon(maxlevel, other->GetLevel());
	if(conlevel == CON_GREEN)
		isgreen = true;

	//Give XP out to lower toons from NPCs that are green to the highest player.
	if(isgreen && !RuleB(AlKabor, GreensGiveXPToGroup))
		return;

	// This loop uses the max player level we now have to determine who is in level range to gain XP. Anybody
	// outside the range is subtracted from the player count and has their level removed from the weighted split.
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr && members[i]->IsClient()) // If Group Member is Client
		{
			Client *cmember = members[i]->CastToClient();
			if (!cmember->IsInLevelRange(maxlevel) &&
				cmember->CastToClient()->GetZoneID() == zone->GetZoneID() &&
				cmember->GetLevelCon(other->GetLevel()) != CON_GREEN) 
			{
				if(membercount != 0 && close_membercount != 0)
				{
					Log.Out(Logs::Detail, Logs::Group, "%s is not within level range, removing from XP gain.", cmember->GetName());
					--membercount;

					if(cmember->CastToClient()->IsInRange(other))
					{
						if(weighted_levels >= cmember->GetLevel())
							weighted_levels -= cmember->GetLevel();

						--close_membercount;
						minlevel = 0;
					}
				}
				else
					return;
			}	
		}
	}

	// Another sanity check.
	if (membercount <= 0 || close_membercount <= 0)
		return;

	// Count players out of range from the corpse but still in the zone towards the bonus.
	if(!RuleB(AlKabor, OutOfRangeGroupXPBonus))
		membercount = close_membercount;

	float groupmod = 1.0;
	if (membercount == 2)
		groupmod += 0.20;
	else if(membercount == 3)
		groupmod += 0.40;
	if(RuleB(AlKabor, GroupEXPBonuses))
	{
		// Use the "broken" bonuses on AK.
		if(membercount == 4)
			groupmod += 1.20;
		else if(membercount > 4)
			groupmod += 1.60;
	}
	else
	{
		// Use the bonuses how they should have been.
		if(membercount == 4)
			groupmod += 0.60;
		else if(membercount > 4)
			groupmod += 0.80;
	}

	float groupexp = (static_cast<float>(exp) * groupmod) * RuleR(Character, GroupExpMultiplier);

	// 6th member is free in the split.
	if(!RuleB(AlKabor, Count6thGroupMember))
	{
		if(close_membercount == 6)
			weighted_levels -= minlevel;
	}

	// This loop figures out the split, and sends XP for each player that qualifies. (NPC is not green to the player, player is in the 
	// zone where the kill occured, is in range of the corpse, and is in level range with the rest of the group.)
	for (i = 0; i < MAX_GROUP_MEMBERS; i++) 
	{
		if (members[i] != nullptr && members[i]->IsClient()) // If Group Member is Client
		{
			Client *cmember = members[i]->CastToClient();
			
			if(cmember->CastToClient()->GetZoneID() == zone->GetZoneID() &&
				cmember->GetLevelCon(other->GetLevel()) != CON_GREEN &&
				cmember->IsInRange(other))
			{
				if (cmember->IsInLevelRange(maxlevel)) 
				{
					float split_percent = static_cast<float>(cmember->GetLevel()) / weighted_levels;
					float splitgroupxp = groupexp * split_percent;
					if(splitgroupxp < 1)
						splitgroupxp = 1;
					
					if(conlevel == CON_GREEN)
						conlevel = Mob::GetLevelCon(cmember->GetLevel(), other->GetLevel());

					cmember->AddEXP(static_cast<uint32>(splitgroupxp), conlevel);
					Log.Out(Logs::Detail, Logs::Group, "%s splits %0.2f with the rest of the group. Their share: %0.2f", cmember->GetName(), groupexp, splitgroupxp);
					//if(cmember->Admin() > 80)
					//	cmember->Message(CC_Yellow, "Group XP awarded is: %0.2f Total XP is: %0.2f for count: %i total count: %i in_exp is: %i", splitgroupxp, groupexp, close_membercount, membercount, exp);

				}
				else
					Log.Out(Logs::Detail, Logs::Group, "%s is too low in level to gain XP from this group.", cmember->GetName());
			}
			else
				Log.Out(Logs::Detail, Logs::Group, "%s is not in the kill zone, is out of range, or %s is green to them. They won't recieve group XP.", cmember->GetName(), other->GetCleanName());
		}
	}
}

void Raid::SplitExp(uint32 exp, Mob* other) {
	if( other->CastToNPC()->MerchantType != 0 ) // Ensure NPC isn't a merchant
		return;

	if(other->GetOwner() && other->GetOwner()->IsClient()) // Ensure owner isn't pc
		return;

	uint32 groupexp = exp;
	uint8 membercount = 0;
	uint8 maxlevel = 1;

	for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
		if (members[i].member != nullptr) {
			if(members[i].member->GetLevel() > maxlevel)
				maxlevel = members[i].member->GetLevel();

			membercount++;
		}
	}

	groupexp = (uint32)((float)groupexp * (1.0f-(RuleR(Character, RaidExpMultiplier))));

	int conlevel = Mob::GetLevelCon(maxlevel, other->GetLevel());
	if(conlevel == CON_GREEN)
		return;	//no exp for greenies...

	if (membercount == 0)
		return;

	for (unsigned int x = 0; x < MAX_RAID_MEMBERS; x++) {
		if (members[x].member != nullptr) // If Group Member is Client
		{
			Client *cmember = members[x].member;
			// add exp + exp cap
			int16 diff = cmember->GetLevel() - maxlevel;
			int16 maxdiff = -(cmember->GetLevel()*15/10 - cmember->GetLevel());
			if(maxdiff > -5)
				maxdiff = -5;
			if (diff >= (maxdiff)) { /*Instead of person who killed the mob, the person who has the highest level in the group*/
				uint32 tmp = (cmember->GetLevel()+3) * (cmember->GetLevel()+3) * 75 * 35 / 10;
				uint32 tmp2 = (groupexp / membercount) + 1;
				cmember->AddEXP( tmp < tmp2 ? tmp : tmp2, conlevel);
			}
		}
	}
}

uint32 Client::GetCharMaxLevelFromQGlobal() {
	QGlobalCache *char_c = nullptr;
	char_c = this->GetQGlobals();

	std::list<QGlobal> globalMap;
	uint32 ntype = 0;

	if(char_c) {
		QGlobalCache::Combine(globalMap, char_c->GetBucket(), ntype, this->CharacterID(), zone->GetZoneID());
	}

	std::list<QGlobal>::iterator iter = globalMap.begin();
	uint32 gcount = 0;
	while(iter != globalMap.end()) {
		if((*iter).name.compare("CharMaxLevel") == 0){
			return atoi((*iter).value.c_str());
		} 
		++iter;
		++gcount;
	}

	return false;
}

bool Client::IsInRange(Mob* defender)
{
	float exprange = RuleR(Zone, GroupEXPRange);

	float t1, t2, t3;
	t1 = defender->GetX() - GetX();
	t2 = defender->GetY() - GetY();
	//t3 = defender->GetZ() - GetZ();
	if(t1 < 0)
		abs(t1);
	if(t2 < 0)
		abs(t2);
	//if(t3 < 0)
	//	abs(t3);
	if(( t1 > exprange) || ( t2 > exprange)) { //	|| ( t3 > 40) ) {
		//_log(CLIENT__EXP, "%s is out of range. distances (%.3f,%.3f,%.3f), range %.3f No XP will be awarded.", defender->GetName(), t1, t2, t3, exprange);
		return false;
	}
	else
		return true;
}

bool Client::IsInLevelRange(uint8 maxlevel)
{
	uint8 max_level = GetLevel()*1.5 + 0.5;
	if(max_level < 6)
		max_level = 6;

	if (max_level >= maxlevel) 
		return true;
	else
		return false;
}

void Client::GetExpLoss(Mob* killerMob, uint16 spell, int &exploss)
{
	float loss;
	uint8 level = GetLevel();
	if(level >= 1 && level <= 29)
		loss = 0.16f;
	if(level == 30)
		loss = 0.08f;
	if(level >= 31 && level <= 34)
		loss = 0.15f;
	if(level == 35)
		loss = 0.075f;
	if(level >= 36 && level <= 39)
		loss = 0.14f;
	if(level == 40)
		loss = 0.07f;
	if(level >= 41 && level <= 44)
		loss = 0.13f;
	if(level == 45)
		loss = 0.065f;
	if(level >= 46 && level <= 50)
		loss = 0.12f;
	if(level >= 51)
		loss = 0.06f;

	if(RuleB(Character, SmoothEXPLoss))
	{
		if(loss >= 0.12)
			loss /= 2;
	}

	int requiredxp = GetEXPForLevel(level + 1) - GetEXPForLevel(level);
	exploss=(int)((float)requiredxp * (loss * RuleR(Character, EXPLossMultiplier)));

	if( (level < RuleI(Character, DeathExpLossLevel)) || (level > RuleI(Character, DeathExpLossMaxLevel)) || IsBecomeNPC() )
	{
		exploss = 0;
	}
	else if( killerMob )
	{
		if( killerMob->IsClient() )
		{
			exploss = 0;
		}
		else if( killerMob->GetOwner() && killerMob->GetOwner()->IsClient() )
		{
			exploss = 0;
		}
	}

	if(spell != SPELL_UNKNOWN)
	{
		uint32 buff_count = GetMaxTotalSlots();
		for(uint16 buffIt = 0; buffIt < buff_count; buffIt++)
		{
			if(buffs[buffIt].spellid == spell && buffs[buffIt].client)
			{
				exploss = 0;	// no exp loss for pvp dot
				break;
			}
		}
	}
}
