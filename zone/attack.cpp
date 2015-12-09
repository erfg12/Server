/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/eq_constants.h"
#include "../common/eq_packet_structs.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/string_util.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "water_map.h"
#include "queryserv.h"
#include "worldserver.h"
#include "zone.h"
#include "remote_call_subscribe.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern WorldServer worldserver;
extern QueryServ* QServ;

#ifdef _WINDOWS
#define snprintf	_snprintf
#define strncasecmp	_strnicmp
#define strcasecmp	_stricmp
#endif

extern EntityList entity_list;
extern Zone* zone;
bool Mob::AttackAnimation(SkillUseTypes &skillinuse, int Hand, const ItemInst* weapon)
{
	// Determine animation
	Animation type = Animation::None;
	if (weapon && weapon->IsType(ItemClassCommon)) {
		const Item_Struct* item = weapon->GetItem();

		Log.Out(Logs::Detail, Logs::Attack, "Weapon skill : %i", item->ItemType);

		switch (item->ItemType)
		{
			case ItemType1HSlash: // 1H Slashing
			{
				skillinuse = Skill1HSlashing;
				type = Animation::Weapon1H;
				break;
			}
			case ItemType2HSlash: // 2H Slashing
			{
				skillinuse = Skill2HSlashing;
				type = Animation::Slashing2H;
				break;
			}
			case ItemType1HPiercing: // Piercing
			{
				skillinuse = Skill1HPiercing;
				type = Animation::Piercing;
				break;
			}
			case ItemType1HBlunt: // 1H Blunt
			{
				skillinuse = Skill1HBlunt;
				type = Animation::Weapon1H;
				break;
			}
			case ItemType2HBlunt: // 2H Blunt
			{
				skillinuse = Skill2HBlunt;
				type = Animation::Weapon2H; //anim2HWeapon
              	break;
			}
			case ItemType2HPiercing: // 2H Piercing
			{
				skillinuse = Skill1HPiercing; // change to Skill2HPiercing once activated
				type = Animation::Weapon2H;
				break;
			}
			case ItemTypeMartial:
			{
				skillinuse = SkillHandtoHand;
				type = Animation::Hand2Hand;
				break;
			}
			default:
			{
				skillinuse = SkillHandtoHand;
				type = Animation::Hand2Hand;
				break;
			}
		}// switch
	}
	else if(IsNPC()) {

		switch (skillinuse)
		{
			case Skill1HSlashing: // 1H Slashing
			{
				type = Animation::Weapon1H;
				break;
			}
			case Skill2HSlashing: // 2H Slashing
			{
				type = Animation::Slashing2H;
				break;
			}
			case Skill1HPiercing: // Piercing
			{
				type = Animation::Piercing;
				break;
			}
			case Skill1HBlunt: // 1H Blunt
			{
				type = Animation::Weapon1H;
				break;
			}
			case Skill2HBlunt: // 2H Blunt
			{
				type = Animation::Weapon2H;
				break;
			}
			case SkillHandtoHand:
			{
				type = Animation::Hand2Hand;
				break;
			}
			default:
			{
				type = Animation::Hand2Hand;
				break;
			}
		}// switch
	}
	else {
		skillinuse = SkillHandtoHand;
		type = Animation::Hand2Hand;
	}

	// If we're attacking with the secondary hand, play the dual wield anim
	if (Hand == MainSecondary)	// DW anim
		type = Animation::DualWield;

	DoAnim(type);
	return true;
}

// returns true on hit
bool Mob::AvoidanceCheck(Mob* attacker, SkillUseTypes skillinuse, int16 chance_mod)
{
	Mob *defender = this;

	if (chance_mod >= 10000)
		return true;

	if (IsClient() && CastToClient()->IsSitting())
	{
		return true;
	}

	int toHit = attacker->GetToHit(skillinuse);
	int avoidance = defender->GetAvoidance();
	int percentMod = 0;
	
	Log.Out(Logs::Detail, Logs::Attack, "AvoidanceCheck: %s attacked by %s;  Avoidance: %i  To-Hit: %i", defender->GetName(), attacker->GetName(), avoidance, toHit);

	// Hit Chance percent modifier
	// Disciplines: Evasive, Precision, Deadeye, Trueshot, Charge
	percentMod = attacker->itembonuses.HitChanceEffect[skillinuse] +
		attacker->spellbonuses.HitChanceEffect[skillinuse] +
		attacker->aabonuses.HitChanceEffect[skillinuse] +
		attacker->itembonuses.HitChanceEffect[HIGHEST_SKILL + 1] +
		attacker->spellbonuses.HitChanceEffect[HIGHEST_SKILL + 1] +
		attacker->aabonuses.HitChanceEffect[HIGHEST_SKILL + 1];

	// Avoidance chance percent modifier
	// Disciplines: Evasive, Precision, Voiddance, Fortitude
	percentMod -= (defender->spellbonuses.AvoidMeleeChanceEffect +
		defender->itembonuses.AvoidMeleeChanceEffect +
		defender->aabonuses.AvoidMeleeChanceEffect);

	percentMod += chance_mod;

	if (percentMod != 0)
	{
		if (skillinuse == SkillArchery && percentMod > 0)
			percentMod -= static_cast<int>(static_cast<float>(percentMod) * RuleR(Combat, ArcheryHitPenalty));

		Log.Out(Logs::Detail, Logs::Attack, "Modified chance to hit: %i%%", percentMod);

		if (percentMod > 0)
		{
			if (zone->random.Roll(percentMod))
			{
				Log.Out(Logs::Detail, Logs::Attack, "Modified Hit");
				return true;
			}
		}
		else
		{
			if (zone->random.Roll(-percentMod))
			{
				Log.Out(Logs::Detail, Logs::Attack, "Modified Miss");
				return false;
			}
		}
	}

	/*	This is an approximation of Sony's unknown function.  It is not precise.
		I needed two different curves to fit the data well. This fits the data
		within +/-1% for most levels, but does not model very low avoidance or
		to-hit values as well. (still around +/- a few percent, way better than
		the old code)

		The data these curves conform to was parsed from PC vs PC combat duels
		with precisely known avoidance and to-hit values.  It's possible PC vs
		PC combat follows different rules, but as the NPC to-hit values were
		also derived from this curve, the hit rate still ends up accurate even
		if the to-hit values for NPCs are wrong.	*/
	if (toHit > avoidance)
	{
		if (zone->random.Real(0.0, 1.0) > (avoidance / (toHit*1.75)))		// this calculates the miss rate, not the hit rate
		{
			Log.Out(Logs::Detail, Logs::Attack, "Hit");
			return true;
		}
	}
	else
	{
		if (zone->random.Real(0.0, 1.0) < (toHit / 2.2 / avoidance))		// hit rate
		{
			Log.Out(Logs::Detail, Logs::Attack, "Hit");
			return true;
		}
	}

	Log.Out(Logs::Detail, Logs::Attack, "Miss");
	return false;
}

bool Mob::AvoidDamage(Mob* attacker, int32 &damage, bool noRiposte, bool isRangedAttack)
{
	/* solar: called when a mob is attacked, does the checks to see if it's a hit
	* and does other mitigation checks. 'this' is the mob being attacked.
	*
	* special return values:
	* -1 - block
	* -2 - parry
	* -3 - riposte
	* -4 - dodge
	*
	*/
	/* Order according to current (SoF+?) dev quotes:
	* https://forums.daybreakgames.com/eq/index.php?threads/test-update-06-10-15.223510/page-2#post-3261772
	* https://forums.daybreakgames.com/eq/index.php?threads/test-update-06-10-15.223510/page-2#post-3268227
	* Riposte 50, hDEX, must have weapon/fists, doesn't work on archery/throwing
	* Block 25, hDEX, works on archery/throwing, behind block done here if back to attacker base1 is chance
	* Parry 45, hDEX, doesn't work on throwing/archery, must be facing target
	* Dodge 45, hAGI, works on archery/throwing, monks can dodge attacks from behind
	* Shield Block, rand base1
	* Staff Block, rand base1 (Alaris AA; not implemented here)
	*    regular strike through
	*    avoiding the attack (CheckHitChance)
	* As soon as one succeeds, none of the rest are checked
	*
	* Formula (all int math)
	* (posted for parry, assume rest at the same)
	* Chance = (((SKILL + 100) + [((SKILL+100) * SPA(175).Base1) / 100]) / 45) + [(hDex / 25) - min([hDex / 25], hStrikethrough)].
	* hStrikethrough is a mob stat that was added to counter the bonuses of heroic stats
	* Number rolled against 100, if the chance is greater than 100 it happens 100% of time
	*
	* Things with 10k accuracy mods can be avoided with these skills qq
	*/
	Mob *defender = this;

	bool InFront = attacker->InFrontMob(this, attacker->GetX(), attacker->GetY());

	//////////////////////////////////////////////////////////
	// make enrage same as riposte
	/////////////////////////////////////////////////////////
	if (IsEnraged() && InFront) {
		damage = -3;
		Log.Out(Logs::Detail, Logs::Combat, "I am enraged, riposting frontal attack.");
	}

	if (!noRiposte && !isRangedAttack && CanThisClassRiposte() && InFront)
	{
		bool cannotRiposte = false;

		if (IsClient())
		{
			ItemInst* weapon;
			weapon = CastToClient()->GetInv().GetItem(MainPrimary);

			if (weapon != nullptr && !weapon->IsWeapon())
			{
				cannotRiposte = true;
			}
			else
			{
				CastToClient()->CheckIncreaseSkill(SkillRiposte, attacker, zone->skill_difficulty[SkillRiposte].difficulty);
			}
		}

		if (!cannotRiposte)
		{
			// check auto discs ... I guess aa/items too :P
			if (spellbonuses.RiposteChance == 10000 || aabonuses.RiposteChance == 10000 || itembonuses.RiposteChance == 10000) {
				damage = -3;
				return true;
			}

			int chance = GetSkill(SkillRiposte) + 100;
			chance += (chance * (aabonuses.RiposteChance + spellbonuses.RiposteChance + itembonuses.RiposteChance)) / 100;
			chance /= 50;

			if (chance > 0 && zone->random.Roll(chance)) { // could be <0 from offhand stuff
				damage = -3;
				return true;
			}
		}
	}

	// block
	bool bBlockFromRear = false;

	// a successful roll on this does not mean a successful block is forthcoming. only that a chance to block
	// from a direction other than the rear is granted.

	int BlockBehindChance = aabonuses.BlockBehind + spellbonuses.BlockBehind + itembonuses.BlockBehind;

	if (BlockBehindChance && zone->random.Roll(BlockBehindChance))
		bBlockFromRear = true;

	if (CanThisClassBlock() && (InFront || bBlockFromRear)) {
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(SkillBlock, attacker, zone->skill_difficulty[SkillBlock].difficulty);

			// check auto discs ... I guess aa/items too :P
		if (spellbonuses.IncreaseBlockChance == 10000 || aabonuses.IncreaseBlockChance == 10000 ||
			itembonuses.IncreaseBlockChance == 10000) {
			damage = -1;
			return true;
		}
		int chance = GetSkill(SkillBlock) + 100;
		chance += (chance * (aabonuses.IncreaseBlockChance + spellbonuses.IncreaseBlockChance + itembonuses.IncreaseBlockChance)) / 100;
		chance /= 25;

		if (zone->random.Roll(chance)) {
			damage = -1;
			return true;
		}
	}

	// parry
	if (CanThisClassParry() && InFront && !isRangedAttack)
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(SkillParry, attacker, zone->skill_difficulty[SkillParry].difficulty);

		// check auto discs ... I guess aa/items too :P
		if (spellbonuses.ParryChance == 10000 || aabonuses.ParryChance == 10000 || itembonuses.ParryChance == 10000) {
			damage = -2;
			return true;
		}
		int chance = GetSkill(SkillParry) + 100;
		chance += (chance * (aabonuses.ParryChance + spellbonuses.ParryChance + itembonuses.ParryChance)) / 100;
		chance /= 45;

		if (zone->random.Roll(chance)) {
			damage = -2;
			return true;
		}
	}

	// dodge
	if (CanThisClassDodge() && InFront)
	{
		if (IsClient())
			CastToClient()->CheckIncreaseSkill(SkillDodge, attacker, zone->skill_difficulty[SkillDodge].difficulty);

		// check auto discs ... I guess aa/items too :P
		if (spellbonuses.DodgeChance == 10000 || aabonuses.DodgeChance == 10000 || itembonuses.DodgeChance == 10000) {
			damage = -4;
			return true;
		}
		int chance = GetSkill(SkillDodge) + 100;
		chance += (chance * (aabonuses.DodgeChance + spellbonuses.DodgeChance + itembonuses.DodgeChance)) / 100;
		chance /= 45;

		if (zone->random.Roll(chance)) {
			damage = -4;
			return true;
		}
	}

	// Try Shield Block
	if (HasShieldEquiped() && (aabonuses.ShieldBlock || spellbonuses.ShieldBlock || itembonuses.ShieldBlock) && (InFront || bBlockFromRear))
	{
		int chance = aabonuses.ShieldBlock + spellbonuses.ShieldBlock + itembonuses.ShieldBlock;

		if (zone->random.Roll(chance)) {
			damage = -1;
			return true;
		}
	}

	return false;
}

/*	This function employs the Box-Muller Transform to generate a Guassian Distribution bell curve.
	EQ's mitigation is based on a 1-20 roll derived from a bell curve as mentioned in this post:
	https://forums.daybreakgames.com/eq/index.php?threads/so-are-melee-still-crap.224298/page-3#post-3297852
	and also is easily observed in parsed logs.  DI1 and DI20 are more frequent because the trailing ends
	of the curve are added together.  This is not a perfect representation of Sony's function, but fairly close.

	Returns a value from 1 to 20, inclusive.*/
int Mob::RollD20(int32 offense, int32 mitigation)
{
	double diff = static_cast<double>(offense) - static_cast<double>(mitigation);
	double mean = 0;

	// changing the mean shifts the bell curve
	// this was mostly determined by trial and error to find what fit best
	if (offense > mitigation)
	{
		mean = diff / offense * 15;
	}
	else if (mitigation > offense)
	{
		mean = diff / mitigation * 15;
	}
	double stddev = 8.8;	// standard deviation adjusts the height of the bell
							// again, trial and error to find what fit best
	double theta = 2 * M_PI * zone->random.Real(0.0, 1.0);
	double rho = sqrt(-2 * log(1 - zone->random.Real(0.0, 1.0)));
	double d = mean + stddev * rho * cos(theta);

	// this combined with the stddev will produce ~15% DI1 and ~15% DI20 when mitigation == offense
	if (d < -9.5)
	{
		d = -9.5;
	}
	else if (d > 9.5)
	{
		d = 9.5;
	}
	d = d + 11;
	return static_cast<int>(d);
}

//Returns the weapon damage against the input mob
//if we cannot hit the mob with the current weapon we will get a value less than or equal to zero
//Else we know we can hit.
//GetWeaponDamage(mob*, const Item_Struct*) is intended to be used for mobs or any other situation where we do not have a client inventory item
//GetWeaponDamage(mob*, const ItemInst*) is intended to be used for situations where we have a client inventory item
int Mob::GetWeaponDamage(Mob *against, const Item_Struct *weapon_item) {
	int dmg = 0;
	int banedmg = 0;

	//can't hit invulnerable stuff with weapons.
	if(against->GetInvul() || against->GetSpecialAbility(IMMUNE_MELEE)){
		return 0;
	}

	//check to see if our weapons or fists are magical.
	if(against->GetSpecialAbility(IMMUNE_MELEE_NONMAGICAL)){
		if(weapon_item && !GetSpecialAbility(SPECATK_MAGICAL)){
			if(weapon_item->Magic){
				dmg = weapon_item->Damage;

				//this is more for non weapon items, ex: boots for kick
				//they don't have a dmg but we should be able to hit magical
				dmg = dmg <= 0 ? 1 : dmg;
			}
			else
				return 0;
		}
		else{
			if((GetClass() == MONK || GetClass() == BEASTLORD) && GetLevel() >= 30){
				dmg = GetMonkHandToHandDamage();
			}
			else if(GetOwner() && GetLevel() >= RuleI(Combat, PetAttackMagicLevel)){
				//pets wouldn't actually use this but...
				//it gives us an idea if we can hit due to the dual nature of this function
				dmg = 1;
			}
			else if(GetSpecialAbility(SPECATK_MAGICAL))
			{
				dmg = 1;
			}
			else
				return 0;
		}
	}
	else{
		if(weapon_item){
			dmg = weapon_item->Damage;

			dmg = dmg <= 0 ? 1 : dmg;
		}
		else{
			if(GetClass() == MONK || GetClass() == BEASTLORD){
				dmg = GetMonkHandToHandDamage();
			}
			else{
				dmg = 1;
			}
		}
	}

	int eledmg = 0;
	if(!against->GetSpecialAbility(IMMUNE_MAGIC)){
		if(weapon_item && weapon_item->ElemDmgAmt){
			//we don't check resist for npcs here
			eledmg = weapon_item->ElemDmgAmt;
			dmg += eledmg;
		}
	}

	if(against->GetSpecialAbility(IMMUNE_MELEE_EXCEPT_BANE)){
		if(weapon_item){
			if(weapon_item->BaneDmgBody == against->GetBodyType()){
				banedmg += weapon_item->BaneDmgAmt;
			}

			if(weapon_item->BaneDmgRace == against->GetRace()){
				banedmg += weapon_item->BaneDmgAmt;
			}
		}

		if(!eledmg && !banedmg){
			if(!GetSpecialAbility(SPECATK_BANE))
				return 0;
			else
				return 1;
		}
		else
			dmg += banedmg;
	}
	else{
		if(weapon_item){
			if(weapon_item->BaneDmgBody == against->GetBodyType()){
				banedmg += weapon_item->BaneDmgAmt;
			}

			if(weapon_item->BaneDmgRace == against->GetRace()){
				banedmg += weapon_item->BaneDmgAmt;
			}
		}

		dmg += (banedmg + eledmg);
	}

	if(dmg <= 0){
		return 0;
	}
	else
		return dmg;
}

int Mob::GetWeaponDamage(Mob *against, const ItemInst *weapon_item, uint32 *hate)
{
	int dmg = 0;
	int banedmg = 0;

	if(!against || against->GetInvul() || against->GetSpecialAbility(IMMUNE_MELEE)){
		return 0;
	}

	//check for items being illegally attained
	if(weapon_item){
		const Item_Struct *mWeaponItem = weapon_item->GetItem();
		if(mWeaponItem){
			if(mWeaponItem->ReqLevel > GetLevel()){
				return 0;
			}

			if(!weapon_item->IsEquipable(GetBaseRace(), GetClass())){
				return 0;
			}
		}
		else{
			return 0;
		}
	}

	if(against->GetSpecialAbility(IMMUNE_MELEE_NONMAGICAL)){
		if(weapon_item && !GetSpecialAbility(SPECATK_MAGICAL)){
			// check to see if the weapon is magic
			bool MagicWeapon = false;
			if(weapon_item->GetItem() && weapon_item->GetItem()->Magic)
				MagicWeapon = true;
			else {
				if(spellbonuses.MagicWeapon || itembonuses.MagicWeapon)
					MagicWeapon = true;
			}

			if(MagicWeapon) {

				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					dmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->Damage);
				}
				else{
					dmg = weapon_item->GetItem()->Damage;
				}

				dmg = dmg <= 0 ? 1 : dmg;
			}
			else
				return 0;
		}
		else{
			bool MagicGloves = false;
			ItemInst *gloves = CastToClient()->GetInv().GetItem(MainHands);

			if (gloves != nullptr)
				MagicGloves = gloves->GetItem()->Magic;

			if ((GetClass() == MONK || GetClass() == BEASTLORD) && (GetLevel() >= 30 || MagicGloves))
			{
				dmg = GetMonkHandToHandDamage();
				if (hate) *hate += dmg;
			}
			else if(GetOwner() && GetLevel() >= RuleI(Combat, PetAttackMagicLevel)) //pets wouldn't actually use this but...
				dmg = 1;															//it gives us an idea if we can hit
			else if (MagicGloves || GetSpecialAbility(SPECATK_MAGICAL))
				dmg = 1;
			else
				return 0;
		}
	}
	else{
		if(weapon_item){
			if(weapon_item->GetItem()){

				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					dmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->Damage);
				}
				else{
					dmg = weapon_item->GetItem()->Damage;
				}

				dmg = dmg <= 0 ? 1 : dmg;
			}
		}
		else{
			if(GetClass() == MONK || GetClass() == BEASTLORD){
				dmg = GetMonkHandToHandDamage();
				if (hate) *hate += dmg;
			}
			else{
				dmg = 1;
			}
		}
	}

	int eledmg = 0;
	if(!against->GetSpecialAbility(IMMUNE_MAGIC)){
		if(weapon_item && weapon_item->GetItem() && weapon_item->GetItem()->ElemDmgAmt){
			if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
				eledmg = CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->ElemDmgAmt);
			}
			else{
				eledmg = weapon_item->GetItem()->ElemDmgAmt;
			}

			if(eledmg)
			{
				eledmg = (eledmg * against->ResistSpell(weapon_item->GetItem()->ElemDmgType, 0, this) / 100);
			}
		}
	}

	if(against->GetSpecialAbility(IMMUNE_MELEE_EXCEPT_BANE)){
		if(weapon_item && weapon_item->GetItem()){
			if(weapon_item->GetItem()->BaneDmgBody == against->GetBodyType()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgAmt;
				}
			}

			if(weapon_item->GetItem()->BaneDmgRace == against->GetRace()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgAmt;
				}
			}
		}

		if(!eledmg && !banedmg)
		{
			if(!GetSpecialAbility(SPECATK_BANE))
				return 0;
			else
				return 1;
		}
		else {
			dmg += (banedmg + eledmg);
			if (hate) *hate += banedmg;
		}
	}
	else{
		if(weapon_item && weapon_item->GetItem()){
			if(weapon_item->GetItem()->BaneDmgBody == against->GetBodyType()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgAmt;
				}
			}

			if(weapon_item->GetItem()->BaneDmgRace == against->GetRace()){
				if(IsClient() && GetLevel() < weapon_item->GetItem()->RecLevel){
					banedmg += CastToClient()->CalcRecommendedLevelBonus(GetLevel(), weapon_item->GetItem()->RecLevel, weapon_item->GetItem()->BaneDmgAmt);
				}
				else{
					banedmg += weapon_item->GetItem()->BaneDmgAmt;
				}
			}
		}
		dmg += (banedmg + eledmg);
		if (hate) *hate += banedmg;
	}

	if(dmg <= 0){
		return 0;
	}
	else
		return dmg;
}

