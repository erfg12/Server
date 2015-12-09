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
#include "../common/eqemu_logsys.h"
#include "../common/spdat.h"

#include "client.h"
#include "entity.h"
#include "mob.h"
#include "beacon.h"

#include "string_ids.h"
#include "worldserver.h"
#include "zonedb.h"
#include "position.h"

float Client::GetActSpellRange(uint16 spell_id, float range, bool IsBard)
{
	float extrange = 100;

	extrange += GetFocusEffect(focusRange, spell_id);

	return (range * extrange) / 100;
}


int32 NPC::GetActSpellDamage(uint16 spell_id, int32 value,  Mob* target) {

	//Quest scale all NPC spell damage via $npc->SetSpellFocusDMG(value)
	//DoT Damage - Mob::DoBuffTic [spell_effects.cpp] / Direct Damage Mob::SpellEffect [spell_effects.cpp]

	int32 dmg = value;

	 if (target) {
		value += dmg*target->GetVulnerability(this, spell_id, 0)/100; 

		if (spells[spell_id].buffduration == 0)
			value -= target->GetFcDamageAmtIncoming(this, spell_id);
		else
			value -= target->GetFcDamageAmtIncoming(this, spell_id)/spells[spell_id].buffduration;
	 }

	 value += dmg*GetSpellFocusDMG()/100;

	if (AI_HasSpellsEffects()){
		int16 chance = 0;
		int ratio = 0;

		if (spells[spell_id].buffduration == 0) {

			if (chance && zone->random.Roll(chance)) {
				ratio += spellbonuses.SpellCritDmgIncrease + spellbonuses.SpellCritDmgIncNoStack;
				value += (value*ratio)/100;
				entity_list.MessageClose_StringID(this, true, 100, MT_SpellCrits, OTHER_CRIT_BLAST, GetCleanName(), itoa(-value));
			}
		}
		else {
			chance += spellbonuses.CriticalDoTChance;
			if (chance && zone->random.Roll(chance)) {
				ratio += spellbonuses.DotCritDmgIncrease;
				value += (value*ratio)/100;
			}
		}
	}

	return value;
}

int32 Client::GetActSpellDamage(uint16 spell_id, int32 value, Mob* target) {

	if (spells[spell_id].targettype == ST_Self)
		return value;

	bool Critical = false;
	int32 value_BaseEffect = 0;

	value_BaseEffect = value + (value*GetFocusEffect(focusFcBaseEffects, spell_id)/100);

	// Need to scale HT damage differently after level 40! It no longer scales by the constant value in the spell file. It scales differently, instead of 10 more damage per level, it does 30 more damage per level. So we multiply the level minus 40 times 20 if they are over level 40.
	if ( (spell_id == SPELL_HARM_TOUCH || spell_id == SPELL_HARM_TOUCH2 || spell_id == SPELL_IMP_HARM_TOUCH ) && GetLevel() > 40)
		value -= (GetLevel() - 40) * 20;

	//This adds the extra damage from the AA Unholy Touch, 450 per level to the AA Improved Harm TOuch.
	if (spell_id == SPELL_IMP_HARM_TOUCH) //Improved Harm Touch
		value -= GetAA(aaUnholyTouch) * 450; //Unholy Touch

	int chance = RuleI(Spells, BaseCritChance); //Wizard base critical chance is 2% (Does not scale with level)
		chance += itembonuses.CriticalSpellChance + spellbonuses.CriticalSpellChance + aabonuses.CriticalSpellChance;

	if (chance > 0 || (GetClass() == WIZARD && GetLevel() >= RuleI(Spells, WizCritLevel))) {

		 int32 ratio = RuleI(Spells, BaseCritRatio); //Critical modifier is applied from spell effects only. Keep at 100 for live like criticals.

		//Improved Harm Touch is a guaranteed crit if you have at least one level of SCF.
		 if (spell_id == SPELL_IMP_HARM_TOUCH && (GetAA(aaSpellCastingFury) > 0) && (GetAA(aaUnholyTouch) > 0))
			 chance = 100;

		 if (zone->random.Roll(chance)) {
			Critical = true;
			ratio += itembonuses.SpellCritDmgIncrease + spellbonuses.SpellCritDmgIncrease + aabonuses.SpellCritDmgIncrease;
			ratio += itembonuses.SpellCritDmgIncNoStack + spellbonuses.SpellCritDmgIncNoStack + aabonuses.SpellCritDmgIncNoStack;
		}

		else if (GetClass() == WIZARD && (GetLevel() >= RuleI(Spells, WizCritLevel)) && (zone->random.Roll(RuleI(Spells, WizCritChance)))) {
			ratio += zone->random.Int(20,70); //Wizard innate critical chance is calculated seperately from spell effect and is not a set ratio. (20-70 is parse confirmed)
			Critical = true;
		}

		ratio += RuleI(Spells, WizCritRatio); //Default is zero

		if (Critical){

			value = value_BaseEffect*ratio/100;

			value += value_BaseEffect*GetFocusEffect(focusImprovedDamage, spell_id)/100;

			value += int(value_BaseEffect*GetFocusEffect(focusFcDamagePctCrit, spell_id)/100)*ratio/100;

			if (target) {
				value += int(value_BaseEffect*target->GetVulnerability(this, spell_id, 0)/100)*ratio/100;
				value -= target->GetFcDamageAmtIncoming(this, spell_id);
			}

			value -= GetFocusEffect(focusFcDamageAmtCrit, spell_id)*ratio/100;

			value -= GetFocusEffect(focusFcDamageAmt, spell_id);

			entity_list.MessageClose_StringID(this, true, 100, MT_SpellCrits,
					OTHER_CRIT_BLAST, GetName(), itoa(-value));
			Message_StringID(MT_SpellCrits, YOU_CRIT_BLAST, itoa(-value));

			return value;
		}
	}

	value = value_BaseEffect;

	value += value_BaseEffect*GetFocusEffect(focusImprovedDamage, spell_id)/100;

	value += value_BaseEffect*GetFocusEffect(focusFcDamagePctCrit, spell_id)/100;

	if (target) {
		value += value_BaseEffect*target->GetVulnerability(this, spell_id, 0)/100;
		value -= target->GetFcDamageAmtIncoming(this, spell_id);
	}

	value -= GetFocusEffect(focusFcDamageAmtCrit, spell_id);

	value -= GetFocusEffect(focusFcDamageAmt, spell_id);

	return value;
}

