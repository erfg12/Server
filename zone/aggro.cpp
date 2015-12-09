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
#include "../common/faction.h"
#include "../common/rulesys.h"
#include "../common/spdat.h"

#include "client.h"
#include "corpse.h"
#include "entity.h"
#include "mob.h"
#include "water_map.h"

#ifdef BOTS
#include "bot.h"
#endif

#include "map.h"

extern Zone* zone;
//#define LOSDEBUG 6

//look around a client for things which might aggro the client.
void EntityList::CheckClientAggro(Client *around)
{
	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		Mob *mob = it->second;
		if (mob->IsClient())	//also ensures that mob != around
			continue;
		if (mob->IsPet())
			continue;
		if (mob->CheckWillAggro(around) && !mob->CheckAggro(around))
		{
			mob->AddToHateList(around, 20);
		}
	}
}

void EntityList::DescribeAggro(Client *towho, NPC *from_who, float d, bool verbose) {
	float d2 = d*d;

	towho->Message(CC_Default, "Describing aggro for %s", from_who->GetName());

	bool engaged = from_who->IsEngaged();
	if(engaged) {
		Mob *top = from_who->GetHateTop();
		towho->Message(CC_Default, ".. I am currently fighting with %s", top == nullptr?"(nullptr)":top->GetName());
	}
	bool check_npcs = from_who->WillAggroNPCs();

	if(verbose) {
		char namebuf[256];

		int my_primary = from_who->GetPrimaryFaction();
		Mob *own = from_who->GetOwner();
		if(own != nullptr)
			my_primary = own->GetPrimaryFaction();

		if(my_primary == 0) {
			strcpy(namebuf, "(No faction)");
		} else if(my_primary < 0) {
			strcpy(namebuf, "(Special faction)");
		} else {
			if(!database.GetFactionName(my_primary, namebuf, sizeof(namebuf)))
				strcpy(namebuf, "(Unknown)");
		}
		towho->Message(CC_Default, ".. I am on faction %s (%d)\n", namebuf, my_primary);
	}

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		Mob *mob = it->second;
	//	if (mob->IsClient())	//also ensures that mob != around
	//		continue;

		if (DistanceSquared(mob->GetPosition(), from_who->GetPosition()) > d2)
			continue;

		if (engaged) {
			uint32 amm = from_who->GetHateAmount(mob);
			if (amm == 0)
				towho->Message(CC_Default, "... %s is not on my hate list.", mob->GetName());
			else
				towho->Message(CC_Default, "... %s is on my hate list with value %lu", mob->GetName(), (unsigned long)amm);
		} else if (!check_npcs && mob->IsNPC()) {
				towho->Message(CC_Default, "... %s is an NPC and my npc_aggro is disabled.", mob->GetName());
		} 
			
		from_who->DescribeAggro(towho, mob, verbose);
	}
}

void NPC::DescribeAggro(Client *towho, Mob *mob, bool verbose) {
	//this logic is duplicated from below, try to keep it up to date.
	float iAggroRange = GetAggroRange();

	float t1, t2, t3;
	t1 = mob->GetX() - GetX();
	t2 = mob->GetY() - GetY();
	t3 = mob->GetZ() - GetZ();
	//Cheap ABS()
	if(t1 < 0)
		t1 = 0 - t1;
	if(t2 < 0)
		t2 = 0 - t2;
	if(t3 < 0)
		t3 = 0 - t3;
	if(( t1 > iAggroRange)
		|| ( t2 > iAggroRange)
		|| ( t3 > iAggroRange) ) {
		towho->Message(CC_Default, "...%s is out of range (fast). distances (%.3f,%.3f,%.3f), range %.3f", mob->GetName(),
		t1, t2, t3, iAggroRange);
		return;
	}

	if(mob->IsInvisible(this)) {
		towho->Message(CC_Default, "...%s is invisible to me. ", mob->GetName());
		return;
	}
	if((mob->IsClient() &&
		(!mob->CastToClient()->Connected()
		|| mob->CastToClient()->IsLD()
		|| mob->CastToClient()->IsBecomeNPC()
		|| mob->CastToClient()->GetGM()
		)
		))
	{
		towho->Message(CC_Default, "...%s a GM or is not connected. ", mob->GetName());
		return;
	}


	if(mob == GetOwner()) {
		towho->Message(CC_Default, "...%s is my owner. ", mob->GetName());
		return;
	}

	if(mob->IsEngaged() && (!mob->GetSpecialAbility(PROX_AGGRO) || (mob->GetSpecialAbility(PROX_AGGRO) && !towho->CombatRange(mob))))
	{
		towho->Message(CC_Default, "...%s is a new client and I am already in combat. ", mob->GetName());
		return;
	}

	if(mob->IsPet() && !mob->IsCharmed())
	{
		towho->Message(CC_Default, "...%s is a summoned pet. ", mob->GetName());
		return;
	}

	float dist2 = DistanceSquared(mob->GetPosition(), m_Position);

	float iAggroRange2 = iAggroRange*iAggroRange;
	if( dist2 > iAggroRange2 ) {
		towho->Message(CC_Default, "...%s is out of range. %.3f > %.3f ", mob->GetName(),
		dist2, iAggroRange2);
		return;
	}


	if (RuleB(Aggro, UseLevelAggro))
	{
		if (GetLevel() < 18 && mob->GetLevelCon(GetLevel()) == CON_GREEN && GetBodyType() != 3 && !IsAggroOnPC())
		{
			towho->Message(CC_Default, "...%s is red to me (basically)", mob->GetName(),	dist2, iAggroRange2);
			return;
		}
	}
	else
	{
		if (GetINT() > RuleI(Aggro, IntAggroThreshold) && mob->GetLevelCon(GetLevel()) == CON_GREEN)
		{
			towho->Message(CC_Default, "...%s is red to me (basically)", mob->GetName(),
				dist2, iAggroRange2);
			return;
		}
	}

	if(verbose) {
		int my_primary = GetPrimaryFaction();
		int mob_primary = mob->GetPrimaryFaction();
		Mob *own = GetOwner();
		if(own != nullptr)
			my_primary = own->GetPrimaryFaction();
		own = mob->GetOwner();
		if(mob_primary > 0 && own != nullptr)
			mob_primary = own->GetPrimaryFaction();

		if(mob_primary == 0) {
			towho->Message(CC_Default, "...%s has no primary faction", mob->GetName());
		} else if(mob_primary < 0) {
			towho->Message(CC_Default, "...%s is on special faction %d", mob->GetName(), mob_primary);
		} else {
			char namebuf[256];
			if(!database.GetFactionName(mob_primary, namebuf, sizeof(namebuf)))
				strcpy(namebuf, "(Unknown)");
			std::list<struct NPCFaction*>::iterator cur,end;
			cur = faction_list.begin();
			end = faction_list.end();
			bool res = false;
			for(; cur != end; ++cur) {
				struct NPCFaction* fac = *cur;
				if ((int32)fac->factionID == mob_primary) {
					if (fac->npc_value > 0) {
						towho->Message(CC_Default, "...%s is on ALLY faction %s (%d) with %d", mob->GetName(), namebuf, mob_primary, fac->npc_value);
						res = true;
						break;
					} else if (fac->npc_value < 0) {
						towho->Message(CC_Default, "...%s is on ENEMY faction %s (%d) with %d", mob->GetName(), namebuf, mob_primary, fac->npc_value);
						res = true;
						break;
					} else {
						towho->Message(CC_Default, "...%s is on NEUTRAL faction %s (%d) with 0", mob->GetName(), namebuf, mob_primary);
						res = true;
						break;
					}
				}
			}
			if(!res) {
				towho->Message(CC_Default, "...%s is on faction %s (%d), which I have no entry for.", mob->GetName(), namebuf, mob_primary);
			}
		}
	}

	FACTION_VALUE fv = mob->GetReverseFactionCon(this);

	if(!(
			fv == FACTION_SCOWLS
			||
			(mob->GetPrimaryFaction() != GetPrimaryFaction() && mob->GetPrimaryFaction() == -4 && GetOwner() == nullptr)
			||
			fv == FACTION_THREATENLY
		)) {
		towho->Message(CC_Default, "...%s faction not low enough. value='%s'", mob->GetName(), FactionValueToString(fv));
		return;
	}
	if(fv == FACTION_THREATENLY) {
		towho->Message(CC_Default, "...%s threatening to me, so they only have a %d chance per check of attacking.", mob->GetName());
	}

	if(!CheckLosFN(mob)) {
		towho->Message(CC_Default, "...%s is out of sight.", mob->GetName());
	}

	towho->Message(CC_Default, "...%s meets all conditions, I should be attacking them.", mob->GetName());
}