// returns a value from 100 to X.  100 means that the damage is unchanged
// divide by 100 after
uint32 Client::RollDamageMultiplier(uint32 offense)
{
	// many logs were parsed for this; may not be complete however
	int rollChance = 50;

	if (GetLevel() >= 60)
	{
		rollChance = 80;
	}
	else if (GetLevel() >= 56)
	{
		if (GetClass() == MONK)
		{
			rollChance = 75;
		}
		else
		{
			rollChance = 70;
		}
	}
	else if (GetLevel() > 50)
	{
		rollChance = 65;
	}
	else if (GetClass() == MONK)
	{
		rollChance = 55;
	}

	if (offense > 153 && zone->random.Roll(rollChance))
	{
		uint32 roll;
		if (GetLevel() <= 50)
		{
			roll = offense * 37 / 100 + 50;				// I don't know how Sony calculates this, but these are pretty close
		}
		else
		{
			roll = offense * 37 / 100 + level + 10;
		}
		roll = zone->random.Int(100, roll);

		uint16 cap = GetDamageTable();

		if (roll > cap)
		{
			roll = cap;
		}

		return roll;
	}
	else
	{
		return 100;
	}
}

//note: throughout this method, setting `damage` to a negative is a way to
//stop the attack calculations
// IsFromSpell added to allow spell effects to use Attack. (Mainly for the Rampage AA right now.)
bool Client::Attack(Mob* other, int Hand, bool bRiposte, bool IsStrikethrough, bool IsFromSpell, ExtraAttackOptions *opts)
{
	if (!other) {
		SetTarget(nullptr);
		Log.Out(Logs::General, Logs::Error, "A null Mob object was passed to Client::Attack() for evaluation!");
		return false;
	}

	if(!GetTarget())
		SetTarget(other);

	Log.Out(Logs::Detail, Logs::Combat, "Attacking %s with hand %d %s", other?other->GetName():"(nullptr)", Hand, bRiposte?"(this is a riposte)":"");

	//SetAttackTimer();
	if (
		(IsCasting() && GetClass() != BARD && !IsFromSpell)
		|| other == nullptr
		|| ((IsClient() && CastToClient()->dead) || (other->IsClient() && other->CastToClient()->dead))
		|| (GetHP() < 0)
		|| (!IsAttackAllowed(other))
		) {
		Log.Out(Logs::Detail, Logs::Combat, "Attack canceled, invalid circumstances.");
		return false; // Only bards can attack while casting
	}

	if(DivineAura() && !GetGM()) {//cant attack while invulnerable unless your a gm
		Log.Out(Logs::Detail, Logs::Combat, "Attack canceled, Divine Aura is in effect.");
		Message_StringID(MT_DefaultText, DIVINE_AURA_NO_ATK);	//You can't attack while invulnerable!
		return false;
	}

	if (GetFeigned())
		return false; // Rogean: How can you attack while feigned? Moved up from Aggro Code.

	ItemInst* weapon;
	if (Hand == MainSecondary){	// Kaiyodo - Pick weapon from the attacking hand
		weapon = GetInv().GetItem(MainSecondary);
		OffHandAtk(true);
	}
	else{
		weapon = GetInv().GetItem(MainPrimary);
		OffHandAtk(false);
	}

	if(weapon != nullptr) {
		if (!weapon->IsWeapon()) {
			Log.Out(Logs::Detail, Logs::Combat, "Attack canceled, Item %s (%d) is not a weapon.", weapon->GetItem()->Name, weapon->GetID());
			return(false);
		}
		Log.Out(Logs::Detail, Logs::Combat, "Attacking with weapon: %s (%d)", weapon->GetItem()->Name, weapon->GetID());
	} else {
		Log.Out(Logs::Detail, Logs::Combat, "Attacking without a weapon.");
	}

	// calculate attack_skill and skillinuse depending on hand and weapon
	// also send Packet to near clients
	SkillUseTypes skillinuse;
	AttackAnimation(skillinuse, Hand, weapon);
	Log.Out(Logs::Detail, Logs::Combat, "Attacking with %s in slot %d using skill %d", weapon?weapon->GetItem()->Name:"Fist", Hand, skillinuse);

	/// Now figure out damage
	int damage = 1;
	uint8 mylevel = GetLevel() ? GetLevel() : 1;
	uint32 hate = 0;
	int weapon_damage = GetWeaponDamage(other, weapon, &hate);
	if (weapon)
	{
		hate = weapon->GetItem()->Damage + weapon->GetItem()->ElemDmgAmt + weapon->GetItem()->BaneDmgAmt;
	}
	else if (GetClass() == MONK)
	{
		hate = GetMonkHandToHandDamage();
	}
	else
	{
		hate = 4;		// how much damage is non-monk hand-to-hand?
	}

	// SE_MinDamageModifier for disciplines: Fellstrike, Innerflame, Duelist, Bestial Rage
	// min hit becomes 4 x weapon damage + 1 x damage bonus
	int minDmgModDmg = weapon_damage * GetMeleeMinDamageMod_SE(skillinuse) / 100;

	// SE_DamageModifier for Disciplines: Aggressive, Defensive, Fellstrike, Duelist, Innerflame, Bestial Rage
	// For fellstrke, innerflame, duelist, and bestial rage max hit should be 2 x weapon dmg + 1 x dmg bonus
	// Under defensive, max roll becomes 9, so meleedmgmod should be -55
	int meleeDmgMod = GetMeleeDamageMod_SE(skillinuse);
	if (meleeDmgMod)
	{
		weapon_damage += weapon_damage * meleeDmgMod / 100;
		hate += hate * meleeDmgMod / 100;
	}

	int damageBonus = 0;
	if (Hand == MainPrimary && mylevel >= 28 && IsWarriorClass())
	{
		damageBonus = GetWeaponDamageBonus(weapon ? weapon->GetItem() : (const Item_Struct*) nullptr);
		hate += damageBonus;
	}

	if(weapon_damage < 1)
	{
		damage = -5;			// immune
	}
	else
	{
		// check avoidance skills
		other->AvoidDamage(this, damage);

		//riposte
		if (damage == -3)
		{
			if (bRiposte)
			{
				return false;
			}
			else
			{
				DoRiposte(other);
				if (IsDead()) return false;
			}
		}

		if (damage > 0)
		{
			// swing not avoided by skills; do avoidance AC check

			int hit_chance_bonus = 0;
			if (opts)
			{
				hit_chance_bonus += opts->hit_chance;
			}

			//if (!other->CheckHitChance(this, skillinuse, Hand, hit_chance_bonus))
			if (!other->AvoidanceCheck(this, skillinuse, hit_chance_bonus))
			{
				Log.Out(Logs::Detail, Logs::Combat, "Attack missed. Damage set to 0.");
				damage = 0;
			}
		}

		if (damage > 0)
		{
			//try a finishing blow.. if successful end the attack
			if(TryFinishingBlow(other, skillinuse))
				return (true);

			int offense = GetOffense(skillinuse);
			int mitigation = other->GetMitigation();

			if (opts)
			{
				mitigation = static_cast<int>((1.0f - opts->armor_pen_percent) * static_cast<float>(mitigation));
				mitigation -= opts->armor_pen_flat;
				if (mitigation < 1)
				{
					mitigation = 1;
				}
			}

			// mitigation roll
			int roll = RollD20(offense, mitigation);

			if (other->IsClient())
			{
				if (other->CastToClient()->IsSitting())
				{
					roll = 20;
				}
				// hardcoding defensive disc for accuracy
				if (roll > 1 && other->GetClass() == WARRIOR && other->CastToClient()->GetActiveDisc() == disc_defensive)
				{
					roll /= 2;
				}
			}

			damage = (roll * weapon_damage * 10 + 5) / 100;				// damage rounds to nearest whole number
			damage = damage * RollDamageMultiplier(offense) / 100;		// client only damage multiplier
			
			if (mylevel < 10 && damage > RuleI(Combat, HitCapPre10))
				damage = (RuleI(Combat, HitCapPre10));
			else if (mylevel < 20 && damage > RuleI(Combat, HitCapPre20))
				damage = (RuleI(Combat, HitCapPre20));

			CheckIncreaseSkill(skillinuse, other, zone->skill_difficulty[skillinuse].difficulty);
			CheckIncreaseSkill(SkillOffense, other, zone->skill_difficulty[SkillOffense].difficulty);

			damage += damageBonus;

			minDmgModDmg += damageBonus;
			int min_hit = (weapon_damage + 5) / 10 + damageBonus;		// +5 is to round to nearest whole number

			if (min_hit < minDmgModDmg) min_hit = minDmgModDmg;
			if (min_hit < 1) min_hit = 1;
			if (damage < min_hit) damage = min_hit;

			damage = mod_client_damage(damage, skillinuse, Hand, weapon, other);

			Log.Out(Logs::Detail, Logs::Combat, "Damage calculated to %d (min %d, str %d, skill %d, DMG %d, lv %d)",
				damage, min_hit, GetSTR(), GetSkill(skillinuse), weapon_damage, mylevel);

			if(opts)
			{
				damage = static_cast<int>(opts->damage_percent * static_cast<float>(damage));
				damage += opts->damage_flat;
				hate = static_cast<uint32>(opts->hate_percent * static_cast<float>(hate));
				hate += opts->hate_flat;
			}

			// Stonestance & Protective Spirit Disciplines.  Defensive handled above
			if (other->IsClient() && other->GetClass() != WARRIOR)
			{
				damage -= damage * other->GetSpellBonuses().MeleeMitigationEffect / 100;
			}

			CommonOutgoingHitSuccess(other, damage, skillinuse);

			Log.Out(Logs::Detail, Logs::Combat, "Final damage after all reductions: %d", damage);
		}
	}

	// Hate Generation is on a per swing basis, regardless of a hit, miss, or block, its always the same.
	// If we are this far, this means we are atleast making a swing.
	other->AddToHateList(this, hate);

	///////////////////////////////////////////////////////////
	////// Send Attack Damage
	///////////////////////////////////////////////////////////
	other->Damage(this, damage, SPELL_UNKNOWN, skillinuse);

	if (IsDead()) return false;

	MeleeLifeTap(damage);

	if (damage > 0){
		CheckNumHitsRemaining(NUMHIT_OutgoingHitSuccess);
	}

	CommonBreakInvisible();

	if (damage > 0)
		return true;

	else
		return false;
}

//used by complete heal and #heal
void Mob::Heal()
{
	SetMaxHP();
	SendHPUpdate();
}

void Client::Damage(Mob* other, int32 damage, uint16 spell_id, SkillUseTypes attack_skill, bool avoidable, int8 buffslot, bool iBuffTic)
{
	if(dead || IsCorpse())
		return;

	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	if(spell_id!=0 && spell_id != SPELL_UNKNOWN && other && damage > 0)
	{
		if(other->IsNPC() && !other->IsPet())
		{
			float npcspellscale = other->CastToNPC()->GetSpellScale();
			damage = ((float)damage * npcspellscale) / (float)100;
		}
	}

	// cut all PVP spell damage to 2/3 -solar
	// Blasting ourselfs is considered PvP
	//Don't do PvP mitigation if the caster is damaging himself
	if(other && other->IsClient() && (other != this) && damage > 0) {
		int PvPMitigation = 100;
		if(attack_skill == SkillArchery)
			PvPMitigation = 80;
		else
			PvPMitigation = 67;
		damage = (damage * PvPMitigation) / 100;
	}

	if(!ClientFinishedLoading())
		damage = -5;

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);

	if (damage > 0) {
		if (spell_id == SPELL_UNKNOWN) {
			CheckIncreaseSkill(SkillDefense, other, zone->skill_difficulty[SkillDefense].difficulty);
        }
	}
}

void Mob::AggroPet(Mob* attacker)
{
	/**
	 * Pets should always assist if someone is trying to attack the master
	 * Uneless Pet hold is activated
	 */
	if(attacker) {
		Mob *pet = GetPet();
		if (pet && !pet->IsFamiliar() && !pet->GetSpecialAbility(IMMUNE_AGGRO) && !pet->IsEngaged() && attacker && attacker != this && !attacker->IsCorpse())
		{
			if (attacker->IsClient() && attacker->CastToClient()->GetFeigned())
				return;

			if (!pet->IsHeld()) {
				Log.Out(Logs::Detail, Logs::Combat, "Sending pet %s into battle due to attack.", pet->GetName());
				pet->AddToHateList(attacker, 1);
				pet->SetTarget(attacker);
				Message_StringID(CC_Default, PET_ATTACKING, pet->GetCleanName(), attacker->GetCleanName());
			}
		}
	}
}

bool Client::Death(Mob* killerMob, int32 damage, uint16 spell, SkillUseTypes attack_skill, uint8 killedby)
{
	if(!ClientFinishedLoading())
		return false;

	if(dead)
		return false;	//cant die more than once...

	if(!spell)
		spell = SPELL_UNKNOWN;

	char buffer[48] = { 0 };
	snprintf(buffer, 47, "%d %d %d %d", killerMob ? killerMob->GetID() : 0, damage, spell, static_cast<int>(attack_skill));
	if(parse->EventPlayer(EVENT_DEATH, this, buffer, 0) != 0) {
		if(GetHP() < 0) {
			SetHP(0);
		}
		return false;
	}

	if(killerMob && killerMob->IsClient() && (spell != SPELL_UNKNOWN) && damage > 0) {
		char val1[20]={0};
	}

	int exploss = 0;
	Log.Out(Logs::Detail, Logs::Combat, "Fatal blow dealt by %s with %d damage, spell %d, skill %d", killerMob ? killerMob->GetName() : "Unknown", damage, spell, attack_skill);

	/*
		#1: Send death packet to everyone
	*/
	uint8 killed_level = GetLevel();

	SendLogoutPackets();
	SendRealPosition();
	/* Make Death Packet */
	EQApplicationPacket app(OP_Death, sizeof(Death_Struct));
	Death_Struct* d = (Death_Struct*)app.pBuffer;
	d->spawn_id = GetID();
	d->killer_id = killerMob ? killerMob->GetID() : 0;
	d->corpseid=GetID();
	d->bindzoneid = m_pp.binds[0].zoneId;
	d->spell_id = spell == SPELL_UNKNOWN ? 0xffffffff : spell;
	d->attack_skill = spell != SPELL_UNKNOWN ? 0xe7 : attack_skill;
	d->damage = damage;
	app.priority = 6;
	entity_list.QueueClients(this, &app);

	/*
		#2: figure out things that affect the player dying and mark them dead
	*/

	if(GetPet() && GetPet()->IsCharmed())
	{
		Log.Out(Logs::General, Logs::Spells, "%s has died. Fading charm on pet.", GetName());
		GetPet()->BuffFadeByEffect(SE_Charm);
	}

	InterruptSpell();
	SetPet(0);
	SetHorseId(0);
	dead = true;

	if(GetClass() == SHADOWKNIGHT && !p_timers.Expired(&database, pTimerHarmTouch))
	{
		p_timers.Clear(&database, pTimerHarmTouch);

	}
	else if(GetClass() == PALADIN  && !p_timers.Expired(&database, pTimerLayHands))
	{
		p_timers.Clear(&database, pTimerLayHands);
	}

	if (killerMob != nullptr)
	{
		if (killerMob->IsNPC()) {
			parse->EventNPC(EVENT_SLAY, killerMob->CastToNPC(), this, "", 0);

			mod_client_death_npc(killerMob);
			killedby = Killed_NPC;

			uint16 emoteid = killerMob->GetEmoteID();
			if(emoteid != 0)
				killerMob->CastToNPC()->DoNPCEmote(KILLEDPC,emoteid);
			killerMob->TrySpellOnKill(killed_level,spell);
		}

		if(killerMob->IsClient() && (IsDueling() || killerMob->CastToClient()->IsDueling())) {
			SetDueling(false);
			SetDuelTarget(0);
			if (killerMob->IsClient() && killerMob->CastToClient()->IsDueling() && killerMob->CastToClient()->GetDuelTarget() == GetID())
			{
				//if duel opponent killed us...
				killerMob->CastToClient()->SetDueling(false);
				killerMob->CastToClient()->SetDuelTarget(0);
				entity_list.DuelMessage(killerMob,this,false);

				mod_client_death_duel(killerMob);
				killedby = Killed_DUEL;

			} else {
				//otherwise, we just died, end the duel.
				Mob* who = entity_list.GetMob(GetDuelTarget());
				if(who && who->IsClient()) {
					who->CastToClient()->SetDueling(false);
					who->CastToClient()->SetDuelTarget(0);
				}
			}
		}
		else if(killerMob->IsClient())
			killedby = Killed_PVP;
	}

	entity_list.RemoveFromTargets(this);
	hate_list.RemoveEnt(this);

	//remove ourself from all proximities
	ClearAllProximities();

	/*
		#3: exp loss and corpse generation
	*/
	if(IsClient())
		CastToClient()->GetExpLoss(killerMob, spell, exploss);

	SetMana(GetMaxMana());

	bool LeftCorpse = false;

	// now we apply the exp loss, unmem their spells, and make a corpse
	// unless they're a GM (or less than lvl 10
	if(!GetGM())
	{
		if(exploss > 0) {
			int32 newexp = GetEXP();
			if(exploss > newexp) {
				//lost more than we have... wtf..
				newexp = 1;
			} else {
				newexp -= exploss;
			}
			SetEXP(newexp, GetAAXP());
			//m_epp.perAA = 0;	//reset to no AA exp on death.
		}

		//this generates a lot of 'updates' to the client that the client does not need
		BuffFadeNonPersistDeath();
		UnmemSpellAll(false);

		if((RuleB(Character, LeaveCorpses) && GetLevel() >= RuleI(Character, DeathItemLossLevel)) || RuleB(Character, LeaveNakedCorpses))
		{
			// If we've died on a boat, make sure corpse falls overboard.
			if(GetBoatNPCID() != 0)
			{
				if(zone->zonemap != nullptr)
				{
					m_Position.z -= 100;
					glm::vec3 dest(m_Position.x, m_Position.y, m_Position.z);
					m_Position.z = zone->zonemap->FindBestZ(dest, nullptr);
				}
			}

			// creating the corpse takes the cash/items off the player too
			Corpse *new_corpse = new Corpse(this, exploss, killedby);

			char tmp[20];
			database.GetVariable("ServerType", tmp, 9);
			if(atoi(tmp)==1 && killerMob != nullptr && killerMob->IsClient()){
				char tmp2[10] = {0};
				database.GetVariable("PvPreward", tmp, 9);
				int reward = atoi(tmp);
				if(reward==3){
					database.GetVariable("PvPitem", tmp2, 9);
					int pvpitem = atoi(tmp2);
					if(pvpitem>0 && pvpitem<200000)
						new_corpse->SetPlayerKillItemID(pvpitem);
				}
				else if(reward==2)
					new_corpse->SetPlayerKillItemID(-1);
				else if(reward==1)
					new_corpse->SetPlayerKillItemID(1);
				else
					new_corpse->SetPlayerKillItemID(0);
				if(killerMob->CastToClient()->isgrouped) {
					Group* group = entity_list.GetGroupByClient(killerMob->CastToClient());
					if(group != 0)
					{
						for(int i=0;i<6;i++)
						{
							if(group->members[i] != nullptr)
							{
								new_corpse->AllowPlayerLoot(group->members[i],i);
							}
						}
					}
				}
			}

			entity_list.AddCorpse(new_corpse, GetID());
			SetID(0);

			LeftCorpse = true;
		}
	} else {
		BuffFadeDetrimental();
	}

	/*
		Finally, send em home

		We change the mob variables, not pp directly, because Save() will copy
		from these and overwrite what we set in pp anyway
	*/
	if(isgrouped)
	{
		Group *g = GetGroup();
		if(g)
			g->MemberZoned(this);
	}

	Raid* r = entity_list.GetRaidByClient(this);

	if(r)
		r->MemberZoned(this);

		dead_timer.Start(5000, true);
		m_pp.zone_id = m_pp.binds[0].zoneId;
		m_pp.zoneInstance = m_pp.binds[0].instance_id;
		database.MoveCharacterToZone(this->CharacterID(), database.GetZoneName(m_pp.zone_id));
		Save();
	GoToDeath();

	/* QS: PlayerLogDeaths */
	if (RuleB(QueryServ, PlayerLogDeaths))
	{
		char killer_name[128];
		if (killerMob && killerMob->GetCleanName())
		{
			strncpy(killer_name, killerMob->GetCleanName(), 128);
		}
		QServ->QSDeathBy(this->CharacterID(), this->GetZoneID(), this->GetInstanceID(), killer_name, spell, damage);
	}

	parse->EventPlayer(EVENT_DEATH_COMPLETE, this, buffer, 0);
	return true;
}