int32 Client::GetActDoTDamage(uint16 spell_id, int32 value, Mob* target) {

	if (target == nullptr)
		return value;

	int32 value_BaseEffect = 0;
	int32 extra_dmg = 0;
	int16 chance = 0;
	chance += itembonuses.CriticalDoTChance + spellbonuses.CriticalDoTChance + aabonuses.CriticalDoTChance;

	if (spellbonuses.CriticalDotDecay)
			chance += GetDecayEffectValue(spell_id, SE_CriticalDotDecay);

	value_BaseEffect = value + (value*GetFocusEffect(focusFcBaseEffects, spell_id)/100);

	if (chance > 0 && (zone->random.Roll(chance))) {
		int32 ratio = 200;
		ratio += itembonuses.DotCritDmgIncrease + spellbonuses.DotCritDmgIncrease + aabonuses.DotCritDmgIncrease;
		value = value_BaseEffect*ratio/100;
		value += int(value_BaseEffect*GetFocusEffect(focusImprovedDamage, spell_id)/100)*ratio/100;
		value += int(value_BaseEffect*GetFocusEffect(focusFcDamagePctCrit, spell_id)/100)*ratio/100;
		value += int(value_BaseEffect*target->GetVulnerability(this, spell_id, 0)/100)*ratio/100;
		extra_dmg = target->GetFcDamageAmtIncoming(this, spell_id) + 
					int(GetFocusEffect(focusFcDamageAmtCrit, spell_id)*ratio/100) +
					GetFocusEffect(focusFcDamageAmt, spell_id);

		if (extra_dmg) {
			int duration = CalcBuffDuration(this, this, spell_id);
			if (duration > 0)
				extra_dmg /= duration;
		}

		value -= extra_dmg;

		return value;
	}

	value = value_BaseEffect;
	value += value_BaseEffect*GetFocusEffect(focusImprovedDamage, spell_id)/100;
	value += value_BaseEffect*GetFocusEffect(focusFcDamagePctCrit, spell_id)/100;
	value += value_BaseEffect*target->GetVulnerability(this, spell_id, 0)/100;
	extra_dmg = target->GetFcDamageAmtIncoming(this, spell_id) +
				GetFocusEffect(focusFcDamageAmtCrit, spell_id) +
				GetFocusEffect(focusFcDamageAmt, spell_id);

	if (extra_dmg) {
		int duration = CalcBuffDuration(this, this, spell_id);
		if (duration > 0)
			extra_dmg /= duration;
	}

	value -= extra_dmg;

	return value;
}

int32 Mob::GetExtraSpellAmt(uint16 spell_id, int32 extra_spell_amt, int32 base_spell_dmg)
{
	int total_cast_time = 0;

	if (spells[spell_id].recast_time >= spells[spell_id].recovery_time)
			total_cast_time = spells[spell_id].recast_time + spells[spell_id].cast_time;
	else
		total_cast_time = spells[spell_id].recovery_time + spells[spell_id].cast_time;

	if (total_cast_time > 0 && total_cast_time <= 2500)
		extra_spell_amt = extra_spell_amt*25/100; 
	 else if (total_cast_time > 2500 && total_cast_time < 7000) 
		 extra_spell_amt = extra_spell_amt*(167*((total_cast_time - 1000)/1000)) / 1000; 
	 else 
		 extra_spell_amt = extra_spell_amt * total_cast_time / 7000; 

		if(extra_spell_amt*2 < base_spell_dmg)
			return 0;

		return extra_spell_amt;
}


