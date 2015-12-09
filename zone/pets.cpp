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

#include "../common/global_define.h"
#include "../common/spdat.h"
#include "../common/string_util.h"
#include "../common/types.h"

#include "entity.h"
#include "client.h"
#include "mob.h"

#include "pets.h"
#include "zonedb.h"

#ifndef WIN32
#include <stdlib.h>
#include "../common/unix.h"
#endif


const char *GetRandPetName()
{
	static const char *petnames[] = { "Gabaner","Gabann","Gabantik","Gabarab","Gabarer","Gabarn","Gabartik",
		"Gabekab","Gabeker","Gabekn","Gaber","Gabn","Gabobab","Gabobn","Gabtik",
		"Ganer","Gann","Gantik","Garab","Garaner","Garann","Garantik","Gararn",
		"Garekn","Garer","Garn","Gartik","Gasaner","Gasann","Gasantik","Gasarer",
		"Gasartik","Gasekn","Gaser","Gebann","Gebantik","Gebarer","Gebarn","Gebartik",
		"Gebeker","Gebekn","Gebn","Gekab","Geker","Gekn","Genaner","Genann","Genantik",
		"Genarer","Genarn","Gener","Genn","Genobtik","Gibaner","Gibann","Gibantik",
		"Gibarn","Gibartik","Gibekn","Giber","Gibn","Gibobtik","Gibtik","Gobaber",
		"Gobaner","Gobann","Gobarn","Gobartik","Gober","Gobn","Gobober","Gobobn",
		"Gobobtik","Gobtik","Gonaner","Gonann","Gonantik","Gonarab","Gonarer",
		"Gonarn","Gonartik","Gonekab","Gonekn","Goner","Gonobtik","Gontik","Gotik",
		"Jabaner","Jabann","Jabantik","Jabarab","Jabarer","Jabarn","Jabartik",
		"Jabekab","Jabeker","Jabekn","Jaber","Jabn","Jabobtik","Jabtik","Janab",
		"Janer","Jann","Jantik","Jarab","Jaranab","Jaraner","Jararer","Jararn",
		"Jarartik","Jareker","Jarekn","Jarer","Jarn","Jarobn","Jarobtik","Jartik",
		"Jasab","Jasaner","Jasantik","Jasarer","Jasartik","Jasekab","Jaseker",
		"Jasekn","Jaser","Jasn","Jasobab","Jasober","Jastik","Jebanab","Jebann",
		"Jebantik","Jebarab","Jebarar","Jebarer","Jebarn","Jebartik","Jebeker",
		"Jebekn","Jeber","Jebobn","Jebtik","Jekab","Jeker","Jekn","Jenann",
		"Jenantik","Jenarer","Jeneker","Jenekn","Jentik","Jibaner","Jibann",
		"Jibantik","Jibarer","Jibarn","Jibartik","Jibeker","Jibn","Jibobn",
		"Jibtik","Jobab","Jobaner","Jobann","Jobantik","Jobarn","Jobartik",
		"Jobekab","Jobeker","Jober","Jobn","Jobtik","Jonanab","Jonaner",
		"Jonann","Jonantik","Jonarer","Jonarn","Jonartik","Jonekab","Joneker",
		"Jonekn","Joner","Jonn","Jonnarn","Jonober","Jonobn","Jonobtik","Jontik",
		"Kabanab","Kabaner","Kabann","Kabantik","Kabarer","Kabarn","Kabartik",
		"Kabeker","Kabekn","Kaber","Kabn","Kabober","Kabobn","Kabobtik","Kabtik",
		"Kanab","Kaner","Kann","Kantik","Karab","Karanab","Karaner","Karann",
		"Karantik","Kararer","Karartik","Kareker","Karer","Karn","Karobab","Karobn",
		"Kartik","Kasaner","Kasann","Kasarer","Kasartik","Kaseker","Kasekn","Kaser",
		"Kasn","Kasober","Kastik","Kebann","Kebantik","Kebarab","Kebartik","Kebeker",
		"Kebekn","Kebn","Kebobab","Kebtik","Kekab","Keker","Kekn","Kenab","Kenaner",
		"Kenantik","Kenarer","Kenarn","Keneker","Kener","Kenn","Kenobn","Kenobtik",
		"Kentik","Kibab","Kibaner","Kibantik","Kibarn","Kibartik","Kibekab","Kibeker",
		"Kibekn","Kibn","Kibobn","Kibobtik","Kobab","Kobanab","Kobaner","Kobann",
		"Kobantik","Kobarer","Kobarn","Kobartik","Kobeker","Kobekn","Kober","Kobn",
		"Kobober","Kobobn","Kobtik","Konanab","Konaner","Konann","Konantik","Konarab",
		"Konarer","Konarn","Konekab","Koneker","Konekn","Koner","Konn","Konobn",
		"Konobtik","Kontik","Labanab","Labaner","Labann","Labarab","Labarer",
		"Labarn","Labartik","Labeker","Labekn","Laner","Lann","Larab","Larantik",
		"Lararer","Lararn","Larartik","Lareker","Larer","Larn","Lartik","Lasaner",
		"Lasann","Lasarer","Laseker","Laser","Lasik","Lasn","Lastik","Lebaner",
		"Lebarer","Lebartik","Lebekn","Lebtik","Lekab","Lekn","Lenanab","Lenaner",
		"Lenann","Lenartik","Lenekab","Leneker","Lenekn","Lentik","Libab","Libaner",
		"Libann","Libantik","Libarer","Libarn","Libartik","Libeker","Libekn","Lobann",
		"Lobarab","Lobarn","Lobartik","Lobekn","Lobn","Lobober","Lobobn","Lobtik",
		"Lonaner","Lonann","Lonantik","Lonarab","Lonarer","Lonarn","Lonartik","Lonekn",
		"Loner","Lonobtik","Lontik","Vabanab","Vabaner","Vabann","Vabantik","Vabarer",
		"Vabarn","Vabartik","Vabeker","Vabekn","Vabtik","Vanikk","Vann","Varartik","Varn",
		"Vartik","Vasann","Vasantik","Vasarab","Vasarer","Vaseker","Vebaner","Vebantik",
		"Vebarab","Vebeker","Vebekn","Vebobn","Vekab","Veker","Venaner","Venantik","Venar",
		"Venarn","Vener","Ventik","Vibann","Vibantik","Viber","Vibobtik","Vobann",
		"Vobarer","Vobartik","Vobekn","Vober","Vobn","Vobtik","Vonaner","Vonann",
		"Vonantik","Vonarab","Vonarn","Vonartik","Voneker","Vonn","Xabanab","Xabaner",
		"Xabarer","Xabarn","Xabartik","Xabekab","Xabeker","Xabekn","Xaber","Xabober",
		"Xaner","Xann","Xarab","Xaranab","Xarann","Xarantik","Xararer","Xarartik","Xarer",
		"Xarn","Xartik","Xasaner","Xasann","Xasarab","Xasarn","Xasekab","Xaseker",
		"Xebarer","Xebarn","Xebeker","Xeber","Xebober","Xebtik","Xekab","Xeker",
		"Xekn","Xenann","Xenantik","Xenarer","Xenartik","Xenekn","Xener","Xenober",
		"Xentik","Xibantik","Xibarer","Xibekab","Xibeker","Xibobab","Xibober","Xibobn",
		"Xobaner","Xobann","Xobarab","Xobarn","Xobekab","Xobeker","Xobekn","Xober",
		"Xobn","Xobobn","Xobtik","Xonaner","Xonann","Xonantik","Xonarer","Xonartik",
		"Xonekab","Xoneker","Xonekn","Xoner","Xonober","Xtik","Zabaner","Zabantik",
		"Zabarab","Zabekab","Zabekn","Zaber","Zabn","Zabobab","Zabober","Zabtik",
		"Zaner","Zantik","Zarann","Zarantik","Zararn","Zarartik","Zareker","Zarekn",
		"Zarer","Zarn","Zarober","Zartik","Zasaner","Zasarer","Zaseker","Zasekn","Zasn",
		"Zebantik","Zebarer","Zebarn","Zebartik","Zebobab","Zekab","Zekn","Zenann",
		"Zenantik","Zenarer","Zenarn","Zenekab","Zeneker","Zenobtik","Zibanab","Zibaner",
		"Zibann","Zibarer","Zibartik","Zibekn","Zibn","Zibobn","Zobaner","Zobann",
		"Zobarn","Zober","Zobn","Zonanab","Zonaner","Zonann","Zonantik","Zonarer",
		"Zonartik","Zonobn","Zonobtik","Zontik","Ztik" };
	int r = zone->random.Int(0, (sizeof(petnames)/sizeof(const char *))-1);
	printf("Pet being created: %s\n",petnames[r]); // DO NOT COMMENT THIS OUT!
	return petnames[r];
}

