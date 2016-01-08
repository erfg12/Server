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

#include "../common/spdat.h"
#include "../common/string_util.h"

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "remote_call_subscribe.h"
#include "worldserver.h"
#include "remote_call_subscribe.h"

#include <limits.h>
#include <math.h>
#include <sstream>

extern EntityList entity_list;

extern Zone* zone;
extern WorldServer worldserver;

Mob::Mob(const char* in_name,
		const char* in_lastname,
		int32		in_cur_hp,
		int32		in_max_hp,
		uint8		in_gender,
		uint16		in_race,
		uint8		in_class,
		bodyType	in_bodytype,
		uint8		in_deity,
		uint8		in_level,
		uint32		in_npctype_id,
		float		in_size,
		float		in_runspeed,
		const glm::vec4& position,
		uint8		in_light,
		uint8		in_texture,
		uint8		in_helmtexture,
		uint16		in_ac,
		uint16		in_atk,
		uint16		in_str,
		uint16		in_sta,
		uint16		in_dex,
		uint16		in_agi,
		uint16		in_int,
		uint16		in_wis,
		uint16		in_cha,
		uint8		in_haircolor,
		uint8		in_beardcolor,
		uint8		in_eyecolor1, // the eyecolors always seem to be the same, maybe left and right eye?
		uint8		in_eyecolor2,
		uint8		in_hairstyle,
		uint8		in_luclinface,
		uint8		in_beard,
		uint32		in_armor_tint[_MaterialCount],
		uint8		in_aa_title,
		uint8		in_see_invis, // see through invis/ivu
		uint8		in_see_invis_undead,
		uint8		in_see_hide,
		uint8		in_see_improved_hide,
		int32		in_hp_regen,
		int32		in_mana_regen,
		uint8		in_qglobal,
		uint8		in_maxlevel,
		uint32		in_scalerate,
		uint8		in_armtexture,
		uint8		in_bracertexture,
		uint8		in_handtexture,
		uint8		in_legtexture,
		uint8		in_feettexture,
		uint8		in_chesttexture
		) :
		attack_timer(2000),
		attack_dw_timer(2000),
		ranged_timer(2000),
		tic_timer(6000),
		mana_timer(2000),
		spellend_timer(0),
		rewind_timer(30000), //Timer used for determining amount of time between actual player position updates for /rewind.
		bindwound_timer(10000),
		stunned_timer(0),
		spun_timer(0),
		bardsong_timer(6000),
		gravity_timer(1000),
		viral_timer(0),
		m_FearWalkTarget(BEST_Z_INVALID,BEST_Z_INVALID,BEST_Z_INVALID),
		m_TargetLocation(glm::vec3()),
		m_TargetV(glm::vec3()),
		flee_timer(FLEE_CHECK_TIMER),
		m_Position(position)
{
	targeted = 0;
	tar_ndx=0;
	tar_vector=0;
	curfp = false;

	AI_Init();
	SetMoving(false);
	moved=false;
	m_RewindLocation = glm::vec3();
	move_tic_count = zone->random.Int(0, RuleI(Zone, NPCPositonUpdateTicCount));

	_egnode = nullptr;
	name[0]=0;
	orig_name[0]=0;
	clean_name[0]=0;
	lastname[0]=0;
	if(in_name) {
		strn0cpy(name,in_name,64);
		strn0cpy(orig_name,in_name,64);
	}
	if(in_lastname)
		strn0cpy(lastname,in_lastname,64);
	cur_hp		= in_cur_hp;
	max_hp		= in_max_hp;
	base_hp		= in_max_hp;
	gender		= in_gender;
	race		= in_race;
	base_gender	= in_gender;
	base_race	= in_race;
	class_		= in_class;
	bodytype	= in_bodytype;
	orig_bodytype = in_bodytype;
	deity		= in_deity;
	level		= in_level;
	orig_level = in_level;
	npctype_id	= in_npctype_id;
	size		= in_size;
	base_size	= in_size;
	runspeed	= in_runspeed;
	current_speed = 0.0f;

	float mysize = in_size;
	if(mysize > RuleR(Map, BestZSizeMax))
		mysize = RuleR(Map, BestZSizeMax);
	z_offset = RuleR(Map, BestZMultiplier) * mysize;


	// sanity check
	if (runspeed < 0 || runspeed > 20)
		runspeed = 1.3f; // mob speeds on mac client, only support increments of 0.1

	int_runspeed = (int)((float)runspeed * 40.0f) + 2; // add the +2 to give a round behavior - int speeds are increments of 4
	
	// clients - these look low, compared to mobs.  But player speeds translate different in client, with respect to NPCs.
	// in game a 0.7 speed for client, is same as a 1.4 speed for NPCs.
	if (runspeed == 0.7f) {
		int_runspeed = 28;
		walkspeed = 0.3f;
		int_walkspeed = 12;
		fearspeed = 0.6f;
		int_fearspeed = 24;
	// npcs
	} else {
		int_walkspeed = int_runspeed * 100 / 265;
		walkspeed = (float)int_walkspeed / 40.0f;
		int_fearspeed = int_runspeed * 100 / 127;
		fearspeed = (float)int_fearspeed / 40.0f;
	}
	// adjust speeds to round to 0.100
	int_runspeed = (int_runspeed >> 2) << 2; // equivalent to divide by 4, then multiply by 4
	runspeed = (float)int_runspeed / 40.0f;
	int_walkspeed = (int_walkspeed >> 2) << 2;
	walkspeed = (float)int_walkspeed / 40.0f;
	int_fearspeed = (int_fearspeed >> 2) << 2;
	fearspeed = (float)int_fearspeed / 40.0f;

	m_Light.Type.Innate = in_light;
	m_Light.Level.Innate = m_Light.TypeToLevel(m_Light.Type.Innate);
	m_Light.Level.Equipment = m_Light.Type.Equipment = 0;
	m_Light.Level.Spell = m_Light.Type.Spell = 0;
	m_Light.Type.Active = m_Light.Type.Innate;
	m_Light.Level.Active = m_Light.Level.Innate;
	
	texture		= in_texture;
	chesttexture = in_chesttexture;
	helmtexture	= in_helmtexture;
	armtexture = in_armtexture;
	bracertexture = in_bracertexture;
	handtexture = in_handtexture;
	legtexture = in_legtexture;
	feettexture = in_feettexture;
	multitexture = (chesttexture || armtexture || bracertexture || handtexture || legtexture || feettexture);

	haircolor	= in_haircolor;
	beardcolor	= in_beardcolor;
	eyecolor1	= in_eyecolor1;
	eyecolor2	= in_eyecolor2;
	hairstyle	= in_hairstyle;
	luclinface	= in_luclinface;
	beard		= in_beard;
	attack_delay = 0;
	slow_mitigation = 0;
	findable	= false;
	trackable	= true;
	has_shieldequiped = false;
	has_numhits = false;
	has_MGB = false;
	has_ProjectIllusion = false;
	SpellPowerDistanceMod = 0;
	last_los_check = false;

	if(in_aa_title>0)
		aa_title	= in_aa_title;
	else
		aa_title	=0xFF;
	AC		= in_ac;
	ATK		= in_atk;
	STR		= in_str;
	STA		= in_sta;
	DEX		= in_dex;
	AGI		= in_agi;
	INT		= in_int;
	WIS		= in_wis;
	CHA		= in_cha;
	MR = CR = FR = DR = PR = Corrup = 0;

	ExtraHaste = 0;
	bEnraged = false;

	shield_target = nullptr;
	cur_mana = 0;
	max_mana = 0;
	hp_regen = in_hp_regen;
	mana_regen = in_mana_regen;
	oocregen = RuleI(NPC, OOCRegen); //default Out of Combat Regen
	maxlevel = in_maxlevel;
	scalerate = in_scalerate;
	invisible = false;
	invisible_undead = false;
	invisible_animals = false;
	sneaking = false;
	hidden = false;
	improved_hidden = false;
	invulnerable = false;
	IsFullHP	= (cur_hp == max_hp);
	qglobal=0;
	spawnpacket_sent = false;

	InitializeBuffSlots();

	// clear the proc arrays
	int i;
	int j;
	for (j = 0; j < MAX_PROCS; j++)
	{
		PermaProcs[j].spellID = SPELL_UNKNOWN;
		PermaProcs[j].chance = 0;
		PermaProcs[j].base_spellID = SPELL_UNKNOWN;
		SpellProcs[j].spellID = SPELL_UNKNOWN;
		SpellProcs[j].chance = 0;
		SpellProcs[j].base_spellID = SPELL_UNKNOWN;
	}

	for (i = 0; i < _MaterialCount; i++)
	{
		if (in_armor_tint)
		{
			armor_tint[i] = in_armor_tint[i];
		}
		else
		{
			armor_tint[i] = 0;
		}
	}

	m_Delta = glm::vec4();
	animation = 0;

	logging_enabled = false;
	isgrouped = false;
	israidgrouped = false;
	entity_id_being_looted = 0;
	_appearance = eaStanding;
	pRunAnimSpeed = 0;

	spellend_timer.Disable();
	bardsong_timer.Disable();
	bardsong = 0;
	bardsong_target_id = 0;
	casting_spell_id = 0;
	casting_spell_timer = 0;
	casting_spell_timer_duration = 0;
	casting_spell_type = 0;
	casting_spell_inventory_slot = 0;
	target = 0;

	memset(&itembonuses, 0, sizeof(StatBonuses));
	memset(&spellbonuses, 0, sizeof(StatBonuses));
	memset(&aabonuses, 0, sizeof(StatBonuses));
	spellbonuses.AggroRange = -1;
	spellbonuses.AssistRange = -1;
	pLastChange = 0;
	SetPetID(0);
	SetOwnerID(0);
	typeofpet = petCharmed;		//default to charmed...
	summonerid = 0;
	petpower = 0;
	held = false;
	nocast = false;
	focused = false;
	_IsTempPet = false;
	pet_owner_client = false;
	can_equip_secondary = false;
	can_dual_wield = false;

	attacked_count = 0;
	mezzed = false;
	stunned = false;
	silenced = false;
	amnesiad = false;
	inWater = false;
	int m;
	for (m = 0; m < MAX_SHIELDERS; m++)
	{
		shielder[m].shielder_id = 0;
		shielder[m].shielder_bonus = 0;
	}

	wandertype=0;
	pausetype=0;
	cur_wp = 0;
	m_CurrentWayPoint = glm::vec4();
	cur_wp_pause = 0;
	patrol=0;
	follow=0;
	follow_dist = 100;	// Default Distance for Follow
	flee_mode = false;
	curfp = false;
	flee_timer.Start();

	permarooted = (runspeed > 0) ? false : true;

	movetimercompleted = false;
	roamer = false;
	rooted = false;
	charmed = false;
	blind = false;
	has_virus = false;
	for (i=0; i<MAX_SPELL_TRIGGER*2; i++) {
		viral_spells[i] = 0;
	}
	pStandingPetOrder = SPO_Follow;
	pseudo_rooted = false;

	see_invis = GetSeeInvisible(in_see_invis);
	see_invis_undead = GetSeeInvisible(in_see_invis_undead);
	see_hide = GetSeeInvisible(in_see_hide);
	see_improved_hide = GetSeeInvisible(in_see_improved_hide);
	qglobal = in_qglobal != 0;

	// Bind wound
	bindwound_timer.Disable();
	bindwound_target = 0;

	trade = new Trade(this);
	// hp event
	nexthpevent = -1;
	nextinchpevent = -1;

	hasTempPet = false;
	count_TempPet = 0;

	//Boats always "run." Ignore launches and player controlled ships.
	if(GetBaseRace() == SHIP && RuleB(NPC, BoatsRunByDefault))
	{
		m_is_running = true;
	}
	else
	{
		m_is_running = false;
	}

	nimbus_effect1 = 0;
	nimbus_effect2 = 0;
	nimbus_effect3 = 0;
	m_targetable = true;

	flymode = FlyMode3;
	// Pathing
	PathingLOSState = UnknownLOS;
	PathingLoopCount = 0;
	PathingLastNodeVisited = -1;
	PathingLastNodeSearched = -1;
	PathingLOSCheckTimer = new Timer(RuleI(Pathing, LOSCheckFrequency));
	PathingRouteUpdateTimerShort = new Timer(RuleI(Pathing, RouteUpdateFrequencyShort));
	PathingRouteUpdateTimerLong = new Timer(RuleI(Pathing, RouteUpdateFrequencyLong));
	DistractedFromGrid = false;
	PathingTraversedNodes = 0;
	hate_list.SetOwner(this);

	m_AllowBeneficial = false;
	m_DisableMelee = false;
	for (int i = 0; i < HIGHEST_SKILL+2; i++) { SkillDmgTaken_Mod[i] = 0; }
	for (int i = 0; i < HIGHEST_RESIST+2; i++) { Vulnerability_Mod[i] = 0; }

	emoteid = 0;
	endur_upkeep = false;
	combat_hp_regen = 0;
	combat_mana_regen = 0;
	iszomm = false;
	adjustedz = 0;
	PrimaryAggro = false;
	AssistAggro = false;
	MerchantSession = 0;
	dire_charmed = false;
	player_damage = 0;
	dire_pet_damage = 0;
	total_damage = 0;
}

Mob::~Mob()
{
	AI_Stop();
	if (GetPet()) {
		if (GetPet()->Charmed())
			GetPet()->BuffFadeByEffect(SE_Charm);
		else
			SetPet(0);
	}

	EQApplicationPacket app;
	CreateDespawnPacket(&app, !IsCorpse());
	Corpse* corpse = entity_list.GetCorpseByID(GetID());
	if(!corpse || (corpse && !corpse->IsPlayerCorpse()))
		entity_list.QueueClients(this, &app, true);

	entity_list.RemoveFromTargets(this);

	if(trade) {
		Mob *with = trade->With();
		if(with && with->IsClient()) {
			with->CastToClient()->FinishTrade(with);
			with->trade->Reset();
		}
		delete trade;
	}

	if(HasTempPetsActive()){
		entity_list.DestroyTempPets(this);
	}
	safe_delete(PathingLOSCheckTimer);
	safe_delete(PathingRouteUpdateTimerShort);
	safe_delete(PathingRouteUpdateTimerLong);
	UninitializeBuffSlots();
}

uint32 Mob::GetAppearanceValue(EmuAppearance iAppearance) {
	switch (iAppearance) {
		// 0 standing, 1 sitting, 2 ducking, 3 lieing down, 4 looting
		case eaStanding: {
			return ANIM_STAND;
		}
		case eaSitting: {
			return ANIM_SIT;
		}
		case eaCrouching: {
			return ANIM_CROUCH;
		}
		case eaDead: {
			return ANIM_DEATH;
		}
		case eaLooting: {
			return ANIM_LOOT;
		}
		//to shup up compiler:
		case _eaMaxAppearance:
			break;
	}
	return(ANIM_STAND);
}

void Mob::SetInvisible(uint8 state, bool showInvis)
{
	if(state == INVIS_OFF || state == INVIS_NORMAL)
	{
		invisible = (bool) state;
	}

	if(showInvis) 
	{
		SendAppearancePacket(AT_Invis, invisible);
	}

	// Invis and hide breaks charms
	if (HasPet() && state != INVIS_OFF)
	{
		FadePetCharmBuff();
		DepopPet();
	}
}

//check to see if `this` is invisible to `other`
bool Mob::IsInvisible(Mob* other) const
{
	if(!other)
		return(false);

	uint8 SeeInvisBonus = 0;
	if (IsClient())
		SeeInvisBonus = aabonuses.SeeInvis;

	//check regular invisibility
	if (invisible && invisible > (other->SeeInvisible()))
		return true;

	//check invis vs. undead
	if (other->GetBodyType() == BT_Undead || other->GetBodyType() == BT_SummonedUndead) {
		if(invisible_undead && !other->SeeInvisibleUndead())
			return true;
	}

	//check invis vs. animals...
	if (other->GetBodyType() == BT_Animal){
		if(invisible_animals && !other->SeeInvisible())
			return true;
	}

	if(hidden){
		if(!other->see_hide && !other->see_improved_hide){
			return true;
		}
	}

	if(improved_hidden){
		if(!other->see_improved_hide){
			return true;
		}
	}

	//handle sneaking
	if(sneaking) {
		if(BehindMob(other, GetX(), GetY()) )
			return true;
	}

	return(false);
}

int Mob::_GetWalkSpeed() const {
 
    if (IsRooted() || IsStunned() || IsMezzed())
		return 0;
	
	int aa_mod = 0;
	int speed_mod = int_walkspeed;
	int base_run = int_runspeed;
	bool has_horse = false;
	if (IsClient())
	{
		Mob* horse = entity_list.GetMob(CastToClient()->GetHorseId());
		if(horse)
		{
			speed_mod = horse->int_walkspeed;
			base_run = horse->int_runspeed;
			has_horse = true;
		} else {
			aa_mod += ((CastToClient()->GetAA(aaInnateRunSpeed))
				+ (CastToClient()->GetAA(aaFleetofFoot))
				+ (CastToClient()->GetAA(aaSwiftJourney))
			);
		}
		//Selo's Enduring Cadence should be +7% per level
	}
	
	int spell_mod = spellbonuses.movementspeed + itembonuses.movementspeed;
	int movemod = 0;

	if(spell_mod < 0)
	{
		movemod += spell_mod;
	}
	else if(spell_mod > aa_mod)
	{
		movemod = spell_mod;
	}
	else
	{
		movemod = aa_mod;
	}
	
	if(movemod < -85) //cap it at moving very very slow
		movemod = -85;
	
	if (!has_horse && movemod != 0)
		speed_mod += (base_run * movemod / 100);

	if(speed_mod < 4)
		return(0);

	//runspeed cap.
	if(IsClient())
	{
		if(GetClass() == BARD) {
			//this extra-high bard cap should really only apply if they have AAs
			if(speed_mod > 72)
				speed_mod = 72;
		} else {
			if(speed_mod > 64)
				speed_mod = 64;
		}
	}
	speed_mod = (speed_mod >> 2) << 2;
	if (speed_mod < 4)
		return 0;
	return speed_mod;
}

int Mob::_GetRunSpeed() const {
	if (IsRooted() || IsStunned() || IsMezzed())
		return 0;
	
	int aa_mod = 0;
	int speed_mod = int_runspeed;
	int base_walk = int_walkspeed;
	bool has_horse = false;
	if (IsClient())
	{
		if(CastToClient()->GetGMSpeed())
		{
			speed_mod = 124; // this is same as speed of 3.1f
		}
		else
		{
			Mob* horse = entity_list.GetMob(CastToClient()->GetHorseId());
			if(horse)
			{
				speed_mod = horse->int_runspeed;
				base_walk = horse->int_walkspeed;
				has_horse = true;
			}
		}
		aa_mod += itembonuses.BaseMovementSpeed + spellbonuses.BaseMovementSpeed + aabonuses.BaseMovementSpeed;

	}
	
	int spell_mod = spellbonuses.movementspeed + itembonuses.movementspeed;
	int movemod = 0;

	if(spell_mod < 0)
	{
		movemod += spell_mod;
	}
	else if(spell_mod > aa_mod)
	{
		movemod = spell_mod;
	}
	else
	{
		movemod = aa_mod;
	}
	
	if(movemod < -85) //cap it at moving very very slow
		movemod = -85;
	
	if (!has_horse && movemod != 0)
	{
		if (IsClient())
		{
			speed_mod += (speed_mod * movemod / 100);
		} else {
			if (movemod < 0) {

				speed_mod += (50 * movemod / 100);

				// basically stoped
				if(speed_mod < 4)
					return(0);
				// moving slowly
				if (speed_mod < 8)
					return(8);
			} else {
				speed_mod += int_walkspeed;
				if (movemod > 50)
					speed_mod += 4;
				if (movemod > 40)
					speed_mod += 3;
			}
		}
	}

	if(speed_mod < 4)
		return(0);

	//runspeed cap.
	if(IsClient())
	{
		if(GetClass() == BARD) {
			//this extra-high bard cap should really only apply if they have AAs
			if(speed_mod > 72)
				speed_mod = 72;
		} else {
			if(speed_mod > 64)
				speed_mod = 64;
		}
	}
	speed_mod = (speed_mod >> 2) << 2;
	if (speed_mod < 4)
		return 0;
	return (speed_mod);
}


