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

#include "client.h"
#include "entity.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"

#include "../common/rulesys.h"

#include "hate_list.h"
#include "quest_parser_collection.h"
#include "zone.h"
#include "water_map.h"

#include <stdlib.h>
#include <list>

extern Zone *zone;

HateList::HateList()
{
	owner = nullptr;
	combatRangeBonus = 0;
	sitInsideBonus = 0;
	sitOutsideBonus = 0;
	lowHealthBonus = 0;
	rememberDistantMobs = false;
	nobodyInMeleeRange = false;
}

HateList::~HateList()
{
}

void HateList::SetOwner(Mob *newOwner)
{
	owner = newOwner;

	// see http://www.eqemulator.org/forums/showthread.php?t=39819
	// for the data this came from.  This is accurate to EQ Live as of 2015

	int32 hitpoints = owner->GetMaxHP();

	// melee range hate bonus
	combatRangeBonus = 100;
	if (hitpoints > 60000)
	{
		combatRangeBonus = 250 + hitpoints / 100;
		if (combatRangeBonus > 2250)
		{
			combatRangeBonus = 2250;
		}
	}
	else if (hitpoints > 4000)
	{
		combatRangeBonus = (hitpoints - 4000) / 75 + 100;
	}

	// inside melee range sitting hate bonus
	sitInsideBonus = 15;
	if (hitpoints > 13500)
	{
		sitInsideBonus = -hitpoints / 100 + 1000;
		if (sitInsideBonus < 0)
		{
			sitInsideBonus = 0;
		}
	}
	else if (hitpoints > 225)
	{
		sitInsideBonus = hitpoints / 15;
	}

	// ouside melee range sitting hate bonus
	sitOutsideBonus = 15;
	if (hitpoints > 225)
	{
		sitOutsideBonus = hitpoints / 15;
		if (sitOutsideBonus > 1000)
		{
			sitOutsideBonus = 1000;
		}
	}

	// low health hate bonus
	lowHealthBonus = hitpoints;
	if (lowHealthBonus < 500)
	{
		lowHealthBonus = 500;
	}
	else if (lowHealthBonus > 10000)
	{
		lowHealthBonus = 10000;
	}
}

// added for frenzy support
// checks if target still is in frenzy mode
void HateList::CheckFrenzyHate()
{
	auto iterator = list.begin();
	while(iterator != list.end())
	{
		if ((*iterator)->ent->GetHPRatio() >= 20)
			(*iterator)->bFrenzy = false;
		++iterator;
	}
}

void HateList::Wipe()
{
	auto iterator = list.begin();

	while(iterator != list.end())
	{
		Mob* m = (*iterator)->ent;
		if(m)
		{
			parse->EventNPC(EVENT_HATE_LIST, owner->CastToNPC(), m, "0", 0);

			if(m->IsClient())
				m->CastToClient()->DecrementAggroCount();
		}
		delete (*iterator);
		iterator = list.erase(iterator);

	}
}

bool HateList::IsOnHateList(Mob *mob)
{
	if(Find(mob))
		return true;
	return false;
}

tHateEntry *HateList::Find(Mob *ent)
{
	auto iterator = list.begin();
	while(iterator != list.end())
	{
		if((*iterator)->ent == ent)
			return (*iterator);
		++iterator;
	}
	return nullptr;
}

void HateList::Set(Mob* other, uint32 in_hate, uint32 in_dam)
{
	tHateEntry *p = Find(other);
	if(p)
	{
		if(in_dam > 0)
			p->damage = in_dam;

		if(in_hate > 0)
			p->hate = in_hate;
	}
	else
	{
		Add(other, in_hate, in_dam);
	}
}

Mob* HateList::GetDamageTop(Mob* hater)
{
	Mob* current = nullptr;
	Group* grp = nullptr;
	Raid* r = nullptr;
	uint32 dmg_amt = 0;

	auto iterator = list.begin();
	while(iterator != list.end())
	{
		grp = nullptr;
		r = nullptr;

		if((*iterator)->ent && (*iterator)->ent->IsClient()){
			r = entity_list.GetRaidByClient((*iterator)->ent->CastToClient());
		}

		grp = entity_list.GetGroupByMob((*iterator)->ent);

		if((*iterator)->ent && r){
			if(r->GetTotalRaidDamage(hater) >= dmg_amt)
			{
				current = (*iterator)->ent;
				dmg_amt = r->GetTotalRaidDamage(hater);
			}
		}
		else if ((*iterator)->ent != nullptr && grp != nullptr)
		{
			if (grp->GetTotalGroupDamage(hater) >= dmg_amt)
			{
				current = (*iterator)->ent;
				dmg_amt = grp->GetTotalGroupDamage(hater);
			}
		}
		else if ((*iterator)->ent != nullptr && (uint32)(*iterator)->damage >= dmg_amt)
		{
			current = (*iterator)->ent;
			dmg_amt = (*iterator)->damage;
		}
		++iterator;
	}
	return current;
}