const FocusPetItem Pet::focusItems[FocusPetItemSize] = {
	// Symbol of Ancient Summoning
	{20508, 25, 60, 40, FocusPetType::ALL},
	// Dark Gloves of Summoning
	{28144, 20, 60, 40, FocusPetType::ALL},
	// Encyclopedia Necrotheurgia
	{11571, 10, 48, 40, FocusPetType::NECRO},
	// Staff of Elemental Mastery: Water
	{11569, 10, 48, 40, FocusPetType::WATER},
	// Staff of Elemental Mastery: Earth
	{11567, 10, 48, 40, FocusPetType::EARTH},
	// Staff of Elemental Mastery: Fire
	{11566, 10, 48, 40, FocusPetType::FIRE},
	// Staff of Elemental Mastery: Air
	{11568, 10, 48, 40, FocusPetType::AIR},
	// Broom of Trilon
	{6360, 5, 48, 4, FocusPetType::AIR},
	// Shovel of Ponz
	{6361, 5, 48, 4, FocusPetType::EARTH},
	// Torch of Alna
	{6362, 5, 48, 4, FocusPetType::FIRE},
	// Stein of Ulissa
	{6363, 5, 48, 4, FocusPetType::WATER},
};

FocusPetType Pet::GetPetItemPetTypeFromSpellId(uint16 spell_id) {
	static const int firePets[]  = {626, 630, 634, 316, 399, 403, 395, 498, 571, 575, 622, 1673, 1677, 3322};
	static const int airPets[]   = {627, 631, 635, 317, 396, 400, 404, 499, 572, 576, 623, 1674, 1678, 3371};
	static const int earthPets[] = {624, 628, 632, 58, 397, 401, 335, 496, 569, 573, 620, 1675, 1671, 3324};
	static const int waterPets[] = {625, 629, 633, 315, 398, 403, 336, 497, 570, 574, 621, 1676, 1672, 3320};

	for(int i=0; i < sizeof(firePets)/ sizeof(firePets[0]); i++) {
		if((int)spell_id == firePets[i]) {
			return FocusPetType::FIRE;
		}
	}
	for(int i=0; i < sizeof(airPets)/ sizeof(airPets[0]); i++) {
		if((int)spell_id == airPets[i]) {
			return FocusPetType::AIR;
		}
	}
	for(int i=0; i < sizeof(earthPets)/ sizeof(earthPets[0]); i++) {
		if((int)spell_id == earthPets[i]) {
			return FocusPetType::EARTH;
		}
	}
	for(int i=0; i < sizeof(waterPets)/ sizeof(waterPets[0]); i++) {
		if((int)spell_id == waterPets[i]) {
			return FocusPetType::WATER;
		}
	}
	return FocusPetType::NONE;
}