int Mob::_GetFearSpeed() const {

	if (IsRooted() || IsStunned() || IsMezzed())
		return 0;
	
	//float speed_mod = fearspeed;
	int speed_mod = int_fearspeed;

	// use a max of 1.8f in calcs.
	int base_run = std::min(int_runspeed, 72);

	int spell_mod = spellbonuses.movementspeed + itembonuses.movementspeed;

	int movemod = 0;

	if(spell_mod < 0)
	{
		movemod += spell_mod;
	}
	
	if(movemod < -85) //cap it at moving very very slow
		movemod = -85;
	
	if (IsClient()) {
		if (CastToClient()->GetRunMode())
			speed_mod = int_fearspeed;
		else
			speed_mod = int_walkspeed;
		if (movemod < 0)
			return int_walkspeed;
		speed_mod += (base_run * movemod / 100);
		speed_mod = (speed_mod >> 2) << 2;
		if (speed_mod < 4)
			return 0;
		return (speed_mod);
	} else {
		float hp_ratio = GetHPRatio();
		// very large snares 50% or higher
		if (movemod < -49)
		{
			if (hp_ratio < 25)
				return (0);
			if (hp_ratio < 50)
				return (8);
			else
				return (12);
		}
		if (hp_ratio < 5) {
			speed_mod = int_walkspeed / 3 + 4;
		} else if (hp_ratio < 15) {
			speed_mod = int_walkspeed / 2 + 4;
 		} else if (hp_ratio < 25) {
			speed_mod = int_walkspeed + 4; // add this, so they are + 0.1 so they do the run animation
		} else if (hp_ratio < 50) {
			speed_mod *= 82;
			speed_mod /= 100;
		}
		if (movemod > 0) {
			speed_mod += int_walkspeed;
			if (movemod > 50)
				speed_mod += 4;
			if (movemod > 40)
				speed_mod += 3;
			speed_mod = (speed_mod >> 2) << 2;
			if (speed_mod < 4)
				return 0;
			return speed_mod;
		}
		else if (movemod < 0) {
			speed_mod += (base_run * movemod / 100);
		}
	}
	if (speed_mod < 4)
		return (0);
	if (speed_mod < 12)
		return (8); // 0.2f is slow walking - this should be the slowest walker for fleeing, if not snared enough to stop
	speed_mod = (speed_mod >> 2) << 2;
	if (speed_mod < 4)
		return 0;
	return speed_mod;
}

//float Mob::_GetMovementSpeed(int mod, bool iswalking) const
//{
//	// List of movement speed modifiers, including AAs & spells:
//	// http://everquest.allakhazam.com/db/item.html?item=1721;page=1;howmany=50#m10822246245352
//	if (IsRooted())
//		return 0.0f;

//	float speed_mod = 0.0f;
	
//	if(iswalking)
//		speed_mod = walkspeed;
//	else
//		speed_mod = runspeed;

	// These two cases ignore the cap, be wise in the DB for horses.
//	if (IsClient()) {
//		if (CastToClient()->GetGMSpeed()) {
//			speed_mod = 3.125f;
//			if (mod != 0)
//				speed_mod += speed_mod * static_cast<float>(mod) / 100.0f;
//			return speed_mod;
//		} else {
//			Mob *horse = entity_list.GetMob(CastToClient()->GetHorseId());
//			if (horse) {
//				speed_mod = horse->GetBaseRunspeed();
//				if (mod != 0)
//					speed_mod += speed_mod * static_cast<float>(mod) / 100.0f;
//				return speed_mod;
//			}
//		}
//	}

//	int aa_mod = 0;
//	int spell_mod = 0;
//	int runspeedcap = RuleI(Character,BaseRunSpeedCap);
//	int movemod = 0;
//	float frunspeedcap = 0.0f;

//	runspeedcap += itembonuses.IncreaseRunSpeedCap + spellbonuses.IncreaseRunSpeedCap + aabonuses.IncreaseRunSpeedCap;
//	aa_mod += itembonuses.BaseMovementSpeed + spellbonuses.BaseMovementSpeed + aabonuses.BaseMovementSpeed;
//	spell_mod += spellbonuses.movementspeed + itembonuses.movementspeed;

	// hard cap
//	if (runspeedcap > 150)
//		runspeedcap = 150;

//	if (spell_mod < 0)
//		movemod += spell_mod;
//	else if (spell_mod > aa_mod)
//		movemod = spell_mod;
//	else
//		movemod = aa_mod;

	// cap negative movemods from snares mostly
//	if (movemod < -85)
//		movemod = -85;

//	if (movemod != 0)
//		speed_mod += speed_mod * static_cast<float>(movemod) / 100.0f;

	// runspeed caps
//	frunspeedcap = static_cast<float>(runspeedcap) / 100.0f;
//	if (IsClient() && speed_mod > frunspeedcap)
//		speed_mod = frunspeedcap;

	// apply final mod such as the -47 for walking
	// use runspeed since it should stack with snares
	// and if we get here, we know runspeed was the initial
	// value before we applied movemod.
//	if (mod != 0)
//		speed_mod += runspeed * static_cast<float>(mod) / 100.0f;

//	if (speed_mod <= 0.0f)
//		speed_mod = IsClient() ? 0.0001f : 0.0f;

//	return speed_mod;
//}

int32 Mob::CalcMaxMana() {
	switch (GetCasterClass()) {
		case 'I':
			max_mana = (((GetINT()/2)+1) * GetLevel()) + spellbonuses.Mana + itembonuses.Mana;
			break;
		case 'W':
			max_mana = (((GetWIS()/2)+1) * GetLevel()) + spellbonuses.Mana + itembonuses.Mana;
			break;
		case 'N':
		default:
			max_mana = 0;
			break;
	}
	if (max_mana < 0) {
		max_mana = 0;
	}

	return max_mana;
}

int32 Mob::CalcMaxHP() {
	max_hp = (base_hp + itembonuses.HP + spellbonuses.HP);
	max_hp += max_hp * ((aabonuses.MaxHPChange + spellbonuses.MaxHPChange + itembonuses.MaxHPChange) / 10000.0f);
	return max_hp;
}

int32 Mob::GetItemHPBonuses() {
	int32 item_hp = 0;
	item_hp = itembonuses.HP;
	item_hp += item_hp * itembonuses.MaxHPChange / 10000;
	return item_hp;
}

int32 Mob::GetSpellHPBonuses() {
	int32 spell_hp = 0;
	spell_hp = spellbonuses.HP;
	spell_hp += spell_hp * spellbonuses.MaxHPChange / 10000;
	return spell_hp;
}

char Mob::GetCasterClass() const {
	switch(class_)
	{
	case CLERIC:
	case PALADIN:
	case RANGER:
	case DRUID:
	case SHAMAN:
	case BEASTLORD:
	case CLERICGM:
	case PALADINGM:
	case RANGERGM:
	case DRUIDGM:
	case SHAMANGM:
	case BEASTLORDGM:
		return 'W';
		break;

	case SHADOWKNIGHT:
	case BARD:
	case NECROMANCER:
	case WIZARD:
	case MAGICIAN:
	case ENCHANTER:
	case SHADOWKNIGHTGM:
	case BARDGM:
	case NECROMANCERGM:
	case WIZARDGM:
	case MAGICIANGM:
	case ENCHANTERGM:
		return 'I';
		break;

	default:
		return 'N';
		break;
	}
}

uint8 Mob::GetArchetype() const {
	switch(class_)
	{
	case PALADIN:
	case RANGER:
	case SHADOWKNIGHT:
	case BARD:
	case BEASTLORD:
	case PALADINGM:
	case RANGERGM:
	case SHADOWKNIGHTGM:
	case BARDGM:
	case BEASTLORDGM:
		return ARCHETYPE_HYBRID;
		break;
	case CLERIC:
	case DRUID:
	case SHAMAN:
	case NECROMANCER:
	case WIZARD:
	case MAGICIAN:
	case ENCHANTER:
	case CLERICGM:
	case DRUIDGM:
	case SHAMANGM:
	case NECROMANCERGM:
	case WIZARDGM:
	case MAGICIANGM:
	case ENCHANTERGM:
		return ARCHETYPE_CASTER;
		break;
	case WARRIOR:
	case MONK:
	case ROGUE:
	case WARRIORGM:
	case MONKGM:
	case ROGUEGM:
		return ARCHETYPE_MELEE;
		break;
	default:
		return ARCHETYPE_HYBRID;
		break;
	}
}

void Mob::CreateSpawnPacket(EQApplicationPacket* app, Mob* ForWho) {
	app->SetOpcode(OP_NewSpawn);
	app->size = sizeof(NewSpawn_Struct);
	app->pBuffer = new uchar[app->size];
	memset(app->pBuffer, 0, app->size);
	NewSpawn_Struct* ns = (NewSpawn_Struct*)app->pBuffer;
	FillSpawnStruct(ns, ForWho);

	if(strlen(ns->spawn.lastName) == 0) {
		switch(ns->spawn.class_)
		{
		case BANKER:
			strcpy(ns->spawn.lastName, "Banker");
			break;
		case WARRIORGM:
			strcpy(ns->spawn.lastName, "Warrior Guildmaster");
			break;
		case PALADINGM:
			strcpy(ns->spawn.lastName, "Paladin Guildmaster");
			break;
		case RANGERGM:
			strcpy(ns->spawn.lastName, "Ranger Guildmaster");
			break;
		case SHADOWKNIGHTGM:
			strcpy(ns->spawn.lastName, "Shadowknight Guildmaster");
			break;
		case DRUIDGM:
			strcpy(ns->spawn.lastName, "Druid Guildmaster");
			break;
		case BARDGM:
			strcpy(ns->spawn.lastName, "Bard Guildmaster");
			break;
		case ROGUEGM:
			strcpy(ns->spawn.lastName, "Rogue Guildmaster");
			break;
		case SHAMANGM:
			strcpy(ns->spawn.lastName, "Shaman Guildmaster");
			break;
		case NECROMANCERGM:
			strcpy(ns->spawn.lastName, "Necromancer Guildmaster");
			break;
		case WIZARDGM:
			strcpy(ns->spawn.lastName, "Wizard Guildmaster");
			break;
		case MAGICIANGM:
			strcpy(ns->spawn.lastName, "Magician Guildmaster");
			break;
		case ENCHANTERGM:
			strcpy(ns->spawn.lastName, "Enchanter Guildmaster");
			break;
		case BEASTLORDGM:
			strcpy(ns->spawn.lastName, "Beastlord Guildmaster");
			break;
		default:
			break;
		}
	}
}

void Mob::CreateSpawnPacket(EQApplicationPacket* app, NewSpawn_Struct* ns) {
	app->SetOpcode(OP_NewSpawn);
	app->size = sizeof(NewSpawn_Struct);

	app->pBuffer = new uchar[sizeof(NewSpawn_Struct)];

	// Copy ns directly into packet
	memcpy(app->pBuffer, ns, sizeof(NewSpawn_Struct));

	// Custom packet data
	NewSpawn_Struct* ns2 = (NewSpawn_Struct*)app->pBuffer;
	strcpy(ns2->spawn.name, ns->spawn.name);
	switch(ns->spawn.class_)
	{
	case BANKER:
		strcpy(ns2->spawn.lastName, "Banker");
		break;
	case WARRIORGM:
		strcpy(ns2->spawn.lastName, "Warrior Guildmaster");
		break;
	case PALADINGM:
		strcpy(ns2->spawn.lastName, "Paladin Guildmaster");
		break;
	case RANGERGM:
		strcpy(ns2->spawn.lastName, "Ranger Guildmaster");
		break;
	case SHADOWKNIGHTGM:
		strcpy(ns2->spawn.lastName, "Shadowknight Guildmaster");
		break;
	case DRUIDGM:
		strcpy(ns2->spawn.lastName, "Druid Guildmaster");
		break;
	case BARDGM:
		strcpy(ns2->spawn.lastName, "Bard Guildmaster");
		break;
	case ROGUEGM:
		strcpy(ns2->spawn.lastName, "Rogue Guildmaster");
		break;
	case SHAMANGM:
		strcpy(ns2->spawn.lastName, "Shaman Guildmaster");
		break;
	case NECROMANCERGM:
		strcpy(ns2->spawn.lastName, "Necromancer Guildmaster");
		break;
	case WIZARDGM:
		strcpy(ns2->spawn.lastName, "Wizard Guildmaster");
		break;
	case MAGICIANGM:
		strcpy(ns2->spawn.lastName, "Magician Guildmaster");
		break;
	case ENCHANTERGM:
		strcpy(ns2->spawn.lastName, "Enchanter Guildmaster");
		break;
	case BEASTLORDGM:
		strcpy(ns2->spawn.lastName, "Beastlord Guildmaster");
		break;
	default:
		strcpy(ns2->spawn.lastName, ns->spawn.lastName);
		break;
	}

	memset(&app->pBuffer[sizeof(Spawn_Struct)-7], 0xFF, 7);
}

void Mob::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho)
{
	int i;

	strcpy(ns->spawn.name, name);
	if(IsClient()) {
		strn0cpy(ns->spawn.lastName, lastname, sizeof(ns->spawn.lastName));
	}

	ns->spawn.heading	= static_cast<uint16>(m_Position.w);
	ns->spawn.x			= m_Position.x;//((int32)x_pos)<<3;
	ns->spawn.y			= m_Position.y;//((int32)y_pos)<<3;
	ns->spawn.z			= m_Position.z;//((int32)z_pos)<<3;
	ns->spawn.spawnId	= GetID();
	ns->spawn.curHp	= static_cast<uint8>(GetHPRatio());
	ns->spawn.max_hp	= 100;		//this field needs a better name
	ns->spawn.race		= race;
	ns->spawn.runspeed	= runspeed;
	ns->spawn.walkspeed	= walkspeed;
	ns->spawn.class_	= class_;
	ns->spawn.gender	= gender;
	ns->spawn.level		= level;
	ns->spawn.deity		= deity;
	ns->spawn.animation	= animation;
	ns->spawn.findable	= findable?1:0;

	UpdateActiveLight();
	ns->spawn.light		= m_Light.Type.Active;

	if(IsNPC())
		ns->spawn.invis		= (invisible || hidden || !trackable) ? 1 : 0;
	else
		ns->spawn.invis		= (invisible || hidden) ? 1 : 0;	// TODO: load this before spawning players

	ns->spawn.NPC		= IsNPC() ? 1 : 0;
	ns->spawn.petOwnerId	= ownerid;

	ns->spawn.haircolor = haircolor;
	ns->spawn.beardcolor = beardcolor;
	ns->spawn.eyecolor1 = eyecolor1;
	ns->spawn.eyecolor2 = eyecolor2;
	ns->spawn.hairstyle = hairstyle;
	ns->spawn.face = luclinface;
	ns->spawn.beard = beard;
	ns->spawn.StandState = GetAppearanceValue(_appearance);
	ns->spawn.bodytexture = texture;

	if(helmtexture && helmtexture != 0xFF)
	{
		ns->spawn.helm=helmtexture;
	} else {
		ns->spawn.helm = 0;
	}

	ns->spawn.guildrank	= 0xFF;
	ns->spawn.size			= size;
	ns->spawn.bodytype = bodytype;
	// The 'flymode' settings have the following effect:
	// 0 - Mobs in water sink like a stone to the bottom
	// 1 - Same as #flymode 1
	// 2 - Same as #flymode 2
	// 3 - Mobs in water do not sink. A value of 3 in this field appears to be the default setting for all mobs
	// (in water or not) according to 6.2 era packet collects.
	if(IsClient())
	{
		ns->spawn.flymode = FindType(SE_Levitate) ? 2 : 0;
	}
	else
		ns->spawn.flymode = flymode;

	ns->spawn.lastName[0] = '\0';

	strn0cpy(ns->spawn.lastName, lastname, sizeof(ns->spawn.lastName));

	for(i = 0; i < _MaterialCount; i++)
	{
		ns->spawn.equipment[i] = GetEquipmentMaterial(i);
		if (armor_tint[i])
		{
			ns->spawn.colors[i].color = armor_tint[i];
		}
		else
		{
			ns->spawn.colors[i].color = GetEquipmentColor(i);
		}
	}

	memset(ns->spawn.set_to_0xFF, 0xFF, sizeof(ns->spawn.set_to_0xFF));
}

void Mob::CreateDespawnPacket(EQApplicationPacket* app, bool Decay)
{
	app->SetOpcode(OP_DeleteSpawn);
	app->size = sizeof(DeleteSpawn_Struct);
	app->pBuffer = new uchar[app->size];
	memset(app->pBuffer, 0, app->size);
	DeleteSpawn_Struct* ds = (DeleteSpawn_Struct*)app->pBuffer;
	ds->spawn_id = GetID();
	// The next field only applies to corpses. If 0, they vanish instantly, otherwise they 'decay'
	ds->Decay = Decay ? 1 : 0;
}

void Mob::CreateHPPacket(EQApplicationPacket* app)
{
	this->IsFullHP=(cur_hp>=max_hp);
	app->SetOpcode(OP_MobHealth);
	app->size = sizeof(SpawnHPUpdate_Struct2);
	app->pBuffer = new uchar[app->size];
	memset(app->pBuffer, 0, sizeof(SpawnHPUpdate_Struct2));
	SpawnHPUpdate_Struct2* ds = (SpawnHPUpdate_Struct2*)app->pBuffer;

	ds->spawn_id = GetID();
	// they don't need to know the real hp
	ds->hp = (int)GetHPRatio();

	// hp event
	if (IsNPC() && (GetNextHPEvent() > 0))
	{
		if (ds->hp < GetNextHPEvent())
		{
			char buf[10];
			snprintf(buf, 9, "%i", GetNextHPEvent());
			buf[9] = '\0';
			SetNextHPEvent(-1);
			parse->EventNPC(EVENT_HP, CastToNPC(), nullptr, buf, 0);
		}
	}

	if (IsNPC() && (GetNextIncHPEvent() > 0))
	{
		if (ds->hp > GetNextIncHPEvent())
		{
			char buf[10];
			snprintf(buf, 9, "%i", GetNextIncHPEvent());
			buf[9] = '\0';
			SetNextIncHPEvent(-1);
			parse->EventNPC(EVENT_HP, CastToNPC(), nullptr, buf, 1);
		}
	}
}

// sends hp update of this mob to people who might care
void Mob::SendHPUpdate()
{
	EQApplicationPacket hp_app;
	Group *group;

	// destructor will free the pBuffer
	CreateHPPacket(&hp_app);

	// send to people who have us targeted
	entity_list.QueueClientsByTarget(this, &hp_app, false, 0, false, true, BIT_AllClients);

	// send to group
	if(IsGrouped())
	{
		group = entity_list.GetGroupByMob(this);
		if(group) //not sure why this might be null, but it happens
			group->SendHPPacketsFrom(this);
	}

	if(IsClient()){
		Raid *r = entity_list.GetRaidByClient(CastToClient());
		if(r){
			r->SendHPPacketsFrom(this);
		}
	}

	// send to master
	if(GetOwner() && GetOwner()->IsClient())
	{
		GetOwner()->CastToClient()->QueuePacket(&hp_app, false);
		group = entity_list.GetGroupByClient(GetOwner()->CastToClient());
		if(group)
			group->SendHPPacketsFrom(this);
		Raid *r = entity_list.GetRaidByClient(GetOwner()->CastToClient());
		if(r)
			r->SendHPPacketsFrom(this);
	}

	// send to pet
	if(GetPet() && GetPet()->IsClient())
	{
		GetPet()->CastToClient()->QueuePacket(&hp_app, false);
	}

	// send to self - we need the actual hps here
	if(IsClient())
	{
		EQApplicationPacket* hp_app2 = new EQApplicationPacket(OP_HPUpdate,sizeof(SpawnHPUpdate_Struct));
		SpawnHPUpdate_Struct* ds = (SpawnHPUpdate_Struct*)hp_app2->pBuffer;
		ds->cur_hp = CastToClient()->GetHP() - itembonuses.HP;
		ds->spawn_id = GetID();
		ds->max_hp = CastToClient()->GetMaxHP() - itembonuses.HP;
		CastToClient()->QueuePacket(hp_app2, false);
		safe_delete(hp_app2);
	}
}

// this one just warps the mob to the current location
void Mob::SendPosition()
{
	EQApplicationPacket* app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
	SpawnPositionUpdates_Struct* spu = (SpawnPositionUpdates_Struct*)app->pBuffer;
	spu->num_updates = 1; // hack - only one spawn position per update
	MakeSpawnUpdateNoDelta(&spu->spawn_update);
	move_tic_count = 0;
	tar_ndx = 20;
	entity_list.QueueCloseClientsPrecalc(this, app, true, nullptr, false);
	//entity_list.QueueCloseClients(this, app, true, 1000, nullptr, false);
	safe_delete(app);
}

// this one just warps the mob to the current location
void Mob::SendRealPosition()
{
	EQApplicationPacket* app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
	SpawnPositionUpdates_Struct* spu = (SpawnPositionUpdates_Struct*)app->pBuffer;
	spu->num_updates = 1; // hack - only one spawn position per update
	MakeSpawnUpdateNoDelta(&spu->spawn_update);
	entity_list.QueueClients(this, app, true);
	safe_delete(app);
}

