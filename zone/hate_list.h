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

#ifndef HATELIST_H
#define HATELIST_H

class Client;
class Group;
class Mob;
class Raid;
struct ExtraAttackOptions;

struct tHateEntry
{
	Mob *ent;
	int32 damage, hate;
	bool bFrenzy;
	Timer timer;
};

class HateList
{
public:
	HateList();
	~HateList();

	// adds a mob to the hatelist
	void Add(Mob *ent, int32 in_hate=0, int32 in_dam=0, bool bFrenzy = false, bool iAddIfNotExist = true);
	// sets existing hate
	void Set(Mob *other, uint32 in_hate, uint32 in_dam);
	// removes mobs from hatelist
	bool RemoveEnt(Mob *ent);
	// Remove all
	void Wipe();
	// ???
	void DoFactionHits(int32 nfl_id);
	// Gets Hate amount for mob
	int32 GetEntHate(Mob *ent, bool damage = false, bool includeBonus = true);
	// gets top hated mob
	Mob *GetTop();
	// gets any on the list
	Mob *GetRandom();
	// get closest mob or nullptr if list empty
	Mob *GetClosest(Mob *hater = nullptr);
	// gets top mob or nullptr if hate list empty
	Mob *GetDamageTop(Mob *hater);
	// used to check if mob is on hatelist
	bool IsOnHateList(Mob *);
	// used to remove or add frenzy hate
	void CheckFrenzyHate();
	//Gets the target with the most hate regardless of things like frenzy etc.
	Mob* GetMostHate(bool includeBonus = true);
	// Count 'Summoned' pets on hatelist
	int SummonedPetCount(Mob *hater);
	// setting this true will allow the NPC to persue targets outside the 600 distance limit
	void SetRememberDistantMobs(bool state) { rememberDistantMobs = state; }

	int AreaRampage(Mob *caster, Mob *target, int count, ExtraAttackOptions *opts);

	void SpellCast(Mob *caster, uint32 spell_id, float range, Mob *ae_center  = nullptr);

	bool IsEmpty();
	void PrintToClient(Client *c);

	//For accessing the hate list via perl; don't use for anything else
	std::list<tHateEntry*>& GetHateList() { return list; }

	//setting owner
	void SetOwner(Mob *newOwner);

protected:
	tHateEntry* Find(Mob *ent);
	int32 GetHateBonus(tHateEntry *entry, bool combatRange, bool firstInRange = false, float distSquared = -1.0f);
private:
	std::list<tHateEntry*> list;
	Mob *owner;
	int32 combatRangeBonus;
	int32 sitInsideBonus;
	int32 sitOutsideBonus;
	int32 lowHealthBonus;
	bool rememberDistantMobs;
	bool nobodyInMeleeRange;
};

#endif
