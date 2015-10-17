#ifndef MAC_STRUCTS_H_
#define MAC_STRUCTS_H_

#include <string>

namespace Mac {
	namespace structs {

/*
** Compiler override to ensure
** byte aligned structures
*/
#pragma pack(1)

struct CharacterSelect_Struct
{
	/*0000*/	char	name[10][64];		// Characters Names
	/*0640*/	uint8	level[10];			// Characters Levels
	/*0650*/	uint8	class_[10];			// Characters Classes
	/*0660*/	uint16	race[10];			// Characters Race
	/*0680*/	uint32	zone[10];			// Characters Current Zone
	/*0720*/	uint8	gender[10];			// Characters Gender
	/*0730*/	uint8	face[10];			// Characters Face Type
	/*0740*/	uint32	equip[10][9];		// 0=helm, 1=chest, 2=arm, 3=bracer, 4=hand, 5=leg, 6=boot, 7=melee1, 8=melee2
	/*1100*/	Color_Struct cs_colors[10][9];	// Characters Equipment Colors (RR GG BB 00)
	/*1460*/	uint16	deity[10];			// Characters Deity
	/*1480*/	uint32	primary[10];		// Characters primary and secondary IDFile number
	/*1520*/	uint32	secondary[10];		// Characters primary and secondary IDFile number
	/*1560*/	uint8	haircolor[10]; 
	/*1570*/	uint8	beardcolor[10];	
	/*1580*/	uint8	eyecolor1[10]; 
	/*1590*/	uint8	eyecolor2[10]; 
	/*1600*/	uint8	hairstyle[10]; 
	/*1610*/	uint8	beard[10];
	/*1620*/
};

struct ServerZoneEntry_Struct 
{
	/*0000*/	uint8	checksum[4];		// Checksum
	/*0004*/	uint8	type;		// ***Placeholder
	/*0005*/	char	name[64];			// Name
	/*0069*/	uint8	sze_unknown0069;	// ***Placeholder
	/*0070*/	uint16	unknown0070;		// ***Placeholder
	/*0072*/	uint32	zoneID;				// Current Zone
	/*0076*/	float	y_pos;				// Y Position
	/*0080*/	float	x_pos;				// X Position
	/*0084*/	float	z_pos;				// Z Position
	/*0088*/	float	heading;
	/*0092*/	float	physicsinfo[8];
	/*0124*/	int32	prev;
	/*0128*/	int32	next;
	/*0132*/	int32	corpse;
	/*0136*/	int32	LocalInfo;
	/*0140*/	int32	My_Char;
	/*0144*/	float	view_height;
	/*0148*/	float	sprite_oheight;
	/*0152*/	uint16	sprite_oheights;
	/*0154*/	uint16	petOwnerId;
	/*0156*/	uint32	max_hp;
	/*0160*/	uint32	curHP;
	/*0164*/	uint16	GuildID;			// Guild ID Number
	/*0166*/	uint8	my_socket[6];		// ***Placeholder
	/*0172*/	uint8	NPC;
	/*0173*/	uint8	class_;				// Class
	/*0174*/	uint16	race;				// Race
	/*0176*/	uint8	gender;				// Gender
	/*0177*/	uint8	level;				// Level
	/*0178*/	uint8	invis;
	/*0179*/	uint8	sneaking;
	/*0180*/	uint8	pvp;				// PVP Flag
	/*0181*/	uint8	anim_type;
	/*0182*/	uint8	light;
	/*0183*/	int8	face;				// Face Type
	/*0184*/    uint16  equipment[9]; // Array elements correspond to struct equipment above
	/*0202*/	uint16	unknown; //Probably part of equipment
	/*0204*/	Color_Struct equipcolors[9]; // Array elements correspond to struct equipment_colors above
	/*0240*/	uint32	bodytexture;	// Texture (0xFF=Player - See list of textures for more)
	/*0244*/	float	size;
	/*0248*/	float	width;
	/*0252*/	float	length;
	/*0256*/	uint32	helm;
	/*0260*/	float	walkspeed;			// Speed when you walk
	/*0264*/	float	runspeed;			// Speed when you run
	/*0268*/	int8	LD;
	/*0269*/	int8	GM;
	/*0270*/	int16	flymode;
	/*0272*/	int8	bodytype;
	/*0273*/	int8	view_player[7];
	/*0280*/	uint8	anon;				// Anon. Flag
	/*0281*/	uint16	avatar;
	/*0283*/	uint8	AFK;
	/*0284*/	uint8	summoned_pc;
	/*0285*/	uint8	title;
	/*0286*/	uint8	extra[18];	// ***Placeholder (At least one flag in here disables a zone point or all)
	/*0304*/	char	Surname[32];		// Lastname (This has to be wrong.. but 70 is to big =/..)
	/*0336*/	uint16  guildrank;
	/*0338*/	uint16	deity;				// Diety (Who you worship for those less literate)
	/*0340*/	uint8	animation;		// ***Placeholder
	/*0341*/	uint8	haircolor;			// Hair Color
	/*0342*/	uint8	beardcolor;			// Beard Color
	/*0343*/	uint8	eyecolor1;			// Left Eye Color
	/*0344*/	uint8	eyecolor2;			// Right Eye Color
	/*0345*/	uint8	hairstyle;			// Hair Style
	/*0346*/	uint8	beard;				// AA Title
	/*0347*/	uint32	SerialNumber;
	/*0351*/	char	m_bTemporaryPet[4];
	/*0355*/	uint8	void_;
	/*0356*/
};

struct ZoneServerInfo_Struct
{
	/*0000*/	char	ip[128];
	/*0128*/	uint16	port;
};

struct NewZone_Struct 
{
	/*0000*/	char	char_name[64];			// Character Name
	/*0064*/	char	zone_short_name[32];	// Zone Short Name
	/*0096*/	char	zone_long_name[278];	// Zone Long Name
	/*0374*/	uint8	ztype;
	/*0375*/	uint8	fog_red[4];				// Red Fog 0-255 repeated over 4 bytes (confirmed)
	/*0379*/	uint8	fog_green[4];			// Green Fog 0-255 repeated over 4 bytes (confirmed)
	/*0383*/	uint8	fog_blue[4];			// Blue Fog 0-255 repeated over 4 bytes (confirmed)
	/*0387*/	uint8	unknown387;
	/*0388*/	float	fog_minclip[4];			// Where the fog begins (lowest clip setting). Repeated over 4 floats. (confirmed)
	/*0404*/	float	fog_maxclip[4];			// Where the fog ends (highest clip setting). Repeated over 4 floats. (confirmed)	
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
	/*0475*/	uint8   unknown0475;
	/*0476*/	uint16  water_music;
	/*0478*/	uint16  normal_music_day;
	/*0480*/	uint16  normal_music_night;
	/*0482*/	uint8	unknown0482[2];
	/*0484*/	float	zone_exp_multiplier;	// Experience Multiplier
	/*0488*/	float	safe_y;					// Zone Safe Y
	/*0492*/	float	safe_x;					// Zone Safe X
	/*0496*/	float	safe_z;					// Zone Safe Z
	/*0500*/	float	max_z;					// Guessed
	/*0504*/	float	underworld;				// Underworld, min z (Not Sure?)
	/*0508*/	float	minclip;				// Minimum View Distance
	/*0512*/	float	maxclip;				// Maximum View DIstance
	/*0516*/	uint32	forage_novice;
	/*0520*/	uint32	forage_medium;
	/*0524*/	uint32	forage_advanced;
	/*0528*/	uint32	fishing_novice;
	/*0532*/	uint32	fishing_medium;
	/*0536*/	uint32	fishing_advanced;
	/*0540*/	uint32	skylock;
	/*0544*/	uint16	graveyard_tme;
	/*0546*/	uint32	scriptPeriodicHour;
	/*0550*/	uint32	scriptPeriodicMinute;
	/*0554*/	uint32	scriptPeriodicFast;
	/*0558*/	uint32	scriptPlayerDead;
	/*0562*/	uint32	scriptNpcDead;
	/*0566*/	uint32  scriptPlayerEntering;
	/*0570*/	uint16	unknown570;		// ***Placeholder
	/*0572*/
};

struct MemorizeSpell_Struct 
{ 
	/*000*/		uint32 slot;			// Comment:  Spot in the spell book/memorized slot 
	/*004*/		uint32 spell_id;		// Comment:  Spell id (200 or c8 is minor healing, etc) 
	/*008*/		uint32 scribing;		// Comment:  1 if memorizing a spell, set to 0 if scribing to book 
	/*012*/
}; 

struct CombatDamage_Struct
{
	/*000*/	uint16	target;
	/*002*/	uint16	source;
	/*004*/	uint8	type;
	/*005*/	uint8   unknown;
	/*006*/	uint16	spellid;
	/*008*/	int32	damage;
	/*012*/	float	force;
	/*016*/	float	sequence;
	/*020*/	float	pushup_angle;
	/*024*/
};

struct Action_Struct
{
	/*00*/	uint16	target;			// Comment: Spell Targets ID 
	/*02*/	uint16	source;			// Comment: Spell Caster ID
	/*04*/	uint8	level;		// Comment: Spell Casters Level
	/*05*/	uint8	unknown5;
	/*06*/	uint8   unknown6;  //0x41
	/*07*/	uint8	unknown7;		// Comment: Unknown -> needs confirming 
	/*08*/	int32	instrument_mod;
	/*12*/	float	force;
	/*16*/	uint32	sequence;			// Comment: Heading of Who? Caster or Target? Needs confirming
	/*20*/	float	pushup_angle;
	/*24*/	uint8	type;				// Comment: Unknown -> needs confirming -> Which action target or caster does maybe?
	/*25*/	uint8	unknown28[5];			// Comment: Spell ID of the Spell being casted? Needs Confirming
	/*30*/	uint16	spell;		// Comment: Unknown -> needs confirming
	/*32*/	uint8	unknown32;		//0x00
	/*33*/  uint8	buff_unknown;
	/*34*/	uint16  unknown34;
};

struct BeginCast_Struct
{
	/*000*/	uint16	caster_id;		// Comment: Unknown -> needs confirming -> ID of Spell Caster? 
	/*002*/	uint16	spell_id;		// Comment: Unknown -> needs confirming -> ID of Spell being Cast?
	/*004*/	uint16	cast_time;		// Comment: Unknown -> needs confirming -> in miliseconds?
	/*006*/ uint16  unknown;
	/*008*/
};

struct Buff_Struct
{
	/*000*/	uint32	entityid;		// Comment: Unknown -> needs confirming -> Target of the Buff
	/*004*/	uint32	b_unknown1;		// Comment: Unknown -> needs confirming
	/*008*/	uint16	spellid;		// Comment: Unknown -> needs confirming -> Spell ID?
	/*010*/	uint32	b_unknown2;		// Comment: Unknown -> needs confirming
	/*014*/	uint16	b_unknown3;		// Comment: Unknown -> needs confirming
	/*016*/	uint32	slotid;		// Comment: Unknown -> needs confirming -> Which buff slot on the target maybe?
	/*020*/
};

struct CastSpell_Struct
{
	/*000*/	uint16	slot;
	/*002*/	uint16	spell_id;
	/*004*/	uint16	inventoryslot;  // slot for clicky item, 0xFFFF = normal cast
	/*006*/	uint16	target_id;
	/*008*/	uint32	cs_unknown2;
	/*012*/
};
		
struct SpawnAppearance_Struct
{
	// len = 8
	/*000*/ uint16 spawn_id;          // ID of the spawn
	/*002*/ uint16 type;              // Values associated with the type
	/*004*/ uint32 parameter;         // Type of data sent
	/*008*/
};

// Length: 20
struct SpellBuffFade_Struct
{
	/*000*/	uint16	entityid;
	/*002*/	uint8   slot;
	/*003*/ uint8   level;
	/*004*/ uint8   effect;
	/*005*/ uint8   unknown1;
	/*006*/	uint16  spellid;
	/*008*/	uint32	duration;
	/*012*/	uint32	slotid;	
	/*016*/	uint32	bufffade;
	/*020*/
};

/*
** client changes target struct
** Length: 2 Bytes
** OpCode: 6241
*/
struct ClientTarget_Struct
{
	/*000*/	uint16	new_target;			// Target ID
	/*002*/
};

struct Spawn_Struct
{
	/*0000*/	uint32  random_dontuse;
	/*0004*/	uint8	accel;
	/*0005*/	uint8	heading;			// Current Heading
	/*0006*/	uint8	deltaHeading;		// Delta Heading
	/*0007*/	int16	y_pos;				// Y Position
	/*0009*/	int16	x_pos;				// X Position
	/*0011*/	int16	z_pos;				// Z Position
	/*0013*/	int32	deltaY:11,			// Velocity Y
						deltaZ:11,			// Velocity Z
						deltaX:10;			// Velocity X
	/*0017*/	uint8	void1;
	/*0018*/	uint16	petOwnerId;		// Id of pet owner (0 if not a pet)
	/*0020*/	uint8	animation;
	/*0021*/    uint8	haircolor; 
	/*0022*/	uint8	beardcolor;	
	/*0023*/	uint8	eyecolor1; 
	/*0024*/	uint8	eyecolor2; 
	/*0025*/	uint8	hairstyle; 
	/*0026*/	uint8	beard;
	/*0027*/    uint8   title; //0xff
	/*0028*/	float	size;
	/*0032*/	float	walkspeed;
	/*0036*/	float	runspeed;
	/*0040*/	Color_Struct	equipcolors[9];
	/*0076*/	uint16	spawn_id;			// Id of new spawn
	/*0078*/	uint16	bodytype;			// 65 is disarmable trap, 66 and 67 are invis triggers/traps
	/*0080*/	int16	cur_hp;				// Current hp's of Spawn
	/*0082*/	uint16	GuildID;			// GuildID - previously Current hp's of Spawn
	/*0084*/	uint16	race;				// Race
	/*0086*/	uint8	NPC;				// NPC type: 0=Player, 1=NPC, 2=Player Corpse, 3=Monster Corpse, 4=???, 5=Unknown Spawn,10=Self
	/*0087*/	uint8	class_;				// Class
	/*0088*/	uint8	gender;				// Gender Flag, 0 = Male, 1 = Female, 2 = Other
	/*0089*/	uint8	level;				// Level of spawn (might be one int8)
	/*0090*/	uint8	invis;				// 0=visable, 1=invisable
	/*0091*/	uint8	sneaking;
	/*0092*/	uint8	pvp;
	/*0093*/	uint8	anim_type;
	/*0094*/	uint8	light;				// Light emitting
	/*0095*/	uint8	anon;				// 0=normal, 1=anon, 2=RP
	/*0096*/	uint8	AFK;				// 0=off, 1=on
	/*0097*/	uint8	summoned_pc;
	/*0098*/	uint8	LD;					// 0=NotLD, 1=LD
	/*0099*/	uint8	GM;					// 0=NotGM, 1=GM
	/*0100*/	uint8	flymode;				
	/*0101*/	uint8	bodytexture;
	/*0102*/	uint8	helm; 
	/*0103*/	uint8	face;		
	/*0104*/	uint16	equipment[9];		// Equipment worn: 0=helm, 1=chest, 2=arm, 3=bracer, 4=hand, 5=leg, 6=boot, 7=melee1, 8=melee2
	/*0122*/	int16	guildrank;			// ***Placeholder
	/*0124*/	uint16	deity;				// Deity.
	/*0126*/	uint8	temporaryPet;			
	/*0127*/	char	name[64];			// Name of spawn (len is 30 or less)
	/*0191*/	char	Surname[32];		// Last Name of player
	/*0223*/	uint8	void_;		
	/*0224*/
};

struct DeleteSpawn_Struct
{
	/*00*/ uint16 spawn_id;				// Comment: Spawn ID to delete
	/*02*/
};

//New ChannelMessage_Struct struct
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

struct WearChange_Struct
{
	/*000*/ uint16 spawn_id;
	/*002*/ uint16 wear_slot_id;
	/*004*/ uint16 material;
	/*006*/ uint16 color;
	/*008*/ uint8  blue;
	/*009*/	uint8  green;
	/*010*/	uint8  red;
	/*011*/	uint8  use_tint;	// if there's a tint this is FF
	/*012*/	
};

struct Object_Struct 
{
	/*000*/		int8	unknown001[8];
	/*008*/		uint16  itemid;
	/*010*/		uint16  unknown010;
	/*012*/		int32	drop_id;	//this id will be used, if someone clicks on this object
	/*016*/		int16	zone_id;
	/*018*/		uint8   unknown014[6];
	/*024*/		uint8   charges;
	/*025*/		uint8   unknown25;
	/*026*/		uint8   maxcharges;
	/*027*/		uint8	unknown027[113];		// ***Placeholder
	/*140*/		float   heading;
	/*144*/		float	z;
	/*148*/		float	x;
	/*152*/		float	y;					// Z Position
	/*156*/		char	object_name[16];				// ACTOR ID
	/*172*/		int8	unknown172[14];
	/*186*/		int16	itemsinbag[10]; //if you drop a bag, thats where the items are
	/*206*/		int16	unknown206;
	/*208*/		int32	unknown208;
	/*212*/		uint32	object_type;
	/*216*/		int16	unknown216[4];
	/*224*/
};

struct ClickObjectAction_Struct
{
	/*000*/	uint32	player_id;		// Comment: Entity Id of player who clicked object
	/*004*/	uint32	drop_id;		// Comment: Unknown 
	/*008*/	uint8	open;			// Comment: 0=close 1=open
	/*009*/	uint8	unknown9;		// Comment: Unknown 
	/*010*/	uint8	type;			// Comment: number that determines the type of the item (i.e forge) 
	/*011*/	uint8	unknown11;		// Comment: Unknown 
	/*012*/	uint8	slot;			// Comment: number of slots of the container
	/*013*/  uint8	unknown10[3];	// Comment: Unknown 
	/*016*/	uint16	icon;		// Comment: Icon of the item
	/*018*/	uint8	unknown16[2];	// Comment: Unknown
	/*020*/	
};

struct Charm_Struct 
{
	/*000*/	uint16	owner_id;
	/*002*/	uint16	pet_id;
	/*004*/	uint16	command; // 1: make pet, 0: release pet
	/*006*/
};

struct ZoneChange_Struct
{
	/*000*/	char	char_name[64];     // Character Name
	/*064*/	uint16	zoneID;
	/*066*/ uint16  zone_reason;
	/*068*/ uint16  unknown[2];
	/*072*/	int8	success;		// =0 client->server, =1 server->client, -X=specific error
	/*073*/	uint8	error[3]; // =0 ok, =ffffff error
	/*076*/	
};

struct Consider_Struct
{
	/*000*/ uint16	playerid;               // PlayerID
	/*002*/ uint16	targetid;               // TargetID
	/*004*/ uint32	faction;                // Faction
	/*008*/ uint32	level;                  // Level
	/*012*/ int32	cur_hp;			// Current Hitpoints
	/*016*/ int32	max_hp;			// Maximum Hitpoints
	/*020*/ uint8	pvpcon;			// Pvp con flag 0/1
	/*021*/ uint8	unknown3[3];
	/*024*/	
};

struct Death_Struct
{
	/*000*/	uint16	spawn_id;		// Comment: 
	/*002*/	uint16	killer_id;		// Comment: 
	/*004*/	uint16	corpseid;		// Comment: corpseid used for looting PC corpses !
	/*006*/	uint8	spawn_level;		// Comment: 
	/*007*/ uint8   unknown007;
	/*008*/	uint16	spell_id;	// Comment: Attack skill (Confirmed )
	/*010*/	uint8	attack_skill;		// Comment: 
	/*011*/ uint8   unknonw011;
	/*012*/	uint32	damage;			// Comment: Damage taken, (Confirmed )
	/*016*/ uint8   is_PC;		// Comment: 
	/*017*/ uint8   unknown015[3];
	/*020*/
};

struct SpawnHPUpdate_Struct
{
	/*000*/ uint32  spawn_id;		// Comment: Id of spawn to update
	/*004*/ int32 cur_hp;		// Comment:  Current hp of spawn
	/*008*/ int32 max_hp;		// Comment: Maximum hp of spawn
	/*012*/	
};

struct SpecialMesg_Struct
{
	/*0000*/ uint32 msg_type;		// Comment: Type of message
	/*0004*/ char  message[0];		// Comment: Message, followed by four bytes?
};

struct ExpUpdate_Struct
{
	/*000*/ uint32 exp;			// Comment: Current experience value
	/*004*/	
};

#define ITEM_STRUCT_SIZE 360
#define SHORT_BOOK_ITEM_STRUCT_SIZE	264
#define SHORT_CONTAINER_ITEM_STRUCT_SIZE 276
struct Item_Struct
{
	/*0000*/ char      Name[64];        // Name of item
	/*0064*/ char      Lore[80];        // Lore text
	/*0144*/ char      IDFile[30];       // This is the filename of the item graphic when held/worn.
	/*0174*/ uint8	   Weight;          // Weight of item
	/*0175*/ int8      NoRent;          // Nosave flag 1=normal, 0=nosave, -1=spell?
	/*0176*/ int8      NoDrop;          // Nodrop flag 1=normal, 0=nodrop, -1=??
	/*0177*/ uint8     Size;            // Size of item
	/*0178*/ int16     ItemClass;
	/*0180*/ int16	   ID;				// Record number. Confirmed to be signed.
	/*0182*/ uint16    Icon;         // Icon Number
	/*0184*/ int16     equipSlot;       // Current slot location of item
	/*0186*/ uint8     unknown0186[2];   // Client dump has equipSlot/location as a short so this is still unknown
	/*0188*/ uint32    Slots;  // Slots where this item is allowed
	/*0192*/ int32     Price;            // Item cost in copper
	/*0196*/ float     cur_x; //Here to 227 are named from client struct dump.
	/*0200*/ float	   cur_y;
	/*0204*/ float     cur_z;
	/*0208*/ float     heading;
	/*0212*/ uint32	   inv_refnum;  // Unique serial. This is required by apply poison. We're just sending slot for now, in the future a serial system will be needed. 
	/*0216*/ int16	   log; 
	/*0218*/ int16     loot_log;
	/*0220*/ uint16    avatar_level;  //Usually 01, sometimes seen as FFFF, once as 0.
	/*0222*/ uint16    bottom_feed;
	/*0224*/ uint32	   poof_item;
	union
	{
		struct
		{
			// 0228- have different meanings depending on flags
			/*0228*/ int8      AStr;              // Strength
			/*0229*/ int8      ASta;              // Stamina
			/*0230*/ int8      ACha;              // Charisma
			/*0231*/ int8      ADex;              // Dexterity
			/*0232*/ int8      AInt;              // Intelligence
			/*0233*/ int8      AAgi;              // Agility
			/*0234*/ int8      AWis;              // Wisdom
			/*0235*/ int8      MR;               // Magic Resistance
			/*0236*/ int8      FR;               // Fire Resistance
			/*0237*/ int8      CR;               // Cold Resistance
			/*0238*/ int8      DR;               // Disease Resistance
			/*0239*/ int8      PR;               // Poison Resistance
			/*0240*/ int16     HP;               // Hitpoints
			/*0242*/ int16     Mana;             // Mana
			/*0244*/ int16     AC;				 // Armor Class
			/*0246*/ uint8     MaxCharges;       // Maximum number of charges, for rechargable? (Sept 25, 2002)
			/*0247*/ int8      GMFlag;           // GM flag 0  - normal item, -1 - gm item (Sept 25, 2002)
			/*0248*/ uint8     Light;            // Light effect of this item
			/*0249*/ uint8     Delay;            // Weapon Delay
			/*0250*/ uint8     Damage;           // Weapon Damage
			/*0251*/ int8      EffectType1;      // 0=combat, 1=click anywhere w/o class check, 2=latent/worn, 3=click anywhere EXPENDABLE, 4=click worn, 5=click anywhere w/ class check, -1=no effect
			/*0252*/ uint8     Range;            // Range of weapon
			/*0253*/ uint8     ItemType;            // Skill of this weapon, refer to weaponskill chart
			/*0254*/ int8      Magic;            // Magic flag
			/*0255*/ int8      EffectLevel1;           // Casting level
			/*0256*/ uint32    Material;         // Material
			/*0260*/ uint32    Color;            // Amounts of RGB in original color
			/*0264*/ uint16    Faction;			// Structs dumped from client has this as Faction
			/*0266*/ uint16    Effect1;         // SpellID of special effect
			/*0268*/ uint32    Classes;          // Classes that can use this item
			/*0272*/ uint32    Races;            // Races that can use this item
			/*0276*/ int8      Stackable;        //  1= stackable, 3 = normal, 0 = ? (not stackable)			
		} common; 
		struct
		{
			/*0228*/ int16	  BookType;	 // Type of book (scroll, note, etc)
			/*0230*/ int8     Book;      // Are we a book
			/*0231*/ char     Filename[30];            // Filename of book text on server
			/*0261*/ int32    buffer1[4];    // Not used, fills out space in the packet so ShowEQ doesn't complain.
		} book;
		struct
		{
			/*0228*/ int32    buffer2[10];     // Not used, fills out space in the packet so ShowEQ doesn't complain.
			/*0268*/ uint8	  BagType;			//Bag type (obviously)
			/*0269*/ uint8    BagSlots;        // number of slots in container
			/*0270*/ int8     IsBagOpen;     // 1 if bag is open, 0 if not.
			/*0271*/ int8     BagSize;    // Maximum size item container can hold
			/*0272*/ uint8    BagWR; // % weight reduction of container
			/*0273*/ uint32   buffer3;     // Not used, fills out space in the packet so ShowEQ doesn't complain.
		} container;
	};
	/*0277*/ uint8    EffectLevel2;            // Casting level
	/*0278*/ int8     Charges;         // Number of charges (-1 = unlimited)
	/*0279*/ int8     EffectType2;      // 0=combat, 1=click anywhere w/o class check, 2=latent/worn, 3=click anywhere EXPENDABLE, 4=click worn, 5=click anywhere w/ class check, -1=no effect
	/*0280*/ uint16   Effect2;         // spellId of special effect
	/*0282*/ int8     unknown0282; //FF
	/*0283*/ int8     unknown0283; //FF
	/*0284*/ uint8    unknown0284[4]; // ***Placeholder 0288
	/*0288*/ float    SellRate;
	/*0292*/ uint32   CastTime;        // Cast time of clicky item in miliseconds
	/*0296*/ uint8    unknown0296[16]; // ***Placeholder
	/*0312*/ uint16   SkillModType;
	/*0314*/ int16    SkillModValue;
	/*0316*/ int16    BaneDmgRace;
	/*0318*/ int16    BaneDmgBody;
	/*0320*/ uint8    BaneDmgAmt;
	/*0321*/ uint8    unknown0321[3]; //title_flag
	/*0324*/ uint8    RecLevel;         // max should be 65
	/*0325*/ uint8    RecSkill;         // Max should be 252
	/*0326*/ uint16   ProcRate; 
	/*0328*/ uint8    ElemDmgType; 
	/*0329*/ uint8    ElemDmgAmt;
	/*0330*/ uint16   FactionMod1;
	/*0332*/ uint16	  FactionMod2;
	/*0334*/ uint16   FactionMod3;
	/*0336*/ uint16	  FactionMod4;
	/*0338*/ uint16   FactionAmt1;
	/*0340*/ uint16	  FactionAmt2;
	/*0342*/ uint16   FactionAmt3;
	/*0344*/ uint16	  FactionAmt4;
	/*0346*/ uint16	  Void346;
	/*0348*/ uint16	  Deity;
	/*0350*/ uint16	  unknown350;
	/*0352*/ uint16   ReqLevel; // Required level
	/*0354*/ uint16   BardType;
	/*0356*/ uint16	  BardValue;
	/*0358*/ uint16   FocusEffect;  //Confirmed
	/*0360*/	
};

struct PlayerItemsPacket_Struct
{
	/*000*/	int16		opcode;		// OP_ItemTradeIn
	/*002*/	struct Item_Struct	item;
};

struct PlayerItems_Struct 
{
	/*000*/	int16		count;
	/*002*/	struct PlayerItemsPacket_Struct	packets[0];
};

struct MerchantItemsPacket_Struct 
{
	/*000*/	uint16 itemtype;
	/*002*/	struct Item_Struct	item;
};

struct TradeItemsPacket_Struct
{
	/*000*/	uint16 fromid;
	/*002*/	uint16 slotid;
	/*004*/	uint8  unknown;
	/*005*/	struct Item_Struct	item;
	/*000*/	uint8 unknown1[5];
	/*000*/	
};

struct PickPocket_Struct 
{
// Size 18
    uint16 to;
    uint16 from;
    uint16 myskill;
    uint16 type; // -1 you are being picked, 0 failed , 1 = plat, 2 = gold, 3 = silver, 4 = copper, 5 = item
    uint32 coin;
    uint8 data[6];
};

struct PickPocketItemPacket_Struct 
{
    uint16 to;
    uint16 from;
    uint16 myskill;
    uint16 type; // -1 you are being picked, 0 failed , 1 = plat, 2 = gold, 3 = silver, 4 = copper, 5 = item
    uint32 coin;
	struct Item_Struct	item;
};

struct MerchantItems_Struct
{
	/*000*/	int16		count;	
	/*002*/	struct MerchantItemsPacket_Struct packets[0];
};

struct Consume_Struct
{
	/*000*/ uint32 slot;
	/*004*/ uint32 auto_consumed; // 0xffffffff when auto eating e7030000 when right click
	/*008*/ uint8 c_unknown1[4];
	/*012*/ uint8 type; // 0x01=Food 0x02=Water
	/*013*/ uint8 unknown13[3];
	/*016*/	
};

struct MoveItem_Struct
{
	/*000*/ uint32 from_slot; 
	/*004*/ uint32 to_slot;
	/*008*/ uint32 number_in_stack;
	/*012*/	
};

struct LootingItem_Struct 
{
	/*000*/	uint16	lootee;	
	/*002*/	uint16	looter;	
	/*004*/	uint16	slot_id;
	/*006*/	uint8	unknown3[2];
	/*008*/	uint32	auto_loot;	
	/*012*/	
};

struct RequestClientZoneChange_Struct 
{
	/*000*/	uint32	zone_id;
	/*004*/	float	y;
	/*008*/	float	x;
	/*012*/	float	z;
	/*016*/	float	heading;
	/*020*/	uint32	type;	//unknown... values
	/*024*/	
};

struct CombatAbility_Struct
{
	/*000*/	uint16 m_target;		//the ID of the target mob
	/*002*/	uint16 unknown;
	/*004*/	uint32 m_atk;
	/*008*/	uint32 m_skill;
	/*012*/	
};

struct CancelTrade_Struct 
{ 
	/*000*/	uint16 fromid;
	/*002*/	uint16 action;
	/*004*/	
};

// EverQuest Time Information:
// 72 minutes per EQ Day
// 3 minutes per EQ Hour
// 6 seconds per EQ Tick (2 minutes EQ Time)
// 3 seconds per EQ Minute
struct TimeOfDay_Struct 
{
	/*000*/	uint8	hour;			// Comment: 
	/*001*/	uint8	minute;			// Comment: 
	/*002*/	uint8	day;			// Comment: 
	/*003*/	uint8	month;			// Comment: 
	/*004*/	uint16	year;			// Comment: Kibanu - Changed from long to uint16 on 7/30/2009
	/*006*/	
};

struct Merchant_Click_Struct 
{
	/*000*/ uint16	npcid;			// Merchant NPC's entity id
	/*002*/ uint16	playerid;
	/*004*/	uint8  command;
	/*005*/ uint8	unknown[3];
	/*008*/ float   rate;
	/*012*/	
};
	
struct Merchant_Sell_Struct
{
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/	uint16	playerid;		// Player's entity id
	/*004*/	uint16	itemslot;
	/*006*/	uint8	IsSold;		// Already sold
	/*007*/	uint8	unknown001;
	/*008*/	uint8	quantity;	// Qty - when used in Merchant_Purchase_Struct
	/*009*/	uint8	unknown004[3];
	/*012*/	uint32	price;
	/*016*/	
};

struct Merchant_Purchase_Struct
{
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/ uint16  playerid;
	/*004*/	uint16	itemslot;		// Player's entity id
	/*006*/ uint16  price;
	/*008*/	uint8	quantity;
	/*009*/ uint8   unknown_void[7];
	/*016*/	
};

struct Merchant_DelItem_Struct
{
	/*000*/	uint16	npcid;			// Merchant NPC's entity id
	/*002*/	uint16	playerid;		// Player's entity id
	/*004*/	uint8	itemslot;       // Slot of the item you want to remove
	/*005*/	uint8	type;     // 0x40
	/*006*/	
};

struct Illusion_Struct
{
	/*000*/	int16	spawnid;
	/*002*/	int16	race;
	/*004*/	int8	gender;
	/*005*/ int8	texture;
	/*006*/ int8	helmtexture;
	/*007*/	int8	unknown007; //Always seems to be 0xFF
	/*008*/ int16	face;
	/*010*/ int8	hairstyle;
	/*011*/	int8	haircolor; 
	/*012*/	int8	beard;
	/*013*/	int8	beardcolor;
	/*014*/	int16	size;		  //Client has height (int) listed for this, but it doesn't line up with what ShowEQ caught at all.
	/*016*/ int32	unknown_void; // Always 0xFFFFFFFF it seems.
	/*020*/	
};

struct GroupInvite_Struct 
{
	/*000*/	char invitee_name[64];		// Comment: 
	/*064*/	char inviter_name[64];		// Comment: 
	/*128*/	char unknown[65];		// Comment: 
	/*193*/	
};

struct FaceChange_Struct 
{
	/*000*/	uint8	haircolor;	// Comment: 
	/*001*/	uint8	beardcolor;	// Comment: 
	/*002*/	uint8	eyecolor1;	// Comment: the eyecolors always seem to be the same, maybe left and right eye?
	/*003*/	uint8	eyecolor2;	// Comment: 
	/*004*/	uint8	hairstyle;	// Comment: 
	/*005*/	uint8	beard;		// Comment: Face Overlay? (barbarian only)
	/*006*/	uint8	face;		// Comment: and beard
	/*007*/	
};

struct TradeRequest_Struct
{
	/*000*/	uint16 to_mob_id;	
	/*002*/	uint16 from_mob_id;	
	/*004*/	
};

struct TradeCoin_Struct
{
	/*000*/	uint16	trader;	
	/*002*/	uint16	slot;	
	/*004*/	uint32	amount;	
	/*008*/	
};

struct Trader_Struct 
{
	/*000*/	uint16	Code;
	/*002*/ uint16  TraderID;
	/*004*/	uint32	Items[80];
	/*324*/	uint32	ItemCost[80];
	/*644*/
};

struct Trader_ShowItems_Struct
{
	/*000*/	uint16 Code;
	/*002*/	uint16 TraderID;
	/*004*/ uint32 SubAction;
	/*008*/	uint32 Items[3];
	/*012*/	
};

struct TraderStatus_Struct
{
	/*000*/	uint16 Code;
	/*002*/	uint16 Unkown04;
	/*004*/	uint32 TraderID;
	/*008*/	uint32 Unkown08[3];
	/*012*/	
};

struct BecomeTrader_Struct
{
	/*000*/	uint16 ID;
	/*002*/ uint16 unknown;
	/*004*/	uint32 Code;
	/*008*/	
};

struct BazaarWindowStart_Struct
{
	/*000*/	uint8   Action;
	/*001*/	uint8   Unknown001;
	/*002*/	
};

struct BazaarWelcome_Struct 
{
	/*000*/	uint32  Action;
	/*004*/	uint32	Traders;
	/*008*/	uint32	Items;
	/*012*/	uint8	Unknown012[8];
	/*020*/	
};

struct BazaarSearch_Struct
{
	/*000*/	BazaarWindowStart_Struct Beginning;
	/*002*/	uint16	TraderID;
	/*004*/	uint16	Class_;
	/*006*/	uint16	Race;
	/*008*/	uint16	ItemStat;
	/*010*/	uint16	Slot;
	/*012*/	uint16	Type;
	/*014*/	char	Name[64];
	/*078*/ uint16  unknown;
	/*080*/	uint32	MinPrice;
	/*084*/	uint32	MaxPrice;
	/*088*/
};

struct TraderBuy_Struct
{
	/*000*/	uint16 Action;
	/*002*/	uint16 TraderID;
	/*004*/	uint32 ItemID;
	/*008*/	uint32 Price;
	/*012*/	uint16 Quantity;
	/*014*/ uint16 Slot; //EQEmu has as AlreadySold for for EQMac this is slot.
	/*016*/	char   ItemName[64];
	/*080*/
};

struct TraderPriceUpdate_Struct
{
	/*000*/	uint16	Action;
	/*002*/	uint16	SubAction;
	/*004*/	int32	SerialNumber;
	/*008*/	uint32	NewPrice;
	/*012*/	
};

// Size: 32 Bytes
struct Combine_Struct 
{ 
	/*000*/	uint8 worldobjecttype;	// Comment: if its a world object like forge, id will be here
	/*000*/	uint8 unknown001;		// Comment: 
	/*000*/	uint8 success;		// Comment: 
	/*000*/	uint8 unknown003;
	/*000*/	uint16 container_slot;	// Comment: the position of the container, or 1000 if its a world container	
	/*000*/	uint16 iteminslot[10];	// Comment: IDs of items in container
	/*000*/	uint16 unknown005;		// Comment: 
	/*000*/	uint16 unknown006;		// Comment: 
	/*000*/	uint16 containerID;		// Comment: ID of container item
};

struct SetDataRate_Struct 
{
	/*000*/	float newdatarate;	// Comment: 
	/*004*/	
};

// This is where the Text is sent to the client.
// Use ` as a newline character in the text.
// Variable length.
struct BookText_Struct
{
	/*000*/	uint8 type;		//type: 0=scroll, 1=book, 2=item info.. prolly others.
	/*001*/	char booktext[1]; // Variable Length
};

// This is the request to read a book.
// This is just a "text file" on the server
// or in our case, the 'name' column in our books table.
struct BookRequest_Struct
{
	/*000*/	uint8 type;		//type: 0=scroll, 1=book, 2=item info.. prolly others.
	/*001*/	char txtfile[1]; // Variable
};

struct GMTrainEnd_Struct 
{
	/*000*/ int16 npcid;
	/*002*/ int16 playerid;
	/*004*/	
};

struct GuildMOTD_Struct{
	/*000*/	char	name[64];
	/*064*/	uint32	unknown64;
	/*068*/	char	motd[512];
	/*580*/	
};

struct GuildInviteAccept_Struct
{
	/*000*/	char inviter[64];
	/*064*/	char newmember[64];
	/*128*/	uint32 response;
	/*132*/	uint16 guildeqid;
	/*134*/	uint16 unknown;
	/*136*/	
};

struct ClickDoor_Struct 
{
	/*000*/	uint8	doorid;
	/*001*/	uint8	unknown[3];
	/*002*/	uint16	item_id;
	/*004*/	uint16	player_id;
	/*008*/	
};

// Added this struct for eqemu and started eimplimentation ProcessOP_SendLoginInfo
//TODO: confirm everything in this struct
struct LoginInfo_Struct 
{
	/*000*/	char	AccountName[127];
	/*127*/	char	Password[24];
	/*151*/ uint8	unknown189[41];		
	/*192*/ uint8   zoning;
	/*193*/ uint8   unknown193[7];
	/*200*/
};

struct EnterWorld_Struct
{
	/*000*/	char	charname[64];
};

struct Arrow_Struct 
{
	/*000*/ uint32 type;			//Always 1
	/*004*/ uint32 unknown004;		//Always 0		
	/*008*/ uint32 unknown008;				
	/*012*/ float src_y;					
	/*016*/ float src_x;					
	/*020*/ float src_z;				
	/*024*/ float launch_angle;		//heading		
	/*028*/ float tilt;
	/*032*/ float velocity;
	/*036*/ float burstVelocity;		
	/*040*/ float burstHorizontal;		
	/*044*/ float burstVertical;		
	/*048*/ float yaw;			
	/*052*/ float pitch;	
	/*056*/ float arc;			
	/*060*/ uint8 unknown060[4];		
	/*064*/	uint16	source_id;
	/*066*/ uint16	target_id;	
	/*068*/ uint16  unknown068;
	/*070*/ uint16  unknown070;
	/*072*/ uint32	object_id; //Spell or ItemID
	/*076*/ uint8  light;
	/*077*/ uint8  unknown077;
	/*078*/ uint8  behavior;
	/*079*/ uint8  effect_type; //9 for spell, uses itemtype for items. 28 is also valid, possibly underwater attack?
	/*080*/ uint8  skill;
	/*081*/ char   model_name[16];
	/*097*/ char   buffer[15];
};

// struct sent by client when using /discp
struct UseDiscipline_Struct
{
	/*0001*/ uint8 discipline;			   // Comment: The discipline executed
	/*0001*/ uint8 unknown[3];			   // Comment: Seems to be always 0 
};

struct EntityId_Struct
{
	/*000*/	int16 entity_id;
	/*002*/
};

struct ApproveWorld_Struct
{
	/*000*/uint8 response;
	/*001*/
};

struct ExpansionInfo_Struct
{
	/*000*/uint32 Expansions;
	/*004*/
};

struct OldSpellBuff_Struct
{
	/*000*/uint8  visable;		// Comment: 0 = Buff not visible, 1 = Visible and permanent buff(Confirmed ) , 2 = Visible and timer on(Confirmed ) 
	/*001*/uint8  level;			// Comment: Level of person who casted buff
	/*002*/uint8  bard_modifier;	// Comment: this seems to be the bard modifier, it is normally 0x0A because we set in in the CastOn_Struct when its not a bard, else its the instrument mod
	/*003*/uint8  activated;	// Comment: ***Placeholder
	/*004*/uint16 spellid;		// Comment: Unknown -> needs confirming -> ID of spell?
	/*006*/uint32 duration;		// Comment: Unknown -> needs confirming -> Duration in ticks
	/*010*/
};

// Length: 10
struct OldItemProperties_Struct
{