void Mob::MakePet(uint16 spell_id, const char* pettype, const char *petname) {
	// petpower of -1 is used to get the petpower based on whichever focus is currently
	// equipped. This should replicate the old functionality for the most part.
	MakePoweredPet(spell_id, pettype, -1, petname);
}

// Split from the basic MakePet to allow backward compatiblity with existing code while also
// making it possible for petpower to be retained without the focus item having to
// stay equipped when the character zones. petpower of -1 means that the currently equipped petfocus
// of a client is searched for and used instead.
void Mob::MakePoweredPet(uint16 spell_id, const char* pettype, int16 petpower,
		const char *petname, float in_size) {
	// Sanity and early out checking first.
	bool scale_pet = false;
	if(HasPet() || pettype == nullptr)
		return;

	//lookup our pets table record for this type
	PetRecord record;
	if(!database.GetPoweredPetEntry(pettype, petpower, &record)) {
		Message(CC_Red, "Unable to find data for pet %s", pettype);
		Log.Out(Logs::General, Logs::Error, "Unable to find data for pet %s, check pets table.", pettype);
		return;
	}

	//find the NPC data for the specified NPC type
	const NPCType *base = database.GetNPCType(record.npc_type);
	if(base == nullptr) {
		Message(CC_Red, "Unable to load NPC data for pet %s", pettype);
		Log.Out(Logs::General, Logs::Error, "Unable to load NPC data for pet %s (NPC ID %d), check pets and npc_types tables.", pettype, record.npc_type);
		return;
	}

	int act_power = 0; // The actual pet power we'll use.
	if (petpower == -1) {
		if (this->IsClient()) {
			//Message(CC_Red, "We are a client time to check for focus items");
			uint16 focusItemId;
			FocusPetItem petItem;
			// Loop over all the focus items and figure out which on is the best to use
			// It will step down from PoP - Classic looking for the best focus item to use based on pet level
			int16 slot = 0;
			for(int i=0; i < FocusPetItemSize; i++) {
				petItem = Pet::focusItems[i];
				// Look in out inventory
				int16 slot_id = this->CastToClient()->GetInv().HasItem(petItem.item_id, 1, invWhereWorn);
				if(slot_id != INVALID_INDEX) {
					//skip this focus item if its effect is out of rage for the pet we are casting
					if(base->level >= petItem.min_level && base->level <= petItem.max_level) {
						if(EQDEBUG>8) Message(CC_Red, "Found Focus Item: %d in Inventory: %d", petItem.item_id, slot_id);
						if(EQDEBUG>8) Message(CC_Red, "Npc spell levels: %d (%d - %d)", base->level, petItem.min_level, petItem.max_level);
						slot = slot_id;
						focusItemId = petItem.item_id;
						break;
					} else {
						if(EQDEBUG>8) Message(CC_Red, "Moving on Pet base level is out of range: %d (%d - %d)", base->level, petItem.min_level, petItem.max_level);
					}
				}
			}
			// we have a focus item
			if(focusItemId)
			{
				FocusPetType focusType;
				// Symbol or Gloves can be used by all NEC, MAG, BST
				if(petItem.pet_type == FocusPetType::ALL)
				{
					//Message(CC_Red, "Type is ALL");
					focusType = FocusPetType::ALL;
				} else {
					// make sure we can use the focus item as the class .. client should never let us fail this but for sanity!
					if (GetClass() == MAGICIAN) {
						if(EQDEBUG>8) Message(CC_Red, "Looking up mage");
						if(EQDEBUG>8) Message(CC_Red, "Looking up if spell: %d is allowed ot be focused", spell_id);
						focusType = Pet::GetPetItemPetTypeFromSpellId(spell_id);
						if(EQDEBUG>8) Message(CC_Red, "FocusType fround %i", focusType);
					} else if (GetClass() == NECROMANCER) {
						if(EQDEBUG>8) Message(CC_Red, "We are a necro");
						focusType = FocusPetType::NECRO;
					}
				}
				// Sets the power to be what the focus item has as a mod
				if(EQDEBUG>8) {
					Message(CC_Red, "Pet Item Type ALL is  %i", FocusPetType::ALL);
					Message(CC_Red, "Pet Item Type FIRE is  %i", FocusPetType::FIRE);
					Message(CC_Red, "Pet Item Type WATER is  %i", FocusPetType::WATER);
					Message(CC_Red, "Pet Item Type AIR is  %i", FocusPetType::AIR);
					Message(CC_Red, "Pet Item Type EARTH is  %i", FocusPetType::EARTH);
					Message(CC_Red, "Pet Item Type NECRO is  %i", FocusPetType::NECRO);
				}
				if(EQDEBUG>8) Message(CC_Red, "Pet Item Type  %i", petItem.pet_type);
				if (focusType == petItem.pet_type) {
					if(EQDEBUG>8) Message(CC_Red, "Setting power to: %d", petItem.power);
					act_power = petItem.power;
					scale_pet = true;
				}
				
				if(act_power > 0)
				{
					if(EQDEBUG>8) Message(CC_Yellow, "Debug: You have cast a powered pet. Unadjusted pet power is: %i. HasItem returned: %i.", act_power, slot);

					if(focusType == FocusPetType::NECRO ||focusType == FocusPetType::EARTH || focusType == FocusPetType::AIR
					|| focusType == FocusPetType::FIRE || focusType == FocusPetType::WATER)
					{
						if(act_power > 10)
							act_power = 10;
					}
					else
					{
						if(act_power > 25)
							act_power = 25;
					}
				}
			}
		}
	}
	else if (petpower > 0) {
		act_power = petpower;
		scale_pet = true;
	}

	if(EQDEBUG>8) Message(CC_Red, "Power is: %d", act_power);
	// optional rule: classic style variance in pets. Achieve this by
	// adding a random 0-4 to pet power, since it only comes in increments
	// of five from focus effects.

	//we copy the npc_type data because we need to edit it a bit
	NPCType *npc_type = new NPCType;
	memcpy(npc_type, base, sizeof(NPCType));

	// If pet power is set to -1 in the DB, use stat scaling
	if (this->IsClient() && record.petpower == -1)
	{
		if(scale_pet) {
			float scale_power = (float)act_power / 100.0f;
			if(scale_power > 0)
			{
				npc_type->max_hp  = (int16) (npc_type->max_hp * (1 + scale_power));
				npc_type->cur_hp  = npc_type->max_hp;
				npc_type->AC	  = (int16) (npc_type->AC * (1 + scale_power));
				npc_type->level  += (int16) 1 + ((int16)act_power / 25); // gains an additional level for every 25 pet power
				npc_type->min_dmg = (int16) (npc_type->min_dmg * (1 + (scale_power / 2)));
				npc_type->max_dmg = (int16) (npc_type->max_dmg * (1 + (scale_power / 2)));
				npc_type->size	  = (npc_type->size * (1 + scale_power));
			}
			record.petpower = act_power;
		}
	}

	//Live AA - Elemental Durability
	int16 MaxHP = aabonuses.PetMaxHP + itembonuses.PetMaxHP + spellbonuses.PetMaxHP;

	if (MaxHP){
		npc_type->max_hp += (npc_type->max_hp*MaxHP)/100;
		npc_type->cur_hp = npc_type->max_hp;
	}

	//TODO: think about regen (engaged vs. not engaged)

	// Pet naming:
	// 0 - `s pet
	// 1 - `s familiar
	// 2 - `s Warder
	// 3 - Random name if client, `s pet for others
	// 4 - Keep DB name


	if (petname != nullptr) {
		// Name was provided, use it.
		strn0cpy(npc_type->name, petname, 64);
	} else if (record.petnaming == 0) {
		strcpy(npc_type->name, this->GetCleanName());
		npc_type->name[25] = '\0';
		strcat(npc_type->name, "`s_pet");
	} else if (record.petnaming == 1) {
		strcpy(npc_type->name, this->GetName());
		npc_type->name[19] = '\0';
		strcat(npc_type->name, "`s_familiar");
	} else if (record.petnaming == 2) {
		strcpy(npc_type->name, this->GetName());
		npc_type->name[21] = 0;
		strcat(npc_type->name, "`s_warder");
	} else if (record.petnaming == 4) {
		// Keep the DB name
	} else if (record.petnaming == 3 && IsClient()) {
		strcpy(npc_type->name, GetRandPetName());
	} else {
		strcpy(npc_type->name, this->GetCleanName());
		npc_type->name[25] = '\0';
		strcat(npc_type->name, "`s_pet");
	}

	//handle beastlord pet appearance
	if(record.petnaming == 2)
	{
		switch(GetBaseRace())
		{
		case VAHSHIR:
			npc_type->race = TIGER;
			npc_type->size *= 0.8f;
			break;
		case TROLL:
			npc_type->race = ALLIGATOR;
			npc_type->size *= 2.5f;
			break;
		case OGRE:
			npc_type->race = BEAR;
			npc_type->texture = 3;
			npc_type->gender = 2;
			break;
		case BARBARIAN:
			npc_type->race = WOLF;
			npc_type->texture = 2;
			break;
		case IKSAR:
			npc_type->race = WOLF;
			npc_type->texture = 0;
			npc_type->gender = 1;
			npc_type->luclinface = 0;
			break;
		default:
			npc_type->race = WOLF;
			npc_type->texture = 0;
		}
	}

	// handle monster summoning pet appearance
	if(record.monsterflag) {

		uint32 monsterid = 0;

		// get a random npc id from the spawngroups assigned to this zone
		auto query = StringFormat("SELECT npcID "
									"FROM (spawnentry INNER JOIN spawn2 ON spawn2.spawngroupID = spawnentry.spawngroupID) "
									"INNER JOIN npc_types ON npc_types.id = spawnentry.npcID "
									"WHERE spawn2.zone = '%s' AND npc_types.bodytype NOT IN (11, 33, 66, 67) "
									"AND npc_types.race NOT IN (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 44, "
									"55, 67, 71, 72, 73, 77, 78, 81, 90, 92, 93, 94, 106, 112, 114, 127, 128, "
									"130, 139, 141, 183, 236, 237, 238, 239, 254, 266, 329, 330, 378, 379, "
									"380, 381, 382, 383, 404, 522) "
									"ORDER BY RAND() LIMIT 1", zone->GetShortName());
		auto results = database.QueryDatabase(query);
		if (!results.Success()) {
			// if the database query failed
		}

		if (results.RowCount() != 0) {
			auto row = results.begin();
			monsterid = atoi(row[0]);
		}

		// since we don't have any monsters, just make it look like an earth pet for now
		if (monsterid == 0)
			monsterid = 567;

		// give the summoned pet the attributes of the monster we found
		const NPCType* monster = database.GetNPCType(monsterid);
		if(monster) {
			npc_type->race = monster->race;
			npc_type->size = monster->size;
			npc_type->texture = monster->texture;
			npc_type->gender = monster->gender;
			npc_type->luclinface = monster->luclinface;
			npc_type->helmtexture = monster->helmtexture;
		} else
			Log.Out(Logs::General, Logs::Error, "Error loading NPC data for monster summoning pet (NPC ID %d)", monsterid);

	}

	//this takes ownership of the npc_type data
	Pet *npc = new Pet(npc_type, this, (PetType)record.petcontrol, spell_id, record.petpower);

	// Now that we have an actual object to interact with, load
	// the base items for the pet. These are always loaded
	// so that a rank 1 suspend minion does not kill things
	// like the special back items some focused pets may receive.
	uint32 petinv[EmuConstants::EQUIPMENT_SIZE];
	memset(petinv, 0, sizeof(petinv));
	const Item_Struct *item = 0;

	if (database.GetBasePetItems(record.equipmentset, petinv)) {
		for (int i = 0; i<EmuConstants::EQUIPMENT_SIZE; i++)
			if (petinv[i]) {
				item = database.GetItem(petinv[i]);
				npc->AddLootDrop(item, &npc->itemlist, 0, 1, 127, true, true);
			}
	}

	npc->UpdateEquipmentLight();

	// finally, override size if one was provided
	if (in_size > 0.0f)
		npc->size = in_size;

	entity_list.AddNPC(npc, true, true);
	SetPetID(npc->GetID());
	// We need to handle PetType 5 (petHatelist), add the current target to the hatelist of the pet
}
/* This is why the pets ghost - pets were being spawned too far away from its npc owner and some
into walls or objects (+10), this sometimes creates the "ghost" effect. I changed to +2 (as close as I
could get while it still looked good). I also noticed this can happen if an NPC is spawned on the same spot of another or in a related bad spot.*/
Pet::Pet(NPCType *type_data, Mob *owner, PetType type, uint16 spell_id, int16 power)
: NPC(type_data, 0, owner->GetPosition() + glm::vec4(2.0f, 2.0f, 0.0f, 0.0f), FlyMode3)
{
	GiveNPCTypeData(type_data);
	typeofpet = type;
	petpower = power;
	SetOwnerID(owner->GetID());
	SetPetSpellID(spell_id);
	summonerid = owner->GetID();
	taunting = true;

	// Class should use npc constructor to set light properties
}

