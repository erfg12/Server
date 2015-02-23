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
#ifndef EQ_PACKET_STRUCTS_H
#define EQ_PACKET_STRUCTS_H

#include "types.h"
#include <string.h>
#include <string>
#include <list>
#include <time.h>
#include "../common/version.h"
//#include "../common/item_struct.h"

static const uint32 BUFF_COUNT = 25;
static const uint32 MAX_MERC = 100;
static const uint32 MAX_MERC_GRADES = 10;
static const uint32 MAX_MERC_STANCES = 10;
static const uint32 BLOCKED_BUFF_COUNT = 20;

//#include "eq_constants.h"
#include "eq_dictionary.h"

/*
** Compiler override to ensure
** byte aligned structures
*/
#pragma pack(1)

struct LoginInfo_Struct {
/*000*/	char	login_info[64];
/*064*/	uint8	unknown064[124];
/*188*/	uint8	zoning;			// 01 if zoning, 00 if not
/*189*/	uint8	unknown189[275];
/*488*/
};

struct EnterWorld_Struct {
/*000*/	char	name[64];
};

struct ExpansionInfo_Struct {
/*0000*/	uint32	Expansions;
};

/* Name Approval Struct */
/* Len: */
/* Opcode: 0x8B20*/
struct NameApproval
{
	char name[64];
	uint32 race;
	uint32 class_;
	uint32 deity;
};

/*
** Entity identification struct
** Size: 4 bytes
** OPCodes: OP_DeleteSpawn, OP_Assist
*/
struct EntityId_Struct
{
/*00*/	uint32	entity_id;
/*04*/
};

struct Duel_Struct
{
	uint16 duel_initiator;
	uint16 duel_target;
};

struct DuelResponse_Struct
{
	uint32 target_id;
	uint32 entity_id;
	uint32 unknown;
};

///////////////////////////////////////////////////////////////////////////////


/*
** Color_Struct
** Size: 4 bytes
** Used for convenience
** Merth: Gave struct a name so gcc 2.96 would compile
**
*/
struct Color_Struct
{
	union
	{
		struct
		{
			uint8	blue;
			uint8	green;
			uint8	red;
			uint8	use_tint;	// if there's a tint this is FF
		} rgb;
		uint32 color;
	};
};

/*
** Character Selection Struct
** Length: 1704 Bytes
**
*/
struct CharacterSelect_Struct {
/*0000*/	uint32	race[10];				// Characters Race
/*0040*/	Color_Struct	cs_colors[10][9];	// Characters Equipment Colors
/*0400*/	uint8	beardcolor[10];			// Characters beard Color
/*0410*/	uint8	hairstyle[10];			// Characters hair style
/*0420*/	uint32	equip[10][9];			// 0=helm, 1=chest, 2=arm, 3=bracer, 4=hand, 5=leg, 6=boot, 7=melee1, 8=melee2 (Might not be)
/*0780*/	uint32	secondary[10];			// Characters secondary IDFile number
/*0940*/	uint32	deity[10];				// Characters Deity
/*1000*/	uint8	beard[10];				// Characters Beard Type
/*1010*/	uint8	unknown902[10];			// 10x ff
/*1020*/	uint32	primary[10];			// Characters primary IDFile number
/*1060*/	uint8	haircolor[10];			// Characters Hair Color
/*1070*/	uint8	unknown0962[2];			// 2x 00
/*1072*/	uint32	zone[10];				// Characters Current Zone
/*1112*/	uint8	class_[10];				// Characters Classes
/*1022*/	uint8	face[10];				// Characters Face Type
/*1032*/	char	name[10][64];			// Characters Names
/*1672*/	uint8	gender[10];				// Characters Gender
/*1682*/	uint8	eyecolor1[10];			// Characters Eye Color
/*1692*/	uint8	eyecolor2[10];			// Characters Eye 2 Color
/*1702*/	uint8	level[10];				// Characters Levels
/*1712*/
};

/*
** Generic Spawn Struct
** Length: 257 Bytes
** Fields from old struct not yet found:
**	float	size;
**	float	walkspeed;	// probably one of the ff 33 33 33 3f
**	float	runspeed;	// probably one of the ff 33 33 33 3f
**	uint8	traptype;	// 65 is disarmable trap, 66 and 67 are invis triggers/traps
**	uint8	npc_armor_graphic;	// 0xFF=Player, 0=none, 1=leather, 2=chain, 3=steelplate
**	uint8	npc_helm_graphic;	// 0xFF=Player, 0=none, 1=leather, 2=chain, 3=steelplate
**
*/

/*
** Generic Spawn Struct
** Length: 383 Octets
** Used in:
** spawnZoneStruct
** dbSpawnStruct
** petStruct
** newSpawnStruct
*/
/*
showeq -> eqemu
sed -e 's/_t//g' -e 's/seto_0xFF/set_to_0xFF/g'
*/
struct Spawn_Struct {
/*0000*/ uint8 unknown0000;
/*0001*/ uint8	gm;					// 0=no, 1=gm
/*0002*/ uint8	unknown0003;
/*0003*/ uint8	aaitle;				// 0=none, 1=general, 2=archtype, 3=class
/*0004*/ uint8	unknown0004;
/*0005*/ uint8	anon;				// 0=normal, 1=anon, 2=roleplay
/*0006*/ uint8	face;				// Face id for players
/*0007*/ char	name[64];			// Player's Name
/*0071*/ uint16	deity;				// Player's Deity
/*0073*/ uint16 unknown0073;
/*0075*/ float	size;				// Model size
/*0079*/ uint32	unknown0079;
/*0083*/ uint8	NPC;				// 0=player,1=npc,2=pc corpse,3=npc corpse,a
/*0084*/ uint8	invis;				// Invis (0=not, 1=invis)
/*0085*/ uint8	haircolor;			// Hair color
/*0086*/ uint8	curHp;				// Current hp %%% wrong
/*0087*/ uint8	max_hp;				// (name prolly wrong)takes on the value 100 for players, 100 or 110 for NPCs and 120 for PC corpses...
/*0088*/ uint8	findable;			// 0=can't be found, 1=can be found
/*0089*/ uint8	unknown0089[5];
/*0094*/ signed	deltaHeading:10;	// change in heading
/*????*/ signed	x:19;				// x coord
/*????*/ signed	padding0054:3;		// ***Placeholder
/*0098*/ signed	y:19;				// y coord
/*????*/ signed	animation:10;		// animation
/*????*/ signed	padding0058:3;		// ***Placeholder
/*0102*/ signed	z:19;				// z coord
/*????*/ signed	deltaY:13;			// change in y
/*0106*/ signed	deltaX:13;			// change in x
/*????*/ unsigned	heading:12;		// heading
/*????*/ signed	padding0066:7;		// ***Placeholder
/*0110*/ signed	deltaZ:13;			// change in z
/*????*/ signed	padding0070:19;		// ***Placeholder
/*0114*/ uint8	eyecolor1;			// Player's left eye color
/*0115*/ uint8	unknown0115[11];	// Was [24]
/*0126*/ uint8	StandState;	// stand state for SoF+ 0x64 for normal animation
/*0139*/ uint8	showhelm;			// 0=no, 1=yes
/*0140*/ uint8	unknown0140[4];
/*0144*/ uint8	is_npc;				// 0=no, 1=yes
/*0145*/ uint8	hairstyle;			// Hair style
/*0146*/ uint8	beard;				// Beard style (not totally, sure but maybe!)
/*0147*/ uint8	unknown0147[4];
/*0151*/ uint8	level;				// Spawn Level
/*0152*/ uint8	unknown0259[4];		// ***Placeholder
/*0156*/ uint8	beardcolor;			// Beard color
/*0157*/ char	suffix[32];			// Player's suffix (of Veeshan, etc.)
/*0189*/ uint32	petOwnerId;			// If this is a pet, the spawn id of owner
/*0193*/ uint8	guildrank;			// 0=normal, 1=officer, 2=leader
/*0194*/ uint8	unknown0194[3];
/*0197*/ union
		 {
			struct
			{
				/*0197*/ uint32 equip_helmet;		// Equipment: Helmet Visual
				/*0201*/ uint32 equip_chest;		// Equipment: Chest Visual
				/*0205*/ uint32 equip_arms;			// Equipment: Arms Visual
				/*0209*/ uint32 equip_bracers;		// Equipment: Bracers Visual
				/*0213*/ uint32 equip_hands;		// Equipment: Hands Visual
				/*0217*/ uint32 equip_legs;			// Equipment: Legs Visual
				/*0221*/ uint32 equip_feet;			// Equipment: Feet Visual
				/*0225*/ uint32 equip_primary;		// Equipment: Primary Visual
				/*0229*/ uint32 equip_secondary;	// Equipment: Secondary Visual
			} equip;
			/*0197*/ uint32 equipment[_MaterialCount]; // Array elements correspond to struct equipment above
		 };
/*0233*/ float	runspeed;		// Speed when running
/*0036*/ uint8	afk;			// 0=no, 1=afk
/*0238*/ uint32	guildID;		// Current guild
/*0242*/ char	title[32];		// Title
/*0274*/ uint8	unknown0274;	// non-zero prefixes name with '!'
/*0275*/ uint8	set_to_0xFF[8];	// ***Placeholder (all ff)
/*0283*/ uint8	helm;			// Helm texture
/*0284*/ uint32	race;			// Spawn race
/*0288*/ uint32	unknown0288;
/*0292*/ char	lastName[32];	// Player's Lastname
/*0324*/ float	walkspeed;		// Speed when walking
/*0328*/ uint8	unknown0328;
/*0329*/ uint8	is_pet;			// 0=no, 1=yes
/*0330*/ uint8	light;			// Spawn's lightsource %%% wrong
/*0331*/ uint8	class_;			// Player's class
/*0332*/ uint8	eyecolor2;		// Left eye color
/*0333*/ uint8	flymode;
/*0334*/ uint8	gender;			// Gender (0=male, 1=female)
/*0335*/ uint8	bodytype;		// Bodytype
/*0336*/ uint8 unknown0336[3];
union
{
/*0339*/ uint8 equip_chest2;	// Second place in packet for chest texture (usually 0xFF in live packets)
								// Not sure why there are 2 of them, but it effects chest texture!
/*0339*/ uint8 mount_color;		// drogmor: 0=white, 1=black, 2=green, 3=red
								// horse: 0=brown, 1=white, 2=black, 3=tan
};
/*0340*/ uint32 spawnId;		// Spawn Id
/*0344*/ uint8 unknown0344[4];
/*0348*/ union
		 {
			struct
			{
				/*0348*/ Color_Struct color_helmet;		// Color of helmet item
				/*0352*/ Color_Struct color_chest;		// Color of chest item
				/*0356*/ Color_Struct color_arms;		// Color of arms item
				/*0360*/ Color_Struct color_bracers;	// Color of bracers item
				/*0364*/ Color_Struct color_hands;		// Color of hands item
				/*0368*/ Color_Struct color_legs;		// Color of legs item
				/*0372*/ Color_Struct color_feet;		// Color of feet item
				/*0376*/ Color_Struct color_primary;	// Color of primary item
				/*0380*/ Color_Struct color_secondary;	// Color of secondary item
			} equipment_colors;
			/*0348*/ Color_Struct colors[_MaterialCount]; // Array elements correspond to struct equipment_colors above
		 };
/*0384*/ uint8	lfg;			// 0=off, 1=lfg on
/*0385*/

	bool DestructibleObject;	// Only used to flag as a destrible object
	char DestructibleModel[64];	// Model of the Destructible Object - Required - Seen "DEST_TNT_G"
	char DestructibleName2[64];	// Secondary name - Not Required - Seen "a_tent"
	char DestructibleString[64];	// Unknown - Not Required - Seen "ZoneActor_01186"
	uint32 DestructibleAppearance;	// Damage Appearance
	uint32 DestructibleUnk1;
	uint32 DestructibleID1;
	uint32 DestructibleID2;
	uint32 DestructibleID3;
	uint32 DestructibleID4;
	uint32 DestructibleUnk2;
	uint32 DestructibleUnk3;
	uint32 DestructibleUnk4;
	uint32 DestructibleUnk5;
	uint32 DestructibleUnk6;
	uint32 DestructibleUnk7;
	uint8 DestructibleUnk8;
	uint32 DestructibleUnk9;
	uint32 zoneID; // for mac.

};

struct LFG_Struct {
	char	name[64];
	int32	value;
};

struct LFG_Appearance_Struct {
	int16	entityid;
	int16	unknown;
	int32	value;
};

/*
** New Spawn
** Length: 176 Bytes
** OpCode: 4921
*/
struct NewSpawn_Struct
{
	struct Spawn_Struct spawn;	// Spawn Information
};

struct ClientZoneEntry_Struct {
/*0000*/	uint32	unknown00;
/*0004*/	char	char_name[64];			// Character Name
};


/*
** Server Zone Entry Struct
** OPCodes: OP_ServerZoneEntry
**
*/
struct ServerZoneEntry_Struct
{
	struct NewSpawn_Struct player;
};

struct NewZone_Struct {
/*0000*/	char	char_name[64];			// Character Name
/*0064*/	char	zone_short_name[32];	// Zone Short Name
/*0096*/	char	zone_long_name[278];	// Zone Long Name
/*0374*/	uint8	ztype;					// Zone type (usually FF)
/*0375*/	uint8	fog_red[4];				// Zone fog (red)
/*0379*/	uint8	fog_green[4];			// Zone fog (green)
/*0383*/	uint8	fog_blue[4];			// Zone fog (blue)
/*0387*/	uint8	unknown323;
/*0388*/	float	fog_minclip[4];
/*0404*/	float	fog_maxclip[4];
/*0420*/	float	gravity;
/*0424*/	uint8	time_type;
/*0425*/    uint8   rain_chance[4];
/*0429*/    uint8   rain_duration[4];
/*0433*/    uint8   snow_chance[4];
/*0437*/    uint8   snow_duration[4];
/*0441*/	uint8	specialdates[16];
/*0457*/	uint8	specialcodes[16];
/*0473*/	uint8	timezone;
/*0474*/	uint8	sky;					// Sky Type
/*0475*/	uint8	unknown331[13];			// ***Placeholder
/*0488*/	float	zone_exp_multiplier;	// Experience Multiplier
/*0492*/	float	safe_y;					// Zone Safe Y
/*0496*/	float	safe_x;					// Zone Safe X
/*0500*/	float	safe_z;					// Zone Safe Z
/*0504*/	float	max_z;					// Guessed
/*0508*/	float	underworld;				// Underworld, min z (Not Sure?)
/*0512*/	float	minclip;				// Minimum View Distance
/*0516*/	float	maxclip;				// Maximum View DIstance
/*0520*/	uint32	skylock;				// This is the wrong position, just for EQMac's benefit atm.
/*0524*/	uint8	unknown_end[80];		// ***Placeholder
/*0604*/	char	zone_short_name2[68];
/*0672*/	char	unknown672[12];
/*0684*/	uint16	zone_id;
/*0686*/	uint16	zone_instance;
/*0688*/	uint32	unknown688;
/*0692*/	uint8	unknown692[8];
/*0700*/	float	fog_density;
/*0704*/	uint32	SuspendBuffs;
/*0704*/
};