// this one is for mobs on the move, and clients.
void Mob::SendPosUpdate(uint8 iSendToSelf) 
{
	EQApplicationPacket* app = new EQApplicationPacket(OP_MobUpdate, sizeof(SpawnPositionUpdates_Struct));
	SpawnPositionUpdates_Struct* spu = (SpawnPositionUpdates_Struct*)app->pBuffer;
	spu->num_updates = 1; // hack - only one spawn position per update
	MakeSpawnUpdate(&spu->spawn_update);

	if (iSendToSelf == 2) {
		if (this->IsClient()) {
			this->CastToClient()->FastQueuePacket(&app,false);
			return;
		}
	}
	else
	{
		if(IsClient())
		{
			if(CastToClient()->gmhideme)
				entity_list.QueueClientsStatus(this,app,(iSendToSelf==0),CastToClient()->Admin(),255);
			else
				entity_list.QueueCloseClientsPrecalc(this, app, (iSendToSelf==0), nullptr, false);
		}
		else
		{
			uint32 position_update = RuleI(Zone, NPCPositonUpdateTicCount);
			if(move_tic_count == position_update)
			{
				entity_list.QueueCloseClientsPrecalc(this, app, (iSendToSelf==0), nullptr, false, true);
				move_tic_count = 0;
			}
			else
			{
				entity_list.QueueCloseClientsPrecalc(this, app, (iSendToSelf==0), nullptr, false);
				move_tic_count++;
			}
		}
	}
	safe_delete(app);
}

// this is for SendPosition() It shouldn't be used for player updates, only NPCs that haven't moved.
void Mob::MakeSpawnUpdateNoDelta(SpawnPositionUpdate_Struct *spu)
{
	memset(spu,0xff,sizeof(SpawnPositionUpdate_Struct));

	spu->spawn_id	= GetID();
	spu->x_pos = static_cast<int16>(m_Position.x + 0.5f);
	spu->y_pos = static_cast<int16>(m_Position.y + 0.5f);
	spu->z_pos = static_cast<int16>((m_Position.z)*10);
	spu->heading	= static_cast<int8>(m_Position.w);

	spu->delta_x	= 0;
	spu->delta_y	= 0;
	spu->delta_z	= 0;
	spu->delta_heading = 0;

	spu->anim_type	= 0;

	if(IsNPC()) 
	{
		std::vector<std::string> params;
		params.push_back(std::to_string((long)GetID()));
		params.push_back(GetCleanName());
		params.push_back(std::to_string((double)m_Position.x));
		params.push_back(std::to_string((double)m_Position.y));
		params.push_back(std::to_string((double)m_Position.z));
		params.push_back(std::to_string((double)m_Position.w));
		params.push_back(std::to_string((double)GetClass()));
		params.push_back(std::to_string((double)GetRace()));
		RemoteCallSubscriptionHandler::Instance()->OnEvent("NPC.Position", params);
	}
}

// this is for SendPosUpdate()
void Mob::MakeSpawnUpdate(SpawnPositionUpdate_Struct* spu) 
{
	auto currentloc = glm::vec4(GetX(), GetY(), GetZ(), GetHeading());

	spu->spawn_id	= GetID();
	spu->x_pos = static_cast<int16>(m_Position.x + 0.5f);
	spu->y_pos = static_cast<int16>(m_Position.y + 0.5f);
	spu->z_pos = static_cast<int16>(10.0f * m_Position.z);
	spu->heading	= static_cast<int8>(currentloc.w);

	spu->delta_x	= static_cast<int32>(m_Delta.x) & 0x3FF;
	spu->delta_y	= static_cast<int32>(m_Delta.y) & 0x7FF;
	spu->delta_z	= 0;
	spu->delta_heading = static_cast<int8>(m_Delta.w);

	if(this->IsClient() || this->iszomm)
	{
		spu->anim_type = animation;
	}
	else if(this->IsNPC())
	{
		spu->anim_type = pRunAnimSpeed;
	}
	
}

void Mob::SetSpawnUpdate(SpawnPositionUpdate_Struct* incoming, SpawnPositionUpdate_Struct* outgoing) 
{
	outgoing->spawn_id	= incoming->spawn_id;
	outgoing->x_pos = incoming->x_pos;
	outgoing->y_pos = incoming->y_pos;
	outgoing->z_pos = incoming->z_pos;
	outgoing->heading	= incoming->heading;
	outgoing->delta_x	= incoming->delta_x;
	outgoing->delta_y	= incoming->delta_y;
	outgoing->delta_z	= incoming->delta_z;
	outgoing->delta_heading = incoming->delta_heading;
	outgoing->anim_type = incoming->anim_type;
}

void Mob::ShowStats(Client* client)
{
	if (IsClient()) {
		CastToClient()->SendStats(client);
	}
	else if (IsCorpse()) {
		if (IsPlayerCorpse()) {
			client->Message(CC_Default, "  CharID: %i  PlayerCorpse: %i Empty: %i Rezed: %i Exp: %i GMExp: %i KilledBy: %i Rez Time: %d Owner Online: %i", CastToCorpse()->GetCharID(), CastToCorpse()->GetCorpseDBID(), CastToCorpse()->IsEmpty(), CastToCorpse()->IsRezzed(), CastToCorpse()->GetRezExp(), CastToCorpse()->GetGMRezExp(), CastToCorpse()->GetKilledBy(), CastToCorpse()->GetRemainingRezTime(), CastToCorpse()->GetOwnerOnline());
		}
		else {
			client->Message(CC_Default, "  NPCCorpse", GetID());
		}
	}
	else {
		client->Message(CC_Default, "  Level: %i  AC: %i  Class: %i  Size: %1.1f  BaseSize: %1.1f Haste: %i", GetLevel(), GetAC(), GetClass(), GetSize(), GetBaseSize(), GetHaste());
		client->Message(CC_Default, "  HP: %i  Max HP: %i",GetHP(), GetMaxHP());
		client->Message(CC_Default, "  Mana: %i  Max Mana: %i", GetMana(), GetMaxMana());
		client->Message(CC_Default, "  X: %0.2f Y: %0.2f Z: %0.2f", GetX(), GetY(), GetZ());
		client->Message(CC_Default, "  Offense: %i  To-Hit: %i  Mitigation: %i  Avoidance: %i", GetOffense(), GetToHit(), GetMitigation(), GetAvoidance());
		client->Message(CC_Default, "  Total ATK: %i  Worn/Spell ATK (Cap %i): %i", GetATK(), RuleI(Character, ItemATKCap), GetATKBonus());
		client->Message(CC_Default, "  STR: %i  STA: %i  DEX: %i  AGI: %i  INT: %i  WIS: %i  CHA: %i", GetSTR(), GetSTA(), GetDEX(), GetAGI(), GetINT(), GetWIS(), GetCHA());
		client->Message(CC_Default, "  MR: %i  PR: %i  FR: %i  CR: %i  DR: %i", GetMR(), GetPR(), GetFR(), GetCR(), GetDR());
		client->Message(CC_Default, "  Race: %i  BaseRace: %i Gender: %i  BaseGender: %i BodyType: %i", GetRace(), GetBaseRace(), GetGender(), GetBaseGender(), GetBodyType());
		client->Message(CC_Default, "  Face: % i Beard: %i  BeardColor: %i  Hair: %i  HairColor: %i Light: %i ActiveLight: %i ", GetLuclinFace(), GetBeard(), GetBeardColor(), GetHairStyle(), GetHairColor(), GetInnateLightType(), GetActiveLightType());
		client->Message(CC_Default, "  Texture: %i  HelmTexture: %i  Chest: %i Arms: %i Bracer: %i Hands: %i Legs: %i Feet: %i ", GetTexture(), GetHelmTexture(), GetChestTexture(), GetArmTexture(), GetBracerTexture(), GetHandTexture(), GetLegTexture(), GetFeetTexture());
		client->Message(CC_Default, "  IsPet: %i PetID: %i  OwnerID: %i IsFamiliar: %i IsCharmed: %i IsDireCharm: %i IsZomm: %i ", IsPet(), GetPetID(), GetOwnerID(), IsFamiliar(), IsCharmed(), IsDireCharmed(), iszomm);
		if (client->Admin() >= 100)
			client->Message(CC_Default, "  EntityID: %i AIControlled: %i Targetted: %i", GetID(), IsAIControlled(), targeted);

		if (IsNPC()) {
			NPC *n = CastToNPC();
			uint32 spawngroupid = 0;
			if(n->respawn2 != 0)
				spawngroupid = n->respawn2->SpawnGroupID();
			client->Message(CC_Default, "  NPCID: %u  SpawnGroupID: %u Grid: %i FactionID: %i PreCharmFactionID: %i PrimaryFaction: %i", GetNPCTypeID(),spawngroupid, n->GetGrid(), n->GetNPCFactionID(), n->GetPreCharmNPCFactionID(), GetPrimaryFaction());
			client->Message(CC_Default, "  HP Regen: %i Mana Regen: %i Magic Atk: %i Immune to Melee: %i", n->GetHPRegen(), n->GetManaRegen(), GetSpecialAbility(SPECATK_MAGICAL), GetSpecialAbility(IMMUNE_MELEE_NONMAGICAL));
			client->Message(CC_Default, "  Attack Speed: %i Accuracy: %i LootTable: %u SpellsID: %u MerchantID: %i", n->GetAttackTimer(), n->GetAccuracyRating(), n->GetLoottableID(), n->GetNPCSpellsID(), n->MerchantType);
			client->Message(CC_Default, "  EmoteID: %i Trackable: %i SeeInvis: %i SeeInvUndead: %i SeeHide: %i SeeImpHide: %i", n->GetEmoteID(), n->IsTrackable(), n->SeeInvisible(), n->SeeInvisibleUndead(), n->SeeHide(), n->SeeImprovedHide());
			client->Message(CC_Default, "  CanEquipSec: %i DualWield: %i KickDmg: %i BashDmg: %i HasShield: %i", n->CanEquipSecondary(), n->CanDualWield(), n->GetKickDamage(), n->GetBashDamage(), n->HasShieldEquiped());
			client->Message(CC_Default, "  PriSkill: %i SecSkill: %i PriMelee: %i SecMelee: %i Double Atk Chance: %i Dual Wield Chance: %i", n->GetPrimSkill(), n->GetSecSkill(), n->GetPrimaryMeleeTexture(), n->GetSecondaryMeleeTexture(), n->DoubleAttackChance(), n->DualWieldChance());
			client->Message(CC_Default, "  Runspeed: %f Walkspeed: %f RunSpeedAnim: %i CurrentSpeed: %f", GetRunspeed(), GetWalkspeed(), GetRunAnimSpeed(), GetCurrentSpeed());
			client->Message(CC_Default, "  Aggro: Pri/Asst %d %d AssistCap: %d IgnoreDistance: %0.2f",  HasPrimaryAggro(), HasAssistAggro(), NPCAssistCap(), n->GetIgnoreDistance());
			if(flee_mode)
				client->Message(CC_Default, "  Fleespeed: %f", n->GetFearSpeed());
			n->QueryLoot(client);
		}
		if (IsAIControlled()) {
			client->Message(CC_Default, "  AggroRange: %1.0f  AssistRange: %1.0f", GetAggroRange(), GetAssistRange());
		}
	}
}

void Mob::DoAnim(Animation animnum, int type, bool ackreq, eqFilterType filter) {
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Animation, sizeof(Animation_Struct));
	Animation_Struct* anim = (Animation_Struct*)outapp->pBuffer;
	anim->spawnid = GetID();
	if(GetTarget())
		anim->target = GetTarget()->GetID();
	anim->action = animnum;
	anim->value=type;
	anim->unknown10=16256;

	entity_list.QueueCloseClients(this, outapp, false, 200, 0, ackreq, filter);
	safe_delete(outapp);
}

void Mob::ShowBuffs(Client* client) {
	if(SPDAT_RECORDS <= 0)
		return;
	client->Message(CC_Default, "Buffs on: %s", this->GetName());
	uint32 i;
	uint32 buff_count = GetMaxTotalSlots();
	for (i=0; i < buff_count; i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			if (spells[buffs[i].spellid].buffdurationformula == DF_Permanent)
				client->Message(CC_Default, "  %i: %s: Permanent", i, spells[buffs[i].spellid].name);
			else
				client->Message(CC_Default, "  %i: %s: %i tics left", i, spells[buffs[i].spellid].name, buffs[i].ticsremaining);

		}
	}
	if (IsClient()){
		client->Message(CC_Default, "itembonuses:");
		client->Message(CC_Default, "Atk:%i Ac:%i HP(%i):%i Mana:%i", itembonuses.ATK, itembonuses.AC, itembonuses.HPRegen, itembonuses.HP, itembonuses.Mana);
		client->Message(CC_Default, "Str:%i Sta:%i Dex:%i Agi:%i Int:%i Wis:%i Cha:%i",
			itembonuses.STR,itembonuses.STA,itembonuses.DEX,itembonuses.AGI,itembonuses.INT,itembonuses.WIS,itembonuses.CHA);
		client->Message(CC_Default, "SvMagic:%i SvFire:%i SvCold:%i SvPoison:%i SvDisease:%i",
				itembonuses.MR,itembonuses.FR,itembonuses.CR,itembonuses.PR,itembonuses.DR);
		client->Message(CC_Default, "DmgShield:%i Haste:%i", itembonuses.DamageShield, itembonuses.haste );
		client->Message(CC_Default, "spellbonuses:");
		client->Message(CC_Default, "Atk:%i Ac:%i HP(%i):%i Mana:%i", spellbonuses.ATK, spellbonuses.AC, spellbonuses.HPRegen, spellbonuses.HP, spellbonuses.Mana);
		client->Message(CC_Default, "Str:%i Sta:%i Dex:%i Agi:%i Int:%i Wis:%i Cha:%i",
			spellbonuses.STR,spellbonuses.STA,spellbonuses.DEX,spellbonuses.AGI,spellbonuses.INT,spellbonuses.WIS,spellbonuses.CHA);
		client->Message(CC_Default, "SvMagic:%i SvFire:%i SvCold:%i SvPoison:%i SvDisease:%i",
				spellbonuses.MR,spellbonuses.FR,spellbonuses.CR,spellbonuses.PR,spellbonuses.DR);
		client->Message(CC_Default, "DmgShield:%i Haste:%i", spellbonuses.DamageShield, spellbonuses.haste );
	}
}

void Mob::ShowBuffList(Client* client) {
	if(SPDAT_RECORDS <= 0)
		return;

	client->Message(CC_Default, "Buffs on: %s", this->GetCleanName());
	uint32 i;
	uint32 buff_count = GetMaxTotalSlots();
	for (i=0; i < buff_count; i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			client->Message(CC_Default, "  %i: %s", i, spells[buffs[i].spellid].name);
		}
	}
}

void Mob::GMMove(float x, float y, float z, float heading, bool SendUpdate) {

	Route.clear();

	if(IsNPC()) {
		entity_list.ProcessMove(CastToNPC(), x, y, z);
	}

	m_Position.x = x;
	m_Position.y = y;
	m_Position.z = z;
	if (m_Position.w != 0.01)
		this->m_Position.w = heading;
	if(SendUpdate)
		SendPosition();
}

void Mob::SetCurrentSpeed(float speed) {
	if (current_speed != speed)
	{ 
		current_speed = speed; 
		tar_ndx = 20;
		if (speed < 0.1f) {
			SetRunAnimSpeed(0);
			SetMoving(false);
			SendPosition();
		} else {
			move_tic_count = RuleI(Zone, NPCPositonUpdateTicCount);
		}
	} 
}

void Mob::SendIllusionPacket(uint16 in_race, uint8 in_gender, uint8 in_texture, uint8 in_helmtexture, uint8 in_haircolor, uint8 in_beardcolor, uint8 in_eyecolor1, uint8 in_eyecolor2, uint8 in_hairstyle, uint8 in_luclinface, uint8 in_beard, uint8 in_aa_title, float in_size) {

	uint16 BaseRace = GetBaseRace();

	if (in_race == 0) {
		this->race = BaseRace;
		if (in_gender == 0xFF)
			this->gender = GetBaseGender();
		else
			this->gender = in_gender;
	}
	else {
		this->race = in_race;
		if (in_gender == 0xFF) {
			uint8 tmp = Mob::GetDefaultGender(this->race, gender);
			if (tmp == 2)
				gender = 2;
			else if (gender == 2 && GetBaseGender() == 2)
				gender = tmp;
			else if (gender == 2)
				gender = GetBaseGender();
		}
		else
			gender = in_gender;
	}
	if (in_texture == 0xFF) {
		if (IsPlayableRace(in_race))
			this->texture = 0xFF;
		else
			this->texture = GetTexture();
	}
	else
		this->texture = in_texture;

	if (in_helmtexture == 0xFF) {
		if (IsPlayableRace(in_race))
			this->helmtexture = 0xFF;
		else if (in_texture != 0xFF)
			this->helmtexture = in_texture;
		else
			this->helmtexture = GetHelmTexture();
	}
	else
		this->helmtexture = in_helmtexture;

	if (in_haircolor == 0xFF)
		this->haircolor = GetHairColor();
	else
		this->haircolor = in_haircolor;

	if (in_beardcolor == 0xFF)
		this->beardcolor = GetBeardColor();
	else
		this->beardcolor = in_beardcolor;

	if (in_eyecolor1 == 0xFF)
		this->eyecolor1 = GetEyeColor1();
	else
		this->eyecolor1 = in_eyecolor1;

	if (in_eyecolor2 == 0xFF)
		this->eyecolor2 = GetEyeColor2();
	else
		this->eyecolor2 = in_eyecolor2;

	if (in_hairstyle == 0xFF)
		this->hairstyle = GetHairStyle();
	else
		this->hairstyle = in_hairstyle;

	if (in_luclinface == 0xFF)
		this->luclinface = GetLuclinFace();
	else
		this->luclinface = in_luclinface;

	if (in_beard == 0xFF)
		this->beard	= GetBeard();
	else
		this->beard = in_beard;

	this->aa_title = 0xFF;

	if (in_size <= 0.0f)
		this->size = GetSize();
	else
		this->size = in_size;

	// Forces the feature information to be pulled from the Player Profile
	if (this->IsClient() && in_race == 0) {
		this->race = CastToClient()->GetBaseRace();
		this->gender = CastToClient()->GetBaseGender();
		this->texture = 0xFF;
		this->helmtexture = 0xFF;
		this->haircolor = CastToClient()->GetBaseHairColor();
		this->beardcolor = CastToClient()->GetBaseBeardColor();
		this->eyecolor1 = CastToClient()->GetBaseEyeColor();
		this->eyecolor2 = CastToClient()->GetBaseEyeColor();
		this->hairstyle = CastToClient()->GetBaseHairStyle();
		this->luclinface = CastToClient()->GetBaseFace();
		this->beard	= CastToClient()->GetBaseBeard();
		this->aa_title = 0xFF;
		this->size = GetBaseSize();
	}

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Illusion, sizeof(Illusion_Struct));
	Illusion_Struct* is = (Illusion_Struct*) outapp->pBuffer;
	is->spawnid = this->GetID();
	strcpy(is->charname, GetCleanName());
	is->race = this->race;
	is->gender = this->gender;
	is->texture = this->texture;
	is->helmtexture = this->helmtexture;
	is->haircolor = this->haircolor;
	is->beardcolor = this->beardcolor;
	is->beard = this->beard;
	is->eyecolor1 = this->eyecolor1;
	is->eyecolor2 = this->eyecolor2;
	is->hairstyle = this->hairstyle;
	is->face = this->luclinface;
	//is->aa_title = this->aa_title;
	is->size = this->size;

	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);
	Log.Out(Logs::Detail, Logs::Spells, "Illusion: Race = %i, Gender = %i, Texture = %i, HelmTexture = %i, HairColor = %i, BeardColor = %i, EyeColor1 = %i, EyeColor2 = %i, HairStyle = %i, Face = %i, Size = %f",
		this->race, this->gender, this->texture, this->helmtexture, this->haircolor, this->beardcolor, this->eyecolor1, this->eyecolor2, this->hairstyle, this->luclinface, this->size);
}

uint8 Mob::GetDefaultGender(uint16 in_race, uint8 in_gender) {
	if (IsPlayableRace(in_race) ||
		in_race == 15 || in_race == 50 || in_race == 57 || in_race == 70 || in_race == 98 || in_race == 118) {
		if (in_gender >= 2) {
			// Female default for PC Races
			return 1;
		}
		else
			return in_gender;
	}
	else if (in_race == 44 || in_race == 52 || in_race == 55 || in_race == 65 || in_race == 67 || in_race == 88 || in_race == 117 || in_race == 127 ||
		in_race == 77 || in_race == 78 || in_race == 81 || in_race == 90 || in_race == 92 || in_race == 93 || in_race == 94 || in_race == 106 || in_race == 112 || in_race == 471) {
		// Male only races
		return 0;

	}
	else if (in_race == 25 || in_race == 56) {
		// Female only races
		return 1;
	}
	else {
		// Neutral default for NPC Races
		return 2;
	}
}

void Mob::SendAppearancePacket(uint32 type, uint32 value, bool WholeZone, bool iIgnoreSelf, Client *specific_target) {
	if (!GetID())
		return;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* appearance = (SpawnAppearance_Struct*)outapp->pBuffer;
	appearance->spawn_id = this->GetID();
	appearance->type = type;
	appearance->parameter = value;
	if (WholeZone)
		entity_list.QueueClients(this, outapp, iIgnoreSelf);
	else if(specific_target != nullptr)
		specific_target->QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
	else if (this->IsClient())
		this->CastToClient()->QueuePacket(outapp, false, Client::CLIENT_CONNECTED);
	safe_delete(outapp);
}

