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

#include "../common/rulesys.h"
#include "../common/string_util.h"

#include "client.h"
#include "entity.h"
#include "mob.h"
#include "string_ids.h"

#include <string.h>


int Mob::GetKickDamage() {

	int32 dmg = 0;

	if(IsClient())
	{
		int multiple=(GetLevel()*100/5)+100;
		dmg=(((GetSkill(SkillKick) + GetSTR() + GetLevel())*100 / 10000) * multiple);
		dmg /= 100;
	}
	else if(IsNPC())
	{
		return CastToNPC()->GetMaxBashKickDmg();
	}

	if(GetClass() == WARRIOR || GetClass() == WARRIORGM) {
		dmg*=12/10;//small increase for warriors
	}

	if(dmg == 0)
		dmg = 1;

	int32 mindmg = 1;
	ApplySpecialAttackMod(SkillKick, dmg,mindmg);

	dmg = mod_kick_damage(dmg);

	return(dmg);
}

int Mob::GetBashDamage() {

	int32 dmg = 0;

	if(IsClient())
	{
		int multiple=(GetLevel()*100/5)+100;

		if(!HasShieldEquiped())
		{
			//Slam
			if(GetRace() == OGRE || GetRace() == TROLL || GetRace() == BARBARIAN)
			{
				dmg=(((GetSkill(SkillBash)/2 + GetSTR()/2 + GetLevel()/2)*100 / 11000) * multiple);
				dmg /= 100;
			}
		}
		else
		{
			//Bash
			dmg=(((GetSkill(SkillBash) + GetSTR() + GetLevel())*100 / 11000) * multiple);
			dmg /= 100;
		}
	}
	else if(IsNPC())
	{
		return CastToNPC()->GetMaxBashKickDmg();
	}

	if(dmg == 0)
		dmg = 1;

	int32 mindmg = 1;
	ApplySpecialAttackMod(SkillBash, dmg, mindmg);

	dmg = mod_bash_damage(dmg);

	return(dmg);
}

int32 NPC::GetMinBashKickDmg()
{
	int16 mindmg = GetMinDMG();
	int16 maxdmg = GetMaxDMG();
	int32 dmg;

	if (mindmg >= maxdmg)
		dmg = mindmg;
	else
		dmg = mindmg - (maxdmg - mindmg) / 19;

	if (dmg < 1)
		dmg = 1;

	return dmg;
}

int32 NPC::GetMaxBashKickDmg()
{
	if (GetLevel() >= 15 && GetLevel() <= 24)
		return GetMinBashKickDmg() + 7;
	else if (GetLevel() >= 25 && GetLevel() <= 34)
		return GetMinBashKickDmg() + 9;
	else if (GetLevel() >= 35)
		return GetMinBashKickDmg() + 11;
	else
		return GetMinBashKickDmg() + 5;
}

void Mob::ApplySpecialAttackMod(SkillUseTypes skill, int32 &dmg, int32 &mindmg) {
	int item_slot = -1;
	//1: Apply bonus from AC (BOOT/SHIELD/HANDS) est. 40AC=6dmg
	switch (skill){

		case SkillFlyingKick:
		case SkillRoundKick:
		case SkillKick:
			item_slot = MainFeet;
			break;

		case SkillBash:
			item_slot = MainSecondary;
			break;

		case SkillDragonPunch:
		case SkillEagleStrike:
		case SkillTigerClaw:
			item_slot = MainHands;
			break;

		default:
			break;
	}
	if (IsClient()){
		if (item_slot >= 0){
			const ItemInst* itm = nullptr;
			itm = CastToClient()->GetInv().GetItem(item_slot);
			if(itm)
				dmg += itm->GetItem()->AC * (RuleI(Combat, SpecialAttackACBonus))/100;
		}
	}
	else if(IsNPC()){
		if (item_slot >= 0){
			ServerLootItem_Struct* item = CastToNPC()->GetItem(item_slot);
			if(item)
			{
				const ItemInst* itm  = database.CreateItem(item->item_id);
				dmg += itm->GetItem()->AC * (RuleI(Combat, SpecialAttackACBonus))/100;
			}
		}

	}
}

void Mob::DoSpecialAttackDamage(Mob *who, SkillUseTypes skill, int32 max_damage, int32 min_damage, int32 hate_override,int ReuseTime, 
								bool HitChance, bool CanAvoid) {
	//this really should go through the same code as normal melee damage to
	//pick up all the special behavior there

	if (!who)
		return;

	int32 hate = max_damage;
	if(hate_override > -1)
		hate = hate_override;

	if(skill == SkillBash){
		if(IsClient()){
			ItemInst *item = CastToClient()->GetInv().GetItem(MainSecondary);
			if(item)
			{
				if(item->GetItem()->ItemType == ItemTypeShield)
				{
					hate += item->GetItem()->AC;
				}
				const Item_Struct *itm = item->GetItem();
				hate = hate * (100 + GetFuriousBash(itm->Focus.Effect)) / 100;
			}
		}
	}

	int32 damage = max_damage;

	bool noRiposte = false;
	if (skill == SkillThrowing || skill == SkillArchery)
	{
		noRiposte = true;
	}

	if (damage > 0 && CanAvoid)
	{
		who->AvoidDamage(this, damage, noRiposte);
	}

	if (damage > 0 && HitChance && !who->AvoidanceCheck(this, skill))
	{
		damage = 0;
	}

	if (damage > 0)
	{
		uint8 roll = RollD20(GetOffense(skill), who->GetMitigation());
		uint32 di1k = 1;

		// SE_MinDamageModifier for disciplines: Fellstrike, Innerflame, Duelist, Beastial Rage
		int32 minDmgModDmg = min_damage * GetMeleeMinDamageMod_SE(skill) / 100;
		if (min_damage < minDmgModDmg) min_damage = minDmgModDmg;

		// SE_DamageModifier for disciplines: Silentfist, Ashenhand, Thunderkick
		max_damage += max_damage * GetMeleeDamageMod_SE(skill) / 100;

		if (max_damage <= min_damage)
		{
			max_damage = min_damage;
			roll = 20;
		}
		else
		{
			di1k = (max_damage - min_damage) * 1000 / 19;			// multiply damage interval by 1000 so truncation doesn't reduce accuracy
		}

		if (roll == 20)
		{
			damage = max_damage;
		}
		else
		{
			damage = (di1k * roll + (min_damage * 1000 - di1k)) / 1000;
		}

		if (damage < 1)
		{
			damage = 1;
		}

		CommonOutgoingHitSuccess(who, damage, skill);
	}

	who->AddToHateList(this, hate, 0);
	who->Damage(this, damage, SPELL_UNKNOWN, skill, false);

	//Make sure 'this' has not killed the target and 'this' is not dead (Damage shield ect).
	if(!GetTarget())return;
	if (HasDied())	return;

	//[AA Dragon Punch] value[0] = 100 for 25%, chance value[1] = skill
	if(aabonuses.SpecialAttackKBProc[0] && aabonuses.SpecialAttackKBProc[1] == skill){
		int kb_chance = 25;
		kb_chance += kb_chance*(100-aabonuses.SpecialAttackKBProc[0])/100;

		if (zone->random.Roll(kb_chance))
			SpellFinished(904, who, 10, 0, -1, spells[904].ResistDiff);
			//who->Stun(100); Kayen: This effect does not stun on live, it only moves the NPC.
	}

	if (damage > 0 && IsNPC())
	{
		TrySpellProc(nullptr, nullptr, who);		// NPC innate procs can proc on special attacks
	}

	if(damage == -3 && !who->HasDied())
		DoRiposte(who);
}

bool Client::HasRacialAbility(const CombatAbility_Struct* ca_atk)
{
	if (ca_atk->m_atk == 100 && ca_atk->m_skill == SkillBash){// SLAM - Bash without a shield equipped

		switch (GetRace())
		{
		case OGRE:
		case TROLL:
		case BARBARIAN:
			return  true;
		default:
			break;
		}

	}
	return false;
}