/*
** Memorize Spell Struct
** Length: 12 Bytes
**
*/
struct MemorizeSpell_Struct {
uint32 slot;		// Spot in the spell book/memorized slot
uint32 spell_id;	// Spell id (200 or c8 is minor healing, etc)
uint32 scribing;	// 1 if memorizing a spell, set to 0 if scribing to book, 2 if un-memming
uint32 unknown12;
};

/*
** Make Charmed Pet
** Length: 12 Bytes
**
*/
struct Charm_Struct {
/*00*/	uint32	owner_id;
/*04*/	uint32	pet_id;
/*08*/	uint32	command; // 1: make pet, 0: release pet
/*12*/
};

struct InterruptCast_Struct
{
	uint16 messageid;
	uint16 color;
	char	message[0];
};

struct DeleteSpell_Struct
{
/*000*/int16	spell_slot;
/*002*/uint8	unknowndss002[2];
/*004*/uint8	success;
/*005*/uint8	unknowndss006[3];
/*008*/
};

struct ManaChange_Struct
{
	uint32	new_mana; // New Mana AMount
	uint32	stamina;
	uint32	spell_id;
	uint32	unknown12;
};

struct SwapSpell_Struct
{
	uint32 from_slot;
	uint32 to_slot;


};

struct BeginCast_Struct
{
	// len = 8
/*000*/	uint16	caster_id;
/*002*/	uint16	spell_id;
/*004*/	uint32	cast_time;		// in miliseconds
};

struct CastSpell_Struct
{
	uint32	slot;
	uint32	spell_id;
	uint32	inventoryslot; // slot for clicky item, 0xFFFF = normal cast
	uint32	target_id;
	uint32  cs_unknown1;
	uint32  cs_unknown2;
 	float   y_pos;
 	float   x_pos;
	float   z_pos;
};

struct SpellEffect_Struct
{
/*000*/	uint32 EffectID;
/*004*/	uint32 EntityID;
/*008*/	uint32 EntityID2;	// EntityID again
/*012*/	uint32 Duration;		// In Milliseconds
/*016*/	uint32 FinishDelay;	// In Milliseconds - delay for final part of spell effect
/*020*/	uint32 Unknown020;	// Seen 3000
/*024*/ uint8 Unknown024;	// Seen 1 for SoD
/*025*/ uint8 Unknown025;	// Seen 1 for Live
/*026*/ uint16 Unknown026;	// Seen 1157 and 1177 - varies per char
/*028*/
};


/*
** SpawnAppearance_Struct
** Changes client appearance for all other clients in zone
** Size: 8 bytes
** Used in: OP_SpawnAppearance
**
*/
struct SpawnAppearance_Struct
{
/*0000*/ uint16 spawn_id;		// ID of the spawn
/*0002*/ uint16 type;			// Values associated with the type
/*0004*/ uint32 parameter;		// Type of data sent
/*0008*/
};


// solar: this is used inside profile
struct SpellBuff_Struct
{
/*000*/	uint8	slotid;		//badly named... seems to be 2 for a real buff, 0 otherwise
/*001*/ uint8	level;
/*002*/	uint8	bard_modifier;
/*003*/	uint8	effect;			//not real
/*004*/	uint32	spellid;
/*008*/ uint32	duration;
/*012*/	uint32	counters;
/*016*/	uint32	player_id;	//'global' ID of the caster, for wearoff messages
/*020*/
};

struct SpellBuffFade_Struct {
/*000*/	uint32 entityid;
/*004*/	uint8 slot;
/*005*/	uint8 level;
/*006*/	uint8 effect;
/*007*/	uint8 unknown7;
/*008*/	uint32 spellid;
/*012*/	uint32 duration;
/*016*/	uint32 num_hits;
/*020*/	uint32 unknown020;	//prolly global player ID
/*024*/	uint32 slotid;
/*028*/	uint32 bufffade;
/*032*/
};

// Length: 10
struct ItemProperties_Struct {

uint8	unknown01[2];
uint8	charges;
uint8	unknown02[13];
};

struct GMTrainee_Struct
{
	/*000*/ uint32 npcid;
	/*004*/ uint32 playerid;
	/*008*/ uint32 skills[PACKET_SKILL_ARRAY_SIZE];
	/*408*/ uint8 unknown408[40];
	/*448*/
};

struct OldGMTrainee_Struct{
	/*000*/ uint16 npcid;
	/*002*/	uint16 playerid;
	/*004*/ uint16 skills[74];
	/*152*/ uint8  unknown154[52];
	/*204*/	float  greed;
	/*208*/ uint8  unknown208; //Always 0x01
	/*209*/	uint8  language[32];
	/*241*/	uint8  ending[3]; //Copied from client packet (probably void)
	/*244*/
};

struct GMTrainEnd_Struct
{
	/*000*/ uint32 npcid;
	/*004*/ uint32 playerid;
	/*008*/
};

struct GMSkillChange_Struct {
/*000*/	uint16		npcid;
/*002*/ uint16		playerid;
/*004*/ uint16		skillbank;		// 0 if normal skills, 1 if languages
/*006*/ uint16		unknown2;
/*008*/ uint16		skill_id;
/*010*/ uint16		unknown3;		//probably void
};

struct ConsentResponse_Struct {
	char grantname[64];
	char ownername[64];
	uint8 permission;
	char zonename[32];
};

/*
** Name Generator Struct
** Length: 72 bytes
** OpCode: 0x0290
*/
struct NameGeneration_Struct
{
/*0000*/	uint32	race;
/*0004*/	uint32	gender;
/*0008*/	char	name[64];
/*0072*/
};

/*
** Character Creation struct
** Length: 140 Bytes
** OpCode: 0x0113
*/
struct CharCreate_Struct
{
/*0000*/	uint32	class_;
/*0004*/	uint32	haircolor;	// Might be hairstyle
/*0008*/	uint32	beardcolor;	// Might be beard
/*0012*/	uint32	beard;		// Might be beardcolor
/*0016*/	uint32	gender;
/*0020*/	uint32	race;
/*0024*/	uint32	start_zone;
	// 0 = odus
	// 1 = qeynos
	// 2 = halas
	// 3 = rivervale
	// 4 = freeport
	// 5 = neriak
	// 6 = gukta/grobb
	// 7 = ogguk
	// 8 = kaladim
	// 9 = gfay
	// 10 = felwithe
	// 11 = akanon
	// 12 = cabalis
	// 13 = shar vahl
/*0028*/	uint32	hairstyle;	// Might be haircolor
/*0032*/	uint32	deity;
/*0036*/	uint32	STR;
/*0040*/	uint32	STA;
/*0044*/	uint32	AGI;
/*0048*/	uint32	DEX;
/*0052*/	uint32	WIS;
/*0056*/	uint32	INT;
/*0060*/	uint32	CHA;
/*0064*/	uint16	oldface;		// Could be unknown0076
/*006*/		uint16  face;
/*0068*/	uint32	eyecolor1;	//its possiable we could have these switched
/*0073*/	uint32	eyecolor2;	//since setting one sets the other we really can't check
};

/*
 *Used in PlayerProfile
 */
struct AA_Array
{
	uint32 AA;
	uint32 value;
};

typedef struct
{
	/*00*/ char Name[64];
	/*64*/ uint32 Level;
	/*68*/ uint32 Race;
	/*72*/ uint32 Class;
	/*76*/ uint32 Zone;
	/*80*/ uint32 Time;
	/*84*/ uint32 Points;
	/*88*/
} PVPStatsEntry_Struct;

static const uint32 MAX_PP_DISCIPLINES = 100;
static const uint32 MAX_DISCIPLINE_TIMERS = 20;

struct Disciplines_Struct {
	uint32 values[MAX_PP_DISCIPLINES];
};

struct ClientDiscipline_Struct {
    uint8	disc_id;	// There are only a few discs < 60
    uint8	unknown3[3];	// Which leaves room for ??
};

struct MovePotionToBelt_Struct {
	uint32	Action;
	uint32	SlotNumber;
	uint32	ItemID;
};

static const uint32 MAX_GROUP_LEADERSHIP_AA_ARRAY = 16;
static const uint32 MAX_RAID_LEADERSHIP_AA_ARRAY = 16;
static const uint32 MAX_LEADERSHIP_AA_ARRAY = (MAX_GROUP_LEADERSHIP_AA_ARRAY+MAX_RAID_LEADERSHIP_AA_ARRAY);
struct LeadershipAA_Struct {
	uint32 ranks[MAX_LEADERSHIP_AA_ARRAY];
};
struct GroupLeadershipAA_Struct {
	uint32 ranks[MAX_GROUP_LEADERSHIP_AA_ARRAY];
};
struct RaidLeadershipAA_Struct {
	uint32 ranks[MAX_RAID_LEADERSHIP_AA_ARRAY];
};

 /**
* A bind point.
* Size: 20 Octets
*/
struct BindStruct {
	/*000*/ uint32 zoneId;
	/*004*/ float x;
	/*008*/ float y;
	/*012*/ float z;
	/*016*/ float heading;
	/*020*/ uint32 instance_id;
	/*024*/
};

struct SuspendedMinion_Struct
{
	/*000*/	uint16 SpellID;
	/*002*/	uint32 HP;
	/*006*/	uint32 Mana;
	/*010*/	SpellBuff_Struct Buffs[BUFF_COUNT];
	/*510*/	uint32 Items[_MaterialCount];
	/*546*/	char Name[64];
	/*610*/
};


/*
** Player Profile
**
** Length: 4308 bytes
** OpCode: 0x006a
 */
static const uint32 MAX_PP_LANGUAGE = 28;
static const uint32 MAX_PP_SPELLBOOK = 480;	// Set for all functions
static const uint32 MAX_PP_MEMSPELL = 9; // Set to latest client so functions can work right
static const uint32 MAX_PP_REF_SPELLBOOK = 480;	// Set for Player Profile size retain
static const uint32 MAX_PP_REF_MEMSPELL = 9; // Set for Player Profile size retain

static const uint32 MAX_PP_SKILL		= PACKET_SKILL_ARRAY_SIZE;	// 100 - actual skills buffer size
static const uint32 MAX_PP_AA_ARRAY		= 240;
static const uint32 MAX_GROUP_MEMBERS	= 6;
static const uint32 MAX_RECAST_TYPES	= 20;

/*
showeq -> eqemu
sed -e 's/_t//g' -e 's/MAX_AA/MAX_PP_AA_ARRAY/g' \
	-e 's/MAX_SPELL_SLOTS/MAX_PP_MEMSPELL/g' \
	-e 's/MAX_KNOWN_SKILLS/MAX_PP_SKILL/g' \
	-e 's/MAXRIBUTES/MAX_PLAYER_TRIBUTES/g' \
	-e 's/MAX_BUFFS/BUFF_COUNT/g' \
	-e 's/MAX_KNOWN_LANGS/MAX_PP_LANGUAGE/g' \
	-e 's/MAX_RECASTYPES/MAX_RECAST_TYPES/g' \
	-e 's/spellBuff/SpellBuff_Struct/g' \
	-e 's/lastName/last_name/g' \
	-e 's/guildID/guildid/g' \
	-e 's/itemint/item_tint/g' \
	-e 's/MANA/mana/g' \
	-e 's/curHp/cur_hp/g' \
	-e 's/sSpellBook/spell_book/g' \
	-e 's/sMemSpells/mem_spells/g' \
	-e 's/uint32[ \t]*disciplines\[MAX_DISCIPLINES\]/Disciplines_Struct disciplines/g' \
	-e 's/aa_unspent/aapoints/g' \
	-e 's/aa_spent/aapoints_spent/g' \
	-e 's/ldon_guk_points/ldon_points_guk/g' \
	-e 's/ldon_mir_points/ldon_points_mir/g' \
	-e 's/ldon_mmc_points/ldon_points_mmc/g' \
	-e 's/ldon_ruj_points/ldon_points_ruj/g' \
	-e 's/ldonak_points/ldon_points_tak/g' \
	-e 's/ldon_avail_points/ldon_points_available/g' \
	-e 's/expGroupLeadAA/group_leadership_exp/g' \
	-e 's/expRaidLeadAA/raid_leadership_exp/g' \
	-e 's/groupLeadAAUnspent/group_leadership_points/g' \
	-e 's/raidLeadAAUnspent/raid_leadership_points/g' \
	-e 's/uint32[ \t]*leadershipAAs\[MAX_LEAD_AA\]/LeadershipAA_Struct leader_abilities/g' \
	-e 's/birthdayTime/birthday/g' \
	-e 's/lastSaveTime/lastlogin/g' \
	-e 's/zoneId/zone_id/g' \
	-e 's/hunger/hunger_level/g' \
	-e 's/thirst/thirst_level/g' \
	-e 's/guildstatus/guildrank/g' \
	-e 's/airRemaining/air_remaining/g' \
 */