Mob* HateList::GetClosest(Mob *hater)
{
	Mob* close_entity = nullptr;
	float close_distance = 99999.9f;
	float this_distance;

	if (!hater && owner)
		hater = owner;

	if (!hater)
		return nullptr;

	auto iterator = list.begin();
	while(iterator != list.end())
	{
		this_distance = DistanceSquaredNoZ((*iterator)->ent->GetPosition(), hater->GetPosition());

		if((*iterator)->ent != nullptr && this_distance <= close_distance)
		{
			close_distance = this_distance;
			close_entity = (*iterator)->ent;
		}
		++iterator;
	}

	if ((!close_entity && hater->IsNPC()) || (close_entity && close_entity->DivineAura()))
		close_entity = hater->CastToNPC()->GetHateTop();

	return close_entity;
}


// a few comments added, rearranged code for readability
void HateList::Add(Mob *ent, int32 in_hate, int32 in_dam, bool bFrenzy, bool iAddIfNotExist)
{
	if(!ent || ent == owner)
		return;

	if(ent->IsCorpse())
		return;

	if(ent->IsClient() && ent->CastToClient()->IsDead())
		return;

	tHateEntry *p = Find(ent);
	if (p)
	{
		p->damage+=(in_dam>=0)?in_dam:0;
		p->hate += in_hate;
		p->bFrenzy = bFrenzy;
	}
	else if (iAddIfNotExist)
	{
		// the first-to-aggro hate on animals does nearly no hate, regardless of what it would normally be
		if (owner->IsNPC() && owner->CastToNPC()->IsAnimal())
			in_hate = 1;

		p = new tHateEntry;
		p->ent = ent;
		p->damage = (in_dam>=0)?in_dam:0;
		p->hate = in_hate;
		p->bFrenzy = bFrenzy;
		list.push_back(p);
		parse->EventNPC(EVENT_HATE_LIST, owner->CastToNPC(), ent, "1", 0);

		if (ent->IsClient()) {
			if (owner->CastToNPC()->IsRaidTarget())
				ent->CastToClient()->SetEngagedRaidTarget(true);
			ent->CastToClient()->IncrementAggroCount();
		}

	}

	if (p)
	{
		p->timer.Start(600000);
	}
}

bool HateList::RemoveEnt(Mob *ent)
{
	if (!ent)
		return false;

	bool found = false;
	auto iterator = list.begin();

	while(iterator != list.end())
	{
		if((*iterator)->ent == ent)
		{
			if(ent)
			parse->EventNPC(EVENT_HATE_LIST, owner->CastToNPC(), ent, "0", 0);
			found = true;


			if(ent && ent->IsClient())
				ent->CastToClient()->DecrementAggroCount();

			delete (*iterator);
			iterator = list.erase(iterator);

		}
		else
			++iterator;
	}
	return found;
}

void HateList::DoFactionHits(int32 nfl_id) {
	if (nfl_id <= 0)
		return;
	auto iterator = list.begin();
	while(iterator != list.end())
	{
		Client *p;

		if ((*iterator)->ent && (*iterator)->ent->IsClient())
			p = (*iterator)->ent->CastToClient();
		else
			p = nullptr;

		if (p)
			p->SetFactionLevel(p->CharacterID(), nfl_id, p->GetBaseClass(), p->GetBaseRace(), p->GetDeity());
		++iterator;
	}
}

int HateList::SummonedPetCount(Mob *hater) {

	//Function to get number of 'Summoned' pets on a targets hate list to allow calculations for certian spell effects.
	//Unclear from description that pets are required to be 'summoned body type'. Will not require at this time.
	int petcount = 0;
	auto iterator = list.begin();
	while(iterator != list.end()) {

		if((*iterator)->ent != nullptr && (*iterator)->ent->IsNPC() && 	((*iterator)->ent->CastToNPC()->IsPet() || ((*iterator)->ent->CastToNPC()->GetSwarmOwner() > 0)))
		{
			++petcount;
		}

		++iterator;
	}

	return petcount;
}