/*
	If you change this function, you should update the above function
	to keep the #aggro command accurate.
*/
bool Mob::CheckWillAggro(Mob *mob) {
	if(!mob)
		return false;

	//sometimes if a client has some lag while zoning into a dangerous place while either invis or a GM
	//they will aggro mobs even though it's supposed to be impossible, to lets make sure we've finished connecting
	if (mob->IsClient()) {
		if (!mob->CastToClient()->ClientFinishedLoading())
			return false;
	}

	Mob *ownr = mob->GetOwner();
	if(ownr && ownr->IsClient() && !ownr->CastToClient()->ClientFinishedLoading())
		return false;

	float iAggroRange = GetAggroRange();

	// Check If it's invisible and if we can see invis
	// Check if it's a client, and that the client is connected and not linkdead,
	// and that the client isn't Playing an NPC, with thier gm flag on
	// Check if it's not a Interactive NPC
	// Trumpcard: The 1st 3 checks are low cost calcs to filter out unnessecary distance checks. Leave them at the beginning, they are the most likely occurence.
	// Image: I moved this up by itself above faction and distance checks because if one of these return true, theres no reason to go through the other information

	float t1, t2, t3;
	t1 = mob->GetX() - GetX();
	t2 = mob->GetY() - GetY();
	t3 = mob->GetZ() - GetZ();
	//Cheap ABS()
	if(t1 < 0)
		t1 = 0 - t1;
	if(t2 < 0)
		t2 = 0 - t2;
	if(t3 < 0)
		t3 = 0 - t3;
	if(( t1 > iAggroRange)
		|| ( t2 > iAggroRange)
		|| ( t3 > iAggroRange)
		||(mob->IsInvisible(this))
		|| (mob->IsClient() &&
			(!mob->CastToClient()->Connected()
				|| mob->CastToClient()->IsLD()
				|| mob->CastToClient()->IsBecomeNPC()
				|| mob->CastToClient()->GetGM()
			)
		))
	{
		return(false);
	}

	// Don't aggro new clients if we are already engaged unless PROX_AGGRO is set
	if(IsEngaged() && (!GetSpecialAbility(PROX_AGGRO) || (GetSpecialAbility(PROX_AGGRO) && !CombatRange(mob))))
	{
		Log.Out(Logs::Moderate, Logs::Aggro, "%s is in combat, and does not have prox_aggro, or does and is out of combat range with %s", GetName(), mob->GetName()); 
		return(false);
	}

	// Skip the owner if this is a pet.
	if(mob == GetOwner()) {
		return(false);
	}

	// Summoned pets are indifferent
	if((mob->IsClient() && mob->CastToClient()->has_zomm) || mob->iszomm || (mob->IsPet() && !mob->IsCharmed()))
	{
		Log.Out(Logs::Moderate, Logs::Aggro, "Zomm or Pet: Skipping aggro.");
		return(false);
	}

	float dist2 = DistanceSquared(mob->GetPosition(), m_Position);
	float iAggroRange2 = iAggroRange*iAggroRange;

	if( dist2 > iAggroRange2 ) {
		// Skip it, out of range
		return(false);
	}

	//Image: Get their current target and faction value now that its required
	//this function call should seem backwards
	FACTION_VALUE fv = mob->GetReverseFactionCon(this);

	// Make sure they're still in the zone
	// Are they in range?
	// Are they kos?
	// Are we stupid or are they green
	// and they don't have their gm flag on

	if (RuleB(Aggro, UseLevelAggro))
	{
		if
		(
		//old InZone check taken care of above by !mob->CastToClient()->Connected()
		(
			(GetLevel() >= 18)
			|| (GetBodyType() == 3)
			|| (CastToNPC()->IsAggroOnPC())
			|| (mob->IsClient() && mob->CastToClient()->IsSitting())
			|| (mob->GetLevelCon(GetLevel()) != CON_GREEN)
		)
		&&
		(
			(
				fv == FACTION_SCOWLS
				||
				(mob->GetPrimaryFaction() != GetPrimaryFaction() && mob->GetPrimaryFaction() == -4 && GetOwner() == nullptr)
				||
				(
					fv == FACTION_THREATENLY
					&& zone->random.Roll(THREATENLY_ARRGO_CHANCE)
				)
			)
		)
		)
		{
			//make sure we can see them. last since it is very expensive
			if (zone->SkipLoS() || CheckLosFN(mob)) {
				Log.Out(Logs::Moderate, Logs::Aggro, "Check aggro for %s target %s.", GetName(), mob->GetName());
				return(mod_will_aggro(mob, this));
			}
		}
	}
	else
	{
		if
		(
		//old InZone check taken care of above by !mob->CastToClient()->Connected()
			(
			(GetINT() <= RuleI(Aggro, IntAggroThreshold))
			|| (mob->IsClient() && mob->CastToClient()->IsSitting())
			|| (mob->GetLevelCon(GetLevel()) != CON_GREEN)
		)
		&&
		(
			(
				fv == FACTION_SCOWLS
				||
				(mob->GetPrimaryFaction() != GetPrimaryFaction() && mob->GetPrimaryFaction() == -4 && GetOwner() == nullptr)
				||
				(
					fv == FACTION_THREATENLY
					&& zone->random.Roll(THREATENLY_ARRGO_CHANCE)
				)
			)
		)
		)
		{
			//make sure we can see them. last since it is very expensive
			if (zone->SkipLoS() || CheckLosFN(mob)) {
				Log.Out(Logs::Moderate, Logs::Aggro, "Check aggro for %s target %s.", GetName(), mob->GetName());
				return(mod_will_aggro(mob, this));
			}
		}
	}

	Log.Out(Logs::Detail, Logs::Aggro, "Is In zone?:%d\n", mob->InZone());
	Log.Out(Logs::Detail, Logs::Aggro, "Dist^2: %f\n", dist2);
	Log.Out(Logs::Detail, Logs::Aggro, "Range^2: %f\n", iAggroRange2);
	Log.Out(Logs::Detail, Logs::Aggro, "Faction: %d\n", fv);
	Log.Out(Logs::Detail, Logs::Aggro, "Int: %d\n", GetINT());
	Log.Out(Logs::Detail, Logs::Aggro, "Con: %d\n", GetLevelCon(mob->GetLevel()));

	return(false);
}

