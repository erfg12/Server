/*
 * vim: set noexpandtab tabstop=4 shiftwidth=4 syntax=cpp:
*/
#ifndef PETS_H
#define PETS_H

#define PET_BACKOFF			1
#define PET_GETLOST			2
#define PET_HEALTHREPORT	4
#define PET_GUARDHERE		5
#define PET_GUARDME			6
#define PET_ATTACK			7
#define PET_FOLLOWME		8
#define PET_SITDOWN			9
#define PET_STANDUP			10
#define PET_TAUNT			11
#define PET_HOLD			12
#define PET_NOTAUNT			14
#define PET_LEADER			16
#define	PET_SLUMBER			17
#define	PET_NOCAST			18
#define	PET_FOCUS			19
#define	PET_FOCUS_ON		25
#define	PET_FOCUS_OFF		26
#define PET_HOLD_ON			27
#define PET_HOLD_OFF		28


typedef enum {
	ALL,
	FIRE,
	WATER,
	AIR,
	EARTH,
	NECRO,
	BEAST,
	NONE
} FocusPetType;

struct FocusPetItem {
	int item_id;
	int power;
	int max_level;
	int min_level;
	int pet_type;
};

#define FocusPetItemSize 11

class Mob;
struct NPCType;

class Pet : public NPC {
public:
	Pet(NPCType *type_data, Mob *owner, PetType type, uint16 spell_id, int16 power);
	static const FocusPetItem focusItems[11];
	static FocusPetType GetPetItemPetTypeFromSpellId(uint16 spell_id);
};

#endif