const int32& Mob::SetMana(int32 amount)
{
	CalcMaxMana();
	int32 mmana = GetMaxMana();
	cur_mana = amount < 0 ? 0 : (amount > mmana ? mmana : amount);
/*
	if(IsClient())
		Log.Out(Logs::Detail, Logs::Debug, "Setting mana for %s to %d (%4.1f%%)", GetName(), amount, GetManaRatio());
*/

	return cur_mana;
}


void Mob::SetAppearance(EmuAppearance app, bool iIgnoreSelf) {
	if (_appearance != app) {
		_appearance = app;
		SendAppearancePacket(AT_Anim, GetAppearanceValue(app), true, iIgnoreSelf);
		if (this->IsClient() && this->IsAIControlled())
			SendAppearancePacket(AT_Anim, ANIM_FREEZE, false, false);
	}
}

bool Mob::UpdateActiveLight()
{
	uint8 old_light_level = m_Light.Level.Active;

	m_Light.Type.Active = 0;
	m_Light.Level.Active = 0;

	if (m_Light.IsLevelGreater((m_Light.Type.Innate & 0x0F), m_Light.Type.Active)) { m_Light.Type.Active = m_Light.Type.Innate; }
	if (m_Light.Level.Equipment > m_Light.Level.Active) { m_Light.Type.Active = m_Light.Type.Equipment; } // limiter in property handler
	if (m_Light.Level.Spell > m_Light.Level.Active) { m_Light.Type.Active = m_Light.Type.Spell; } // limiter in property handler

	m_Light.Level.Active = m_Light.TypeToLevel(m_Light.Type.Active);

	return (m_Light.Level.Active != old_light_level);
}

void Mob::ChangeSize(float in_size = 0, bool bNoRestriction) {
	// Size Code
	if (!bNoRestriction)
	{
		if (this->IsClient() || this->petid != 0)
		{
			if (in_size < 3.0)
				in_size = 3.0;

			if (in_size > 15.0)
				in_size = 15.0;
		}
	}

	if (in_size < 1.0)
		in_size = 1.0;

	if (in_size > 255.0)
		in_size = 255.0;
	//End of Size Code
	this->size = in_size;
	uint32 newsize = floor(in_size + 0.5);
	this->z_offset = CalcZOffset();
	SendAppearancePacket(AT_Size, newsize);
}

Mob* Mob::GetOwnerOrSelf() {
	if (!GetOwnerID())
		return this;
	Mob* owner = entity_list.GetMob(this->GetOwnerID());
	if (!owner) {
		SetOwnerID(0);
		return(this);
	}
	if (owner->GetPetID() == this->GetID()) {
		return owner;
	}
	if(IsNPC() && CastToNPC()->GetSwarmInfo()){
		return (CastToNPC()->GetSwarmInfo()->GetOwner());
	}
	SetOwnerID(0);
	return this;
}

Mob* Mob::GetOwner() {
	Mob* owner = entity_list.GetMob(this->GetOwnerID());
	if (owner)
	{
		return owner;
	}
	if(IsNPC() && CastToNPC()->GetSwarmInfo()){
		return (CastToNPC()->GetSwarmInfo()->GetOwner());
	}
	SetOwnerID(0);
	return 0;
}

Mob* Mob::GetUltimateOwner()
{
	Mob* Owner = GetOwner();

	if(!Owner)
		return this;

	while(Owner && Owner->HasOwner())
		Owner = Owner->GetOwner();

	return Owner ? Owner : this;
}

void Mob::SetOwnerID(uint16 NewOwnerID) {
	if (NewOwnerID == GetID() && NewOwnerID != 0) // ok, no charming yourself now =p
		return;
	ownerid = NewOwnerID;
	if (ownerid == 0 && this->IsNPC() && this->GetPetType() != petCharmed)
		this->Depop();
}

// used in checking for behind (backstab) and checking in front (melee LoS)
float Mob::MobAngle(Mob *other, float ourx, float oury) const {
	if (!other || other == this)
		return 0.0f;

	float angle, lengthb, vectorx, vectory, dotp;
	float mobx = -(other->GetX());	// mob xloc (inverse because eq)
	float moby = other->GetY();		// mob yloc
	float heading = other->GetHeadingRadians();	// mob heading

	vectorx = mobx + (10.0f * cosf(heading));	// create a vector based on heading
	vectory = moby + (10.0f * sinf(heading));	// of mob length 10

	// length of mob to player vector
	lengthb = (float) sqrtf(((-ourx - mobx) * (-ourx - mobx)) + ((oury - moby) * (oury - moby)));

	// calculate dot product to get angle
	// Handle acos domain errors due to floating point rounding errors
	dotp = ((vectorx - mobx) * (-ourx - mobx) +
			(vectory - moby) * (oury - moby)) / (10 * lengthb);
	// I haven't seen any errors that  cause problems that weren't slightly
	// larger/smaller than 1/-1, so only handle these cases for now
	if (dotp > 1)
		return 0.0f;
	else if (dotp < -1)
		return 180.0f;

	angle = acosf(dotp);
	angle = angle * 180.0f / 3.1415f;

	return angle;
}

void Mob::SetZone(uint32 zone_id, uint32 instance_id)
{
	if(IsClient())
	{
		CastToClient()->GetPP().zone_id = zone_id;
		CastToClient()->GetPP().zoneInstance = instance_id;
		CastToClient()->Save();
	}
	Save();
}

void Mob::Kill() {
	Death(this, 0, SPELL_UNKNOWN, SkillHandtoHand);
}

bool Mob::CanThisClassDualWield(void) const {
	if(!IsClient()) {
		return(GetSkill(SkillDualWield) > 0);
	}
	else if(CastToClient()->HasSkill(SkillDualWield) || GetClass() == MONK) {
		const ItemInst* pinst = CastToClient()->GetInv().GetItem(MainPrimary);
		const ItemInst* sinst = CastToClient()->GetInv().GetItem(MainSecondary);

		// 2HS, 2HB, or 2HP
		if(pinst && pinst->IsWeapon()) {
			const Item_Struct* item = pinst->GetItem();

			if((item->ItemType == ItemType2HBlunt) || (item->ItemType == ItemType2HSlash) || (item->ItemType == ItemType2HPiercing))
				return false;
		}

		// OffHand Weapon
		if(sinst && !sinst->IsWeapon())
			return false;

		// Dual-Wielding Empty Fists
		if(!pinst && !sinst)
			if(class_ != MONK && class_ != MONKGM && class_ != BEASTLORD && class_ != BEASTLORDGM)
				return false;

		return true;
	}

	return false;
}

bool Mob::CanThisClassDoubleAttack(void) const
{
	if(!IsClient()) {
		return(GetSkill(SkillDoubleAttack) > 0);
	} else {
		if(aabonuses.GiveDoubleAttack || itembonuses.GiveDoubleAttack || spellbonuses.GiveDoubleAttack) {
			return true;
		}
		return(CastToClient()->HasSkill(SkillDoubleAttack));
	}
}

bool Mob::IsWarriorClass(void) const
{
	switch(GetClass())
	{
	case WARRIOR:
	case WARRIORGM:
	case ROGUE:
	case ROGUEGM:
	case MONK:
	case MONKGM:
	case PALADIN:
	case PALADINGM:
	case SHADOWKNIGHT:
	case SHADOWKNIGHTGM:
	case RANGER:
	case RANGERGM:
	case BEASTLORD:
	case BEASTLORDGM:
	case BARD:
	case BARDGM:
		{
			return true;
		}
	default:
		{
			return false;
		}
	}

}

bool Mob::CanThisClassParry(void) const
{
	if(!IsClient()) {
		return(GetSkill(SkillParry) > 0);
	} else {
		return(CastToClient()->HasSkill(SkillParry));
	}
}

bool Mob::CanThisClassDodge(void) const
{
	if(!IsClient()) {
		return(GetSkill(SkillDodge) > 0);
	} else {
		return(CastToClient()->HasSkill(SkillDodge));
	}
}

bool Mob::CanThisClassRiposte(void) const
{
	if(!IsClient()) {
		return(GetSkill(SkillRiposte) > 0);
	} else {
		return(CastToClient()->HasSkill(SkillRiposte));
	}
}

bool Mob::CanThisClassBlock(void) const
{
	if(!IsClient()) {
		return(GetSkill(SkillBlock) > 0);
	} else {
		return(CastToClient()->HasSkill(SkillBlock));
	}
}
/*
float Mob::GetReciprocalHeading(Mob* target) {
	float Result = 0;

	if(target) {
		// Convert to radians
		float h = (target->GetHeading() / 256.0f) * 6.283184f;

		// Calculate the reciprocal heading in radians
		Result = h + 3.141592f;

		// Convert back to eq heading from radians
		Result = (Result / 6.283184f) * 256.0f;
	}

	return Result;
}
*/
bool Mob::PlotPositionAroundTarget(Mob* target, float &x_dest, float &y_dest, float &z_dest, bool lookForAftArc) {
	bool Result = false;

	if(target) {
		float look_heading = 0;

		if(lookForAftArc)
			look_heading = GetReciprocalHeading(target->GetPosition());
		else
			look_heading = target->GetHeading();

		// Convert to sony heading to radians
		look_heading = (look_heading / 256.0f) * 6.283184f;

		float tempX = 0;
		float tempY = 0;
		float tempZ = 0;
		float tempSize = 0;
		const float rangeCreepMod = 0.25;
		const uint8 maxIterationsAllowed = 4;
		uint8 counter = 0;
		float rangeReduction= 0;

		tempSize = target->GetSize();
		rangeReduction = (tempSize * rangeCreepMod);

		while(tempSize > 0 && counter != maxIterationsAllowed) {
			tempX = GetX() + (tempSize * static_cast<float>(sin(double(look_heading))));
			tempY = GetY() + (tempSize * static_cast<float>(cos(double(look_heading))));
			tempZ = target->GetZ();

			if(!CheckLosFN(tempX, tempY, tempZ, tempSize)) {
				tempSize -= rangeReduction;
			}
			else {
				Result = true;
				break;
			}

			counter++;
		}

		if(!Result) {
			// Try to find an attack arc to position at from the opposite direction.
			look_heading += (3.141592 / 2);

			tempSize = target->GetSize();
			counter = 0;

			while(tempSize > 0 && counter != maxIterationsAllowed) {
				tempX = GetX() + (tempSize * static_cast<float>(sin(double(look_heading))));
				tempY = GetY() + (tempSize * static_cast<float>(cos(double(look_heading))));
				tempZ = target->GetZ();

				if(!CheckLosFN(tempX, tempY, tempZ, tempSize)) {
					tempSize -= rangeReduction;
				}
				else {
					Result = true;
					break;
				}

				counter++;
			}
		}

		if(Result) {
			x_dest = tempX;
			y_dest = tempY;
			z_dest = tempZ;
		}
	}

	return Result;
}

bool Mob::HateSummon() {
	// check if mob has ability to summon
	// 97% is the offical % that summoning starts on live, not 94
	// if the mob can summon and is charmed, it can only summon mobs it has LoS to
	Mob* mob_owner = nullptr;
	if(GetOwnerID())
		mob_owner = entity_list.GetMob(GetOwnerID());

	int summon_level = GetSpecialAbility(SPECATK_SUMMON);
	if(summon_level == 1 || summon_level == 2) {
		if(!GetTarget() || (mob_owner && mob_owner->IsClient() && !CheckLosFN(GetTarget()))) {
			return false;
		}
	} else {
		//unsupported summon level or OFF
		return false;
	}

	// validate hp
	int hp_ratio = GetSpecialAbilityParam(SPECATK_SUMMON, 1);
	hp_ratio = hp_ratio > 0 ? hp_ratio : 97;
	if(GetHPRatio() > static_cast<float>(hp_ratio)) {
		return false;
	}

	// now validate the timer
	int summon_timer_duration = GetSpecialAbilityParam(SPECATK_SUMMON, 0);
	summon_timer_duration = summon_timer_duration > 0 ? summon_timer_duration : 6000;
	Timer *timer = GetSpecialAbilityTimer(SPECATK_SUMMON);
	if (!timer)
	{
		StartSpecialAbilityTimer(SPECATK_SUMMON, summon_timer_duration);
	} else {
		if(!timer->Check())
			return false;

		timer->Start(summon_timer_duration);
	}

	// get summon target
	SetTarget(GetHateTop());
	if(target)
	{
		if(summon_level == 1) {
			entity_list.MessageClose(this, true, 500, MT_Say, "%s says,'You will not evade me, %s!' ", GetCleanName(), target->GetCleanName() );

			if (target->IsClient()) {
				target->CastToClient()->MovePC(zone->GetZoneID(), zone->GetInstanceID(), m_Position.x, m_Position.y, m_Position.z, target->GetHeading() * 2, 0, SummonPC);
			}
			else {
				target->GMMove(m_Position.x, m_Position.y, m_Position.z, target->GetHeading());
			}

			return true;
		} else if(summon_level == 2) {
			entity_list.MessageClose(this, true, 500, MT_Say, "%s says,'You will not evade me, %s!'", GetCleanName(), target->GetCleanName());
			GMMove(target->GetX(), target->GetY(), target->GetZ());
		}
	}
	return false;
}

void Mob::FaceTarget(Mob* MobToFace) {
	Mob* facemob = MobToFace;
	if(!facemob) {
		if(!GetTarget()) {
			return;
		}
		else {
			facemob = GetTarget();
		}
	}

	float oldheading = GetHeading();
	float newheading = CalculateHeadingToTarget(facemob->GetX(), facemob->GetY());
	if(oldheading != newheading) {
		SetHeading(newheading);
		if(moving)
			SendPosUpdate();
		else
		{
			SendPosition();
		}
	}

	if(IsNPC() && !IsEngaged()) {
		CastToNPC()->GetRefaceTimer()->Start(15000);
		CastToNPC()->GetRefaceTimer()->Enable();
	}
}

bool Mob::RemoveFromHateList(Mob* mob)
{
	SetRunAnimSpeed(0);
	bool bFound = false;
	if(IsEngaged())
	{
		bFound = hate_list.RemoveEnt(mob);
		if(hate_list.IsEmpty())
		{
			AI_Event_NoLongerEngaged();
			zone->DelAggroMob();
			if(IsNPC() && !RuleB(AlKabor, AllowTickSplit))
			{
				ResetAssistCap();
			}
		}
	}
	if(GetTarget() == mob)
	{
		SetTarget(hate_list.GetTop());
	}

	return bFound;
}

void Mob::WipeHateList()
{
	if(IsEngaged())
	{
		hate_list.Wipe();
		SetTarget(nullptr);
		AI_Event_NoLongerEngaged();
	}
	else
	{
		hate_list.Wipe();
	}
}

bool Mob::SetHeading2(float iHeading)
{
	if (GetHeading() != iHeading)
	{
		float diff = GetHeading() - iHeading;
		if (abs(diff) < 13 || abs(diff) > 243)
		{
			// close enough, so snap to heading
			m_Delta.w = 0.0f;
			SetHeading(iHeading);
			SendPosition();
			return true;
		}
		moved = true;
		while (diff < 0)
			diff += 256;
		while (diff >= 256.0)
			diff -= 256.0f;
		float delta_heading = ((256 - diff) < diff) ? 12.0f : -12.0f;
		if (IsBoat())
			delta_heading /= 2.0f;

		bool send_update = false;
		if (m_Delta.w == 0.0f)
			send_update = true;

		m_Delta = glm::vec4(0.0, 0.0f, 0.0f, delta_heading);
		if (send_update)
			SendPosUpdate(1);

		float new_heading = GetHeading() + delta_heading;

		if (new_heading < 0.0f)
			new_heading += 256.0f;
		if (new_heading >= 256.0f)
			new_heading -= 256.0f;

		SetHeading(new_heading);
		return false;
	} 
	if (m_Delta.w != 0.0f) {
		m_Delta.w = 0.0f;
		SendPosition();
	}
	return true;
}

uint32 Mob::RandomTimer(int min,int max) {
	int r = 14000;
	if(min != 0 && max != 0 && min < max)
	{
		r = zone->random.Int(min, max);
	}
	return r;
}

uint32 NPC::GetEquipment(uint8 material_slot) const
{
	if(material_slot > 8)
		return 0;
	int invslot = Inventory::CalcSlotFromMaterial(material_slot);
	if (invslot == -1)
		return 0;
	return equipment[invslot];
}

void Mob::SendWearChange(uint8 material_slot)
{
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	wc->spawn_id = GetID();
	wc->material = GetEquipmentMaterial(material_slot);
	wc->color.color = GetEquipmentColor(material_slot);
	wc->wear_slot_id = material_slot;

	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);
}

void Mob::SendTextureWC(uint8 slot, uint16 texture, uint32 hero_forge_model, uint32 elite_material, uint32 unknown06, uint32 unknown18)
{
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	wc->spawn_id = this->GetID();
	wc->material = texture;
	if (this->IsClient())
		wc->color.color = GetEquipmentColor(slot);
	else
		wc->color.color = this->GetArmorTint(slot);
	wc->wear_slot_id = slot;

	wc->unknown06 = unknown06;
	wc->elite_material = elite_material;
	wc->hero_forge_model = hero_forge_model;
	wc->unknown18 = unknown18;


	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);
}

void Mob::SetSlotTint(uint8 material_slot, uint8 red_tint, uint8 green_tint, uint8 blue_tint)
{
	uint32 color;
	color = (red_tint & 0xFF) << 16;
	color |= (green_tint & 0xFF) << 8;
	color |= (blue_tint & 0xFF);
	color |= (color) ? (0xFF << 24) : 0;
	armor_tint[material_slot] = color;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	wc->spawn_id = this->GetID();
	wc->material = GetEquipmentMaterial(material_slot);
	wc->color.color = color;
	wc->wear_slot_id = material_slot;

	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);
}

void Mob::WearChange(uint8 material_slot, uint16 texture, uint32 color)
{
	armor_tint[material_slot] = color;

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	WearChange_Struct* wc = (WearChange_Struct*)outapp->pBuffer;

	wc->spawn_id = this->GetID();
	wc->material = texture;
	wc->color.color = color;
	wc->wear_slot_id = material_slot;

	entity_list.QueueClients(this, outapp);
	safe_delete(outapp);
}

int32 Mob::GetEquipmentMaterial(uint8 material_slot) const
{
	const Item_Struct *item;
	item = database.GetItem(GetEquipment(material_slot));
	if(item != 0)
	{
		if	// for primary and secondary we need the model, not the material
		(
			material_slot == MaterialPrimary ||
			material_slot == MaterialSecondary
		)
		{
			if (this->IsClient()){
				if (strlen(item->IDFile) > 2)
					return atoi(&item->IDFile[2]);
				else	//may as well try this, since were going to 0 anyways
					return item->Material;
			}
			else {
				if (strlen(item->IDFile) > 2)
					return atoi(&item->IDFile[2]);
				else	//may as well try this, since were going to 0 anyways
					return item->Material;
			}
		}
		else
		{
			return item->Material;
		}
	}
	else if(IsNPC())
	{
		if(material_slot == MaterialHead)
			return helmtexture;
		else if(material_slot == MaterialChest)
			return chesttexture;
		else if(material_slot == MaterialArms)
			return armtexture;
		else if(material_slot == MaterialWrist)
			return bracertexture;
		else if(material_slot == MaterialHands)
			return handtexture;
		else if(material_slot == MaterialLegs)
			return legtexture;
		else if(material_slot == MaterialFeet)
			return feettexture;
	}

	return 0;
}

uint32 Mob::GetEquipmentColor(uint8 material_slot) const
{
	const Item_Struct *item;

	item = database.GetItem(GetEquipment(material_slot));
	if(item != 0)
	{
		return item->Color;
	}

	return 0;
}

// works just like a printf
void Mob::Say(const char *format, ...)
{
	char buf[1000];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, 1000, format, ap);
	va_end(ap);

	Mob* talker = this;
	if(spellbonuses.VoiceGraft != 0) {
		if(spellbonuses.VoiceGraft == GetPetID())
			talker = entity_list.GetMob(spellbonuses.VoiceGraft);
		else
			spellbonuses.VoiceGraft = 0;
	}

	if(!talker)
		talker = this;

	entity_list.MessageClose_StringID(talker, false, 200, 10,
		GENERIC_SAY, GetCleanName(), buf);
}

//
// solar: this is like the above, but the first parameter is a string id
//
void Mob::Say_StringID(uint32 string_id, const char *message3, const char *message4, const char *message5, const char *message6, const char *message7, const char *message8, const char *message9)
{
	char string_id_str[10];

	snprintf(string_id_str, 10, "%d", string_id);

	entity_list.MessageClose_StringID(this, false, 200, 10,
		GENERIC_STRINGID_SAY, GetCleanName(), string_id_str, message3, message4, message5,
		message6, message7, message8, message9
	);
}

void Mob::Say_StringID(uint32 type, uint32 string_id, const char *message3, const char *message4, const char *message5, const char *message6, const char *message7, const char *message8, const char *message9)
{
	char string_id_str[10];

	snprintf(string_id_str, 10, "%d", string_id);

	entity_list.MessageClose_StringID(this, false, 200, type,
		GENERIC_STRINGID_SAY, GetCleanName(), string_id_str, message3, message4, message5,
		message6, message7, message8, message9
	);
}

