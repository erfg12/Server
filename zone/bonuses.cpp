/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2004 EQEMu Development Team (http://eqemu.org)

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
#include "../common/classes.h"
#include "../common/global_define.h"
#include "../common/item.h"
#include "../common/rulesys.h"
#include "../common/skills.h"
#include "../common/spdat.h"

#include "client.h"
#include "entity.h"
#include "mob.h"

#ifdef BOTS
#include "bot.h"
#endif

#include "quest_parser_collection.h"


#ifndef WIN32
#include <stdlib.h>
#include "../common/unix.h"
#endif


void Mob::CalcBonuses()
{
	CalcSpellBonuses(&spellbonuses);
	CalcMaxHP();
	CalcMaxMana();
	SetAttackTimer();

	rooted = FindType(SE_Root);
}

void NPC::CalcBonuses()
{
	Mob::CalcBonuses();
	memset(&aabonuses, 0, sizeof(StatBonuses));

	if(RuleB(NPC, UseItemBonusesForNonPets)){
		memset(&itembonuses, 0, sizeof(StatBonuses));
		CalcItemBonuses(&itembonuses);
	}
	else{
		if(GetOwner()){
			memset(&itembonuses, 0, sizeof(StatBonuses));
			CalcItemBonuses(&itembonuses);
		}
	}

	// This has to happen last, so we actually take the item bonuses into account.
	Mob::CalcBonuses();
}

void Client::CalcBonuses()
{
	memset(&itembonuses, 0, sizeof(StatBonuses));
	CalcItemBonuses(&itembonuses);
	CalcEdibleBonuses(&itembonuses);

	CalcSpellBonuses(&spellbonuses);

	Log.Out(Logs::Detail, Logs::AA, "Calculating AA Bonuses for %s.", this->GetCleanName());
	CalcAABonuses(&aabonuses);	//we're not quite ready for this
	Log.Out(Logs::Detail, Logs::AA, "Finished calculating AA Bonuses for %s.", this->GetCleanName());

	RecalcWeight();

	CalcAC();
	CalcATK();
	CalcHaste();

	CalcSTR();
	CalcSTA();
	CalcDEX();
	CalcAGI();
	CalcINT();
	CalcWIS();
	CalcCHA();

	CalcMR();
	CalcFR();
	CalcDR();
	CalcPR();
	CalcCR();

	CalcMaxHP();
	CalcMaxMana();
	CalcMaxEndurance();

	SetAttackTimer();

	rooted = FindType(SE_Root);

	XPRate = 100 + spellbonuses.XPRateMod;
}

int Client::CalcRecommendedLevelBonus(uint8 level, uint8 reclevel, int basestat)
{
	if( (reclevel > 0) && (level < reclevel) )
	{
		int32 statmod = (level * 10000 / reclevel) * basestat;

		if( statmod < 0 )
		{
			statmod -= 5000;
			return (statmod/10000);
		}
		else
		{
			statmod += 5000;
			return (statmod/10000);
		}
	}

	return 0;
}

void Client::CalcItemBonuses(StatBonuses* newbon) {
	//memset assumed to be done by caller.

	// Clear item faction mods
	ClearItemFactionBonuses();
	ShieldEquiped(false);

	unsigned int i;
	//should not include 21 (SLOT_AMMO)
	for (i = MainEar1; i < MainAmmo; i++) {
		const ItemInst* inst = m_inv[i];
		if(inst == 0)
			continue;
		AddItemBonuses(inst, newbon);

		//Check if item is secondary slot is a 'shield'. Required for multiple spelll effects.
		if (i == MainSecondary && (m_inv.GetItem(MainSecondary)->GetItem()->ItemType == ItemTypeShield))
			ShieldEquiped(true);
	}

	// Caps
	if(newbon->HPRegen > CalcHPRegenCap())
		newbon->HPRegen = CalcHPRegenCap();

	if(newbon->ManaRegen > CalcManaRegenCap())
		newbon->ManaRegen = CalcManaRegenCap();

	if(newbon->EnduranceRegen > CalcEnduranceRegenCap())
		newbon->EnduranceRegen = CalcEnduranceRegenCap();
}

void Client::AddItemBonuses(const ItemInst *inst, StatBonuses* newbon) {
	if(!inst || !inst->IsType(ItemClassCommon))
	{
		return;
	}

	const Item_Struct *item = inst->GetItem();

	if(!inst->IsEquipable(GetBaseRace(),GetClass()))
	{
		if(item->ItemType != ItemTypeFood && item->ItemType != ItemTypeDrink)
			return;
	}

	if(GetLevel() < item->ReqLevel)
	{
		return;
	}

	if(GetLevel() >= item->RecLevel)
	{
		newbon->AC += item->AC;
		newbon->HP += item->HP;
		newbon->Mana += item->Mana;
		newbon->STR += (item->AStr);
		newbon->STA += (item->ASta);
		newbon->DEX += (item->ADex);
		newbon->AGI += (item->AAgi);
		newbon->INT += (item->AInt);
		newbon->WIS += (item->AWis);
		newbon->CHA += (item->ACha);

		newbon->MR += (item->MR);
		newbon->FR += (item->FR);
		newbon->CR += (item->CR);
		newbon->PR += (item->PR);
		newbon->DR += (item->DR);

		newbon->STRCapMod += item->AStr;
		newbon->STACapMod += item->ASta;
		newbon->DEXCapMod += item->ADex;
		newbon->AGICapMod += item->AAgi;
		newbon->INTCapMod += item->AInt;
		newbon->WISCapMod += item->AWis;
		newbon->CHACapMod += item->ACha;
		newbon->MRCapMod += item->MR;
		newbon->CRCapMod += item->FR;
		newbon->FRCapMod += item->CR;
		newbon->PRCapMod += item->PR;
		newbon->DRCapMod += item->DR;

	}
	else
	{
		int lvl = GetLevel();
		int reclvl = item->RecLevel;

		newbon->AC += CalcRecommendedLevelBonus( lvl, reclvl, item->AC );
		newbon->HP += CalcRecommendedLevelBonus( lvl, reclvl, item->HP );
		newbon->Mana += CalcRecommendedLevelBonus( lvl, reclvl, item->Mana );
		newbon->STR += CalcRecommendedLevelBonus( lvl, reclvl, (item->AStr) );
		newbon->STA += CalcRecommendedLevelBonus( lvl, reclvl, (item->ASta) );
		newbon->DEX += CalcRecommendedLevelBonus( lvl, reclvl, (item->ADex) );
		newbon->AGI += CalcRecommendedLevelBonus( lvl, reclvl, (item->AAgi) );
		newbon->INT += CalcRecommendedLevelBonus( lvl, reclvl, (item->AInt) );
		newbon->WIS += CalcRecommendedLevelBonus( lvl, reclvl, (item->AWis) );
		newbon->CHA += CalcRecommendedLevelBonus( lvl, reclvl, (item->ACha) );

		newbon->MR += CalcRecommendedLevelBonus( lvl, reclvl, (item->MR) );
		newbon->FR += CalcRecommendedLevelBonus( lvl, reclvl, (item->FR) );
		newbon->CR += CalcRecommendedLevelBonus( lvl, reclvl, (item->CR) );
		newbon->PR += CalcRecommendedLevelBonus( lvl, reclvl, (item->PR) );
		newbon->DR += CalcRecommendedLevelBonus( lvl, reclvl, (item->DR) );

		newbon->STRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->AStr );
		newbon->STACapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->ASta );
		newbon->DEXCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->ADex );
		newbon->AGICapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->AAgi );
		newbon->INTCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->AInt );
		newbon->WISCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->AWis );
		newbon->CHACapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->ACha );
		newbon->MRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->MR );
		newbon->CRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->FR );
		newbon->FRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->CR );
		newbon->PRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->PR );
		newbon->DRCapMod += CalcRecommendedLevelBonus( lvl, reclvl, item->DR );

	}

	//FatherNitwit: New style haste, shields, and regens
	if (item->Worn.Effect>0 && (item->Worn.Type == ET_WornEffect)) { // latent effects
		ApplySpellsBonuses(item->Worn.Effect, item->Worn.Level > 0 ? item->Worn.Level : GetLevel(), newbon, 0, true);
	}

	if (item->Focus.Effect>0 && (item->Focus.Type == ET_Focus)) { // focus effects
		ApplySpellsBonuses(item->Focus.Effect, GetLevel(), newbon, 0, true);
	}

	switch(item->BardType)
	{
	case 51: /* All (e.g. Singing Short Sword) */
		{
			if(item->BardValue > newbon->singingMod)
				newbon->singingMod = item->BardValue;
			if(item->BardValue > newbon->brassMod)
				newbon->brassMod = item->BardValue;
			if(item->BardValue > newbon->stringedMod)
				newbon->stringedMod = item->BardValue;
			if(item->BardValue > newbon->percussionMod)
				newbon->percussionMod = item->BardValue;
			if(item->BardValue > newbon->windMod)
				newbon->windMod = item->BardValue;
			break;
		}
	case 50: /* Singing */
		{
			if(item->BardValue > newbon->singingMod)
				newbon->singingMod = item->BardValue;
			break;
		}
	case 23: /* Wind */
		{
			if(item->BardValue > newbon->windMod)
				newbon->windMod = item->BardValue;
			break;
		}
	case 24: /* stringed */
		{
			if(item->BardValue > newbon->stringedMod)
				newbon->stringedMod = item->BardValue;
			break;
		}
	case 25: /* brass */
		{
			if(item->BardValue > newbon->brassMod)
				newbon->brassMod = item->BardValue;
			break;
		}
	case 26: /* Percussion */
		{
			if(item->BardValue > newbon->percussionMod)
				newbon->percussionMod = item->BardValue;
			break;
		}
	}

	if (item->SkillModValue != 0 && item->SkillModType <= HIGHEST_SKILL){
		if ((item->SkillModValue > 0 && newbon->skillmod[item->SkillModType] < item->SkillModValue) ||
			(item->SkillModValue < 0 && newbon->skillmod[item->SkillModType] > item->SkillModValue))
		{
			newbon->skillmod[item->SkillModType] = item->SkillModValue;
		}
	}

	// Add Item Faction Mods
	if (item->FactionMod1)
	{
		if (item->FactionAmt1 > 0 && item->FactionAmt1 > GetItemFactionBonus(item->FactionMod1))
		{
			AddItemFactionBonus(item->FactionMod1, item->FactionAmt1);
		}
		else if (item->FactionAmt1 < 0 && item->FactionAmt1 < GetItemFactionBonus(item->FactionMod1))
		{
			AddItemFactionBonus(item->FactionMod1, item->FactionAmt1);
		}
	}
	if (item->FactionMod2)
	{
		if (item->FactionAmt2 > 0 && item->FactionAmt2 > GetItemFactionBonus(item->FactionMod2))
		{
			AddItemFactionBonus(item->FactionMod2, item->FactionAmt2);
		}
		else if (item->FactionAmt2 < 0 && item->FactionAmt2 < GetItemFactionBonus(item->FactionMod2))
		{
			AddItemFactionBonus(item->FactionMod2, item->FactionAmt2);
		}
	}
	if (item->FactionMod3)
	{
		if (item->FactionAmt3 > 0 && item->FactionAmt3 > GetItemFactionBonus(item->FactionMod3))
		{
			AddItemFactionBonus(item->FactionMod3, item->FactionAmt3);
		}
		else if (item->FactionAmt3 < 0 && item->FactionAmt3 < GetItemFactionBonus(item->FactionMod3))
		{
			AddItemFactionBonus(item->FactionMod3, item->FactionAmt3);
		}
	}
	if (item->FactionMod4)
	{
		if (item->FactionAmt4 > 0 && item->FactionAmt4 > GetItemFactionBonus(item->FactionMod4))
		{
			AddItemFactionBonus(item->FactionMod4, item->FactionAmt4);
		}
		else if (item->FactionAmt4 < 0 && item->FactionAmt4 < GetItemFactionBonus(item->FactionMod4))
		{
			AddItemFactionBonus(item->FactionMod4, item->FactionAmt4);
		}
	}
}