int32 NPC::GetActSpellHealing(uint16 spell_id, int32 value, Mob* target) {

	//Scale all NPC spell healing via SetSpellFocusHeal(value)

	value += value*GetSpellFocusHeal()/100;

	 if (target) {
		value += target->GetFocusIncoming(focusFcHealAmtIncoming, SE_FcHealAmtIncoming, this, spell_id);
		value += value*target->GetHealRate(spell_id, this)/100;
	 }

	 //Allow for critical heal chance if NPC is loading spell effect bonuses.
	 if (AI_HasSpellsEffects()){
		if(spells[spell_id].buffduration < 1) {
			if(spellbonuses.CriticalHealChance && (zone->random.Roll(spellbonuses.CriticalHealChance))) {
				value = value*2;
				entity_list.MessageClose_StringID(this, true, 100, MT_SpellCrits, OTHER_CRIT_HEAL, GetCleanName(), itoa(value));
			}
		}
		else if(spellbonuses.CriticalHealOverTime && (zone->random.Roll(spellbonuses.CriticalHealOverTime))) {
				value = value*2;
		}
	 }

	return value;
}

int32 Client::GetActSpellHealing(uint16 spell_id, int32 value, Mob* target) {

	if (target == nullptr)
		target = this;

	int32 value_BaseEffect = 0;
	int16 chance = 0;
	int8 modifier = 1;
	bool Critical = false;

	value_BaseEffect = value + (value*GetFocusEffect(focusFcBaseEffects, spell_id)/100);

	value = value_BaseEffect;

	value += int(value_BaseEffect*GetFocusEffect(focusImprovedHeal, spell_id)/100);

	// Instant Heals
	if(spells[spell_id].buffduration < 1) {

		chance += itembonuses.CriticalHealChance + spellbonuses.CriticalHealChance + aabonuses.CriticalHealChance;

		chance += target->GetFocusIncoming(focusFcHealPctCritIncoming, SE_FcHealPctCritIncoming, this, spell_id);

		if (spellbonuses.CriticalHealDecay)
			chance += GetDecayEffectValue(spell_id, SE_CriticalHealDecay);

		if(chance && (zone->random.Roll(chance))) {
			Critical = true;
			modifier = 2; //At present time no critical heal amount modifier SPA exists.
		}

		value *= modifier;
		value += GetFocusEffect(focusFcHealAmtCrit, spell_id) * modifier;
		value += GetFocusEffect(focusFcHealAmt, spell_id);
		value += target->GetFocusIncoming(focusFcHealAmtIncoming, SE_FcHealAmtIncoming, this, spell_id);
		value += value*target->GetHealRate(spell_id, this)/100;

		if (Critical) {
			entity_list.MessageClose_StringID(this, true, 100, MT_SpellCrits,
					OTHER_CRIT_HEAL, GetName(), itoa(value));
			Message_StringID(MT_SpellCrits, YOU_CRIT_HEAL, itoa(value));
		}

		return value;
	}

	//Heal over time spells. [Heal Rate and Additional Healing effects do not increase this value]
	else {

		chance = itembonuses.CriticalHealOverTime + spellbonuses.CriticalHealOverTime + aabonuses.CriticalHealOverTime;

		chance += target->GetFocusIncoming(focusFcHealPctCritIncoming, SE_FcHealPctCritIncoming, this, spell_id);

		if (spellbonuses.CriticalRegenDecay)
			chance += GetDecayEffectValue(spell_id, SE_CriticalRegenDecay);

		if(chance && zone->random.Roll(chance))
			return (value * 2);
	}

	return value;
}


int32 Client::GetActSpellCost(uint16 spell_id, int32 cost)
{
	// This formula was derived from the following resource:
	// http://www.eqsummoners.com/eq1/specialization-library.html
	// WildcardX
	float PercentManaReduction = 0.0f;
	float SpecializeSkill = GetSpecializeSkillValue(spell_id);

	if (SpecializeSkill > 0.0f)
	{
		PercentManaReduction = 1 + SpecializeSkill / 20.0f;
		switch(GetAA(aaSpellCastingMastery))
		{
		case 1:
			PercentManaReduction += 2.5f;
			break;
		case 2:
			PercentManaReduction += 5.0f;
			break;
		case 3:
			PercentManaReduction += 10.0f;
			break;
		}
	}

	int16 focus_redux = GetFocusEffect(focusManaCost, spell_id);

	if(focus_redux > 0)
	{
		PercentManaReduction += zone->random.Real(1, (double)focus_redux);
	}

	cost -= (cost * (PercentManaReduction / 100));

	if(cost < 0)
		cost = 0;

	return cost;
}