// This is for npc_aggro npc->npc only.
Mob* EntityList::AICheckCloseAggro(Mob* sender, float iAggroRange, float iAssistRange) {
	if (!sender || !sender->IsNPC())
		return(nullptr);


	//npc->client is checked elsewhere, no need to check again
	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		Mob *mob = it->second;
		if (sender->CheckWillAggro(mob))
			return mob;
		++it;
	}
	//Log.Out(Logs::Detail, Logs::Debug, "Check aggro for %s no target.", sender->GetName());
	return nullptr;
}

int EntityList::GetHatedCount(Mob *attacker, Mob *exclude)
{
	// Return a list of how many non-feared, non-mezzed, non-green mobs, within aggro range, hate *attacker
	if (!attacker)
		return 0;

	int Count = 0;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *mob = it->second;
		if (!mob || (mob == exclude))
			continue;

		if (!mob->IsEngaged())
			continue;

		if (mob->IsFeared() || mob->IsMezzed())
			continue;

		if (attacker->GetLevelCon(mob->GetLevel()) == CON_GREEN)
			continue;

		if (!mob->CheckAggro(attacker))
			continue;

		float AggroRange = mob->GetAggroRange();

		// Square it because we will be using DistNoRoot

		AggroRange *= AggroRange;

		if (DistanceSquared(mob->GetPosition(), attacker->GetPosition()) > AggroRange)
			continue;

		Count++;
	}

	return Count;

}