void Client::CalcEdibleBonuses(StatBonuses* newbon) {
	uint32 i;

	bool food = false;
	bool drink = false;
	for (i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_BAGS_BEGIN; i++)
	{
		if (food && drink)
			break;
		const ItemInst* inst = GetInv().GetItem(i);
		if (inst && inst->GetItem() && inst->IsType(ItemClassCommon)) {
			const Item_Struct *item=inst->GetItem();
			if (item->ItemType == ItemTypeFood && !food)
				food = true;
			else if (item->ItemType == ItemTypeDrink && !drink)
				drink = true;
			else
				continue;
			AddItemBonuses(inst, newbon);
		}
	}
	for (i = EmuConstants::GENERAL_BAGS_BEGIN; i <= EmuConstants::GENERAL_BAGS_END; i++)
	{
		if (food && drink)
			break;
		const ItemInst* inst = GetInv().GetItem(i);
		if (inst && inst->GetItem() && inst->IsType(ItemClassCommon)) {
			const Item_Struct *item=inst->GetItem();
			if (item->ItemType == ItemTypeFood && !food)
				food = true;
			else if (item->ItemType == ItemTypeDrink && !drink)
				drink = true;
			else
				continue;
			AddItemBonuses(inst, newbon);
		}
	}
}

void Client::CalcAABonuses(StatBonuses* newbon) {
	memset(newbon, 0, sizeof(StatBonuses));	//start fresh

	int i;
	uint32 slots = 0;
	uint32 aa_AA = 0;
	uint32 aa_value = 0;
	if(this->aa) {
		for (i = 0; i < MAX_PP_AA_ARRAY; i++) {	//iterate through all of the client's AAs
			if (this->aa[i]) {	// make sure aa exists or we'll crash zone
				aa_AA = this->aa[i]->AA;	//same as aaid from the aa_effects table
				aa_value = this->aa[i]->value;	//how many points in it
				if (aa_AA > 0 || aa_value > 0) {	//do we have the AA? if 1 of the 2 is set, we can assume we do
					//slots = database.GetTotalAALevels(aa_AA);	//find out how many effects from aa_effects table
					slots = zone->GetTotalAALevels(aa_AA);	//find out how many effects from aa_effects, which is loaded into memory
					if (slots > 0)	//and does it have any effects? may be able to put this above, not sure if it runs on each iteration
						ApplyAABonuses(aa_AA, slots, newbon);	//add the bonuses
				}
			}
		}
	}
}