bool ZoneDatabase::GetPetEntry(const char *pet_type, PetRecord *into) {
	return GetPoweredPetEntry(pet_type, 0, into);
}

bool ZoneDatabase::GetPoweredPetEntry(const char *pet_type, int16 petpower, PetRecord *into) {
	std::string query;

	if (petpower <= 0)
		query = StringFormat("SELECT npcID, temp, petpower, petcontrol, petnaming, monsterflag, equipmentset "
							"FROM pets WHERE type='%s' AND petpower<=0", pet_type);
	else
		query = StringFormat("SELECT npcID, temp, petpower, petcontrol, petnaming, monsterflag, equipmentset "
                            "FROM pets WHERE type='%s' AND petpower<=%d ORDER BY petpower DESC LIMIT 1",
                            pet_type, petpower);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

	if (results.RowCount() != 1)
		return false;

	auto row = results.begin();

	into->npc_type = atoi(row[0]);
	into->temporary = atoi(row[1]);
	into->petpower = atoi(row[2]);
	into->petcontrol = atoi(row[3]);
	into->petnaming = atoi(row[4]);
	into->monsterflag = atoi(row[5]);
	into->equipmentset = atoi(row[6]);

	return true;
}

Mob* Mob::GetPet() {
	if(GetPetID() == 0)
		return(nullptr);

	Mob* tmp = entity_list.GetMob(GetPetID());
	if(tmp == nullptr) {
		SetPetID(0);
		return(nullptr);
	}

	if(tmp->GetOwnerID() != GetID()) {
		SetPetID(0);
		return(nullptr);
	}

	return(tmp);
}