int32 HateList::GetHateBonus(tHateEntry *entry, bool combatRange, bool firstInRange, float distSquared)
{
	int32 bonus = 0;

	if (combatRange)
	{
		bonus += combatRangeBonus;
		if (firstInRange)
		{
			bonus += 35;
		}
	}

	if (entry->ent->IsClient() && entry->ent->CastToClient()->IsSitting())
	{
		if (combatRange)
		{
			bonus += sitInsideBonus;
		}
		else
		{
			bonus += sitOutsideBonus;
		}
	}

	if (entry->bFrenzy || entry->ent->GetMaxHP() != 0 && ((entry->ent->GetHP() * 100 / entry->ent->GetMaxHP()) < 20))
	{
		if (distSquared == -1.0f)
		{
			float distX = entry->ent->GetX() - owner->GetX();
			float distY = entry->ent->GetY() - owner->GetY();
			distSquared = distX * distX + distY * distY;
		}

		if (distSquared > 10000.0f)
		{
			float dist = sqrtf(distSquared);

			// taper bonus after 100 distance to target
			bonus += lowHealthBonus * (100.0f / dist);
		}
		else
		{
			bonus += lowHealthBonus;
		}
	}

	// if nobody in melee range but entry is nearby, apply a bonus that scales with distance to target
	// this has the effect of the melee range bonus tapering off gradually over 100 distance
	if (nobodyInMeleeRange && list.size() > 1)
	{
		if (distSquared == -1.0f)
		{
			float distX = entry->ent->GetX() - owner->GetX();
			float distY = entry->ent->GetY() - owner->GetY();
			distSquared = distX * distX + distY * distY;
		}

		if (distSquared < 10000.0f)
		{
			float dist = sqrtf(distSquared);
			if (dist < 100 && dist > 0)
			{
				bonus += combatRangeBonus * static_cast<int32>(100.0f - dist) / 100;
			}
		}
	}
	return bonus;
}