void Client::OPCombatAbility(const EQApplicationPacket *app) {
	if(!GetTarget())
		return;
	//make sure were actually able to use such an attack.
	if(spellend_timer.Enabled() || IsFeared() || IsStunned() || IsMezzed() || DivineAura() || dead)
		return;

	CombatAbility_Struct* ca_atk = (CombatAbility_Struct*) app->pBuffer;

	/* Check to see if actually have skill or innate racial ability (like Ogres have Slam) */
	if (MaxSkill(static_cast<SkillUseTypes>(ca_atk->m_skill)) <= 0 && !HasRacialAbility(ca_atk))
		return;

	if(GetTarget()->GetID() != ca_atk->m_target)
		return;	//invalid packet.

	if(!IsAttackAllowed(GetTarget()))
		return;

	//These two are not subject to the combat ability timer, as they
	//allready do their checking in conjunction with the attack timer
	//throwing weapons
	if(ca_atk->m_atk == MainRange) {
		if (ca_atk->m_skill == SkillThrowing) {
			SetAttackTimer();
			ThrowingAttack(GetTarget());
			if (CheckDoubleRangedAttack())
				RangedAttack(GetTarget(), true);
			return;
		}
		//ranged attack (archery)
		if (ca_atk->m_skill == SkillArchery) {
			SetAttackTimer();
			RangedAttack(GetTarget());
			if (CheckDoubleRangedAttack())
				RangedAttack(GetTarget(), true);
			return;
		}
		//could we return here? Im not sure is m_atk 11 is used for real specials
	}

	//check range for all these abilities, they are all close combat stuff
	if(!CombatRange(GetTarget()))
		return;

	if(!p_timers.Expired(&database, pTimerCombatAbility, false)) {
		Message(CC_Red,"Ability recovery time not yet met.");
		return;
	}

	int ReuseTime = 0;
	int ClientHaste = GetHaste();
	int HasteMod = 0;

	if(ClientHaste >= 0){
		HasteMod = (10000/(100+ClientHaste)); //+100% haste = 2x as many attacks
	}
	else{
		HasteMod = (100-ClientHaste); //-100% haste = 1/2 as many attacks
	}
	int32 dmg = 0;

	int32 skill_reduction = this->GetSkillReuseTime(ca_atk->m_skill);

	// not sure what the '100' indicates..if ->m_atk is not used as 'slot' reference, then change MainRange above back to '11'
	if ((ca_atk->m_atk == 100) && (ca_atk->m_skill == SkillBash)) { // SLAM - Bash without a shield equipped
		if (GetTarget() != this) {

			CheckIncreaseSkill(SkillBash, GetTarget(), zone->skill_difficulty[SkillBash].difficulty);
			DoAnim(Animation::Slam);

			dmg = GetBashDamage();

			ReuseTime = BashReuseTime-1-skill_reduction;
			ReuseTime = (ReuseTime*HasteMod)/100;
			DoSpecialAttackDamage(GetTarget(), SkillBash, dmg, 1, dmg, ReuseTime, true);
			if(ReuseTime > 0)
			{
				p_timers.Start(pTimerCombatAbility, ReuseTime);
			}
		}
		return;
	}

	if ((ca_atk->m_atk == 100) && (ca_atk->m_skill == SkillFrenzy)){
		CheckIncreaseSkill(SkillFrenzy, GetTarget(), zone->skill_difficulty[SkillFrenzy].difficulty);
		int AtkRounds = 3;
		int skillmod = 100*GetSkill(SkillFrenzy)/MaxSkill(SkillFrenzy);
		int32 max_dmg = (26 + ((((GetLevel()-6) * 2)*skillmod)/100)) * ((100+RuleI(Combat, FrenzyBonus))/100);
		int32 min_dmg = 0;
		DoAnim(Animation::Slashing2H);

		max_dmg = mod_frenzy_damage(max_dmg);

		if (GetLevel() < 51)
			min_dmg = 1;
		else
			min_dmg = GetLevel()*8/10;

		if (min_dmg > max_dmg)
			max_dmg = min_dmg;

		ReuseTime = FrenzyReuseTime-1-skill_reduction;
		ReuseTime = (ReuseTime*HasteMod)/100;

		//Live parses show around 55% Triple 35% Double 10% Single, you will always get first hit.
		while(AtkRounds > 0) {
			if (GetTarget() && (AtkRounds == 1 || zone->random.Roll(75))) {
				DoSpecialAttackDamage(GetTarget(), SkillFrenzy, max_dmg, min_dmg, max_dmg , ReuseTime, true);
			}
			AtkRounds--;
		}

		if(ReuseTime > 0) {
			p_timers.Start(pTimerCombatAbility, ReuseTime);
		}
		return;
	}

	switch(GetClass()){
		case WARRIOR:
		case RANGER:
		case BEASTLORD:
			if (ca_atk->m_atk != 100 || ca_atk->m_skill != SkillKick) {
				break;
			}
			if (GetTarget() != this) {
				CheckIncreaseSkill(SkillKick, GetTarget(), zone->skill_difficulty[SkillKick].difficulty);
				DoAnim(Animation::Kick);

				int32 hate = GetKickDamage();
				if(GetWeaponDamage(GetTarget(), GetInv().GetItem(MainFeet)) <= 0){
					dmg = -5;
				}
				else
				{
					dmg = hate;
				}

				ReuseTime = KickReuseTime-1-skill_reduction;
				DoSpecialAttackDamage(GetTarget(), SkillKick, dmg, 1, hate, ReuseTime, true);
			}
			break;
		case MONK: {
			ReuseTime = MonkSpecialAttack(GetTarget(), ca_atk->m_skill) - 1 - skill_reduction;

			//Live AA - Technique of Master Wu
			int wuchance = itembonuses.DoubleSpecialAttack + spellbonuses.DoubleSpecialAttack + aabonuses.DoubleSpecialAttack;
			if (wuchance) {
				if (wuchance >= 100 || zone->random.Roll(wuchance)) {
					int MonkSPA [5] = { SkillFlyingKick, SkillDragonPunch, SkillEagleStrike, SkillTigerClaw, SkillRoundKick };
					int extra = 1;
					// always 1/4 of the double attack chance, 25% at rank 5 (100/4)
					if (zone->random.Roll(wuchance / 4))
						extra++;
					// They didn't add a string ID for this.
					std::string msg = StringFormat("The spirit of Master Wu fills you!  You gain %d additional attack(s).", extra);
					// live uses 400 here -- not sure if it's the best for all clients though
					while (extra) {
						MonkSpecialAttack(GetTarget(), MonkSPA[zone->random.Int(0, 4)]);
						extra--;
					}
				}
			}

			if(ReuseTime < 100) {
				//hackish... but we return a huge reuse time if this is an
				// invalid skill, otherwise, we can safely assume it is a
				// valid monk skill and just cast it to a SkillType
				CheckIncreaseSkill((SkillUseTypes) ca_atk->m_skill, GetTarget(), zone->skill_difficulty[ca_atk->m_skill].difficulty);
			}
			break;
		}
		case ROGUE: {
			if (ca_atk->m_atk != 100 || ca_atk->m_skill != SkillBackstab) {
				break;
			}
			ReuseTime = BackstabReuseTime-1 - skill_reduction;
			TryBackstab(GetTarget(), ReuseTime);
			break;
		}
		default:
			//they have no abilities... wtf? make em wait a bit
			ReuseTime = 9 - skill_reduction;
			break;
	}

	ReuseTime = (ReuseTime*HasteMod)/100;
	if(ReuseTime > 0){
		p_timers.Start(pTimerCombatAbility, ReuseTime);
	}
}