int32 Client::GetActSpellDuration(uint16 spell_id, int32 duration)
{
	if (spells[spell_id].not_extendable)
		return duration;

	int increase = 100;
	increase += GetFocusEffect(focusSpellDuration, spell_id);
	int tic_inc = 0;
	tic_inc = GetFocusEffect(focusSpellDurByTic, spell_id);

	// Only need this for clients, since the change was for bard songs, I assume we should keep non bard songs getting +1
	// However if its bard or not and is mez, charm or fear, we need to add 1 so that client is in sync
	if (!(IsShortDurationBuff(spell_id) && IsBardSong(spell_id)) ||
			IsFearSpell(spell_id) ||
			IsCharmSpell(spell_id) ||
			IsMezSpell(spell_id) ||
			IsBlindSpell(spell_id))
		tic_inc += 1;

	return (((duration * increase) / 100) + tic_inc);
}

int32 Client::GetActSpellCasttime(uint16 spell_id, int32 casttime)
{
	int32 cast_reducer = 0;
	cast_reducer += GetFocusEffect(focusSpellHaste, spell_id);

	//this function loops through the effects of spell_id many times
	//could easily be consolidated.

	if (GetLevel() >= 51 && casttime >= 3000 && !BeneficialSpell(spell_id)
		&& (GetClass() == SHADOWKNIGHT || GetClass() == RANGER
			|| GetClass() == PALADIN || GetClass() == BEASTLORD ))
		cast_reducer += (GetLevel()-50)*3;

	//LIVE AA SpellCastingDeftness, QuickBuff, QuickSummoning, QuickEvacuation, QuickDamage

	if (cast_reducer > RuleI(Spells, MaxCastTimeReduction))
		cast_reducer = RuleI(Spells, MaxCastTimeReduction);

	casttime = (casttime*(100 - cast_reducer)/100);

	return casttime;
}

bool Client::UseDiscipline(uint8 disc_id) 
{
	// Dont let client waste a reuse timer if they can't use the disc
	if (IsStunned() || IsFeared() || IsMezzed() || IsAmnesiad() || IsPet())
	{
		return(false);
	}

	//Check the disc timer
	uint32 remain = p_timers.GetRemainingTime(pTimerDisciplineReuseStart);
	if(remain > 0 && !GetGM())
	{
		char val1[20]={0};
		char val2[20]={0};
		Message_StringID(CC_User_Disciplines, DISCIPLINE_CANUSEIN, ConvertArray((remain)/60,val1), ConvertArray(remain%60,val2));
		return(false);
	}

	bool active = disc_ability_timer.Enabled();
	if(active)
	{
		Message(CC_User_Disciplines, "You must wait before using this discipline."); //find correct message
		return(false);
	}

	//can we use the disc? the client checks this for us, but we should also confirm server side.
	uint8 level_to_use = DisciplineUseLevel(disc_id);
	if(level_to_use > GetLevel() || level_to_use == 0) {
		Message_StringID(CC_User_Disciplines, DISC_LEVEL_USE_ERROR);
		return(false);
	}

	// Disciplines with no ability timer (ashenhand, silentfist, thunderkick, and unholyaura) will remain on the player until they either 
	// use the skill the disc affects successfully, camp/zone, or attempt to use another disc. If we're here, clear that disc so they can 
	// cast a new one.
	if(GetActiveDisc() != 0)
	{
		Log.Out(Logs::General, Logs::Discs, "Clearing disc %d so that disc %d can be cast.", GetActiveDisc(), disc_id);
		FadeDisc();
	}

	//cast the disc
	if(CastDiscipline(disc_id, level_to_use))
		return(true);
	else
		return(false);
}