bool NPC::Attack(Mob* other, int Hand, bool bRiposte, bool IsStrikethrough, bool IsFromSpell, ExtraAttackOptions *opts)
{   
	if (!other) {
		SetTarget(nullptr);
		Log.Out(Logs::General, Logs::Error, "A null Mob object was passed to NPC::Attack() for evaluation!");
		return false;
	}

	if(IsPet() && GetOwner()->IsClient() && other->IsMezzed()) {
		RemoveFromHateList(other);
		GetOwner()->Message_StringID(CC_Yellow, CANNOT_WAKE, GetCleanName(), other->GetCleanName());
		return false;
	}
	int damage = 1;

	if(DivineAura())
		return false;

	if(!GetTarget())
		SetTarget(other);

	//Check that we can attack before we calc heading and face our target
	if (!IsAttackAllowed(other)) {
		if (this->GetOwnerID())
			this->Say_StringID(NOT_LEGAL_TARGET);
		if(other) {
			RemoveFromHateList(other);
			Log.Out(Logs::Detail, Logs::Combat, "I am not allowed to attack %s", other->GetName());
		}
		return false;
	}

	FaceTarget(GetTarget());

	SkillUseTypes skillinuse = SkillHandtoHand;
	if (Hand == MainPrimary) {
		skillinuse = static_cast<SkillUseTypes>(GetPrimSkill());
		OffHandAtk(false);
	}
	if (Hand == MainSecondary) {
		skillinuse = static_cast<SkillUseTypes>(GetSecSkill());
		OffHandAtk(true);
	}

	//figure out what weapon they are using, if any
	const Item_Struct* weapon = nullptr;
	if (Hand == MainPrimary && equipment[MainPrimary] > 0)
		weapon = database.GetItem(equipment[MainPrimary]);
	else if (equipment[MainSecondary])
		weapon = database.GetItem(equipment[MainSecondary]);

	//We don't factor much from the weapon into the attack.
	//Just the skill type so it doesn't look silly using punching animations and stuff while wielding weapons
	if(weapon) {
		Log.Out(Logs::Detail, Logs::Combat, "Attacking with weapon: %s (%d) (too bad im not using it for much)", weapon->Name, weapon->ID);

		if(Hand == MainSecondary && weapon->ItemType == ItemTypeShield){
			Log.Out(Logs::Detail, Logs::Combat, "Attack with shield canceled.");
			return false;
		}

		switch(weapon->ItemType){
			case ItemType1HSlash:
				skillinuse = Skill1HSlashing;
				break;
			case ItemType2HSlash:
				skillinuse = Skill2HSlashing;
				break;
			case ItemType1HPiercing:
				//skillinuse = Skill1HPiercing;
				//break;
			case ItemType2HPiercing:
				skillinuse = Skill1HPiercing; // change to Skill2HPiercing once activated
				break;
			case ItemType1HBlunt:
				skillinuse = Skill1HBlunt;
				break;
			case ItemType2HBlunt:
				skillinuse = Skill2HBlunt;
				break;
			case ItemTypeBow:
				skillinuse = SkillArchery;
				break;
			case ItemTypeLargeThrowing:
			case ItemTypeSmallThrowing:
				skillinuse = SkillThrowing;
				break;
			default:
				skillinuse = SkillHandtoHand;
				break;
		}
	}

	int weapon_damage = GetWeaponDamage(other, weapon);

	//do attack animation regardless of whether or not we can hit below
	int16 charges = 0;
	ItemInst weapon_inst(weapon, charges);
	AttackAnimation(skillinuse, Hand, &weapon_inst);

	// Remove this once Skill2HPiercing is activated
	//Work-around for there being no 2HP skill - We use 99 for the 2HB animation and 36 for pierce messages
	if(skillinuse == 99)
		skillinuse = static_cast<SkillUseTypes>(36);

	if (weapon_damage == 0)
	{
		damage = -5;		// immune to weapon
	}
	else
	{
		// check avoidance skills
		other->AvoidDamage(this, damage);

		if (damage > 0)
		{
			int hit_chance_bonus = 0;

			if (opts)
			{
				hit_chance_bonus += opts->hit_chance;
			}

			// check avoidance AC
			if (!other->AvoidanceCheck(this, skillinuse, hit_chance_bonus))
			{
				damage = 0;	//miss
			}
		}
	}

	if (damage > 0)
	{
		//ele and bane dmg too
		//NPCs add this differently than PCs
		//if NPCs can't inheriently hit the target we don't add bane/magic dmg which isn't exactly the same as PCs
		uint32 eleBane = 0;
		if (weapon)
		{
			if (weapon->BaneDmgBody == other->GetBodyType()){
				eleBane += weapon->BaneDmgAmt;
			}

			if(weapon->BaneDmgRace == other->GetRace()){
				eleBane += weapon->BaneDmgAmt;
			}

			if (weapon->ElemDmgAmt){
				eleBane += (weapon->ElemDmgAmt * other->ResistSpell(weapon->ElemDmgType, 0, this) / 100);
			}
		}

		if (!RuleB(NPC, UseItemBonusesForNonPets)){
			if (!GetOwner()){
				eleBane = 0;
			}
		}

		int32 hate = max_dmg + eleBane;
		hate = hate < 2 ? 1 : hate / 2;

		int32 offense = GetOffense();
		int32 mitigation = other->GetMitigation();

		if (opts)
		{
			mitigation = static_cast<int32>((1.0f - opts->armor_pen_percent) * static_cast<float>(mitigation));
			mitigation -= opts->armor_pen_flat;
			if (mitigation < 1)
			{
				mitigation = 1;
			}
		}

		// mitigation roll
		uint16 roll = RollD20(offense, mitigation);

		if (other->IsClient())
		{
			if (other->CastToClient()->IsSitting())
			{
				roll = 20;
			}
			// hardcoding defensive disc for accuracy
			if (roll > 1 && other->GetClass() == WARRIOR && other->CastToClient()->GetActiveDisc() == disc_defensive)
			{
				roll /= 2;
			}
		}

		uint32 maxHit = max_dmg;
		uint32 di1k;

		// lower level pets and NPCs with weapons hit harder
		if (weapon && maxHit < (weapon->Damage * 2))
		{
			maxHit = weapon->Damage * 2;
		}

		maxHit += eleBane;
		if (maxHit <= min_dmg)
		{
			maxHit = min_dmg;
			roll = 20;
			di1k = 1;
		}
		else
		{
			di1k = (maxHit - min_dmg) * 1000 / 19;			// multiply damage interval by 1000 so truncation doesn't reduce accuracy
		}

		if (roll == 20)
		{
			damage = maxHit;
		}
		else
		{
			// EQ uses a DB + DI * d20 system
			// Our database merely has a min hit and a max hit
			// so this (or similar) is required to emulate Sony's system
			damage = (di1k * roll + (min_dmg * 1000 - di1k)) / 1000;
		}

		damage = mod_npc_damage(damage, skillinuse, Hand, weapon, other);

		if (opts)
		{
			damage = static_cast<int>(opts->damage_percent * static_cast<float>(damage));
			damage += opts->damage_flat;
			hate = static_cast<int>(opts->hate_percent * static_cast<float>(hate));
			hate += opts->hate_flat;
		}

		// Stonestance & Protective Spirit Disciplines.  Defensive handled above
		if (other->IsClient() && other->GetClass() != WARRIOR)
		{
			damage -= damage * other->GetSpellBonuses().MeleeMitigationEffect / 100;
		}

		CommonOutgoingHitSuccess(other, damage, skillinuse);

		Log.Out(Logs::Detail, Logs::Combat, "Generating hate %d towards %s", hate, GetName());

		other->AddToHateList(this, hate);

		Log.Out(Logs::Detail, Logs::Combat, "Final damage against %s: %d", other->GetName(), damage);

		if(other->IsClient() && IsPet() && GetOwner()->IsClient()) {
			//pets do half damage to clients in pvp
			damage=damage/2;
		}
	}

	//cant riposte a riposte
	if (bRiposte && damage == -3) {
		Log.Out(Logs::Detail, Logs::Combat, "Riposte of riposte canceled.");
		return false;
	}

	if(GetHP() > 0 && !other->HasDied()) {
		other->Damage(this, damage, SPELL_UNKNOWN, skillinuse, false); // Not avoidable client already had thier chance to Avoid
	} else
		return false;

	if (HasDied()) //killed by damage shield ect
		return false;

	MeleeLifeTap(damage);

	CommonBreakInvisible();

	//I doubt this works...
	if (!GetTarget())
		return true; //We killed them

	if(!other->HasDied())
	{
		// innate procs proc on rips, weapon procs don't
		if (!bRiposte)
			TryWeaponProc(nullptr, weapon, other, Hand);	//no weapon

		if (damage > 0 && !other->GetSpellBonuses().MeleeRune[0])			// NPCs only proc innate procs on a hit
		{
			TrySpellProc(nullptr, weapon, other, Hand);
		}
	}

	// now check ripostes
	if (damage == -3) { // riposting
		DoRiposte(other);
	}

	if (damage > 0)
		return true;

	else
		return false;
}

void NPC::Damage(Mob* other, int32 damage, uint16 spell_id, SkillUseTypes attack_skill, bool avoidable, int8 buffslot, bool iBuffTic) {
	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	//handle EVENT_ATTACK. Resets after we have not been attacked for 12 seconds
	if(attacked_timer.Check())
	{
		Log.Out(Logs::Detail, Logs::Combat, "Triggering EVENT_ATTACK due to attack by %s", other->GetName());
		parse->EventNPC(EVENT_ATTACK, this, other, "", 0);
	}

	attacked_timer.Start(CombatEventTimer_expire);
	SetPrimaryAggro(true);

	if (!IsEngaged())
	{
		zone->AddAggroMob();
	}

	//do a majority of the work...
	CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);

	if(damage > 0) {
		//see if we are gunna start fleeing
		if(!IsPet() && !IsCasting()) CheckFlee();
	}
}

bool NPC::Death(Mob* killerMob, int32 damage, uint16 spell, SkillUseTypes attack_skill, uint8 killedby) {
	Log.Out(Logs::Detail, Logs::Combat, "Fatal blow dealt by %s with %d damage, spell %d, skill %d", killerMob->GetName(), damage, spell, attack_skill);
	bool MadeCorpse = false;
	uint16 OrigEntID = this->GetID();
	Mob *oos = nullptr;
	if(killerMob) {
		oos = killerMob->GetOwnerOrSelf();

		char buffer[48] = { 0 };
		snprintf(buffer, 47, "%d %d %d %d", killerMob ? killerMob->GetID() : 0, damage, spell, static_cast<int>(attack_skill));
		if(parse->EventNPC(EVENT_DEATH, this, oos, buffer, 0) != 0)
		{
			if(GetHP() < 0) {
				SetHP(0);
			}
			return false;
		}

		if(killerMob && killerMob->IsClient() && (spell != SPELL_UNKNOWN) && damage > 0) {
			char val1[20]={0};
		}
	} else {

		char buffer[48] = { 0 };
		snprintf(buffer, 47, "%d %d %d %d", killerMob ? killerMob->GetID() : 0, damage, spell, static_cast<int>(attack_skill));
		if(parse->EventNPC(EVENT_DEATH, this, nullptr, buffer, 0) != 0)
		{
			if(GetHP() < 0) {
				SetHP(0);
			}
			return false;
		}
	}

	if (IsEngaged())
	{
		zone->DelAggroMob();
		Log.Out(Logs::Detail, Logs::Attack, "%s Mobs currently Aggro %i", __FUNCTION__, zone->MobsAggroCount());
	}
	SetHP(0);
	DepopPet();		// NPC pets should stay alive when the owner dies.  Pets should be fixed to keep attacking
					// after the master's death and this method call removed when somebody gets around to it

	if (GetSwarmOwner()){
		Mob* owner = entity_list.GetMobID(GetSwarmOwner());
		if (owner)
			owner->SetTempPetCount(owner->GetTempPetCount() - 1);
	}

	Mob* killer = GetHateDamageTop(this);

	entity_list.RemoveFromTargets(this);

	if(killer && HasPrimaryAggro())
	{
		if(!entity_list.TransferPrimaryAggro(killer))
			Log.Out(Logs::Detail, Logs::Aggro, "%s failed to transfer primary aggro.", GetName());
	}

	if(p_depop == true)
		return false;

	SendRealPosition();

	HasAISpellEffects = false;
	BuffFadeAll();
	uint8 killed_level = GetLevel();

	EQApplicationPacket* app= new EQApplicationPacket(OP_Death,sizeof(Death_Struct));
	Death_Struct* d = (Death_Struct*)app->pBuffer;
	d->spawn_id = GetID();
	d->killer_id = killerMob ? killerMob->GetID() : 0;
	d->bindzoneid = 0;
	d->spell_id = spell == SPELL_UNKNOWN ? 0xffffffff : spell;
	d->attack_skill = SkillDamageTypes[attack_skill];
	d->damage = damage;
	app->priority = 6;
	entity_list.QueueClients(killerMob, app, false);

	if(respawn2) {
		respawn2->DeathReset(1);
	}

	if (killerMob) {
		hate_list.Add(killerMob, damage);
	}

	safe_delete(app);

	Mob *give_exp = hate_list.GetDamageTop(this);
	if(killerMob)
	{
		if(oos && oos->IsNPC())
			give_exp = oos;
	}

	if(give_exp == nullptr)
		give_exp = killer;

	if(give_exp && give_exp->HasOwner()) {

		bool ownerInGroup = false;
		if((give_exp->HasGroup() && give_exp->GetGroup()->IsGroupMember(give_exp->GetUltimateOwner()))
			|| (give_exp->IsPet() && (give_exp->GetOwner()->IsClient()
			|| ( give_exp->GetOwner()->HasGroup() && give_exp->GetOwner()->GetGroup()->IsGroupMember(give_exp->GetOwner()->GetUltimateOwner())))))
			ownerInGroup = true;

		give_exp = give_exp->GetUltimateOwner();

	}

	int PlayerCount = 0; // QueryServ Player Counting

	Client *give_exp_client = nullptr;
	if(give_exp && give_exp->IsClient())
		give_exp_client = give_exp->CastToClient();

	//do faction hits even if we are a merchant, so long as a player killed us
	if (give_exp_client)
		hate_list.DoFactionHits(GetNPCFactionID());

	// NPC is a player pet (duel/pvp)
	if(IsPet() && GetOwner()->IsClient())
	{
		give_exp_client = nullptr;
	}

	if (give_exp_client && !IsCorpse() && MerchantType == 0 && class_ != MERCHANT)
	{
		Group *kg = entity_list.GetGroupByClient(give_exp_client);
		Raid *kr = entity_list.GetRaidByClient(give_exp_client);

		int32 finalxp = static_cast<int32>(GetBaseEXP());
		finalxp = give_exp_client->mod_client_xp(finalxp, this);

		if(kr)
		{
			/* Send the EVENT_KILLED_MERIT event for all raid members */
			for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
				if (kr->members[i].member != nullptr && kr->members[i].member->IsClient()) { // If Group Member is Client
					Client *c = kr->members[i].member;
					parse->EventNPC(EVENT_KILLED_MERIT, this, c, "killed", 0);

					if(RuleB(NPC, EnableMeritBasedFaction))
						c->SetFactionLevel(c->CharacterID(), GetNPCFactionID(), c->GetBaseClass(), c->GetBaseRace(), c->GetDeity());

					mod_npc_killed_merit(kr->members[i].member);

					PlayerCount++;
				}
			}

			// QueryServ Logging - Raid Kills
			if(RuleB(QueryServ, PlayerLogNPCKills)){
				ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLogNPCKills, sizeof(QSPlayerLogNPCKill_Struct) + (sizeof(QSPlayerLogNPCKillsPlayers_Struct) * PlayerCount));
				PlayerCount = 0;
				QSPlayerLogNPCKill_Struct* QS = (QSPlayerLogNPCKill_Struct*) pack->pBuffer;
				QS->s1.NPCID = this->GetNPCTypeID();
				QS->s1.ZoneID = this->GetZoneID();
				QS->s1.Type = 2; // Raid Fight
				for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
					if (kr->members[i].member != nullptr && kr->members[i].member->IsClient()) { // If Group Member is Client
						Client *c = kr->members[i].member;
						QS->Chars[PlayerCount].char_id = c->CharacterID();
						PlayerCount++;
					}
				}
				worldserver.SendPacket(pack); // Send Packet to World
				safe_delete(pack);
			}
			// End QueryServ Logging

		}
		else if (give_exp_client->IsGrouped() && kg != nullptr)
		{
			kg->SplitExp((finalxp), this);
			if(killerMob && (kg->IsGroupMember(killerMob->GetName()) || kg->IsGroupMember(killerMob->GetUltimateOwner()->GetName())))
				killerMob->TrySpellOnKill(killed_level,spell);

			/* Send the EVENT_KILLED_MERIT event and update kill tasks
			* for all group members */
			for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
				if (kg->members[i] != nullptr && kg->members[i]->IsClient()) { // If Group Member is Client
					Client *c = kg->members[i]->CastToClient();
					parse->EventNPC(EVENT_KILLED_MERIT, this, c, "killed", 0);

					if(RuleB(NPC, EnableMeritBasedFaction))
						c->SetFactionLevel(c->CharacterID(), GetNPCFactionID(), c->GetBaseClass(), c->GetBaseRace(), c->GetDeity());

					mod_npc_killed_merit(c);

					PlayerCount++;
				}
			}

			// QueryServ Logging - Group Kills
			if(RuleB(QueryServ, PlayerLogNPCKills)){
				ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLogNPCKills, sizeof(QSPlayerLogNPCKill_Struct) + (sizeof(QSPlayerLogNPCKillsPlayers_Struct) * PlayerCount));
				PlayerCount = 0;
				QSPlayerLogNPCKill_Struct* QS = (QSPlayerLogNPCKill_Struct*) pack->pBuffer;
				QS->s1.NPCID = this->GetNPCTypeID();
				QS->s1.ZoneID = this->GetZoneID();
				QS->s1.Type = 1; // Group Fight
				for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
					if (kg->members[i] != nullptr && kg->members[i]->IsClient()) { // If Group Member is Client
						Client *c = kg->members[i]->CastToClient();
						QS->Chars[PlayerCount].char_id = c->CharacterID();
						PlayerCount++;
					}
				}
				worldserver.SendPacket(pack); // Send Packet to World
				safe_delete(pack);
			}
			// End QueryServ Logging
		}
		else
		{
			int conlevel = give_exp_client->GetLevelCon(GetLevel());
			if (conlevel != CON_GREEN)
			{
				give_exp_client->AddEXP((finalxp), conlevel);
				if(killerMob && (killerMob->GetID() == give_exp_client->GetID() || killerMob->GetUltimateOwner()->GetID() == give_exp_client->GetID()))
					killerMob->TrySpellOnKill(killed_level,spell);
			}
			 /* Send the EVENT_KILLED_MERIT event */
			parse->EventNPC(EVENT_KILLED_MERIT, this, give_exp_client, "killed", 0);

			if(RuleB(NPC, EnableMeritBasedFaction))
				give_exp_client->SetFactionLevel(give_exp_client->CharacterID(), GetNPCFactionID(), give_exp_client->GetBaseClass(),
					give_exp_client->GetBaseRace(), give_exp_client->GetDeity());

			mod_npc_killed_merit(give_exp_client);

			// QueryServ Logging - Solo
			if(RuleB(QueryServ, PlayerLogNPCKills)){
				ServerPacket* pack = new ServerPacket(ServerOP_QSPlayerLogNPCKills, sizeof(QSPlayerLogNPCKill_Struct) + (sizeof(QSPlayerLogNPCKillsPlayers_Struct) * 1));
				QSPlayerLogNPCKill_Struct* QS = (QSPlayerLogNPCKill_Struct*) pack->pBuffer;
				QS->s1.NPCID = this->GetNPCTypeID();
				QS->s1.ZoneID = this->GetZoneID();
				QS->s1.Type = 0; // Solo Fight
				Client *c = give_exp_client;
				QS->Chars[0].char_id = c->CharacterID();
				PlayerCount++;
				worldserver.SendPacket(pack); // Send Packet to World
				safe_delete(pack);
			}
			// End QueryServ Logging
		}
	}

	DeleteInvalidQuestLoot();

	if (give_exp_client && !HasOwner() && class_ != MERCHANT && !GetSwarmInfo()
		&& MerchantType == 0 && killer && (killer->IsClient() || (killer->HasOwner() && killer->GetUltimateOwner()->IsClient()) ||
		(killer->IsNPC() && killer->CastToNPC()->GetSwarmInfo() && killer->CastToNPC()->GetSwarmInfo()->GetOwner() && killer->CastToNPC()->GetSwarmInfo()->GetOwner()->IsClient())))
	{
		if(killer != 0)
		{
			if(killer->GetOwner() != 0 && killer->GetOwner()->IsClient())
				killer = killer->GetOwner();

			if(!killer->CastToClient()->GetGM() && killer->IsClient())
				this->CheckMinMaxLevel(killer);
		}
		uint16 emoteid = this->GetEmoteID();

		Corpse* corpse = new Corpse(this, &itemlist, GetNPCTypeID(), &NPCTypedata,level>54?RuleI(NPC,MajorNPCCorpseDecayTimeMS):RuleI(NPC,MinorNPCCorpseDecayTimeMS));
		MadeCorpse = true;
		entity_list.LimitRemoveNPC(this);
		entity_list.AddCorpse(corpse, GetID());

		entity_list.RemoveNPC(GetID());
		this->SetID(0);
		if(killer != 0 && emoteid != 0)
			corpse->CastToNPC()->DoNPCEmote(AFTERDEATH, emoteid);
		if(killer != 0 && killer->IsClient()) {
			corpse->AllowPlayerLoot(killer, 0);
			if(killer->IsGrouped()) {
				Group* group = entity_list.GetGroupByClient(killer->CastToClient());
				if(group != 0) {
					for(int i=0;i<6;i++) { // Doesnt work right, needs work
						if(group->members[i] != nullptr) {
							corpse->AllowPlayerLoot(group->members[i],i);
						}
					}
				}
			}
			else if(killer->IsRaidGrouped()){
				Raid* r = entity_list.GetRaidByClient(killer->CastToClient());
				if(r){
					int i = 0;
					for(int x = 0; x < MAX_RAID_MEMBERS; x++)
					{
						switch(r->GetLootType())
						{
						case 0:
						case 1:
							if(r->members[x].member && r->members[x].IsRaidLeader){
								corpse->AllowPlayerLoot(r->members[x].member, i);
								i++;
							}
							break;
						case 2:
							if(r->members[x].member && r->members[x].IsRaidLeader){
								corpse->AllowPlayerLoot(r->members[x].member, i);
								i++;
							}
							else if(r->members[x].member && r->members[x].IsGroupLeader){
								corpse->AllowPlayerLoot(r->members[x].member, i);
								i++;
							}
							break;
						case 3:
							if(r->members[x].member && r->members[x].IsLooter){
								corpse->AllowPlayerLoot(r->members[x].member, i);
								i++;
							}
							break;
						case 4:
							if(r->members[x].member)
							{
								corpse->AllowPlayerLoot(r->members[x].member, i);
								i++;
							}
							break;
						}
					}
				}
			}
		}
	}

	/* Web Interface: Entity Death  */
	if (RemoteCallSubscriptionHandler::Instance()->IsSubscribed("Entity.Events")) {
		std::vector<std::string> params;
		params.push_back(std::to_string((long)EntityEvents::Entity_Death));
		params.push_back(std::to_string((long)OrigEntID));
		params.push_back(std::to_string((bool)MadeCorpse));
		RemoteCallSubscriptionHandler::Instance()->OnEvent("Entity.Events", params);
	}

	// Parse quests even if we're killed by an NPC
	if(oos) {
		mod_npc_killed(oos);

		uint16 emoteid = this->GetEmoteID();
		if(emoteid != 0)
			this->DoNPCEmote(ONDEATH, emoteid);
		if(oos->IsNPC())
		{
			parse->EventNPC(EVENT_NPC_SLAY, oos->CastToNPC(), this, "", 0);
			uint16 emoteid = oos->GetEmoteID();
			if(emoteid != 0)
				oos->CastToNPC()->DoNPCEmote(KILLEDNPC, emoteid);
			killerMob->TrySpellOnKill(killed_level, spell);
		}
	}

	WipeHateList();
	p_depop = true;
	if(killerMob && killerMob->GetTarget() == this) //we can kill things without having them targeted
		killerMob->SetTarget(nullptr); //via AE effects and such..

	char buffer[48] = { 0 };
	snprintf(buffer, 47, "%d %d %d %d", killerMob ? killerMob->GetID() : 0, damage, spell, static_cast<int>(attack_skill));
	parse->EventNPC(EVENT_DEATH_COMPLETE, this, oos, buffer, 0);
	
	return true;
}