//returns the reuse time in sec for the special attack used.
int Mob::MonkSpecialAttack(Mob* other, uint8 unchecked_type)
{
	if(!other)
		return 0;

	int32 ndamage = 0;
	int32 max_dmg = 0;
	int32 min_dmg = 1;
	int reuse = 0;
	SkillUseTypes skill_type;	//to avoid casting... even though it "would work"
	uint8 itemslot = MainFeet;

	switch(unchecked_type){
		case SkillFlyingKick:{
			skill_type = SkillFlyingKick;
			max_dmg = ((GetSTR()+GetSkill(skill_type)) * RuleI(Combat, FlyingKickBonus) / 100) + 35;
			min_dmg = ((level*8)/10);
			ApplySpecialAttackMod(skill_type, max_dmg, min_dmg);
			DoAnim(Animation::FlyingKick);
			reuse = FlyingKickReuseTime;
			break;
		}
		case SkillDragonPunch:{
			skill_type = SkillDragonPunch;
			max_dmg = ((GetSTR()+GetSkill(skill_type)) * RuleI(Combat, DragonPunchBonus) / 100) + 26;
			itemslot = MainHands;
			ApplySpecialAttackMod(skill_type, max_dmg, min_dmg);
			DoAnim(Animation::Slam);
			reuse = TailRakeReuseTime;
			break;
		}

		case SkillEagleStrike:{
			skill_type = SkillEagleStrike;
			max_dmg = ((GetSTR()+GetSkill(skill_type)) * RuleI(Combat, EagleStrikeBonus) / 100) + 19;
			itemslot = MainHands;
			ApplySpecialAttackMod(skill_type, max_dmg, min_dmg);
			DoAnim(Animation::EagleStrike);
			reuse = EagleStrikeReuseTime;
			break;
		}

		case SkillTigerClaw:{
			skill_type = SkillTigerClaw;
			max_dmg = ((GetSTR()+GetSkill(skill_type)) * RuleI(Combat, TigerClawBonus) / 100) + 12;
			itemslot = MainHands;
			ApplySpecialAttackMod(skill_type, max_dmg, min_dmg);
			DoAnim(Animation::TigerClaw);
			reuse = TigerClawReuseTime;
			break;
		}

		case SkillRoundKick:{
			skill_type = SkillRoundKick;
			max_dmg = ((GetSTR()+GetSkill(skill_type)) * RuleI(Combat, RoundKickBonus) / 100) + 10;
			ApplySpecialAttackMod(skill_type, max_dmg, min_dmg);
			DoAnim(Animation::RoundKick);
			reuse = RoundKickReuseTime;
			break;
		}

		case SkillKick:{
			skill_type = SkillKick;
			max_dmg = GetKickDamage();
			DoAnim(Animation::Kick);
			reuse = KickReuseTime;
			break;
		}
		default:
			Log.Out(Logs::Detail, Logs::Attack, "Invalid special attack type %d attempted", unchecked_type);
			return(1000); /* nice long delay for them, the caller depends on this! */
	}

	if(IsClient()){
		if(GetWeaponDamage(other, CastToClient()->GetInv().GetItem(itemslot)) <= 0){
			ndamage = -5;
		}
	}
	else{
		if(GetWeaponDamage(other, (const Item_Struct*)nullptr) <= 0){
			ndamage = -5;
		}
	}

	if(ndamage == 0)
	{
		ndamage = max_dmg;
	}

	//This can potentially stack with changes to kick damage
	ndamage = mod_monk_special_damage(ndamage, skill_type);

	DoSpecialAttackDamage(other, skill_type, ndamage, min_dmg, max_dmg, reuse, true);

	if(IsClient() && CastToClient()->HasInstantDisc(skill_type))
	{
		CastToClient()->FadeDisc();
	}

	return(reuse);
}

void Mob::TryBackstab(Mob *other, int ReuseTime) {
	if(!other)
		return;

	bool bIsBehind = false;

	//make sure we have a proper weapon if we are a client.
	if(IsClient()) {
		const ItemInst *wpn = CastToClient()->GetInv().GetItem(MainPrimary);
		if(!wpn || (wpn->GetItem()->ItemType != ItemType1HPiercing)){
			Message_StringID(CC_Red, BACKSTAB_WEAPON);
			return;
		}
	}

	if (BehindMob(other, GetX(), GetY()))
		bIsBehind = true;

	if (bIsBehind)
	{
		RogueBackstab(other,false,ReuseTime);
		if (level > 54) {
			if(IsClient() && CastToClient()->CheckDoubleAttack(false))
			{
				if(other->GetHP() > 0)
					RogueBackstab(other,false,ReuseTime);
			}
		}

		if(IsClient())
			CastToClient()->CheckIncreaseSkill(SkillBackstab, other, zone->skill_difficulty[SkillBackstab].difficulty);

	}
	//Luclin AA - Chaotic Backstab
	else if(aabonuses.FrontalBackstabMinDmg || itembonuses.FrontalBackstabMinDmg || spellbonuses.FrontalBackstabMinDmg)
	{
		//we can stab from any angle, we do min damage though.
		RogueBackstab(other, true, ReuseTime);
		if (level > 54)
		{
			// Check for double attack with main hand assuming maxed DA Skill (MS)
			if(IsClient() && CastToClient()->CheckDoubleAttack(false))	
				if(other->GetHP() > 0)
					RogueBackstab(other,true, ReuseTime);
		}

		if(IsClient())
			CastToClient()->CheckIncreaseSkill(SkillBackstab, other, zone->skill_difficulty[SkillBackstab].difficulty);
	}
	else { //We do a single regular attack if we attack from the front without chaotic stab
		Attack(other, MainPrimary);
	}
}

void Mob::RogueBackstab(Mob* defender, bool doMinDmg, int ReuseTime)
{
	if (!defender)
		return;

	int32 damage = 1;
	int32 max_hit = 0;
	int32 min_hit = 0;

	if (IsNPC())
	{
		// this is wrong, but what this replaced was also wrong
		min_hit = CastToNPC()->GetMinDMG() + GetLevel();
		max_hit = GetLevel() * 5 + min_hit;
		DoSpecialAttackDamage(defender, SkillBackstab, max_hit, min_hit, max_hit / 2, 0, true);
		DoAnim(Animation::Piercing);

		return;
	}

	if (!IsClient()) return;

	int32 hate = 0;
	int32 primaryweapondamage = 0;
	int32 backstab_dmg = 0;
	int32 base;
	uint16 skillLevel = GetSkill(SkillBackstab);

	const ItemInst *wpn = nullptr;
	wpn = CastToClient()->GetInv().GetItem(MainPrimary);
	if(wpn)
	{
		primaryweapondamage = GetWeaponDamage(defender, wpn);
		backstab_dmg = wpn->GetItem()->Damage;
	}

	if (skillLevel > 0)
	{
		base = backstab_dmg * ((skillLevel * 100 / 50) + 200) / 100;
	}
	else
	{
		base = backstab_dmg * 2;
	}

	if (primaryweapondamage <= 0)
	{
		damage = -5;
	}
	else
	{
		defender->AvoidDamage(this, damage);

		if (damage > 0 && !defender->AvoidanceCheck(this, SkillBackstab))
		{
			damage = 0;
		}

		if (damage > 0)
		{
			if (GetMeleeMinDamageMod_SE(SkillBackstab))
			{
				// duelist min damage is non-disc base * 4.  388 damage with epic at 60
				min_hit = base * GetMeleeMinDamageMod_SE(SkillBackstab) / 100;
			}
			else if (level >= 60)
			{
				min_hit = level * 2;
			}
			else if (level > 50)
			{
				min_hit = level * 3 / 2;
			}
			base += base * GetMeleeDamageMod_SE(SkillBackstab) / 100;		// duelist discipline

			int32 offense = GetOffense(SkillBackstab);
			int8 roll = RollD20(offense, defender->GetMitigation());

			damage = base + (base * roll + 10) / 20;									// +10 is to round and make the numbers slightly more accurate
			damage = damage * CastToClient()->RollDamageMultiplier(offense) / 100;		// client only damage multiplier

			if (doMinDmg || damage < min_hit)
			{
				damage = min_hit;
			}
		}
		damage = mod_backstab_damage(damage);

		uint32 assassinateDmg = 0;
		assassinateDmg = TryAssassinate(defender, SkillBackstab, ReuseTime);

		if (assassinateDmg) {
			damage = assassinateDmg;
			entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, ASSASSINATES, GetName());
		}
	}

	TryCriticalHit(defender, SkillBackstab, damage);
	CheckNumHitsRemaining(NUMHIT_OutgoingHitSuccess);
	DoAnim(Animation::Piercing);

	hate = base;		// might not be correct
	defender->AddToHateList(this, hate, 0);
	defender->Damage(this, damage, SPELL_UNKNOWN, SkillBackstab, false);

	//Make sure 'this' has not killed the target and 'this' is not dead (Damage shield ect).
	if (!GetTarget()) return;
	if (HasDied()) return;

	if (!defender->HasDied())
	{
		if (damage == -3)
		{
			DoRiposte(defender);
		}
		else
		{
			TrySpellProc(nullptr, nullptr, defender);		// can client backstabs proc wepaons?
		}
	}
}

// solar - assassinate [Kayen: No longer used for regular assassinate 6-29-14]
void Mob::RogueAssassinate(Mob* other)
{
	//can you dodge, parry, etc.. an assassinate??
	//if so, use DoSpecialAttackDamage(other, BACKSTAB, 32000); instead
	if(GetWeaponDamage(other, IsClient()?CastToClient()->GetInv().GetItem(MainPrimary):(const ItemInst*)nullptr) > 0){
		other->Damage(this, 32000, SPELL_UNKNOWN, SkillBackstab);
	}else{
		other->Damage(this, -5, SPELL_UNKNOWN, SkillBackstab);
	}
	DoAnim(Animation::Piercing);	//piercing animation
}