uint8 Client::DisciplineUseLevel(uint8 disc_id)
{
	switch(disc_id) 
	{
		case disc_aggressive:
			if(GetClass() == WARRIOR)
				return 60;
			else
				return 0;
			break;
		case disc_precision:
			if(GetClass() == WARRIOR)
				return 57;
			else
				return 0;
			break;
		case disc_defensive:
			if(GetClass() == WARRIOR)
				return 55;
			else
				return 0;
			break;
		case disc_evasive:
			if(GetClass() == WARRIOR)
				return 52;
			else
				return 0;
			break;
		case disc_ashenhand:
			if(GetClass() == MONK)
				return 60;
			else
				return 0;
			break;
		case disc_furious:
			if(GetClass() == WARRIOR)
				return 56;
			else if(GetClass() == MONK || GetClass() == ROGUE)
				return 53;
			else
				return 0;
			break;
		case disc_stonestance:
			if(GetClass() == MONK)
				return 51;
			else if(GetClass() == BEASTLORD)
				return 55;
			else
				return 0;
			break;
		case disc_thunderkick:
			if(GetClass() == MONK)
				return 52;
			else
				return 0;
			break;
		case disc_fortitude:
			if(GetClass() == WARRIOR)
				return 59;
			else if(GetClass() == MONK)
				return 54;
			else
				return 0;
			break;
		case disc_fellstrike:
			if(GetClass() == WARRIOR)
				return 58;
			else if(GetClass() == MONK)
				return 56;
			else if(GetClass() == BEASTLORD)
				return 60;
			else if(GetClass() == ROGUE)
				return 59;
			else
				return 0;
			break;
		case disc_hundredfist:
			if(GetClass() == MONK)
				return 57;
			else if(GetClass() == ROGUE)
				return 58;
			else
				return 0;
			break;
		case disc_charge:
			if(GetClass() == WARRIOR)
				return 53;
			else if(GetClass() == ROGUE)
				return 54;
			else
				return 0;
			break;
		case disc_mightystrike:
			if(GetClass() == WARRIOR)
				return 54;
			else
				return 0;
			break;
		case disc_nimble:
			if(GetClass() == ROGUE)
				return 55;
			else
				return 0;
			break;
		case disc_silentfist:
			if(GetClass() == MONK)
				return 59;
			else
				return 0;
			break;
		case disc_kinesthetics:
			if(GetClass() == ROGUE)
				return 57;
			else
				return 0;
			break;
		case disc_holyforge:
			if(GetClass() == PALADIN)
				return 55;
			else
				return 0;
			break;
		case disc_sanctification:
			if(GetClass() == PALADIN)
				return 60;
			else
				return 0;
			break;
		case disc_trueshot:
			if(GetClass() == RANGER)
				return 55;
			else
				return 0;
			break;
		case disc_weaponshield:
			if(GetClass() == RANGER)
				return 60;
			else
				return 0;
			break;
		case disc_unholyaura:
			if(GetClass() == SHADOWKNIGHT)
				return 55;
			else
				return 0;
			break;
		case disc_leechcurse:
			if(GetClass() == SHADOWKNIGHT)
				return 60;
			else
				return 0;
			break;
		case disc_deftdance:
			if(GetClass() == BARD)
				return 55;
			else
				return 0;
			break;
		case disc_puretone:
			if(GetClass() == BARD)
				return 60;
			else
				return 0;
			break;
		case disc_resistant:
			if(GetClass() == WARRIOR || GetClass() == MONK || GetClass() == ROGUE)
				return 30;
			else if(GetClass() == PALADIN || GetClass() == RANGER || GetClass() == SHADOWKNIGHT || GetClass() == BARD || GetClass() == BEASTLORD)
				return 51;
			else
				return 0;
			break;
		case disc_fearless:
			if(GetClass() == WARRIOR || GetClass() == MONK || GetClass() == ROGUE)
				return 40;
			else if(GetClass() == PALADIN || GetClass() == RANGER || GetClass() == SHADOWKNIGHT || GetClass() == BARD || GetClass() == BEASTLORD)
				return 54;
			else
				return 0;
			break;
		default:
			return 0;
			break;
	}
}