//A lot of the normal spell functions (IsBlankSpellEffect, etc) are set for just spells (in common/spdat.h).
//For now, we'll just put them directly into the code and comment with the corresponding normal function
//Maybe we'll fix it later? :-D
void Client::ApplyAABonuses(uint32 aaid, uint32 slots, StatBonuses* newbon)
{
	if(slots == 0)	//sanity check. why bother if no slots to fill?
		return;

	//from AA_Ability struct
	uint32 effect = 0;
	int32 base1 = 0;
	int32 base2 = 0;	//only really used for SE_RaiseStatCap & SE_ReduceSkillTimer in aa_effects table
	uint32 slot = 0;

	std::map<uint32, std::map<uint32, AA_Ability> >::const_iterator find_iter = aa_effects.find(aaid);
	if(find_iter == aa_effects.end())
	{
		return;
	}

	for (std::map<uint32, AA_Ability>::const_iterator iter = aa_effects[aaid].begin(); iter != aa_effects[aaid].end(); ++iter) {
		effect = iter->second.skill_id;
		base1 = iter->second.base1;
		base2 = iter->second.base2;
		slot = iter->second.slot;

		//we default to 0 (SE_CurrentHP) for the effect, so if there aren't any base1/2 values, we'll just skip it
		if (effect == 0 && base1 == 0 && base2 == 0)
			continue;

		//IsBlankSpellEffect()
		if (effect == SE_Blank || (effect == SE_CHA && base1 == 0) || effect == SE_StackingCommand_Block || effect == SE_StackingCommand_Overwrite)
			continue;

		Log.Out(Logs::Detail, Logs::AA, "Applying Effect %d from AA %u in slot %d (base1: %d, base2: %d) on %s", effect, aaid, slot, base1, base2, this->GetCleanName());
			
		uint8 focus = IsFocusEffect(0, 0, true,effect);
		if (focus)
		{
			newbon->FocusEffects[focus] = effect;
			continue;
		}

		switch (effect)
		{
			//Note: AA effects that use accuracy are skill limited, while spell effect is not.
			case SE_Accuracy:
				if ((base2 == -1) && (newbon->Accuracy[HIGHEST_SKILL+1] < base1))
					newbon->Accuracy[HIGHEST_SKILL+1] = base1;
				else if (newbon->Accuracy[base2] < base1)
					newbon->Accuracy[base2] += base1;
				break;
			case SE_CurrentHP: //regens
				newbon->HPRegen += base1;
				break;
			case SE_CurrentEndurance:
				newbon->EnduranceRegen += base1;
				break;
			case SE_MovementSpeed:
				newbon->movementspeed += base1;	//should we let these stack?
				/*if (base1 > newbon->movementspeed)	//or should we use a total value?
					newbon->movementspeed = base1;*/
				break;
			case SE_STR:
				newbon->STR += base1;
				break;
			case SE_DEX:
				newbon->DEX += base1;
				break;
			case SE_AGI:
				newbon->AGI += base1;
				break;
			case SE_STA:
				newbon->STA += base1;
				break;
			case SE_INT:
				newbon->INT += base1;
				break;
			case SE_WIS:
				newbon->WIS += base1;
				break;
			case SE_CHA:
				newbon->CHA += base1;
				break;
			case SE_WaterBreathing:
				//handled by client
				break;
			case SE_CurrentMana:
				newbon->ManaRegen += base1;
				break;
			case SE_ItemManaRegenCapIncrease:
				newbon->ItemManaRegenCap += base1;
				break;
			case SE_ResistFire:
				newbon->FR += base1;
				break;
			case SE_ResistCold:
				newbon->CR += base1;
				break;
			case SE_ResistPoison:
				newbon->PR += base1;
				break;
			case SE_ResistDisease:
				newbon->DR += base1;
				break;
			case SE_ResistMagic:
				newbon->MR += base1;
				break;
			case SE_IncreaseSpellHaste:
				break;
			case SE_IncreaseRange:
				break;
			case SE_MaxHPChange:
				newbon->MaxHP += base1;
				break;
			case SE_Packrat:
				newbon->Packrat += base1;
				break;
			case SE_TwoHandBash:
				break;
			case SE_SetBreathLevel:
				break;
			case SE_RaiseStatCap:
				switch(base2)
				{
					//are these #define'd somewhere?
					case 0: //str
						newbon->STRCapMod += base1;
						break;
					case 1: //sta
						newbon->STACapMod += base1;
						break;
					case 2: //agi
						newbon->AGICapMod += base1;
						break;
					case 3: //dex
						newbon->DEXCapMod += base1;
						break;
					case 4: //wis
						newbon->WISCapMod += base1;
						break;
					case 5: //int
						newbon->INTCapMod += base1;
						break;
					case 6: //cha
						newbon->CHACapMod += base1;
						break;
					case 7: //mr
						newbon->MRCapMod += base1;
						break;
					case 8: //cr
						newbon->CRCapMod += base1;
						break;
					case 9: //fr
						newbon->FRCapMod += base1;
						break;
					case 10: //pr
						newbon->PRCapMod += base1;
						break;
					case 11: //dr
						newbon->DRCapMod += base1;
						break;
				}
				break;
			case SE_PetDiscipline2:
				break;
			case SE_SpellSlotIncrease:
				break;
			case SE_MysticalAttune:
				newbon->BuffSlotIncrease += base1;
				break;
			case SE_TotalHP:
				newbon->HP += base1;
				break;
			case SE_StunResist:
				newbon->StunResist += base1;
				break;
			case SE_SpellCritChance:
				newbon->CriticalSpellChance += base1;
				break;
			case SE_SpellCritDmgIncrease:
				newbon->SpellCritDmgIncrease += base1;
				break;
			case SE_DotCritDmgIncrease:
				newbon->DotCritDmgIncrease += base1;
				break;
			case SE_ResistSpellChance:
				newbon->ResistSpellChance += base1;
				break;
			case SE_CriticalHealChance:
				newbon->CriticalHealChance += base1;
				break;
			case SE_CriticalHealOverTime:
				newbon->CriticalHealOverTime += base1;
				break;
			case SE_CriticalDoTChance:
				newbon->CriticalDoTChance += base1;
				break;
			case SE_ReduceSkillTimer:
				newbon->SkillReuseTime[base2] += base1;
				break;
			case SE_Fearless:
				newbon->Fearless = true;
				break;
			case SE_FrontalStunResist:
				newbon->FrontalStunResist += base1;
				break;
			case SE_ImprovedBindWound:
				newbon->BindWound += base1;
				break;
			case SE_MaxBindWound:
				newbon->MaxBindWound += base1;
				break;
			case SE_ExtraAttackChance:
				newbon->ExtraAttackChance += base1;
				break;
			case SE_SeeInvis:
				newbon->SeeInvis = base1;
				break;
			case SE_BaseMovementSpeed:
				newbon->BaseMovementSpeed += base1;
				break;
			case SE_IncreaseRunSpeedCap:
				newbon->IncreaseRunSpeedCap += base1;
				break;
			case SE_ConsumeProjectile:
				newbon->ConsumeProjectile += base1;
				break;
			case SE_ForageAdditionalItems:
				newbon->ForageAdditionalItems += base1;
				break;
			case SE_Salvage:
				newbon->SalvageChance += base1;
				break;
			case SE_ArcheryDamageModifier:
				newbon->ArcheryDamageModifier += base1;
				break;
			case SE_DoubleRangedAttack:
				newbon->DoubleRangedAttack += base1;
				break;
			case SE_DamageShield:
				newbon->DamageShield += base1;
				break;
			case SE_CharmBreakChance:
				newbon->CharmBreakChance += base1;
				break;
			case SE_OffhandRiposteFail:
				newbon->OffhandRiposteFail += base1;
				break;
			case SE_ItemAttackCapIncrease:
				newbon->ItemATKCap += base1;
				break;
			case SE_GivePetGroupTarget:
				newbon->GivePetGroupTarget = true;
				break;
			case SE_ItemHPRegenCapIncrease:
				newbon->ItemHPRegenCap = +base1;
				break;
			case SE_Ambidexterity:
				newbon->Ambidexterity += base1;
				break;
			case SE_PetMaxHP:
				newbon->PetMaxHP += base1;
				break;
			case SE_AvoidMeleeChance:
				newbon->AvoidMeleeChanceEffect += base1;
				break;
			case SE_CombatStability:
				newbon->CombatStability += base1;
				break;
			case SE_AddSingingMod:
				switch (base2)
				{
					case ItemTypeWindInstrument:
						newbon->windMod += base1;
						break;
					case ItemTypeStringedInstrument:
						newbon->stringedMod += base1;
						break;
					case ItemTypeBrassInstrument:
						newbon->brassMod += base1;
						break;
					case ItemTypePercussionInstrument:
						newbon->percussionMod += base1;
						break;
				}
				break;
			case SE_SongModCap:
				newbon->songModCap += base1;
				break;
			case SE_ShieldBlock:
				newbon->ShieldBlock += base1;
				break;
			case SE_ShieldEquipHateMod:
				newbon->ShieldEquipHateMod += base1;
				break;
			case SE_ShieldEquipDmgMod:
				newbon->ShieldEquipDmgMod[0] += base1;
				newbon->ShieldEquipDmgMod[1] += base2;
				break;
			case SE_SecondaryDmgInc:
				newbon->SecondaryDmgInc = true;
				break;
			case SE_ChangeAggro:
				newbon->hatemod += base1;
				break;
			case SE_EndurancePool:
				newbon->Endurance += base1;
				break;
			case SE_ChannelChanceItems:
				newbon->ChannelChanceItems += base1;
				break;
			case SE_ChannelChanceSpells:
				newbon->ChannelChanceSpells += base1;
				break;
			case SE_DoubleSpecialAttack:
				newbon->DoubleSpecialAttack += base1;
				break;
			case SE_FrontalBackstabMinDmg:
				newbon->FrontalBackstabMinDmg = true;
				break;
			case SE_BlockBehind:
				newbon->BlockBehind += base1;
				break;
			
			case SE_StrikeThrough:
			case SE_StrikeThrough2:
				newbon->StrikeThrough += base1;
				break;
			case SE_DoubleAttackChance:
				newbon->DoubleAttackChance += base1;
				break;
			case SE_GiveDoubleAttack:
				newbon->GiveDoubleAttack += base1;
				break;
			case SE_RiposteChance:
				newbon->RiposteChance += base1;
				break;
			case SE_Flurry:
				newbon->FlurryChance += base1;
				break;
			case SE_PetFlurry:
				newbon->PetFlurry += base1;
				break;
			case SE_BardSongRange:
				newbon->SongRange += base1;
				break;
			case SE_RootBreakChance:
				newbon->RootBreakChance += base1;
				break;
			case SE_UnfailingDivinity:
				newbon->UnfailingDivinity += base1;
				break;
			case SE_CrippBlowChance:
				newbon->CrippBlowChance += base1;
				break;

			case SE_HitChance:
			{
				if(base2 == -1)
					newbon->HitChanceEffect[HIGHEST_SKILL+1] += base1;
				else
					newbon->HitChanceEffect[base2] += base1;
			}

			case SE_SpellOnDeath:
				for(int i = 0; i < MAX_SPELL_TRIGGER*2; i+=2)
				{
					if(!newbon->SpellOnDeath[i])
					{
						// base1 = SpellID to be triggered, base2 = chance to fire
						newbon->SpellOnDeath[i] = base1;
						newbon->SpellOnDeath[i+1] = base2;
						break;
					}
				}
			break;

			case SE_CriticalHitChance:
			{
				if(base2 == -1)
					newbon->CriticalHitChance[HIGHEST_SKILL+1] += base1;
				else
					newbon->CriticalHitChance[base2] += base1;
			}
			break;

			case SE_CriticalSpellChance:
			{
				newbon->CriticalSpellChance += base1;

				if (base2 > newbon->SpellCritDmgIncNoStack)
					newbon->SpellCritDmgIncNoStack = base2;

				break;
			}

			case SE_ResistFearChance:
			{
				if(base1 == 100) // If we reach 100% in a single spell/item then we should be immune to negative fear resist effects until our immunity is over
					newbon->Fearless = true;

				newbon->ResistFearChance += base1; // these should stack
				break;
			}

			case SE_SkillDamageAmount:
			{
				if(base2 == -1)
					newbon->SkillDamageAmount[HIGHEST_SKILL+1] += base1;
				else
					newbon->SkillDamageAmount[base2] += base1;
				break;
			}

			case SE_SpecialAttackKBProc:
			{
				//You can only have one of these per client. [AA Dragon Punch]
				newbon->SpecialAttackKBProc[0] = base1; //Chance base 100 = 25% proc rate
				newbon->SpecialAttackKBProc[1] = base2; //Skill to KB Proc Off
				break;
			}

			case SE_DamageModifier:
			{
				if(base2 == -1)
					newbon->DamageModifier[HIGHEST_SKILL+1] += base1;
				else
					newbon->DamageModifier[base2] += base1;
				break;
			}

			case SE_DamageModifier2:
			{
				if(base2 == -1)
					newbon->DamageModifier2[HIGHEST_SKILL+1] += base1;
				else
					newbon->DamageModifier2[base2] += base1;
				break;
			}

			case SE_SlayUndead:
			{
				if(newbon->SlayUndead[1] < base1)
					newbon->SlayUndead[0] = base1; // Rate
					newbon->SlayUndead[1] = base2; // Damage Modifier
				break;
			}

			case SE_DoubleRiposte:
			{
				newbon->DoubleRiposte += base1;
			}

			case SE_GiveDoubleRiposte:
			{
				//0=Regular Riposte 1=Skill Attack Riposte 2=Skill
				if(base2 == 0){
					if(newbon->GiveDoubleRiposte[0] < base1)
						newbon->GiveDoubleRiposte[0] = base1;
				}
				//Only for special attacks.
				else if(base2 > 0 && (newbon->GiveDoubleRiposte[1] < base1)){
					newbon->GiveDoubleRiposte[1] = base1;
					newbon->GiveDoubleRiposte[2] = base2;
				}

				break;
			}

			//Kayen: Not sure best way to implement this yet.
			//Physically raises skill cap ie if 55/55 it will raise to 55/60
			case SE_RaiseSkillCap:
			{
				if(newbon->RaiseSkillCap[0] < base1){
					newbon->RaiseSkillCap[0] = base1; //value
					newbon->RaiseSkillCap[1] = base2; //skill
				}
				break;
			}

			case SE_MasteryofPast:
			{
				if(newbon->MasteryofPast < base1)
					newbon->MasteryofPast = base1;
				break;
			}

			case SE_CastingLevel2:
			case SE_CastingLevel:
			{
				newbon->effective_casting_level += base1;
				break;
			}

			case SE_DivineSave:
			{
				if(newbon->DivineSaveChance[0] < base1)
				{
					newbon->DivineSaveChance[0] = base1;
					newbon->DivineSaveChance[1] = base2;
				}
				break;
			}


			case SE_SpellEffectResistChance:
			{
				for(int e = 0; e < MAX_RESISTABLE_EFFECTS*2; e+=2)
				{
					if(newbon->SEResist[e+1] && (newbon->SEResist[e] == base2) && (newbon->SEResist[e+1] < base1)){
						newbon->SEResist[e] = base2; //Spell Effect ID
						newbon->SEResist[e+1] = base1; //Resist Chance
						break;
					}
					else if (!newbon->SEResist[e+1]){
						newbon->SEResist[e] = base2; //Spell Effect ID
						newbon->SEResist[e+1] = base1; //Resist Chance
						break;
					}
				}
				break;
			}

			case SE_MitigateDamageShield:
			{
				if (base1 < 0)
					base1 = base1*(-1);

				newbon->DSMitigationOffHand += base1;
				break;
			}

			case SE_FinishingBlow:
			{
				//base1 = chance, base2 = damage
				if (newbon->FinishingBlow[1] < base2){
					newbon->FinishingBlow[0] = base1;
					newbon->FinishingBlow[1] = base2;
				}
				break;
			}

			case SE_FinishingBlowLvl:
			{
				//base1 = level, base2 = ??? (Set to 200 in AA data, possible proc rate mod?)
				if (newbon->FinishingBlowLvl[0] < base1){
					newbon->FinishingBlowLvl[0] = base1;
					newbon->FinishingBlowLvl[1] = base2;
				}
				break;
			}

			case SE_IncreaseChanceMemwipe:
				newbon->IncreaseChanceMemwipe += base1;
				break;

			case SE_CriticalMend:
				newbon->CriticalMend += base1;
				break;

			case SE_HealRate:
				newbon->HealRate += base1;
				break;

			case SE_MeleeLifetap:
			{

				if((base1 < 0) && (newbon->MeleeLifetap > base1))
					newbon->MeleeLifetap = base1;

				else if(newbon->MeleeLifetap < base1)
					newbon->MeleeLifetap = base1;
				break;
			}

			case SE_Vampirism:
				newbon->Vampirism += base1;
				break;			

			case SE_Berserk:
				newbon->BerserkSPA = true;
				break;

			case SE_Metabolism:
				newbon->Metabolism += base1;
				break;

			case SE_ImprovedReclaimEnergy:
			{
				if((base1 < 0) && (newbon->ImprovedReclaimEnergy > base1))
					newbon->ImprovedReclaimEnergy = base1;

				else if(newbon->ImprovedReclaimEnergy < base1)
					newbon->ImprovedReclaimEnergy = base1;
				break;
			}

			case SE_HeadShot:
			{
				if(newbon->HeadShot[1] < base2){
					newbon->HeadShot[0] = base1;
					newbon->HeadShot[1] = base2;
				}
				break;
			}

			case SE_HeadShotLevel:
			{
				if(newbon->HSLevel < base1)
					newbon->HSLevel = base1;
				break;
			}

			case SE_Assassinate:
			{
				if(newbon->Assassinate[1] < base2){
					newbon->Assassinate[0] = base1;
					newbon->Assassinate[1] = base2;
				}
				break;
			}

			case SE_AssassinateLevel:
			{
				if(newbon->AssassinateLevel < base1)
					newbon->AssassinateLevel = base1;
				break;
			}

			case SE_MeleeVulnerability:
				newbon->MeleeVulnerability += base1;
				break;

			case SE_FactionModPct:
			{
				if((base1 < 0) && (newbon->FactionModPct > base1))
					newbon->FactionModPct = base1;

				else if(newbon->FactionModPct < base1)
					newbon->FactionModPct = base1;
				break;
			}

			case SE_IllusionPersistence:
				newbon->IllusionPersistence = true;
				break;

			case SE_LimitToSkill:{
				if (base1 <= HIGHEST_SKILL)
					newbon->LimitToSkill[base1] = true;
				break;
			}

			case SE_MeleeMitigation:
				newbon->MeleeMitigationEffect -= base1;
				break;

		}
	}
}