int EntityList::GetHatedCountByFaction(Mob *attacker, Mob *exclude)
{
	// Return a list of how many non-feared, non-mezzed, non-green mobs, within aggro range, hate *attacker
	if (!attacker)
		return 0;

	int Count = 0;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *mob = it->second;
		if (!mob || (mob == exclude))
			continue;

		if (!mob->IsEngaged())
			continue;

		if (mob->IsFeared() || mob->IsMezzed())
			continue;

		//if (attacker->GetLevelCon(mob->GetLevel()) == CON_GREEN)
		//	continue;

		if (!mob->CheckAggro(attacker))
			continue;

		float AggroRange = mob->GetAggroRange();

		// Square it because we will be using DistNoRoot

		AggroRange *= AggroRange;

		if (DistanceSquared(mob->GetPosition(), attacker->GetPosition()) > AggroRange)
			continue;

		// If exclude doesn't have a faction, check for buddies based on race.
		if(exclude->GetPrimaryFaction() != 0)
		{
			if (mob->GetPrimaryFaction() != exclude->GetPrimaryFaction())
					continue;
		}
		else
		{
			if (mob->GetBaseRace() != exclude->GetBaseRace())
				continue;
		}

		Count++;
	}

	return Count;

}
void EntityList::AIYellForHelp(Mob* sender, Mob* attacker) {
	if(!sender || !attacker)
		return;
	if (sender->GetPrimaryFaction() == 0 )
		return; // well, if we dont have a faction set, we're gonna be indiff to everybody
	if(!sender->GetSpecialAbility(ALWAYS_CALL_HELP) && sender->HasAssistAggro())
		return;

	for (auto it = npc_list.begin(); it != npc_list.end(); ++it) {
		NPC *mob = it->second;
		if (!mob)
			continue;

		if(mob->CheckAggro(attacker))
			continue;

		//Check if we are over our assist aggro cap
		if (!sender->GetSpecialAbility(ALWAYS_CALL_HELP) && sender->NPCAssistCap() >= RuleI(Combat, NPCAssistCap))
			break;

		float r = mob->GetAssistRange();
		r = r * r;

		if (
			mob != sender
			&& mob != attacker
			&& mob->GetClass() != MERCHANT
//			&& !mob->IsCorpse()
//			&& mob->IsAIControlled()
			&& mob->GetPrimaryFaction() != 0
			&& DistanceSquared(mob->GetPosition(), sender->GetPosition()) <= r
			&& ((!mob->IsPet()) || (mob->IsPet() && mob->GetOwner() && !mob->GetOwner()->IsClient() && mob->GetOwner() == sender)) // If we're a pet we don't react to any calls for help if our owner is a client or if our owner was not the one calling for help.
			)
		{
			//if they are in range, make sure we are not green...
			//then jump in if they are our friend
			if(attacker->GetLevelCon(mob->GetLevel()) != CON_GREEN)
			{
				if (RuleB(AlKabor, AllowPetPull))
				{
					if (attacker->IsPet() && mob->GetLevelCon(attacker->GetLevel()) == CON_GREEN)
						return;
				}

				bool useprimfaction = false;
				if(mob->GetPrimaryFaction() == sender->CastToNPC()->GetPrimaryFaction())
				{
					const NPCFactionList *cf = database.GetNPCFactionEntry(mob->GetNPCFactionID());
					if(cf){
						if(cf->assistprimaryfaction != 0)
							useprimfaction = true;
					}
				}

				if(useprimfaction || sender->GetReverseFactionCon(mob) <= FACTION_AMIABLE )
				{
					//attacking someone on same faction, or a friend
					//make sure we can see them.
					if(zone->SkipLoS() || mob->CheckLosFN(sender)) 
					{

						Log.Out(Logs::Detail, Logs::Aggro, "AIYellForHelp(\"%s\",\"%s\") %s attacking %s Dist %f Z %f",
						sender->GetName(), attacker->GetName(), mob->GetName(), attacker->GetName(), DistanceSquared(mob->GetPosition(), sender->GetPosition()), std::abs(sender->GetZ()+mob->GetZ()));

						mob->AddToHateList(attacker, 20, 0, false);
						sender->AddAssistCap();
					}
				}
			}
		}
	}
}

/*
solar: returns false if attack should not be allowed
I try to list every type of conflict that's possible here, so it's easy
to see how the decision is made. Yea, it could be condensed and made
faster, but I'm doing it this way to make it readable and easy to modify
*/

bool Mob::IsAttackAllowed(Mob *target, bool isSpellAttack, int16 spellid)
{

	Mob *mob1, *mob2, *tempmob;
	Client *c1, *c2, *becomenpc;
//	NPC *npc1, *npc2;
	int reverse;

	if(!zone->CanDoCombat())
		return false;

	// some special cases
	if(!target)
		return false;

	if(this == target)	// you can attack yourself
		return true;

	//Lifetap on Eye of Zomm.
	if(target->IsNPC() && target->GetRace() == EYE_OF_ZOMM && IsLifetapSpell(spellid))
		return true;

	if(target->GetSpecialAbility(NO_HARM_FROM_CLIENT)){
		return false;
	}

	// Pets cant attack mezed mobs
	if(IsPet() && GetOwner()->IsClient() && target->IsMezzed()) {
		return false;
	}

	if(IsNPC() && target->IsNPC())
	{
		int32 npc_faction = CastToNPC()->GetPrimaryFaction();
		int32 target_faction = target->CastToNPC()->GetPrimaryFaction();
		if(npc_faction == target_faction && npc_faction != 0)
		{
			Log.Out(Logs::General, Logs::Combat, "IsAttackAllowed failed: %s is on the same faction as %s", GetName(), target->GetName());
			RemoveFromHateList(target);
			return false;
		}

	}

	// can't damage own pet (applies to everthing)
	Mob *target_owner = target->GetOwner();
	Mob *our_owner = GetOwner();
	if(target_owner && target_owner == this)
		return false;
	else if(our_owner && our_owner == target)
		return false;

	// invalidate for swarm pets for later on if their owner is a corpse
	if (IsNPC() && CastToNPC()->GetSwarmInfo() && our_owner &&
			our_owner->IsCorpse() && !our_owner->IsPlayerCorpse())
		our_owner = nullptr;
	if (target->IsNPC() && target->CastToNPC()->GetSwarmInfo() && target_owner &&
			target_owner->IsCorpse() && !target_owner->IsPlayerCorpse())
		target_owner = nullptr;

	//cannot hurt untargetable mobs
	bodyType bt = target->GetBodyType();

	if(bt == BT_NoTarget || bt == BT_NoTarget2) {
		if (RuleB(Pets, UnTargetableSwarmPet)) {
			if (target->IsNPC()) {
				if (!target->CastToNPC()->GetSwarmOwner()) {
					return(false);
				}
			} else {
				return(false);
			}
		} else {
			return(false);
		}
	}

	// solar: the format here is a matrix of mob type vs mob type.
	// redundant ones are omitted and the reverse is tried if it falls through.

	// first figure out if we're pets. we always look at the master's flags.
	// no need to compare pets to anything
	mob1 = our_owner ? our_owner : this;
	mob2 = target_owner ? target_owner : target;

	reverse = 0;
	do
	{
		if(_CLIENT(mob1))
		{
			if(_CLIENT(mob2))					// client vs client
			{
				c1 = mob1->CastToClient();
				c2 = mob2->CastToClient();

				if	// if both are pvp they can fight
				(
					c1->GetPVP() &&
					c2->GetPVP()
				)
					return true;
				else if	// if they're dueling they can go at it
				(
					c1->IsDueling() &&
					c2->IsDueling() &&
					c1->GetDuelTarget() == c2->GetID() &&
					c2->GetDuelTarget() == c1->GetID()
				)
					return true;
				else
					return false;
			}
			else if(_NPC(mob2))				// client vs npc
			{
				return true;
			}
			else if(_BECOMENPC(mob2))	// client vs becomenpc
			{
				c1 = mob1->CastToClient();
				becomenpc = mob2->CastToClient();

				if(c1->GetLevel() > becomenpc->GetBecomeNPCLevel())
					return false;
				else
					return true;
			}
			else if(_CLIENTCORPSE(mob2))	// client vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// client vs npc corpse
			{
				return false;
			}
		}
		else if(_NPC(mob1))
		{
			if(_NPC(mob2))						// npc vs npc
			{
/*
this says that an NPC can NEVER attack a faction ally...
this is stupid... somebody else should check this rule if they want to
enforce it, this just says 'can they possibly fight based on their
type', in which case, the answer is yes.
*/
/*				npc1 = mob1->CastToNPC();
				npc2 = mob2->CastToNPC();
				if
				(
					npc1->GetPrimaryFaction() != 0 &&
					npc2->GetPrimaryFaction() != 0 &&
					(
						npc1->GetPrimaryFaction() == npc2->GetPrimaryFaction() ||
						npc1->IsFactionListAlly(npc2->GetPrimaryFaction())
					)
				)
					return false;
				else
*/
					return true;
			}
			else if(_BECOMENPC(mob2))	// npc vs becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// npc vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// npc vs npc corpse
			{
				return false;
			}
		}
		else if(_BECOMENPC(mob1))
		{
			if(_BECOMENPC(mob2))			// becomenpc vs becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// becomenpc vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// becomenpc vs npc corpse
			{
				return false;
			}
		}
		else if(_CLIENTCORPSE(mob1))
		{
			if(_CLIENTCORPSE(mob2))		// client corpse vs client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// client corpse vs npc corpse
			{
				return false;
			}
		}
		else if(_NPCCORPSE(mob1))
		{
			if(_NPCCORPSE(mob2))			// npc corpse vs npc corpse
			{
				return false;
			}
		}

		// we fell through, now we swap the 2 mobs and run through again once more
		tempmob = mob1;
		mob1 = mob2;
		mob2 = tempmob;
	}
	while( reverse++ == 0 );

	Log.Out(Logs::General, Logs::Combat, "Mob::IsAttackAllowed: don't have a rule for this - %s vs %s\n", this->GetName(), target->GetName());
	return false;
}