bool Client::CastDiscipline(uint8 disc_id, uint8 level_to_use)
{
	uint8 current_level = GetLevel();
	if(current_level > 60)
		current_level = 60;

	if(level_to_use > current_level || level_to_use == 0)
		return false;

	// reuse_timer is in seconds, ability_timer is in milliseconds.
	uint32 reuse_timer = 0, ability_timer = 0, string = 0;
	int16 spellid = 0;

	switch(disc_id)
	{
		case disc_aggressive:
			reuse_timer = 1620 - (current_level - level_to_use) * 60;
			ability_timer = 180000;
			spellid = 4498;
			string = DISCIPLINE_AGRESSIVE;

			break;
		case disc_precision:
			reuse_timer = 1800 - (current_level - level_to_use) * 60;
			ability_timer = 180000;
			spellid = 4501;
			string = DISCIPLINE_PRECISION;

			break;
		case disc_defensive:
			reuse_timer = 900 - (current_level - level_to_use) * 60;
			ability_timer = 180000;
			spellid = 4499;
			string = DISCIPLINE_DEFENSIVE;

			break;
		case disc_evasive:
			reuse_timer = 900 - (current_level - level_to_use) * 60;
			ability_timer = 180000;
			spellid = 4503;
			string = DISCIPLINE_EVASIVE;

			break;
		case disc_ashenhand:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			spellid = 4508;
			string = DISCIPLINE_ASHENHAND;

			break;
		case disc_furious:
			reuse_timer = 3600 - (current_level - level_to_use) * 60;
			ability_timer = 9000;
			if(GetBaseClass() == WARRIOR)
				spellid = 4674;
			else if(GetBaseClass() == MONK)
				spellid = 4509;
			else if(GetBaseClass() == ROGUE)
				spellid = 4673;
			string = DISCIPLINE_FURIOUS;

			break;
		case disc_stonestance:
			reuse_timer = 720 - (current_level - level_to_use) * 60;
			ability_timer = 12000;
			if(GetBaseClass() == MONK)
				spellid = 4510;
			else if(GetBaseClass() == BEASTLORD)
				spellid = 4671;
			string = DISCIPLINE_STONESTANCE;

			break;
		case disc_thunderkick:
			reuse_timer = 420 - (current_level - level_to_use) * 60;
			spellid = 4511;
			string = DISCIPLINE_THUNDERKICK;

			break;
		case disc_fortitude:
			reuse_timer = 3600 - (current_level - level_to_use) * 60;
			ability_timer = 8000;
			if(GetBaseClass() == WARRIOR)
				spellid = 4670;
			else if(GetBaseClass() == MONK)
				spellid = 4502;
			string = DISCIPLINE_FORTITUDE;

			break;
		case disc_fellstrike:
			reuse_timer = 1800 - (current_level - level_to_use) * 60;
			ability_timer = 12000;
			if(GetBaseClass() == WARRIOR)
				spellid = 4675;
			else if(GetBaseClass() == MONK)
				spellid = 4512;
			else if(GetBaseClass() == ROGUE)
				spellid = 4676;
			else if(GetBaseClass() == BEASTLORD)
				spellid = 4678;
			string = DISCIPLINE_FELLSTRIKE;

			break;
		case disc_hundredfist:
			reuse_timer = 1800 - (current_level - level_to_use) * 60;
			ability_timer = 15000;
			if(GetBaseClass() == MONK)
				spellid = 4513;
			else if(GetBaseClass() == ROGUE)
				spellid = 4677;
			string = DISCIPLINE_HUNDREDFIST;

			break;
		case disc_charge:
			reuse_timer = 1800 - (current_level - level_to_use) * 60;
			ability_timer = 14000;
			if(GetBaseClass() == WARRIOR)
				spellid = 4672;
			else if(GetBaseClass() == ROGUE)
				spellid = 4505;
			string = DISCIPLINE_CHARGE;

			break;
		case disc_mightystrike:
			reuse_timer = 3600 - (current_level - level_to_use) * 60;
			ability_timer = 10000;
			spellid = 4514;
			string = DISCIPLINE_MIGHTYSTRIKE;

			break;
		case disc_nimble:
			reuse_timer = 1800 - (current_level - level_to_use) * 60;
			ability_timer = 12000;
			spellid = 4515;
			string = DISCIPLINE_NIMBLE;

			break;
		case disc_silentfist:
			reuse_timer = 540 - (current_level - level_to_use) * 60;
			spellid = 4507;
			if(GetRace() == IKSAR)
				string = DISCIPLINE_SILENTFIST_IKSAR;
			else
				string = DISCIPLINE_SILENTFIST;

			break;
		case disc_kinesthetics:
			reuse_timer = 1800 - (current_level - level_to_use) * 60;
			ability_timer = 18000;
			spellid = 4517;
			string = DISCIPLINE_KINESTHETICS;

			break;
		case disc_holyforge:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 120000;
			spellid = 4500;
			string = DISCIPLINE_HOLYFORGE;

			break;
		case disc_sanctification:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 10000;
			spellid = 4518;
			string = DISCIPLINE_SANCTIFICATION;

			break;
		case disc_trueshot:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 120000;
			spellid = 4506;
			string = DISCIPLINE_TRUESHOT;

			break;
		case disc_weaponshield:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 15000;
			spellid = 4519;
			if(GetGender() == 0)
				string = DISCIPLINE_WPNSLD_MALE;
			else if(GetGender() == 1)
				string = DISCIPLINE_WPNSLD_FEMALE;
			else
				string = DISCIPLINE_WPNSLD_MONSTER;

			break;
		case disc_unholyaura:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			spellid = 4520;
			string = DISCIPLINE_UNHOLYAURA;

			break;
		case disc_leechcurse:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 15000;
			spellid = 4504;
			string = DISCIPLINE_LEECHCURSE;

			break;
		case disc_deftdance:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 10000;
			spellid = 4516;
			string = DISCIPLINE_DEFTDANCE;

			break;
		case disc_puretone:
			reuse_timer = 4320 - (current_level - level_to_use) * 60;
			ability_timer = 120000;
			spellid = 4586;
			string = DISCIPLINE_PURETONE;

			break;
		case disc_resistant:
			reuse_timer = 3600 - (current_level - level_to_use) * 60;
			ability_timer = 60000;
			spellid = 4585;
			string = DISCIPLINE_RESISTANT;

			break;
		case disc_fearless:
			reuse_timer = 3600 - (current_level - level_to_use) * 60;
			ability_timer = 11000;
			spellid = 4587;
			string = DISCIPLINE_FEARLESS;

			break;
		default:
			Log.Out(Logs::General, Logs::Discs, "Invalid disc id %d was passed to CastDiscipline.", disc_id);
			return false;
	}

	if(string > 0 && IsDisc(spellid))
	{
		entity_list.MessageClose_StringID(this, false, 50, CC_User_Disciplines, string, GetName());
		p_timers.Start(pTimerDisciplineReuseStart, reuse_timer);
		if(ability_timer > 0)
		{
			disc_ability_timer.SetTimer(ability_timer);
		}
		else
		{
			Log.Out(Logs::General, Logs::Discs, "Disc %d is an instant effect", disc_id);
		}

		SetActiveDisc(disc_id, spellid);
		SpellFinished(spellid, this);
	}
	else
	{
		Log.Out(Logs::General, Logs::Discs, "Disc: %d Invalid stringid or spellid specified.", disc_id);
		return false;
	}

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_DisciplineChange, sizeof(ClientDiscipline_Struct));
	ClientDiscipline_Struct *d = (ClientDiscipline_Struct*)outapp->pBuffer;
	d->disc_id = disc_id;
	QueuePacket(outapp);
	safe_delete(outapp);

	return true;
}