void Mob::CalcSpellBonuses(StatBonuses* newbon)
{
	int i;

	memset(newbon, 0, sizeof(StatBonuses));
	newbon->AggroRange = -1;
	newbon->AssistRange = -1;

	int buff_count = GetMaxTotalSlots();
	for(i = 0; i < buff_count; i++) {
		if(buffs[i].spellid != SPELL_UNKNOWN){
			ApplySpellsBonuses(buffs[i].spellid, buffs[i].casterlevel, newbon, buffs[i].casterid, false, buffs[i].instrumentmod, buffs[i].ticsremaining,i);

			if (buffs[i].numhits > 0)
				Numhits(true);
		}
	}

	//Applies any perma NPC spell bonuses from npc_spells_effects table.
	if (IsNPC())
		CastToNPC()->ApplyAISpellEffects(newbon);

	//this prolly suffer from roundoff error slightly...
	newbon->AC = newbon->AC * 10 / 34;	//ratio determined impirically from client.
	if (GetClass() == BARD) newbon->ManaRegen = 0; // Bards do not get mana regen from spells.
}

void Mob::ApplySpellsBonuses(uint16 spell_id, uint8 casterlevel, StatBonuses* new_bonus, uint16 casterId, bool item_bonus, int16 instrumentmod, uint32 ticsremaining, int buffslot,
							 bool IsAISpellEffect, uint16 effect_id, int32 se_base, int32 se_limit, int32 se_max)
{
	int i, effect_value, base2, max, effectid;
	Mob *caster = nullptr;

	if(!IsAISpellEffect && !IsValidSpell(spell_id))
		return;

	if(casterId > 0)
		caster = entity_list.GetMob(casterId);

	for (i = 0; i < EFFECT_COUNT; i++)
	{
		//Buffs/Item effects
		if (!IsAISpellEffect) {

			if(IsBlankSpellEffect(spell_id, i))
				continue;

			uint8 focus = IsFocusEffect(spell_id, i);
			if (focus)
			{
				new_bonus->FocusEffects[focus] = spells[spell_id].effectid[i];
				continue;
			}

		
			effectid = spells[spell_id].effectid[i];
			effect_value = CalcSpellEffectValue(spell_id, i, casterlevel, caster, ticsremaining);
			base2 = spells[spell_id].base2[i];
			max = spells[spell_id].max[i];
		}
		//Use AISpellEffects
		else {
			effectid = effect_id;
			effect_value = se_base;
			base2 = se_limit;
			max = se_max;
			i = EFFECT_COUNT; //End the loop
		}

		switch (effectid)
		{
			case SE_CurrentHP: //regens
				if(effect_value > 0) {
					new_bonus->HPRegen += effect_value;
				}
				break;

			case SE_CurrentEndurance:
				new_bonus->EnduranceRegen += effect_value;
				break;

			case SE_ChangeFrenzyRad:
			{
				// redundant to have level check here
				if(new_bonus->AggroRange == -1 || effect_value < new_bonus->AggroRange)
				{
					new_bonus->AggroRange = static_cast<float>(effect_value);
				}
				break;
			}

			case SE_Harmony:
			{
				// Harmony effect as buff - kinda tricky
				// harmony could stack with a lull spell, which has better aggro range
				// take the one with less range in any case
				if(new_bonus->AssistRange == -1 || effect_value < new_bonus->AssistRange)
				{
					new_bonus->AssistRange = static_cast<float>(effect_value);
				}
				break;
			}

			case SE_AttackSpeed:
			{
				if ((effect_value - 100) > 0) { // Haste
					if (new_bonus->haste < 0) break; // Slowed - Don't apply haste
					if ((effect_value - 100) > new_bonus->haste) {
						new_bonus->haste = effect_value - 100;
					}
				}
				else if ((effect_value - 100) < 0) { // Slow
					int real_slow_value = (100 - effect_value) * -1;
					real_slow_value -= ((real_slow_value * GetSlowMitigation()/100));
					if (real_slow_value < new_bonus->haste)
						new_bonus->haste = real_slow_value;
				}
				break;
			}

			case SE_AttackSpeed2:
			{
				if ((effect_value - 100) > 0) { // Haste V2 - Stacks with V1 but does not Overcap
					if (new_bonus->hastetype2 < 0) break; //Slowed - Don't apply haste2
					if ((effect_value - 100) > new_bonus->hastetype2) {
						new_bonus->hastetype2 = effect_value - 100;
					}
				}
				else if ((effect_value - 100) < 0) { // Slow
					int real_slow_value = (100 - effect_value) * -1;
					real_slow_value -= ((real_slow_value * GetSlowMitigation()/100));
					if (real_slow_value < new_bonus->hastetype2)
						new_bonus->hastetype2 = real_slow_value;
				}
				break;
			}

			case SE_AttackSpeed3:
			{
				if (effect_value < 0){ //Slow
					effect_value -= ((effect_value * GetSlowMitigation()/100));
					if (effect_value < new_bonus->hastetype3)
						new_bonus->hastetype3 = effect_value;
				}

				else if (effect_value > 0) { // Haste V3 - Stacks and Overcaps
					if (effect_value > new_bonus->hastetype3) {
						new_bonus->hastetype3 = effect_value;
					}
				}
				break;
			}

			case SE_AttackSpeed4:
			{
				// These don't generate the IMMUNE_ATKSPEED message and the icon shows up
				// but have no effect on the mobs attack speed
				if (GetSpecialAbility(UNSLOWABLE))
					break;

				if (effect_value < 0) //A few spells use negative values(Descriptions all indicate it should be a slow)
					effect_value = effect_value * -1;

				if (effect_value > 0 && effect_value > new_bonus->inhibitmelee) {
					effect_value -= ((effect_value * GetSlowMitigation()/100));
					if (effect_value > new_bonus->inhibitmelee) 
						new_bonus->inhibitmelee = effect_value;
				}
			
				break;
			}

			case SE_TotalHP:
			{
				new_bonus->HP += effect_value;
				break;
			}

			case SE_ManaRegen_v2:
			case SE_CurrentMana:
			{
				new_bonus->ManaRegen += effect_value;
				break;
			}

			case SE_ManaPool:
			{
				new_bonus->Mana += effect_value;
				break;
			}

			case SE_Stamina:
			{
				new_bonus->EnduranceReduction += effect_value;
				break;
			}

			case SE_ACv2:
			case SE_ArmorClass:
			{
				new_bonus->AC += effect_value;
				break;
			}

			case SE_ATK:
			{
				new_bonus->ATK += effect_value;
				break;
			}

			case SE_STR:
			{
				new_bonus->STR += effect_value;
				break;
			}

			case SE_DEX:
			{
				new_bonus->DEX += effect_value;
				break;
			}

			case SE_AGI:
			{
				new_bonus->AGI += effect_value;
				break;
			}

			case SE_STA:
			{
				new_bonus->STA += effect_value;
				break;
			}

			case SE_INT:
			{
				new_bonus->INT += effect_value;
				break;
			}

			case SE_WIS:
			{
				new_bonus->WIS += effect_value;
				break;
			}

			case SE_CHA:
			{
				if (spells[spell_id].base[i] != 0) {
					new_bonus->CHA += effect_value;
				}
				break;
			}

			case SE_AllStats:
			{
				new_bonus->STR += effect_value;
				new_bonus->DEX += effect_value;
				new_bonus->AGI += effect_value;
				new_bonus->STA += effect_value;
				new_bonus->INT += effect_value;
				new_bonus->WIS += effect_value;
				new_bonus->CHA += effect_value;
				break;
			}

			case SE_ResistFire:
			{
				new_bonus->FR += effect_value;
				break;
			}

			case SE_ResistCold:
			{
				new_bonus->CR += effect_value;
				break;
			}

			case SE_ResistPoison:
			{
				new_bonus->PR += effect_value;
				break;
			}

			case SE_ResistDisease:
			{
				new_bonus->DR += effect_value;
				break;
			}

			case SE_ResistMagic:
			{
				new_bonus->MR += effect_value;
				break;
			}

			case SE_ResistAll:
			{
				new_bonus->MR += effect_value;
				new_bonus->DR += effect_value;
				new_bonus->PR += effect_value;
				new_bonus->CR += effect_value;
				new_bonus->FR += effect_value;
				break;
			}

			case SE_RaiseStatCap:
			{
				switch(spells[spell_id].base2[i])
				{
					//are these #define'd somewhere?
					case 0: //str
						new_bonus->STRCapMod += effect_value;
						break;
					case 1: //sta
						new_bonus->STACapMod += effect_value;
						break;
					case 2: //agi
						new_bonus->AGICapMod += effect_value;
						break;
					case 3: //dex
						new_bonus->DEXCapMod += effect_value;
						break;
					case 4: //wis
						new_bonus->WISCapMod += effect_value;
						break;
					case 5: //int
						new_bonus->INTCapMod += effect_value;
						break;
					case 6: //cha
						new_bonus->CHACapMod += effect_value;
						break;
					case 7: //mr
						new_bonus->MRCapMod += effect_value;
						break;
					case 8: //cr
						new_bonus->CRCapMod += effect_value;
						break;
					case 9: //fr
						new_bonus->FRCapMod += effect_value;
						break;
					case 10: //pr
						new_bonus->PRCapMod += effect_value;
						break;
					case 11: //dr
						new_bonus->DRCapMod += effect_value;
						break;
				}
				break;
			}

			case SE_CastingLevel2:
			case SE_CastingLevel:	// Brilliance of Ro
			{
				new_bonus->effective_casting_level += effect_value;
				break;
			}

			case SE_MovementSpeed:
				new_bonus->movementspeed += effect_value;
				break;

			case SE_SpellDamageShield:
				new_bonus->SpellDamageShield += effect_value;
				break;

			case SE_DamageShield:
			{
				new_bonus->DamageShield += effect_value;
				new_bonus->DamageShieldSpellID = spell_id;
				//When using npc_spells_effects MAX value can be set to determine DS Type
				if (IsAISpellEffect && max)
					new_bonus->DamageShieldType = GetDamageShieldType(spell_id, max);
				else
					new_bonus->DamageShieldType = GetDamageShieldType(spell_id);
				
				break;
			}

			case SE_ReverseDS:
			{
				new_bonus->ReverseDamageShield += effect_value;
				new_bonus->ReverseDamageShieldSpellID = spell_id;

				if (IsAISpellEffect && max)
					new_bonus->ReverseDamageShieldType = GetDamageShieldType(spell_id, max);
				else
					new_bonus->ReverseDamageShieldType = GetDamageShieldType(spell_id);
				break;
			}

			case SE_Reflect:
				new_bonus->reflect_chance += effect_value;
				break;

			case SE_Amplification:
				new_bonus->Amplification += effect_value;
				break;

			case SE_ChangeAggro:
				new_bonus->hatemod += effect_value;
				break;

			case SE_MeleeMitigation:
				//for some reason... this value is negative for increased mitigation
				new_bonus->MeleeMitigationEffect -= effect_value;
				break;

			case SE_CriticalHitChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus) {
					if(base2 == -1)
						new_bonus->CriticalHitChance[HIGHEST_SKILL+1] += effect_value;
					else
						new_bonus->CriticalHitChance[base2] += effect_value;
				}

				else if(effect_value < 0) {

					if(base2 == -1 && new_bonus->CriticalHitChance[HIGHEST_SKILL+1] > effect_value)
						new_bonus->CriticalHitChance[HIGHEST_SKILL+1] = effect_value;
					else if(base2 != -1 && new_bonus->CriticalHitChance[base2] > effect_value)
						new_bonus->CriticalHitChance[base2] = effect_value;
				}


				else if(base2 == -1 && new_bonus->CriticalHitChance[HIGHEST_SKILL+1] < effect_value)
						new_bonus->CriticalHitChance[HIGHEST_SKILL+1] = effect_value;
				else if(base2 != -1 && new_bonus->CriticalHitChance[base2] < effect_value)
						new_bonus->CriticalHitChance[base2] = effect_value;

				break;
			}

			case SE_CrippBlowChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->CrippBlowChance += effect_value;

				else if((effect_value < 0) && (new_bonus->CrippBlowChance > effect_value))
						new_bonus->CrippBlowChance = effect_value;

				else if(new_bonus->CrippBlowChance < effect_value)
					new_bonus->CrippBlowChance = effect_value;

				break;
			}

			case SE_AvoidMeleeChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->AvoidMeleeChanceEffect += effect_value;

				else if((effect_value < 0) && (new_bonus->AvoidMeleeChanceEffect > effect_value))
					new_bonus->AvoidMeleeChanceEffect = effect_value;

				else if(new_bonus->AvoidMeleeChanceEffect < effect_value)
					new_bonus->AvoidMeleeChanceEffect = effect_value;
				break;
			}

			case SE_RiposteChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->RiposteChance += effect_value;

				else if((effect_value < 0) && (new_bonus->RiposteChance > effect_value))
					new_bonus->RiposteChance = effect_value;

				else if(new_bonus->RiposteChance < effect_value)
					new_bonus->RiposteChance = effect_value;
				break;
			}

			case SE_DodgeChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->DodgeChance += effect_value;

				else if((effect_value < 0) && (new_bonus->DodgeChance > effect_value))
					new_bonus->DodgeChance = effect_value;

				if(new_bonus->DodgeChance < effect_value)
					new_bonus->DodgeChance = effect_value;
				break;
			}

			case SE_ParryChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->ParryChance += effect_value;

				else if((effect_value < 0) && (new_bonus->ParryChance > effect_value))
					new_bonus->ParryChance = effect_value;

				if(new_bonus->ParryChance < effect_value)
					new_bonus->ParryChance = effect_value;
				break;
			}

			case SE_DualWieldChance:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->DualWieldChance += effect_value;

				else if((effect_value < 0) && (new_bonus->DualWieldChance > effect_value))
					new_bonus->DualWieldChance = effect_value;

				if(new_bonus->DualWieldChance < effect_value)
					new_bonus->DualWieldChance = effect_value;
				break;
			}

			case SE_DoubleAttackChance:
			{

				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->DoubleAttackChance += effect_value;

				else if((effect_value < 0) && (new_bonus->DoubleAttackChance > effect_value))
					new_bonus->DoubleAttackChance = effect_value;

				if(new_bonus->DoubleAttackChance < effect_value)
					new_bonus->DoubleAttackChance = effect_value;
				break;
			}

			case SE_TripleAttackChance:
			{

				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->TripleAttackChance += effect_value;

				else if((effect_value < 0) && (new_bonus->TripleAttackChance > effect_value))
					new_bonus->TripleAttackChance = effect_value;

				if(new_bonus->TripleAttackChance < effect_value)
					new_bonus->TripleAttackChance = effect_value;
				break;
			}

			case SE_MeleeLifetap:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->MeleeLifetap += spells[spell_id].base[i];

				else if((effect_value < 0) && (new_bonus->MeleeLifetap > effect_value))
					new_bonus->MeleeLifetap = effect_value;

				else if(new_bonus->MeleeLifetap < effect_value)
					new_bonus->MeleeLifetap = effect_value;
				break;
			}

			case SE_Vampirism:
				new_bonus->Vampirism += effect_value;
				break;	

			case SE_AllInstrumentMod:
			{
				if(effect_value > new_bonus->singingMod)
					new_bonus->singingMod = effect_value;
				if(effect_value > new_bonus->brassMod)
					new_bonus->brassMod = effect_value;
				if(effect_value > new_bonus->percussionMod)
					new_bonus->percussionMod = effect_value;
				if(effect_value > new_bonus->windMod)
					new_bonus->windMod = effect_value;
				if(effect_value > new_bonus->stringedMod)
					new_bonus->stringedMod = effect_value;
				break;
			}

			case SE_ResistSpellChance:
				new_bonus->ResistSpellChance += effect_value;
				break;

			case SE_ResistFearChance:
			{
				if(effect_value == 100) // If we reach 100% in a single spell/item then we should be immune to negative fear resist effects until our immunity is over
					new_bonus->Fearless = true;

				new_bonus->ResistFearChance += effect_value; // these should stack
				break;
			}

			case SE_Fearless:
				new_bonus->Fearless = true;
				break;

			case SE_HundredHands:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus)
					new_bonus->HundredHands += effect_value;

				if (effect_value > 0 && effect_value > new_bonus->HundredHands)
					new_bonus->HundredHands = effect_value; //Increase Weapon Delay
				else if (effect_value < 0 && effect_value < new_bonus->HundredHands)
					new_bonus->HundredHands = effect_value; //Decrease Weapon Delay
				break;
			}

			case SE_MeleeSkillCheck:
			{
				if(new_bonus->MeleeSkillCheck < effect_value) {
					new_bonus->MeleeSkillCheck = effect_value;
					new_bonus->MeleeSkillCheckSkill = base2==-1?255:base2;
				}
				break;
			}

			case SE_IncreaseArchery:
			case SE_HitChance:
			{

				if (RuleB(Spells, AdditiveBonusValues) && item_bonus){
					if(base2 == -1)
						new_bonus->HitChanceEffect[HIGHEST_SKILL+1] += effect_value;
					else
						new_bonus->HitChanceEffect[base2] += effect_value;
				}

				else if(base2 == -1){

					if ((effect_value < 0) && (new_bonus->HitChanceEffect[HIGHEST_SKILL+1] > effect_value))
						new_bonus->HitChanceEffect[HIGHEST_SKILL+1] = effect_value;

					else if (!new_bonus->HitChanceEffect[HIGHEST_SKILL+1] ||
							((new_bonus->HitChanceEffect[HIGHEST_SKILL+1] > 0) && (new_bonus->HitChanceEffect[HIGHEST_SKILL+1] < effect_value)))
							new_bonus->HitChanceEffect[HIGHEST_SKILL+1] = effect_value;
				}

				else {

					if ((effect_value < 0) && (new_bonus->HitChanceEffect[base2] > effect_value))
						new_bonus->HitChanceEffect[base2] = effect_value;

					else if (!new_bonus->HitChanceEffect[base2] ||
							((new_bonus->HitChanceEffect[base2] > 0) && (new_bonus->HitChanceEffect[base2] < effect_value)))
							new_bonus->HitChanceEffect[base2] = effect_value;
				}

				break;

			}

			case SE_DamageModifier:
			{
				if(base2 == -1)
					new_bonus->DamageModifier[HIGHEST_SKILL+1] += effect_value;
				else
					new_bonus->DamageModifier[base2] += effect_value;
				break;
			}

			case SE_DamageModifier2:
			{
				if(base2 == -1)
					new_bonus->DamageModifier2[HIGHEST_SKILL+1] += effect_value;
				else
					new_bonus->DamageModifier2[base2] += effect_value;
				break;
			}

			case SE_MinDamageModifier:
			{
				if(base2 == -1)
					new_bonus->MinDamageModifier[HIGHEST_SKILL+1] += effect_value;
				else
					new_bonus->MinDamageModifier[base2] += effect_value;
				break;
			}

			case SE_StunResist:
			{
				if(new_bonus->StunResist < effect_value)
					new_bonus->StunResist = effect_value;
				break;
			}

			case SE_ExtraAttackChance:
				new_bonus->ExtraAttackChance += effect_value;
				break;

			case SE_PercentXPIncrease:
			{
				if(new_bonus->XPRateMod < effect_value)
					new_bonus->XPRateMod = effect_value;
				break;
			}

			case SE_DeathSave:
			{
				if(new_bonus->DeathSave[0] < effect_value)
				{
					new_bonus->DeathSave[0] = effect_value; //1='Partial' 2='Full'
					new_bonus->DeathSave[1] = buffslot;
					//These are used in later expansion spell effects.
					new_bonus->DeathSave[2] = base2;//Min level for HealAmt
					new_bonus->DeathSave[3] = max;//HealAmt
				}
				break;
			}

			case SE_DivineSave:
			{
				if (RuleB(Spells, AdditiveBonusValues) && item_bonus) {
					new_bonus->DivineSaveChance[0] += effect_value;
					new_bonus->DivineSaveChance[1] = 0;
				}

				else if(new_bonus->DivineSaveChance[0] < effect_value)
				{
					new_bonus->DivineSaveChance[0] = effect_value;
					new_bonus->DivineSaveChance[1] = base2;
					//SetDeathSaveChance(true);
				}
				break;
			}

			case SE_Flurry:
				new_bonus->FlurryChance += effect_value;
				break;

			case SE_Accuracy:
			{
				if ((effect_value < 0) && (new_bonus->Accuracy[HIGHEST_SKILL+1] > effect_value))
						new_bonus->Accuracy[HIGHEST_SKILL+1] = effect_value;

				else if (!new_bonus->Accuracy[HIGHEST_SKILL+1] ||
						((new_bonus->Accuracy[HIGHEST_SKILL+1] > 0) && (new_bonus->Accuracy[HIGHEST_SKILL+1] < effect_value)))
							new_bonus->Accuracy[HIGHEST_SKILL+1] = effect_value;
				break;
			}

			case SE_MaxHPChange:
				new_bonus->MaxHPChange += effect_value;
				break;

			case SE_EndurancePool:
				new_bonus->Endurance += effect_value;
				break;

			case SE_HealRate:
				new_bonus->HealRate += effect_value;
				break;

			case SE_SkillDamageTaken:
			{
				//When using npc_spells_effects if MAX value set, use stackable quest based modifier.
				if (IsAISpellEffect && max){
					if(base2 == -1)
						SkillDmgTaken_Mod[HIGHEST_SKILL+1] = effect_value;
					else
						SkillDmgTaken_Mod[base2] = effect_value;
				}
				else {

					if(base2 == -1)
						new_bonus->SkillDmgTaken[HIGHEST_SKILL+1] += effect_value;
					else
						new_bonus->SkillDmgTaken[base2] += effect_value;

				}
				break;
			}

			case SE_SpellCritChance:
				new_bonus->CriticalSpellChance += effect_value;
				break;

			case SE_CriticalSpellChance:
			{
				new_bonus->CriticalSpellChance += effect_value;
				
				if (base2 > new_bonus->SpellCritDmgIncNoStack)
					new_bonus->SpellCritDmgIncNoStack = base2;
				break;
			}

			case SE_SpellCritDmgIncrease:
				new_bonus->SpellCritDmgIncrease += effect_value;
				break;

			case SE_DotCritDmgIncrease:
				new_bonus->DotCritDmgIncrease += effect_value;
				break;

			case SE_CriticalHealChance:
				new_bonus->CriticalHealChance += effect_value;
				break;

			case SE_CriticalHealOverTime:
				new_bonus->CriticalHealOverTime += effect_value;
				break;

			case SE_CriticalHealDecay:
				new_bonus->CriticalHealDecay = true;
				break;

			case SE_CriticalRegenDecay:
				new_bonus->CriticalRegenDecay = true;
				break;

			case SE_CriticalDotDecay:
				new_bonus->CriticalDotDecay = true;
				break;

			case SE_CriticalDoTChance:
				new_bonus->CriticalDoTChance += effect_value;
				break;

			case SE_SpellOnDeath:
			{
				for(int e = 0; e < MAX_SPELL_TRIGGER; e+=2)
				{
					if(!new_bonus->SpellOnDeath[e])
					{
						// Base2 = Spell to fire | Base1 = % chance
						new_bonus->SpellOnDeath[e] = base2;
						new_bonus->SpellOnDeath[e+1] = effect_value;
						break;
					}
				}
				break;
			}

			case SE_ReduceSkillTimer:
			{
				if(new_bonus->SkillReuseTime[base2] < effect_value)
					new_bonus->SkillReuseTime[base2] = effect_value;
				break;
			}

			case SE_SkillDamageAmount:
			{
				if(base2 == -1)
					new_bonus->SkillDamageAmount[HIGHEST_SKILL+1] += effect_value;
				else
					new_bonus->SkillDamageAmount[base2] += effect_value;
				break;
			}

			case SE_GravityEffect:
				new_bonus->GravityEffect = 1;
				break;

			case SE_AntiGate:
				new_bonus->AntiGate = true;
				break;

			case SE_MagicWeapon:
				new_bonus->MagicWeapon = true;
				break;

			case SE_IncreaseBlockChance:
				new_bonus->IncreaseBlockChance += effect_value;
				break;

			case SE_LimitHPPercent:
			{
				if(new_bonus->HPPercCap[0] != 0 && new_bonus->HPPercCap[0] > effect_value){
					new_bonus->HPPercCap[0] = effect_value;
					new_bonus->HPPercCap[1] = base2;
				}
				else if(new_bonus->HPPercCap[0] == 0){
					new_bonus->HPPercCap[0] = effect_value;
					new_bonus->HPPercCap[1] = base2;
				}
				break;
			}
			case SE_LimitManaPercent:
			{
				if(new_bonus->ManaPercCap[0] != 0 && new_bonus->ManaPercCap[0] > effect_value){
					new_bonus->ManaPercCap[0] = effect_value;
					new_bonus->ManaPercCap[1] = base2;
				}
				else if(new_bonus->ManaPercCap[0] == 0) {
					new_bonus->ManaPercCap[0] = effect_value;
					new_bonus->ManaPercCap[1] = base2;
				}

				break;
			}
			case SE_LimitEndPercent:
			{
				if(new_bonus->EndPercCap[0] != 0 && new_bonus->EndPercCap[0] > effect_value) {
					new_bonus->EndPercCap[0] = effect_value;
					new_bonus->EndPercCap[1] = base2;
				}

				else if(new_bonus->EndPercCap[0] == 0){
					new_bonus->EndPercCap[0] = effect_value;
					new_bonus->EndPercCap[1] = base2;
				}

				break;
			}

			case SE_BlockNextSpellFocus:
				new_bonus->BlockNextSpell = true;
				break;

			case SE_ImmuneFleeing:
				new_bonus->ImmuneToFlee = true;
				break;

			case SE_CharmBreakChance:
				new_bonus->CharmBreakChance += effect_value;
				break;

			case SE_BardSongRange:
				new_bonus->SongRange += effect_value;
				break;

			case SE_HPToMana:
			{
				//Lower the ratio the more favorable
				if((!new_bonus->HPToManaConvert) || (new_bonus->HPToManaConvert >= effect_value))
				new_bonus->HPToManaConvert = spells[spell_id].base[i];
				break;
			}

			case SE_SkillDamageAmount2:
			{
				if(base2 == -1)
					new_bonus->SkillDamageAmount2[HIGHEST_SKILL+1] += effect_value;
				else
					new_bonus->SkillDamageAmount2[base2] += effect_value;
				break;
			}

			case SE_MitigateDotDamage:
			{
				if (new_bonus->MitigateDotRune[0] < effect_value){
					new_bonus->MitigateDotRune[0] = effect_value;
					new_bonus->MitigateDotRune[1] = buffslot;
					new_bonus->MitigateDotRune[2] = base2;
					new_bonus->MitigateDotRune[3] = max;
				}
				break;
			}

			case SE_ShieldBlock:
				new_bonus->ShieldBlock += effect_value;
				break;

			case SE_ShieldEquipHateMod:
				new_bonus->ShieldEquipHateMod += effect_value;
				break;

			case SE_ShieldEquipDmgMod:
				new_bonus->ShieldEquipDmgMod[0] += effect_value;
				new_bonus->ShieldEquipDmgMod[1] += base2;
				break;

			case SE_BlockBehind:
				new_bonus->BlockBehind += effect_value;
				break;

			case SE_Blind:
				new_bonus->IsBlind = true;
				break;

			case SE_Fear:
				new_bonus->IsFeared = true;
				break;

			//AA bonuses - implemented broadly into spell/item effects
			case SE_FrontalStunResist:
				new_bonus->FrontalStunResist += effect_value;
				break;

			case SE_ImprovedBindWound:
				new_bonus->BindWound += effect_value;
				break;

			case SE_MaxBindWound:
				new_bonus->MaxBindWound += effect_value;
				break;

			case SE_BaseMovementSpeed:
				new_bonus->BaseMovementSpeed += effect_value;
				break;

			case SE_IncreaseRunSpeedCap:
				new_bonus->IncreaseRunSpeedCap += effect_value;
				break;

			case SE_DoubleSpecialAttack:
				new_bonus->DoubleSpecialAttack += effect_value;
				break;

			case SE_FrontalBackstabMinDmg:
				new_bonus->FrontalBackstabMinDmg = true;
				break;

			case SE_ConsumeProjectile:
				new_bonus->ConsumeProjectile += effect_value;
				break;

			case SE_ForageAdditionalItems:
				new_bonus->ForageAdditionalItems += effect_value;
				break;

			case SE_Salvage:
				new_bonus->SalvageChance += effect_value;
				break;

			case SE_ArcheryDamageModifier:
				new_bonus->ArcheryDamageModifier += effect_value;
				break;

			case SE_DoubleRangedAttack:
				new_bonus->DoubleRangedAttack += effect_value;
				break;

			case SE_SecondaryDmgInc:
				new_bonus->SecondaryDmgInc = true;
				break;

			case SE_StrikeThrough:
			case SE_StrikeThrough2:
				new_bonus->StrikeThrough += effect_value;
				break;

			case SE_GiveDoubleAttack:
				new_bonus->GiveDoubleAttack += effect_value;
				break;

			case SE_CombatStability:
				new_bonus->CombatStability += effect_value;
				break;

			case SE_AddSingingMod:
				switch (base2)
				{
					case ItemTypeWindInstrument:
						new_bonus->windMod += effect_value;
						break;
					case ItemTypeStringedInstrument:
						new_bonus->stringedMod += effect_value;
						break;
					case ItemTypeBrassInstrument:
						new_bonus->brassMod += effect_value;
						break;
					case ItemTypePercussionInstrument:
						new_bonus->percussionMod += effect_value;
						break;
				}
				break;

			case SE_SongModCap:
				new_bonus->songModCap += effect_value;
				break;

			case SE_Ambidexterity:
				new_bonus->Ambidexterity += effect_value;
				break;

			case SE_PetMaxHP:
				new_bonus->PetMaxHP += effect_value;
				break;

			case SE_PetFlurry:
				new_bonus->PetFlurry += effect_value;
				break;

			case SE_GivePetGroupTarget:
				new_bonus->GivePetGroupTarget = true;
				break;

			case SE_RootBreakChance:
				new_bonus->RootBreakChance += effect_value;
				break;

			case SE_ChannelChanceItems:
				new_bonus->ChannelChanceItems += effect_value;
				break;

			case SE_ChannelChanceSpells:
				new_bonus->ChannelChanceSpells += effect_value;
				break;

			case SE_UnfailingDivinity:
				new_bonus->UnfailingDivinity += effect_value;
				break;


			case SE_ItemHPRegenCapIncrease:
				new_bonus->ItemHPRegenCap += effect_value;
				break;

			case SE_OffhandRiposteFail:
				new_bonus->OffhandRiposteFail += effect_value;
				break;

			case SE_ItemAttackCapIncrease:
				new_bonus->ItemATKCap += effect_value;
				break;

			case SE_TwoHandBluntBlock:
				new_bonus->TwoHandBluntBlock += effect_value;
				break;

			case SE_IncreaseChanceMemwipe:
				new_bonus->IncreaseChanceMemwipe += effect_value;
				break;

			case SE_CriticalMend:
				new_bonus->CriticalMend += effect_value;
				break;

			case SE_SpellEffectResistChance:
			{
				for(int e = 0; e < MAX_RESISTABLE_EFFECTS*2; e+=2)
				{
					if(new_bonus->SEResist[e+1] && (new_bonus->SEResist[e] == base2) && (new_bonus->SEResist[e+1] < effect_value)){
						new_bonus->SEResist[e] = base2; //Spell Effect ID
						new_bonus->SEResist[e+1] = effect_value; //Resist Chance
						break;
					}
					else if (!new_bonus->SEResist[e+1]){
						new_bonus->SEResist[e] = base2; //Spell Effect ID
						new_bonus->SEResist[e+1] = effect_value; //Resist Chance
						break;
					}
				}
				break;
			}

			case SE_MasteryofPast:
			{
				if(new_bonus->MasteryofPast < effect_value)
					new_bonus->MasteryofPast = effect_value;
				break;
			}

			case SE_DoubleRiposte:
			{
				new_bonus->DoubleRiposte += effect_value;
			}

			case SE_GiveDoubleRiposte:
			{
				//Only allow for regular double riposte chance.
				if(new_bonus->GiveDoubleRiposte[base2] == 0){
					if(new_bonus->GiveDoubleRiposte[0] < effect_value)
						new_bonus->GiveDoubleRiposte[0] = effect_value;
				}
				break;
			}

			case SE_SlayUndead:
			{
				if(new_bonus->SlayUndead[1] < effect_value)
					new_bonus->SlayUndead[0] = effect_value; // Rate
					new_bonus->SlayUndead[1] = base2; // Damage Modifier
				break;
			}

			case SE_TriggerOnReqTarget:
			case SE_TriggerOnReqCaster:
				new_bonus->TriggerOnValueAmount = true;
				break;

			case SE_DivineAura:
				new_bonus->DivineAura = true;
				break;

			case SE_ImprovedTaunt:
				if (new_bonus->ImprovedTaunt[0] < effect_value) {
					new_bonus->ImprovedTaunt[0] = effect_value;
					new_bonus->ImprovedTaunt[1] = base2;
					new_bonus->ImprovedTaunt[2] = buffslot;
				}
				break;


			case SE_DistanceRemoval:
				new_bonus->DistanceRemoval = true;
				break;

			case SE_Root:
				if (new_bonus->Root[0] && (new_bonus->Root[1] > buffslot)){
					new_bonus->Root[0] = 1;
					new_bonus->Root[1] = buffslot;
				}
				else if (!new_bonus->Root[0]){
					new_bonus->Root[0] = 1;
					new_bonus->Root[1] = buffslot;
				}
				break;

			case SE_Rune:

				if (new_bonus->MeleeRune[0] && (new_bonus->MeleeRune[1] > buffslot)){

					new_bonus->MeleeRune[0] = effect_value;
					new_bonus->MeleeRune[1] = buffslot;
				}
				else if (!new_bonus->MeleeRune[0]){
					new_bonus->MeleeRune[0] = effect_value;
					new_bonus->MeleeRune[1] = buffslot;
				}

				break;

			case SE_AbsorbMagicAtt:
				if (new_bonus->AbsorbMagicAtt[0] && (new_bonus->AbsorbMagicAtt[1] > buffslot)){
					new_bonus->AbsorbMagicAtt[0] = effect_value;
					new_bonus->AbsorbMagicAtt[1] = buffslot;
				}
				else if (!new_bonus->AbsorbMagicAtt[0]){
					new_bonus->AbsorbMagicAtt[0] = effect_value;
					new_bonus->AbsorbMagicAtt[1] = buffslot;
				}
				break;

			case SE_NegateIfCombat:
				new_bonus->NegateIfCombat = true;
				break;

			case SE_Screech: 
				new_bonus->Screech = effect_value;
				break;

			case SE_AlterNPCLevel:

				if (IsNPC()){
					if (!new_bonus->AlterNPCLevel 
					|| ((effect_value < 0) && (new_bonus->AlterNPCLevel > effect_value)) 
					|| ((effect_value > 0) && (new_bonus->AlterNPCLevel < effect_value))) {
	
						int tmp_lv =  GetOrigLevel() + effect_value;
						if (tmp_lv < 1)
							tmp_lv = 1;
						else if (tmp_lv > 255)
							tmp_lv = 255;
						if ((GetLevel() != tmp_lv)){
							new_bonus->AlterNPCLevel = effect_value;
							SetLevel(tmp_lv);
						}
					}
				}
				break;

			case SE_AStacker:
				new_bonus->AStacker[0] = 1;
				new_bonus->AStacker[1] = effect_value;
				break;

			case SE_BStacker:
				new_bonus->BStacker[0] = 1;
				new_bonus->BStacker[1] = effect_value;
				break;

			case SE_CStacker:
				new_bonus->CStacker[0] = 1;
				new_bonus->CStacker[1] = effect_value;
				break;

			case SE_DStacker:
				new_bonus->DStacker[0] = 1;
				new_bonus->DStacker[1] = effect_value;
				break;

			case SE_Berserk:
				new_bonus->BerserkSPA = true;
				break;

				
			case SE_Metabolism:
				new_bonus->Metabolism += effect_value;
				break;

			case SE_ImprovedReclaimEnergy:
			{
				if((effect_value < 0) && (new_bonus->ImprovedReclaimEnergy > effect_value))
					new_bonus->ImprovedReclaimEnergy = effect_value;

				else if(new_bonus->ImprovedReclaimEnergy < effect_value)
					new_bonus->ImprovedReclaimEnergy = effect_value;
				break;
			}

			case SE_HeadShot:
			{
				if(new_bonus->HeadShot[1] < base2){
					new_bonus->HeadShot[0] = effect_value;
					new_bonus->HeadShot[1] = base2;
				}
				break;
			}

			case SE_HeadShotLevel:
			{
				if(new_bonus->HSLevel < effect_value)
					new_bonus->HSLevel = effect_value;
				break;
			}

			case SE_Assassinate:
			{
				if(new_bonus->Assassinate[1] < base2){
					new_bonus->Assassinate[0] = effect_value;
					new_bonus->Assassinate[1] = base2;
				}
				break;
			}

			case SE_AssassinateLevel:
			{
				if(new_bonus->AssassinateLevel < effect_value)
					new_bonus->AssassinateLevel = effect_value;
				break;
			}

			case SE_FinishingBlow:
			{
				//base1 = chance, base2 = damage
				if (new_bonus->FinishingBlow[1] < base2){
					new_bonus->FinishingBlow[0] = effect_value;
					new_bonus->FinishingBlow[1] = base2;
				}
				break;
			}

			case SE_FinishingBlowLvl:
			{
				//base1 = level, base2 = ??? (Set to 200 in AA data, possible proc rate mod?)
				if (new_bonus->FinishingBlowLvl[0] < effect_value){
					new_bonus->FinishingBlowLvl[0] = effect_value;
					new_bonus->FinishingBlowLvl[1] = base2;
				}
				break;
			}

			case SE_MeleeVulnerability:
				new_bonus->MeleeVulnerability += effect_value;
				break;

			case SE_Sanctuary:
				new_bonus->Sanctuary = true;
				break;

			case SE_FactionModPct:
			{
				if((effect_value < 0) && (new_bonus->FactionModPct > effect_value))
					new_bonus->FactionModPct = effect_value;

				else if(new_bonus->FactionModPct < effect_value)
					new_bonus->FactionModPct = effect_value;
				break;
			}

			case SE_IllusionPersistence:
				new_bonus->IllusionPersistence = true;
				break;

			case SE_LimitToSkill:{
				if (effect_value <= HIGHEST_SKILL){
					new_bonus->LimitToSkill[effect_value] = true;
				}
				break;
			}

			//Special custom cases for loading effects on to NPC from 'npc_spels_effects' table
			if (IsAISpellEffect) {
				
				//Non-Focused Effect to modify incoming spell damage by resist type.
				case SE_FcSpellVulnerability: 
					ModVulnerability(base2, effect_value);
				break;
			}
		}
	}
}