// solar: this is to check if non detrimental things are allowed to be done
// to the target. clients cannot affect npcs and vice versa, and clients
// cannot affect other clients that are not of the same pvp flag as them.
// also goes for their pets
bool Mob::IsBeneficialAllowed(Mob *target)
{
	Mob *mob1, *mob2, *tempmob;
	Client *c1, *c2;
	int reverse;

	if(!target)
		return false;

	if (target->GetAllowBeneficial())
		return true;

	// solar: see IsAttackAllowed for notes

	// first figure out if we're pets. we always look at the master's flags.
	// no need to compare pets to anything
	mob1 = this->GetOwnerID() ? this->GetOwner() : this;
	mob2 = target->GetOwnerID() ? target->GetOwner() : target;

	// if it's self target or our own pet it's ok
	if(mob1 == mob2)
		return true;

	reverse = 0;
	do
	{
		if(_CLIENT(mob1))
		{
			if(_CLIENT(mob2))					// client to client
			{
				c1 = mob1->CastToClient();
				c2 = mob2->CastToClient();

				if(c1->GetPVP() == c2->GetPVP())
					return true;
				else if	// if they're dueling they can heal each other too
				(
					c1->IsDueling() &&
					c2->IsDueling() &&
					c1->GetDuelTarget() == c2->GetID() &&
					c2->GetDuelTarget() == c1->GetID()
				)
					return true;
				else
					return false;
			}
			else if(_NPC(mob2))				// client to npc
			{
				/* fall through and swap positions */
			}
			else if(_BECOMENPC(mob2))	// client to becomenpc
			{
				return false;
			}
			else if(_CLIENTCORPSE(mob2))	// client to client corpse
			{
				return true;
			}
			else if(_NPCCORPSE(mob2))	// client to npc corpse
			{
				return false;
			}
		}
		else if(_NPC(mob1))
		{
			if(_CLIENT(mob2))
			{
				return false;
			}
			if(_NPC(mob2))						// npc to npc
			{
				return true;
			}
			else if(_BECOMENPC(mob2))	// npc to becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// npc to client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// npc to npc corpse
			{
				return false;
			}
		}
		else if(_BECOMENPC(mob1))
		{
			if(_BECOMENPC(mob2))			// becomenpc to becomenpc
			{
				return true;
			}
			else if(_CLIENTCORPSE(mob2))	// becomenpc to client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// becomenpc to npc corpse
			{
				return false;
			}
		}
		else if(_CLIENTCORPSE(mob1))
		{
			if(_CLIENTCORPSE(mob2))		// client corpse to client corpse
			{
				return false;
			}
			else if(_NPCCORPSE(mob2))	// client corpse to npc corpse
			{
				return false;
			}
		}
		else if(_NPCCORPSE(mob1))
		{
			if(_NPCCORPSE(mob2))			// npc corpse to npc corpse
			{
				return false;
			}
		}

		// we fell through, now we swap the 2 mobs and run through again once more
		tempmob = mob1;
		mob1 = mob2;
		mob2 = tempmob;
	}
	while( reverse++ == 0 );

	Log.Out(Logs::General, Logs::Spells, "Mob::IsBeneficialAllowed: don't have a rule for this - %s to %s\n", this->GetName(), target->GetName());
	return false;
}