void EntityList::AETaunt(Client* taunter, float range)
{
	if (range == 0)
		range = 100;		//arbitrary default...

	range = range * range;

	auto it = npc_list.begin();
	while (it != npc_list.end()) {
		NPC *them = it->second;
		float zdiff = taunter->GetZ() - them->GetZ();
		if (zdiff < 0)
			zdiff *= -1;
		if (zdiff < 10
				&& taunter->IsAttackAllowed(them)
				&& DistanceSquaredNoZ(taunter->GetPosition(), them->GetPosition()) <= range) {
			if (taunter->CheckLosFN(them)) {
				taunter->Taunt(them, true);
			}
		}
		++it;
	}
}

// solar: causes caster to hit every mob within dist range of center with
// spell_id.
// NPC spells will only affect other NPCs with compatible faction
void EntityList::AESpell(Mob *caster, Mob *center, uint16 spell_id, bool affect_caster, int16 resist_adjust)
{
	Mob *curmob;

	float dist = caster->GetAOERange(spell_id);
	float dist2 = dist * dist;
	float min_range2 = spells[spell_id].min_range * spells[spell_id].min_range;
	float dist_targ = 0;

	bool detrimental = IsDetrimentalSpell(spell_id);
	bool clientcaster = caster->IsClient();
	const int MAX_TARGETS_ALLOWED = 4;
	int targets_hit = 0;
	if(center->IsBeacon())
		targets_hit = center->CastToBeacon()->GetTargetsHit();

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		curmob = it->second;
		// test to fix possible cause of random zone crashes..external methods accessing client properties before they're initialized
		if (curmob->IsClient() && !curmob->CastToClient()->ClientFinishedLoading())
			continue;
		if (curmob == center)	//do not affect center
			continue;
		if (curmob == caster && !affect_caster)	//watch for caster too
			continue;

		dist_targ = DistanceSquared(curmob->GetPosition(), center->GetPosition());

		if (dist_targ > dist2)	//make sure they are in range
			continue;
		if (dist_targ < min_range2)	//make sure they are in range
			continue;
		if (!clientcaster && curmob->IsNPC()) {	//check npc->npc casting
			FACTION_VALUE f = curmob->GetReverseFactionCon(caster);
			if (detrimental) {
				//affect mobs that are on our hate list, or
				//which have bad faction with us
				if (!(caster->CheckAggro(curmob) || f == FACTION_THREATENLY || f == FACTION_SCOWLS))
					continue;
			}
			else {
				//only affect mobs we would assist.
				if (!(f <= FACTION_AMIABLE))
					continue;
			}
		}
		//finally, make sure they are within range
		if (detrimental) {
			if (!caster->IsAttackAllowed(curmob, true))
			{
				Log.Out(Logs::Detail, Logs::Spells, "Attempting to cast a detrimental AE spell/song on a player.");
				continue;
			}
			if (!zone->SkipLoS() && !spells[spell_id].npc_no_los && curmob != caster && !center->CheckLosFN(curmob))
				continue;
		}
		else { // check to stop casting beneficial ae buffs (to wit: bard songs) on enemies...
			// This does not check faction for beneficial AE buffs..only agro and attackable.
			// I've tested for spells that I can find without problem, but a faction-based
			// check may still be needed. Any changes here should also reflect in BardAEPulse() -U
			if (caster->IsAttackAllowed(curmob, true))
			{
				Log.Out(Logs::Detail, Logs::Spells, "Attempting to cast a beneficial AE spell/song on a NPC.");
				caster->Message_StringID(MT_SpellFailure, SPELL_NO_HOLD);
				continue;
			}
			if (caster->CheckAggro(curmob))
				continue;
		}

		curmob->CalcSpellPowerDistanceMod(spell_id, dist_targ);

		//if we get here... cast the spell.
		if (IsTargetableAESpell(spell_id) && detrimental) 
		{
			if (targets_hit < MAX_TARGETS_ALLOWED || (clientcaster && curmob == caster)) 
			{
				caster->SpellOnTarget(spell_id, curmob, false, true, resist_adjust);
				Log.Out(Logs::Detail, Logs::Spells, "AE Rain Spell: %d has hit target #%d: %s", spell_id, targets_hit, curmob->GetCleanName());

				if (clientcaster && curmob != caster) //npcs are not target limited, pc caster does not count towards the limit.
					++targets_hit;
			}
		}
		else 
		{
			caster->SpellOnTarget(spell_id, curmob, false, true, resist_adjust);
		}

	}

	if(center->IsBeacon())
		center->CastToBeacon()->SetTargetsHit(targets_hit);
}