void NPC::CalcItemBonuses(StatBonuses *newbon)
{
	if(newbon){

		for(int i = 0; i < EmuConstants::EQUIPMENT_SIZE; i++){
			const Item_Struct *cur = database.GetItem(equipment[i]);
			if(cur){
				//basic stats
				newbon->AC += cur->AC;
				newbon->HP += cur->HP;
				newbon->Mana += cur->Mana;
				newbon->STR += (cur->AStr);
				newbon->STA += (cur->ASta);
				newbon->DEX += (cur->ADex);
				newbon->AGI += (cur->AAgi);
				newbon->INT += (cur->AInt);
				newbon->WIS += (cur->AWis);
				newbon->CHA += (cur->ACha);
				newbon->MR += (cur->MR);
				newbon->FR += (cur->FR);
				newbon->CR += (cur->CR);
				newbon->PR += (cur->PR);
				newbon->DR += (cur->DR);

				//more complex stats
				if (cur->Worn.Effect>0 && (cur->Worn.Type == ET_WornEffect)) { // latent effects
					ApplySpellsBonuses(cur->Worn.Effect, GetLevel(), newbon);
				}
			}
		}

	}
}

void Client::CalcItemScale() {
	bool changed = false;

	// MainAmmo excluded in helper function below
	if(CalcItemScale(EmuConstants::EQUIPMENT_BEGIN, EmuConstants::EQUIPMENT_END)) // original coding excluded MainAmmo (< 21)
		changed = true;

	if(CalcItemScale(EmuConstants::GENERAL_BEGIN, EmuConstants::GENERAL_END)) // original coding excluded MainCursor (< 30)
		changed = true;

	// I excluded cursor bag slots here because cursor was excluded above..if this is incorrect, change 'slot_y' here to CURSOR_BAG_END
	// and 'slot_y' above to CURSOR from GENERAL_END above - or however it is supposed to be...
	if (CalcItemScale(EmuConstants::GENERAL_BAGS_BEGIN, EmuConstants::CURSOR_BAG_END)) // (< 341)
		changed = true;

	if(changed)
	{
		CalcBonuses();
	}
}