bool Mob::CombatRange(Mob* other)
{
	if(!other)
		return(false);

	float size_mod = GetSize();
	float other_size_mod = other->GetSize();

	// race 49 == 'Lava Dragon' (Lord Nagafen, Lady Vox, All VP dragons, Klandicar, etc)
	// race 158 == Wurms
	// race 196 == 'Ghost Dragon' (Jaled`Dar's shade)
	if(GetRace() == 49 || GetRace() == 158 || GetRace() == 196) //For races with a fixed size
		size_mod = 60.0f;
	else if (size_mod < 6.0)
		size_mod = 8.0f;

	if(other->GetRace() == 49 || other->GetRace() == 158 || other->GetRace() == 196) //For races with a fixed size
		other_size_mod = 60.0f;
	else if (other_size_mod < 6.0)
		other_size_mod = 8.0f;

	if (other_size_mod > size_mod)
	{
		size_mod = other_size_mod;
	}

	// this could still use some work, but for now it's an improvement....

	if (size_mod > 29)
		size_mod *= size_mod;
	else if (size_mod > 19)
		size_mod *= size_mod * 2;
	else
		size_mod *= size_mod * 4;


	// prevention of ridiculously sized hit boxes
	if (size_mod > 10000)
		size_mod = size_mod / 7;

	float _DistNoRoot = DistanceSquared(m_Position, other->GetPosition());

	if (GetSpecialAbility(NPC_CHASE_DISTANCE)){
		
		bool DoLoSCheck = true;
		float max_dist = static_cast<float>(GetSpecialAbilityParam(NPC_CHASE_DISTANCE, 0));
		float min_dist = static_cast<float>(GetSpecialAbilityParam(NPC_CHASE_DISTANCE, 1));
		
		if (GetSpecialAbilityParam(NPC_CHASE_DISTANCE, 2))
			DoLoSCheck = false; //Ignore line of sight check

		if (max_dist == 1)
			max_dist = 250.0f; //Default it to 250 if you forget to put a value

		max_dist = max_dist * max_dist;

		if (!min_dist)
			min_dist = size_mod; //Default to melee range
		else
			min_dist = min_dist * min_dist;
		
		if ((DoLoSCheck && CheckLastLosState()) && (_DistNoRoot >= min_dist && _DistNoRoot <= max_dist))
			SetPseudoRoot(true); 
		else 
			SetPseudoRoot(false);
	}

	if (_DistNoRoot <= size_mod)
	{
		return true;
	}
	return false;
}

//Father Nitwit's LOS code
bool Mob::CheckLosFN(Mob* other) {
	bool Result = false;

	if(other)
		Result = CheckLosFN(other->GetX(), other->GetY(), other->GetZ(), other->GetSize());

	SetLastLosState(Result);
	return Result;
}

bool Mob::CheckRegion(Mob* other, bool skipwater) {

	if (zone->watermap == nullptr) 
		return true;

	//Hack for kedge and powater since we have issues with watermaps.
	if(zone->IsWaterZone() && skipwater)
		return true;

	WaterRegionType ThisRegionType;
	WaterRegionType OtherRegionType;
	auto position = glm::vec3(GetX(), GetY(), GetZ());
	auto other_position = glm::vec3(other->GetX(), other->GetY(), other->GetZ());
	ThisRegionType = zone->watermap->ReturnRegionType(position);
	OtherRegionType = zone->watermap->ReturnRegionType(other_position);

	Log.Out(Logs::Moderate, Logs::Maps, "Caster Region: %d Other Region: %d", ThisRegionType, OtherRegionType);

	if(ThisRegionType == OtherRegionType || 
		(ThisRegionType == RegionTypeWater && OtherRegionType == RegionTypeVWater) ||
		(OtherRegionType == RegionTypeWater && ThisRegionType == RegionTypeVWater))
		return true;

	return false;
}

bool Mob::CheckLosFN(float posX, float posY, float posZ, float mobSize) {

	glm::vec3 myloc(GetX(), GetY(), GetZ());
	glm::vec3 oloc(posX, posY, posZ);
	float mybestz = myloc.z;
	float obestz = oloc.z;

	if(zone->zonemap == nullptr) {
		//not sure what the best return is on error
		//should make this a database variable, but im lazy today
#ifdef LOS_DEFAULT_CAN_SEE
		return(true);
#else
		return(false);
#endif
	}
	else
	{
		if((zone->watermap && !zone->watermap->InWater(myloc) && !zone->watermap->InVWater(myloc)
			&& !zone->watermap->InWater(oloc) && !zone->watermap->InVWater(oloc))
			|| !zone->watermap)
		{
			mybestz = zone->zonemap->FindBestZ(myloc, nullptr);
			obestz = zone->zonemap->FindBestZ(oloc, nullptr);
		}
	}

#define LOS_DEFAULT_HEIGHT 6.0f


	myloc.z = mybestz + (GetSize()==0.0?LOS_DEFAULT_HEIGHT:GetSize()) * HEAD_POSITION;
	oloc.z = obestz + (mobSize==0.0?LOS_DEFAULT_HEIGHT:mobSize) * SEE_POSITION;

	Log.Out(Logs::Detail, Logs::Maps, "LOS from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f) sizes: (%.2f, %.2f)", myloc.x, myloc.y, myloc.z, oloc.x, oloc.y, oloc.z, GetSize(), mobSize);
	return zone->zonemap->CheckLoS(myloc, oloc);
}