void Client::RangedAttack(Mob* other, bool CanDoubleAttack) {
	//conditions to use an attack checked before we are called

	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if(!CanDoubleAttack && ((attack_timer.Enabled() && !attack_timer.Check(false)) || (ranged_timer.Enabled() && !ranged_timer.Check()))) {
		Log.Out(Logs::Detail, Logs::Combat, "Throwing attack canceled. Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		// The server and client timers are not exact matches currently, so this would spam too often if enabled
		//Message(CC_Default, "Error: Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return;
	}
	const ItemInst* RangeWeapon = m_inv[MainRange];

	//locate ammo
	int ammo_slot = MainAmmo;
	const ItemInst* Ammo = m_inv[MainAmmo];

	if (!RangeWeapon || !RangeWeapon->IsType(ItemClassCommon)) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack canceled. Missing or invalid ranged weapon (%d) in slot %d", GetItemIDAt(MainRange), MainRange);
		Message(CC_Default, "Error: Rangeweapon: GetItem(%i)==0, you have no bow!", GetItemIDAt(MainRange));
		return;
	}
	if (!Ammo || !Ammo->IsType(ItemClassCommon)) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack canceled. Missing or invalid ammo item (%d) in slot %d", GetItemIDAt(MainAmmo), MainAmmo);
		Message(CC_Default, "Error: Ammo: GetItem(%i)==0, you have no ammo!", GetItemIDAt(MainAmmo));
		return;
	}

	const Item_Struct* RangeItem = RangeWeapon->GetItem();
	const Item_Struct* AmmoItem = Ammo->GetItem();

	if(RangeItem->ItemType != ItemTypeBow) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ranged item is not a bow. type %d.", RangeItem->ItemType);
		Message(CC_Default, "Error: Rangeweapon: Item %d is not a bow.", RangeWeapon->GetID());
		return;
	}
	if(AmmoItem->ItemType != ItemTypeArrow) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ammo item is not an arrow. type %d.", AmmoItem->ItemType);
		Message(CC_Default, "Error: Ammo: type %d != %d, you have the wrong type of ammo!", AmmoItem->ItemType, ItemTypeArrow);
		return;
	}

	Log.Out(Logs::Detail, Logs::Combat, "Shooting %s with bow %s (%d) and arrow %s (%d)", GetTarget()->GetName(), RangeItem->Name, RangeItem->ID, AmmoItem->Name, AmmoItem->ID);

	//look for ammo in inventory if we only have 1 left...
	if(Ammo->GetCharges() == 1) {
		//first look for quivers
		int r;
		bool found = false;
		for(r = EmuConstants::GENERAL_BEGIN; r <= EmuConstants::GENERAL_END; r++) {
			const ItemInst *pi = m_inv[r];
			if(pi == nullptr || !pi->IsType(ItemClassContainer))
				continue;
			const Item_Struct* bagitem = pi->GetItem();
			if(!bagitem || bagitem->BagType != BagTypeQuiver)
				continue;

			//we found a quiver, look for the ammo in it
			int i;
			for (i = 0; i < bagitem->BagSlots; i++) {
				ItemInst* baginst = pi->GetItem(i);
				if(!baginst)
					continue;	//empty
				if(baginst->GetID() == Ammo->GetID()) {
					//we found it... use this stack
					//the item wont change, but the instance does
					Ammo = baginst;
					ammo_slot = m_inv.CalcSlotId(r, i);
					found = true;
					Log.Out(Logs::Detail, Logs::Combat, "Using ammo from quiver stack at slot %d. %d in stack.", ammo_slot, Ammo->GetCharges());
					break;
				}
			}
			if(found)
				break;
		}

		if(!found) {
			//if we dont find a quiver, look through our inventory again
			//not caring if the thing is a quiver.
			int32 aslot = m_inv.HasItem(AmmoItem->ID, 1, invWherePersonal);
			if (aslot != INVALID_INDEX) {
				ammo_slot = aslot;
				Ammo = m_inv[aslot];
				Log.Out(Logs::Detail, Logs::Combat, "Using ammo from inventory stack at slot %d. %d in stack.", ammo_slot, Ammo->GetCharges());
			}
		}
	}

	float range = RangeItem->Range + AmmoItem->Range + 7.5f; //Fudge it a little, client will let you hit something at 0 0 0 when you are at 205 0 0
	Log.Out(Logs::Detail, Logs::Combat, "Calculated bow range to be %.1f", range);
	range *= range;
	float dist = DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition());
	if(dist > range) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack out of range... client should catch this. (%f > %f).\n", dist, range);
		Message_StringID(CC_Red,TARGET_OUT_OF_RANGE);//Client enforces range and sends the message, this is a backup just incase.
		return;
	}
	else if(dist < (RuleI(Combat, MinRangedAttackDist)*RuleI(Combat, MinRangedAttackDist))){
		return;
	}

	if(!IsAttackAllowed(GetTarget()) ||
		IsCasting() ||
		IsSitting() ||
		(DivineAura() && !GetGM()) ||
		IsStunned() ||
		IsFeared() ||
		IsMezzed() ||
		(GetAppearance() == eaDead)){
		return;
	}

	//SendItemAnimation(GetTarget(), AmmoItem, SkillArchery);
	ProjectileAnimation(GetTarget(), AmmoItem->ID,true,-1,-1,-1,-1,SkillArchery);

	DoArcheryAttackDmg(GetTarget(), RangeWeapon, Ammo);

	//EndlessQuiver AA base1 = 100% Chance to avoid consumption arrow.
	int ChanceAvoidConsume = aabonuses.ConsumeProjectile + itembonuses.ConsumeProjectile + spellbonuses.ConsumeProjectile;

	if (!ChanceAvoidConsume || (ChanceAvoidConsume < 100 && zone->random.Int(0,99) > ChanceAvoidConsume)){ 
		//Let Handle_OP_DeleteCharge handle the delete, to avoid item desyncs.
		//DeleteItemInInventory(ammo_slot, 1, false);
		Log.Out(Logs::Detail, Logs::Combat, "Consumed one arrow from slot %d", ammo_slot);
	} else {
		Log.Out(Logs::Detail, Logs::Combat, "Endless Quiver prevented ammo consumption.");
	}

	CheckIncreaseSkill(SkillArchery, GetTarget(), zone->skill_difficulty[SkillArchery].difficulty); 
	CommonBreakInvisible();
}

void Mob::DoArcheryAttackDmg(Mob* other, const ItemInst* RangeWeapon, const ItemInst* Ammo, uint16 weapon_damage, int16 chance_mod, int16 focus, int ReuseTime) {
	if (!CanDoSpecialAttack(other))
		return;

	int16 bowDmg = 0;
	int16 arrowDmg = 0;
	int damage = 1;
	int hate = weapon_damage;

	if (!weapon_damage){
		bowDmg = GetWeaponDamage(other, RangeWeapon);
		arrowDmg = GetWeaponDamage(other, Ammo);
	}
	else
		bowDmg = weapon_damage;

	if (bowDmg > 0 && focus) //From FcBaseEffects
		bowDmg += bowDmg*focus / 100;

	int32 combinedDmg = bowDmg + arrowDmg;

	if (combinedDmg < 1)
	{
		damage = -5;		// immune
	}
	else
	{
		other->AvoidDamage(this, damage, true);

		if (damage > 0 && !other->AvoidanceCheck(this, SkillArchery, chance_mod)) {
			Log.Out(Logs::Detail, Logs::Combat, "Ranged attack missed %s.", other->GetName());
			damage = 0;
		}
	}

	if (damage > 0)
	{
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack hit %s.", other->GetName());

		combinedDmg = static_cast<int>(RuleR(Combat, ArcheryBaseDamageBonus) * 100.0)*combinedDmg / 100;

		// for discipline: Trueshot
		uint16 bonusArcheryDamageModifier = aabonuses.ArcheryDamageModifier + itembonuses.ArcheryDamageModifier + spellbonuses.ArcheryDamageModifier;
		combinedDmg += combinedDmg * bonusArcheryDamageModifier / 100;

		if (combinedDmg > weapon_damage)
		{
			hate = combinedDmg;
		}

		int32 offense = GetOffense(SkillArchery);
		int32 mitigation = other->GetMitigation();

		// mitigation roll
		uint16 roll = RollD20(offense, mitigation);

		damage = (roll * combinedDmg * 10 + 5) / 100;
		if (IsClient())
		{
			damage = damage * CastToClient()->RollDamageMultiplier(GetOffense(SkillArchery)) / 100;
		}

		Log.Out(Logs::Detail, Logs::Combat, "Bow DMG %d, Arrow DMG %d, Damage %d.", bowDmg, arrowDmg, damage);

		bool dobonus = false;
		if(GetClass() == RANGER && GetLevel() > 50)
		{
			int bonuschance = RuleI(Combat, ArcheryBonusChance);

			bonuschance = mod_archery_bonus_chance(bonuschance, RangeWeapon);

			if (!RuleB(Combat, UseArcheryBonusRoll) || (zone->random.Int(1, 100) < bonuschance))
			{
				if(RuleB(Combat, ArcheryBonusRequiresStationary))
				{
					if(other->IsNPC() && !other->IsMoving() && !other->IsRooted())
					{
						dobonus = true;
					}
				}
				else
				{
					dobonus = true;
				}
			}

			if (dobonus)
			{
				damage *= 2;
				hate *= 2;
				damage = mod_archery_bonus_damage(damage, RangeWeapon);
				Log.Out(Logs::Detail, Logs::Combat, "Ranger. Double damage success roll, doubling damage to %d", damage);
				Message_StringID(MT_CritMelee, BOW_DOUBLE_DAMAGE);
			}
		}

		if (damage < 1)
			damage = 1;

		damage += other->GetFcDamageAmtIncoming(this, 0, true, SkillArchery);
		damage += (damage * other->GetSkillDmgTaken(SkillArchery) / 100) + GetSkillDmgAmt(SkillArchery);

		damage = mod_archery_damage(damage, dobonus, RangeWeapon);

		TryCriticalHit(other, SkillArchery, damage);
		CheckNumHitsRemaining(NUMHIT_OutgoingHitSuccess);
	}

	other->Damage(this, damage, SPELL_UNKNOWN, SkillArchery);
	other->AddToHateList(this, hate, 0);

	//try proc on hits and misses
	if((RangeWeapon != nullptr) && GetTarget() && other && !other->HasDied()){
		TryWeaponProc(RangeWeapon, other, MainRange);
	}

	//Arrow procs because why not?
    if((Ammo != NULL) && GetTarget() && other && !other->HasDied())
    {
        TryWeaponProc(Ammo, other, MainRange);
    }
}