bool Client::CalcItemScale(uint32 slot_x, uint32 slot_y) {
	// behavior change: 'slot_y' is now [RANGE]_END and not [RANGE]_END + 1
	bool changed = false;
	uint32 i;
	for (i = slot_x; i <= slot_y; i++) {
		if (i == MainAmmo) // moved here from calling procedure to facilitate future range changes where MainAmmo may not be the last slot
			continue;

		ItemInst* inst = m_inv.GetItem(i);

		if(inst == nullptr)
			continue;

		// TEST CODE: test for bazaar trader crashing with charm items
		if (Trader)
			if (i >= EmuConstants::GENERAL_BAGS_BEGIN && i <= EmuConstants::GENERAL_BAGS_END) {
				ItemInst* parent_item = m_inv.GetItem(Inventory::CalcSlotId(i));
				if (parent_item && parent_item->GetItem()->ID == 17899) // trader satchel
					continue;
			}

		bool update_slot = false;
		if(inst->IsScaling())
		{
			uint16 oldexp = inst->GetExp();
			parse->EventItem(EVENT_SCALE_CALC, this, inst, nullptr, "", 0);

			if (inst->GetExp() != oldexp) {	// if the scaling factor changed, rescale the item and update the client
				inst->ScaleItem();
				changed = true;
				update_slot = true;
			}
		}

		if(update_slot)
		{
			SendItemPacket(i, inst, ItemPacketCharmUpdate);
		}
	}
	return changed;
}