//offensive spell aggro
int32 Mob::CheckAggroAmount(uint16 spell_id, Mob* target, int32 &jolthate, bool isproc)
{
	int32 AggroAmount = 0;
	int32 nonModifiedAggro = 0;
	uint16 slevel = GetLevel();
	int32 damage = 0;

	// Spell hate for non-damaging spells scales by target NPC hitpoints
	int32 thp = 10000;
	if (target)
	{
		thp = target->GetMaxHP();
		if (thp < 375)
		{
			thp = 375;		// force a minimum of 25 hate
		}
	}
	int32 standardSpellHate = thp / 15;
	if (standardSpellHate > 1200)
	{
		standardSpellHate = 1200;
	}

	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		switch (spells[spell_id].effectid[o])
		{
			case SE_CurrentHPOnce:
			case SE_CurrentHP:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if(val < 0)
					damage -= val;
				break;
			}
			case SE_MovementSpeed:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					AggroAmount += standardSpellHate;
				break;
			}
			case SE_AttackSpeed:
			case SE_AttackSpeed2:
			case SE_AttackSpeed3:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 100)
					AggroAmount += standardSpellHate;
				break;
			}
			case SE_Stun:
			case SE_Blind:
			case SE_Mez:
			case SE_Charm:
			case SE_Fear:
			{
				AggroAmount += standardSpellHate;
				break;
			}
			case SE_ACv2:
			case SE_ArmorClass:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					AggroAmount += standardSpellHate;
				break;
			}
			case SE_DiseaseCounter:
			case SE_PoisonCounter:
			{
				AggroAmount += standardSpellHate / 3;		// old EQ counters had +hate.  this is a wild guess
				break;
			}
			case SE_Root:
			{
				AggroAmount += 10;
				break;
			}
			case SE_ResistMagic:
			case SE_ResistFire:
			case SE_ResistCold:
			case SE_ResistPoison:
			case SE_ResistDisease:
			case SE_STR:
			case SE_STA:
			case SE_DEX:
			case SE_AGI:
			case SE_INT:
			case SE_WIS:
			case SE_CHA:
			case SE_ATK:
			{
				// note: the enchanter tash line has a 'hate added' component which overrides this (Sony's data)
				// which is how it does not-trivial amounts of hate.  malo spells all do 40 hate
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					AggroAmount += 10;
				break;
			}
			case SE_ResistAll:
			{
				// SE_ResistAll with negative vals are only in NPC AoEs.  does not include malo* spells
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					AggroAmount += 50;
				break;
			}
			case SE_AllStats:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					AggroAmount += 70;
				break;
			}
			case SE_BardAEDot:
			{
				AggroAmount += 10;
				break;
			}
			case SE_SpinTarget:
			case SE_Amnesia:
			case SE_Silence:
			case SE_Destroy:
			{
				AggroAmount += standardSpellHate;
				break;
			}
			case SE_Harmony:
			case SE_CastingLevel:
			case SE_MeleeMitigation:
			case SE_CriticalHitChance:
			case SE_AvoidMeleeChance:
			case SE_RiposteChance:
			case SE_DodgeChance:
			case SE_ParryChance:
			case SE_DualWieldChance:
			case SE_DoubleAttackChance:
			case SE_MeleeSkillCheck:
			case SE_HitChance:
			case SE_IncreaseArchery:
			case SE_DamageModifier:
			case SE_MinDamageModifier:
			case SE_IncreaseBlockChance:
			case SE_Accuracy:
			case SE_DamageShield:
			case SE_SpellDamageShield:
			case SE_ReverseDS:
			{
				AggroAmount += slevel * 2;		// this is wrong, but don't know what to set these yet; most implemented after PoP
				break;
			}
			case SE_CurrentMana:
			case SE_ManaRegen_v2:
			case SE_ManaPool:
			case SE_CurrentEndurance: {
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val < 0)
					AggroAmount -= val * 2;		// also wrong
				break;
			}
			case SE_CancelMagic:
			case SE_DispelDetrimental: {
				AggroAmount += 1;
				break;
			}
			case SE_ReduceHate: {
				nonModifiedAggro = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				break;
			}
			case SE_InstantHate: {
				nonModifiedAggro = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);

				//Jolt type spells only.
				if((spells[spell_id].spell_category == 90 || spells[spell_id].spell_category == 99) && nonModifiedAggro < 0)
				{
					jolthate = nonModifiedAggro;
					nonModifiedAggro = 0;
					Log.Out(Logs::General, Logs::Spells, "Jolt type spell checking aggro. jolthate is %d hate is %d", jolthate, nonModifiedAggro);
				}
				break;
			}
		}
	}

	// procs and clickables capped at 400
	// damage aggro is still added later, so procs like the SoD's anarchy still do more than 400
	if (isproc && AggroAmount > 400)
		AggroAmount = 400;

	if (GetClass() == BARD)
	{
		if (damage > 0)
		{
			AggroAmount = damage;
		}
		else if (AggroAmount > 40)
		{
			if (target)
			{
				if (target->GetLevel() >= 20)
				{
					AggroAmount = 40;
				}
			}
			else if (slevel >= 20)
			{
				AggroAmount = 40;
			}
		}
	}
	else
	{
		AggroAmount += damage;
	}

	if (spells[spell_id].HateAdded > 0)
		AggroAmount = spells[spell_id].HateAdded;

	if (GetOwner() && IsPet())
		AggroAmount = AggroAmount * RuleI(Aggro, PetSpellAggroMod) / 100;

	if (AggroAmount > 0)
	{
		int HateMod = RuleI(Aggro, SpellAggroMod);

		if (IsClient())
			HateMod += CastToClient()->GetFocusEffect(focusSpellHateMod, spell_id);

		//Live AA - Spell casting subtlety
		HateMod += aabonuses.hatemod + spellbonuses.hatemod + itembonuses.hatemod;

		AggroAmount = (AggroAmount * HateMod) / 100;
	}

	return AggroAmount + spells[spell_id].bonushate + nonModifiedAggro;
}