struct PlayerProfile_Struct
{
/*0000*/	uint32				checksum;			// Checksum from CRC32::SetEQChecksum
/*0004*/	char				name[64];			// Name of player sizes not right
/*0068*/	char				last_name[32];		// Last name of player sizes not right
/*0100*/	uint32				gender;				// Player Gender - 0 Male, 1 Female
/*0104*/	uint32				race;				// Player race
/*0108*/	uint32				class_;				// Player class
/*0112*/	uint32				unknown0112;		//
/*0116*/	uint32				level;				// Level of player (might be one byte)
/*0120*/	BindStruct			binds[5];			// Bind points (primary is first, home city is fifth)
/*0220*/	uint32				deity;				// deity
/*0224*/	uint32				guild_id;
/*0228*/	uint32				birthday;			// characters bday
/*0232*/	uint32				lastlogin;			// last login or zone time
/*0236*/	uint32				timePlayedMin;		// in minutes
/*0240*/	uint8				pvp;
/*0241*/	uint8				level2;				//no idea why this is here, but thats how it is on live
/*0242*/	uint8				anon;				// 2=roleplay, 1=anon, 0=not anon
/*0243*/	uint8				gm;
/*0244*/	uint8				guildrank;
/*0245*/	uint8				guildbanker;
/*0246*/	uint8				unknown0246[6];		//
/*0252*/	uint32				intoxication;
/*0256*/	uint32				spellSlotRefresh[MAX_PP_REF_MEMSPELL];	//in ms
/*0292*/	uint32				abilitySlotRefresh;
/*0296*/	uint8				haircolor;			// Player hair color
/*0297*/	uint8				beardcolor;			// Player beard color
/*0298*/	uint8				eyecolor1;			// Player left eye color
/*0299*/	uint8				eyecolor2;			// Player right eye color
/*0300*/	uint8				hairstyle;			// Player hair style
/*0301*/	uint8				beard;				// Beard type
/*0302*/	uint8				ability_time_seconds;	//The following four spots are unknown right now.....
/*0303*/	uint8				ability_number;		//ability used
/*0304*/	uint8				ability_time_minutes;
/*0305*/	uint8				ability_time_hours;	//place holder
/*0306*/	uint8				unknown0306[6];		// @bp Spacer/Flag?
/*0312*/	uint32				item_material[_MaterialCount];	// Item texture/material of worn/held items
/*0348*/	uint8				unknown0348[44];
/*0392*/	Color_Struct		item_tint[_MaterialCount];
/*0428*/	AA_Array			aa_array[MAX_PP_AA_ARRAY];
/*2348*/	float				unknown2384;		//seen ~128, ~47
/*2352*/	char				servername[32];		// length probably not right
/*2384*/	char				title[32];			// length might be wrong
/*2416*/	char				suffix[32];			// length might be wrong
/*2448*/	uint32				guildid2;			//
/*2452*/	uint32				exp;				// Current Experience
/*2456*/	uint32				unknown2492;
/*2460*/	uint32				points;				// Unspent Practice points
/*2464*/	uint32				mana;				// current mana
/*2468*/	uint32				cur_hp;				// current hp
/*2472*/	uint32				famished;
/*2476*/	uint32				STR;				// Strength
/*2480*/	uint32				STA;				// Stamina
/*2484*/	uint32				CHA;				// Charisma
/*2488*/	uint32				DEX;				// Dexterity
/*2492*/	uint32				INT;				// Intelligence
/*2496*/	uint32				AGI;				// Agility
/*2500*/	uint32				WIS;				// Wisdom
/*2504*/	uint8				face;				// Player face
/*2505*/	uint8				unknown2541[47];	// ?
/*2552*/	uint8				languages[MAX_PP_LANGUAGE];
/*2580*/	uint8				unknown2616[4];
/*2584*/	uint32				spell_book[MAX_PP_REF_SPELLBOOK];
/*4504*/	uint8				unknown4540[128];	// Was [428] all 0xff
/*4632*/	uint32				mem_spells[MAX_PP_REF_MEMSPELL];
/*4668*/	uint8				unknown4704[32];	//
/*4700*/	float				y;					// Player y position
/*4704*/	float				x;					// Player x position
/*4708*/	float				z;					// Player z position
/*4712*/	float				heading;			// Direction player is facing
/*4716*/	uint8				unknown4752[4];		//
/*4720*/	int32				platinum;			// Platinum Pieces on player
/*4724*/	int32				gold;				// Gold Pieces on player
/*4728*/	int32				silver;				// Silver Pieces on player
/*4732*/	int32				copper;				// Copper Pieces on player
/*4736*/	int32				platinum_bank;		// Platinum Pieces in Bank
/*4740*/	int32				gold_bank;			// Gold Pieces in Bank
/*4744*/	int32				silver_bank;		// Silver Pieces in Bank
/*4748*/	int32				copper_bank;		// Copper Pieces in Bank
/*4752*/	int32				platinum_cursor;	// Platinum on cursor
/*4756*/	int32				gold_cursor;		// Gold on cursor
/*4760*/	int32				silver_cursor;		// Silver on cursor
/*4764*/	int32				copper_cursor;		// Copper on cursor
/*4768*/	int32				platinum_shared;	// Platinum shared between characters
/*4772*/	uint8				unknown4808[24];
/*4796*/	uint32				skills[MAX_PP_SKILL];	// [400] List of skills	// 100 dword buffer
/*5196*/	uint8				unknown5132[184];
/*5380*/	uint32				pvp2;				//
/*5384*/	uint32				unknown5420;		//
/*5388*/	uint32				pvptype;			//
/*5392*/	uint32				unknown5428;		//
/*5396*/	uint32				ability_down;		// Guessing
/*5400*/	uint8				unknown5436[8];		//
/*5408*/	uint32				autosplit;			//not used right now
/*5412*/	uint8				unknown5448[6];
/*5418*/	uint16				boatid;
/*5420*/	uint32				zone_change_count;	// Number of times user has zoned in their career (guessing)
/*5424*/	char				unknown5424[16];	//
/*5452*/	uint32				expansions;			// expansion setting, bit field of expansions avaliable
/*5456*/	int32				toxicity;			//from drinking potions, seems to increase by 3 each time you drink
/*5460*/	char				unknown5496[16];	//
/*5476*/	int32				hunger_level;
/*5480*/	int32				thirst_level;
/*5484*/	uint32				ability_up;
/*5488*/	char				unknown5524[16];
/*5504*/	uint16				zone_id;			// Current zone of the player
/*5506*/	uint16				zoneInstance;		// Instance ID
/*5508*/	SpellBuff_Struct	buffs[BUFF_COUNT];	// Buffs currently on the player
/*6008*/	char				groupMembers[6][64];//
			char				boat[20];
/*6392*/	char				unknown6428[636];
/*7048*/	uint32				entityid;
/*7052*/	uint32				leadAAActive;
/*7056*/	uint32				unknown7092;
/*7124*/	uint8				unknown7160[72];
/*7200*/	uint32				showhelm;
/*7208*/	uint32				unknown7244;
/*7216*/	uint32				unknown7252;
/*7264*/	Disciplines_Struct	disciplines;
/*7664*/	uint32				recastTimers[MAX_RECAST_TYPES];	// Timers (GMT of last use)
/*7744*/	char				unknown7780[160];
/*7904*/	uint32				endurance;
/*7908*/	uint32				group_leadership_exp;	//0-1000
/*7912*/	uint32				raid_leadership_exp;	//0-2000
/*7916*/	uint32				group_leadership_points;
/*7920*/	uint32				raid_leadership_points;
/*7924*/	LeadershipAA_Struct	leader_abilities;
/*8052*/	uint8				unknown8088[132];
/*8184*/	uint32				air_remaining;
/*8188*/	uint32				PVPKills;
/*8192*/	uint32				PVPDeaths;
/*8196*/	uint32				PVPCurrentPoints;
/*8200*/	uint32				PVPCareerPoints;
/*8204*/	uint32				PVPBestKillStreak;
/*8208*/	uint32				PVPWorstDeathStreak;
/*8212*/	uint32				PVPCurrentKillStreak;
/*8216*/	PVPStatsEntry_Struct	PVPLastKill;
/*8304*/	PVPStatsEntry_Struct	PVPLastDeath;
/*8392*/	uint32				PVPNumberOfKillsInLast24Hours;
/*8396*/	PVPStatsEntry_Struct	PVPRecentKills[50];
/*12796*/	uint32				aapoints_spent;
/*12800*/	uint32				expAA;
/*12804*/	uint32				aapoints;			//avaliable, unspent
/*12808*/	uint8				perAA;				//For Mac
/*12809*/	uint8				unknown12844[35];
/*14124*/	uint32				ATR_PET_LOH_timer;
			uint32				UnknownTimer;
			uint32				HarmTouchTimer;
/*14128*/	uint8				unknown14160[4494];
/*18630*/	SuspendedMinion_Struct	SuspendedMinion; // No longer in use
/*19240*/	uint32				timeentitledonaccount;
/*19532*/	uint8				unknown19568[8];
/*19556*/	uint8				groupAutoconsent;	// 0=off, 1=on
/*19557*/	uint8				raidAutoconsent;	// 0=off, 1=on
/*19558*/	uint8				guildAutoconsent;	// 0=off, 1=on
/*19559*/	uint8				unknown19595[5];	// ***Placeholder (6/29/2005)
/*19564*/	uint32				RestTimer;
/*19568*/
};




/*
** Client Target Struct
** Length: 2 Bytes
** OpCode: 6221
*/
struct ClientTarget_Struct {
/*000*/	uint32	new_target;			// Target ID
};

struct PetCommand_Struct {
/*000*/ uint32	command;
/*004*/ uint32	unknown;
};

/*
** Delete Spawn
** Length: 4 Bytes
** OpCode: OP_DeleteSpawn
*/
struct DeleteSpawn_Struct
{
/*00*/ uint32 spawn_id;		// Spawn ID to delete
/*04*/ uint8 Decay;			// 0 = vanish immediately, 1 = 'Decay' sparklies for corpses.
};

/*
** Channel Message received or sent
** Length: 144 Bytes + Variable Length + 1
** OpCode: OP_ChannelMessage
**
*/
struct ChannelMessage_Struct
{
	/*000*/	char	targetname[64];		// Tell recipient
	/*064*/	char	sender[64];			// The senders name (len might be wrong)
	/*128*/	uint16	language;			// Language
	/*130*/	uint16	chan_num;			// Channel
	/*132*/	uint16	cm_unknown4;		// ***Placeholder
	/*134*/	uint16	skill_in_language;	// The players skill in this language? might be wrong
	/*136*/	char	message[0];			// Variable length message
};

/*
** Special Message
** Length: 4 Bytes + Variable Text Length + 1
** OpCode: OP_SpecialMesg
**
*/
/*
	Theres something wrong with this... example live packet:
Server->Client: [ Opcode: OP_SpecialMesg (0x0fab) Size: 244 ]
   0: 01 02 00 0A 00 00 00 09 - 05 00 00 42 61 72 73 74  | ...........Barst
  16: 72 65 20 53 6F 6E 67 77 - 65 61 76 65 72 00 7C F9  | re Songweaver.|.
  32: FF FF 84 FF FF FF 03 00 - 00 00 47 72 65 65 74 69  | ..........Greeti

*/
struct SpecialMesg_Struct
{
/*00*/	char	header[3];				// 04 04 00 <-- for #emote style msg
/*03*/	uint32	msg_type;				// Color of text (see MT_*** below)
/*07*/	uint32	target_spawn_id;		// Who is it being said to?
/*11*/	char	sayer[1];				// Who is the source of the info
/*12*/	uint8	unknown12[12];
/*24*/	char	message[1];				// What is being said?
};

struct OldSpecialMesg_Struct
{
/*0000*/ uint32 msg_type;		// Comment: Type of message
/*0004*/ char  message[0];		// Comment: Message, followed by four bytes?
};

/*
** When somebody changes what they're wearing or give a pet a weapon (model changes)
** Length: 19 Bytes
*/
struct WearChange_Struct{
/*000*/ uint16 spawn_id;
/*002*/ uint32 material;
/*006*/ uint32 unknown06;
/*010*/ uint32 elite_material;	// 
/*014*/ uint32 hero_forge_model; // New to VoA
/*018*/ uint32 unknown18; // New to RoF
/*022*/ Color_Struct color;
/*026*/ uint8 wear_slot_id;
/*027*/
};

struct BindWound_Struct {
/*000*/    uint16  to; // entity id
/*002*/    uint8   type; // 0 or 1 complete, 2 Unknown, 3 ACK, 4 Died, 5 Left, 6 they moved, 7 you moved
/*003*/    uint8   void_;
};

/*
** Type: Zone Change Request (before hand)
** Length: 88 bytes
** OpCode: a320
*/

struct ZoneChange_Struct {
/*000*/	char	char_name[64];	// Character Name
/*064*/	uint16	zoneID;
/*066*/	uint16	instanceID;
/*068*/	float	y;
/*072*/	float	x;
/*076*/	float	z;
/*080*/	uint32	zone_reason;	//0x0A == death, I think
/*084*/	int32	success;		// =0 client->server, =1 server->client, -X=specific error
/*088*/
};

// Whatever you send to the client in RequestClientZoneChange_Struct.type, the client will send back
// to the server in ZoneChange_Struct.zone_reason. My guess is this is a memo field of sorts.
// WildcardX 27 January 2008

struct RequestClientZoneChange_Struct {
/*00*/	uint16	zone_id;
/*02*/	uint16	instance_id;
/*04*/	float	y;
/*08*/	float	x;
/*12*/	float	z;
/*16*/	float	heading;
/*20*/	uint32	type;	//unknown... values
};

struct Animation_Struct {
/*00*/	uint16 spawnid;
/*02*/	uint16 target;
/*04*/	uint8  action;
/*05*/  uint8  value;
/*06*/	uint32 unknown06;
/*10*/	uint16 unknown10; // 80 3F
};

// solar: this is what causes the caster to animate and the target to
// get the particle effects around them when a spell is cast
// also causes a buff icon
struct Action_Struct
{
 /* 00 */	uint16 target;	// id of target
 /* 02 */	uint16 source;	// id of caster
 /* 04 */	uint16 level; // level of caster
 /* 06 */	uint16 instrument_mod;
 /* 08 */	uint32 bard_focus_id;
 /* 12 */	uint16 unknown16;
// some kind of sequence that's the same in both actions
// as well as the combat damage, to tie em together?
 /* 14 */	uint32 sequence;
 /* 18 */	uint32 unknown18;
 /* 22 */	uint8 type;		// 231 (0xE7) for spells
 /* 23 */	uint32 unknown23;
 /* 27 */	uint16 spell;	// spell id being cast
 /* 29 */	uint8 unknown29;
// this field seems to be some sort of success flag, if it's 4
 /* 30 */	uint8 buff_unknown;	// if this is 4, a buff icon is made
 /* 31 */
};