void Mob::SetPet(Mob* newpet) {
	Mob* oldpet = GetPet();
	if (oldpet && IsClient()) {
		oldpet->SetOwnerID(0);
	}
	if (newpet == nullptr) {
		SetPetID(0);
	} else {
		SetPetID(newpet->GetID());
		Mob* oldowner = entity_list.GetMob(newpet->GetOwnerID());
		if (oldowner && oldowner->IsClient())
			oldowner->SetPetID(0);
		newpet->SetOwnerID(this->GetID());
	}
}

void Mob::DepopPet()
{
	if (HasPet())
	{
		Mob* mypet = GetPet();
		SetPet(nullptr);
		if (!mypet->IsCharmed())
			mypet->CastToNPC()->Depop();
	}

	// kill summoned pet even if charmed
	uint16 petID = entity_list.GetSummonedPetID(this);
	if (petID)
	{
		Mob* pet = entity_list.GetMobID(petID);

		if (pet)
			pet->SetOwnerID(0);
	}
}

void Mob::FadePetCharmBuff()
{
	if ((this->GetPetType() == petCharmed))
	{
		Mob* formerpet = this->GetPet();

		if (formerpet) {
			formerpet->BuffFadeByEffect(SE_Charm);
		}
	}
}

void Mob::SetPetID(uint16 NewPetID) {
	if (NewPetID == GetID() && NewPetID != 0)
		return;
	petid = NewPetID;
}