Mob *HateList::GetTop()
{
	Mob* topMob = nullptr;
	int32 topHate = -1;
	bool somebodyInMeleeRange = false;

	if(owner == nullptr)
		return nullptr;

	if (RuleB(Aggro,SmartAggroList))
	{
		Mob* topClientTypeInRange = nullptr;
		int32 hateClientTypeInRange = -1;
		int skipped_count = 0;
		bool firstInRangeBonusApplied = false;

		auto iterator = list.begin();
		while (iterator != list.end())
		{
			tHateEntry *cur = (*iterator);

			// remove mobs that have not added hate in 10 minutes
			if (cur->timer.Check())
			{
				RemoveEnt(cur->ent);

				if (list.size() == 0)
					return nullptr;

				iterator = list.begin();
				skipped_count = 0;
				firstInRangeBonusApplied = false;
				continue;
			}

			if (!cur || !cur->ent)
			{
				++iterator;
				continue;
			}

			auto hateEntryPosition = glm::vec3(cur->ent->GetX(), cur->ent->GetY(), cur->ent->GetZ());

			if (owner->IsNPC() && owner->CastToNPC()->IsUnderwaterOnly() && zone->HasWaterMap())
			{
				if (!zone->watermap->InLiquid(hateEntryPosition)) {
					skipped_count++;
					++iterator;
					continue;
				}
			}

			float distX = hateEntryPosition.x - owner->GetX();
			float distY = hateEntryPosition.y - owner->GetY();
			float distSquared = distX * distX + distY * distY;

			float ignoreDistance = 0.0f;
			if(owner->IsNPC())
				ignoreDistance = owner->CastToNPC()->GetIgnoreDistance();

			// ignore players farther away than distance specified in the database.
			if (!rememberDistantMobs && ignoreDistance > 0 && distSquared > ignoreDistance*ignoreDistance
				// exception for damaged summoning NPCs
				&& (!owner->GetSpecialAbility(SPECATK_SUMMON) || owner->GetHPRatio() > 90))
			{
				++iterator;
				continue;
			}

			if (cur->ent->Sanctuary())
			{
				if (topHate == -1)
				{
					topMob = cur->ent;
					topHate = 1;
				}
				++iterator;
				continue;
			}

			if (cur->ent->DivineAura() || cur->ent->IsMezzed() || (cur->ent->IsFeared() && !cur->ent->IsFleeing()))
			{
				if (topHate == -1)
				{
					topMob = cur->ent;
					topHate = 0;
				}
				++iterator;
				continue;
			}

			bool combatRange = owner->CombatRange(cur->ent);
			if (combatRange)
			{
				somebodyInMeleeRange = true;
			}

			int32 currentHate = cur->hate + GetHateBonus(cur, combatRange, !firstInRangeBonusApplied, distSquared);

			if (!firstInRangeBonusApplied && combatRange)
			{
				firstInRangeBonusApplied = true;
			}

			// favor targets inside 600 distance
			if (distSquared > 360000.0f)
			{
				currentHate = 0;
			}

			if (cur->ent->IsClient())
			{
				if (currentHate > hateClientTypeInRange && combatRange)
				{
					hateClientTypeInRange = currentHate;
					topClientTypeInRange = cur->ent;
				}
			}
			else if (cur->ent->IsPet() && cur->ent->IsCharmed() && !RuleB(AlKabor, AllowCharmPetRaidTanks))
			{
				// this makes NPCs ignore charmed pets if more than X players+pets get on the hate list
				if (list.size() > RuleI(AlKabor, MaxEntitiesCharmTanks))
					currentHate = 0;
			}

			if (currentHate > topHate)
			{
				topHate = currentHate;
				topMob = cur->ent;
			}

			++iterator;
		}
		nobodyInMeleeRange = !somebodyInMeleeRange;

		// this is mostly to make sure NPCs attack players instead of pets in melee range
		if (topClientTypeInRange != nullptr && topMob != nullptr)
		{
			bool isTopClientType = topMob->IsClient();

			if (!isTopClientType)
			{
				if (topMob->GetSpecialAbility(ALLOW_TO_TANK))
				{
					isTopClientType = true;
					topClientTypeInRange = topMob;
				}
			}

			if(!isTopClientType)
				return topClientTypeInRange ? topClientTypeInRange : nullptr;

			return topMob ? topMob : nullptr;
		}
		else
		{
			if (topMob == nullptr && skipped_count > 0)
			{
				return owner->GetTarget() ? owner->GetTarget() : nullptr;
			}
			return topMob ? topMob : nullptr;
		}
	}	// if (RuleB(Aggro,SmartAggroList))
	else
	{
		auto iterator = list.begin();
		int skipped_count = 0;
		while(iterator != list.end())
		{
			tHateEntry *cur = (*iterator);
 			if(owner->IsNPC() && owner->CastToNPC()->IsUnderwaterOnly() && zone->HasWaterMap())
			{
				if(!zone->watermap->InLiquid(glm::vec3(cur->ent->GetPosition())))
				{
					skipped_count++;
					++iterator;
					continue;
				}
			}

			if (cur->ent != nullptr && ((cur->hate > topHate) || cur->bFrenzy))
			{
				topMob = cur->ent;
				topHate = cur->hate;
			}
			++iterator;
		}
		if (topMob == nullptr && skipped_count > 0)
		{
			return owner->GetTarget() ? owner->GetTarget() : nullptr;
		}
		return topMob ? topMob : nullptr;
	}
	return nullptr;
}

Mob *HateList::GetMostHate(bool includeBonus)
{
	Mob* topMob = nullptr;
	int32 topHate = -1;
	bool firstInRangeBonusApplied = false;

	auto iterator = list.begin();
	while(iterator != list.end())
	{
		tHateEntry *cur = (*iterator);
		int32 bonus = 0;

		if (includeBonus)
		{
			bool combatRange = owner->CombatRange(cur->ent);

			bonus = GetHateBonus(cur, combatRange, !firstInRangeBonusApplied);

			if (!firstInRangeBonusApplied && combatRange)
			{
				firstInRangeBonusApplied = true;
			}
		}

		if(cur->ent != nullptr && ((cur->hate + bonus) > topHate))
		{
			topMob = cur->ent;
			topHate = cur->hate + bonus;
		}
		++iterator;
	}
	return topMob;
}


Mob *HateList::GetRandom()
{
	int count = list.size();
	if(count == 0) //If we don't have any entries it'll crash getting a random 0, -1 position.
		return nullptr;

	if(count == 1) //No need to do all that extra work if we only have one hate entry
	{
		if(*list.begin()) // Just in case tHateEntry is invalidated somehow...
			return (*list.begin())->ent;

		return nullptr;
	}

	auto iterator = list.begin();
	int random = zone->random.Int(0, count - 1);
	for (int i = 0; i < random; i++)
		++iterator;

	return (*iterator)->ent;
}