// solar: this is what prints the You have been struck. and the regular
// melee messages like You try to pierce, etc. It's basically the melee
// and spell damage message
struct CombatDamage_Struct
{
/* 00 */	uint16	target;
/* 02 */	uint16	source;
/* 04 */	uint8	type; //slashing, etc. 231 (0xE7) for spells
/* 05 */	uint16	spellid;
/* 07 */	uint32	damage;
/* 11 */	uint32 unknown11;
/* 15 */	uint32 sequence;	// see above notes in Action_Struct
/* 19 */	uint32	unknown19;
/* 23 */
};

/*
** Consider Struct
*/
struct Consider_Struct{
/*000*/ uint32	playerid;		// PlayerID
/*004*/ uint32	targetid;		// TargetID
/*008*/ uint32	faction;		// Faction
/*012*/ uint32	level;			// Level
/*016*/ int32	cur_hp;			// Current Hitpoints
/*020*/ int32	max_hp;			// Maximum Hitpoints
/*024*/ uint8	pvpcon;			// Pvp con flag 0/1
/*025*/ uint8	unknown3[3];
};

/*
** Spawn Death Blow
** Length: 32 Bytes
** OpCode: 0114
*/
struct Death_Struct
{
/*000*/	uint32	spawn_id;
/*004*/	uint32	killer_id;
/*008*/	uint32	corpseid;	// was corpseid
/*012*/	uint32	bindzoneid;
/*016*/	uint32	spell_id;
/*020*/	uint32	attack_skill;
/*024*/	uint32	damage;
/*028*/	uint32	unknown028;
};

  struct OldDeath_Struct
{
/*000*/	uint16	spawn_id;		// Comment: 
/*002*/	uint16	killer_id;		// Comment: 
/*004*/	uint16	corpseid;		// Comment: corpseid used for looting PC corpses ! (Tazadar)
/*006*/	uint8	spawn_level;		// Comment: 
/*007*/ uint8   unknown007;
/*008*/	uint16	spell_id;	// Comment: Attack skill (Confirmed by Tazadar)
/*010*/	uint8	attack_skill;		// Comment: 
/*011*/ uint8   unknonw011;
/*012*/	uint32	damage;			// Comment: Damage taken, (Confirmed by Tazadar)
/*014*/ uint8   is_PC;		// Comment: 
/*015*/ uint8   unknown015[3];
};

/*
**  Emu Spawn position update
**	Struct sent from server->client to update position of
**	another spawn's position update in zone (whether NPC or PC)
**   
**  This is modified to support multiple client versions in encodes.
**
*/

  struct SpawnPositionUpdate_Struct
  {
	  /*0000*/ uint16	spawn_id;               // Id of spawn to update
	  /*0002*/ uint8	anim_type; // ??
	  /*0003*/ uint8	heading;                // Heading
	  /*0004*/ int8		delta_heading;          // Heading Change
	  /*0005*/ int16	y_pos;                  // New X position of spawn
	  /*0007*/ int16	x_pos;                  // New Y position of spawn
	  /*0009*/ int16	z_pos;                  // New Z position of spawn
	  /*0011*/ uint32	delta_y : 10,             // Y Velocity
						spacer1 : 1,              // ***Placeholder
						delta_z : 10,             // Z Velocity
						spacer2 : 1,              // ***Placeholder
						delta_x : 10;             // Z Velocity
	  /*015*/
  };

  struct SpawnPositionUpdates_Struct
{
	/*0000*/ uint32  num_updates;               // Number of SpawnUpdates
	/*0004*/ struct SpawnPositionUpdate_Struct // Spawn Position Update
						spawn_update;
};

struct PlayerPositionUpdates_Struct
{
	/*0000*/ uint32  num_updates;               // Number of SpawnUpdates
	/*0004*/ struct SpawnPositionUpdate_Struct // Spawn Position Update
						spawn_update[0];
};

/*
** Spawn HP Update
** Length: 10 Bytes
** OpCode: OP_HPUpdate
*/
struct SpawnHPUpdate_Struct
{
/*00*/ uint32	cur_hp;		// Id of spawn to update
/*04*/ int32	max_hp;		// Maximum hp of spawn
/*08*/ int16	spawn_id;	// Current hp of spawn
/*10*/
};

struct ManaUpdate_Struct
{
/*00*/ uint32	cur_mana;
/*04*/ uint32	max_mana;
/*08*/ uint16	spawn_id;
/*10*/
};

struct SpawnHPUpdate_Struct2
{
/*00*/ int16	spawn_id;
/*02*/ uint8		hp;			//HP Percentage
/*03*/
};

// Is this even used?
struct MobHealth
{
	/*0000*/	uint8	hp;	//health percent
	/*0001*/	uint16	id;	//mobs id
};

/*
** Stamina
** Length: 8 Bytes
** OpCode: 5721
*/
struct Stamina_Struct {
/*00*/ uint16 food;		// (low more hungry 127-0)
/*02*/ uint16 water;	// (low more thirsty 127-0)
/*04*/ uint16 fatigue;  // (low more 0-100)
};

/*
** Level Update
** Length: 12 Bytes
*/
struct LevelUpdate_Struct
{
/*00*/ uint32 level;		// New level
/*04*/ uint32 level_old;	// Old level
/*08*/ uint32 exp;			// Current Experience
};

/*
** Experience Update
** Length: 14 Bytes
** OpCode: 9921
*/
struct ExpUpdate_Struct
{
/*0000*/ uint32 exp;	// Current experience ratio from 0 to 330
/*0004*/ uint32 aaxp;	// @BP ??
};

/*
** Item Packet Struct - Works on a variety of opcodes
** Packet Types: See ItemPacketType enum
**
*/
enum ItemPacketType
{
	ItemPacketViewLink			= 0x00,
	ItemPacketTradeView			= 0x65,
	ItemPacketLoot				= 0x66,
	ItemPacketTrade				= 0x67,
	ItemPacketCharInventory		= 0x69,
	ItemPacketSummonItem		= 0x6A,
	ItemPacketTributeItem		= 0x6C,
	ItemPacketMerchant			= 0x64,
	ItemPacketWorldContainer	= 0x6B,
	ItemPacketCharmUpdate		= 0x6E
};
struct ItemPacket_Struct
{
/*00*/	ItemPacketType	PacketType;
/*04*/	uint16			fromid;
/*06*/	char			SerializedItem[1];
/*xx*/
};

struct BulkItemPacket_Struct
{
/*00*/	char			SerializedItem[0];
/*xx*/
};

struct Consume_Struct
{
/*0000*/ uint32	slot;
/*0004*/ uint32	auto_consumed; // 0xffffffff when auto eating e7030000 when right click
/*0008*/ uint8	c_unknown1[4];
/*0012*/ uint8	type; // 0x01=Food 0x02=Water
/*0013*/ uint8	unknown13[3];
};

struct MoveItem_Struct
{
/*0000*/ uint32 from_slot;
/*0004*/ uint32 to_slot;
/*0008*/ uint32 number_in_stack;
/*0012*/
};

// both MoveItem_Struct/DeleteItem_Struct server structures will be changing to a structure-based slot format..this will
// be used for handling SoF/SoD/etc... time stamps sent using the MoveItem_Struct format. (nothing will be done with this
// info at the moment..but, it is forwarded on to the server for handling/future use)
struct ClientTimeStamp_Struct
{
/*0000*/ uint32	from_slot;
/*0004*/ uint32	to_slot;
/*0008*/ uint32	number_in_stack;
/*0012*/
};

//
// from_slot/to_slot
// -1 - destroy
// 0 - cursor
// 1 - inventory
// 2 - bank
// 3 - trade
// 4 - shared bank
//
// cointype
// 0 - copeer
// 1 - silver
// 2 - gold
// 3 - platinum
//
static const uint32 COINTYPE_PP = 3;
static const uint32 COINTYPE_GP = 2;
static const uint32 COINTYPE_SP = 1;
static const uint32 COINTYPE_CP = 0;

struct MoveCoin_Struct
{
	int32 from_slot;
	int32 to_slot;
	uint32 cointype1;
	uint32 cointype2;
	int32	amount;
};
struct TradeCoin_Struct{
	uint32	trader;
	uint8	slot;
	uint16	unknown5;
	uint8	unknown7;
	uint32	amount;
};
struct TradeMoneyUpdate_Struct{
	uint16	trader;
	uint16	type;
	int32	amount;
};
/*
** Surname struct
** Size: 100 bytes
*/
struct Surname_Struct
{
/*0000*/	char name[64];
/*0064*/	uint32 unknown0064;
/*0068*/	char lastname[32];
/*0100*/
};

struct GuildsListEntry_Struct {
	char name[64];
};

struct GuildsList_Struct {
	uint8 head[64]; // First on guild list seems to be empty...
	GuildsListEntry_Struct Guilds[512];
};

struct OldGuildsListEntry_Struct 
{
/*0000*/	uint32 guildID;				// Comment: empty = 0xFFFFFFFF
/*0004*/	char name[64];				// Comment: 
/*0068*/	uint32 unknown1;			// Comment: = 0xFF
/*0072*/	uint16 exists;				// Comment: = 1 if exists, 0 on empty
/*0074*/	uint8 unknown2[6];			// Comment: = 0x00
/*0080*/	uint32 unknown3;			// Comment: = 0xFF
/*0084*/	uint8 unknown4[8];			// Comment: = 0x00
/*0092*/	uint32 unknown5;
/*0096*/
};

struct OldGuildsList_Struct 
{
	uint8 head[4];							// Comment: 
	OldGuildsListEntry_Struct Guilds[512];		// Comment: 
};

struct OldGuildUpdate_Struct {
	uint32	guildID;
	OldGuildsListEntry_Struct entry;
};

struct GuildUpdate_Struct {
	uint32	guildID;
	GuildsListEntry_Struct entry;
};

/*
** Money Loot
** Length: 22 Bytes
** OpCode: 5020
*/
struct moneyOnCorpseStruct {
/*0000*/ uint8	response;		// 0 = someone else is, 1 = OK, 2 = not at this time
/*0001*/ uint8	unknown1;		// = 0x5a
/*0002*/ uint8	unknown2;		// = 0x40
/*0003*/ uint8	unknown3;		// = 0
/*0004*/ uint32	platinum;		// Platinum Pieces
/*0008*/ uint32	gold;			// Gold Pieces

/*0012*/ uint32	silver;			// Silver Pieces
/*0016*/ uint32	copper;			// Copper Pieces
};

//opcode = 0x5220
// size 292


struct LootingItem_Struct {
/*000*/	uint32	lootee;
/*002*/	uint32	looter;
/*004*/	uint16	slot_id;
/*006*/	uint8	unknown3[2];
/*008*/	uint32	auto_loot;
};

struct GuildInviteAccept_Struct {
	char inviter[64];
	char newmember[64];
	uint32 response;
	uint32 guildeqid;
};
struct GuildRemove_Struct
{
	/*000*/	char Remover[64];
	/*064*/	char Removee[64];
	/*128*/	uint16 guildeqid;
	/*130*/	uint8 unknown[2];
	/*132*/	uint32 rank;
	/*136*/
};

struct GuildCommand_Struct {
	char othername[64];
	char myname[64];
	uint16 guildeqid;
	uint8 unknown[2]; // for guildinvite all 0's, for remove 0=0x56, 2=0x02
	uint32 officer;
};

// Server -> Client
// Update a guild members rank and banker status
struct GuildSetRank_Struct
{
/*00*/	uint32	Unknown00;
/*04*/	uint32	Unknown04;
/*08*/	uint32	Rank;
/*12*/	char	MemberName[64];
/*76*/	uint32	Banker;
/*80*/
};

// Opcode OP_GMZoneRequest
// Size = 88 bytes
struct GMZoneRequest_Struct {
/*0000*/	char	charname[64];
/*0064*/	uint32	zone_id;
/*0068*/	float	x;
/*0072*/	float	y;
/*0076*/	float	z;
/*0080*/	char	unknown0080[4];
/*0084*/	uint32	success;		// 0 if command failed, 1 if succeeded?
/*0088*/
//	/*072*/	int8	success;		// =0 client->server, =1 server->client, -X=specific error
//	/*073*/	uint8	unknown0073[3]; // =0 ok, =ffffff error
};

struct GMSummon_Struct {
/*  0*/	char	charname[64];
/* 30*/	char	gmname[64];
/* 60*/	uint32	success;
/* 61*/	uint32	zoneID;
/*92*/	float	y;
/*96*/	float	x;
/*100*/	float	z;
/*104*/	uint32	unknown2; // E0 E0 56 00
};

struct GMGoto_Struct { // x,y is swapped as compared to summon and makes sense as own packet
/*  0*/ char	charname[64];

/* 64*/ char	gmname[64];
/* 128*/uint32	success;
/* 132*/ uint32	zoneID;

/*136*/ int32	y;
/*140*/ int32	x;
/*144*/ int32	z;
/*148*/ uint32	unknown2; // E0 E0 56 00
};

struct GMLastName_Struct {
	char name[64];
	char gmname[64];
	char lastname[64];
	uint16 unknown[4];	// 0x00, 0x00
						// 0x01, 0x00 = Update the clients
};

//Combat Abilities
struct CombatAbility_Struct {
	uint32 m_target;		//the ID of the target mob
	uint32 m_atk;
	uint32 m_skill;
};

//Instill Doubt
struct Instill_Doubt_Struct {
	uint8 i_id;
	uint8 ia_unknown;
	uint8 ib_unknown;
	uint8 ic_unknown;
	uint8 i_atk;

	uint8 id_unknown;
	uint8 ie_unknown;
	uint8 if_unknown;
	uint8 i_type;
	uint8 ig_unknown;
	uint8 ih_unknown;
	uint8 ii_unknown;
};

struct GiveItem_Struct {
	uint16 to_entity;
	int16 to_equipSlot;
	uint16 from_entity;
	int16 from_equipSlot;
};

struct RandomReq_Struct {
	uint32 low;
	uint32 high;
};

/* solar: 9/23/03 reply to /random command; struct from Zaphod */
struct RandomReply_Struct {
/* 00 */	uint32 low;
/* 04 */	uint32 high;
/* 08 */	uint32 result;
/* 12 */	char name[64];
/* 76 */
};

// EverQuest Time Information:
// 72 minutes per EQ Day
// 3 minutes per EQ Hour
// 6 seconds per EQ Tick (2 minutes EQ Time)
// 3 seconds per EQ Minute