void Client::DoItemEnterZone() {
	bool changed = false;

	// MainAmmo excluded in helper function below
	if(DoItemEnterZone(EmuConstants::EQUIPMENT_BEGIN, EmuConstants::EQUIPMENT_END)) // original coding excluded MainAmmo (< 21)
		changed = true;

	if(DoItemEnterZone(EmuConstants::GENERAL_BEGIN, EmuConstants::GENERAL_END)) // original coding excluded MainCursor (< 30)
		changed = true;

	// I excluded cursor bag slots here because cursor was excluded above..if this is incorrect, change 'slot_y' here to CURSOR_BAG_END
	// and 'slot_y' above to CURSOR from GENERAL_END above - or however it is supposed to be...
	if (DoItemEnterZone(EmuConstants::GENERAL_BAGS_BEGIN, EmuConstants::CURSOR_BAG_END)) // (< 341)
		changed = true;

	if(changed)
	{
		CalcBonuses();
	}
}

bool Client::DoItemEnterZone(uint32 slot_x, uint32 slot_y) {
	// behavior change: 'slot_y' is now [RANGE]_END and not [RANGE]_END + 1
	bool changed = false;
	for(uint32 i = slot_x; i <= slot_y; i++) {
		if (i == MainAmmo) // moved here from calling procedure to facilitate future range changes where MainAmmo may not be the last slot
			continue;

		ItemInst* inst = m_inv.GetItem(i);

		if(!inst)
			continue;

		// TEST CODE: test for bazaar trader crashing with charm items
		if (Trader)
			if (i >= EmuConstants::GENERAL_BAGS_BEGIN && i <= EmuConstants::GENERAL_BAGS_END) {
				ItemInst* parent_item = m_inv.GetItem(Inventory::CalcSlotId(i));
				if (parent_item && parent_item->GetItem()->ID == 17899) // trader satchel
					continue;
			}

		bool update_slot = false;
		if(inst->IsScaling())
		{
			uint16 oldexp = inst->GetExp();

			parse->EventItem(EVENT_ITEM_ENTER_ZONE, this, inst, nullptr, "", 0);
			if(i <= MainAmmo || i == MainPowerSource) {
				parse->EventItem(EVENT_EQUIP_ITEM, this, inst, nullptr, "", i);
			}

			if (inst->GetExp() != oldexp) {	// if the scaling factor changed, rescale the item and update the client
				inst->ScaleItem();
				changed = true;
				update_slot = true;
			}
		} else {
			if(i <= MainAmmo || i == MainPowerSource) {
				parse->EventItem(EVENT_EQUIP_ITEM, this, inst, nullptr, "", i);
			}

			parse->EventItem(EVENT_ITEM_ENTER_ZONE, this, inst, nullptr, "", 0);
		}

		if(update_slot)
		{
			SendItemPacket(i, inst, ItemPacketCharmUpdate);
		}
	}
	return changed;
}