void NPC::GetPetState(SpellBuff_Struct *pet_buffs, uint32 *items, char *name) {
	//save the pet name
	strn0cpy(name, GetName(), 64);

	//save their items, we only care about what they are actually wearing
	memcpy(items, equipment, sizeof(uint32)*EmuConstants::EQUIPMENT_SIZE);

	//save their buffs.
	for (int i=0; i < GetPetMaxTotalSlots(); i++) {
		if (buffs[i].spellid != SPELL_UNKNOWN) {
			pet_buffs[i].spellid = buffs[i].spellid;
			pet_buffs[i].slotid = i+1;
			pet_buffs[i].duration = buffs[i].ticsremaining;
			pet_buffs[i].level = buffs[i].casterlevel;
			pet_buffs[i].effect = 10;
			pet_buffs[i].counters = buffs[i].counters;
		}
		else {
			pet_buffs[i].spellid = SPELL_UNKNOWN;
			pet_buffs[i].duration = 0;
			pet_buffs[i].level = 0;
			pet_buffs[i].effect = 0;
			pet_buffs[i].counters = 0;
		}
	}
}

void NPC::SetPetState(SpellBuff_Struct *pet_buffs, uint32 *items) {
	//restore their buffs...

	int i;
	for (i = 0; i < GetPetMaxTotalSlots(); i++) {
		for(int z = 0; z < GetPetMaxTotalSlots(); z++) {
		// check for duplicates
			if(buffs[z].spellid != SPELL_UNKNOWN && buffs[z].spellid == pet_buffs[i].spellid) {
				buffs[z].spellid = SPELL_UNKNOWN;
				pet_buffs[i].spellid = 0xFFFFFFFF;
			}
		}

		if (pet_buffs[i].spellid <= (uint32)SPDAT_RECORDS && pet_buffs[i].spellid != 0 && pet_buffs[i].duration > 0) {
			if(pet_buffs[i].level == 0 || pet_buffs[i].level > 100)
				pet_buffs[i].level = 1;
			buffs[i].spellid			= pet_buffs[i].spellid;
			buffs[i].ticsremaining		= pet_buffs[i].duration;
			buffs[i].casterlevel		= pet_buffs[i].level;
			buffs[i].casterid			= 0;
			buffs[i].counters			= pet_buffs[i].counters;
			buffs[i].numhits			= spells[pet_buffs[i].spellid].numhits;
		}
		else {
			buffs[i].spellid = SPELL_UNKNOWN;
			pet_buffs[i].spellid = 0xFFFFFFFF;
			pet_buffs[i].slotid = 0;
			pet_buffs[i].level = 0;
			pet_buffs[i].duration = 0;
			pet_buffs[i].effect = 0;
		}
	}
	for (int j1=0; j1 < GetPetMaxTotalSlots(); j1++) {
		if (buffs[j1].spellid <= (uint32)SPDAT_RECORDS) {
			for (int x1=0; x1 < EFFECT_COUNT; x1++) {
				switch (spells[buffs[j1].spellid].effectid[x1]) {
					case SE_WeaponProc:
						// We need to reapply buff based procs
						// We need to do this here so suspended pets also regain their procs.
						if (spells[buffs[j1].spellid].base2[x1] == 0) {
							AddProcToWeapon(GetProcID(buffs[j1].spellid,x1), false, 100, buffs[j1].spellid);
						} else {
							AddProcToWeapon(GetProcID(buffs[j1].spellid,x1), false, 100+spells[buffs[j1].spellid].base2[x1], buffs[j1].spellid);
						}
						break;
					case SE_Charm:
					case SE_Rune:
					case SE_Illusion:
						buffs[j1].spellid = SPELL_UNKNOWN;
						pet_buffs[j1].spellid = SPELLBOOK_UNKNOWN;
						pet_buffs[j1].slotid = 0;
						pet_buffs[j1].level = 0;
						pet_buffs[j1].duration = 0;
						pet_buffs[j1].effect = 0;
						x1 = EFFECT_COUNT;
						break;
					// We can't send appearance packets yet, put down at CompleteConnect
				}
			}
		}
	}

	//restore their equipment...
	for (i = 0; i < EmuConstants::EQUIPMENT_SIZE; i++) {
		if(items[i] == 0)
			continue;

		const Item_Struct* item2 = database.GetItem(items[i]);
		if (item2 && item2->NoDrop != 0) {
			//dont bother saving item charges for now, NPCs never use them
			//and nobody should be able to get them off the corpse..?
			AddLootDrop(item2, &itemlist, 0, 1, 127, true, true);
		}
	}
}