struct TimeOfDay_Struct {
	uint8	hour;
	uint8	minute;
	uint8	day;
	uint8	month;
	uint32	year;
};

// Darvik: shopkeeper structs
struct Merchant_Click_Struct {
/*000*/ uint32	npcid;			// Merchant NPC's entity id
/*004*/ uint32	playerid;
/*008*/ uint32	command;		//1=open, 0=cancel/close
/*012*/ float	rate;			//cost multiplier, dosent work anymore
};
/*
Unknowns:
0 is e7 from 01 to // MAYBE SLOT IN PURCHASE
1 is 03
2 is 00
3 is 00
4 is ??
5 is ??
6 is 00 from a0 to
7 is 00 from 3f to */
/*
0 is F6 to 01
1 is CE CE
4A 4A
00 00
00 E0
00 CB
00 90
00 3F
*/



struct Merchant_Sell_Struct {
/*000*/	uint32	npcid;			// Merchant NPC's entity id
/*004*/	uint32	playerid;		// Player's entity id
/*008*/	uint32	itemslot;
		uint32	unknown12;
/*016*/	uint8	quantity;		// Already sold
/*017*/ uint8	Unknown016[3];
/*020*/ uint32	price;
};
struct Merchant_Purchase_Struct {
/*000*/	uint32	npcid;			// Merchant NPC's entity id
/*004*/	uint32	itemslot;		// Player's entity id
/*008*/	uint32	quantity;
/*012*/	uint32	price;
};
struct OldMerchant_Purchase_Struct {
/*000*/	uint16	npcid;			// Merchant NPC's entity id
/*002*/ uint16  playerid;
/*004*/	uint16	itemslot;		// Player's entity id
/*006*/ uint16  price;
/*008*/	uint8	quantity;
/*009*/ uint8   unknown_void[7];
};
struct Merchant_DelItem_Struct{
/*000*/	uint32	npcid;			// Merchant NPC's entity id
/*004*/	uint32	playerid;		// Player's entity id
/*008*/	uint32	itemslot;
/*012*/	uint32	type;
};

/*struct Item_Shop_Struct {
	uint16 merchantid;
	uint8 itemtype;
	Item_Struct item;
	uint8 iss_unknown001[6];
};*/

struct Illusion_Struct { //size: 256 - SoF
/*000*/	uint32	spawnid;
/*004*/	char charname[64];		//
/*068*/	uint16	race;			//
/*070*/	char	unknown006[2];
/*072*/	uint8	gender;
/*073*/	uint8	texture;
/*074*/	uint8	unknown008;		//
/*075*/	uint8	unknown009;		//
/*076*/	uint8	helmtexture;	//
/*077*/	uint8	unknown010;		//
/*078*/	uint8	unknown011;		//
/*079*/	uint8	unknown012;		//
/*080*/	uint32	face;			//
/*084*/	uint8	hairstyle;		//
/*085*/	uint8	haircolor;		//
/*086*/	uint8	beard;			//
/*087*/	uint8	beardcolor;		//
/*088*/	float	size;			//
/*104*/	uint32	armor_tint[_MaterialCount];	//
/*140*/	uint8	eyecolor1;		// Field Not Identified in any Illusion Struct
/*141*/	uint8	eyecolor2;		// Field Not Identified in any Illusion Struct
/*142*/	uint8	unknown138[114];	//
/*256*/
};

struct Illusion_Struct_Old {
/*000*/	uint32	spawnid;
		char charname[64];
/**/	uint16	race;
/**/	char	unknown006[2];
/**/	uint8	gender;
/**/	uint8	texture;
/**/	uint8	helmtexture;
/**/	uint8	unknown011;
/**/	uint32	face;
/**/	char	unknown020[88];
/**/
};

// OP_Sound - Size: 68
struct QuestReward_Struct
{
/*000*/ uint32	mob_id;	// ID of mob awarding the client
/*004*/ uint32	target_id;
/*008*/ uint32	exp_reward;
/*012*/ uint32	faction;
/*016*/ int32	faction_mod;
/*020*/ uint32	copper;		// Gives copper to the client
/*024*/ uint32	silver;		// Gives silver to the client
/*028*/ uint32	gold;		// Gives gold to the client
/*032*/ uint32	platinum;	// Gives platinum to the client
/*036*/ uint32	item_id;
/*040*/ uint32	unknown040;
/*044*/ uint32	unknown044;
/*048*/ uint32	unknown048;
/*052*/ uint32	unknown052;
/*056*/ uint32	unknown056;
/*060*/ uint32	unknown060;
/*064*/ uint32	unknown064;
/*068*/
};

// Size: 8
struct Camera_Struct
{
	uint32	duration;	// Duration in ms
	uint32	intensity;	// Between 1023410176 and 1090519040
};

struct ZonePoint_Entry {
/*0000*/	uint32	iterator;
/*0004*/	float	y;
/*0008*/	float	x;
/*0012*/	float	z;
/*0016*/	float	heading;
/*0020*/	uint16	zoneid;
/*0022*/	uint16	zoneinstance; // LDoN instance
};

struct ZonePoints {
/*0000*/	uint32	count;
/*0004*/	struct	ZonePoint_Entry zpe[0]; // Always add one extra to the end after all zonepoints
};

struct SkillUpdate_Struct {
/*00*/	uint32 skillId;
/*04*/	uint32 value;
/*08*/
};

struct ZoneUnavail_Struct {
	//This actually varies, but...
	char zonename[16];
	int16 unknown[4];
};

enum {	//Group action fields
	groupActJoin = 0,
	groupActLeave = 1,
	groupActDisband = 6,
	groupActUpdate = 7,
	groupActMakeLeader = 8,
	groupActInviteInitial = 9,
	groupActAAUpdate = 10
};

struct GroupGeneric_Struct {
	char name1[64];
	char name2[64];
};

struct GroupCancel_Struct {
	char	name1[64];
	char	name2[64];
	uint8	toggle;
};

//Group structs are fragile, this works for now until they can be cleaned up.

struct GroupUpdate_Struct {
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[5][64];
/*0388*/	char	leadersname[64];
};

struct GroupUpdate2_Struct {
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[5][64];
/*0388*/	char	leadersname[64];
/*0452*/	GroupLeadershipAA_Struct leader_aas;
/*0580*/	uint8	unknown580[196];
/*0766*/	uint32	NPCMarkerID;	// EntityID of player delegated MarkNPC ability
/*0780*/	uint8	unknown780[56];
/*0836*/
};

struct GroupJoin_Struct {
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[64];
/*0132*/	GroupLeadershipAA_Struct leader_aas;
/*0196*/	uint8	unknown196[196];
/*0392*/	uint32	NPCMarkerID;	// EntityID of player delegated MarkNPC ability
/*0396*/	uint8	unknown396[56];
/*0452*/
};

struct GroupFollow_Struct { // SoF Follow Struct
/*0000*/	char	name1[64];	// inviter
/*0064*/	char	name2[64];	// invitee
/*0128*/	uint32	unknown0128;
/*0132*/
};

struct GroupLeader_Struct
{
/*0000*/	uint32	action;
/*0004*/	char	yourname[64];
/*0068*/	char	membername[64];
/*0132*/	uint32	unknown132;
};

struct FaceChange_Struct {
/*000*/	uint8	haircolor;
/*001*/	uint8	beardcolor;
/*002*/	uint8	eyecolor1; // the eyecolors always seem to be the same, maybe left and right eye?
/*003*/	uint8	eyecolor2;
/*004*/	uint8	hairstyle;
/*005*/	uint8	beard;
/*006*/	uint8	face;
//there are only 10 faces for barbs changing woad just
//increase the face value by ten so if there were 8 woad
//designs then there would be 80 barb faces
};

/*
** Trade request from one client to another
** Used to initiate a trade
** Size: 8 bytes
** Used in: OP_TradeRequest
*/
struct TradeRequest_Struct {
/*00*/	uint32 to_mob_id;
/*04*/	uint32 from_mob_id;
/*08*/
};

struct TradeAccept_Struct {
/*00*/	uint32 from_mob_id;
/*04*/	uint32 unknown4;		//seems to be garbage
/*08*/
};

/*
** Cancel Trade struct
** Sent when a player cancels a trade
** Size: 8 bytes
** Used In: OP_CancelTrade
**
*/
struct CancelTrade_Struct {
/*00*/	uint32 fromid;
/*04*/	uint32 action;
/*08*/
};

struct PetitionUpdate_Struct {
	uint32 petnumber;	// Petition Number
	uint32 color;		// 0x00 = green, 0x01 = yellow, 0x02 = red
	uint32 status;
	time_t senttime;	// 4 has to be 0x1F
	char accountid[32];
	char gmsenttoo[64];
	int32 quetotal;
	char charname[64];
};

struct Petition_Struct {
	uint32 petnumber;
	uint32 urgency;
	char accountid[32];
	char lastgm[32];
	uint32	zone;
	//char zone[32];
	char charname[64];
	uint32 charlevel;
	uint32 charclass;
	uint32 charrace;
	uint32 unknown;
	//time_t senttime; // Time?
	uint32 checkouts;
	uint32 unavail;
	//uint8 unknown5[4];
	time_t senttime;
	uint32 unknown2;
	char petitiontext[1024];
	char gmtext[1024];
};

struct Who_All_Struct 
{ 
	/*000*/	char	whom[64];
	/*064*/	int16	wrace;		// FF FF = no race
	/*066*/	int16	wclass;		// FF FF = no class
	/*068*/	int16	lvllow;		// FF FF = no numbers
	/*070*/	int16	lvlhigh;	// FF FF = no numbers
	/*072*/	int16	gmlookup;	// FF FF = not doing /who all gm
	/*074*/	int16	guildid;
	/*076*/	int8	unknown076[64];
	/*140*/
};

struct Stun_Struct { // 4 bytes total
	uint32 duration; // Duration of stun
};

// OP_Emote
struct Emote_Struct {
/*0000*/	uint32 unknown01;
/*0004*/	char message[1024];
/*1028*/
};

// OP_Emote
struct OldEmote_Struct {
/*0000*/	uint16 unknown01;
/*0002*/	char message[1024];
/*1026*/
};

// Inspect
struct Inspect_Struct {
	uint16 TargetID;
	uint16 PlayerID;
};

//OP_InspectAnswer - Size: 1860
struct InspectResponse_Struct {
/*000*/	uint32 TargetID;
/*004*/	uint32 playerid;
/*008*/	char itemnames[23][64];
/*1480*/uint32 itemicons[23];
/*1572*/char text[288];	// Max number of chars in Inspect Window appears to be 254 // Msg struct property is 256 (254 + '\0' is my guess) -U
/*1860*/
};

struct OldInspectResponse_Struct 
{ 
	int16 TargetID;			// Comment: ? 
	int16 PlayerID;			// Comment: ?
	int8  unknown[1740];	// Comment: ?
}; 

//OP_SetDataRate
struct SetDataRate_Struct {
	float newdatarate;
};

//OP_SetServerFilter
struct SetServerFilter_Struct 
{
	/*000*/	uint32 filters[17];	// Comment: 
	/*068*/	
};

//Op_SetServerFilterAck
struct SetServerFilterAck_Struct {
	uint8 blank[8];
};

struct GMName_Struct {
	char oldname[64];
	char gmname[64];
	char newname[64];
	uint8 badname;
	uint8 unknown[3];
};

struct GMDelCorpse_Struct {
	char corpsename[64];
	char gmname[64];
	uint8 unknown;
};

struct GMKick_Struct {
	char name[64];
	char gmname[64];
	uint8 unknown;
};


struct GMKill_Struct {
	char name[64];
	char gmname[64];
	uint8 unknown;
};


struct GMEmoteZone_Struct {
	char text[512];
};

// This is where the Text is sent to the client.
// Use ` as a newline character in the text.
// Variable length.
struct BookText_Struct {
	uint8 window;	// where to display the text (0xFF means new window)
	uint8 type;		//type: 0=scroll, 1=book, 2=item info.. prolly others.
	uint32 invslot;	// Only used in SoF and later clients.
	char booktext[1]; // Variable Length
};
// This is the request to read a book.
// This is just a "text file" on the server
// or in our case, the 'name' column in our books table.
struct BookRequest_Struct {
	uint8 window;	// where to display the text (0xFF means new window)
	uint8 type;		//type: 0=scroll, 1=book, 2=item info.. prolly others.
	uint32 invslot;	// Only used in Sof and later clients;
	char txtfile[20];
};

/*
** Object/Ground Spawn struct
	We rely on the encode/decode to send the proper packet, this is just an internal data struct like PP.
	*/
struct Object_Struct {
/*00*/	uint32	linked_list_addr[2];// <Zaphod> They are, get this, prev and next, ala linked list
/*08*/	uint16	unknown008;			//
/*10*/	uint16	unknown010;			//
/*12*/	uint32	drop_id;			// Unique object id for zone
/*16*/	uint16	zone_id;			// Redudant, but: Zone the object appears in
/*18*/	uint16	zone_instance;		//
/*20*/	uint32	unknown020;			//
/*24*/	uint32	unknown024;			//
/*28*/	float	heading;			// heading
/*32*/	float	z;					// z coord
/*36*/	float	x;					// x coord
/*40*/	float	y;					// y coord
/*44*/	char	object_name[32];	// Name of object, usually something like IT63_ACTORDEF
/*76*/	uint32	unknown076;			//
/*80*/	uint32	object_type;		// Type of object, not directly translated to OP_OpenObject
/*84*/	uint32	unknown084;			//set to 0xFF
/*88*/	uint32	spawn_id;			// Spawn Id of client interacting with object
		uint16	itemsinbag[10];
		uint8	charges;
		uint8	maxcharges;
/*92*/
};
//<Zaphod> 01 = generic drop, 02 = armor, 19 = weapon
//[13:40] <Zaphod> and 0xff seems to be indicative of the tradeskill/openable items that end up returning the old style item type in the OP_OpenObject

/*
** Click Object Struct
** Client clicking on zone object (forge, groundspawn, etc)
** Size: 8 bytes
** Last Updated: Oct-17-2003
**
*/
struct ClickObject_Struct {
/*00*/	uint32 drop_id;
/*04*/	uint32 player_id;
/*08*/
};

struct Shielding_Struct {
	uint16 target_id;
};