void Mob::Shout(const char *format, ...)
{
	char buf[1000];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, 1000, format, ap);
	va_end(ap);

	entity_list.Message_StringID(this, false, MT_Shout,
		GENERIC_SHOUT, GetCleanName(), buf);
}

void Mob::Emote(const char *format, ...)
{
	char buf[1000];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, 1000, format, ap);
	va_end(ap);

	entity_list.MessageClose_StringID(this, false, 200, 10,
		GENERIC_EMOTE, GetCleanName(), buf);
}

void Mob::QuestJournalledSay(Client *QuestInitiator, const char *str)
{
		entity_list.QuestJournalledSayClose(this, QuestInitiator, 200, GetCleanName(), str);
}

const char *Mob::GetCleanName()
{
	if(!strlen(clean_name))
	{
		CleanMobName(GetName(), clean_name);
	}

	return clean_name;
}

// hp event
void Mob::SetNextHPEvent( int hpevent )
{
	nexthpevent = hpevent;
}

void Mob::SetNextIncHPEvent( int inchpevent )
{
	nextinchpevent = inchpevent;
}
//warp for quest function,from sandy
void Mob::Warp(const glm::vec3& location)
{
	if(IsNPC())
		entity_list.ProcessMove(CastToNPC(), location.x, location.y, location.z);

	m_Position = glm::vec4(location, m_Position.w);

	Mob* target = GetTarget();
	if (target)
		FaceTarget( target );

	SendPosition();
}

int16 Mob::GetResist(uint8 type) const
{
	if (IsNPC())
	{
		if (type == 1)
			return MR + spellbonuses.MR + itembonuses.MR;
		else if (type == 2)
			return FR + spellbonuses.FR + itembonuses.FR;
		else if (type == 3)
			return CR + spellbonuses.CR + itembonuses.CR;
		else if (type == 4)
			return PR + spellbonuses.PR + itembonuses.PR;
		else if (type == 5)
			return DR + spellbonuses.DR + itembonuses.DR;
	}
	else if (IsClient())
	{
		if (type == 1)
			return CastToClient()->GetMR();
		else if (type == 2)
			return CastToClient()->GetFR();
		else if (type == 3)
			return CastToClient()->GetCR();
		else if (type == 4)
			return CastToClient()->GetPR();
		else if (type == 5)
			return CastToClient()->GetDR();
	}
	return 25;
}

uint32 Mob::GetLevelHP(uint8 tlevel)
{
	int multiplier = 0;
	if (tlevel < 10)
	{
		multiplier = tlevel*20;
	}
	else if (tlevel < 20)
	{
		multiplier = tlevel*25;
	}
	else if (tlevel < 40)
	{
		multiplier = tlevel*tlevel*12*((tlevel*2+60)/100)/10;
	}
	else if (tlevel < 45)
	{
		multiplier = tlevel*tlevel*15*((tlevel*2+60)/100)/10;
	}
	else if (tlevel < 50)
	{
		multiplier = tlevel*tlevel*175*((tlevel*2+60)/100)/100;
	}
	else
	{
		multiplier = tlevel*tlevel*2*((tlevel*2+60)/100)*(1+((tlevel-50)*20/10));
	}
	return multiplier;
}

int32 Mob::GetActSpellCasttime(uint16 spell_id, int32 casttime) {
	if (level >= 60 && casttime > 1000)
	{
		casttime = casttime / 2;
		if (casttime < 1000)
			casttime = 1000;
	} else if (level >= 50 && casttime > 1000) {
		int32 cast_deduction = (casttime*(level - 49))/5;
		if (cast_deduction > casttime/2)
			casttime /= 2;
		else
			casttime -= cast_deduction;
	}
	return(casttime);
}

void Mob::ExecWeaponProc(const ItemInst *inst, uint16 spell_id, Mob *on) {
	// Changed proc targets to look up based on the spells goodEffect flag.
	// This should work for the majority of weapons.
	if(spell_id == SPELL_UNKNOWN || on->GetSpecialAbility(NO_HARM_FROM_CLIENT)) {
		//This is so 65535 doesn't get passed to the client message and to logs because it is not relavant information for debugging.
		return;
	}

	if (IsNoCast())
		return;

	if(spell_id == 0)
		return;

	if(!IsValidSpell(spell_id)) { // Check for a valid spell otherwise it will crash through the function
		if(IsClient()){
			Message(CC_Default, "Invalid spell proc %u", spell_id);
			Log.Out(Logs::Detail, Logs::Spells, "Player %s, Weapon Procced invalid spell %u", this->GetName(), spell_id);
		}
		return;
	}

	if(inst && IsClient()) {
		//const cast is dirty but it would require redoing a ton of interfaces at this point
		//It should be safe as we don't have any truly const ItemInst floating around anywhere.
		//So we'll live with it for now
		int i = parse->EventItem(EVENT_WEAPON_PROC, CastToClient(), const_cast<ItemInst*>(inst), on, "", spell_id);
		if(i != 0) {
			return;
		}
	}

	if ((inst && inst->GetID() == 14811) ||	// Iron Bound Tome was bugged during this era and would proc on self
		(IsBeneficialSpell(spell_id) &&
		(!IsNPC() || CastToNPC()->GetInnateProcSpellId() != spell_id)))		// innate NPC procs always hit the target
	{
		SpellFinished(spell_id, this, 10, 0, -1, spells[spell_id].ResistDiff, true);
	}
	else if(!(on->IsClient() && on->CastToClient()->dead)) //dont proc on dead clients
	{ 
		SpellFinished(spell_id, on, 10, 0, -1, spells[spell_id].ResistDiff, true);
	}
	return;
}

uint32 Mob::GetZoneID() const {
	return(zone->GetZoneID());
}

int Mob::GetHaste()
{
	// See notes in Client::CalcHaste
	// Need to check if the effect of inhibit melee differs for NPCs
	if (spellbonuses.haste < 0) {
		if (-spellbonuses.haste <= spellbonuses.inhibitmelee)
			return 100 - spellbonuses.inhibitmelee;
		else
			return 100 + spellbonuses.haste;
	}

	if (spellbonuses.haste == 0 && spellbonuses.inhibitmelee)
		return 100 - spellbonuses.inhibitmelee;

	int h = 0;
	int cap = 0;
	int level = GetLevel();

	if (spellbonuses.haste)
		h += spellbonuses.haste - spellbonuses.inhibitmelee;
	if (spellbonuses.hastetype2 && level > 49)
		h += spellbonuses.hastetype2 > 10 ? 10 : spellbonuses.hastetype2;

	// 26+ no cap, 1-25 10
	if (level > 25) // 26+
		h += itembonuses.haste;
	else // 1-25
		h += itembonuses.haste > 10 ? 10 : itembonuses.haste;

	// 60+ 100, 51-59 85, 1-50 level+25
	if (level > 59) // 60+
		cap = RuleI(Character, HasteCap);
	else if (level > 50) // 51-59
		cap = 85;
	else // 1-50
		cap = level + 25;

	if(h > cap)
		h = cap;

	// 51+ 25 (despite there being higher spells...), 1-50 10
	if (level > 50) // 51+
		h += spellbonuses.hastetype3 > 25 ? 25 : spellbonuses.hastetype3;
	else // 1-50
		h += spellbonuses.hastetype3 > 10 ? 10 : spellbonuses.hastetype3;

	h += ExtraHaste;	//GM granted haste.

	return 100 + h;
}

void Mob::SetTarget(Mob* mob) {
	if (target == mob) return;
	target = mob;
	if(IsNPC())
		parse->EventNPC(EVENT_TARGET_CHANGE, CastToNPC(), mob, "", 0);
	else if (IsClient())
		parse->EventPlayer(EVENT_TARGET_CHANGE, CastToClient(), "", 0);
	
	return;
}

float Mob::FindGroundZ(float new_x, float new_y, float z_offset)
{
	float ret = BEST_Z_INVALID;
	if (zone->zonemap != nullptr)
	{
		glm::vec3 me;
		me.x = new_x;
		me.y = new_y;
		me.z = m_Position.z + z_offset;
		glm::vec3 hit;
		float best_z = zone->zonemap->FindBestZ(me, &hit);
		if (best_z != BEST_Z_INVALID)
		{
			ret = best_z;
		}
	}
	return ret;
}

// Copy of above function that isn't protected to be exported to Perl::Mob
float Mob::GetGroundZ(float new_x, float new_y, float z_offset)
{
	float ret = BEST_Z_INVALID;
	if (zone->zonemap != 0)
	{
		glm::vec3 me;
		me.x = new_x;
		me.y = new_y;
		me.z = m_Position.z + z_offset;
		glm::vec3 hit;
		float best_z = zone->zonemap->FindBestZ(me, &hit);
		if (best_z != BEST_Z_INVALID)
		{
			ret = best_z;
		}
	}
	return ret;
}

//helper function for npc AI; needs to be mob:: cause we need to be able to count buffs on other clients and npcs
int Mob::CountDispellableBuffs()
{
	int val = 0;
	int buff_count = GetMaxTotalSlots();
	for(int x = 0; x < buff_count; x++)
	{
		if(!IsValidSpell(buffs[x].spellid))
			continue;

		if(buffs[x].counters)
			continue;

		if(spells[buffs[x].spellid].goodEffect == 0)
			continue;

		if(buffs[x].spellid != SPELL_UNKNOWN &&	spells[buffs[x].spellid].buffdurationformula != DF_Permanent)
			val++;
	}
	return val;
}

// Returns the % that a mob is snared (as a positive value). -1 means not snared
int Mob::GetSnaredAmount()
{
	int worst_snare = -1;

	int buff_count = GetMaxTotalSlots();
	for (int i = 0; i < buff_count; i++)
	{
		if (!IsValidSpell(buffs[i].spellid))
			continue;

		for(int j = 0; j < EFFECT_COUNT; j++)
		{
			if (spells[buffs[i].spellid].effectid[j] == SE_MovementSpeed)
			{
				int val = CalcSpellEffectValue_formula(spells[buffs[i].spellid].formula[j], spells[buffs[i].spellid].base[j], spells[buffs[i].spellid].max[j], buffs[i].casterlevel, buffs[i].spellid);
				//int effect = CalcSpellEffectValue(buffs[i].spellid, spells[buffs[i].spellid].effectid[j], buffs[i].casterlevel);
				if (val < 0 && std::abs(val) > worst_snare)
					worst_snare = std::abs(val);
			}
		}
	}

	return worst_snare;
}

void Mob::SetEntityVariable(const char *id, const char *m_var)
{
	std::string n_m_var = m_var;
	m_EntityVariables[id] = n_m_var;
}

const char* Mob::GetEntityVariable(const char *id)
{
	std::map<std::string, std::string>::iterator iter = m_EntityVariables.find(id);
	if(iter != m_EntityVariables.end())
	{
		return iter->second.c_str();
	}
	return nullptr;
}

bool Mob::EntityVariableExists(const char *id)
{
	std::map<std::string, std::string>::iterator iter = m_EntityVariables.find(id);
	if(iter != m_EntityVariables.end())
	{
		return true;
	}
	return false;
}

void Mob::SetFlyMode(uint8 flymode)
{
	if(IsClient() && flymode >= 0 && flymode < 3)
	{
		this->SendAppearancePacket(AT_Levitate, flymode);
	}
	else if(IsNPC() && flymode >= 0 && flymode <= 3)
	{
		this->SendAppearancePacket(AT_Levitate, flymode);
		this->CastToNPC()->SetFlyMode(flymode);
	}
}

bool Mob::IsNimbusEffectActive(uint32 nimbus_effect)
{
	if(nimbus_effect1 == nimbus_effect || nimbus_effect2 == nimbus_effect || nimbus_effect3 == nimbus_effect)
	{
		return true;
	}
	return false;
}

void Mob::SetNimbusEffect(uint32 nimbus_effect)
{
	if(nimbus_effect1 == 0)
	{
		nimbus_effect1 = nimbus_effect;
	}
	else if(nimbus_effect2 == 0)
	{
		nimbus_effect2 = nimbus_effect;
	}
	else
	{
		nimbus_effect3 = nimbus_effect;
	}
}

void Mob::TryTriggerOnValueAmount(bool IsHP, bool IsMana, bool IsEndur, bool IsPet)
{
	/*
	At present time there is no obvious difference between ReqTarget and ReqCaster
	ReqTarget is typically used in spells cast on a target where the trigger occurs on that target.
	ReqCaster is typically self only spells where the triggers on self.
	Regardless both trigger on the owner of the buff.
	*/

	/*
	Base2 Range: 1004	 = Below < 80% HP
	Base2 Range: 500-520 = Below (base2 - 500)*5 HP
	Base2 Range: 521	 = Below (?) Mana UKNOWN - Will assume its 20% unless proven otherwise
	Base2 Range: 522	 = Below (40%) Endurance
	Base2 Range: 523	 = Below (40%) Mana
	Base2 Range: 220-?	 = Number of pets on hatelist to trigger (base2 - 220) (Set at 30 pets max for now)
	38311 = < 10% mana;
	*/

	if (!spellbonuses.TriggerOnValueAmount)
		return;

	if (spellbonuses.TriggerOnValueAmount){

		int buff_count = GetMaxTotalSlots();

		for (int e = 0; e < buff_count; e++){

			uint32 spell_id = buffs[e].spellid;

			if (IsValidSpell(spell_id)){

				for (int i = 0; i < EFFECT_COUNT; i++){

					if ((spells[spell_id].effectid[i] == SE_TriggerOnReqTarget) || (spells[spell_id].effectid[i] == SE_TriggerOnReqCaster)) {

						int base2 = spells[spell_id].base2[i];
						bool use_spell = false;

						if (IsHP){
							if ((base2 >= 500 && base2 <= 520) && GetHPRatio() < (base2 - 500) * 5)
								use_spell = true;

							else if (base2 = 1004 && GetHPRatio() < 80)
								use_spell = true;
						}

						else if (IsMana){
							if ((base2 = 521 && GetManaRatio() < 20) || (base2 = 523 && GetManaRatio() < 40))
								use_spell = true;

							else if (base2 = 38311 && GetManaRatio() < 10)
								use_spell = true;
						}

						else if (IsEndur){
							if (base2 = 522 && GetEndurancePercent() < 40){
								use_spell = true;
							}
						}

						else if (IsPet){
							int count = hate_list.SummonedPetCount(this);
							if ((base2 >= 220 && base2 <= 250) && count >= (base2 - 220)){
								use_spell = true;
							}
						}

						if (use_spell){
							SpellFinished(spells[spell_id].base[i], this, 10, 0, -1, spells[spell_id].ResistDiff);

							if (!TryFadeEffect(e))
								BuffFadeBySlot(e);
						}
					}
				}
			}
		}
	}
}

int32 Mob::GetVulnerability(Mob* caster, uint32 spell_id, uint32 ticsremaining)
{
	if (!IsValidSpell(spell_id))
		return 0;

	if (!caster)
		return 0;

	int32 value = 0;

	//Apply innate vulnerabilities
	if (Vulnerability_Mod[GetSpellResistType(spell_id)] != 0)
		value = Vulnerability_Mod[GetSpellResistType(spell_id)];


	else if (Vulnerability_Mod[HIGHEST_RESIST+1] != 0)
		value = Vulnerability_Mod[HIGHEST_RESIST+1];

	//Apply spell derived vulnerabilities
	if (spellbonuses.FocusEffects[focusSpellVulnerability]){

		int32 tmp_focus = 0;
		int tmp_buffslot = -1;

		int buff_count = GetMaxTotalSlots();
		for(int i = 0; i < buff_count; i++) {

			if((IsValidSpell(buffs[i].spellid) && IsEffectInSpell(buffs[i].spellid, SE_FcSpellVulnerability))){

				int32 focus = caster->CalcFocusEffect(focusSpellVulnerability, buffs[i].spellid, spell_id);

				if (!focus)
					continue;

				if (tmp_focus && focus > tmp_focus){
					tmp_focus = focus;
					tmp_buffslot = i;
				}

				else if (!tmp_focus){
					tmp_focus = focus;
					tmp_buffslot = i;
				}

			}
		}

		if (tmp_focus < -99)
			tmp_focus = -99;

		value += tmp_focus;

		if (tmp_buffslot >= 0)
			CheckNumHitsRemaining(NUMHIT_MatchingSpells, tmp_buffslot);
	}
	return value;
}

int16 Mob::GetSkillDmgTaken(const SkillUseTypes skill_used)
{
	int skilldmg_mod = 0;

	int16 MeleeVuln = spellbonuses.MeleeVulnerability + itembonuses.MeleeVulnerability + aabonuses.MeleeVulnerability;

	// All skill dmg mod + Skill specific
	skilldmg_mod += itembonuses.SkillDmgTaken[HIGHEST_SKILL+1] + spellbonuses.SkillDmgTaken[HIGHEST_SKILL+1] +
					itembonuses.SkillDmgTaken[skill_used] + spellbonuses.SkillDmgTaken[skill_used];

	//Innate SetSkillDamgeTaken(skill,value)
	if ((SkillDmgTaken_Mod[skill_used]) || (SkillDmgTaken_Mod[HIGHEST_SKILL+1]))
		skilldmg_mod += SkillDmgTaken_Mod[skill_used] + SkillDmgTaken_Mod[HIGHEST_SKILL+1];

	skilldmg_mod += MeleeVuln;

	if(skilldmg_mod < -100)
		skilldmg_mod = -100;

	return skilldmg_mod;
}

int16 Mob::GetHealRate(uint16 spell_id, Mob* caster) {

	int16 heal_rate = 0;

	heal_rate += itembonuses.HealRate + spellbonuses.HealRate + aabonuses.HealRate;
	heal_rate += GetFocusIncoming(focusFcHealPctIncoming, SE_FcHealPctIncoming, caster, spell_id);

	if(heal_rate < -99)
		heal_rate = -99;

	return heal_rate;
}