void NPC::RangedAttack(Mob* other)
{

	if (!other)
		return;
	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if((attack_timer.Enabled() && !attack_timer.Check(false)) || (ranged_timer.Enabled() && !ranged_timer.Check())){
		Log.Out(Logs::Detail, Logs::Combat, "Archery canceled. Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return;
	}

	if(!CheckLosFN(other))
		return;

	int attacks = GetSpecialAbilityParam(SPECATK_RANGED_ATK, 0);
	attacks = attacks > 0 ? attacks : 1;
	for(int i = 0; i < attacks; ++i) {

		//if we have SPECATK_RANGED_ATK set then we range attack without weapon or ammo
		const Item_Struct* weapon = nullptr;
		const Item_Struct* ammo = nullptr;
		if(!GetSpecialAbility(SPECATK_RANGED_ATK))
		{
			//find our bow and ammo return if we can't find them...
			return;
		}

		int sa_min_range = GetSpecialAbilityParam(SPECATK_RANGED_ATK, 4); //Min Range of NPC attack
		int sa_max_range = GetSpecialAbilityParam(SPECATK_RANGED_ATK, 1); //Max Range of NPC attack

		float min_range = static_cast<float>(RuleI(Combat, MinRangedAttackDist));
		float max_range = 250; // needs to be longer than 200(most spells)
	
		if (sa_max_range)
			max_range = static_cast<float>(sa_max_range);

		if (sa_min_range)
			min_range = static_cast<float>(sa_min_range);

		Log.Out(Logs::Detail, Logs::Combat, "Calculated bow range to be %.1f", max_range);
		max_range *= max_range;
		if (DistanceSquaredNoZ(m_Position, other->GetPosition()) > max_range)
			return;
		else if(DistanceSquaredNoZ(m_Position, other->GetPosition()) < (min_range * min_range))
			return;
	

		if(!other || !IsAttackAllowed(other) ||
			IsCasting() ||
			DivineAura() ||
			IsStunned() ||
			IsFeared() ||
			IsMezzed() ||
			(GetAppearance() == eaDead)){
			return;
		}

		SkillUseTypes skillinuse = SkillArchery;
		skillinuse = static_cast<SkillUseTypes>(GetRangedSkill());

		ammo = database.GetItem(8005);

		if (ammo)
			//SendItemAnimation(GetTarget(), ammo, SkillArchery);
			ProjectileAnimation(GetTarget(), ammo->ID, true, -1, -1, -1, -1, SkillArchery);

		FaceTarget(other);

		int damage = 1;

		other->AvoidDamage(this, damage, true);

		if (damage > 0 && !other->AvoidanceCheck(this, skillinuse, GetSpecialAbilityParam(SPECATK_RANGED_ATK, 2)))
		{
			damage = 0;
			Log.Out(Logs::Detail, Logs::Combat, "Ranged attack missed %s.", other->GetName());
		}

		if (damage > 0)
		{
			int16 WDmg = GetWeaponDamage(other, weapon);
			int16 ADmg = GetWeaponDamage(other, ammo);

			if (WDmg < 1 && ADmg < 1)
			{
				damage = -5;		// immune
			}
			else
			{
				Log.Out(Logs::Detail, Logs::Combat, "Ranged attack hit %s.", other->GetName());

				int32 maxDmg = max_dmg * RuleR(Combat, ArcheryNPCMultiplier); // should add a field to npc_types
				int32 minDmg = min_dmg * RuleR(Combat, ArcheryNPCMultiplier);
				
				uint8 roll = RollD20(GetOffense(), other->GetMitigation());
				uint32 di1k = 1;

				if (maxDmg <= minDmg)
				{
					maxDmg = minDmg;
					roll = 20;
				}
				else
				{
					di1k = (maxDmg - minDmg) * 1000 / 19;			// multiply damage interval by 1000 so truncation doesn't reduce accuracy
				}

				if (roll == 20)
				{
					damage = maxDmg;
				}
				else
				{
					damage = (di1k * roll + (minDmg * 1000 - di1k)) / 1000;
				}

				if (damage < 1)
				{
					damage = 1;
				}

				damage += damage * GetSpecialAbilityParam(SPECATK_RANGED_ATK, 3) / 100; //Damage modifier

				CommonOutgoingHitSuccess(other, damage, skillinuse);
			}
		}

		DoAnim(Animation::ShootBow);
		other->Damage(this, damage, SPELL_UNKNOWN, skillinuse);
		other->AddToHateList(this, max_dmg / 2, 0);

		//try proc on hits and misses
		if(other && !other->HasDied())
			TrySpellProc(nullptr, (const Item_Struct*)nullptr, other, MainRange);

		CommonBreakInvisible();
	}
}

void Client::ThrowingAttack(Mob* other, bool CanDoubleAttack) { //old was 51
	//conditions to use an attack checked before we are called

	//make sure the attack and ranged timers are up
	//if the ranged timer is disabled, then they have no ranged weapon and shouldent be attacking anyhow
	if((!CanDoubleAttack && (attack_timer.Enabled() && !attack_timer.Check(false)) || (ranged_timer.Enabled() && !ranged_timer.Check()))) {
		Log.Out(Logs::Detail, Logs::Combat, "Throwing attack canceled. Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		// The server and client timers are not exact matches currently, so this would spam too often if enabled
		//Message(CC_Default, "Error: Timer not up. Attack %d, ranged %d", attack_timer.GetRemainingTime(), ranged_timer.GetRemainingTime());
		return;
	}

	int ammo_slot = MainRange;
	const ItemInst* RangeWeapon = m_inv[MainRange];

	if (!RangeWeapon || !RangeWeapon->IsType(ItemClassCommon)) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack canceled. Missing or invalid ranged weapon (%d) in slot %d", GetItemIDAt(MainRange), MainRange);
		//Message(CC_Default, "Error: Rangeweapon: GetItem(%i)==0, you have nothing to throw!", GetItemIDAt(MainRange));
		return;
	}

	const Item_Struct* item = RangeWeapon->GetItem();
	if(item->ItemType != ItemTypeLargeThrowing && item->ItemType != ItemTypeSmallThrowing) {
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack canceled. Ranged item %d is not a throwing weapon. type %d.", item->ItemType);
		//Message(CC_Default, "Error: Rangeweapon: GetItem(%i)==0, you have nothing useful to throw!", GetItemIDAt(MainRange));
		return;
	}

	Log.Out(Logs::Detail, Logs::Combat, "Throwing %s (%d) at %s", item->Name, item->ID, GetTarget()->GetName());

	int range = item->Range +50/*Fudge it a little, client will let you hit something at 0 0 0 when you are at 205 0 0*/;
	Log.Out(Logs::Detail, Logs::Combat, "Calculated bow range to be %.1f", range);
	range *= range;
	float dist = DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition());
	if(dist > range) {
		Log.Out(Logs::Detail, Logs::Combat, "Throwing attack out of range... client should catch this. (%f > %f).\n", dist, range);
		Message_StringID(CC_Red,TARGET_OUT_OF_RANGE);//Client enforces range and sends the message, this is a backup just incase.
		return;
	}
	else if(dist < (RuleI(Combat, MinRangedAttackDist)*RuleI(Combat, MinRangedAttackDist))){
		return;
	}

	if(!IsAttackAllowed(GetTarget()) ||
		IsCasting() ||
		IsSitting() ||
		(DivineAura() && !GetGM()) ||
		IsStunned() ||
		IsFeared() ||
		IsMezzed() ||
		(GetAppearance() == eaDead)){
		return;
	}
	//send item animation, also does the throw animation
	ProjectileAnimation(GetTarget(), item->ID,true,-1,-1,-1,-1,SkillThrowing);

	DoThrowingAttackDmg(GetTarget(), RangeWeapon, item);

	//Let Handle_OP_DeleteCharge handle the delete, to avoid item desyncs. 
	//DeleteItemInInventory(ammo_slot, 1, false);

	CheckIncreaseSkill(SkillThrowing, GetTarget(), zone->skill_difficulty[SkillThrowing].difficulty); 
	CommonBreakInvisible();
}