/*
** Click Object Action Struct
** Response to client clicking on a World Container (ie, forge)
** also sent by the client when they close the container.
**
*/
struct ClickObjectAction_Struct {
/*00*/	uint32	player_id;	// Entity Id of player who clicked object
/*04*/	uint32	drop_id;	// Zone-specified unique object identifier
/*08*/	uint32	open;		// 1=opening, 0=closing
/*12*/	uint32	type;		// See object.h, "Object Types"
/*16*/	uint32	unknown16;	// set to 0xA
/*20*/	uint32	icon;		// Icon to display for tradeskill containers
/*24*/	uint32	unknown24;	//
/*28*/	char	object_name[64]; // Object name to display
/*92*/
};

/*
** Generic Door Struct
** Length: 52 Octets
** Used in:
** cDoorSpawnsStruct(f721)
**
*/
struct Door_Struct
{
/*0000*/ char	name[32];		// Filename of Door // Was 10char long before... added the 6 in the next unknown to it: Daeken M. BlackBlade //changed both to 32: Trevius
/*0032*/ float	yPos;			// y loc
/*0036*/ float	xPos;			// x loc
/*0040*/ float	zPos;			// z loc
/*0044*/ float	heading;
/*0048*/ uint32	incline;		// rotates the whole door
/*0052*/ uint16	size;			// 100 is normal, smaller number = smaller model
/*0054*/ uint8	unknown0038[6];
/*0060*/ uint8	doorId;			// door's id #
/*0061*/ uint8	opentype;
/*
 * Open types:
 * 66 = PORT1414 (Qeynos)
 * 55 = BBBOARD (Qeynos)
 * 100 = QEYLAMP (Qeynos)
 * 56 = CHEST1 (Qeynos)
 * 5 = DOOR1 (Qeynos)
 */
/*0062*/ uint8 state_at_spawn;
/*0063*/ uint8 invert_state;	// if this is 1, the door is normally open
/*0064*/ uint32 door_param;
/*0068*/ uint8 unknown0052[12]; // mostly 0s, the last 3 bytes are something tho
/*0080*/
};

struct OldDoor_Struct
{
/*0000*/ char    name[16];            // Filename of Door // Was 10char long before... added the 6 in the next unknown to it: Daeken M. BlackBlade
/*0016*/ float   yPos;               // y loc
/*0020*/ float   xPos;               // x loc
/*0024*/ float   zPos;               // z loc
/*0028*/ float	 heading;
/*0032*/ uint16  incline;
/*0034*/ uint16	 size;
/*0036*/ uint8	 unknown[2];
/*0038*/ uint8	 doorid;             // door's id #
/*0039*/ uint8	 opentype;
/*0040*/ uint8	 doorIsOpen;
/*0041*/ uint8	 inverted;
/*0042*/ uint16	 parameter; 
};

struct DoorSpawns_Struct {
	struct Door_Struct doors[0];
};

struct OldDoorSpawns_Struct	//SEQ
{
	uint16 count;            
	struct OldDoor_Struct doors[0];
};
/*
 OP Code: Op_ClickDoor
 Size:		16
*/
struct ClickDoor_Struct {
/*000*/	uint8	doorid;
/*001*/	uint8	unknown001;		// This may be some type of action setting
/*002*/	uint8	unknown002;		// This is sometimes set after a lever is closed
/*003*/	uint8	unknown003;		// Seen 0
/*004*/	uint8	picklockskill;
/*005*/	uint8	unknown005[3];
/*008*/ uint32	item_id;
/*012*/ uint16	player_id;
/*014*/ uint8	unknown014[2];
/*016*/
};

struct MoveDoor_Struct {
	uint8	doorid;
	uint8	action;
};


struct BecomeNPC_Struct {
	uint32 id;
	int32 maxlevel;
};

struct Underworld_Struct {
	float speed;
	float y;
	float x;
	float z;
};

struct Resurrect_Struct	{
/*000*/	uint32	unknown000;
/*004*/	uint16	zone_id;
/*006*/	uint16	instance_id;
/*008*/	float	y;
/*012*/	float	x;
/*016*/	float	z;
/*020*/	uint32	unknown020;
/*024*/	char	your_name[64];
/*088*/	uint32	unknown088;
/*092*/	char	rezzer_name[64];
/*156*/	uint32	spellid;
/*160*/	char	corpse_name[64];
/*224*/	uint32	action;
/* 228 */
};

struct Translocate_Struct {
/*000*/	uint32	ZoneID;
/*004*/	uint32	SpellID;
/*008*/	uint32	unknown008; //Heading ?
/*012*/	char	Caster[64];
/*076*/	float	y;
/*080*/	float	x;
/*084*/	float	z;
/*088*/	uint32	Complete;
};

struct PendingTranslocate_Struct
{
	uint32 zone_id;
	uint16 instance_id;
	float heading;
	float x;
	float y;
	float z;
	uint32 spell_id;
};

struct Sacrifice_Struct {
/*000*/	uint32	CasterID;
/*004*/	uint32	TargetID;
/*008*/	uint32	Confirm;
};

struct SetRunMode_Struct {
	uint8 mode;
	uint8 unknown[3];
};

//EnvDamage is EnvDamage2 without a few bytes at the end.

struct EnvDamage2_Struct {
/*0000*/	uint32 id;
/*0004*/	uint16 unknown4;
/*0006*/	uint32 damage;
/*0010*/	uint8 unknown10[12];
/*0022*/	uint8 dmgtype; //FA = Lava; FC = Falling
/*0023*/	uint8 unknown2[4];
/*0027*/	uint16 constant; //Always FFFF
/*0029*/	uint16 unknown29;
};

//Bazaar Stuff =D
//

enum {
	BazaarTrader_StartTraderMode = 1,
	BazaarTrader_EndTraderMode = 2,
	BazaarTrader_UpdatePrice = 3,
	BazaarTrader_EndTransaction = 4,
	BazaarSearchResults = 7,
	BazaarWelcome = 9,
	BazaarBuyItem = 10,
	BazaarTrader_ShowItems = 11,
	BazaarSearchDone = 12,
	BazaarTrader_CustomerBrowsing = 13,
	BazaarInspectItem = 18,
	BazaarSearchDone2 = 19,
	BazaarTrader_StartTraderMode2 = 22
};

enum {
	BazaarPriceChange_Fail = 0,
	BazaarPriceChange_UpdatePrice = 1,
	BazaarPriceChange_RemoveItem = 2,
	BazaarPriceChange_AddItem = 3
};

struct BazaarWindowStart_Struct {
	uint8	Action;
	uint8	Unknown001;
	uint16	Unknown002;
};


struct BazaarWelcome_Struct {
	BazaarWindowStart_Struct Beginning;
	uint32	Traders;
	uint32	Items;
	uint8	Unknown012[8];
};

struct BazaarSearch_Struct {
	BazaarWindowStart_Struct Beginning;
	uint32	TraderID;
	uint32	Class_;
	uint32	Race;
	uint32	ItemStat;
	uint32	Slot;
	uint32	Type;
	char	Name[64];
	uint32	MinPrice;
	uint32	MaxPrice;
	uint32	Minlevel;
	uint32	MaxLlevel;
};
struct BazaarInspect_Struct{
	uint32 ItemID;
	uint32 Unknown004;
	char Name[64];
};

struct NewBazaarInspect_Struct {
/*000*/	BazaarWindowStart_Struct Beginning;
/*004*/	char Name[64];
/*068*/	uint32 Unknown068;
/*072*/	uint32 Unknown072;
/*076*/	uint32 Unknown076;
/*080*/	int32 SerialNumber;
/*084*/	uint32 Unknown084;
};

struct BazaarReturnDone_Struct{
	uint32 Type;
	uint32 TraderID;
	uint32 Unknown008;
	uint32 Unknown012;
	uint32 Unknown016;
};
struct BazaarSearchResults_Struct {
/*000*/	BazaarWindowStart_Struct Beginning;
/*004*/	uint32	NumItems;
/*008*/	uint32	SerialNumber;
/*012*/	uint32	SellerID;
/*016*/	uint32	Cost;
/*020*/	uint32	ItemStat;
/*024*/	char	ItemName[64];
/*088*/
	// New fields for SoD+, stripped off when encoding for older clients.
	char	SellerName[64];
	uint32	ItemID;
};

struct OldBazaarSearchResults_Struct {
/*000*/	uint16  Action;
/*002*/	uint16  NumItems;
/*004*/	uint16	ItemID;
/*006*/	uint16	SellerID;
/*008*/	uint16	Cost;
/*010*/	uint16	ItemStat;
/*012*/	char	ItemName[64];
/*076*/

};

struct BuyerWelcomeMessageUpdate_Struct {
/*000*/	uint32	Action;
/*004*/	char	WelcomeMessage[256];
};

struct BuyerItemSearch_Struct {
/*000*/	uint32	Unknown000;
/*004*/	char	SearchString[64];
};

struct	BuyerItemSearchResultEntry_Struct {
/*000*/	char	ItemName[64];
/*064*/	uint32	ItemID;
/*068*/	uint32	Unknown068;
/*072*/	uint32	Unknown072;
};

#define MAX_BUYER_ITEMSEARCH_RESULTS 200

struct	BuyerItemSearchResults_Struct {
	uint32	Action;
	uint32	ResultCount;
	BuyerItemSearchResultEntry_Struct	Results[MAX_BUYER_ITEMSEARCH_RESULTS];
};

struct ServerSideFilters_Struct {
uint8	clientattackfilters;	// 0) No, 1) All (players) but self, 2) All (players) but group
uint8	npcattackfilters;		// 0) No, 1) Ignore NPC misses (all), 2) Ignore NPC Misses + Attacks (all but self), 3) Ignores NPC Misses + Attacks (all but group)
uint8	clientcastfilters;		// 0) No, 1) Ignore PC Casts (all), 2) Ignore PC Casts (not directed towards self)
uint8	npccastfilters;			// 0) No, 1) Ignore NPC Casts (all), 2) Ignore NPC Casts (not directed towards self)
};

struct	ItemViewRequest_Struct
{
	/*000*/int16	item_id;
	/*002*/char	item_name[64];
	/*066*/
};

/*
 * Client to server packet
 */
struct PickPocket_Struct {
// Size 18
	uint32 to;
	uint32 from;
	uint16 myskill;
	uint8 type; // -1 you are being picked, 0 failed , 1 = plat, 2 = gold, 3 = silver, 4 = copper, 5 = item
	uint8 unknown1; // 0 for response, unknown for input
	uint32 coin;
	uint8 lastsix[2];
};
/*
 * Server to client packet
 */

enum {
	PickPocketFailed = 0,
	PickPocketPlatinum = 1,
	PickPocketGold = 2,
	PickPocketSilver = 3,
	PickPocketCopper = 4,
	PickPocketItem = 5
};


struct sPickPocket_Struct {
	// Size 28 = coin/fail
	uint32 to;
	uint32 from;
	uint32 myskill;
	uint32 type;
	uint32 coin;
	char itemname[64];
};

struct LogServer_Struct {
// Op_Code OP_LOGSERVER
/*000*/	uint32	unknown000;
/*004*/	uint8	enable_pvp;
/*005*/	uint8	unknown005;
/*006*/	uint8	unknown006;
/*007*/	uint8	unknown007;
/*008*/	uint8	enable_FV;
/*009*/	uint8	unknown009;
/*010*/	uint8	unknown010;
/*011*/	uint8	unknown011;
/*012*/	uint32	unknown012;	// htonl(1) on live
/*016*/	uint32	unknown016;	// htonl(1) on live
/*020*/	uint8	unknown020[12];
/*032*/	char	worldshortname[32];
/*064*/	uint8	unknown064[32];
/*096*/	char	unknown096[16];	// 'pacman' on live
/*112*/	char	unknown112[16];	// '64.37,148,36' on live
/*126*/	uint8	unknown128[48];
/*176*/	uint32	unknown176;	// htonl(0x00002695)
/*180*/	char	unknown180[80];	// 'eqdataexceptions@mail.station.sony.com' on live
/*260*/	uint8	enable_petition_wnd;
/*261*/	uint8	enablevoicemacros;
/*262*/	uint8	enablemail;
/*264*/
};

struct ApproveWorld_Struct {
// Size 544
// Op_Code OP_ApproveWorld
	uint8 unknown544[544];
};

struct ClientError_Struct
{
/*00001*/	char	type;
/*00001*/	char	unknown0001[69];
/*00069*/	char	character_name[64];
/*00134*/	char	unknown134[192];
/*00133*/	char	message[31994];
/*32136*/
};

struct Track_Struct {
	uint16 entityid;
	uint16 padding002;
	float distance;
	// Fields for SoD and later
	uint8 level;
	uint8 NPC;
	uint8 GroupMember;
	char name[64];
};

struct Tracking_Struct {
	Track_Struct Entrys[0];
};

/*
** ZoneServerInfo_Struct
** Zone server information
** Size: 130 bytes
** Used In: OP_ZoneServerInfo
**
*/
struct ZoneServerInfo_Struct
{
/*0000*/	char	ip[128];
/*0128*/	uint16	port;
};

struct WhoAllPlayer{
	uint16	formatstring;
	uint16	pidstring;
	char*	name;
	uint16	rankstring;
	char*	guild;
	uint16	unknown80[3];
	uint16	zonestring;
	uint32	zone;
	uint16	class_;
	uint16	level;
	uint16	race;
	char*	account;
	uint16	unknown100;
};

struct WhoAllReturnStruct {
/*000*/	uint32	id;
/*004*/	uint16	playerineqstring;
/*006*/	char	line[27];
/*033*/	uint8	unknown35; //0A
/*034*/	uint16	unknown36;//0s
/*036*/	uint16	playersinzonestring;
/*038*/	uint16	unknown44[5]; //0s
/*048*/	uint32	unknown52;//1
/*052*/	uint32	unknown56;//1
/*056*/	uint16	playercount;//1
struct WhoAllPlayer player[0];
};

// The following four structs are the WhoAllPlayer struct above broken down
// for use in World ClientList::SendFriendsWho to accomodate the user of variable
// length strings within the struct above.

struct	WhoAllPlayerPart1 {
	uint16	FormatMSGID;
	uint16	PIDMSGID;
	char	Name[1];;
};

struct	WhoAllPlayerPart2 {
	uint16	RankMSGID;
	char	Guild[1];
};