	/*000*/	uint8	unknown01[2];
	/*002*/	int8	charges;				// Comment: signed int because unlimited charges are -1
	/*003*/	uint8	unknown02[7];
	/*010*/
};

struct OldBindStruct 
{
	/*000*/ float x;
	/*004*/ float y;
	/*008*/ float z;
	/*012*/
};

/*
	*Used in PlayerProfile
	*/
struct AA_Array
{
	uint8 AA;
	uint8 value;
};

static const uint32 MAX_PP_MEMSPELL		= 8;
static const uint32  MAX_PP_AA_ARRAY		= 120;
static const uint32 MAX_PP_SKILL		= 74; // _SkillPacketArraySize;	// 100 - actual skills buffer size
struct PlayerProfile_Struct
{
	#define pp_inventory_size 30
	#define pp_containerinv_size 80
	#define pp_cursorbaginventory_size 10
	#define pp_bank_inv_size 8
	/* ***************** */
	/*0000*/	uint32  checksum;		    // Checksum
	/*0004*/	uint8	unknown0004[2];		// ***Placeholder
	/*0006*/	char	name[64];			// Player First Name
	/*0070*/	char	Surname[66];		// Surname OR title.
	/*0136*/	uint32	uniqueGuildID;
	/*0140*/	uint8	gender;				// Player Gender
	/*0141*/	char	genderchar[1];		// ***Placeholder
	/*0142*/	uint16	race;				// Player Race (Lyenu: Changed to an int16, since races can be over 255)
	/*0144*/	uint16	class_;				// Player Class
	/*0146*/	uint16	bodytype;
	/*0148*/	uint8	level;				// Player Level
	/*0149*/	char	levelchar[3];		// ***Placeholder
	/*0152*/	uint32	exp;				// Current Experience
	/*0156*/	uint16	points;				// Players Points
	/*0158*/	int16	mana;				// Player Mana
	/*0160*/	int16	cur_hp;				// Player Health
	/*0162*/	uint16	status;				
	/*0164*/	uint16	STR;				// Player Strength
	/*0166*/	uint16	STA;				// Player Stamina
	/*0168*/	uint16	CHA;				// Player Charisma
	/*0170*/	uint16	DEX;				// Player Dexterity
	/*0172*/	uint16	INT;				// Player Intelligence
	/*0174*/	uint16	AGI;				// Player Agility
	/*0176*/	uint16	WIS;				// Player Wisdom
	/*0178*/	uint8	oldface;               //
	/*0179*/    int8    EquipType[9];       // i think its the visible parts of the body armor
	/*0188*/    int32   EquipColor[9];      //
	/*0224*/	int16	inventory[pp_inventory_size];		// Player Inventory Item Numbers
	/*0284*/	uint8	languages[26];		// Player Languages
	/*0310*/	uint8	unknown0310[6];		// ***Placeholder
	/*0316*/	struct	OldItemProperties_Struct	invItemProperties[pp_inventory_size];	// These correlate with inventory[30]
	/*0616*/	struct	OldSpellBuff_Struct	buffs[15];	// Player Buffs Currently On
	/*0766*/	int16	containerinv[pp_containerinv_size];	
	/*0926*/	int16   cursorbaginventory[pp_cursorbaginventory_size]; // If a bag is in slot 0, this is where the bag's items are
	/*0946*/	struct	OldItemProperties_Struct	bagItemProperties[pp_containerinv_size];	// Just like InvItemProperties
	/*1746*/    struct  OldItemProperties_Struct	cursorItemProperties[pp_cursorbaginventory_size];	  //just like invitemprops[]
	/*1846*/	int16	spell_book[256];	// Player spells scribed in their book
	/*2358*/	uint8	unknown2374[512];	// 0xFF
	/*2870*/	int16	mem_spells[8];	// Player spells memorized
	/*2886*/	uint8	unknown2886[16];	// 0xFF
	/*2902*/	uint16	available_slots;
	/*2904*/	float	y;					// Player Y
	/*2908*/	float	x;					// Player X
	/*2912*/	float	z;					// Player Z
	/*2916*/	float	heading;			// Player Heading
	/*2920*/	uint32	position;		// ***Placeholder
	/*2924*/	uint32	platinum;			// Player Platinum (Character)
	/*2928*/	uint32	gold;				// Player Gold (Character)
	/*2932*/	uint32	silver;				// Player Silver (Character)
	/*2936*/	uint32	copper;				// Player Copper (Character)
	/*2940*/	uint32	platinum_bank;		// Player Platinum (Bank)
	/*2944*/	uint32	gold_bank;			// Player Gold (Bank)
	/*2948*/	uint32	silver_bank;		// Player Silver (Bank)
	/*2952*/	uint32	copper_bank;		// Player Copper (Bank)
	/*2956*/	uint32	platinum_cursor;
	/*2960*/	uint32	gold_cursor;
	/*2964*/	uint32	silver_cursor;
	/*2968*/	uint32	copper_cursor;
	/*2972*/	uint8	currency[16];	    //Unused currency?
	/*2988*/	uint16	skills[74];			// Player Skills
	/*3136*/	uint16	innate[23];
	/*3182*/	uint16	innate_unknowns[4];	//Always 255.
	/*3190*/	uint16	innate_monk_ranger;	//Monk and Ranger are 0.
	/*3192*/	uint16	innate_ogre;		//Ogre is 0.
	/*3194*/	uint16	innate_unknown;		//Always 255.
	/*3196*/	uint16	innate_druid;		//Only Druids are 0.
	/*3198*/	uint16	innate_sk;			//Only SKs have this set as 0 in pp
	/*3200*/	uint16	innate_all;			//Everybody seems to have 0 here.
	/*3202*/	uint16	innate_paladin;		//Only Paladins have this as 0.
	/*3204*/	uint16	innate_[16];
	/*3236*/	uint8	unknown_skillvoid;			//255
	/*3237*/    uint16  air_supply;
	/*3239*/    uint8   texture;
	/*3240*/	float   height;
	/*3244*/	float	width;
	/*3248*/	float   length;
	/*3252*/	float   view_height;
	/*3256*/    char    boat[32];
	/*3280*/    uint8   unknown[60];
	/*3348*/	uint8	autosplit;
	/*3349*/	uint8	unknown3449[43];
	/*3392*/	uint8	expansions;			//Effects features such as /disc, AA, raid
	/*3393*/	uint8	unknown3393[51];
	/*3444*/	uint32	current_zone;		// 
	/*3448*/	uint8	unknown3448[336];	// Lots of data on fake PP struct, none in normal decoded packet.
	/*3784*/	uint32	bind_point_zone[5];	
	/*3804*/	float	bind_y[5];
	/*3824*/	float	bind_x[5];
	/*3844*/	float	bind_z[5];
	/*3864*/	float	bind_heading[5];
	/*3884*/	OldItemProperties_Struct	bankinvitemproperties[pp_bank_inv_size];
	/*3964*/	OldItemProperties_Struct	bankbagitemproperties[pp_containerinv_size];
	/*4764*/	uint32	login_time;
	/*4768*/	int16	bank_inv[pp_bank_inv_size];		// Player Bank Inventory Item Numbers
	/*4784*/	int16	bank_cont_inv[pp_containerinv_size];	// Player Bank Inventory Item Numbers (Bags)
	/*4944*/	uint16	deity;		// ***Placeholder
	/*4946*/	uint16	guild_id;			// Player Guild ID Number
	/*4948*/	uint32  birthday;
	/*4952*/	uint32  lastlogin;
	/*4956*/	uint32  timePlayedMin;
	/*4960*/	int8    thirst_level;
	/*4961*/    int8    hunger_level;
	/*4962*/	uint8   fatigue;
	/*4963*/	uint8	pvp;				// Player PVP Flag
	/*4964*/	uint8	level2;		// ***Placeholder
	/*4965*/	uint8	anon;				// Player Anon. Flag
	/*4966*/	uint8	gm;					// Player GM Flag
	/*4967*/	uint8	guildrank;			// Player Guild Rank (0=member, 1=officer, 2=leader)
	/*4968*/    uint8   intoxication;
	/*4969*/	uint8	eqbackground;
	/*4970*/	uint8	unknown4760[2];
	/*4972*/	uint32	spellSlotRefresh[8];
	/*5004*/	uint32	unknown5003;
	/*5008*/	uint32	abilitySlotRefresh;
	/*5012*/	char	groupMembers[6][64];	// Group Members
	/*5396*/	uint8	unknown5396[20];	
	/*5416*/	uint32	groupdat;
	/*5420*/	uint32	expAA;				// Not working? client has this as Post60Exp
	/*5424*/    uint8	title;
	/*5425*/	uint8	perAA;			    // Player AA Percent
	/*5426*/	uint8	haircolor;			// Player Hair Color
	/*5427*/	uint8	beardcolor;			// Player Beard Color
	/*5428*/	uint8	eyecolor1;			// Player Left Eye Color
	/*5429*/	uint8	eyecolor2;			// Player Right Eye Color
	/*5430*/	uint8	hairstyle;			// Player Hair Style
	/*5431*/	uint8	beard;				// Player Beard Type
	/*5432*/	uint8	face;				// Player Face Type
	/*5433*/	uint32	item_material[_MaterialCount];
	/*5469*/	uint8	unknown5469[143]; //item_tint is in here somewhere.
	/*5612*/	AA_Array aa_array[MAX_PP_AA_ARRAY];
	/*5852*/	uint32	ATR_DIVINE_RES_timer;
	/*5856*/    uint32  ATR_FREE_HOT_timer;
	/*5860*/	uint32	ATR_TARGET_DA_timer;
	/*5864*/	uint32	SptWoodTimer;
	/*5868*/	uint32	DireCharmTimer;
	/*5872*/	uint32	ATR_STRONG_ROOT_timer;
	/*5876*/	uint32	ATR_MASOCHISM_timer;
	/*5880*/	uint32	ATR_MANA_BURN_timer;
	/*5884*/	uint32	ATR_GATHER_MANA_timer;
	/*5888*/	uint32	ATR_PET_LOH_timer;
	/*5892*/	uint32	ExodusTimer;
	/*5896*/	uint32	ATR_MASS_FEAR_timer;
	/*5900*/    uint16  air_remaining;
	/*5902*/    uint16  aapoints;
	/*5904*/	uint32	MGBTimer;
	/*5908*/	uint8   unknown5908[91];
	/*5999*/	int8	mBitFlags[6];
	/*6005*/	uint8	Unknown6004[707];
	/*6712*/	uint32	WrathWildTimer;
	/*6716*/	uint32	UnknownTimer;
	/*6720*/	uint32	HarmTouchTimer;
	/*6724*/	uint8	Unknown6724[1736];
	/*8460*/
};

/*
** CharCreate
** Length: 8452 Bytes
*/
struct CharCreate_Struct
{
	/*0000*/	uint8	unknown0004[136];	
	/*0136*/	uint8	gender;				// Player Gender
	/*0137*/	char	unknown137[1];		
	/*0138*/	uint16	race;				// Player Race
	/*0140*/	uint16	class_;				// Player Class
	/*0142*/	uint8	unknown0142[18];
	/*0160*/	uint16	STR;				// Player Strength
	/*0162*/	uint16	STA;				// Player Stamina
	/*0164*/	uint16	CHA;				// Player Charisma
	/*0166*/	uint16	DEX;				// Player Dexterity
	/*0168*/	uint16	INT;				// Player Intelligence
	/*0170*/	uint16	AGI;				// Player Agility
	/*0172*/	uint16	WIS;				// Player Wisdom
	/*0174*/	uint8	oldface;            
	/*0175*/	uint8	unknown0175[3265];
	/*3440*/	uint32	start_zone;	
	/*3444*/	uint8	unknown3444[1496];
	/*4940*/	uint16	deity;
	/*4942*/	uint8	unknown4946[480];
	/*5422*/	uint8	haircolor;			// Player Hair Color
	/*5423*/	uint8	beardcolor;			// Player Beard Color
	/*5424*/	uint8	eyecolor1;			// Player Left Eye Color
	/*5425*/	uint8	eyecolor2;			// Player Right Eye Color
	/*5426*/	uint8	hairstyle;			// Player Hair Style
	/*5427*/	uint8	beard;				// Player Beard Type
	/*5428*/	uint8	face;				// Player Face Type
	/*5429*/	uint8	unknown5429[3023];
	/*8452*/
};

struct AltAdvStats_Struct 
{
	/*000*/	int32 experience;
	/*004*/	int16 unspent;
	/*006*/	int8	percentage;
	/*007*/	int8	unknown_void;
	/*008*/	
};

//Server sends this packet for reuse timers
//Client sends this packet as 256 bytes, and is our equivlent of AA_Action
struct UseAA_Struct
{
	/*000*/ int32 begin;
	/*004*/ int16 ability; // skill_id of a purchased AA.
	/*006*/ int16  unknown_void; 
	/*008*/ int32 end;
	/*012*/

};

struct EnvDamage2_Struct 
{
	/*000*/	int16 id;
	/*002*/int16 unknown;
	/*004*/	int8 dmgtype; //FA = Lava; FC = Falling
	/*005*/	int8 unknown2;
	/*006*/	int16 constant; //Always FFFF
	/*008*/	int16 damage;
	/*010*/	int8 unknown3[14]; //A bunch of 00's...
	/*024*/
};

struct	ItemViewRequest_Struct
{
	/*000*/int16	item_id;
	/*002*/char	item_name[64];
	/*066*/
};

/* _MAC_NET_MSG_reward_MacMsg, OP_Sound, Size: 48 */
struct QuestReward_Struct
{
	/*000*/	uint16	mob_id; 
	/*002*/	uint16	target_id;
	/*006*/	uint32	exp_reward;
	/*010*/	uint32	faction;
	/*014*/	uint32	faction_mod;
	/*018*/	uint32	copper;
	/*022*/	uint32	silver;
	/*024*/	uint32	gold;
	/*028*/	uint32	platinum;
	/*032*/	uint16	item_id;
	/*034*/	uint8	unknown[14];
	/*048*/	
};

/*Not Implemented*/

//           No idea what this is used for, but it creates a
//           perminent object that no client may interact with.
//			 It also accepts spell sprites (i.e., GENC00), but 
//			 they do not currently display. I guess we could use 
//			 this for GM events?
//
//Opcode: 0xF640 MSG_ADD_OBJECT
struct ObjectDisplayOnly_Struct
{
	/*0000*/ char test1[32];
	/*0032*/ char modelName[16];	
	/*0048*/ char test2[12];
	/*0060*/ float size;			
	/*0064*/ float y;				
	/*0068*/ float x;				
	/*0072*/ float z;				
	/*0076*/ float heading;			
	/*0080*/ float tilt;
	/*0084*/ char test4[40];
};

	};	//end namespace structs
};	//end namespace MAC

#endif /*MAC_STRUCTS_H_*/