bool Mob::TryFadeEffect(int slot)
{
	if(IsValidSpell(buffs[slot].spellid))
	{
		for(int i = 0; i < EFFECT_COUNT; i++)
		{
			if (spells[buffs[slot].spellid].effectid[i] == SE_CastOnFadeEffectAlways ||
				spells[buffs[slot].spellid].effectid[i] == SE_CastOnRuneFadeEffect)
			{
				uint16 spell_id = spells[buffs[slot].spellid].base[i];
				BuffFadeBySlot(slot);

				if(spell_id)
				{

					if(spell_id == SPELL_UNKNOWN)
						return false;

					if(IsValidSpell(spell_id))
					{
						if (IsBeneficialSpell(spell_id)) {
							SpellFinished(spell_id, this, 10, 0, -1, spells[spell_id].ResistDiff);
						}
						else if(!(IsClient() && CastToClient()->dead)) {
							SpellFinished(spell_id, this, 10, 0, -1, spells[spell_id].ResistDiff);
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}

int32 Mob::GetItemStat(uint32 itemid, const char *identifier)
{
	const ItemInst* inst = database.CreateItem(itemid);
	if (!inst)
		return 0;

	const Item_Struct* item = inst->GetItem();
	if (!item)
		return 0;

	if (!identifier)
		return 0;

	int32 stat = 0;

	std::string id = identifier;
	for(uint32 i = 0; i < id.length(); ++i)
	{
		id[i] = tolower(id[i]);
	}

	if (id == "itemclass")
		stat = int32(item->ItemClass);
	if (id == "id")
		stat = int32(item->ID);
	if (id == "idfile")
		stat = atoi(&item->IDFile[2]);
	if (id == "weight")
		stat = int32(item->Weight);
	if (id == "norent")
		stat = int32(item->NoRent);
	if (id == "nodrop")
		stat = int32(item->NoDrop);
	if (id == "size")
		stat = int32(item->Size);
	if (id == "slots")
		stat = int32(item->Slots);
	if (id == "price")
		stat = int32(item->Price);
	if (id == "icon")
		stat = int32(item->Icon);
	if (id == "fvnodrop")
		stat = int32(item->FVNoDrop);
	if (id == "bagtype")
		stat = int32(item->BagType);
	if (id == "bagslots")
		stat = int32(item->BagSlots);
	if (id == "bagsize")
		stat = int32(item->BagSize);
	if (id == "bagwr")
		stat = int32(item->BagWR);
	if (id == "benefitflag")
		stat = int32(item->BenefitFlag);
	if (id == "tradeskills")
		stat = int32(item->Tradeskills);
	if (id == "cr")
		stat = int32(item->CR);
	if (id == "dr")
		stat = int32(item->DR);
	if (id == "pr")
		stat = int32(item->PR);
	if (id == "mr")
		stat = int32(item->MR);
	if (id == "fr")
		stat = int32(item->FR);
	if (id == "astr")
		stat = int32(item->AStr);
	if (id == "asta")
		stat = int32(item->ASta);
	if (id == "aagi")
		stat = int32(item->AAgi);
	if (id == "adex")
		stat = int32(item->ADex);
	if (id == "acha")
		stat = int32(item->ACha);
	if (id == "aint")
		stat = int32(item->AInt);
	if (id == "awis")
		stat = int32(item->AWis);
	if (id == "hp")
		stat = int32(item->HP);
	if (id == "mana")
		stat = int32(item->Mana);
	if (id == "ac")
		stat = int32(item->AC);
	if (id == "deity")
		stat = int32(item->Deity);
	if (id == "skillmodvalue")
		stat = int32(item->SkillModValue);
	if (id == "skillmodtype")
		stat = int32(item->SkillModType);
	if (id == "banedmgrace")
		stat = int32(item->BaneDmgRace);
	if (id == "banedmgamt")
		stat = int32(item->BaneDmgAmt);
	if (id == "banedmgbody")
		stat = int32(item->BaneDmgBody);
	if (id == "magic")
		stat = int32(item->Magic);
	if (id == "casttime_")
		stat = int32(item->CastTime_);
	if (id == "reqlevel")
		stat = int32(item->ReqLevel);
	if (id == "bardtype")
		stat = int32(item->BardType);
	if (id == "bardvalue")
		stat = int32(item->BardValue);
	if (id == "light")
		stat = int32(item->Light);
	if (id == "delay")
		stat = int32(item->Delay);
	if (id == "reclevel")
		stat = int32(item->RecLevel);
	if (id == "recskill")
		stat = int32(item->RecSkill);
	if (id == "elemdmgtype")
		stat = int32(item->ElemDmgType);
	if (id == "elemdmgamt")
		stat = int32(item->ElemDmgAmt);
	if (id == "range")
		stat = int32(item->Range);
	if (id == "damage")
		stat = int32(item->Damage);
	if (id == "color")
		stat = int32(item->Color);
	if (id == "classes")
		stat = int32(item->Classes);
	if (id == "races")
		stat = int32(item->Races);
	if (id == "maxcharges")
		stat = int32(item->MaxCharges);
	if (id == "itemtype")
		stat = int32(item->ItemType);
	if (id == "material")
		stat = int32(item->Material);
	if (id == "casttime")
		stat = int32(item->CastTime);
	if (id == "procrate")
		stat = int32(item->ProcRate);
	if (id == "charmfileid")
		stat = int32(item->CharmFileID);
	if (id == "factionmod1")
		stat = int32(item->FactionMod1);
	if (id == "factionmod2")
		stat = int32(item->FactionMod2);
	if (id == "factionmod3")
		stat = int32(item->FactionMod3);
	if (id == "factionmod4")
		stat = int32(item->FactionMod4);
	if (id == "factionamt1")
		stat = int32(item->FactionAmt1);
	if (id == "factionamt2")
		stat = int32(item->FactionAmt2);
	if (id == "factionamt3")
		stat = int32(item->FactionAmt3);
	if (id == "factionamt4")
		stat = int32(item->FactionAmt4);
	if (id == "recastdelay")
		stat = int32(item->RecastDelay);
	if (id == "recasttype")
		stat = int32(item->RecastType);
	if (id == "nopet")
		stat = int32(item->NoPet);
	if (id == "stackable")
		stat = int32(item->Stackable);
	if (id == "notransfer")
		stat = int32(item->NoTransfer);
	if (id == "questitemflag")
		stat = int32(item->QuestItemFlag);
	if (id == "stacksize")
		stat = int32(item->StackSize);
	if (id == "book")
		stat = int32(item->Book);
	if (id == "booktype")
		stat = int32(item->BookType);
	// Begin Effects
	if (id == "clickeffect")
		stat = int32(item->Click.Effect);
	if (id == "clicktype")
		stat = int32(item->Click.Type);
	if (id == "clicklevel")
		stat = int32(item->Click.Level);
	if (id == "clicklevel2")
		stat = int32(item->Click.Level2);
	if (id == "proceffect")
		stat = int32(item->Proc.Effect);
	if (id == "proctype")
		stat = int32(item->Proc.Type);
	if (id == "proclevel")
		stat = int32(item->Proc.Level);
	if (id == "proclevel2")
		stat = int32(item->Proc.Level2);
	if (id == "worneffect")
		stat = int32(item->Worn.Effect);
	if (id == "worntype")
		stat = int32(item->Worn.Type);
	if (id == "wornlevel")
		stat = int32(item->Worn.Level);
	if (id == "wornlevel2")
		stat = int32(item->Worn.Level2);
	if (id == "focuseffect")
		stat = int32(item->Focus.Effect);
	if (id == "focustype")
		stat = int32(item->Focus.Type);
	if (id == "focuslevel")
		stat = int32(item->Focus.Level);
	if (id == "focuslevel2")
		stat = int32(item->Focus.Level2);
	if (id == "scrolleffect")
		stat = int32(item->Scroll.Effect);
	if (id == "scrolltype")
		stat = int32(item->Scroll.Type);
	if (id == "scrolllevel")
		stat = int32(item->Scroll.Level);
	if (id == "scrolllevel2")
		stat = int32(item->Scroll.Level2);

	safe_delete(inst);
	return stat;
}

std::string Mob::GetGlobal(const char *varname) {
	int qgCharid = 0;
	int qgNpcid = 0;
	
	if (this->IsNPC())
		qgNpcid = this->GetNPCTypeID();
	
	if (this->IsClient())
		qgCharid = this->CastToClient()->CharacterID();
	
	QGlobalCache *qglobals = nullptr;
	std::list<QGlobal> globalMap;
	
	if (this->IsClient())
		qglobals = this->CastToClient()->GetQGlobals();
	
	if (this->IsNPC())
		qglobals = this->CastToNPC()->GetQGlobals();

	if(qglobals)
		QGlobalCache::Combine(globalMap, qglobals->GetBucket(), qgNpcid, qgCharid, zone->GetZoneID());
	
	std::list<QGlobal>::iterator iter = globalMap.begin();
	while(iter != globalMap.end()) {
		if ((*iter).name.compare(varname) == 0)
			return (*iter).value;

		++iter;
	}
	
	return "Undefined";
}

void Mob::SetGlobal(const char *varname, const char *newvalue, int options, const char *duration, Mob *other) {

	int qgZoneid = zone->GetZoneID();
	int qgCharid = 0;
	int qgNpcid = 0;

	if (this->IsNPC())
	{
		qgNpcid = this->GetNPCTypeID();
	}
	else if (other && other->IsNPC())
	{
		qgNpcid = other->GetNPCTypeID();
	}

	if (this->IsClient())
	{
		qgCharid = this->CastToClient()->CharacterID();
	}
	else if (other && other->IsClient())
	{
		qgCharid = other->CastToClient()->CharacterID();
	}
	else
	{
		qgCharid = -qgNpcid;		// make char id negative npc id as a fudge
	}

	if (options < 0 || options > 7)
	{
		//cerr << "Invalid options for global var " << varname << " using defaults" << endl;
		options = 0;	// default = 0 (only this npcid,player and zone)
	}
	else
	{
		if (options & 1)
			qgNpcid=0;
		if (options & 2)
			qgCharid=0;
		if (options & 4)
			qgZoneid=0;
	}

	InsertQuestGlobal(qgCharid, qgNpcid, qgZoneid, varname, newvalue, QGVarDuration(duration));
}

void Mob::TarGlobal(const char *varname, const char *value, const char *duration, int qgNpcid, int qgCharid, int qgZoneid)
{
	InsertQuestGlobal(qgCharid, qgNpcid, qgZoneid, varname, value, QGVarDuration(duration));
}

void Mob::DelGlobal(const char *varname) {

	int qgZoneid=zone->GetZoneID();
	int qgCharid=0;
	int qgNpcid=0;

	if (this->IsNPC())
		qgNpcid = this->GetNPCTypeID();

	if (this->IsClient())
		qgCharid = this->CastToClient()->CharacterID();
	else
		qgCharid = -qgNpcid;		// make char id negative npc id as a fudge

    std::string query = StringFormat("DELETE FROM quest_globals "
                                    "WHERE name='%s' && (npcid=0 || npcid=%i) "
                                    "&& (charid=0 || charid=%i) "
                                    "&& (zoneid=%i || zoneid=0)",
                                    varname, qgNpcid, qgCharid, qgZoneid);

	database.QueryDatabase(query);

	if(zone)
	{
		ServerPacket* pack = new ServerPacket(ServerOP_QGlobalDelete, sizeof(ServerQGlobalDelete_Struct));
		ServerQGlobalDelete_Struct *qgu = (ServerQGlobalDelete_Struct*)pack->pBuffer;

		qgu->npc_id = qgNpcid;
		qgu->char_id = qgCharid;
		qgu->zone_id = qgZoneid;
		strcpy(qgu->name, varname);

		entity_list.DeleteQGlobal(std::string((char*)qgu->name), qgu->npc_id, qgu->char_id, qgu->zone_id);
		zone->DeleteQGlobal(std::string((char*)qgu->name), qgu->npc_id, qgu->char_id, qgu->zone_id);

		worldserver.SendPacket(pack);
		safe_delete(pack);
	}
}

// Inserts global variable into quest_globals table
void Mob::InsertQuestGlobal(int charid, int npcid, int zoneid, const char *varname, const char *varvalue, int duration) {

	// Make duration string either "unix_timestamp(now()) + xxx" or "NULL"
	std::stringstream duration_ss;

	if (duration == INT_MAX)
		duration_ss << "NULL";
	else
		duration_ss << "unix_timestamp(now()) + " << duration;

	//NOTE: this should be escaping the contents of arglist
	//npcwise a malicious script can arbitrarily alter the DB
	uint32 last_id = 0;
	std::string query = StringFormat("REPLACE INTO quest_globals "
                                    "(charid, npcid, zoneid, name, value, expdate)"
                                    "VALUES (%i, %i, %i, '%s', '%s', %s)",
                                    charid, npcid, zoneid, varname, varvalue, duration_ss.str().c_str());
	database.QueryDatabase(query);

	if(zone)
	{
		//first delete our global
		ServerPacket* pack = new ServerPacket(ServerOP_QGlobalDelete, sizeof(ServerQGlobalDelete_Struct));
		ServerQGlobalDelete_Struct *qgd = (ServerQGlobalDelete_Struct*)pack->pBuffer;
		qgd->npc_id = npcid;
		qgd->char_id = charid;
		qgd->zone_id = zoneid;
		qgd->from_zone_id = zone->GetZoneID();
		qgd->from_instance_id = zone->GetInstanceID();
		strcpy(qgd->name, varname);

		entity_list.DeleteQGlobal(std::string((char*)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);
		zone->DeleteQGlobal(std::string((char*)qgd->name), qgd->npc_id, qgd->char_id, qgd->zone_id);

		worldserver.SendPacket(pack);
		safe_delete(pack);

		//then create a new one with the new id
		pack = new ServerPacket(ServerOP_QGlobalUpdate, sizeof(ServerQGlobalUpdate_Struct));
		ServerQGlobalUpdate_Struct *qgu = (ServerQGlobalUpdate_Struct*)pack->pBuffer;
		qgu->npc_id = npcid;
		qgu->char_id = charid;
		qgu->zone_id = zoneid;

		if(duration == INT_MAX)
			qgu->expdate = 0xFFFFFFFF;
		else
			qgu->expdate = Timer::GetTimeSeconds() + duration;

		strcpy((char*)qgu->name, varname);
		strcpy((char*)qgu->value, varvalue);
		qgu->id = last_id;
		qgu->from_zone_id = zone->GetZoneID();
		qgu->from_instance_id = zone->GetInstanceID();

		QGlobal temp;
		temp.npc_id = npcid;
		temp.char_id = charid;
		temp.zone_id = zoneid;
		temp.expdate = qgu->expdate;
		temp.name.assign(qgu->name);
		temp.value.assign(qgu->value);
		entity_list.UpdateQGlobal(qgu->id, temp);
		zone->UpdateQGlobal(qgu->id, temp);

		worldserver.SendPacket(pack);
		safe_delete(pack);
	}

}

// Converts duration string to duration value (in seconds)
// Return of INT_MAX indicates infinite duration
int Mob::QGVarDuration(const char *fmt)
{
	int duration = 0;

	// format:	Y#### or D## or H## or M## or S## or T###### or C#######

	int len = static_cast<int>(strlen(fmt));

	// Default to no duration
	if (len < 1)
		return 0;

	// Set val to value after type character
	// e.g., for "M3924", set to 3924
	int val = atoi(&fmt[0] + 1);

	switch (fmt[0])
	{
		// Forever
		case 'F':
		case 'f':
			duration = INT_MAX;
			break;
		// Years
		case 'Y':
		case 'y':
			duration = val * 31556926;
			break;
		case 'D':
		case 'd':
			duration = val * 86400;
			break;
		// Hours
		case 'H':
		case 'h':
			duration = val * 3600;
			break;
		// Minutes
		case 'M':
		case 'm':
			duration = val * 60;
			break;
		// Seconds
		case 'S':
		case 's':
			duration = val;
			break;
		// Invalid
		default:
			duration = 0;
			break;
	}

	return duration;
}

bool Mob::DoKnockback(Mob *caster, float pushback, float pushup)
{
    if (GetRunspeed() <= 0.0 && GetWalkspeed() <= 0.0) {
        return false;
    }
	// This method should only be used for spell effects.

	glm::vec3 newloc(GetX(), GetY(), GetZ() + pushup);
	float newz = GetZ();
	
	GetPushHeadingMod(caster, pushback, newloc.x, newloc.y);
	if(pushup == 0 && zone->zonemap)
	{
		newz = zone->zonemap->FindBestZ(newloc, nullptr);
		if (newz != BEST_Z_INVALID)
			newloc.z = SetBestZ(newz);
	}

	if(CheckCoordLosNoZLeaps(GetX(), GetY(), GetZ(), newloc.x, newloc.y, newloc.z))
	{
		m_Position.x = newloc.x;
		m_Position.y = newloc.y;
		m_Position.z = newloc.z;

		uint8 self_update = 0;
		if(IsClient())
		{
			self_update = 1;
			CastToClient()->SetKnockBackExemption(true);
		}

		if(IsNPC() && !IsMoving())
			SendPosition();
		else
			SendPosUpdate(self_update);
	
		return true;
	}
	else
	{
		return false;
	}
}

bool Mob::CombatPush(Mob* attacker, float pushback)
{
    if (GetRunspeed() <= 0.0 && GetWalkspeed() <= 0.0) {
        return false;
    }
	// Use this method for stun/combat pushback.

	glm::vec3 newloc(GetX(), GetY(), GetZ());
	float newz = GetZ();

	GetPushHeadingMod(attacker, pushback, newloc.x, newloc.y);
	if(zone->zonemap)
	{
		newz = zone->zonemap->FindBestZ(newloc, nullptr);
		if (newz != BEST_Z_INVALID)
			newloc.z = SetBestZ(newz);

		Log.Out(Logs::Detail, Logs::Combat, "Push: BestZ returned %0.2f for %0.2f,%0.2f,%0.2f", newloc.z, newloc.x, newloc.y, m_Position.z);
	}

	if(CheckCoordLosNoZLeaps(m_Position.x, m_Position.y, m_Position.z, newloc.x, newloc.y, newloc.z))
	{
		Log.Out(Logs::Detail, Logs::Combat, "Push: X: %0.2f -> %0.2f Y: %0.2f -> %0.2f Z: %0.2f -> %0.2f", m_Position.x, newloc.x, m_Position.y, newloc.y, m_Position.z, newloc.z);
		m_Position.x = newloc.x;
		m_Position.y = newloc.y;
		m_Position.z = newloc.z;

		uint8 self_update = 0;
		if(IsClient())
		{
			self_update = 1;
			CastToClient()->SetKnockBackExemption(true);
		}

		if(IsNPC() && !IsMoving())
			SendPosition();
		else
			SendPosUpdate(self_update);

		return true;
	}
	return false;
}

void Mob::GetPushHeadingMod(Mob* attacker, float pushback, float &x_coord, float &y_coord)
{
	float headingRadians = attacker->GetHeading();
	headingRadians = (headingRadians * 360.0f) / 256.0f;
	if (headingRadians < 270)
		headingRadians += 90;
	else
		headingRadians -= 270;
	headingRadians = headingRadians * 3.1415f / 180.0f;

	float tmpx = -cosf(headingRadians) * pushback;
	float tmpy = sinf(headingRadians) * pushback;

	x_coord += tmpx;
	y_coord += tmpy;
}

void Mob::TrySpellOnKill(uint8 level, uint16 spell_id)
{
	if (spell_id != SPELL_UNKNOWN)
	{
		if(IsEffectInSpell(spell_id, SE_ProcOnSpellKillShot)) {
			for (int i = 0; i < EFFECT_COUNT; i++) {
				if (spells[spell_id].effectid[i] == SE_ProcOnSpellKillShot)
				{
					if (IsValidSpell(spells[spell_id].base2[i]) && spells[spell_id].max[i] <= level)
					{
						if(zone->random.Roll(spells[spell_id].base[i]))
							SpellFinished(spells[spell_id].base2[i], this, 10, 0, -1, spells[spells[spell_id].base2[i]].ResistDiff);
					}
				}
			}
		}
	}
}

bool Mob::TrySpellOnDeath()
{
	if (IsNPC() && !spellbonuses.SpellOnDeath[0] && !itembonuses.SpellOnDeath[0])
		return false;

	if (IsClient() && !aabonuses.SpellOnDeath[0] && !spellbonuses.SpellOnDeath[0] && !itembonuses.SpellOnDeath[0])
		return false;

	for(int i = 0; i < MAX_SPELL_TRIGGER*2; i+=2) {
		if(IsClient() && aabonuses.SpellOnDeath[i] && IsValidSpell(aabonuses.SpellOnDeath[i])) {
			if(zone->random.Roll(static_cast<int>(aabonuses.SpellOnDeath[i + 1]))) {
				SpellFinished(aabonuses.SpellOnDeath[i], this, 10, 0, -1, spells[aabonuses.SpellOnDeath[i]].ResistDiff);
			}
		}

		if(itembonuses.SpellOnDeath[i] && IsValidSpell(itembonuses.SpellOnDeath[i])) {
			if(zone->random.Roll(static_cast<int>(itembonuses.SpellOnDeath[i + 1]))) {
				SpellFinished(itembonuses.SpellOnDeath[i], this, 10, 0, -1, spells[itembonuses.SpellOnDeath[i]].ResistDiff);
			}
		}

		if(spellbonuses.SpellOnDeath[i] && IsValidSpell(spellbonuses.SpellOnDeath[i])) {
			if(zone->random.Roll(static_cast<int>(spellbonuses.SpellOnDeath[i + 1]))) {
				SpellFinished(spellbonuses.SpellOnDeath[i], this, 10, 0, -1, spells[spellbonuses.SpellOnDeath[i]].ResistDiff);
				}
			}
		}

	BuffFadeAll();
	return false;
	//You should not be able to use this effect and survive (ALWAYS return false),
	//attempting to place a heal in these effects will still result
	//in death because the heal will not register before the script kills you.
}

void Mob::SetGrouped(bool v)
{
	if(v)
	{
		israidgrouped = false;
	}
	isgrouped = v;

	if(IsClient())
	{
			parse->EventPlayer(EVENT_GROUP_CHANGE, CastToClient(), "", 0);
	}
}

void Mob::SetRaidGrouped(bool v)
{
	if(v)
	{
		isgrouped = false;
	}
	israidgrouped = v;

	if(IsClient())
	{
		parse->EventPlayer(EVENT_GROUP_CHANGE, CastToClient(), "", 0);
	}
}

int16 Mob::GetCriticalChanceBonus(uint16 skill)
{
	int critical_chance = 0;

	// All skills + Skill specific
	critical_chance +=	itembonuses.CriticalHitChance[HIGHEST_SKILL+1] + spellbonuses.CriticalHitChance[HIGHEST_SKILL+1] + aabonuses.CriticalHitChance[HIGHEST_SKILL+1] +
						itembonuses.CriticalHitChance[skill] + spellbonuses.CriticalHitChance[skill] + aabonuses.CriticalHitChance[skill];

	if(critical_chance < -100)
		critical_chance = -100;

	return critical_chance;
}

int16 Mob::GetMeleeDamageMod_SE(uint16 skill)
{
	int dmg_mod = 0;

	// All skill dmg mod + Skill specific
	dmg_mod += itembonuses.DamageModifier[HIGHEST_SKILL+1] + spellbonuses.DamageModifier[HIGHEST_SKILL+1] + aabonuses.DamageModifier[HIGHEST_SKILL+1] +
				itembonuses.DamageModifier[skill] + spellbonuses.DamageModifier[skill] + aabonuses.DamageModifier[skill];

	dmg_mod += itembonuses.DamageModifier2[HIGHEST_SKILL+1] + spellbonuses.DamageModifier2[HIGHEST_SKILL+1] + aabonuses.DamageModifier2[HIGHEST_SKILL+1] +
				itembonuses.DamageModifier2[skill] + spellbonuses.DamageModifier2[skill] + aabonuses.DamageModifier2[skill];

	if (HasShieldEquiped() && !IsOffHandAtk())
		dmg_mod += itembonuses.ShieldEquipDmgMod[0] + spellbonuses.ShieldEquipDmgMod[0] + aabonuses.ShieldEquipDmgMod[0];

	if(dmg_mod < -100)
		dmg_mod = -100;

	return dmg_mod;
}

int16 Mob::GetMeleeMinDamageMod_SE(uint16 skill)
{
	int dmg_mod = 0;

	dmg_mod = itembonuses.MinDamageModifier[skill] + spellbonuses.MinDamageModifier[skill] +
				itembonuses.MinDamageModifier[HIGHEST_SKILL+1] + spellbonuses.MinDamageModifier[HIGHEST_SKILL+1];

	if(dmg_mod < -100)
		dmg_mod = -100;

	return dmg_mod;
}

int16 Mob::GetCrippBlowChance()
{
	int16 crip_chance = 0;

	crip_chance += itembonuses.CrippBlowChance + spellbonuses.CrippBlowChance + aabonuses.CrippBlowChance;

	if(crip_chance < 0)
		crip_chance = 0;

	return crip_chance;
}

int16 Mob::GetSkillReuseTime(uint16 skill)
{
	int skill_reduction = this->itembonuses.SkillReuseTime[skill] + this->spellbonuses.SkillReuseTime[skill] + this->aabonuses.SkillReuseTime[skill];

	return skill_reduction;
}

int16 Mob::GetSkillDmgAmt(uint16 skill)
{
	int skill_dmg = 0;

	// All skill dmg(only spells do this) + Skill specific
	skill_dmg += spellbonuses.SkillDamageAmount[HIGHEST_SKILL+1] + itembonuses.SkillDamageAmount[HIGHEST_SKILL+1] + aabonuses.SkillDamageAmount[HIGHEST_SKILL+1]
				+ itembonuses.SkillDamageAmount[skill] + spellbonuses.SkillDamageAmount[skill] + aabonuses.SkillDamageAmount[skill];

	skill_dmg += spellbonuses.SkillDamageAmount2[HIGHEST_SKILL+1] + itembonuses.SkillDamageAmount2[HIGHEST_SKILL+1]
				+ itembonuses.SkillDamageAmount2[skill] + spellbonuses.SkillDamageAmount2[skill];

	return skill_dmg;
}

void Mob::MeleeLifeTap(int32 damage) {

	int32 lifetap_amt = 0;
	lifetap_amt = spellbonuses.MeleeLifetap + itembonuses.MeleeLifetap + aabonuses.MeleeLifetap
				+ spellbonuses.Vampirism + itembonuses.Vampirism + aabonuses.Vampirism;

	if(lifetap_amt && damage > 0){

		lifetap_amt = damage * lifetap_amt / 100;
		Log.Out(Logs::Detail, Logs::Combat, "Melee lifetap healing for %d damage.", damage);

		if (lifetap_amt > 0)
			HealDamage(lifetap_amt); //Heal self for modified damage amount.
		else
			Damage(this, -lifetap_amt,0, SkillEvocation,false); //Dmg self for modified damage amount.
	}
}

bool Mob::TryReflectSpell(uint32 spell_id)
{
	if (!spells[spell_id].reflectable)
 		return false;

	int chance = itembonuses.reflect_chance + spellbonuses.reflect_chance + aabonuses.reflect_chance;

	if(chance && zone->random.Roll(chance))
		return true;

	return false;
}

void Mob::DoGravityEffect()
{
	Mob *caster = nullptr;
	int away = -1;
	float caster_x, caster_y, amount, value, cur_x, my_x, cur_y, my_y, x_vector, y_vector, hypot;

	// Set values so we can run through all gravity effects and then apply the culmative move at the end
	// instead of many small moves if the mob/client had more than 1 gravity effect on them
	cur_x = my_x = GetX();
	cur_y = my_y = GetY();

	int buff_count = GetMaxTotalSlots();
	for (int slot = 0; slot < buff_count; slot++)
	{
		if (buffs[slot].spellid != SPELL_UNKNOWN && IsEffectInSpell(buffs[slot].spellid, SE_GravityEffect))
		{
			for (int i = 0; i < EFFECT_COUNT; i++)
			{
				if(spells[buffs[slot].spellid].effectid[i] == SE_GravityEffect) {

					int casterId = buffs[slot].casterid;
					if(casterId)
						caster = entity_list.GetMob(casterId);

					if(!caster || casterId == this->GetID())
						continue;

					caster_x = caster->GetX();
					caster_y = caster->GetY();

					value = static_cast<float>(spells[buffs[slot].spellid].base[i]);
					if(value == 0)
						continue;

					if(value > 0)
						away = 1;

					amount = std::abs(value) /
						 (100.0f); // to bring the values in line, arbitarily picked

					x_vector = cur_x - caster_x;
					y_vector = cur_y - caster_y;
					hypot = sqrt(x_vector*x_vector + y_vector*y_vector);

					if(hypot <= 5) // dont want to be inside the mob, even though we can, it looks bad
						continue;

					x_vector /= hypot;
					y_vector /= hypot;

					cur_x = cur_x + (x_vector * amount * away);
					cur_y = cur_y + (y_vector * amount * away);
				}
			}
		}
	}

	if ((std::abs(my_x - cur_x) > 0.01) || (std::abs(my_y - cur_y) > 0.01)) {
		float new_ground = GetGroundZ(cur_x, cur_y);
		// If we cant get LoS on our new spot then keep checking up to 5 units up.
		if(!CheckLosFN(cur_x, cur_y, new_ground, GetSize())) {
			for(float z_adjust = 0.1f; z_adjust < 5; z_adjust += 0.1f) {
				if(CheckLosFN(cur_x, cur_y, new_ground+z_adjust, GetSize())) {
					new_ground += z_adjust;
					break;
				}
			}
			// If we still fail, then lets only use the x portion(ie sliding around a wall)
			if(!CheckLosFN(cur_x, my_y, new_ground, GetSize())) {
				// If that doesnt work, try the y
				if(!CheckLosFN(my_x, cur_y, new_ground, GetSize())) {
					// If everything fails, then lets do nothing
					return;
				}
				else {
					cur_x = my_x;
				}
			}
			else {
				cur_y = my_y;
			}
		}

		if(IsClient())
			this->CastToClient()->MovePC(zone->GetZoneID(), zone->GetInstanceID(), cur_x, cur_y, new_ground, GetHeading()*2); // I know the heading thing is weird(chance of movepc to halve the heading value, too lazy to figure out why atm)
		else
			this->GMMove(cur_x, cur_y, new_ground, GetHeading());
	}
}

void Mob::SpreadVirus(uint16 spell_id, uint16 casterID)
{
	int num_targs = spells[spell_id].viral_targets;

	Mob* caster = entity_list.GetMob(casterID);
	Mob* target = nullptr;
	// Only spread in zones without perm buffs
	if(!zone->BuffTimersSuspended()) {
		for(int i = 0; i < num_targs; i++) {
			target = entity_list.GetTargetForVirus(this, spells[spell_id].viral_range);
			if(target) {
				// Only spreads to the uninfected
				if(!target->FindBuff(spell_id)) {
					if(caster)
						caster->SpellOnTarget(spell_id, target);

				}
			}
		}
	}
}

bool Mob::IsBoat() const 
{
	return (GetBaseRace() == SHIP || GetBaseRace() == LAUNCH || GetBaseRace() == CONTROLLED_BOAT);
}

void Mob::SetBodyType(bodyType new_body, bool overwrite_orig) {
	bool needs_spawn_packet = false;
	if(bodytype == 11 || bodytype >= 65 || new_body == 11 || new_body >= 65) {
		needs_spawn_packet = true;
	}

	if(overwrite_orig) {
		orig_bodytype = new_body;
	}
	bodytype = new_body;

	if(needs_spawn_packet) {
		EQApplicationPacket* app = new EQApplicationPacket;
		CreateDespawnPacket(app, true);
		entity_list.QueueClients(this, app);
		CreateSpawnPacket(app, this);
		entity_list.QueueClients(this, app);
		safe_delete(app);
	}
}


void Mob::ModSkillDmgTaken(SkillUseTypes skill_num, int value)
{
	if (skill_num <= HIGHEST_SKILL)
		SkillDmgTaken_Mod[skill_num] = value;


	else if (skill_num == 255 || skill_num == -1)
		SkillDmgTaken_Mod[HIGHEST_SKILL+1] = value;
}

int16 Mob::GetModSkillDmgTaken(const SkillUseTypes skill_num)
{
	if (skill_num <= HIGHEST_SKILL)
		return SkillDmgTaken_Mod[skill_num];

	else if (skill_num == 255 || skill_num == -1)
		return SkillDmgTaken_Mod[HIGHEST_SKILL+1];

	return 0;
}

void Mob::ModVulnerability(uint8 resist, int16 value)
{
	if (resist < HIGHEST_RESIST+1)
		Vulnerability_Mod[resist] = value;

	else if (resist == 255)
		Vulnerability_Mod[HIGHEST_RESIST+1] = value;
}

int16 Mob::GetModVulnerability(const uint8 resist)
{
	if (resist < HIGHEST_RESIST+1)
		return Vulnerability_Mod[resist];

	else if (resist == 255)
		return Vulnerability_Mod[HIGHEST_RESIST+1];

	return 0;
}

void Mob::CastOnCurer(uint32 spell_id)
{
	for(int i = 0; i < EFFECT_COUNT; i++)
	{
		if (spells[spell_id].effectid[i] == SE_CastOnCurer)
		{
			if(IsValidSpell(spells[spell_id].base[i]))
			{
				SpellFinished(spells[spell_id].base[i], this);
			}
		}
	}
}

void Mob::CastOnCure(uint32 spell_id)
{
	for(int i = 0; i < EFFECT_COUNT; i++)
	{
		if (spells[spell_id].effectid[i] == SE_CastOnCure)
		{
			if(IsValidSpell(spells[spell_id].base[i]))
			{
				SpellFinished(spells[spell_id].base[i], this);
			}
		}
	}
}

void Mob::CastOnNumHitFade(uint32 spell_id)
{
	if(!IsValidSpell(spell_id))
		return;

	for(int i = 0; i < EFFECT_COUNT; i++)
	{
		if (spells[spell_id].effectid[i] == SE_CastonNumHitFade)
		{
			if(IsValidSpell(spells[spell_id].base[i]))
			{
				SpellFinished(spells[spell_id].base[i], this);
			}
		}
	}
}

void Mob::SlowMitigation(Mob* caster)
{
	/*if (GetSlowMitigation() && caster && caster->IsClient())
	{
		if ((GetSlowMitigation() > 0) && (GetSlowMitigation() < 26))
			caster->Message_StringID(MT_SpellFailure, SLOW_MOSTLY_SUCCESSFUL);

		else if ((GetSlowMitigation() >= 26) && (GetSlowMitigation() < 74))
			caster->Message_StringID(MT_SpellFailure, SLOW_PARTIALLY_SUCCESSFUL);

		else if ((GetSlowMitigation() >= 74) && (GetSlowMitigation() < 101))
			caster->Message_StringID(MT_SpellFailure, SLOW_SLIGHTLY_SUCCESSFUL);

		else if (GetSlowMitigation() > 100)
			caster->Message_StringID(MT_SpellFailure, SPELL_OPPOSITE_EFFECT);
	}*/
}

uint16 Mob::GetSkillByItemType(int ItemType)
{
	switch (ItemType)
	{
		case ItemType1HSlash:
			return Skill1HSlashing;
		case ItemType2HSlash:
			return Skill2HSlashing;
		case ItemType1HPiercing:
			return Skill1HPiercing;
		case ItemType1HBlunt:
			return Skill1HBlunt;
		case ItemType2HBlunt:
			return Skill2HBlunt;
		case ItemType2HPiercing:
			return Skill1HPiercing; // change to 2HPiercing once activated
		case ItemTypeBow:
			return SkillArchery;
		case ItemTypeLargeThrowing:
		case ItemTypeSmallThrowing:
			return SkillThrowing;
		case ItemTypeMartial:
			return SkillHandtoHand;
		default:
			return SkillHandtoHand;
	}
	return SkillHandtoHand;
 }

uint8 Mob::GetItemTypeBySkill(SkillUseTypes skill)
{
	switch (skill)
	{
		case SkillThrowing:
			return ItemTypeSmallThrowing;
		case SkillArchery:
			return ItemTypeArrow;
		case Skill1HSlashing:
			return ItemType1HSlash;
		case Skill2HSlashing:
			return ItemType2HSlash;
		case Skill1HPiercing:
			return ItemType1HPiercing;
		case Skill1HBlunt:
			return ItemType1HBlunt;
		case Skill2HBlunt:
			return ItemType2HBlunt;
		case SkillHandtoHand:
			return ItemTypeMartial;
		default:
			return ItemTypeMartial;
	}
	return ItemTypeMartial;
 }


bool Mob::PassLimitToSkill(uint16 spell_id, uint16 skill) {

	if (!IsValidSpell(spell_id))
		return false;

	for (int i = 0; i < EFFECT_COUNT; i++) {
		if (spells[spell_id].effectid[i] == SE_LimitToSkill){
			if (spells[spell_id].base[i] == skill){
				return true;
			}
		}
	}
	return false;
}

uint16 Mob::GetWeaponSpeedbyHand(uint16 hand) {

	uint16 weapon_speed = 0;
	switch (hand) {

		case 13:
			weapon_speed = attack_timer.GetDuration();
			break;
		case 14:
			weapon_speed = attack_dw_timer.GetDuration();
			break;
		case 11:
			weapon_speed = ranged_timer.GetDuration();
			break;
	}

	if (weapon_speed < RuleI(Combat, MinHastedDelay))
		weapon_speed = RuleI(Combat, MinHastedDelay);

	return weapon_speed;
}

int8 Mob::GetDecayEffectValue(uint16 spell_id, uint16 spelleffect) {

	if (!IsValidSpell(spell_id))
		return false;

	int spell_level = spells[spell_id].classes[(GetClass()%16) - 1];
	int effect_value = 0;
	int lvlModifier = 100;

	int buff_count = GetMaxTotalSlots();
	for (int slot = 0; slot < buff_count; slot++){
		if (IsValidSpell(buffs[slot].spellid)){
			for (int i = 0; i < EFFECT_COUNT; i++){
				if(spells[buffs[slot].spellid].effectid[i] == spelleffect) {

					int critchance = spells[buffs[slot].spellid].base[i];
					int decay = spells[buffs[slot].spellid].base2[i];
					int lvldiff = spell_level - spells[buffs[slot].spellid].max[i];

					if(lvldiff > 0 && decay > 0)
					{
						lvlModifier -= decay*lvldiff;
						if (lvlModifier > 0){
							critchance = (critchance*lvlModifier)/100;
							effect_value += critchance;
						}
					}

					else
						effect_value += critchance;
				}
			}
		}
	}

	return effect_value;
}

// Faction Mods for Alliance type spells
void Mob::AddFactionBonus(uint32 pFactionID,int32 bonus) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	typedef std::pair <uint32, int32> NewFactionBonus;

	faction_bonus = faction_bonuses.find(pFactionID);
	if(faction_bonus == faction_bonuses.end())
	{
		faction_bonuses.insert(NewFactionBonus(pFactionID,bonus));
	}
	else
	{
		if(faction_bonus->second<bonus)
		{
			faction_bonuses.erase(pFactionID);
			faction_bonuses.insert(NewFactionBonus(pFactionID,bonus));
		}
	}
}

// Faction Mods from items
void Mob::AddItemFactionBonus(uint32 pFactionID,int32 bonus) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	typedef std::pair <uint32, int32> NewFactionBonus;

	faction_bonus = item_faction_bonuses.find(pFactionID);
	if(faction_bonus == item_faction_bonuses.end())
	{
		item_faction_bonuses.insert(NewFactionBonus(pFactionID,bonus));
	}
	else
	{
		if((bonus > 0 && faction_bonus->second < bonus) || (bonus < 0 && faction_bonus->second > bonus))
		{
			item_faction_bonuses.erase(pFactionID);
			item_faction_bonuses.insert(NewFactionBonus(pFactionID,bonus));
		}
	}
}

int32 Mob::GetFactionBonus(uint32 pFactionID) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	faction_bonus = faction_bonuses.find(pFactionID);
	if(faction_bonus != faction_bonuses.end())
	{
		return (*faction_bonus).second;
	}
	return 0;
}

int32 Mob::GetItemFactionBonus(uint32 pFactionID) {
	std::map <uint32, int32> :: const_iterator faction_bonus;
	faction_bonus = item_faction_bonuses.find(pFactionID);
	if(faction_bonus != item_faction_bonuses.end())
	{
		return (*faction_bonus).second;
	}
	return 0;
}

void Mob::ClearItemFactionBonuses() {
	item_faction_bonuses.clear();
}

FACTION_VALUE Mob::GetSpecialFactionCon(Mob* iOther) {
	if (!iOther)
		return FACTION_INDIFFERENT;

	iOther = iOther->GetOwnerOrSelf();
	Mob* self = this->GetOwnerOrSelf();

	bool selfAIcontrolled = self->IsAIControlled();
	bool iOtherAIControlled = iOther->IsAIControlled();
	int selfPrimaryFaction = self->GetPrimaryFaction();
	int iOtherPrimaryFaction = iOther->GetPrimaryFaction();

	if (selfPrimaryFaction >= 0 && selfAIcontrolled)
		return FACTION_INDIFFERENT;
	if (iOther->GetPrimaryFaction() >= 0)
		return FACTION_INDIFFERENT;
/* special values:
	-2 = indiff to player, ally to AI on special values, indiff to AI
	-3 = dub to player, ally to AI on special values, indiff to AI
	-4 = atk to player, ally to AI on special values, indiff to AI
	-5 = indiff to player, indiff to AI
	-6 = dub to player, indiff to AI
	-7 = atk to player, indiff to AI
	-8 = indiff to players, ally to AI on same value, indiff to AI
	-9 = dub to players, ally to AI on same value, indiff to AI
	-10 = atk to players, ally to AI on same value, indiff to AI
	-11 = indiff to players, ally to AI on same value, atk to AI
	-12 = dub to players, ally to AI on same value, atk to AI
	-13 = atk to players, ally to AI on same value, atk to AI
*/
	switch (iOtherPrimaryFaction) {
		case -2: // -2 = indiff to player, ally to AI on special values, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_ALLY;
			else
				return FACTION_INDIFFERENT;
		case -3: // -3 = dub to player, ally to AI on special values, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_ALLY;
			else
				return FACTION_DUBIOUS;
		case -4: // -4 = atk to player, ally to AI on special values, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_ALLY;
			else
				return FACTION_SCOWLS;
		case -5: // -5 = indiff to player, indiff to AI
			return FACTION_INDIFFERENT;
		case -6: // -6 = dub to player, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_INDIFFERENT;
			else
				return FACTION_DUBIOUS;
		case -7: // -7 = atk to player, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled)
				return FACTION_INDIFFERENT;
			else
				return FACTION_SCOWLS;
		case -8: // -8 = indiff to players, ally to AI on same value, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_INDIFFERENT;
			}
			else
				return FACTION_INDIFFERENT;
		case -9: // -9 = dub to players, ally to AI on same value, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_INDIFFERENT;
			}
			else
				return FACTION_DUBIOUS;
		case -10: // -10 = atk to players, ally to AI on same value, indiff to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_INDIFFERENT;
			}
			else
				return FACTION_SCOWLS;
		case -11: // -11 = indiff to players, ally to AI on same value, atk to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_SCOWLS;
			}
			else
				return FACTION_INDIFFERENT;
		case -12: // -12 = dub to players, ally to AI on same value, atk to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_SCOWLS;


			}
			else
				return FACTION_DUBIOUS;
		case -13: // -13 = atk to players, ally to AI on same value, atk to AI
			if (selfAIcontrolled && iOtherAIControlled) {
				if (selfPrimaryFaction == iOtherPrimaryFaction)
					return FACTION_ALLY;
				else
					return FACTION_SCOWLS;
			}
			else
				return FACTION_SCOWLS;
		default:
			return FACTION_INDIFFERENT;
	}
}