void Mob::DoThrowingAttackDmg(Mob* other, const ItemInst* RangeWeapon, const Item_Struct* item, uint16 weapon_damage, int16 chance_mod,int16 focus, int ReuseTime)
{
	if (!CanDoSpecialAttack(other))
		return;

	int damage = 1;

	int16 weaponDmg = 0;

	if (!weapon_damage && item != nullptr)
		weaponDmg = GetWeaponDamage(other, item);
	else
		weaponDmg = weapon_damage;

	if (weaponDmg > 0 && focus) //From FcBaseEffects
		weaponDmg += weaponDmg*focus / 100;

	if (weaponDmg < 1)
	{
		damage = -5;		// immune
	}
	else
	{
		other->AvoidDamage(this, damage, true); //noRiposte=true - Can not riposte throw attacks.
	}

	if (damage > 0 && !other->AvoidanceCheck(this, SkillThrowing, chance_mod))
	{
		damage = 0;
		Log.Out(Logs::Detail, Logs::Combat, "Ranged attack missed %s.", other->GetName());
	}
	
	if (damage > 0)
	{
		Log.Out(Logs::Detail, Logs::Combat, "Throwing attack hit %s.", other->GetName());

		int minDmg = 1;

		int32 offense = GetOffense(SkillThrowing);
		int32 mitigation = other->GetMitigation();

		// mitigation roll
		uint16 roll = RollD20(offense, mitigation);

		damage = (roll * weaponDmg * 10 + 5) / 100;
		if (IsClient())
		{
			damage = damage * CastToClient()->RollDamageMultiplier(GetOffense(SkillThrowing)) / 100;
		}

		damage = mod_throwing_damage(damage);

		uint32 Assassinate_Dmg = 0;
		if (GetClass() == ROGUE && (BehindMob(other, GetX(), GetY())))
			Assassinate_Dmg = TryAssassinate(other, SkillThrowing, ranged_timer.GetDuration());
		if (Assassinate_Dmg)
		{
			damage = Assassinate_Dmg;
			entity_list.MessageClose_StringID(this, false, 200, MT_CritMelee, ASSASSINATES, GetName());
		}

		Log.Out(Logs::Detail, Logs::Combat, "Item DMG %d. Hit for damage %d", weaponDmg, damage);
		CommonOutgoingHitSuccess(other, damage, SkillThrowing);
	}

	other->AddToHateList(this, weaponDmg, 0);
	other->Damage(this, damage, SPELL_UNKNOWN, SkillThrowing);

	if((RangeWeapon != nullptr) && GetTarget() && other && (other->GetHP() > -10))
		TryWeaponProc(RangeWeapon, other, MainRange);
}

void Mob::SendItemAnimation(Mob *to, const Item_Struct *item, SkillUseTypes skillInUse) {
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_Projectile, sizeof(Arrow_Struct));
	Arrow_Struct *as = (Arrow_Struct *) outapp->pBuffer;
	as->type = 1;
	as->src_x = GetX();
	as->src_y = GetY();
	as->src_z = GetZ();
	as->source_id = GetID();
	as->target_id = to->GetID();
	as->object_id = item->ID;

	as->effect_type = item->ItemType;
	as->skill = (uint8)skillInUse;

	strn0cpy(as->model_name, item->IDFile, 16);


	/*
		The angular field affects how the object flies towards the target.
		A low angular (10) makes it circle the target widely, where a high
		angular (20000) makes it go straight at them.

		The tilt field causes the object to be tilted flying through the air
		and also seems to have an effect on how it behaves when circling the
		target based on the angular field.

		Arc causes the object to form an arc in motion. A value too high will
	*/
	as->velocity = 4.0;

	//these angle and tilt used together seem to make the arrow/knife throw as straight as I can make it

	as->launch_angle = CalculateHeadingToTarget(to->GetX(), to->GetY()) * 2;
	as->tilt = 125;
	as->arc = 50;


	//fill in some unknowns, we dont know their meaning yet
	//neither of these seem to change the behavior any
	as->unknown088 = 125;
	as->unknown092 = 16;

	entity_list.QueueCloseClients(this, outapp);
	safe_delete(outapp);
}

void Mob::ProjectileAnimation(Mob* to, int id, bool IsItem, float speed, float angle, float tilt, float arc, SkillUseTypes skillInUse) {
	if (!to)
		return;

	const Item_Struct* item = nullptr;
	uint8 effect_type = 0;
	char name[16];
	uint8 behavior = 0;
	uint8 light = 0;
	uint8 yaw = 0;
	uint16 target_id = to->GetID();

	if(!id)
		item = database.GetItem(8005); // Arrow will be default
	else
	{
		if(IsItem) 
			item = database.GetItem(id);
	}

	if(item)
	{
		effect_type = item->ItemType;
		behavior = 1;
		strn0cpy(name,item->IDFile, 16);
	}
	else
	{
		//28 is also a valid type. 
		effect_type = 9;
		light = 8;
		strn0cpy(name,"PLAYER_1", 16);
	}

	if(!speed || speed < 0)
		speed = 4.0;

	if(!angle || angle < 0) 
		if(IsItem)
			angle = CalculateHeadingToTarget(to->GetX(), to->GetY()) * 2;
		else
			angle = to->GetHeading()*2;

	if(!tilt || tilt < 0)
		if(IsItem) 
			tilt = 125;
		else
			tilt = 0;
	
	if(!arc || arc < 0)
		if(IsItem)
			arc = 50;
		else
			arc = 0;

	if(GetID() == to->GetID())
	{
		yaw = 0.001;
		target_id = 0;
	}

	// See SendItemAnimation() for some notes on this struct
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_Projectile, sizeof(Arrow_Struct));	
	Arrow_Struct *as = (Arrow_Struct *) outapp->pBuffer;
	as->type = 1;
	as->src_x = GetX();
	as->src_y = GetY();
	as->src_z = GetZ();
	as->source_id = GetID();
	as->target_id = target_id;
	as->object_id = id;
	as->effect_type = effect_type;
	as->skill = (uint8)skillInUse;
	strn0cpy(as->model_name, name, 16);	
	as->velocity = speed;
	as->launch_angle = angle;
	as->tilt = tilt;
	as->arc = arc;
	as->light = light;
	as->behavior = behavior;
	as->pitch = 0;
	as->yaw = yaw;

	//For the benefit of modern clients
	as->unknown088 = 125; 
	as->unknown092 = 16;

	if(!IsItem)
		entity_list.QueueClients(this, outapp, false);
	else
		entity_list.QueueCloseClients(this, outapp);

	safe_delete(outapp);

}