void EntityList::MassGroupBuff(Mob *caster, Mob *center, uint16 spell_id, bool affect_caster)
{
	Mob *curmob;

	float dist = caster->GetAOERange(spell_id);
	float dist2 = dist * dist;

	bool bad = IsDetrimentalSpell(spell_id);

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		curmob = it->second;
		if (curmob == center)	//do not affect center
			continue;
		if (curmob == caster && !affect_caster)	//watch for caster too
			continue;
		if (DistanceSquared(center->GetPosition(), curmob->GetPosition()) > dist2)	//make sure they are in range
			continue;

		//Only npcs mgb should hit are client pets...
		if (curmob->IsNPC()) {
			Mob *owner = curmob->GetOwner();
			if (owner) {
				if (!owner->IsClient()) {
					continue;
				}
			} else {
				continue;
			}
		}

		if (bad) {
			continue;
		}

		caster->SpellOnTarget(spell_id, curmob);
	}
}

// solar: causes caster to hit every mob within dist range of center with
// a bard pulse of spell_id.
// NPC spells will only affect other NPCs with compatible faction
void EntityList::AEBardPulse(Mob *caster, Mob *center, uint16 spell_id, bool affect_caster)
{
	Mob *curmob;

	float dist = caster->GetAOERange(spell_id);
	float dist2 = dist * dist;

	bool bad = IsDetrimentalSpell(spell_id);
	bool isnpc = caster->IsNPC();

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		curmob = it->second;
		if (curmob == center)	//do not affect center
			continue;
		if (curmob == caster && !affect_caster)	//watch for caster too
			continue;
		if (DistanceSquared(center->GetPosition(), curmob->GetPosition()) > dist2)	//make sure they are in range
			continue;
		if (isnpc && curmob->IsNPC()) {	//check npc->npc casting
			FACTION_VALUE f = curmob->GetReverseFactionCon(caster);
			if (bad) {
				//affect mobs that are on our hate list, or
				//which have bad faction with us
				if (!(caster->CheckAggro(curmob) || f == FACTION_THREATENLY || f == FACTION_SCOWLS) )
					continue;
			} else {
				//only affect mobs we would assist.
				if (!(f <= FACTION_AMIABLE))
					continue;
			}
		}
		//finally, make sure they are within range
		if (bad) {
			if (!center->CheckLosFN(curmob))
				continue;
		} else { // check to stop casting beneficial ae buffs (to wit: bard songs) on enemies...
			// See notes in AESpell() above for more info. 
			if (caster->IsAttackAllowed(curmob, true))
				continue;
			if (caster->CheckAggro(curmob))
				continue;

			AddHealAggro(curmob, caster, caster->CheckHealAggroAmount(spell_id, curmob));
		}

		//if we get here... cast the spell.
		curmob->BardPulse(spell_id, caster);
	}
	if (caster->IsClient())
		caster->CastToClient()->CheckSongSkillIncrease(spell_id);
}

//Dook- Rampage and stuff for clients.
//NPCs handle it differently in Mob::Rampage
void EntityList::AEAttack(Mob *attacker, float dist, int Hand, int count, bool IsFromSpell) {
//Dook- Will need tweaking, currently no pets or players or horses
	Mob *curmob;

	float dist2 = dist * dist;

	int hit = 0;

	for (auto it = mob_list.begin(); it != mob_list.end(); ++it) {
		curmob = it->second;
		if (curmob->IsNPC()
				&& curmob != attacker //this is not needed unless NPCs can use this
				&&(attacker->IsAttackAllowed(curmob))
				&& curmob->GetRace() != 216 && curmob->GetRace() != 472 /* dont attack horses */
				&& (DistanceSquared(curmob->GetPosition(), attacker->GetPosition()) <= dist2)
		) {
			attacker->Attack(curmob, Hand, false, false, IsFromSpell);
			hit++;
			if (count != 0 && hit >= count)
				return;
		}
	}
}