bool Mob::HasSpellEffect(int effectid)
{
    int i;

    int buff_count = GetMaxTotalSlots();
    for(i = 0; i < buff_count; i++)
    {
        if(buffs[i].spellid == SPELL_UNKNOWN) { continue; }

        if(IsEffectInSpell(buffs[i].spellid, effectid))
        {
            return(1);
        }
    }
    return(0);
}

int Mob::GetSpecialAbility(int ability) {
	if(ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return 0;
	}

	return SpecialAbilities[ability].level;
}

int Mob::GetSpecialAbilityParam(int ability, int param) {
	if(param >= MAX_SPECIAL_ATTACK_PARAMS || param < 0 || ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return 0;
	}

	return SpecialAbilities[ability].params[param];
}

void Mob::SetSpecialAbility(int ability, int level) {
	if(ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return;
	}

	SpecialAbilities[ability].level = level;
}

void Mob::SetSpecialAbilityParam(int ability, int param, int value) {
	if(param >= MAX_SPECIAL_ATTACK_PARAMS || param < 0 || ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return;
	}

	SpecialAbilities[ability].params[param] = value;
}

void Mob::StartSpecialAbilityTimer(int ability, uint32 time) {
	if (ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return;
	}

	if(SpecialAbilities[ability].timer) {
		SpecialAbilities[ability].timer->Start(time);
	} else {
		SpecialAbilities[ability].timer = new Timer(time);
		SpecialAbilities[ability].timer->Start();
	}
}