// Load the equipmentset from the DB. Might be worthwhile to load these into
// shared memory at some point due to the number of queries needed to load a
// nested set.
bool ZoneDatabase::GetBasePetItems(int32 equipmentset, uint32 *items) {
	if (equipmentset < 0 || items == nullptr)
		return false;

	// Equipment sets can be nested. We start with the top-most one and
	// add all items in it to the items array. Referenced equipmentsets
	// are loaded after that, up to a max depth of 5. (Arbitrary limit
	// so we don't go into an endless loop if the DB data is cyclic for
	// some reason.)
	// A slot will only get an item put in it if it is empty. That way
	// an equipmentset can overload a slot for the set(s) it includes.

	int depth = 0;
	int32 curset = equipmentset;
	int32 nextset = -1;
	uint32 slot;

	// outline:
	// get equipmentset from DB. (Mainly check if we exist and get the
	// nested ID)
	// query pets_equipmentset_entries with the set_id and loop over
	// all of the result rows. Check if we have something in the slot
	// already. If no, add the item id to the equipment array.
	while (curset >= 0 && depth < 5) {
		std::string  query = StringFormat("SELECT nested_set FROM pets_equipmentset WHERE set_id = '%s'", curset);
		auto results = QueryDatabase(query);
		if (!results.Success()) {
			return false;
		}

        if (results.RowCount() != 1) {
            // invalid set reference, it doesn't exist
            Log.Out(Logs::General, Logs::Error, "Error in GetBasePetItems equipment set '%d' does not exist", curset);
            return false;
        }

		auto row = results.begin();
		nextset = atoi(row[0]);

        query = StringFormat("SELECT slot, item_id FROM pets_equipmentset_entries WHERE set_id='%s'", curset);
        results = QueryDatabase(query);
        if (results.Success()){
            for (row = results.begin(); row != results.end(); ++row)
            {
                slot = atoi(row[0]);

				if (slot >= EmuConstants::EQUIPMENT_SIZE)
					continue;

				if (items[slot] == 0)
					items[slot] = atoi(row[1]);
			}
		}

		curset = nextset;
		depth++;
	}

	return true;
}