void Mob::AddToHateList(Mob* other, int32 hate, int32 damage, bool iYellForHelp, bool bFrenzy, bool iBuffTic)
{
	if (!other || other == this)
		return;

	if(damage < 0){
		hate = 1;
	}

	if(!iYellForHelp)
	{
		SetAssistAggro(true);
	}
	else
	{
		SetPrimaryAggro(true);
	}

	bool wasengaged = IsEngaged();
	Mob* owner = other->GetOwner();
	Mob* mypet = this->GetPet();
	Mob* myowner = this->GetOwner();
	Mob* targetmob = this->GetTarget();

	if(other){
		AddRampage(other);
		int hatemod = 100 + other->spellbonuses.hatemod + other->itembonuses.hatemod + other->aabonuses.hatemod;

		int32 shieldhatemod = other->spellbonuses.ShieldEquipHateMod + other->itembonuses.ShieldEquipHateMod + other->aabonuses.ShieldEquipHateMod;

		if (shieldhatemod && other->HasShieldEquiped())
			hatemod += shieldhatemod;

		if(hatemod < 1)
			hatemod = 1;
		hate = ((hate * (hatemod))/100);
	}

	if(IsPet() && GetOwner() && GetOwner()->GetAA(aaPetDiscipline) && IsHeld() && !IsFocused()) { //ignore aggro if hold and !focus
		return;
	}

	if(IsPet() && GetOwner() && GetOwner()->GetAA(aaPetDiscipline) && IsHeld() && GetOwner()->GetAA(aaAdvancedPetDiscipline) >= 1 && IsFocused()) {
		if (!targetmob)
			return;
	}

	if (other->IsNPC() && (other->IsPet() || other->CastToNPC()->GetSwarmOwner() > 0))
		TryTriggerOnValueAmount(false, false, false, true);

	if(IsClient() && !IsAIControlled())
		return;

	if(IsFamiliar() || GetSpecialAbility(IMMUNE_AGGRO))
		return;

	if (other == myowner)
		return;

	if(other->GetSpecialAbility(IMMUNE_AGGRO_ON))
		return;

	if(GetSpecialAbility(NPC_TUNNELVISION)) {
		int tv_mod = GetSpecialAbilityParam(NPC_TUNNELVISION, 0);

		Mob *top = GetTarget();
		if(top && top != other) {
			if(tv_mod) {
				float tv = tv_mod / 100.0f;
				hate *= tv;
			} else {
				hate *= RuleR(Aggro, TunnelVisionAggroMod);
			}
		}
	}

	if(IsNPC() && CastToNPC()->IsUnderwaterOnly() && zone->HasWaterMap()) {
		if(!zone->watermap->InLiquid(glm::vec3(other->GetPosition()))) {
			return;
		}
	}
	// first add self

	// The damage on the hate list is used to award XP to the killer. This check is to prevent Killstealing.
	// e.g. Mob has 5000 hit points, Player A melees it down to 500 hp, Player B executes a headshot (10000 damage).
	// If we add 10000 damage, Player B would get the kill credit, so we only award damage credit to player B of the
	// amount of HP the mob had left.
	//
	if(damage > GetHP())
		damage = GetHP();

	if (spellbonuses.ImprovedTaunt[1] && (GetLevel() < spellbonuses.ImprovedTaunt[0])
		&& other &&  (buffs[spellbonuses.ImprovedTaunt[2]].casterid != other->GetID()))
		hate = (hate*spellbonuses.ImprovedTaunt[1])/100;

	hate_list.Add(other, hate, damage, bFrenzy, !iBuffTic);

	// then add pet owner if there's one
	if (owner) { // Other is a pet, add him and it
		// EverHood 6/12/06
		// Can't add a feigned owner to hate list
		if(owner->IsClient() && owner->CastToClient()->GetFeigned()) {
			//they avoid hate due to feign death...
		} else {
			// cb:2007-08-17
			// owner must get on list, but he's not actually gained any hate yet
			if(!owner->GetSpecialAbility(IMMUNE_AGGRO))
			{
				hate_list.Add(owner, 0, 0, false, !iBuffTic);
			}
		}
	}

	if (mypet && (!(GetAA(aaPetDiscipline) && mypet->IsHeld()))) 
	{ // I have a pet, add other to it
		if(!mypet->IsFamiliar() && !mypet->GetSpecialAbility(IMMUNE_AGGRO) && HasPrimaryAggro())
			mypet->hate_list.Add(other, 0, 0, bFrenzy);
	} 
	else if (myowner) 
	{ // I am a pet, add other to owner if it's NPC/LD
		if (myowner->IsAIControlled() && !myowner->GetSpecialAbility(IMMUNE_AGGRO))
			myowner->hate_list.Add(other, 0, 0, bFrenzy);
	}

	if (other->GetTempPetCount())
		entity_list.AddTempPetsToHateList(other, this, bFrenzy);

	if (!wasengaged) {
		if(IsNPC() && other->IsClient() && other->CastToClient())
			parse->EventNPC(EVENT_AGGRO, this->CastToNPC(), other, "", 0);
		AI_Event_Engaged(other, iYellForHelp);
	}
}

// solar: this is called from Damage() when 'this' is attacked by 'other.
// 'this' is the one being attacked
// 'other' is the attacker
// a damage shield causes damage (or healing) to whoever attacks the wearer
// a reverse ds causes damage to the wearer whenever it attack someone
// given this, a reverse ds must be checked each time the wearer is attacking
// and not when they're attacked
//a damage shield on a spell is a negative value but on an item it's a positive value so add the spell value and subtract the item value to get the end ds value
void Mob::DamageShield(Mob* attacker, bool spell_ds) {
	
	if(!attacker || this == attacker)
		return;

	int DS = 0;
	int rev_ds = 0;
	uint16 spellid = 0;

	if(!spell_ds)
	{
		DS = spellbonuses.DamageShield;
		rev_ds = attacker->spellbonuses.ReverseDamageShield;
		
		if(spellbonuses.DamageShieldSpellID != 0 && spellbonuses.DamageShieldSpellID != SPELL_UNKNOWN)
			spellid = spellbonuses.DamageShieldSpellID;
	}
	else {
		DS = spellbonuses.SpellDamageShield;
		rev_ds = 0;
		// This ID returns "you are burned", seemed most appropriate for spell DS
		spellid = 2166;
	}

	if(DS == 0 && rev_ds == 0)
		return;

	Log.Out(Logs::Detail, Logs::Combat, "Applying Damage Shield of value %d to %s", DS, attacker->GetName());

	//invert DS... spells yield negative values for a true damage shield
	if(DS < 0) {
		if(!spell_ds)
		{
			DS += aabonuses.DamageShield; //Live AA - coat of thistles. (negative value)
		}
		
		attacker->Damage(this, -DS, spellid, SkillAbjuration/*hackish*/, false);
		//we can assume there is a spell now
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Damage, sizeof(CombatDamage_Struct));
		CombatDamage_Struct* cds = (CombatDamage_Struct*)outapp->pBuffer;
		cds->target = attacker->GetID();
		cds->source = GetID();
		cds->type = spellbonuses.DamageShieldType;
		cds->spellid = spellid;
		cds->damage = DS;
		entity_list.QueueCloseClients(this, outapp);
		safe_delete(outapp);
	} else if (DS > 0 && !spell_ds) {
		//we are healing the attacker...
		attacker->HealDamage(DS);
		//TODO: send a packet???
	}

	//Reverse DS
	//this is basically a DS, but the spell is on the attacker, not the attackee
	//if we've gotten to this point, we know we know "attacker" hit "this" (us) for damage & we aren't invulnerable
	uint16 rev_ds_spell_id = SPELL_UNKNOWN;

	if(spellbonuses.ReverseDamageShieldSpellID != 0 && spellbonuses.ReverseDamageShieldSpellID != SPELL_UNKNOWN)
		rev_ds_spell_id = spellbonuses.ReverseDamageShieldSpellID;

	if(rev_ds < 0) {
		Log.Out(Logs::Detail, Logs::Combat, "Applying Reverse Damage Shield of value %d to %s", rev_ds, attacker->GetName());
		attacker->Damage(this, -rev_ds, rev_ds_spell_id, SkillAbjuration/*hackish*/, false); //"this" (us) will get the hate, etc. not sure how this works on Live, but it'll works for now, and tanks will love us for this
		//do we need to send a damage packet here also?
	}
}