void Mob::StopSpecialAbilityTimer(int ability) {
	if (ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return;
	}

	safe_delete(SpecialAbilities[ability].timer);
}

Timer *Mob::GetSpecialAbilityTimer(int ability) {
	if (ability >= MAX_SPECIAL_ATTACK || ability < 0) {
		return nullptr;
	}

	return SpecialAbilities[ability].timer;
}

void Mob::ClearSpecialAbilities() {
	for(int a = 0; a < MAX_SPECIAL_ATTACK; ++a) {
		SpecialAbilities[a].level = 0;
		safe_delete(SpecialAbilities[a].timer);
		for(int p = 0; p < MAX_SPECIAL_ATTACK_PARAMS; ++p) {
			SpecialAbilities[a].params[p] = 0;
		}
	}
}

void Mob::ProcessSpecialAbilities(const std::string &str) {
	ClearSpecialAbilities();

	std::vector<std::string> sp = SplitString(str, '^');
	for(auto iter = sp.begin(); iter != sp.end(); ++iter) {
		std::vector<std::string> sub_sp = SplitString((*iter), ',');
		if(sub_sp.size() >= 2) {
			int ability = std::stoi(sub_sp[0]);
			int value = std::stoi(sub_sp[1]);

			SetSpecialAbility(ability, value);
			switch(ability) {
			case SPECATK_QUAD:
				if(value > 0) {
					SetSpecialAbility(SPECATK_TRIPLE, 1);
				}
				break;
			case DESTRUCTIBLE_OBJECT:
				break;
			default:
				break;
			}

			for(size_t i = 2, p = 0; i < sub_sp.size(); ++i, ++p) {
				if(p >= MAX_SPECIAL_ATTACK_PARAMS) {
					break;
				}

				SetSpecialAbilityParam(ability, p, std::stoi(sub_sp[i]));
			}
		}
	}
}

// derived from client to keep these functions more consistent
// if anything seems weird, blame SoE
bool Mob::IsFacingMob(Mob *other)
{
	if (!other)
		return false;
	float angle = HeadingAngleToMob(other);
	// what the client uses appears to be 2x our internal heading
	float heading = GetHeading() * 2.0f;

	if (angle > 472.0 && heading < 40.0)
		angle = heading;
	if (angle < 40.0 && heading > 472.0)
		angle = heading;

	if (std::abs(angle - heading) <= 80.0)
		return true;

	return false;
}

// All numbers derived from the client
float Mob::HeadingAngleToMob(Mob *other)
{
	float mob_x = other->GetX();
	float mob_y = other->GetY();
	float this_x = GetX();
	float this_y = GetY();

	float y_diff = std::abs(this_y - mob_y);
	float x_diff = std::abs(this_x - mob_x);
	if (y_diff < 0.0000009999999974752427)
		y_diff = 0.0000009999999974752427;

	float angle = atan2(x_diff, y_diff) * 180.0f * 0.3183099014828645f; // angle, nice "pi"

	// return the right thing based on relative quadrant
	// I'm sure this could be improved for readability, but whatever
	if (this_y >= mob_y) {
		if (mob_x >= this_x)
			return (90.0f - angle + 90.0f) * 511.5f * 0.0027777778f;
		if (mob_x <= this_x)
			return (angle + 180.0f) * 511.5f * 0.0027777778f;
	}
	if (this_y > mob_y || mob_x > this_x)
		return angle * 511.5f * 0.0027777778f;
	else
		return (90.0f - angle + 270.0f) * 511.5f * 0.0027777778f;
}

bool Mob::GetSeeInvisible(uint8 see_invis)
{ 
	if(see_invis > 0)
	{
		if(see_invis == 1)
			return true;
		else
		{
			if (zone->random.Int(0, 99) < see_invis)
				return true;
		}
	}
	return false;
}

int32 Mob::GetSpellStat(uint32 spell_id, const char *identifier, uint8 slot)
{
	if (!IsValidSpell(spell_id))
		return 0;

	if (!identifier)
		return 0;

	int32 stat = 0;

	if (slot > 0)
		slot = slot - 1;

	std::string id = identifier;
	for(uint32 i = 0; i < id.length(); ++i)
	{
		id[i] = tolower(id[i]);
	}

	if (slot < 16){
		if (id == "classes") {stat = spells[spell_id].classes[slot]; }
		else if (id == "dieties") {stat = spells[spell_id].deities[slot];}
	}

	if (slot < 12){
		if (id == "base") {stat = spells[spell_id].base[slot];}
		else if (id == "base2") {stat = spells[spell_id].base2[slot];}
		else if (id == "max") {stat = spells[spell_id].max[slot];}
		else if (id == "formula") {spells[spell_id].formula[slot];}
		else if (id == "effectid") {spells[spell_id].effectid[slot];}
	}

	if (slot < 4){
		if (id == "components") { spells[spell_id].components[slot];}
		else if (id == "component_counts") {spells[spell_id].component_counts[slot];}
		else if (id == "NoexpendReagent") {spells[spell_id].NoexpendReagent[slot];}
	}

	if (id == "range") {stat = static_cast<int32>(spells[spell_id].range); }
	else if (id == "aoerange") {stat = static_cast<int32>(spells[spell_id].aoerange);}
	else if (id == "pushback") {stat = static_cast<int32>(spells[spell_id].pushback);}
	else if (id == "pushup") {stat = static_cast<int32>(spells[spell_id].pushup);}
	else if (id == "cast_time") {stat = spells[spell_id].cast_time;}
	else if (id == "recovery_time") {stat = spells[spell_id].recovery_time;}
	else if (id == "recast_time") {stat = spells[spell_id].recast_time;}
	else if (id == "buffdurationformula") {stat = spells[spell_id].buffdurationformula;}
	else if (id == "buffduration") {stat = spells[spell_id].buffduration;}
	else if (id == "AEDuration") {stat = spells[spell_id].AEDuration;}
	else if (id == "mana") {stat = spells[spell_id].mana;}
	//else if (id == "LightType") {stat = spells[spell_id].LightType;} - Not implemented
	else if (id == "goodEffect") {stat = spells[spell_id].goodEffect;}
	else if (id == "Activated") {stat = spells[spell_id].Activated;}
	else if (id == "resisttype") {stat = spells[spell_id].resisttype;}
	else if (id == "targettype") {stat = spells[spell_id].targettype;}
	else if (id == "basedeiff") {stat = spells[spell_id].basediff;}
	else if (id == "skill") {stat = spells[spell_id].skill;}
	else if (id == "zonetype") {stat = spells[spell_id].zonetype;}
	else if (id == "EnvironmentType") {stat = spells[spell_id].EnvironmentType;}
	else if (id == "TimeOfDay") {stat = spells[spell_id].TimeOfDay;}
	else if (id == "CastingAnim") {stat = spells[spell_id].CastingAnim;}
	else if (id == "SpellAffectIndex") {stat = spells[spell_id].SpellAffectIndex; }
	else if (id == "disallow_sit") {stat = spells[spell_id].disallow_sit; }
	//else if (id == "spellanim") {stat = spells[spell_id].spellanim; } - Not implemented
	else if (id == "uninterruptable") {stat = spells[spell_id].uninterruptable; }
	else if (id == "ResistDiff") {stat = spells[spell_id].ResistDiff; }
	else if (id == "dot_stacking_exemp") {stat = spells[spell_id].dot_stacking_exempt; }
	else if (id == "RecourseLink") {stat = spells[spell_id].RecourseLink; }
	else if (id == "no_partial_resist") {stat = spells[spell_id].no_partial_resist; }
	else if (id == "short_buff_box") {stat = spells[spell_id].short_buff_box; }
	else if (id == "descnum") {stat = spells[spell_id].descnum; }
	else if (id == "effectdescnum") {stat = spells[spell_id].effectdescnum; }
	else if (id == "npc_no_los") {stat = spells[spell_id].npc_no_los; }
	else if (id == "reflectable") {stat = spells[spell_id].reflectable; }
	else if (id == "bonushate") {stat = spells[spell_id].bonushate; }
	else if (id == "EndurCost") {stat = spells[spell_id].EndurCost; }
	else if (id == "EndurTimerIndex") {stat = spells[spell_id].EndurTimerIndex; }
	else if (id == "HateAdded") {stat = spells[spell_id].HateAdded; }
	else if (id == "EndurUpkeep") {stat = spells[spell_id].EndurUpkeep; }
	else if (id == "numhitstype") {stat = spells[spell_id].numhitstype; }
	else if (id == "numhits") {stat = spells[spell_id].numhits; }
	else if (id == "pvpresistbase") {stat = spells[spell_id].pvpresistbase; }
	else if (id == "pvpresistcalc") {stat = spells[spell_id].pvpresistcalc; }
	else if (id == "pvpresistcap") {stat = spells[spell_id].pvpresistcap; }
	else if (id == "spell_category") {stat = spells[spell_id].spell_category; }
	else if (id == "can_mgb") {stat = spells[spell_id].can_mgb; }
	else if (id == "dispel_flag") {stat = spells[spell_id].dispel_flag; }
	else if (id == "MinResist") {stat = spells[spell_id].MinResist; }
	else if (id == "MaxResist") {stat = spells[spell_id].MaxResist; }
	else if (id == "viral_targets") {stat = spells[spell_id].viral_targets; }
	else if (id == "viral_timer") {stat = spells[spell_id].viral_timer; }
	else if (id == "NimbusEffect") {stat = spells[spell_id].NimbusEffect; }
	else if (id == "directional_start") {stat = static_cast<int32>(spells[spell_id].directional_start); }
	else if (id == "directional_end") {stat = static_cast<int32>(spells[spell_id].directional_end); }
	else if (id == "not_extendable") {stat = spells[spell_id].not_extendable; }
	else if (id == "suspendable") {stat = spells[spell_id].suspendable; }
	else if (id == "viral_range") {stat = spells[spell_id].viral_range; }
	else if (id == "spellgroup") {stat = spells[spell_id].spellgroup; }
	else if (id == "rank") {stat = spells[spell_id].rank; }
	else if (id == "powerful_flag") {stat = spells[spell_id].powerful_flag; }
	else if (id == "CastRestriction") {stat = spells[spell_id].CastRestriction; }
	else if (id == "AllowRest") {stat = spells[spell_id].AllowRest; }
	else if (id == "InCombat") {stat = spells[spell_id].InCombat; }
	else if (id == "OutofCombat") {stat = spells[spell_id].OutofCombat; }
	else if (id == "aemaxtargets") {stat = spells[spell_id].aemaxtargets; }
	else if (id == "maxtargets") {stat = spells[spell_id].maxtargets; }
	else if (id == "persistdeath") {stat = spells[spell_id].persistdeath; }
	else if (id == "min_dist") {stat = static_cast<int32>(spells[spell_id].min_dist); }
	else if (id == "min_dist_mod") {stat = static_cast<int32>(spells[spell_id].min_dist_mod); }
	else if (id == "max_dist") {stat = static_cast<int32>(spells[spell_id].max_dist); }
	else if (id == "min_range") {stat = static_cast<int32>(spells[spell_id].min_range); }
	else if (id == "DamageShieldType") {stat = spells[spell_id].DamageShieldType; }

	return stat;
}

uint32 Mob::GetRaceStringID() {

	switch (GetRace()) {
		case HUMAN:
			return 1257; break;
		case BARBARIAN:
			return 1258; break;
		case ERUDITE:
			return 1259; break;
		case WOOD_ELF:
			return 1260; break;
		case HIGH_ELF:
			return 1261; break;
		case DARK_ELF:
			return 1262; break;
		case HALF_ELF:
			return 1263; break;
		case DWARF:
			return 1264; break;
		case TROLL:
			return 1265; break;
		case OGRE:
			return 1266; break;
		case HALFLING:
			return 1267; break;
		case GNOME:
			return 1268; break;
		case IKSAR:
			return 1269; break;
		case VAHSHIR:
			return 1270; break;
		//case FROGLOK:
		//	return "Frogloks"; break;
		default:
			return 1256; break;
	}
}

uint32 Mob::GetClassStringID() {

	switch (GetClass()) {
		case WARRIOR:
		case WARRIORGM:
			return 1240; break;
		case CLERIC:
		case CLERICGM:
			return 1241; break;
		case PALADIN:
		case PALADINGM:
			return 1242; break;
		case RANGER:
		case RANGERGM:
			return 1243; break;
		case SHADOWKNIGHT:
		case SHADOWKNIGHTGM:
			return 1244; break;
		case DRUID:
		case DRUIDGM:
			return 1245; break;
		case MONK:
		case MONKGM:
			return 1246; break;
		case BARD:
		case BARDGM:
			return 1247; break;
		case ROGUE:
		case ROGUEGM:
			return 1248; break;
		case SHAMAN:
		case SHAMANGM:
			return 1249; break;
		case NECROMANCER:
		case NECROMANCERGM:
			return 1250; break;
		case WIZARD:
		case WIZARDGM:
			return 1251; break;
		case MAGICIAN:
		case MAGICIANGM:
			return 1252; break;
		case ENCHANTER:
		case ENCHANTERGM:
			return 1253; break;
		case BEASTLORD:
		case BEASTLORDGM:
			return 1254; break;
		case BANKER:
			return 1255; break;
		default:
			return 1239; break;
	}
}

// Double attack and dual wield chances based on this data: http://www.eqemulator.org/forums/showthread.php?t=38708
uint8 Mob::DoubleAttackChance()
{
	uint8 level = GetLevel();

	if (!IsPet() || !GetOwner())
	{
		if (level > 59)
		return (level - 59) / 4 + 61;
	
		else if (level > 50)
			return 61;

		else if (level > 35)
			return (level - 35) / 2 + 44;

		else if (level < 6)
			return 0;

		else return level;
	}
	else
	{
		// beastlord and shaman pets have different double attack and dual wield rates
		if (GetOwner()->GetClass() == BEASTLORD || GetOwner()->GetClass() == SHAMAN)
		{
			if (level < 36)
				return level;
			else if (level < 42)
				return level * 1.2f;
			else
				return 48 + (level - 41) / 4;
		}
		else
		{
			if (level > 45)
				return (level - 45) / 4 + 53;
			else
				return level * 1.2f;
		}
	}

}

uint8 Mob::DualWieldChance()
{
	if (!IsPet() || !GetOwner())
	{
		return DoubleAttackChance() * 1.333f;
	}
	else
	{
		// dual wield chance is always about 1 and 1/3 times double attack chance except for these low level pets
		if (GetOwner()->GetClass() == BEASTLORD || GetOwner()->GetClass() == SHAMAN)
		{
			if (GetLevel() > 35)
				return DoubleAttackChance() * 1.333f;
			else if (GetLevel() < 25)
				return 0;
			else
				return (GetLevel() - 20) * 2;
		}
		else
		{
			return DoubleAttackChance() * 1.333f;
		}
	}
}

uint8 Mob::Disarm(float chance)
{
	if(this->IsNPC())
	{
		ServerLootItem_Struct* weapon = CastToNPC()->GetItem(MainPrimary);
		if(weapon)
		{
			float roll = zone->random.Real(0, 150);
			if (roll < chance) 
			{
				uint16 weaponid = weapon->item_id;
				const ItemInst* inst = database.CreateItem(weaponid);

				if (inst->GetItem()->Magic)
				{
					safe_delete(inst);
					return 0;				// magic weapons cannot be disarmed
				}

				const Item_Struct* item = inst->GetItem();
				int8 charges = item->MaxCharges;
				CastToNPC()->RemoveItem(weaponid, 1, MainPrimary);

				if (inst->GetItem()->NoDrop == 0)
				{
					CastToNPC()->AddLootDrop(item, &CastToNPC()->itemlist, charges, 1, 127, false, true);
				}
				else
				{
					entity_list.CreateGroundObject(weaponid, glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, DisarmDecayTime));
				}

				CastToNPC()->SetPrimSkill(SkillHandtoHand);
				if (!GetSpecialAbility(SPECATK_INNATE_DW) && !GetSpecialAbility(SPECATK_QUAD))
					can_dual_wield = false;

				WearChange(MaterialPrimary, 0, 0);

				safe_delete(inst);

				// NPC has a weapon and was disarmed.
				return 2;
			}
			else
			{
				// NPC has a weapon, but the roll failed.
				return 1;
			}
		}
	}
	return 0;
}

float Mob::CalcZOffset()
{
	float mysize = GetSize();
		if(mysize > RuleR(Map, BestZSizeMax))
		mysize = RuleR(Map, BestZSizeMax);

	return (RuleR(Map, BestZMultiplier) * mysize);
}

int32 Mob::GetSkillStat(SkillUseTypes skillid)
{

	if(EQEmu::IsSpellSkill(skillid))
	{
		if(GetCasterClass() == 'I')
		{
			return (GetINT() <= 190) ? GetINT() : 190;
		}
		else
		{
			return (GetWIS() <= 190) ? GetWIS() : 190;
		}
	}

	int32 stat = GetINT();
	bool penalty = true;
	switch (skillid) 
	{
		case Skill1HBlunt: 
		case Skill1HSlashing: 
		case Skill2HBlunt: 
		case Skill2HSlashing: 
		case SkillArchery: 
		case Skill1HPiercing: 
		case SkillHandtoHand: 
		case SkillThrowing: 
		case SkillApplyPoison: 
		case SkillDisarmTraps: 
		case SkillPickLock: 
		case SkillPickPockets:
		case SkillSenseTraps: 
		case SkillSafeFall: 
		case SkillHide: 
		case SkillSneak:
			stat = GetDEX();
			penalty = false;
			break;
		case SkillBackstab: 
		case SkillBash: 
		case SkillDisarm: 
		case SkillDoubleAttack:
		case SkillDragonPunch: 
		case SkillDualWield: 
		case SkillEagleStrike:
		case SkillFlyingKick: 
		case SkillKick: 
		case SkillOffense: 
		case SkillRoundKick:
		case SkillTigerClaw: 
		case SkillTaunt: 
		case SkillIntimidation: 
		case SkillFrenzy:			
			stat = GetSTR();
			penalty = false;
			break;
		case SkillBlock: 
		case SkillDefense: 
		case SkillDodge: 
		case SkillParry: 
		case SkillRiposte:
			stat = GetAGI();
			penalty = true;
			break;
		case SkillBegging:
			stat = GetCHA();
			penalty = false;
			break;
		case SkillSwimming:
			stat = GetSTA();
			penalty = true;
			break;
		default:
			penalty = true;
			break;
	}

	int16 higher_from_int_wis = (GetINT() > GetWIS()) ? GetINT() : GetWIS();
	stat = (higher_from_int_wis > stat) ? higher_from_int_wis : stat;

	if(penalty)
		stat -= 15;

	return (stat <= 190) ? stat : 190;
}

bool Mob::IsPlayableRace(uint16 race)
{
	if(race > 0 && (race <= GNOME || race == IKSAR || race == VAHSHIR))
	{
		return true;
	}

	return false;
}

float Mob::GetPlayerHeight(uint16 race)
{
	switch (race)
	{
		case OGRE:
			return 9;
		case TROLL:
			return 8;
		case VAHSHIR: case BARBARIAN:
			return 7;
		case HUMAN: case HIGH_ELF: case ERUDITE: case IKSAR:
			return 6;
		case HALF_ELF:
			return 5.5;
		case WOOD_ELF: case DARK_ELF: case WOLF: case ELEMENTAL:
			return 5;
		case DWARF:
			return 4;
		case HALFLING:
			return 3.5;
		case GNOME:
			return 3;
		default:
			return 6;
	}
}

float Mob::GetHeadingRadians()
{
	float headingRadians = GetHeading();
	headingRadians = (headingRadians * 360.0f) / 256.0f;	// convert to degrees first; heading range is 0-255
	if (headingRadians < 270)
		headingRadians += 90;
	else
		headingRadians -= 270;
	
	return headingRadians * 3.141592f / 180.0f;
}

bool Mob::IsFacingTarget()
{
	if (!target)
		return false;

	float heading = GetHeadingRadians();
	float headingUnitVx = cosf(heading);
	float headingUnitVy = sinf(heading);

	float toTargetVx = -GetTarget()->GetPosition().x - -m_Position.x;
	float toTargetVy = GetTarget()->GetPosition().y - m_Position.y;

	float distance = sqrtf(toTargetVx * toTargetVx + toTargetVy * toTargetVy);

	float dotp = headingUnitVx * (toTargetVx / distance) +
		headingUnitVy * (toTargetVy / distance);

	if (dotp > 0.95f)
		return true;

	return false;
}

bool Mob::CanCastBindAffinity()
{
	uint8 class_ = GetClass();
	uint8 level = GetLevel();

	if(level >= 12 && (class_ == NECROMANCER || class_ == WIZARD || class_ == MAGICIAN || class_ == ENCHANTER))
	{
		return true;
	}
	else if(level >= 14 && (class_ == CLERIC || class_ == SHAMAN || class_ == DRUID))
	{
		return true;
	}
	else
	{
		return false;
	}
}