int32 HateList::GetEntHate(Mob *ent, bool damage, bool includeBonus)
{
	bool firstInRangeBonusApplied = false;
	bool combatRange;

	auto iterator = list.begin();
	while (iterator != list.end())
	{
		tHateEntry *p = (*iterator);

		if (!p)
		{
			++iterator;
			continue;
		}
		
		if (!damage && includeBonus)
		{
			combatRange = owner->CombatRange(p->ent);

			if (!firstInRangeBonusApplied && combatRange)
			{
				firstInRangeBonusApplied = true;
			}
		}

		if (p->ent == ent)
		{
			if (damage)
			{
				return p->damage;
			}

			if (includeBonus)
			{
				return (p->hate + GetHateBonus(p, combatRange, !firstInRangeBonusApplied));
			}
			else
			{
				return p->hate;
			}
		}
		++iterator;
	}
	return 0;
}

//looking for any mob with hate > -1
bool HateList::IsEmpty() {
	return(list.size() == 0);
}

// Prints hate list to a client
void HateList::PrintToClient(Client *c)
{
	int32 bonusHate = 0;
	auto iterator = list.begin();
	bool firstInRangeBonusApplied = false;

	while (iterator != list.end())
	{
		tHateEntry *e = (*iterator);
		uint32 timer = e->timer.GetDuration() - e->timer.GetRemainingTime();
		if (timer > 0)
		{
			timer /= 1000;
		}
		bool combatRange = owner->CombatRange(e->ent);

		bonusHate = GetHateBonus(e, combatRange, !firstInRangeBonusApplied);
		if (!firstInRangeBonusApplied && combatRange)
		{
			firstInRangeBonusApplied = true;
		}

		if (bonusHate > 0)
		{
			c->Message(CC_Default, "- name: %s, timer: %i, damage: %d, hate: %d (+%i)",
				(e->ent && e->ent->GetName()) ? e->ent->GetName() : "(null)",
				timer, e->damage, e->hate, bonusHate);
		}
		else
		{
			c->Message(CC_Default, "- name: %s, timer: %i, damage: %d, hate: %d",
				(e->ent && e->ent->GetName()) ? e->ent->GetName() : "(null)",
				timer, e->damage, e->hate);
		}

		++iterator;
	}
}

int HateList::AreaRampage(Mob *caster, Mob *target, int count, ExtraAttackOptions *opts)
{
	if(!target || !caster)
		return 0;

	int targetsHit = 0;
	auto iterator = list.begin();
	tHateEntry* h = nullptr;

	while (iterator != list.end() && targetsHit < count)
	{
		h = (*iterator);
		if (h && h->ent && h->ent != caster)
		{
			if (caster->CombatRange(h->ent))
			{
				caster->DoMainHandRound(h->ent, opts);
				caster->DoOffHandRound(h->ent, opts);

				++targetsHit;
			}
		}
		++iterator;
	}

	return targetsHit;
}

void HateList::SpellCast(Mob *caster, uint32 spell_id, float range, Mob* ae_center)
{
	if(!caster)
		return;

	Mob* center = caster;

	if (ae_center)
		center = ae_center;

	//this is slower than just iterating through the list but avoids
	//crashes when people kick the bucket in the middle of this call
	//that invalidates our iterator but there's no way to know sadly
	//So keep a list of entity ids and look up after
	std::list<uint32> id_list;
	range = range * range;
	float min_range2 = spells[spell_id].min_range * spells[spell_id].min_range;
	float dist_targ = 0;
	auto iterator = list.begin();
	while (iterator != list.end())
	{
		tHateEntry *h = (*iterator);
		if(range > 0)
		{
			dist_targ = DistanceSquared(center->GetPosition(), h->ent->GetPosition());
			if (dist_targ <= range && dist_targ >= min_range2)
			{
				id_list.push_back(h->ent->GetID());
				h->ent->CalcSpellPowerDistanceMod(spell_id, dist_targ);
			}
		}
		else
		{
			id_list.push_back(h->ent->GetID());
			h->ent->CalcSpellPowerDistanceMod(spell_id, 0, caster);
		}
		++iterator;
	}

	std::list<uint32>::iterator iter = id_list.begin();
	while(iter != id_list.end())
	{
		Mob *cur = entity_list.GetMobID((*iter));
		if(cur)
		{
			caster->SpellOnTarget(spell_id, cur);
		}
		iter++;
	}
}