uint8 Mob::IsFocusEffect(uint16 spell_id,int effect_index, bool AA,uint32 aa_effect)
{
	uint16 effect = 0;

	if (!AA)
		effect = spells[spell_id].effectid[effect_index];
	else
		effect = aa_effect;

	switch (effect)
	{
		case SE_ImprovedDamage:
			return focusImprovedDamage;
		case SE_ImprovedHeal:
			return focusImprovedHeal;
		case SE_ReduceManaCost:
			return focusManaCost;
		case SE_IncreaseSpellHaste:
			return focusSpellHaste;
		case SE_IncreaseSpellDuration:
			return focusSpellDuration;
		case SE_SpellDurationIncByTic:
			return focusSpellDurByTic;
		case SE_SwarmPetDuration:
			return focusSwarmPetDuration;
		case SE_IncreaseRange:
			return focusRange;
		case SE_ReduceReagentCost:
			return focusReagentCost;
		case SE_PetPowerIncrease:
			return focusPetPower;
		case SE_SpellResistReduction:
			return focusResistRate;
		case SE_SpellHateMod:
			return focusSpellHateMod;
		case SE_ReduceReuseTimer:
			return focusReduceRecastTime;
		case SE_TriggerOnCast:
			//return focusTriggerOnCast;
			return 0; //This is calculated as an actual bonus
		case SE_FcSpellVulnerability:
			return focusSpellVulnerability;
		case SE_BlockNextSpellFocus:
			//return focusBlockNextSpell;
			return 0; //This is calculated as an actual bonus
		case SE_FcTwincast:
			return focusTwincast;
		case SE_SympatheticProc:
			return focusSympatheticProc;
		case SE_FcDamageAmt:
			return focusFcDamageAmt;
		case SE_FcDamageAmtCrit:
			return focusFcDamageAmtCrit;
		case SE_FcDamagePctCrit:
			return focusFcDamagePctCrit;
		case SE_FcDamageAmtIncoming:
			return focusFcDamageAmtIncoming;
		case SE_FcHealAmtIncoming:
			return focusFcHealAmtIncoming;
		case SE_FcHealPctIncoming:
			return focusFcHealPctIncoming;
		case SE_FcBaseEffects:
			return focusFcBaseEffects;
		case SE_FcIncreaseNumHits:
			return focusIncreaseNumHits;
		case SE_FcLimitUse:
			return focusFcLimitUse;
		case SE_FcMute:
			return focusFcMute;
		case SE_FcTimerRefresh:
			return focusFcTimerRefresh;
		case SE_FcStunTimeMod:
			return focusFcStunTimeMod;
		case SE_FcHealPctCritIncoming:
			return focusFcHealPctCritIncoming;
		case  SE_FcHealAmt:
			return focusFcHealAmt;
		case SE_FcHealAmtCrit:
			return focusFcHealAmtCrit;
	}
	return 0;
}