//healing and buffing aggro
int32 Mob::CheckHealAggroAmount(uint16 spell_id, Mob* target, uint32 heal_possible)
{
	int32 AggroAmount = 0;
	uint8 slevel = GetLevel();
	uint8 tlevel = slevel;
	if (target)
		tlevel = target->GetLevel();

	for (int o = 0; o < EFFECT_COUNT; o++)
	{
		switch (spells[spell_id].effectid[o])
		{
			case SE_CurrentHP:
			{
				int val = CalcSpellEffectValue_formula(spells[spell_id].formula[o], spells[spell_id].base[o], spells[spell_id].max[o], slevel, spell_id);
				if (val > 0)
				{
					if (heal_possible < val)
						val = heal_possible;		// aggro is based on amount healed, not including crits/focii/AA multipliers

					if (val > 0)
						val = 1 + 2 * val / 3;		// heal aggro is 2/3rds amount healed

					if (tlevel <= 50 && val > 800)	// heal aggro is capped.  800 was stated in a patch note
						val = 800;
					else if (val > 1500)			// cap after level 50 is 1500 on EQLive as of 2015
						val = 1500;

					AggroAmount += val;
				}
				break;
			}
			case SE_Rune:
			{
				AggroAmount += CalcSpellEffectValue_formula(spells[spell_id].formula[0], spells[spell_id].base[0], spells[spell_id].max[o], slevel, spell_id) * 2;
				break;
			}
			case SE_HealOverTime:
			{
				AggroAmount += 10;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	if (AggroAmount < 9 && IsBuffSpell(spell_id))
	{
		if (IsBardSong(spell_id) && AggroAmount < 2)
		{
			AggroAmount = 2;
		}
		else
		{
			AggroAmount = 9;
		}
	}

	if (GetOwner() && IsPet())
		AggroAmount = AggroAmount * RuleI(Aggro, PetSpellAggroMod) / 100;

	if (AggroAmount > 0)
	{
		int HateMod = RuleI(Aggro, SpellAggroMod);

		if (IsClient())
			HateMod += CastToClient()->GetFocusEffect(focusSpellHateMod, spell_id);

		//Live AA - Spell casting subtlety
		HateMod += aabonuses.hatemod + spellbonuses.hatemod + itembonuses.hatemod;

		AggroAmount = (AggroAmount * HateMod) / 100;
	}

	if (AggroAmount < 0)
		return 0;
	else
		return AggroAmount;
}

void Mob::AddFeignMemory(Client* attacker) {
	if(feign_memory_list.empty() && AIfeignremember_timer != nullptr)
		AIfeignremember_timer->Start(AIfeignremember_delay);
	feign_memory_list.insert(attacker->CharacterID());
}

void Mob::RemoveFromFeignMemory(Client* attacker) {
	feign_memory_list.erase(attacker->CharacterID());
	if(feign_memory_list.empty() && AIfeignremember_timer != nullptr)
		AIfeignremember_timer->Disable();
	if(feign_memory_list.empty())
	{
		minLastFightingDelayMoving = RuleI(NPC, LastFightingDelayMovingMin);
		maxLastFightingDelayMoving = RuleI(NPC, LastFightingDelayMovingMax);
		if(AIfeignremember_timer != nullptr)
			AIfeignremember_timer->Disable();
	}
}

void Mob::ClearFeignMemory() {
	std::set<uint32>::iterator RememberedCharID = feign_memory_list.begin();
	while (RememberedCharID != feign_memory_list.end())
	{
		Client* remember_client = entity_list.GetClientByCharID(*RememberedCharID);
		++RememberedCharID;
	}

	feign_memory_list.clear();
	minLastFightingDelayMoving = RuleI(NPC, LastFightingDelayMovingMin);
	maxLastFightingDelayMoving = RuleI(NPC, LastFightingDelayMovingMax);
	if(AIfeignremember_timer != nullptr)
		AIfeignremember_timer->Disable();
}

bool Mob::PassCharismaCheck(Mob* caster, uint16 spell_id) {

	/*
	Charm formula is correct based on over 50 hours of personal live parsing - Kayen
	Charisma ONLY effects the initial resist check when charm is cast with 10 CHA = -1 Resist mod up to 255 CHA (min ~ 75 CHA)
	Charisma DOES NOT extend charm durations.
	Base effect value of charm spells in the spell file DOES NOT effect duration OR resist rate (unclear if does anything)
	Charm has a lower limit of 5% chance to break per tick, regardless of resist modifiers / level difference.
	*/

	if(!caster) return false;

	if(spells[spell_id].ResistDiff <= -600)
		return true;

	float resist_check = 0;
	
	if(IsCharmSpell(spell_id)) {

		if (spells[spell_id].powerful_flag == -1) //If charm spell has this set(-1), it can not break till end of duration.
			return true;

		//1: The mob has a default 25% chance of being allowed a resistance check against the charm.
		if (zone->random.Int(0, 99) > RuleI(Spells, CharmBreakCheckChance))
			return true;

		if (RuleB(Spells, CharismaCharmDuration))
			resist_check = ResistSpell(spells[spell_id].resisttype, spell_id, caster,false,0,true,true);
		else
			resist_check = ResistSpell(spells[spell_id].resisttype, spell_id, caster, false,0, false, true);

		//2: The mob makes a resistance check against the charm
		if (resist_check == 100) 
			return true;

		else
		{
			if (caster->IsClient())
			{
				//3: At maxed ability, Total Domination has a 50% chance of preventing the charm break that otherwise would have occurred.
				int16 TotalDominationBonus = caster->aabonuses.CharmBreakChance + caster->spellbonuses.CharmBreakChance + caster->itembonuses.CharmBreakChance;

				if (zone->random.Int(0, 99) < TotalDominationBonus)
					return true;

			}
		}
	}

	else
	{
		// Assume this is a harmony/pacify spell

		/*	Data this is based on:

			Level 50 Enchanter with 62 Charisma casting Pacify on Test Fifty Five:
			2428 casts; 2111 hits; 317 resists (13.05%)
			Aggros on resists: 241 (76% of resists, 9.9% of casts)

			Level 50 Enchanter with 115 Charisma casting Pacify on Test Fifty Five:
			671 casts; 585 hits; 86 resists (12.8%)
			Aggros on resists: 53 (61.3% of resists, 7.9% of casts)

			Level 50 Enchanter with 159 Charisma casting Pacify on Test Fifty:
			1482 casts; 1365 hits; 117 resists (7.89%)
			Aggros on resists: 52 (44.44% of resists, 3.5% of casts)

			Level 50 Enchanter with 200 Charisma casting Pacify on Test Fifty Five:
			1496 casts; 1294 hits; 202 resists (13.5%)
			Aggros on resists: 80 (39.6% of resists, 5.3% of casts)

			Level 50 Enchanter with 255 Charisma casting Pacify on Test Fifty:
			1865 casts; 1722 hits; 143 resists (7.66%)
			Aggros on resists: 36 (25.17% of resists, 1.93% of casts)

			Level 65 Enchanter with 305 Charisma casting Pacification on Test Sixty Five:
			4833 casts; 4463 hits; 370 resists (7.66%)
			Aggros on resists: 51 (13.78% of resists, 1.06% of casts)

			Dev comment stated that this check is entirely charisma based.  Logs confirm.
		*/
		int aggroChance = 93 - caster->GetCHA() * 100 / 375;
		if (aggroChance < 15)
			aggroChance = 15;
		
		if (!zone->random.Roll(aggroChance))
			return true;
	}

	return false;
}

void Mob::RogueEvade(Mob *other)
{
	int amount = other->GetHateAmount(this) - (GetLevel() * 13);
	other->SetHate(this, std::max(1, amount));

	return;
}