struct	WhoAllPlayerPart3 {
	uint16	Unknown80[3];
	uint16	ZoneMSGID;
	uint32	Zone;
	uint16	Class_;
	uint16	Level;
	uint16	Race;
	char	Account[1];
};

struct	WhoAllPlayerPart4 {
	uint16	Unknown100;
};

struct Trader_Struct {
/*000*/	uint32	Code;
/*004*/	uint32	Unknown004;
/*008*/	uint64	Items[80];
/*648*/	uint32	ItemCost[80];
};

struct ClickTrader_Struct {
/*000*/	uint32	Code;
/*004*/	uint32	Unknown004;
/*008*/	int64	SerialNumber[80];
/*648*/	uint32	ItemCost[80];
};

struct GetItems_Struct{
	uint32	Items[80];
	int32	SerialNumber[80];
	int32	Charges[80];
};

struct BecomeTrader_Struct
{
/*000*/	uint32 ID;
/*004*/	uint32 Code;
/*008*/	char Name[64];
/*072*/	uint32 Unknown072;	// Observed 0x33,0x91 etc on zone-in, 0x00 when sent for a new trader after zone-in
/*076*/
};

struct TraderStatus_Struct{
	uint32 Code;
	uint32 Uknown04;
	uint32 TraderID;
};

struct Trader_ShowItems_Struct{
/*000*/	uint32 Code;
/*004*/	uint32 TraderID;
/*012*/ uint32 SubAction;
/*012*/	uint32 Unknown08[2];
};

struct TraderBuy_Struct{
/*000*/	uint32 Action;
/*004*/	uint32 TraderID;
/*008*/	uint32 ItemID;
/*012*/	uint32 AlreadySold;
/*016*/	uint32 Price;
/*020*/	uint32 Quantity;
/*024*/	char ItemName[64];
};

struct TraderItemUpdate_Struct{
	uint32	Unknown000;
	uint32	TraderID;
	uint8	FromSlot;
	int		ToSlot; //7?
	uint16	Charges;
};

struct TraderPriceUpdate_Struct {
/*000*/	uint32	Action;
/*004*/	uint32	SubAction;
/*008*/	int32	SerialNumber;
/*012*/	uint32	Unknown012;
/*016*/	uint32	NewPrice;
/*020*/	uint32	Unknown016;
};

struct MoneyUpdate_Struct{
	int32 platinum;
	int32 gold;
	int32 silver;
	int32 copper;
};

struct TraderDelItem_Struct{
	uint32 Unknown000;
	uint32 TraderID;
	uint32 ItemID;
	uint32 Unknown012;
};

struct TraderClick_Struct{
/*000*/	uint32 TraderID;
/*004*/	uint32 Unknown004;
/*008*/	uint32 Unknown008;
/*012*/	uint32 Approval;
};

struct FormattedMessage_Struct{
	uint32	unknown0;
	uint32	string_id;
	uint32	type;
	char	message[0];
};

struct OldFormattedMessage_Struct{
	uint16	unknown0;
	uint16	string_id;
	uint16	type;
	char	message[0];
};
struct SimpleMessage_Struct{
	uint32	string_id;
	uint32	color;
	uint32	unknown8;
};

struct Internal_GuildMemberEntry_Struct {
//	char	name[64];					//variable length
	uint32	level;						//network byte order
	uint32	banker;						//1=yes, 0=no, network byte order
	uint32	class_;						//network byte order
	uint32	rank;						//network byte order
	uint32	time_last_on;				//network byte order
//	char	public_note[1];				//variable length.
	uint16	zoneinstance;				//network byte order
	uint16	zone_id;					//network byte order
};

struct Internal_GuildMembers_Struct {	//just for display purposes, this is not actually used in the message encoding.
	char	player_name[64];		//variable length.
	uint32	count;				//network byte order
	uint32	name_length;	//total length of all of the char names, excluding terminators
	uint32	note_length;	//total length of all the public notes, excluding terminators
	Internal_GuildMemberEntry_Struct member[0];
	/*
	* followed by a set of `count` null terminated name strings
	* and then a set of `count` null terminated public note strings
	*/
};

struct GuildMOTD_Struct{
/*0000*/	uint32	unknown0;
/*0004*/	char	name[64];
/*0068*/	char	setby_name[64];
/*0132*/	uint32	unknown132;
/*0136*/	char	motd[512];
};

struct GuildUpdate_PublicNote{
	uint32	unknown0;
	char	name[64];
	char	target[64];
	char	note[1]; //variable length.
};

struct GuildMakeLeader{
	char	name[64];
	char	target[64];
};

struct BugStruct
{
	/*0000*/	char	chartype[64];
	/*0064*/	char	name[96];
	/*0160*/	float	x;
	/*0164*/	float	y;
	/*0168*/	float	z;
	/*0172*/	float	heading;
	/*0176*/	char	unknown176[16];
	/*0192*/	char	target_name[64];
	/*0256*/	uint32	type;
	/*0260*/	char	unknown256[2052];
	/*2312*/	char	bug[1024];
	/*3336*/	uint32	unknown3336;
	/*3340*/
};

struct IntelBugStruct
{
	/*0000*/  char name[96];
	/*0096*/  uint8 unknown096[9];
	/*0105*/  uint8 duplicate;
	/*0106*/  uint8 crash;
	/*0107*/  char bug[1024];
	/*1131*/
};

struct Make_Pet_Struct { //Simple struct for getting pet info
	uint8 level;
	uint8 class_;
	uint16 race;
	uint8 texture;
	uint8 pettype;
	float size;
	uint8 type;
	uint32 min_dmg;
	uint32 max_dmg;
};
struct Ground_Spawn{
	float max_x;
	float max_y;
	float min_x;
	float min_y;
	float max_z;
	float heading;
	char name[20];
	uint32 item;
	uint32 max_allowed;
	uint32 respawntimer;
};
struct Ground_Spawns {
	struct Ground_Spawn spawn[50]; //Assigned max number to allow
};

struct DyeStruct
{
	union
	{
		struct
		{
			struct Color_Struct head;
			struct Color_Struct chest;
			struct Color_Struct arms;
			struct Color_Struct wrists;
			struct Color_Struct hands;
			struct Color_Struct legs;
			struct Color_Struct feet;
			struct Color_Struct primary;	// you can't actually dye this
			struct Color_Struct secondary;	// or this
		}
		dyes;
		struct Color_Struct dye[_MaterialCount];
	};
};

struct ZoneInSendName_Struct {
	uint32	unknown0;
	char	name[64];
	char	name2[64];
	uint32	unknown132;
};
struct ZoneInSendName_Struct2 {
	uint32	unknown0;
	char	name[64];
	uint32	unknown68[145];
};

struct Split_Struct
{
	uint32	platinum;
	uint32	gold;
	uint32	silver;
	uint32	copper;
};


/*
** New Combine Struct
** Client requesting to perform a tradeskill combine
** Size: 4 bytes
** Used In: OP_TradeSkillCombine
** Last Updated: Oct-15-2003
**
*/
struct NewCombine_Struct {
/*00*/	int16	container_slot;
/*04*/
};


//client requesting favorite recipies
struct TradeskillFavorites_Struct {
	uint32 object_type;
	uint32 some_id;
	uint32 favorite_recipes[500];
};

struct MerchantList {
	uint32	id;
	uint32	slot;
	uint32	item;
	int16	faction_required;
	int8	level_required;
	uint32	classes_required;
	uint8	probability;
};

struct TempMerchantList {
	uint32	npcid;
	uint32	slot;
	uint32	item;
	uint32	charges; //charges/quantity
	uint32	origslot;
};


struct NPC_Emote_Struct {
	uint32	emoteid;
	uint8	event_;
	uint8	type;
	char	text[515];
};

struct FindPerson_Point {
	float y;
	float x;
	float z;
};

struct FindPersonRequest_Struct {
	uint32	npc_id;
	FindPerson_Point client_pos;
};

//variable length packet of points
struct FindPersonResult_Struct {
	FindPerson_Point dest;
	FindPerson_Point path[0];	//last element must be the same as dest
};

//this is for custom title display in the skill window
struct TitleEntry_Struct {
	uint32	title_id;
	char	title[1];
	char	suffix[1];
};

struct Titles_Struct {
	uint32	title_count;
	TitleEntry_Struct titles[0];
};

//this is for title selection by the client
struct TitleListEntry_Struct {
	uint32	unknown0;	//title ID
	char prefix[1];		//variable length, null terminated
	char postfix[1];		//variable length, null terminated
};

struct TitleList_Struct {
	uint32 title_count;
	TitleListEntry_Struct titles[0];	//list of title structs
	//uint32 unknown_ending; seen 0x7265, 0
};

struct SetTitle_Struct {
	uint32	is_suffix;	//guessed: 0 = prefix, 1 = suffix
	uint32	title_id;
};

enum { VoiceMacroTell = 1, VoiceMacroGroup = 2, VoiceMacroRaid = 3 };

struct VoiceMacroOut_Struct {
/*000*/	char	From[64];
/*064*/	uint32	Type;	// 1 = Tell, 2 = Group, 3 = Raid
/*068*/	uint32	Unknown068;
/*072*/	uint32	Voice;
/*076*/	uint32	MacroNumber;
/*080*/	char	Unknown080[60];
};

struct RaidGeneral_Struct {
/*00*/	uint32		action;	//=10
/*04*/	char		player_name[64];	//should both be the player's name
/*64*/	char		leader_name[64];
/*132*/	uint32		parameter;
};

struct RaidAddMember_Struct {
/*000*/ RaidGeneral_Struct raidGen; //param = (group num-1); 0xFFFFFFFF = no group
/*136*/ uint8 _class;
/*137*/	uint8 level;
/*138*/	uint8 isGroupLeader;
/*139*/	uint8 flags[5]; //no idea if these are needed...
};

struct RaidMOTD_Struct {
/*000*/ RaidGeneral_Struct general; // leader_name and action only used
/*136*/ char motd[0]; // max size is 1024, but reply is variable
};

struct RaidAdd_Struct {
/*000*/	uint32		action;	//=0
/*004*/	char		player_name[64];	//should both be the player's name
/*068*/	char		leader_name[64];
/*132*/	uint8		_class;
/*133*/	uint8		level;
/*134*/	uint8		has_group;
/*135*/	uint8		unknown135;	//seems to be 0x42 or 0
};

struct RaidCreate_Struct {
/*00*/	uint32		action;	//=8
/*04*/	char		leader_name[64];
/*68*/	uint32		leader_id;
};

struct RaidMemberInfo_Struct {
/*00*/	uint8		group_number;
/*01*/	char		member_name[1];		//dyanmic length, null terminated '\0'
/*00*/	uint8		unknown00;
/*01*/	uint8		_class;
/*02*/	uint8		level;
/*03*/	uint8		is_raid_leader;
/*04*/	uint8		is_group_leader;
/*05*/	uint8		main_tank;		//not sure
/*06*/	uint8		unknown06[5];	//prolly more flags
};

struct RaidDetails_Struct {
/*000*/	uint32		action;	//=6,20
/*004*/	char		leader_name[64];
/*068*/	uint32		unknown68[4];
/*084*/	LeadershipAA_Struct abilities;	//ranks in backwards byte order
/*128*/	uint8		unknown128[142];
/*354*/	uint32		leader_id;
};

struct RaidMembers_Struct {
/*000*/	RaidDetails_Struct		details;
/*358*/	uint32					member_count;		//including leader
/*362*/	RaidMemberInfo_Struct	members[1];
/*...*/	RaidMemberInfo_Struct	empty;	//seem to have an extra member with a 0 length name on the end
};

struct Arrow_Struct {
/*000*/	uint32	type;		//unsure on name, seems to be 0x1, dosent matter
/*005*/	uint8	unknown004[12];
/*016*/	float	src_y;
/*020*/	float	src_x;
/*024*/	float	src_z;
/*028*/	uint8	unknown028[12];
/*040*/	float	velocity;		//4 is normal, 20 is quite fast
/*044*/	float	launch_angle;	//0-450ish, not sure the units, 140ish is straight
/*048*/	float	tilt;		//on the order of 125
/*052*/	float	yaw;
/*056*/ float	pitch;
/*060*/	float	arc;
/*064*/	uint8	unknown064[12];
/*076*/	uint32	source_id;
/*080*/ uint32	target_id;	//entity ID
/*084*/	uint32	object_id;	//1 to about 150ish
/*088*/	uint32	unknown088;	//seen 125, dosent seem to change anything..
/*092*/ uint32	unknown092;	//seen 16, dosent seem to change anything
/*096*/ uint8	light;
/*097*/	uint8	unknown097;
/*098*/	uint8	skill;
/*099*/	uint8	effect_type;
/*100*/	uint8	behavior;
/*101*/	char	model_name[16];
/*117*/	uint8	unknown117[19];
};

//made a bunch of trivial structs for stuff for opcode finder to use
struct Consent_Struct {
	char name[1];	//always at least a null
};

struct Save_Struct {
	uint8	unknown00[192];
};

struct GMToggle_Struct {
	uint8 unknown0[64];
	uint32 toggle;
};

struct GroupInvite_Struct {
	char invitee_name[64];
	char inviter_name[64];
//	uint8	unknown128[65];
};

struct UseAA_Struct {
	uint32 begin;
	uint32 ability;
	uint32 end;
};

struct AA_Ability {
/*00*/	uint32 skill_id;
/*04*/	uint32 base1;
/*08*/	uint32 base2;
/*12*/	uint32 slot;
};