uint8 Mob::GetWeaponDamageBonus( const Item_Struct *Weapon )
{
	// This function calculates and returns the damage bonus for the weapon identified by the parameter "Weapon".
	// Modified 9/21/2008 by Cantus

	// Assert: This function should only be called for hits by the mainhand, as damage bonuses apply only to the
	// weapon in the primary slot. Be sure to check that Hand == MainPrimary before calling.

	// Assert: The caller should ensure that Weapon is actually a weapon before calling this function.
	// The ItemInst::IsWeapon() method can be used to quickly determine this.

	// Assert: This function should not be called if the player's level is below 28, as damage bonuses do not begin
	// to apply until level 28.

	// Assert: This function should not be called unless the player is a melee class, as casters do not receive a damage bonus.

	if( Weapon == nullptr || Weapon->ItemType == ItemType1HSlash || Weapon->ItemType == ItemType1HBlunt || Weapon->ItemType == ItemTypeMartial || Weapon->ItemType == ItemType1HPiercing )
	{
		// The weapon in the player's main (primary) hand is a one-handed weapon, or there is no item equipped at all.
		//
		// According to player posts on Allakhazam, 1H damage bonuses apply to bare fists (nothing equipped in the mainhand,
		// as indicated by Weapon == nullptr).
		//
		// The following formula returns the correct damage bonus for all 1H weapons:

		return (uint8) ((GetLevel() - 25) / 3);
	}

	// If we've gotten to this point, the weapon in the mainhand is a two-handed weapon.
	// Calculating damage bonuses for 2H weapons is more complicated, as it's based on PC level AND the delay of the weapon.
	// The formula to calculate 2H bonuses is HIDEOUS. It's a huge conglomeration of ternary operators and multiple operations.
	//
	// The following is a hybrid approach. In cases where the Level and Delay merit a formula that does not use many operators,
	// the formula is used. In other cases, lookup tables are used for speed.
	// Though the following code may look bloated and ridiculous, it's actually a very efficient way of calculating these bonuses.

	// Player Level is used several times in the code below, so save it into a variable.
	// If GetLevel() were an ordinary function, this would DEFINITELY make sense, as it'd cut back on all of the function calling
	// overhead involved with multiple calls to GetLevel(). But in this case, GetLevel() is a simple, inline accessor method.
	// So it probably doesn't matter. If anyone knows for certain that there is no overhead involved with calling GetLevel(),
	// as I suspect, then please feel free to delete the following line, and replace all occurences of "ucPlayerLevel" with "GetLevel()".
	uint8 ucPlayerLevel = (uint8) GetLevel();

	// The following may look cleaner, and would certainly be easier to understand, if it was
	// a simple 53x150 cell matrix.
	//
	// However, that would occupy 7,950 Bytes of memory (7.76 KB), and would likely result
	// in "thrashing the cache" when performing lookups.
	//
	// Initially, I thought the best approach would be to reverse-engineer the formula used by
	// Sony/Verant to calculate these 2H weapon damage bonuses. But the more than Reno and I
	// worked on figuring out this formula, the more we're concluded that the formula itself ugly
	// (that is, it contains so many operations and conditionals that it's fairly CPU intensive).
	// Because of that, we're decided that, in most cases, a lookup table is the most efficient way
	// to calculate these damage bonuses.
	//
	// The code below is a hybrid between a pure formulaic approach and a pure, brute-force
	// lookup table. In cases where a formula is the best bet, I use a formula. In other places
	// where a formula would be ugly, I use a lookup table in the interests of speed.

	if( Weapon->Delay <= 27 )
	{
		// Damage Bonuses for all 2H weapons with delays of 27 or less are identical.
		// They are the same as the damage bonus would be for a corresponding 1H weapon, plus one.
		// This formula applies to all levels 28-80, and will probably continue to apply if

		// the level cap on Live ever is increased beyond 80.

		return (ucPlayerLevel - 22) / 3;
	}

	if( ucPlayerLevel == 65 && Weapon->Delay <= 59 )
	{
		// Consider these two facts:
		// * Level 65 is the maximum level on many EQ Emu servers.
		// * If you listed the levels of all characters logged on to a server, odds are that the number you'll
		// see most frequently is level 65. That is, there are more level 65 toons than any other single level.
		//
		// Therefore, if we can optimize this function for level 65 toons, we're speeding up the server!
		//
		// With that goal in mind, I create an array of Damage Bonuses for level 65 characters wielding 2H weapons with
		// delays between 28 and 59 (inclusive). I suspect that this one small lookup array will therefore handle
		// many of the calls to this function.

		static const uint8 ucLevel65DamageBonusesForDelays28to59[] = {35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 42, 42, 42, 45, 45, 47, 48, 49, 49, 51, 51, 52, 53, 54, 54, 56, 56, 57, 58, 59};

		return ucLevel65DamageBonusesForDelays28to59[Weapon->Delay-28];
	}

	if( ucPlayerLevel > 65 )
	{
		if( ucPlayerLevel > 80 )
		{
			// As level 80 is currently the highest achievable level on Live, we only include
			// damage bonus information up to this level.
			//
			// If there is a custom EQEmu server that allows players to level beyond 80, the
			// damage bonus for their 2H weapons will simply not increase beyond their damage
			// bonus at level 80.

			ucPlayerLevel = 80;
		}

		// Lucy does not list a chart of damage bonuses for players levels 66+,
		// so my original version of this function just applied the level 65 damage
		// bonus for level 66+ toons. That sucked for higher level toons, as their
		// 2H weapons stopped ramping up in DPS as they leveled past 65.
		//
		// Thanks to the efforts of two guys, this is no longer the case:
		//
		// Janusd (Zetrakyl) ran a nifty query against the PEQ item database to list
		// the name of an example 2H weapon that represents each possible unique 2H delay.
		//
		// Romai then wrote an excellent script to automatically look up each of those
		// weapons, open the Lucy item page associated with it, and iterate through all
		// levels in the range 66 - 80. He saved the damage bonus for that weapon for
		// each level, and that forms the basis of the lookup tables below.

		if( Weapon->Delay <= 59 )
		{
			static const uint8 ucDelay28to59Levels66to80[32][15]=
			{
			/*							Level:								*/
			/*	 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80	*/

				{36, 37, 38, 39, 41, 42, 43, 44, 45, 47, 49, 49, 49, 50, 53},	/* Delay = 28 */
				{36, 38, 38, 39, 42, 43, 43, 45, 46, 48, 49, 50, 51, 52, 54},	/* Delay = 29 */
				{37, 38, 39, 40, 43, 43, 44, 46, 47, 48, 50, 51, 52, 53, 55},	/* Delay = 30 */
				{37, 39, 40, 40, 43, 44, 45, 46, 47, 49, 51, 52, 52, 52, 54},	/* Delay = 31 */
				{38, 39, 40, 41, 44, 45, 45, 47, 48, 48, 50, 52, 53, 55, 57},	/* Delay = 32 */
				{38, 40, 41, 41, 44, 45, 46, 48, 49, 50, 52, 53, 54, 56, 58},	/* Delay = 33 */
				{39, 40, 41, 42, 45, 46, 47, 48, 49, 51, 53, 54, 55, 57, 58},	/* Delay = 34 */
				{39, 41, 42, 43, 46, 46, 47, 49, 50, 52, 54, 55, 56, 57, 59},	/* Delay = 35 */
				{40, 41, 42, 43, 46, 47, 48, 50, 51, 53, 55, 55, 56, 58, 60},	/* Delay = 36 */
				{40, 42, 43, 44, 47, 48, 49, 50, 51, 53, 55, 56, 57, 59, 61},	/* Delay = 37 */
				{41, 42, 43, 44, 47, 48, 49, 51, 52, 54, 56, 57, 58, 60, 62},	/* Delay = 38 */
				{41, 43, 44, 45, 48, 49, 50, 52, 53, 55, 57, 58, 59, 61, 63},	/* Delay = 39 */
				{43, 45, 46, 47, 50, 51, 52, 54, 55, 57, 59, 60, 61, 63, 65},	/* Delay = 40 */
				{43, 45, 46, 47, 50, 51, 52, 54, 55, 57, 59, 60, 61, 63, 65},	/* Delay = 41 */
				{44, 46, 47, 48, 51, 52, 53, 55, 56, 58, 60, 61, 62, 64, 66},	/* Delay = 42 */
				{46, 48, 49, 50, 53, 54, 55, 58, 59, 61, 63, 64, 65, 67, 69},	/* Delay = 43 */
				{47, 49, 50, 51, 54, 55, 56, 58, 59, 61, 64, 65, 66, 68, 70},	/* Delay = 44 */
				{48, 50, 51, 52, 56, 57, 58, 60, 61, 63, 65, 66, 68, 70, 72},	/* Delay = 45 */
				{50, 52, 53, 54, 57, 58, 59, 62, 63, 65, 67, 68, 69, 71, 74},	/* Delay = 46 */
				{50, 52, 53, 55, 58, 59, 60, 62, 63, 66, 68, 69, 70, 72, 74},	/* Delay = 47 */
				{51, 53, 54, 55, 58, 60, 61, 63, 64, 66, 69, 69, 71, 73, 75},	/* Delay = 48 */
				{52, 54, 55, 57, 60, 61, 62, 65, 66, 68, 70, 71, 73, 75, 77},	/* Delay = 49 */
				{53, 55, 56, 57, 61, 62, 63, 65, 67, 69, 71, 72, 74, 76, 78},	/* Delay = 50 */
				{53, 55, 57, 58, 61, 62, 64, 66, 67, 69, 72, 73, 74, 77, 79},	/* Delay = 51 */
				{55, 57, 58, 59, 63, 64, 65, 68, 69, 71, 74, 75, 76, 78, 81},	/* Delay = 52 */
				{57, 55, 59, 60, 63, 65, 66, 68, 70, 72, 74, 76, 77, 79, 82},	/* Delay = 53 */
				{56, 58, 59, 61, 64, 65, 67, 69, 70, 73, 75, 76, 78, 80, 82},	/* Delay = 54 */
				{57, 59, 61, 62, 66, 67, 68, 71, 72, 74, 77, 78, 80, 82, 84},	/* Delay = 55 */
				{58, 60, 61, 63, 66, 68, 69, 71, 73, 75, 78, 79, 80, 83, 85},	/* Delay = 56 */

				/* Important Note: Janusd's search for 2H weapons did not find	*/
				/* any 2H weapon with a delay of 57. Therefore the values below	*/
				/* are interpolated, not exact!									*/
				{59, 61, 62, 64, 67, 69, 70, 72, 74, 76, 77, 78, 81, 84, 86},	/* Delay = 57 INTERPOLATED */

				{60, 62, 63, 65, 68, 70, 71, 74, 75, 78, 80, 81, 83, 85, 88},	/* Delay = 58 */

				/* Important Note: Janusd's search for 2H weapons did not find	*/
				/* any 2H weapon with a delay of 59. Therefore the values below	*/
				/* are interpolated, not exact!									*/
				{60, 62, 64, 65, 69, 70, 72, 74, 76, 78, 81, 82, 84, 86, 89},	/* Delay = 59 INTERPOLATED */
			};

			return ucDelay28to59Levels66to80[Weapon->Delay-28][ucPlayerLevel-66];
		}
		else
		{
			// Delay is 60+

			const static uint8 ucDelayOver59Levels66to80[6][15] =
			{
			/*							Level:								*/
			/*	 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80	*/

				{61, 63, 65, 66, 70, 71, 73, 75, 77, 79, 82, 83, 85, 87, 90},				/* Delay = 60 */
				{65, 68, 69, 71, 75, 76, 78, 80, 82, 85, 87, 89, 91, 93, 96},				/* Delay = 65 */

				/* Important Note: Currently, the only 2H weapon with a delay	*/
				/* of 66 is not player equippable (it's None/None). So I'm		*/
				/* leaving it commented out to keep this table smaller.			*/
				//{66, 68, 70, 71, 75, 77, 78, 81, 83, 85, 88, 90, 91, 94, 97},				/* Delay = 66 */

				{70, 72, 74, 76, 80, 81, 83, 86, 88, 88, 90, 95, 97, 99, 102},				/* Delay = 70 */
				{82, 85, 87, 89, 89, 94, 98, 101, 103, 106, 109, 111, 114, 117, 120},		/* Delay = 85 */
				{90, 93, 96, 98, 103, 105, 107, 111, 113, 116, 120, 122, 125, 128, 131},	/* Delay = 95 */

				/* Important Note: Currently, the only 2H weapons with delay	*/
				/* 100 are GM-only items purchased from vendors in Sunset Home	*/
				/* (cshome). Because they are highly unlikely to be used in		*/
				/* combat, I'm commenting it out to keep the table smaller.		*/
				//{95, 98, 101, 103, 108, 110, 113, 116, 119, 122, 126, 128, 131, 134, 138},/* Delay = 100 */

				{136, 140, 144, 148, 154, 157, 161, 166, 170, 174, 179, 183, 187, 191, 196}	/* Delay = 150 */
			};

			if( Weapon->Delay < 65 )
			{
				return ucDelayOver59Levels66to80[0][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 70 )
			{
				return ucDelayOver59Levels66to80[1][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 85 )
			{
				return ucDelayOver59Levels66to80[2][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 95 )
			{
				return ucDelayOver59Levels66to80[3][ucPlayerLevel-66];
			}
			else if( Weapon->Delay < 150 )
			{
				return ucDelayOver59Levels66to80[4][ucPlayerLevel-66];
			}
			else
			{
				return ucDelayOver59Levels66to80[5][ucPlayerLevel-66];
			}
		}
	}

	// If we've gotten to this point in the function without hitting a return statement,
	// we know that the character's level is between 28 and 65, and that the 2H weapon's
	// delay is 28 or higher.

	// The Damage Bonus values returned by this function (in the level 28-65 range) are
	// based on a table of 2H Weapon Damage Bonuses provided by Lucy at the following address:
	// http://lucy.allakhazam.com/dmgbonus.html

	if( Weapon->Delay <= 39 )
	{
		if( ucPlayerLevel <= 53)
		{
			// The Damage Bonus for all 2H weapons with delays between 28 and 39 (inclusive) is the same for players level 53 and below...
			static const uint8 ucDelay28to39LevelUnder54[] = {1, 1, 2, 3, 3, 3, 4, 5, 5, 6, 6, 6, 8, 8, 8, 9, 9, 10, 11, 11, 11, 12, 13, 14, 16, 17};

			// As a note: The following formula accurately calculates damage bonuses for 2H weapons with delays in the range 28-39 (inclusive)
			// for characters levels 28-50 (inclusive):
			// return ( (ucPlayerLevel - 22) / 3 ) + ( (ucPlayerLevel - 25) / 5 );
			//
			// However, the small lookup array used above is actually much faster. So we'll just use it instead of the formula
			//
			// (Thanks to Reno for helping figure out the above formula!)

			return ucDelay28to39LevelUnder54[ucPlayerLevel-28];
		}
		else
		{
			// Use a matrix to look up the damage bonus for 2H weapons with delays between 28 and 39 wielded by characters level 54 and above.
			static const uint8 ucDelay28to39Level54to64[12][11] =
			{
			/*						Level:					*/
			/*	 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64	*/

				{17, 21, 21, 23, 25, 26, 28, 30, 31, 31, 33},	/* Delay = 28 */
				{17, 21, 22, 23, 25, 26, 29, 30, 31, 32, 34},	/* Delay = 29 */
				{18, 21, 22, 23, 25, 27, 29, 31, 32, 32, 34},	/* Delay = 30 */
				{18, 21, 22, 23, 25, 27, 29, 31, 32, 33, 34},	/* Delay = 31 */
				{18, 21, 22, 24, 26, 27, 30, 32, 32, 33, 35},	/* Delay = 32 */
				{18, 21, 22, 24, 26, 27, 30, 32, 33, 34, 35},	/* Delay = 33 */
				{18, 22, 22, 24, 26, 28, 30, 32, 33, 34, 36},	/* Delay = 34 */
				{18, 22, 23, 24, 26, 28, 31, 33, 34, 34, 36},	/* Delay = 35 */
				{18, 22, 23, 25, 27, 28, 31, 33, 34, 35, 37},	/* Delay = 36 */
				{18, 22, 23, 25, 27, 29, 31, 33, 34, 35, 37},	/* Delay = 37 */
				{18, 22, 23, 25, 27, 29, 32, 34, 35, 36, 38},	/* Delay = 38 */
				{18, 22, 23, 25, 27, 29, 32, 34, 35, 36, 38}	/* Delay = 39 */
			};

			return ucDelay28to39Level54to64[Weapon->Delay-28][ucPlayerLevel-54];
		}
	}
	else if( Weapon->Delay <= 59 )
	{
		if( ucPlayerLevel <= 52 )
		{
			if( Weapon->Delay <= 45 )
			{
				static const uint8 ucDelay40to45Levels28to52[6][25] =
				{
				/*												Level:														*/
				/*	 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52		*/

					{2,  2,  3,  4,  4,  4,  5,  6,  6,  7,  7,  7,  9,  9,  9,  10, 10, 11, 12, 12, 12, 13, 14, 16, 18},	/* Delay = 40 */
					{2,  2,  3,  4,  4,  4,  5,  6,  6,  7,  7,  7,  9,  9,  9,  10, 10, 11, 12, 12, 12, 13, 14, 16, 18},	/* Delay = 41 */
					{2,  2,  3,  4,  4,  4,  5,  6,  6,  7,  7,  7,  9,  9,  9,  10, 10, 11, 12, 12, 12, 13, 14, 16, 18},	/* Delay = 42 */
					{4,  4,  5,  6,  6,  6,  7,  8,  8,  9,  9,  9,  11, 11, 11, 12, 12, 13, 14, 14, 14, 15, 16, 18, 20},	/* Delay = 43 */
					{4,  4,  5,  6,  6,  6,  7,  8,  8,  9,  9,  9,  11, 11, 11, 12, 12, 13, 14, 14, 14, 15, 16, 18, 20},	/* Delay = 44 */
					{5,  5,  6,  7,  7,  7,  8,  9,  9,  10, 10, 10, 12, 12, 12, 13, 13, 14, 15, 15, 15, 16, 17, 19, 21} 	/* Delay = 45 */
				};

				return ucDelay40to45Levels28to52[Weapon->Delay-40][ucPlayerLevel-28];
			}
			else
			{
				static const uint8 ucDelay46Levels28to52[] = {6, 6, 7, 8, 8, 8, 9, 10, 10, 11, 11, 11, 13, 13, 13, 14, 14, 15, 16, 16, 16, 17, 18, 20, 22};

				return ucDelay46Levels28to52[ucPlayerLevel-28] + ((Weapon->Delay-46) / 3);
			}
		}
		else
		{
			// Player is in the level range 53 - 64

			// Calculating damage bonus for 2H weapons with a delay between 40 and 59 (inclusive) involves, unforunately, a brute-force matrix lookup.
			static const uint8 ucDelay40to59Levels53to64[20][37] =
			{
			/*						Level:							*/
			/*	 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64		*/

				{19, 20, 24, 25, 27, 29, 31, 34, 36, 37, 38, 40},	/* Delay = 40 */
				{19, 20, 24, 25, 27, 29, 31, 34, 36, 37, 38, 40},	/* Delay = 41 */
				{19, 20, 24, 25, 27, 29, 31, 34, 36, 37, 38, 40},	/* Delay = 42 */
				{21, 22, 26, 27, 29, 31, 33, 37, 39, 40, 41, 43},	/* Delay = 43 */
				{21, 22, 26, 27, 29, 32, 34, 37, 39, 40, 41, 43},	/* Delay = 44 */
				{22, 23, 27, 28, 31, 33, 35, 38, 40, 42, 43, 45},	/* Delay = 45 */
				{23, 24, 28, 30, 32, 34, 36, 40, 42, 43, 44, 46},	/* Delay = 46 */
				{23, 24, 29, 30, 32, 34, 37, 40, 42, 43, 44, 47},	/* Delay = 47 */
				{23, 24, 29, 30, 32, 35, 37, 40, 43, 44, 45, 47},	/* Delay = 48 */
				{24, 25, 30, 31, 34, 36, 38, 42, 44, 45, 46, 49},	/* Delay = 49 */
				{24, 26, 30, 31, 34, 36, 39, 42, 44, 46, 47, 49},	/* Delay = 50 */
				{24, 26, 30, 31, 34, 36, 39, 42, 45, 46, 47, 49},	/* Delay = 51 */
				{25, 27, 31, 33, 35, 38, 40, 44, 46, 47, 49, 51},	/* Delay = 52 */
				{25, 27, 31, 33, 35, 38, 40, 44, 46, 48, 49, 51},	/* Delay = 53 */
				{26, 27, 32, 33, 36, 38, 41, 44, 47, 48, 49, 52},	/* Delay = 54 */
				{27, 28, 33, 34, 37, 39, 42, 46, 48, 50, 51, 53},	/* Delay = 55 */
				{27, 28, 33, 34, 37, 40, 42, 46, 49, 50, 51, 54},	/* Delay = 56 */
				{27, 28, 33, 34, 37, 40, 43, 46, 49, 50, 52, 54},	/* Delay = 57 */
				{28, 29, 34, 36, 39, 41, 44, 48, 50, 52, 53, 56},	/* Delay = 58 */
				{28, 29, 34, 36, 39, 41, 44, 48, 51, 52, 54, 56}	/* Delay = 59 */
			};

			return ucDelay40to59Levels53to64[Weapon->Delay-40][ucPlayerLevel-53];
		}
	}
	else
	{
		// The following table allows us to look up Damage Bonuses for weapons with delays greater than or equal to 60.
		//
		// There aren't a lot of 2H weapons with a delay greater than 60. In fact, both a database and Lucy search run by janusd confirm
		// that the only unique 2H delays greater than 60 are: 65, 70, 85, 95, and 150.
		//
		// To be fair, there are also weapons with delays of 66 and 100. But they are either not equippable (None/None), or are
		// only available to GMs from merchants in Sunset Home (cshome). In order to keep this table "lean and mean", I will not
		// include the values for delays 66 and 100. If they ever are wielded, the 66 delay weapon will use the 65 delay bonuses,
		// and the 100 delay weapon will use the 95 delay bonuses. So it's not a big deal.
		//
		// Still, if someone in the future decides that they do want to include them, here are the tables for these two delays:
		//
		// {12, 12, 13, 14, 14, 14, 15, 16, 16, 17, 17, 17, 19, 19, 19, 20, 20, 21, 22, 22, 22, 23, 24, 26, 29, 30, 32, 37, 39, 42, 45, 48, 53, 55, 57, 59, 61, 64}		/* Delay = 66 */
		// {24, 24, 25, 26, 26, 26, 27, 28, 28, 29, 29, 29, 31, 31, 31, 32, 32, 33, 34, 34, 34, 35, 36, 39, 43, 45, 48, 55, 57, 62, 66, 71, 77, 80, 83, 85, 89, 92}		/* Delay = 100 */
		//
		// In case there are 2H weapons added in the future with delays other than those listed above (and until the damage bonuses
		// associated with that new delay are added to this function), this function is designed to do the following:
		//
		//		For weapons with delays in the range 60-64, use the Damage Bonus that would apply to a 2H weapon with delay 60.
		//		For weapons with delays in the range 65-69, use the Damage Bonus that would apply to a 2H weapon with delay 65
		//		For weapons with delays in the range 70-84, use the Damage Bonus that would apply to a 2H weapon with delay 70.
		//		For weapons with delays in the range 85-94, use the Damage Bonus that would apply to a 2H weapon with delay 85.
		//		For weapons with delays in the range 95-149, use the Damage Bonus that would apply to a 2H weapon with delay 95.
		//		For weapons with delays 150 or higher, use the Damage Bonus that would apply to a 2H weapon with delay 150.

		static const uint8 ucDelayOver59Levels28to65[6][38] =
		{
		/*																	Level:																					*/
		/*	 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64. 65	*/

			{10, 10, 11, 12, 12, 12, 13, 14, 14, 15, 15, 15, 17, 17, 17, 18, 18, 19, 20, 20, 20, 21, 22, 24, 27, 28, 30, 35, 36, 39, 42, 45, 49, 51, 53, 54, 57, 59},		/* Delay = 60 */
			{12, 12, 13, 14, 14, 14, 15, 16, 16, 17, 17, 17, 19, 19, 19, 20, 20, 21, 22, 22, 22, 23, 24, 26, 29, 30, 32, 37, 39, 42, 45, 48, 52, 55, 57, 58, 61, 63},		/* Delay = 65 */
			{14, 14, 15, 16, 16, 16, 17, 18, 18, 19, 19, 19, 21, 21, 21, 22, 22, 23, 24, 24, 24, 25, 26, 28, 31, 33, 35, 40, 42, 45, 48, 52, 56, 59, 61, 62, 65, 68},		/* Delay = 70 */
			{19, 19, 20, 21, 21, 21, 22, 23, 23, 24, 24, 24, 26, 26, 26, 27, 27, 28, 29, 29, 29, 30, 31, 34, 37, 39, 41, 47, 49, 54, 57, 61, 66, 69, 72, 74, 77, 80},		/* Delay = 85 */
			{22, 22, 23, 24, 24, 24, 25, 26, 26, 27, 27, 27, 29, 29, 29, 30, 30, 31, 32, 32, 32, 33, 34, 37, 40, 43, 45, 52, 54, 59, 62, 67, 73, 76, 79, 81, 84, 88},		/* Delay = 95 */
			{40, 40, 41, 42, 42, 42, 43, 44, 44, 45, 45, 45, 47, 47, 47, 48, 48, 49, 50, 50, 50, 51, 52, 56, 61, 65, 69, 78, 82, 89, 94, 102, 110, 115, 119, 122, 127, 132}	/* Delay = 150 */
		};

		if( Weapon->Delay < 65 )
		{
			return ucDelayOver59Levels28to65[0][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 70 )
		{
			return ucDelayOver59Levels28to65[1][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 85 )
		{
			return ucDelayOver59Levels28to65[2][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 95 )
		{
			return ucDelayOver59Levels28to65[3][ucPlayerLevel-28];
		}
		else if( Weapon->Delay < 150 )
		{
			return ucDelayOver59Levels28to65[4][ucPlayerLevel-28];
		}
		else
		{
			return ucDelayOver59Levels28to65[5][ucPlayerLevel-28];
		}
	}
}

int Mob::GetMonkHandToHandDamage(void)
{
	// Kaiyodo - Determine a monk's fist damage. Table data from www.monkly-business.com
	// saved as static array - this should speed this function up considerably
	static int damage[66] = {
	// 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
		99, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,
		 8, 8, 8, 8, 8, 9, 9, 9, 9, 9,10,10,10,10,10,11,11,11,11,11,
		12,12,12,12,12,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,
		14,14,15,15,15,15 };

	// Have a look to see if we have epic fists on

	if (IsClient() && CastToClient()->GetItemIDAt(12) == 10652)
		return(9);
	else
	{
		int Level = GetLevel();
		if (Level > 65)
			return(19);
		else
			return damage[Level];
	}
}

int Mob::GetMonkHandToHandDelay(void)
{
	// Kaiyodo - Determine a monk's fist delay. Table data from www.monkly-business.com
	// saved as static array - this should speed this function up considerably
	static int delayshuman[66] = {
	//  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19
		99,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,
		36,36,36,36,36,35,35,35,35,35,34,34,34,34,34,33,33,33,33,33,
		32,32,32,32,32,31,31,31,31,31,30,30,30,29,29,29,28,28,28,27,
		26,24,22,20,20,20  };
	static int delaysiksar[66] = {
	//  0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19
		99,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,
		36,36,36,36,36,36,36,36,36,36,35,35,35,35,35,34,34,34,34,34,
		33,33,33,33,33,32,32,32,32,32,31,31,31,30,30,30,29,29,29,28,
		27,24,22,20,20,20 };

	// Have a look to see if we have epic fists on
	if (IsClient() && CastToClient()->GetItemIDAt(12) == 10652)
		return(16);
	else
	{
		int Level = GetLevel();
		if (GetRace() == HUMAN)
		{
			if (Level > 65)
				return(24);
			else
				return delayshuman[Level];
		}
		else	//heko: iksar table
		{
			if (Level > 65)
				return(25);
			else
				return delaysiksar[Level];
		}
	}
}

int32 Mob::ReduceDamage(int32 damage)
{
	if(damage <= 0)
		return damage;

	int32 slot = -1;

	if(damage < 1)
		return -6;

	if (spellbonuses.MeleeRune[0] && spellbonuses.MeleeRune[1] >= 0)
		damage = RuneAbsorb(damage, SE_Rune);

	if(damage < 1)
		return -6;

	return(damage);
}

int32 Mob::AffectMagicalDamage(int32 damage, uint16 spell_id, const bool iBuffTic, Mob* attacker)
{
	if(damage <= 0)
		return damage;

	bool DisableSpellRune = false;
	int32 slot = -1;

	// If this is a DoT, use DoT Shielding...
	if(iBuffTic) {
		damage -= (damage * itembonuses.DoTShielding / 100);

		if (spellbonuses.MitigateDotRune[0]){
			slot = spellbonuses.MitigateDotRune[1];
			if(slot >= 0)
			{
				int damage_to_reduce = damage * spellbonuses.MitigateDotRune[0] / 100;

				if (spellbonuses.MitigateDotRune[2] && (damage_to_reduce > spellbonuses.MitigateDotRune[2]))
					damage_to_reduce = spellbonuses.MitigateDotRune[2];

				if(spellbonuses.MitigateDotRune[3] && (damage_to_reduce >= buffs[slot].dot_rune))
				{
					damage -= buffs[slot].dot_rune;
					if(!TryFadeEffect(slot))
						BuffFadeBySlot(slot);
				}
				else
				{
					if (spellbonuses.MitigateDotRune[3])
						buffs[slot].dot_rune = (buffs[slot].dot_rune - damage_to_reduce);

					damage -= damage_to_reduce;
				}
			}
		}
	}

	// This must be a DD then so lets apply Spell Shielding and runes.
	else
	{
		// Reduce damage by the Spell Shielding first so that the runes don't take the raw damage.
		damage -= (damage * itembonuses.SpellShield / 100);

		// Do runes now.
		if (spellbonuses.MitigateSpellRune[0] && !DisableSpellRune){
			slot = spellbonuses.MitigateSpellRune[1];
			if(slot >= 0)
			{
				int damage_to_reduce = damage * spellbonuses.MitigateSpellRune[0] / 100;

				if (spellbonuses.MitigateSpellRune[2] && (damage_to_reduce > spellbonuses.MitigateSpellRune[2]))
					damage_to_reduce = spellbonuses.MitigateSpellRune[2];

				if(spellbonuses.MitigateSpellRune[3] && (damage_to_reduce >= buffs[slot].magic_rune))
				{
					Log.Out(Logs::Detail, Logs::Spells, "Mob::ReduceDamage SE_MitigateSpellDamage %d damage negated, %d"
						" damage remaining, fading buff.", damage_to_reduce, buffs[slot].magic_rune);
					damage -= buffs[slot].magic_rune;
					if(!TryFadeEffect(slot))
						BuffFadeBySlot(slot);
				}
				else
				{
					Log.Out(Logs::Detail, Logs::Spells, "Mob::ReduceDamage SE_MitigateMeleeDamage %d damage negated, %d"
						" damage remaining.", damage_to_reduce, buffs[slot].magic_rune);

					if (spellbonuses.MitigateSpellRune[3])
						buffs[slot].magic_rune = (buffs[slot].magic_rune - damage_to_reduce);

					damage -= damage_to_reduce;
				}
			}
		}

		if(damage < 1)
			return 0;

		//Regular runes absorb spell damage (except dots) - Confirmed on live.
		if (spellbonuses.MeleeRune[0] && spellbonuses.MeleeRune[1] >= 0)
			damage = RuneAbsorb(damage, SE_Rune);

		if (spellbonuses.AbsorbMagicAtt[0] && spellbonuses.AbsorbMagicAtt[1] >= 0)
			damage = RuneAbsorb(damage, SE_AbsorbMagicAtt);

		if(damage < 1)
			return 0;
	}
	return damage;
}

bool Mob::HasProcs() const
{
	for (int i = 0; i < MAX_PROCS; i++)
		if (PermaProcs[i].spellID != SPELL_UNKNOWN || SpellProcs[i].spellID != SPELL_UNKNOWN)
			return true;
	return false;
}

bool Client::CheckDoubleAttack(bool tripleAttack) {

	//Check for bonuses that give you a double attack chance regardless of skill (ie Bestial Frenzy/Harmonious Attack AA)
	uint32 bonusGiveDA = aabonuses.GiveDoubleAttack + spellbonuses.GiveDoubleAttack + itembonuses.GiveDoubleAttack;

	if(!HasSkill(SkillDoubleAttack) && !bonusGiveDA)
		return false;

	float chance = 0.0f;

	uint16 skill = GetSkill(SkillDoubleAttack);

	int32 bonusDA = aabonuses.DoubleAttackChance + spellbonuses.DoubleAttackChance + itembonuses.DoubleAttackChance;

	//Use skill calculations otherwise, if you only have AA applied GiveDoubleAttack chance then use that value as the base.
	if (skill)
		chance = (float(skill+GetLevel()) * (float(100.0f+bonusDA+bonusGiveDA) /100.0f)) /500.0f;
	else
		chance = (float(bonusGiveDA) * (float(100.0f+bonusDA)/100.0f) ) /100.0f;

	//Live now uses a static Triple Attack skill (lv 46 = 2% lv 60 = 20%) - We do not have this skill on EMU ATM.
	//A reasonable forumla would then be TA = 20% * chance
	//AA's can also give triple attack skill over cap. (ie Burst of Power) NOTE: Skill ID in spell data is 76 (Triple Attack)
	//Kayen: Need to decide if we can implement triple attack skill before working in over the cap effect.
	if(tripleAttack) {
		// Only some Double Attack classes get Triple Attack [This is already checked in client_processes.cpp]
		int32 triple_bonus = spellbonuses.TripleAttackChance + itembonuses.TripleAttackChance;
		chance *= 0.2f; //Baseline chance is 20% of your double attack chance.
		chance *= float(100.0f+triple_bonus)/100.0f; //Apply modifiers.
	}

	if(zone->random.Roll(chance))
		return true;

	return false;
}

bool Client::CheckDoubleRangedAttack() {

	int32 chance = spellbonuses.DoubleRangedAttack + itembonuses.DoubleRangedAttack + aabonuses.DoubleRangedAttack;

	if(chance && zone->random.Roll(chance))
		return true;

	return false;
}

void Mob::CommonDamage(Mob* attacker, int32 &damage, const uint16 spell_id, const SkillUseTypes skill_used, bool &avoidable, const int8 buffslot, const bool iBuffTic) {

	// Agro pet someone tried to damage me
	if (!iBuffTic)
		AggroPet(attacker);

	if(!IsSelfConversionSpell(spell_id))
		CommonBreakInvisible();

	// This method is called with skill_used=ABJURE for Damage Shield damage.
	bool FromDamageShield = (skill_used == SkillAbjuration);

	Log.Out(Logs::Detail, Logs::Combat, "Applying damage %d done by %s with skill %d and spell %d, avoidable? %s, is %sa buff tic in slot %d",
		damage, attacker?attacker->GetName():"NOBODY", skill_used, spell_id, avoidable?"yes":"no", iBuffTic?"":"not ", buffslot);

	if ((GetInvul() || DivineAura()) && spell_id != SPELL_CAZIC_TOUCH) {
		Log.Out(Logs::Detail, Logs::Combat, "Avoiding %d damage due to invulnerability.", damage);
		damage = -5;
	}

	if( spell_id != SPELL_UNKNOWN || attacker == nullptr )
		avoidable = false;

	// only apply DS if physical damage (no spell damage)
	// damage shield calls this function with spell_id set, so its unavoidable
	if (attacker && damage > 0 && spell_id == SPELL_UNKNOWN && skill_used != SkillArchery && skill_used != SkillThrowing) {
		DamageShield(attacker);
	}

	if (spell_id == SPELL_UNKNOWN && skill_used) {
		CheckNumHitsRemaining(NUMHIT_IncomingHitAttempts);

		if (attacker)
			attacker->CheckNumHitsRemaining(NUMHIT_OutgoingHitAttempts);
	}

	if(attacker){
		if(attacker->IsClient()){
			if (!RuleB(Combat, EXPFromDmgShield)) {
			// Damage shield damage shouldn't count towards who gets EXP
				if(!attacker->CastToClient()->GetFeigned() && !FromDamageShield)
					AddToHateList(attacker, 0, damage, true, false, iBuffTic);
			}
			else {
				if(!attacker->CastToClient()->GetFeigned())
					AddToHateList(attacker, 0, damage, true, false, iBuffTic);
			}
		}
		else
			AddToHateList(attacker, 0, damage, true, false, iBuffTic);
	}

	if(damage > 0) {
		//if there is some damage being done and theres an attacker involved
		if(attacker) {
			if(spell_id == SPELL_HARM_TOUCH2 && attacker->IsClient() && attacker->CastToClient()->CheckAAEffect(aaEffectLeechTouch)){
				int healed = damage;
				healed = attacker->GetActSpellHealing(spell_id, healed);
				attacker->HealDamage(healed);
				entity_list.MessageClose(this, true, 300, MT_Emote, "%s beams a smile at %s", attacker->GetCleanName(), this->GetCleanName() );
				attacker->CastToClient()->DisableAAEffect(aaEffectLeechTouch);
			}

			// if spell is lifetap add hp to the caster
			if (spell_id != SPELL_UNKNOWN && IsLifetapSpell( spell_id )) {
				int healed = damage;

				healed = attacker->GetActSpellHealing(spell_id, healed);
				Log.Out(Logs::Detail, Logs::Combat, "Applying lifetap heal of %d to %s", healed, attacker->GetName());
				attacker->HealDamage(healed);

				//we used to do a message to the client, but its gone now.
				// emote goes with every one ... even npcs
				entity_list.MessageClose(this, true, 300, MT_Emote, "%s beams a smile at %s", attacker->GetCleanName(), this->GetCleanName() );
			}

			if(IsNPC())
			{
				total_damage += damage;

				if(attacker->IsClient())
					player_damage += damage;

				if(attacker->IsDireCharmed())
					dire_pet_damage += damage;
			}
		}	//end `if there is some damage being done and theres anattacker person involved`


		//see if any runes want to reduce this damage
		if(spell_id == SPELL_UNKNOWN) {
			damage = ReduceDamage(damage);
			Log.Out(Logs::Detail, Logs::Combat, "Melee Damage reduced to %d", damage);
		} else {
			int32 origdmg = damage;
			damage = AffectMagicalDamage(damage, spell_id, iBuffTic, attacker);
			if (origdmg != damage && attacker && attacker->IsClient()) {
				if(attacker->CastToClient()->GetFilter(FilterDamageShields) != FilterHide)
					attacker->Message(CC_Yellow, "The Spellshield absorbed %d of %d points of damage", origdmg - damage, origdmg);
			}
			if (damage == 0 && attacker && origdmg != damage && IsClient()) {
				//Kayen: Probably need to add a filter for this - Not sure if this msg is correct but there should be a message for spell negate/runes.
				Message(263, "%s tries to cast on YOU, but YOUR magical skin absorbs the spell.",attacker->GetCleanName());
			}
		}

		if (skill_used)
			CheckNumHitsRemaining(NUMHIT_IncomingHitSuccess);

		if(IsClient() && CastToClient()->sneaking){
			CastToClient()->sneaking = false;
			SendAppearancePacket(AT_Sneak, 0);
		}
		if(attacker && attacker->IsClient() && attacker->CastToClient()->sneaking){
			attacker->CastToClient()->sneaking = false;
			attacker->SendAppearancePacket(AT_Sneak, 0);
		}

		//final damage has been determined.

		SetHP(GetHP() - damage);

		if(HasDied()) {
			bool IsSaved = false;

			if(TryDivineSave()) {
				IsSaved = true;
            }

			if(!IsSaved && !TrySpellOnDeath()) {
				SetHP(-500);
				Death(attacker, damage, spell_id, skill_used);
			}
		}
		else{
			if(GetHPRatio() < 16)
				TryDeathSave();
		}

		TryTriggerOnValueAmount(true);

		//fade mez if we are mezzed
		if (IsMezzed() && attacker) {
			Log.Out(Logs::Detail, Logs::Combat, "Breaking mez due to attack.");
			BuffFadeByEffect(SE_Mez);
		}

		if(spell_id != SPELL_UNKNOWN && !iBuffTic) {
			//see if root will break
			if (IsRooted() && !FromDamageShield)  // neotoyko: only spells cancel root
				TryRootFadeByDamage(buffslot, attacker);
		}
		else if(spell_id == SPELL_UNKNOWN)
		{
			//increment chances of interrupting
			if(IsCasting()) { //shouldnt interrupt on regular spell damage
				attacked_count++;
				Log.Out(Logs::Detail, Logs::Combat, "Melee attack while casting. Attack count %d", attacked_count);
			}
		}

		//send an HP update if we are hurt
		if(GetHP() < GetMaxHP())
			SendHPUpdate();
	}	//end `if damage was done`

	//send damage packet...
	if(!iBuffTic) { //buff ticks do not send damage, instead they just call SendHPUpdate(), which is done below

		// hundreds of spells have the skill id set to tiger claw in the spell data
		// without this, lots of stuff will 'strike' instead of give a proper spell damage message
		uint8 skill_id = skill_used;
		if (skill_used == SkillTigerClaw && spell_id > 0 && (attacker->GetClass() != MONK || spell_id != SPELL_UNKNOWN))
		{
			skill_id = SkillEvocation;
		}

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_Damage, sizeof(CombatDamage_Struct));
		CombatDamage_Struct* a = (CombatDamage_Struct*)outapp->pBuffer;
		a->target = GetID();
		if (attacker == nullptr)
			a->source = 0;
		else if (attacker->IsClient() && attacker->CastToClient()->GMHideMe())
			a->source = 0;
		else
			a->source = attacker->GetID();
		a->type = SkillDamageTypes[skill_id]; // was 0x1c
		a->damage = damage;
		a->spellid = spell_id;

		if (damage > 0  && skill_used < 75) {
			// Push magnitudes in unknown11 are from client decompile
			switch (skill_used) {
			case Skill1HBlunt:
			case Skill1HSlashing:
			case SkillHandtoHand:
			case SkillThrowing:
				a->force = 0.1f;
				break;
			case Skill2HBlunt:
			case Skill2HSlashing:
			case SkillEagleStrike:
			case SkillKick:
				a->force = 0.2f;
				break;
			case SkillArchery:
				a->force = 0.15f;
				break;
			case SkillBackstab:
			case SkillBash:
				a->force = 0.3f;
				break;
			case SkillDragonPunch:
				a->force = 0.25f;
				break;
			case SkillFlyingKick:
				a->force = 0.4f;
				break;
			case Skill1HPiercing:
			case SkillFrenzy:
				a->force = 0.05f;
				break;
			default:
				a->force = 0.0f;
			}
			if (a->force> 0.0f)
				a->sequence = attacker->GetHeading() * 511.0f / 256.0f;

			if (IsNPC())
			{
				CastToNPC()->AddPush(attacker->GetHeading(), a->force);
			}
		}
		
		//Note: if players can become pets, they will not receive damage messages of their own
		//this was done to simplify the code here (since we can only effectively skip one mob on queue)
		eqFilterType filter;
		Mob *skip = attacker;
		if(attacker && attacker->GetOwnerID()) 
		{
			//attacker is a pet
			Mob* owner = attacker->GetOwner();
			if (owner && owner->IsClient()) 
			{
				if(damage > 0) 
				{
					if(spell_id != SPELL_UNKNOWN)
						filter = FilterNone;
					else
						filter = FilterOthersHit;
				} 
				else if(damage == -5)
					filter = FilterNone;	//cant filter invulnerable
				else
					filter = FilterOthersMiss;

				if(!FromDamageShield)
					owner->CastToClient()->QueuePacket(outapp,true,CLIENT_CONNECTED,filter);
			}
			skip = owner;
		} 
		else 
		{
			//attacker is not a pet, send to the attacker

			//if the attacker is a client, try them with the correct filter
			if(attacker && attacker->IsClient()) {
				if ((spell_id != SPELL_UNKNOWN || FromDamageShield) && damage > 0) {
					//special crap for spell damage, looks hackish to me	
					char val1[20]={0};
					if (FromDamageShield)
					{
						attacker->FilteredMessage_StringID(this,MT_NonMelee,FilterDamageShields,OTHER_HIT_NONMELEE,GetCleanName(),ConvertArray(damage,val1));
					}
					else if(attacker != this)
					{
						attacker->Message_StringID(MT_NonMelee,OTHER_HIT_NONMELEE,GetCleanName(),ConvertArray(damage,val1));
					}
				} else {
					if(damage > 0) {
						if(spell_id != SPELL_UNKNOWN)
							filter = FilterNone;
						else
							filter = FilterNone;	//cant filter our own hits
					} else if(damage == -5)
						filter = FilterNone;	//cant filter invulnerable
					else
						filter = FilterMyMisses;

					attacker->CastToClient()->QueuePacket(outapp, true, CLIENT_CONNECTED, filter);
				}
			}
			skip = attacker;
		}

		//send damage to all clients around except the specified skip mob (attacker or the attacker's owner) and ourself
		if(damage > 0) {
			if(spell_id != SPELL_UNKNOWN)
				filter = FilterNone;
			else
				filter = FilterOthersHit;
		} else if(damage == -5)
			filter = FilterNone;	//cant filter invulnerable
		else
			filter = FilterOthersMiss;
		//make attacker (the attacker) send the packet so we can skip them and the owner
		//this call will send the packet to `this` as well (using the wrong filter) (will not happen until PC charm works)
		// If this is Damage Shield damage, the correct OP_Damage packets will be sent from Mob::DamageShield, so
		// we don't send them here.
		if(!FromDamageShield) {
			entity_list.QueueCloseClients(this, outapp, true, 200, skip, true, filter);
			//send the damage to ourself if we are a client
			if(IsClient()) {
				//I dont think any filters apply to damage affecting us
				CastToClient()->QueuePacket(outapp);
			}
		}

		safe_delete(outapp);
	} //end packet sending

}

void Mob::HealDamage(uint32 amount, Mob *caster, uint16 spell_id)
{
	int32 maxhp = GetMaxHP();
	int32 curhp = GetHP();
	uint32 acthealed = 0;

	if (caster && amount > 0) {
		if (caster->IsNPC() && !caster->IsPet()) {
			float npchealscale = caster->CastToNPC()->GetHealScale();
			amount = (static_cast<float>(amount) * npchealscale) / 100.0f;
		}
	}

	if (amount > (maxhp - curhp))
		acthealed = (maxhp - curhp);
	else
		acthealed = amount;
	if (acthealed > 0) 
	{
		if (caster) 
		{
			// message to target	
			if (caster->IsClient()) 
			{
				FilteredMessage_StringID(this, MT_NonMelee, FilterNone, YOU_HEALED, itoa(acthealed));
			} 
			// message to caster
			if (caster->IsClient() && caster != this) 
			{
				caster->Message(MT_NonMelee, "You have healed %s for %i points of damage.", GetCleanName(), acthealed);
			}
		} 
		else 
		{
			FilteredMessage_StringID(this, MT_NonMelee, FilterNone, YOU_HEALED, itoa(acthealed));
		}
	}

	if (curhp < maxhp) {
		if ((curhp + amount) > maxhp)
			curhp = maxhp;
		else
			curhp += amount;
		SetHP(curhp);

		SendHPUpdate();
	}
}

float Mob::GetProcChance(uint16 hand)
{
	double chance = 0.0;
	double weapon_speed = GetWeaponSpeedbyHand(hand);
	weapon_speed /= 100.0;

	double dex = GetDEX();
	if (dex > 255.0)
		dex = 255.0;		// proc chance caps at 255 (possibly 250; need more logs)
	
	/* Kind of ugly, but results are very accurate
	   Proc chance is a linear function based on dexterity
	   0.0004166667 == base proc chance at 1 delay with 0 dex (~0.25 PPM for main hand)
	   1.1437908496732e-5 == chance increase per point of dex at 1 delay
	   Result is 0.25 PPM at 0 dex, 2 PPM at 255 dex
	*/
	chance = static_cast<float>((0.0004166667 + 1.1437908496732e-5 * dex) * weapon_speed);

	if (hand == MainSecondary)
	{
		chance /= 1.6f;				// parses show offhand procs at half the rate,
	}								// but dual wield doesn't always succeed, so
									// divide by 1.6 instead of 2.0
	
	Log.Out(Logs::Detail, Logs::Combat, "Proc chance %.2f", chance);
	return chance;
}

void Mob::TryWeaponProc(const ItemInst* weapon_g, Mob *on, uint16 hand) {
	if(!on) {
		SetTarget(nullptr);
		Log.Out(Logs::General, Logs::Error, "A null Mob object was passed to Mob::TryWeaponProc for evaluation!");
		return;
	}

	if (!IsAttackAllowed(on)) {
		Log.Out(Logs::Detail, Logs::Combat, "Preventing procing off of unattackable things.");
		return;
	}

	if(!weapon_g) {
		TrySpellProc(nullptr, (const Item_Struct*)nullptr, on);
		return;
	}

	if(!weapon_g->IsType(ItemClassCommon)) {
		TrySpellProc(nullptr, (const Item_Struct*)nullptr, on);
		return;
	}

	TryWeaponProc(weapon_g, weapon_g->GetItem(), on, hand);
	// Procs from Buffs and AA both melee and range
	TrySpellProc(weapon_g, weapon_g->GetItem(), on, hand);

	return;
}

void Mob::TryWeaponProc(const ItemInst *inst, const Item_Struct *weapon, Mob *on, uint16 hand)
{
	if (!weapon)
		return;

	float ProcChance = GetProcChance(hand);

	if (weapon->Proc.Type == ET_CombatProc)
	{
		float WPC = ProcChance * (100.0f + static_cast<float>(weapon->ProcRate)) / 100.0f;

		if (zone->random.Roll(WPC))
		{
			if (weapon->Proc.Level > GetLevel())
			{
				Log.Out(Logs::Detail, Logs::Combat,
						"Tried to proc (%s), but our level (%d) is lower than required (%d)",
						weapon->Name, GetLevel(), weapon->Proc.Level);

				if (IsPet())
				{
					Mob *own = GetOwner();
					if (own)
						own->Message_StringID(CC_Red, PROC_PETTOOLOW);
				}
				else
				{
					Message_StringID(CC_Red, PROC_TOOLOW);
				}
			}
			else
			{
				Log.Out(Logs::Detail, Logs::Combat,
						"Attacking weapon (%s) successfully procing spell %d (%.2f percent chance)",
						weapon->Name, weapon->Proc.Effect, WPC * 100);

				ExecWeaponProc(inst, weapon->Proc.Effect, on);
			}
		}
	}
	return;
}

void Mob::TrySpellProc(const ItemInst *inst, const Item_Struct *weapon, Mob *on, uint16 hand)
{
	float ProcChance = GetProcChance(hand);

	bool rangedattk = false;
	if (weapon && hand == MainRange) {
		if (weapon->ItemType == ItemTypeArrow ||
				weapon->ItemType == ItemTypeLargeThrowing ||
				weapon->ItemType == ItemTypeSmallThrowing ||
				weapon->ItemType == ItemTypeBow)
			rangedattk = true;
	}

	if (!weapon && hand == MainRange && GetSpecialAbility(SPECATK_RANGED_ATK))
		rangedattk = true;

	for (uint32 i = 0; i < MAX_PROCS; i++) {
		if (IsPet() && hand != MainPrimary) //Pets can only proc spell procs from their primay hand (ie; beastlord pets)
			continue; // If pets ever can proc from off hand, this will need to change

		// Not ranged
		if (!rangedattk) {
			// Perma procs (AAs)
			if (PermaProcs[i].spellID != SPELL_UNKNOWN) {
				if (zone->random.Roll(PermaProcs[i].chance)) { // TODO: Do these get spell bonus?
					Log.Out(Logs::Detail, Logs::Combat,
							"Permanent proc %d procing spell %d (%d percent chance)",
							i, PermaProcs[i].spellID, PermaProcs[i].chance);
					ExecWeaponProc(nullptr, PermaProcs[i].spellID, on);
				} else {
					Log.Out(Logs::Detail, Logs::Combat,
							"Permanent proc %d failed to proc %d (%d percent chance)",
							i, PermaProcs[i].spellID, PermaProcs[i].chance);
				}
			}

			// Spell procs (buffs)
			if (SpellProcs[i].spellID != SPELL_UNKNOWN) {
				float chance = ProcChance * (static_cast<float>(SpellProcs[i].chance) / 100.0f);
				if (zone->random.Roll(chance)) {
					Log.Out(Logs::Detail, Logs::Combat,
							"Spell proc %d procing spell %d (%.2f percent chance)",
							i, SpellProcs[i].spellID, chance);
					ExecWeaponProc(nullptr, SpellProcs[i].spellID, on);
					CheckNumHitsRemaining(NUMHIT_OffensiveSpellProcs, 0, SpellProcs[i].base_spellID);
				} else {
					Log.Out(Logs::Detail, Logs::Combat,
							"Spell proc %d failed to proc %d (%.2f percent chance)",
							i, SpellProcs[i].spellID, chance);
				}
			}
		}
	}

	return;
}

void Mob::TryCriticalHit(Mob *defender, uint16 skill, int32 &damage, ExtraAttackOptions *opts)
{
	if(damage < 1)
		return;

	float critChance = 0.0f;
	bool IsBerskerSPA = false;

	//1: Try Slay Undead
	if (defender && (defender->GetBodyType() == BT_Undead ||
				defender->GetBodyType() == BT_SummonedUndead || defender->GetBodyType() == BT_Vampire)) {
		int32 SlayRateBonus = aabonuses.SlayUndead[0] + itembonuses.SlayUndead[0] + spellbonuses.SlayUndead[0];
		if (SlayRateBonus) {
			float slayChance = static_cast<float>(SlayRateBonus) / 10000.0f;
			if (zone->random.Roll(slayChance)) {
				int32 SlayDmgBonus = aabonuses.SlayUndead[1] + itembonuses.SlayUndead[1] + spellbonuses.SlayUndead[1];
				damage = (damage * SlayDmgBonus * 2.25) / 100;
				if (GetGender() == 1) // female
					entity_list.FilteredMessageClose_StringID(this, false, 200,
							MT_CritMelee, FilterMeleeCrits, FEMALE_SLAYUNDEAD,
							GetCleanName(), itoa(damage));
				else // males and neuter I guess
					entity_list.FilteredMessageClose_StringID(this, false, 200,
							MT_CritMelee, FilterMeleeCrits, MALE_SLAYUNDEAD,
							GetCleanName(), itoa(damage));
				return;
			}
		}
	}

	//2: Try Melee Critical

	//Base critical rate for all classes is dervived from DEX stat, this rate is then augmented
	//by item,spell and AA bonuses allowing you a chance to critical hit. If the following rules
	//are defined you will have an innate chance to hit at Level 1 regardless of bonuses.
	//Warning: Do not define these rules if you want live like critical hits.
	critChance += RuleI(Combat, MeleeBaseCritChance);

	if (IsClient()) {
		critChance  += RuleI(Combat, ClientBaseCritChance);

		if (spellbonuses.BerserkSPA || itembonuses.BerserkSPA || aabonuses.BerserkSPA)
				IsBerskerSPA = true;

		if (((GetClass() == WARRIOR) && GetLevel() >= 12)  || IsBerskerSPA) {
			if (IsBerserk() || IsBerskerSPA)
				critChance += RuleI(Combat, BerserkBaseCritChance);
			else
				critChance += RuleI(Combat, WarBerBaseCritChance);
		}
	}

	int deadlyChance = 0;
	int deadlyMod = 0;
	if(skill == SkillArchery && GetClass() == RANGER && GetSkill(SkillArchery) >= 65)
		critChance += 6;

	if (skill == SkillThrowing && GetClass() == ROGUE && GetSkill(SkillThrowing) >= 65) {
		critChance += RuleI(Combat, RogueCritThrowingChance);
		deadlyChance = RuleI(Combat, RogueDeadlyStrikeChance);
		deadlyMod = RuleI(Combat, RogueDeadlyStrikeMod);
	}

	int CritChanceBonus = GetCriticalChanceBonus(skill);

	if (CritChanceBonus || critChance) {

		//Get Base CritChance from Dex. (200 = ~1.6%, 255 = ~2.0%, 355 = ~2.20%) Fall off rate > 255
		//http://giline.versus.jp/shiden/su.htm , http://giline.versus.jp/shiden/damage_e.htm
		if (GetDEX() <= 255)
			critChance += (float(GetDEX()) / 125.0f);
		else if (GetDEX() > 255)
			critChance += (float(GetDEX()-255)/ 500.0f) + 2.0f;
		critChance += critChance*(float)CritChanceBonus /100.0f;
	}

	if(opts) {
		critChance *= opts->crit_percent;
		critChance += opts->crit_flat;
	}

	if(critChance > 0) {

		critChance /= 100;

		if(zone->random.Roll(critChance))
		{
			uint32 critMod = 165;
			bool crip_success = false;
			int32 CripplingBlowChance = GetCrippBlowChance();

			//Crippling Blow Chance: The percent value of the effect is applied
			//to the your Chance to Critical. (ie You have 10% chance to critical and you
			//have a 200% Chance to Critical Blow effect, therefore you have a 20% Chance to Critical Blow.
			if (CripplingBlowChance || (IsBerserk() || IsBerskerSPA)) {
				if (!IsBerserk() && !IsBerskerSPA)
					critChance *= float(CripplingBlowChance)/100.0f;

				if ((IsBerserk() || IsBerskerSPA) || zone->random.Roll(critChance)) {
					critMod = 280;
					crip_success = true;
				}
			}

			damage = damage * critMod / 100;

			bool deadlySuccess = false;
			if (deadlyChance && zone->random.Roll(static_cast<float>(deadlyChance) / 100.0f)) {
				if (BehindMob(defender, GetX(), GetY())) {
					damage *= deadlyMod;
					deadlySuccess = true;
				}
			}

			if (crip_success) {
				entity_list.FilteredMessageClose_StringID(this, false, 200,
						MT_CritMelee, FilterMeleeCrits, CRIPPLING_BLOW,
						GetCleanName(), itoa(damage));
				// Crippling blows also have a chance to stun
				//Kayen: Crippling Blow would cause a chance to interrupt for npcs < 55, with a staggers message.
				if (defender != nullptr && defender->GetLevel() <= 55 && !defender->GetSpecialAbility(IMMUNE_STUN)){
					defender->Emote("staggers.");
					defender->Stun(0, this);
				}
			} else if (deadlySuccess) {
				entity_list.FilteredMessageClose_StringID(this, false, 200,
						MT_CritMelee, FilterMeleeCrits, DEADLY_STRIKE,
						GetCleanName(), itoa(damage));
			} else {
				entity_list.FilteredMessageClose_StringID(this, false, 200,
						MT_CritMelee, FilterMeleeCrits, CRITICAL_HIT,
						GetCleanName(), itoa(damage));
			}
		}
	}
}

bool Mob::TryFinishingBlow(Mob *defender, SkillUseTypes skillinuse)
{
	if (defender && !defender->IsClient() && defender->GetHPRatio() < 10){

		uint32 FB_Dmg = aabonuses.FinishingBlow[1] + spellbonuses.FinishingBlow[1] + itembonuses.FinishingBlow[1];

		uint32 FB_Level = 0;
		FB_Level = aabonuses.FinishingBlowLvl[0];
		if (FB_Level < spellbonuses.FinishingBlowLvl[0])
			FB_Level = spellbonuses.FinishingBlowLvl[0];
		else if (FB_Level < itembonuses.FinishingBlowLvl[0])
			FB_Level = itembonuses.FinishingBlowLvl[0];

		//Proc Chance value of 500 = 5%
		int32 ProcChance = (aabonuses.FinishingBlow[0] + spellbonuses.FinishingBlow[0] + spellbonuses.FinishingBlow[0])/10;

		if(FB_Level && FB_Dmg && (defender->GetLevel() <= FB_Level) && (ProcChance >= zone->random.Int(0, 1000))){
			entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, FINISHING_BLOW, GetName());
			DoSpecialAttackDamage(defender, skillinuse, FB_Dmg, 1, -1, 10, false, false);
			return true;
		}
	}
	return false;
}

void Mob::DoRiposte(Mob* defender) {
	Log.Out(Logs::Detail, Logs::Combat, "Preforming a riposte");

	if (!defender)
		return;

	if (defender->IsClient())
	{
		if (defender->CastToClient()->IsUnconscious() || defender->IsStunned() || defender->CastToClient()->IsSitting()
			|| defender->GetAppearance() == eaDead || defender->GetAppearance() == eaCrouching
		)
			return;
	}


	defender->Attack(this, MainPrimary, true);
	if (HasDied()) return;

	int32 DoubleRipChance = defender->aabonuses.GiveDoubleRiposte[0] +
							defender->spellbonuses.GiveDoubleRiposte[0] +
							defender->itembonuses.GiveDoubleRiposte[0];

	DoubleRipChance		 =  defender->aabonuses.DoubleRiposte +
							defender->spellbonuses.DoubleRiposte +
							defender->itembonuses.DoubleRiposte;

	//Live AA - Double Riposte
	if(DoubleRipChance && zone->random.Roll(DoubleRipChance)) {
		Log.Out(Logs::Detail, Logs::Combat, "Preforming a double riposed (%d percent chance)", DoubleRipChance);
		defender->Attack(this, MainPrimary, true);
		if (HasDied()) return;
	}

	//Double Riposte effect, allows for a chance to do RIPOSTE with a skill specfic special attack (ie Return Kick).
	//Coded narrowly: Limit to one per client. Limit AA only. [1 = Skill Attack Chance, 2 = Skill]

	DoubleRipChance = defender->aabonuses.GiveDoubleRiposte[1];

	if(DoubleRipChance && zone->random.Roll(DoubleRipChance)) {
	Log.Out(Logs::Detail, Logs::Combat, "Preforming a return SPECIAL ATTACK (%d percent chance)", DoubleRipChance);

		if (defender->GetClass() == MONK)
			defender->MonkSpecialAttack(this, defender->aabonuses.GiveDoubleRiposte[2]);
		else if (defender->IsClient())
			defender->CastToClient()->DoClassAttacks(this,defender->aabonuses.GiveDoubleRiposte[2], true);
	}
}

bool Mob::HasDied() {
	bool Result = false;

	if((GetHP()) <= 0)
		Result = true;

	return Result;
}

// returns the player character damage multiplier cap
uint16 Mob::GetDamageTable()
{
	// PC damage multiplier caps for levels 51-65, non-monks
	static int caps[15] = {
		245, 245, 245, 245, 245,		// 51-55
		265, 265, 265, 265,				// 56-59
		285, 285, 285,					// 60-62
		290, 290,						// 63-64
		295								// 65
	};

	// monk caps
	static int monk[15] = {
		245, 245, 245, 245, 245,		// 51-55
		285, 285, 285, 285,				// 56-59
		290, 290, 290,					// 60-62
		295, 295,						// 63-64
		300								// 65
	};

	if (!IsWarriorClass())
		return 210;

	uint8 level = GetLevel();

	if (level < 1)
		level = 1;
	else if (level > 65)
		level = 65;

	if (level <= 50)
	{
		if (GetClass() == MONK)
		{
			return 220;
		}
		else
		{
			return 210;
		}
	}
	else
	{
		if (GetClass() == MONK)
		{
			return monk[level - 51];
		}
		else
		{
			return caps[level - 51];
		}
	}
}

bool Mob::TryRootFadeByDamage(int buffslot, Mob* attacker) {

 	/*Dev Quote 2010: http://forums.station.sony.com/eq/posts/list.m?topic_id=161443
 	The Viscid Roots AA does the following: Reduces the chance for root to break by X percent.
 	There is no distinction of any kind between the caster inflicted damage, or anyone
 	else's damage. There is also no distinction between Direct and DOT damage in the root code.

 	/* General Mechanics
 	- Check buffslot to make sure damage from a root does not cancel the root
 	- If multiple roots on target, always and only checks first root slot and if broken only removes that slots root.
 	- Only roots on determental spells can be broken by damage.
	- Root break chance values obtained from live parses.
 	*/

	if (!attacker || !spellbonuses.Root[0] || spellbonuses.Root[1] < 0)
		return false;

	if (IsDetrimentalSpell(spellbonuses.Root[1]) && spellbonuses.Root[1] != buffslot){
		int BreakChance = RuleI(Spells, RootBreakFromSpells);

		BreakChance -= BreakChance*buffs[spellbonuses.Root[1]].RootBreakChance/100;
		int level_diff = attacker->GetLevel() - GetLevel();

		//Use baseline if level difference <= 1 (ie. If target is (1) level less than you, or equal or greater level)

		if (level_diff == 2)
			BreakChance = (BreakChance * 80) /100; //Decrease by 20%;

		else if (level_diff >= 3 && level_diff <= 20)
			BreakChance = (BreakChance * 60) /100; //Decrease by 40%;

		else if (level_diff > 21)
			BreakChance = (BreakChance * 20) /100; //Decrease by 80%;

		if (BreakChance < 1)
			BreakChance = 1;

		if (zone->random.Roll(BreakChance)) {

			if (!TryFadeEffect(spellbonuses.Root[1])) {
				BuffFadeBySlot(spellbonuses.Root[1]);
				Log.Out(Logs::Detail, Logs::Combat, "Spell broke root! BreakChance percent chance");
				return true;
			}
		}
	}

	Log.Out(Logs::Detail, Logs::Combat, "Spell did not break root. BreakChance percent chance");
	return false;
}

int32 Mob::RuneAbsorb(int32 damage, uint16 type)
{
	uint32 buff_max = GetMaxTotalSlots();
	if (type == SE_Rune){
		for(uint32 slot = 0; slot < buff_max; slot++) {
			if(slot == spellbonuses.MeleeRune[1] && spellbonuses.MeleeRune[0] && buffs[slot].melee_rune && IsValidSpell(buffs[slot].spellid)){
				int melee_rune_left = buffs[slot].melee_rune;

				if(melee_rune_left > damage)
				{
					melee_rune_left -= damage;
					buffs[slot].melee_rune = melee_rune_left;
					return -6;
				}

				else
				{
					if(melee_rune_left > 0)
						damage -= melee_rune_left;

					if(!TryFadeEffect(slot))
						BuffFadeBySlot(slot);
				}
			}
		}
	}


	else{
		for(uint32 slot = 0; slot < buff_max; slot++) {
			if(slot == spellbonuses.AbsorbMagicAtt[1] && spellbonuses.AbsorbMagicAtt[0] && buffs[slot].magic_rune && IsValidSpell(buffs[slot].spellid)){
				int magic_rune_left = buffs[slot].magic_rune;
				if(magic_rune_left > damage)
				{
					magic_rune_left -= damage;
					buffs[slot].magic_rune = magic_rune_left;
					return 0;
				}

				else
				{
					if(magic_rune_left > 0)
						damage -= magic_rune_left;

					if(!TryFadeEffect(slot))
						BuffFadeBySlot(slot);
				}
			}
		}
	}

	return damage;
}

void Mob::CommonOutgoingHitSuccess(Mob* defender, int32 &damage, SkillUseTypes skillInUse)
{
	if (!defender)
		return;

	// SE_SkillDamageTaken for Aggressive discipline
	damage += (damage * defender->GetSkillDmgTaken(skillInUse) / 100) + (GetSkillDmgAmt(skillInUse) + defender->GetFcDamageAmtIncoming(this, 0, true, skillInUse));

	TryCriticalHit(defender, skillInUse, damage);
	CheckNumHitsRemaining(NUMHIT_OutgoingHitSuccess);
}

void Mob::CommonBreakInvisible()
{
	//break invis when you attack
	if(invisible) {
		Log.Out(Logs::Detail, Logs::Combat, "Removing invisibility due to attack.");
		BuffFadeByEffect(SE_Invisibility);
		BuffFadeByEffect(SE_Invisibility2);
		invisible = false;
	}
	if(invisible_undead) {
		Log.Out(Logs::Detail, Logs::Combat, "Removing invisibility vs. undead due to attack.");
		BuffFadeByEffect(SE_InvisVsUndead);
		BuffFadeByEffect(SE_InvisVsUndead2);
		invisible_undead = false;
	}
	if(invisible_animals){
		Log.Out(Logs::Detail, Logs::Combat, "Removing invisibility vs. animals due to attack.");
		BuffFadeByEffect(SE_InvisVsAnimals);
		invisible_animals = false;
	}

	if (spellbonuses.NegateIfCombat)
		BuffFadeByEffect(SE_NegateIfCombat);

	if(hidden || improved_hidden){
		hidden = false;
		improved_hidden = false;
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
		SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
		sa_out->spawn_id = GetID();
		sa_out->type = 0x03;
		sa_out->parameter = 0;
		entity_list.QueueClients(this, outapp, true);
		safe_delete(outapp);
	}

	if (spellbonuses.NegateIfCombat)
		BuffFadeByEffect(SE_NegateIfCombat);

	hidden = false;
	improved_hidden = false;
}

/* Dev quotes:
 * Old formula
 *	 Final delay = (Original Delay / (haste mod *.01f)) + ((Hundred Hands / 100) * Original Delay)
 * New formula
 *	 Final delay = (Original Delay / (haste mod *.01f)) + ((Hundred Hands / 1000) * (Original Delay / (haste mod *.01f))
 * Base Delay	  20			  25			  30			  37
 * Haste		   2.25			2.25			2.25			2.25
 * HHE (old)	  -17			 -17			 -17			 -17
 * Final Delay	 5.488888889	 6.861111111	 8.233333333	 10.15444444
 *
 * Base Delay	  20			  25			  30			  37
 * Haste		   2.25			2.25			2.25			2.25
 * HHE (new)	  -383			-383			-383			-383
 * Final Delay	 5.484444444	 6.855555556	 8.226666667	 10.14622222
 *
 * Difference	 -0.004444444   -0.005555556   -0.006666667   -0.008222222
 *
 * These times are in 10th of a second
 */

void Mob::SetAttackTimer()
{
	attack_timer.SetAtTrigger(4000, true);
}

void Client::SetAttackTimer()
{
	float haste_mod = GetHaste() * 0.01f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetAtTrigger(4000, true);

	Timer *TimerToUse = nullptr;
	const Item_Struct *PrimaryWeapon = nullptr;

	for (int i = MainRange; i <= MainSecondary; i++) {
		//pick a timer
		if (i == MainPrimary)
			TimerToUse = &attack_timer;
		else if (i == MainRange)
			TimerToUse = &ranged_timer;
		else if (i == MainSecondary)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		const Item_Struct *ItemToUse = nullptr;

		//find our item
		ItemInst *ci = GetInv().GetItem(i);
		if (ci)
			ItemToUse = ci->GetItem();

		//special offhand stuff
		if (i == MainSecondary) {
			//if we have a 2H weapon in our main hand, no dual
			if (PrimaryWeapon != nullptr) {
				if (PrimaryWeapon->ItemClass == ItemClassCommon
						&& (PrimaryWeapon->ItemType == ItemType2HSlash
						|| PrimaryWeapon->ItemType == ItemType2HBlunt
						|| PrimaryWeapon->ItemType == ItemType2HPiercing)) {
					attack_dw_timer.Disable();
					continue;
				}
			}

			//if we cant dual wield, skip it
			if (!CanThisClassDualWield()) {
				attack_dw_timer.Disable();
				continue;
			}
		}

		//see if we have a valid weapon
		if (ItemToUse != nullptr) {
			//check type and damage/delay
			if (ItemToUse->ItemClass != ItemClassCommon
					|| ItemToUse->Damage == 0
					|| ItemToUse->Delay == 0) {
				//no weapon
				ItemToUse = nullptr;
			}
			// Check to see if skill is valid
			else if ((ItemToUse->ItemType > ItemTypeLargeThrowing) &&
					(ItemToUse->ItemType != ItemTypeMartial) &&
					(ItemToUse->ItemType != ItemType2HPiercing)) {
				//no weapon
				ItemToUse = nullptr;
			}
		}

		int hhe = itembonuses.HundredHands + spellbonuses.HundredHands;
		int speed = 0;
		int delay = 36;
		float quiver_haste = 0.0f;

		//if we have no weapon..
		if (ItemToUse == nullptr) {
			//above checks ensure ranged weapons do not fall into here
			// Work out if we're a monk
			if (GetClass() == MONK || GetClass() == BEASTLORD)
				delay = GetMonkHandToHandDelay();
		} else {
			//we have a weapon, use its delay
			delay = ItemToUse->Delay;
			if (ItemToUse->ItemType == ItemTypeBow || ItemToUse->ItemType == ItemTypeLargeThrowing)
				quiver_haste = GetQuiverHaste();
		}
		speed = static_cast<int>(((delay / haste_mod) + ((hhe / 100.0f) * delay)) * 100);
		// this is probably wrong
		if (quiver_haste > 0)
			speed *= static_cast<int>(quiver_haste);
		TimerToUse->SetAtTrigger(std::max(RuleI(Combat, MinHastedDelay), speed), true, true);

		if (i == MainPrimary)
			PrimaryWeapon = ItemToUse;
	}
}

void NPC::SetAttackTimer()
{
	float haste_mod = GetHaste() * 0.01f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetAtTrigger(4000, true);

	Timer *TimerToUse = nullptr;
	int hhe = itembonuses.HundredHands + spellbonuses.HundredHands;

	// Technically NPCs should do some logic for weapons, but the effect is minimal
	// What they do is take the lower of their set delay and the weapon's
	// ex. Mob's delay set to 20, weapon set to 19, delay 19
	// Mob's delay set to 20, weapon set to 21, delay 20
	int speed = 0;
	speed = static_cast<int>(((attack_delay / haste_mod) + ((hhe / 100.0f) * attack_delay)) * 100);

	for (int i = MainRange; i <= MainSecondary; i++) {
		//pick a timer
		if (i == MainPrimary)
			TimerToUse = &attack_timer;
		else if (i == MainRange)
			TimerToUse = &ranged_timer;
		else if (i == MainSecondary)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		//special offhand stuff
		if (i == MainSecondary) {
			if(GetLevel() < DUAL_WIELD_LEVEL && !GetSpecialAbility(SPECATK_INNATE_DW)) {
				attack_dw_timer.Disable();
				continue;
			}
		}

		TimerToUse->SetAtTrigger(std::max(RuleI(Combat, MinHastedDelay), speed), true, true);
	}
}

uint32 NPC::GetAttackTimer()
{
	float haste_mod = GetHaste() * 0.01f;
	int hhe = itembonuses.HundredHands + spellbonuses.HundredHands;
	int speed = 0;

	speed = static_cast<int>(((attack_delay / haste_mod) + ((hhe / 100.0f) * attack_delay)) * 100);

	return speed;
}

// crude approximation until I can come up with something more accurate
// finding out how much str or weapon skill an NPC has is sort of impossible after all
int Mob::GetOffense(int hand)
{
	int offense = GetLevel() * 65 / 10 - 1 + ATK;
	int bonus = 0;

	if (spellbonuses.ATK < 0 && -spellbonuses.ATK > (offense / 2))
	{
		return offense / 2;
	}

	return offense + spellbonuses.ATK;
}

int Client::GetOffense(SkillUseTypes weaponSkill)
{
	// this is the precise formula Sony/DBG uses, at least for clients
	// reminder: implement atk cap and primal weapon proc to item atk instead of spell atk logic
	int statBonus;

	if (weaponSkill == SkillArchery || weaponSkill == SkillThrowing)
	{
		statBonus = GetDEX();
	}
	else
	{
		statBonus = GetSTR();
	}
	int offense = GetSkill(weaponSkill) + spellbonuses.ATK + (statBonus > 75 ? ((2 * statBonus - 150) / 3) : 0);
	if (offense < 1)
	{
		offense = 1;
	}
	
	return offense * 1000 / 744;
}

int Client::GetOffense(int hand)
{
	ItemInst* weapon;

	if (hand == MainSecondary)
	{
		weapon = GetInv().GetItem(MainSecondary);
	}
	else
	{
		weapon = GetInv().GetItem(MainPrimary);
	}

	if (weapon && weapon->IsType(ItemClassCommon))
	{
		return GetOffense(static_cast<SkillUseTypes>(GetSkillByItemType(weapon->GetItem()->ItemType)));
	}
	else
	{
		return GetOffense(SkillHandtoHand);
	}
}

// very crude approximation that I will refine as data is created and evaluated
// still way better than before
int Mob::GetToHit(int hand)
{
	int32 accuracy = 0;

	if (IsNPC())
	{
		accuracy = CastToNPC()->GetAccuracyRating();	// database value
	}

	if (GetLevel() > 50)
	{
		return GetLevel() * 4 + 500 + accuracy;
	}
	else if (GetLevel() > 45)
	{
		return GetLevel() * 4 + 425 + accuracy;
	}
	return GetLevel() * 14 + 5 + accuracy;
}

int Client::GetToHit(SkillUseTypes weaponSkill)
{
	int32 accuracy = itembonuses.Accuracy[HIGHEST_SKILL + 1] +
		spellbonuses.Accuracy[HIGHEST_SKILL + 1] +
		aabonuses.Accuracy[HIGHEST_SKILL + 1] +
		aabonuses.Accuracy[weaponSkill] +
		itembonuses.HitChance; //Item Mod 'Accuracy'

	// this is the precise formula Sony/DBG uses, at least for clients
	int32 toHit = (7 + GetSkill(SkillOffense) + GetSkill(weaponSkill)) * 1000 / 744 + accuracy;

	if (toHit < 25)
	{
		toHit = 25;			// level 1s with no skills need a boost with the current avoidance model
	}

	return toHit;
}

int Client::GetToHit(int hand)
{
	ItemInst* weapon;

	if (hand == MainSecondary)
	{
		weapon = GetInv().GetItem(MainSecondary);
	}
	else
	{
		weapon = GetInv().GetItem(MainPrimary);
	}

	if (weapon && weapon->IsType(ItemClassCommon))
	{
		return GetToHit(static_cast<SkillUseTypes>(GetSkillByItemType(weapon->GetItem()->ItemType)));
	}
	else
	{
		return GetToHit(SkillHandtoHand);
	}
}

// NOTE: Current database AC values are completely out of whack and pulled from nowhere
// using this crude algorithm as a temporary placeholder until I get the data to adjust
// database values permanently.  If I didn't do this, players would have a really hard time
int Mob::GetMitigation()
{
	if (GetLevel() < 10)
	{
		return GetLevel() * 3 + 2;
	}
	if (GetLevel() < 50)
	{
		return GetLevel() * 7 - 43;
	}
	else
	{
		if (GetAC() > 600)
		{
			return GetAC() / 2;
		}
		else
		{
			return GetLevel() * 6;
		}
	}
}

// This returns the mitigation portion of the client AC value
// this is accurate and based on a Sony developer post
// See https://forums.daybreakgames.com/eq/index.php?threads/ac-vs-acv2.210028/
int Client::GetMitigation()
{
	int32 acSum = itembonuses.AC;
	uint8 playerClass = GetClass();
	uint8 level = GetLevel();

	acSum = 4 * acSum / 3;

	// anti-twink
	if (level < 50 && acSum > (GetLevel() * 26 + 25))
	{
		acSum = GetLevel() * 26 + 25;
	}

	if (playerClass == MONK)
	{
		int32 hardcap, softcap;

		if (level <= 15)
		{
			hardcap = 32;
			softcap = 15;
		}
		else if (level <= 30)
		{
			hardcap = 34;
			softcap = 16;
		}
		else if (level <= 45)
		{
			hardcap = 36;
			softcap = 17;
		}
		else if (level <= 51)
		{
			hardcap = 38;
			softcap = 18;
		}
		else if (level <= 55)
		{
			hardcap = 40;
			softcap = 20;
		}
		else if (level <= 60)
		{
			hardcap = 45;
			softcap = 24;
		}
		else if (level <= 62)
		{
			hardcap = 47;
			softcap = 24;
		}
		else if (level <= 64)
		{
			hardcap = 50;
			softcap = 24;
		}
		else
		{
			hardcap = 53;
			softcap = 26;
		}

		int32 weight = GetWeight() / 10;
		double acBonus = level + 5.0;

		if (weight <= softcap)
		{
			acSum += static_cast<int32>(acBonus);
		}
		else if (weight > hardcap)
		{
			double penalty = level + 5.0;
			double multiplier = (weight - (hardcap - 10)) / 100.0;
			if (multiplier > 1.0) multiplier = 1.0;
			penalty = 4.0 * penalty / 3.0;
			penalty = multiplier * penalty;

			acSum -= static_cast<int32>(penalty);
		}
		else if (weight > softcap)
		{
			double reduction = (weight - softcap) * 6.66667;
			if (reduction > 100.0) reduction = 100.0;
			reduction = (100.0 - reduction) / 100.0;
			acBonus *= reduction;
			if (acBonus < 0.0) acBonus = 0.0;
			acBonus = 4.0 * acBonus / 3.0;

			acSum += static_cast<int32>(acBonus);
		}

	}
	else if (playerClass == ROGUE)
	{
		if (level > 30 && GetAGI() > 75)
		{
			int32 levelScaler = level - 26;
			int32 acBonus = 0;

			if (GetAGI() < 80)
			{
				acBonus = levelScaler / 4;
			}
			else if (GetAGI() < 85)
			{
				acBonus = levelScaler * 2 / 4;
			}
			else if (GetAGI() < 90)
			{
				acBonus = levelScaler * 3 / 4;
			}
			else if (GetAGI() < 100)
			{
				acBonus = levelScaler * 4 / 4;
			}
			else
			{
				acBonus = levelScaler * 5 / 4;
			}

			if (acBonus > 12) acBonus = 12;

			acSum += acBonus;
		}
	}
	else if (playerClass == BEASTLORD)
	{
		if (level > 10)
		{
			int32 levelScaler = level - 6;
			int32 acBonus = 0;

			if (GetAGI() < 80)
			{
				acBonus = levelScaler / 5;
			}
			else if (GetAGI() < 85)
			{
				acBonus = levelScaler * 2 / 5;
			}
			else if (GetAGI() < 90)
			{
				acBonus = levelScaler * 3 / 5;
			}
			else if (GetAGI() < 100)
			{
				acBonus = levelScaler * 4 / 5;
			}
			else
			{
				acBonus = levelScaler * 5 / 5;
			}

			if (acBonus > 16) acBonus = 16;

			acSum += acBonus;
		}
	}
	if (GetRace() == IKSAR)
	{
		if (level < 10)
		{
			acSum += 10;
		}
		else if (level > 35)
		{
			acSum += 35;
		}
		else
		{
			acSum += level;
		}
	}

	if (acSum < 1)
	{
		acSum = 1;
	}

	int32 defense = GetSkill(SkillDefense);
	if (defense > 0)
	{
		if (playerClass == WIZARD || playerClass == NECROMANCER || playerClass == ENCHANTER || playerClass == MAGICIAN)
		{
			acSum += defense / 2;
		}
		else
		{
			acSum += defense / 3;
		}
	}

	int spellACDivisor = 4;
	if (playerClass == WIZARD || playerClass == MAGICIAN || playerClass == NECROMANCER || playerClass == ENCHANTER)
	{
		spellACDivisor = 3;
	}
	acSum += (spellbonuses.AC / spellACDivisor);

	if (GetAGI() > 70)
	{
		acSum += GetAGI() / 20;
	}

	int32 softcap;

	// the AC softcap values and logic were taken from Demonstar55's client decompile
	switch (playerClass)
	{
		case WARRIOR:
		{
			softcap = 430;
			break;
		}
		case PALADIN:
		case SHADOWKNIGHT:
		case CLERIC:
		case BARD:
		{
			softcap = 403;
			break;
		}
		case RANGER:
		case SHAMAN:
		{
		   softcap = 375;
		   break;
		}
		case MONK:
		{
			softcap = 315;
			break;
		}
		default:
		{
			softcap = 350;		// dru, rog, wiz, ench, nec, mag, bst
		}
	}

	// Combat Stability AA - this raises the softcap
	softcap += aabonuses.CombatStability + itembonuses.CombatStability + spellbonuses.CombatStability;

	const ItemInst *inst = m_inv.GetItem(MainSecondary);
	if (inst)
	{
		if (inst->GetItem()->ItemType == ItemTypeShield)
		{
			softcap += inst->GetItem()->AC;
		}
	}

	if (acSum > softcap)
	{
		if (level < 50)
		{
			return softcap;		// it's hard < level 50
		}

		int32 overcap = acSum - softcap;
		int32 returns = 20;					// CLR, DRU, SHM, NEC, WIZ, MAG, ENC

		if (playerClass == WARRIOR)
		{
			if (level <= 61)
			{
				returns = 5;
			}
			else if (level <= 63)
			{
				returns = 4;
			}
			else
			{
				returns = 3;
			}
		}
		else if (playerClass == PALADIN || playerClass == SHADOWKNIGHT)
		{
			if (level <= 61)
			{
				returns = 6;
			}
			else if (level <= 63)
			{
				returns = 5;
			}
			else
			{
				returns = 4;
			}
		}
		else if (playerClass == BARD)
		{
			if (level <= 61)
			{
				returns = 8;
			}
			else if (level <= 63)
			{
				returns = 7;
			}
			else
			{
				returns = 6;
			}
		}
		else if (playerClass == MONK || playerClass == ROGUE)
		{
			if (level <= 61)
			{
				returns = 20;
			}
			else if (level == 62)
			{
				returns = 18;
			}
			else if (level == 63)
			{
				returns = 16;
			}
			else if (level == 64)
			{
				returns = 14;
			}
			else
			{
				returns = 12;
			}
		}
		else if (playerClass == RANGER || playerClass == BEASTLORD)
		{
			if (level <= 61)
			{
				returns = 10;
			}
			else if (level == 62)
			{
				returns = 9;
			}
			else if (level == 63)
			{
				returns = 8;
			}
			else
			{
				returns = 7;
			}
		}

		acSum = softcap + overcap / returns;
	}

	return acSum;
}

// crude estimate based on arena test dummy data
// will refine this as data is generated and evaluated
int Mob::GetAvoidance()
{
	uint8 level = GetLevel();

	if (level <= 45)
	{
		return level * 914 / 100 + 10;
	}
	else if (level <= 50)
	{
		return 420;
	}
	else
	{
		return 470;
	}
}

// this is the precise formula that Sony/DBG uses
// minus the drunkeness reduction (add this later)
int Client::GetAvoidance()
{
	int32 defense = GetSkill(SkillDefense);

	if (defense > 0)
	{
		defense = defense * 400 / 225;
	}

	return defense + (GetAGI() > 40 ? (GetAGI() - 40) * 8000 / 36000 : 0) + itembonuses.AvoidMeleeChance;	//Item Mod 'Avoidence'
}