void NPC::DoClassAttacks(Mob *target) {
	if(target == nullptr)
		return;	//gotta have a target for all these

	bool taunt_time = taunt_timer.Check();
	bool ca_time = classattack_timer.Check(false);
	bool ka_time = knightattack_timer.Check(false);

	//only check attack allowed if we are going to do something
	if((taunt_time || ca_time || ka_time) && !IsAttackAllowed(target))
		return;

	if(ka_time){
		int knightreuse = 1000; //lets give it a small cooldown actually.
		switch(GetClass()){
			case SHADOWKNIGHT: case SHADOWKNIGHTGM:{
				CastSpell(SPELL_NPC_HARM_TOUCH, target->GetID());
				knightreuse = HarmTouchReuseTime * 1000;
				break;
			}
			case PALADIN: case PALADINGM:{
				if(GetHPRatio() < 20) {
					CastSpell(SPELL_LAY_ON_HANDS, GetID());
					knightreuse = LayOnHandsReuseTime * 1000;
				} else {
					knightreuse = 2000; //Check again in two seconds.
				}
				break;
			}
		}
		knightattack_timer.Start(knightreuse);
	}

	//general stuff, for all classes....
	//only gets used when their primary ability get used too
	if (taunting && HasOwner() && target->IsNPC() && target->GetBodyType() != BT_Undead && taunt_time) {
		this->GetOwner()->Message_StringID(MT_PetResponse, PET_TAUNTING);
		Taunt(target->CastToNPC(), false);
	}

	if(!ca_time)
		return;

	float HasteModifier = GetHaste() * 0.01f;

	int level = GetLevel();
	int reuse = TauntReuseTime * 1000;	//make this very long since if they dont use it once, they prolly never will
	bool did_attack = false;
	int32 dmg = 0;

	//class specific stuff...
	switch(GetClass()) {
		case ROGUE: case ROGUEGM:
			if(level >= 10) {
				reuse = BackstabReuseTime * 1000;
				TryBackstab(target, reuse);
				did_attack = true;
			}
			break;
		case MONK: case MONKGM: {
			uint8 satype = SkillKick;
			if(level > 29) { satype = SkillFlyingKick; }
			else if(level > 24) { satype = SkillDragonPunch; }
			else if(level > 19) { satype = SkillEagleStrike; }
			else if(level > 9) { satype = SkillTigerClaw; }
			else if(level > 4) { satype = SkillRoundKick; }
			reuse = MonkSpecialAttack(target, satype);

			reuse *= 1000;
			did_attack = true;
			break;
		}
		case WARRIOR: case WARRIORGM:{
			if(level >= RuleI(Combat, NPCBashKickLevel)){
				if(zone->random.Roll(75)) { //tested on live, warrior mobs both kick and bash, kick about 75% of the time, casting doesn't seem to make a difference.
					DoAnim(Animation::Kick);

					if(GetWeaponDamage(target, (const Item_Struct*)nullptr) <= 0){
						dmg = -5;
					}
					else
					{
						dmg = GetMaxBashKickDmg();
					}

					reuse = (KickReuseTime + 3) * 1000;
					DoSpecialAttackDamage(target, SkillKick, dmg, GetMinBashKickDmg(), -1, reuse, true);
					did_attack = true;
				}
				else {
					DoAnim(Animation::Slam);

					dmg = GetMaxBashKickDmg();

					reuse = (BashReuseTime + 3) * 1000;
					DoSpecialAttackDamage(target, SkillBash, dmg, GetMinBashKickDmg(), -1, reuse, true);
					did_attack = true;
				}
			}
			break;
		}
		case RANGER: case RANGERGM:
		case BEASTLORD: case BEASTLORDGM: {
			//kick
			if(level >= RuleI(Combat, NPCBashKickLevel)){
				DoAnim(Animation::Kick);

				if(GetWeaponDamage(target, (const Item_Struct*)nullptr) <= 0){
					dmg = -5;
				}
				else
				{
					dmg = GetMaxBashKickDmg();
				}

				reuse = (KickReuseTime + 3) * 1000;
				DoSpecialAttackDamage(target, SkillKick, dmg, GetMinBashKickDmg(), -1, reuse, true);
				did_attack = true;
			}
			break;
		}
		case CLERIC: case CLERICGM: //clerics can bash too.
		case SHADOWKNIGHT: case SHADOWKNIGHTGM:
		case PALADIN: case PALADINGM:{
			if(level >= RuleI(Combat, NPCBashKickLevel)){
				DoAnim(Animation::Slam);

				dmg = GetMaxBashKickDmg();

				reuse = (BashReuseTime + 3) * 1000;
				DoSpecialAttackDamage(target, SkillBash, dmg, GetMinBashKickDmg(), -1, reuse, true);
				did_attack = true;
			}
			break;
		}
	}

	classattack_timer.Start(reuse / HasteModifier);
}

void Client::DoClassAttacks(Mob *ca_target, uint16 skill, bool IsRiposte)
{
	if(!ca_target)
		return;

	if(spellend_timer.Enabled() || IsFeared() || IsStunned() || IsMezzed() || DivineAura() || dead)
		return;

	if(!IsAttackAllowed(ca_target))
		return;

	//check range for all these abilities, they are all close combat stuff
	if(!CombatRange(ca_target)){
		return;
	}

	if(!IsRiposte && (!p_timers.Expired(&database, pTimerCombatAbility, false))) {
		return;
	}

	int ReuseTime = 0;
	float HasteMod = GetHaste() * 0.01f;

	int32 dmg = 0;

	uint16 skill_to_use = -1;

	if (skill == -1){
		switch(GetClass()){
		case WARRIOR:
		case RANGER:
		case BEASTLORD:
			skill_to_use = SkillKick;
			break;
		case SHADOWKNIGHT:
		case PALADIN:
			skill_to_use = SkillBash;
			break;
		case MONK:
			if(GetLevel() >= 30)
			{
				skill_to_use = SkillFlyingKick;
			}
			else if(GetLevel() >= 25)
			{
				skill_to_use = SkillDragonPunch;
			}
			else if(GetLevel() >= 20)
			{
				skill_to_use = SkillEagleStrike;
			}
			else if(GetLevel() >= 10)
			{
				skill_to_use = SkillTigerClaw;
			}
			else if(GetLevel() >= 5)
			{
				skill_to_use = SkillRoundKick;
			}
			else
			{
				skill_to_use = SkillKick;
			}
			break;
		case ROGUE:
			skill_to_use = SkillBackstab;
			break;
		}
	}

	else
		skill_to_use = skill;

	if(skill_to_use == -1)
		return;

	if(skill_to_use == SkillBash) {
		if (ca_target!=this) {
			DoAnim(Animation::Slam);

			dmg = GetBashDamage();

			ReuseTime = (BashReuseTime - 1) / HasteMod;

			DoSpecialAttackDamage(ca_target, SkillBash, dmg, 1, -1, ReuseTime, true);

			if(ReuseTime > 0 && !IsRiposte) {
				p_timers.Start(pTimerCombatAbility, ReuseTime);
			}
		}
		return;
	}

	if(skill_to_use == SkillFrenzy){
		CheckIncreaseSkill(SkillFrenzy, GetTarget(), zone->skill_difficulty[SkillFrenzy].difficulty);
		int AtkRounds = 3;
		int skillmod = 100*GetSkill(SkillFrenzy)/MaxSkill(SkillFrenzy);
		int32 max_dmg = (26 + ((((GetLevel()-6) * 2)*skillmod)/100)) * ((100+RuleI(Combat, FrenzyBonus))/100);
		int32 min_dmg = 0;
		DoAnim(Animation::Slashing2H);

		if (GetLevel() < 51)
			min_dmg = 1;
		else
			min_dmg = GetLevel()*8/10;

		if (min_dmg > max_dmg)
			max_dmg = min_dmg;

		ReuseTime = (FrenzyReuseTime - 1) / HasteMod;

		//Live parses show around 55% Triple 35% Double 10% Single, you will always get first hit.
		while(AtkRounds > 0) {

			if (GetTarget() && (AtkRounds == 1 || zone->random.Roll(75))) {
				DoSpecialAttackDamage(GetTarget(), SkillFrenzy, max_dmg, min_dmg, max_dmg , ReuseTime, true);
			}
			AtkRounds--;
		}

		if(ReuseTime > 0 && !IsRiposte) {
			p_timers.Start(pTimerCombatAbility, ReuseTime);
		}
		return;
	}

	if(skill_to_use == SkillKick){
		if(ca_target!=this){
			DoAnim(Animation::Kick);

			if(GetWeaponDamage(ca_target, GetInv().GetItem(MainFeet)) <= 0){
				dmg = -5;
			}
			else
			{
				dmg = GetKickDamage();
			}

			ReuseTime = KickReuseTime-1;

			DoSpecialAttackDamage(ca_target, SkillKick, dmg, 1,-1, ReuseTime, true);
		}
	}

	if(skill_to_use == SkillFlyingKick || skill_to_use == SkillDragonPunch || skill_to_use == SkillEagleStrike || skill_to_use == SkillTigerClaw || skill_to_use == SkillRoundKick) {
		ReuseTime = MonkSpecialAttack(ca_target, skill_to_use) - 1;
		MonkSpecialAttack(ca_target, skill_to_use);

		if (IsRiposte)
			return;

		//Live AA - Technique of Master Wu
		int wuchance = itembonuses.DoubleSpecialAttack + spellbonuses.DoubleSpecialAttack + aabonuses.DoubleSpecialAttack;
		if (wuchance) {
			if (wuchance >= 100 || zone->random.Roll(wuchance)) {
				int MonkSPA [5] = { SkillFlyingKick, SkillDragonPunch, SkillEagleStrike, SkillTigerClaw, SkillRoundKick };
				int extra = 1;
				if (zone->random.Roll(wuchance / 4))
					extra++;
				// They didn't add a string ID for this.
				std::string msg = StringFormat("The spirit of Master Wu fills you!  You gain %d additional attack(s).", extra);
				// live uses 400 here -- not sure if it's the best for all clients though
				while (extra) {
					MonkSpecialAttack(ca_target, MonkSPA[zone->random.Int(0, 4)]);
					extra--;
				}
			}
		}
	}

	if(skill_to_use == SkillBackstab){
		ReuseTime = BackstabReuseTime-1;

		if (IsRiposte)
			ReuseTime=0;

		TryBackstab(ca_target,ReuseTime);
	}

	ReuseTime = ReuseTime / HasteMod;
	if(ReuseTime > 0 && !IsRiposte){ 
		p_timers.Start(pTimerCombatAbility, ReuseTime);
	}
}