struct SendAA_Struct {
/* EMU additions for internal use */
	char name[128];
	int16 cost_inc;
	uint32 sof_current_level;
	uint32 sof_next_id;
	uint8 level_inc;
	uint8 eqmacid;

/*0000*/	uint32 id;
/*0004*/	uint32 unknown004;
/*0008*/	uint32 hotkey_sid;
/*0012*/	uint32 hotkey_sid2;
/*0016*/	uint32 title_sid;
/*0020*/	uint32 desc_sid;
/*0024*/	uint32 class_type;
/*0028*/	uint32 cost;
/*0032*/	uint32 seq;
/*0036*/	uint32 current_level; //1s, MQ2 calls this AARankRequired
/*0040*/	uint32 prereq_skill;		//is < 0, abs() is category #
/*0044*/	uint32 prereq_minpoints; //min points in the prereq
/*0048*/	uint32 type;
/*0052*/	uint32 spellid;
/*0056*/	uint32 spell_type;
/*0060*/	uint32 spell_refresh;
/*0064*/	uint16 classes;
/*0066*/	uint16 berserker; //seems to be 1 if its a berserker ability
/*0068*/	uint32 max_level;
/*0072*/	uint32 last_id;
/*0076*/	uint32 next_id;
/*0080*/	uint32 cost2;
/*0084*/	uint32 unknown80[2]; //0s
// Begin SoF Specific/Adjusted AA Fields
/*0088*/	uint32 aa_expansion;
/*0092*/	uint32 special_category;
/*0096*/	uint32 sof_type;
/*0100*/	uint32 sof_cost_inc;
/*0104*/	uint32 sof_max_level;
/*0108*/	uint32 sof_next_skill;
/*0112*/	uint32 clientver;
/*0016*/	uint32 account_time_required;
/*0120*/	uint32 total_abilities;
/*0124*/	AA_Ability abilities[0];
};

struct AA_Action {
/*00*/	uint32	action;
/*04*/	uint32	ability;
/*08*/	uint32	unknown08;
/*12*/	uint32	exp_value;
};


struct AA_Skills {		//this should be removed and changed to AA_Array
/*00*/	uint32	aa_skill;						// Total AAs Spent
/*04*/	uint32	aa_value;
/*08*/	uint32	unknown08;
/*12*/
};

struct OldAA_Skills {
	uint8 aa_value;
};

struct OldAATable_Struct {
	uint8 unknown;
	OldAA_Skills aa_list[226];
};

struct AAExpUpdate_Struct {
/*00*/	uint32 unknown00;	//seems to be a value from AA_Action.ability
/*04*/	uint32 aapoints_unspent;
/*08*/	uint8 aaxp_percent;	//% of exp that goes to AAs
/*09*/	uint8 unknown09[3];	//live dosent always zero these, so they arnt part of aaxp_percent
};


struct AltAdvStats_Struct {
/*000*/	uint32 experience;
/*004*/	uint16 unspent;
/*006*/	uint16	unknown006;
/*008*/	uint8	percentage;
/*009*/	uint8	unknown009[3];
};

struct PlayerAA_Struct {						// Is this still used?
	AA_Skills aa_list[MAX_PP_AA_ARRAY];
};

struct AATable_Struct {
/*00*/ int32		aa_spent;					// Total AAs Spent
/*04*/ AA_Skills	aa_list[MAX_PP_AA_ARRAY];
};

struct Weather_Struct {
	uint32	val1;	//generall 0x000000FF
	uint32	type;	//0x31=rain, 0x02=snow(i think), 0 = normal
	uint32	mode;
};

struct ZoneInUnknown_Struct {
	uint32	val1;
	uint32	val2;
	uint32	val3;
};

struct MobHealth_Struct {
	uint16 entity_id;
	uint8 hp;
};

// This is the structure for OP_ZonePlayerToBind opcode. Discovered on Feb 9 2007 by FNW from packet logs for titanium client
// This field "zone_name" is text the Titanium client will display on player death
// it appears to be a variable length, null-terminated string
// In logs it has "Bind Location" text which shows up on Titanium client as ....
// "Return to Bind Location, please wait..."
// This can be used to send zone name instead.. On 6.2 client, this is ignored.
struct ZonePlayerToBind_Struct {
/*000*/	uint16 bind_zone_id;
/*002*/	uint16 bind_instance_id;
/*004*/	float x;
/*008*/	float y;
/*012*/	float z;
/*016*/	float heading;
/*020*/	char zone_name[1];
};

typedef struct {
/*000*/	uint32	bind_number;		// Number of this bind in the iteration
/*004*/	uint32	bind_zone_id;		// ID of the zone for this bind point or resurect point
/*008*/	float	x;					// X loc for this bind point
/*012*/	float	y;					// Y loc for this bind point
/*016*/	float	z;					// Z loc for this bind point
/*020*/	float	heading;			// Heading for this bind point
/*024*/	char	bind_zone_name[1];	// Or "Bind Location" or "Resurrect"
/*000*/	uint8	validity;		// 0 = valid choice, 1 = not a valid choice at this time (resurrection)
} RespawnOptions_Struct;

struct RespawnWindow_Struct {
/*000*/	uint32	unknown000;		// Seen 0
/*004*/	uint32	time_remaining;	// Total time before respawn in milliseconds
/*008*/	uint32	unknown008;		// Seen 0
/*012*/	uint32	total_binds;	// Total Bind Point Options? - Seen 2
/*016*/ RespawnOptions_Struct bind_points;
// First bind point is "Bind Location" and the last one is "Ressurect"
};

/**
 * Shroud spawn. For others shrouding, this has their spawnId and
 * spawnStruct.
 *
 * Length: 586
 * OpCode: OP_Shroud
 */
struct spawnShroudOther
{
/*0000*/ uint32 spawnId;		// Spawn Id of the shrouded player
/*0004*/ Spawn_Struct spawn;	// Updated spawn struct for the player
/*0586*/
};

struct ApplyPoison_Struct {
	uint32 inventorySlot;
	uint32 success;
};

struct ItemVerifyRequest_Struct {
/*000*/	int32	slot;		// Slot being Right Clicked
/*004*/	uint32	target;		// Target Entity ID
/*008*/
};

struct ItemVerifyReply_Struct {
/*000*/	int32	slot;		// Slot being Right Clicked
/*004*/	uint32	spell;		// Spell ID to cast if different than item effect
/*008*/	uint32	target;		// Target Entity ID
/*012*/
};

struct ItemRecastDelay_Struct {
/*000*/	uint32	recast_delay;	// in seconds
/*004*/	uint32	recast_type;
/*008*/	uint32	unknown008;
/*012*/
};

/**
 * Shroud yourself. For yourself shrouding, this has your spawnId, spawnStruct,
 * bits of your charProfileStruct (no checksum, then charProfile up till
 * but not including name), and an itemPlayerPacket for only items on the player
 * and not the bank.
 *
 * Length: Variable
 * OpCode: OP_Shroud
 */
#if 0
struct spawnShroudSelf
{
/*00000*/ uint32 spawnId;				// Spawn Id of you
/*00004*/ Spawn_Struct spawn;			// Updated spawnStruct for you
//this is a sub-struct of PlayerProfile, which we havent broken out yet.
/*00586*/ playerProfileStruct profile;	// Character profile for shrouded char
/*13522*/ uint8 items;					// Items on the player
/*xxxxx*/
};
#endif


struct ControlBoat_Struct {
/*000*/	uint32	boatId;			// entitylist id of the boat
/*004*/	bool	TakeControl;	// 01 if taking control, 00 if releasing it
/*007*/	char	unknown[3];		// no idea what these last three bytes represent
};

struct OldControlBoat_Struct
{
	/*000*/	uint16	boatId;			// entitylist id of the boat
	/*002*/	bool	TakeControl;	// 01 if taking control, 00 if releasing it
	/*003*/ uint8   unknown5;	// no idea what these last byte represent
	/*004*/							
};

struct ClearObject_Struct
{
/*000*/	uint8	Clear;	// If this is not set to non-zero there is a random chance of a client crash.
/*001*/	uint8	Unknown001[7];
};

struct DisciplineTimer_Struct
{
/*00*/ uint32	TimerID;
/*04*/ uint32	Duration;
/*08*/ uint32	Unknown08;
};

struct GMSearchCorpse_Struct
{
/*000*/	char Unknown000[64];
/*064*/	char Name[64];
/*128*/	uint32 Unknown128;
};

struct BeggingResponse_Struct
{
/*00*/	uint32	Unknown00;
/*04*/	uint32	Unknown04;
/*08*/	uint32	Unknown08;
/*12*/	uint32	Result;	// 0 = Fail, 1 = Plat, 2 = Gold, 3 = Silver, 4 = Copper
/*16*/	uint32	Amount;
};

struct BuffIconEntry_Struct
{
	uint32 buff_slot;
	uint32 spell_id;
	uint32 tics_remaining;
	uint32 num_hits;
};

struct BuffIcon_Struct
{
	uint32 entity_id;
	uint8  all_buffs;
	uint16 count;
	BuffIconEntry_Struct entries[0];
};

struct CorpseDrag_Struct
{
/*000*/ char CorpseName[64];
/*064*/ char DraggerName[64];
/*128*/ uint8 Unknown128[24];
/*152*/
};

struct Untargetable_Struct {
/*000*/	uint32 id;
/*004*/	uint32 targetable_flag; //0 = not targetable, 1 or higher = targetable
/*008*/
};

struct LFGuild_SearchPlayer_Struct
{
/*00*/	uint32	Command;
/*04*/	uint32	Unknown04;
/*08*/	uint32	FromLevel;
/*12*/	uint32	ToLevel;
/*16*/	uint32	MinAA;
/*20*/	uint32	TimeZone;
/*24*/	uint32	Classes;
};

struct LFGuild_SearchGuild_Struct
{
/*00*/	uint32	Command;
/*04*/	uint32	Unknown04;
/*08*/	uint32	Level;
/*12*/	uint32	AAPoints;
/*16*/	uint32	TimeZone;
/*20*/	uint32	Class;
/*24*/
};

struct LFGuild_PlayerToggle_Struct
{
/*000*/ uint32	Command;
/*004*/ uint8	Unknown004[68];
/*072*/ char	Comment[256];
/*328*/ uint8	Unknown328[268];
/*596*/ uint32	TimeZone;
/*600*/ uint8	Toggle;
/*601*/ uint8	Unknown601[7];
/*608*/ uint32	TimePosted;
/*612*/ uint8	Unknown612[12];
/*624*/
};

struct LFGuild_GuildToggle_Struct
{
/*000*/ uint32	Command;
/*004*/ uint8	Unknown004[8];
/*012*/ char	Comment[256];
/*268*/ uint8	Unknown268[256];
/*524*/ uint32	FromLevel;
/*528*/ uint32	ToLevel;
/*532*/ uint32	Classes;
/*536*/ uint32	AACount;
/*540*/ uint32	TimeZone;
/*544*/ uint8	Toggle;
/*545*/ uint8	Unknown545[3];
/*548*/ uint32	TimePosted;
/*552*/ char	Name[64];
/*616*/
};

struct ServerLootItem_Struct {
	uint32	item_id;	  // uint32	item_id;
	int16	equip_slot;	  // int16	equip_slot;
	uint16	charges;	  // uint8	charges; 
	uint16	lootslot;	  // uint16	lootslot;
	uint8	min_level;		  // 
	uint8	max_level;		  // 
};

//Found in client near a ref to the string:
//"Got a broadcast message for ... %s ...\n"
struct ClientMarqueeMessage_Struct {
	uint32 type;
	uint32 unk04; // no idea, didn't notice a change when altering it.
	//According to asm the following are hard coded values: 2, 4, 5, 6, 7, 10, 12, 13, 14, 15, 16, 18, 20
	//There is also a non-hardcoded fall through but to be honest i don't know enough about what it does yet
	uint32 priority; //needs a better name but it does:
	//opacity = (priority / 255) - floor(priority / 255)
	//# of fade in/out blinks = (int)((priority - 1) / 255)
	//so 510 would have 100% opacity and 1 extra blink at end
	uint32 fade_in_time; //The fade in time, in ms
	uint32 fade_out_time; //The fade out time, in ms
	uint32 duration; //in ms
	char msg[1]; //message plus null terminator
	
};

struct Checksum_Struct {
	uint64 checksum;
	uint8  data[2048];
};

struct Disarm_Struct {
/*000*/ int16 entityid;
/*002*/ int16 target;
/*004*/ int16 skill;
/*006*/ int16 status;
/*008*/
};


//C->S, used by the client for adding Soulmarks to a player. 
//0x41D0
struct SoulMarkUpdate_Struct {
	char gmname[64];
	char gmacctname[32];
	char charname[64];
	char acctname[32];
};

//S->C and C->S ENTRY for informing us about players' soulmarks. Used in struct below this one.
struct SoulMarkEntry_Struct {
	char name[64]; //charname
	char accountname[32]; //station name
	char gmname[64];  //charname
	char gmaccountname[32]; //gm station name
	uint32 unix_timestamp; //time of remark
	uint32 type; //time of remark
	char description[256]; //actual text description
};

//S->C C->S PACKET
//0x41D1
struct SoulMarkList_Struct {
	char interrogatename[64];
	SoulMarkEntry_Struct entries[12]; //I have no idea why SOE hardcodes the amount of soulmarks, but it shall never exceed 12. Delete soulmarks I guess in the future /shrug
};

//C->S PACKET
//0x41D2
struct SoulMarkAdd_Struct{
	SoulMarkEntry_Struct entry;
};


struct Feedback_Struct {
/*0000*/ char	name[64];	
/*0064*/ char	unknown[43];
/*0107*/ char	message[1024];
/*1131*/
};

// Struct for Clients Request to Show specific message board
struct MBRetrieveMessages_Struct {
	uint16 entityID;
	uint16 category; // category is the type of board selected by the client
	/* Categories */ 
	/* 00 - OFFICIAL */
	/* 01 - FOR SALE */
	/* 02 - GENERAL */
	/* 03 - HELP WANTED */
	/* 04 - PERSONALS */
	/* 05 - GUILDS */
};


struct MBMessageRetrieval_Struct {		
	uint32 id; 
	char date[10]; /* char year[2]; char month[2]; char day[2]; */
	char unknown4[4];
	char author[64];
	uint8 language;
	char unknown8;
	char subject[64];
	uint8 category;
	char unknown12;
}; 

struct MBMessageRetrievalGen_Struct {		
	uint32 id; 
	char date[10]; /* char year[2]; char month[2]; char day[2]; */
	char unknown4[4];
	char author[64];
	uint8 language;
	char unknown8;
	char subject[64];
	uint8 category;
	char unknown12;
	char message[2048]; // see eqgame function at .text:0047D4E5
}; 


struct MBModifyRequest_Struct {
	uint16 EntityID;
	uint16 id;	
	uint16 category;
		 
}; 

struct MBEraseRequest_Struct {
	 uint16 EntityID;
	 uint16 id;
	 uint16 category;
	 uint16 unknown;

}; 

typedef std::list<ServerLootItem_Struct*> ItemList;

// Restore structure packing to default
#pragma pack()

#endif