void Mob::Taunt(NPC* who, bool always_succeed, float chance_bonus) {

	if (who == nullptr)
		return;

	if(DivineAura())
		return;

	if(!CombatRange(who))
		return;

	if(!always_succeed && IsClient())
		CastToClient()->CheckIncreaseSkill(SkillTaunt, who, zone->skill_difficulty[SkillTaunt].difficulty);

	Mob *hate_top = who->GetHateMost();

	int level_difference = GetLevel() - who->GetLevel();
	bool Success = false;

	//Support for how taunt worked pre 2000 on LIVE - Can not taunt NPC over your level.
	if ((RuleB(Combat,TauntOverLevel) == false) && (level_difference < 0) || who->GetSpecialAbility(IMMUNE_TAUNT)){
		//Message_StringID(MT_SpellFailure,FAILED_TAUNT);
		return;
	}

	//All values used based on live parses after taunt was updated in 2006.
	int32 newhate = 0;
	float tauntchance = 50.0f;

	if(always_succeed)
		tauntchance = 101.0f;

	else
	{
		if (level_difference < 0)
		{
			tauntchance += static_cast<float>(level_difference)*3.0f;
			if (tauntchance < 20)
				tauntchance = 20.0f;
		}
		else
		{
			tauntchance += static_cast<float>(level_difference)*5.0f;
			if (tauntchance > 65)
				tauntchance = 65.0f;
		}
	}

	//TauntSkillFalloff rate is not based on any real data. Default of 33% gives a reasonable result.
	if (IsClient() && !always_succeed)
		tauntchance -= (RuleR(Combat,TauntSkillFalloff) * (CastToClient()->MaxSkill(SkillTaunt) - GetSkill(SkillTaunt)));

	//From SE_Taunt (Does a taunt with a chance modifier)
	if (chance_bonus)
		tauntchance += tauntchance*chance_bonus/100.0f;

	if (tauntchance < 1)
		tauntchance = 1.0f;

	tauntchance /= 100.0f;

	if (tauntchance > zone->random.Real(0, 1))
	{
		if (hate_top && hate_top != this)
		{
			newhate = (who->GetNPCHate(hate_top, false) - who->GetNPCHate(this, false));
			if (newhate > 0)
			{
				who->CastToNPC()->AddToHateList(this, newhate);
			}
			Success = true;
		}
		else
			who->CastToNPC()->AddToHateList(this,12);

		if (who->CanTalk())
			who->Say_StringID(SUCCESSFUL_TAUNT,GetCleanName());
	}
//	else{
	//	Message_StringID(MT_SpellFailure,FAILED_TAUNT);
//	}

	//else
	//	Message_StringID(MT_SpellFailure,FAILED_TAUNT);

	if (HasSkillProcs())
		TrySkillProc(who, SkillTaunt, TauntReuseTime*1000);

	if (Success && HasSkillProcSuccess())
		TrySkillProc(who, SkillTaunt, TauntReuseTime*1000, true);
}


void Mob::InstillDoubt(Mob *who) {
	//make sure we can use this skill
	/*int skill = GetSkill(INTIMIDATION);*/	//unused

	//make sure our target is an NPC
	if(!who || !who->IsNPC())
		return;

	if(DivineAura())
		return;

	//range check
	if(!CombatRange(who))
		return;

	//I think this formula needs work
	int value = 0;

	//user's bonus
	value += GetSkill(SkillIntimidation) + GetCHA()/4;

	//target's counters
	value -= target->GetLevel()*4 + who->GetWIS()/4;

	if (zone->random.Roll(value)) {
		//temporary hack...
		//cast fear on them... should prolly be a different spell
		//and should be un-resistable.
		SpellOnTarget(229, who, false, true, -2000);
		//is there a success message?
	} else {
		Message_StringID(CC_Blue,NOT_SCARING);
		//Idea from WR:
		/* if (target->IsNPC() && zone->random.Int(0,99) < 10 ) {
			entity_list.MessageClose(target, false, 50, MT_NPCRampage, "%s lashes out in anger!",target->GetName());
			//should we actually do this? and the range is completely made up, unconfirmed
			entity_list.AEAttack(target, 50);
		}*/
	}
}

float Mob::GetSpecialProcChances(uint16 hand)
{
	int mydex = GetDEX();

	if (mydex > 255)
		mydex = 255;

	uint16 weapon_speed;
	float ProcChance = 0.0f;
	float ProcBonus = 0.0f;

	weapon_speed = GetWeaponSpeedbyHand(hand);

	if (RuleB(Combat, AdjustSpecialProcPerMinute)) {
		ProcChance = (static_cast<float>(weapon_speed) *
				RuleR(Combat, AvgSpecialProcsPerMinute) / 60000.0f); 
		ProcBonus +=  static_cast<float>(mydex/35);
		ProcChance += ProcChance * ProcBonus / 100.0f;
	} else {
		/*PRE 2014 CHANGE Dev Quote - "Elidroth SOE:Proc chance is a function of your base hardcapped Dexterity / 35 + Heroic Dexterity / 25.
		Kayen: Most reports suggest a ~ 6% chance to Headshot which consistent with above.*/

		ProcChance = static_cast<float>(mydex/35)/100.0f;
	}

	return ProcChance;
}

uint32 Mob::TryAssassinate(Mob* defender, SkillUseTypes skillInUse, uint16 ReuseTime) {

	if(defender && (defender->GetBodyType() == BT_Humanoid) && !defender->IsClient() &&
		(skillInUse == SkillBackstab || skillInUse == SkillThrowing)) {

		uint32 Assassinate_Dmg = aabonuses.Assassinate[1] + spellbonuses.Assassinate[1] + itembonuses.Assassinate[1];

		uint8 Assassinate_Level = 0; //Get Highest Headshot Level
		Assassinate_Level = aabonuses.AssassinateLevel;
		if (Assassinate_Level < spellbonuses.AssassinateLevel)
			Assassinate_Level = spellbonuses.AssassinateLevel;
		else if (Assassinate_Level < itembonuses.AssassinateLevel)
			Assassinate_Level = itembonuses.AssassinateLevel;

		if (GetLevel() >= 60){ //Innate Assassinate Ability if client as no bonuses.
			if (!Assassinate_Level)
				Assassinate_Level = 45;

			if (!Assassinate_Dmg)
				Assassinate_Dmg = 32000;
		}

		if(Assassinate_Dmg && Assassinate_Level && (defender->GetLevel() <= Assassinate_Level)){
			float ProcChance = 0.0f;

			if (skillInUse == SkillThrowing)
				ProcChance = GetSpecialProcChances(MainRange);
			else
				ProcChance = GetAssassinateProcChances(ReuseTime);

			if(zone->random.Roll(ProcChance))
				return Assassinate_Dmg;
		}
	}

	return 0;
}

float Mob::GetAssassinateProcChances(uint16 ReuseTime)
{
	float mydex = static_cast<float>(GetDEX());

	if (mydex > 255.0f)
		mydex = 255.0f;

	float ProcChance = 0.0f;
	float ProcBonus = 0.0f;

	if (RuleB(Combat, AdjustSpecialProcPerMinute))
	{
		ProcChance = (static_cast<float>(ReuseTime*1000) *
				RuleR(Combat, AvgSpecialProcsPerMinute) / 60000.0f);
		ProcBonus += mydex / 100.0f / 100.0f;
		ProcChance += ProcChance * ProcBonus / 100.0f;
	}
	else
	{
		// this is pretty much a wild guess
		ProcChance = mydex / 100.0f / 100.0f;
	}

	return ProcChance;
}

bool Mob::CanDoSpecialAttack(Mob *other) {
	//Make sure everything is valid before doing any attacks.
	if (!other) {
		SetTarget(nullptr);
		return false;
	}

	if(!GetTarget())
		SetTarget(other);

	if ((other == nullptr || ((IsClient() && CastToClient()->dead) || (other->IsClient() && other->CastToClient()->dead)) || HasDied() || (!IsAttackAllowed(other)))) {
		return false;
	}

	if(other->GetInvul() || other->GetSpecialAbility(IMMUNE_MELEE))
		return false;

	return true;
}
