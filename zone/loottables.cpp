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

#include "../common/global_define.h"
#include "../common/loottable.h"
#include "../common/misc_functions.h"
#include "../common/data_verification.h"

#include "client.h"
#include "entity.h"
#include "mob.h"
#include "npc.h"
#include "zonedb.h"

#include <iostream>
#include <stdlib.h>

#ifdef _WINDOWS
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif


// Queries the loottable: adds item & coin to the npc
void ZoneDatabase::AddLootTableToNPC(NPC* npc, uint32 loottable_id, ItemList* itemlist, uint32* copper, uint32* silver, uint32* gold, uint32* plat) {
	const LootTable_Struct* lts = nullptr;
	*copper = 0;
	*silver = 0;
	*gold = 0;
	*plat = 0;

	lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	uint32 min_cash = lts->mincash;
	uint32 max_cash = lts->maxcash;
	if (min_cash > max_cash) {
		uint32 t = min_cash;
		min_cash = max_cash;
		max_cash = t;
	}

	uint32 cash = 0;
	if (max_cash > 0 && lts->avgcoin > 0 && EQEmu::ValueWithin(lts->avgcoin, min_cash, max_cash)) {
		float upper_chance = (float)(lts->avgcoin - min_cash) / (float)(max_cash - min_cash);
		float avg_cash_roll = (float)zone->random.Real(0.0, 1.0);

		if (avg_cash_roll < upper_chance) {
			cash = zone->random.Int(lts->avgcoin, max_cash);
		}
		else {
			cash = zone->random.Int(min_cash, lts->avgcoin);
		}
	}
	else {
		cash = zone->random.Int(min_cash, max_cash);
	}

	if (cash != 0) {
		*plat = cash / 1000;
		cash -= *plat * 1000;

		*gold = cash / 100;
		cash -= *gold * 100;

		*silver = cash / 10;
		cash -= *silver * 10;

		*copper = cash;
	}

	// Do items
	for (uint32 i = 0; i<lts->NumEntries; i++) {
		uint8 multiplier_count = 0;
		for (uint32 k = 1; k <= lts->Entries[i].multiplier; k++) {
			uint8 droplimit = lts->Entries[i].droplimit;
			uint8 mindrop = lts->Entries[i].mindrop;
			uint8 multiplier_min = lts->Entries[i].multiplier_min;

			//LootTable Entry probability
			float ltchance = 0.0f;
			ltchance = lts->Entries[i].probability;

			float drop_chance = 0.0f;
			if (ltchance > 0.0 && ltchance < 100.0 && multiplier_count >= multiplier_min) {
				drop_chance = (float)zone->random.Real(0.0, 100.0);
			}
			else if (multiplier_count < multiplier_min)
			{
				drop_chance = 0.0f;
			}

			if (ltchance != 0.0 && (ltchance == 100.0 || drop_chance <= ltchance)) {
				AddLootDropToNPC(npc, lts->Entries[i].lootdrop_id, itemlist, droplimit, mindrop);
			}

			++multiplier_count;
		}
	}
}

// Called by AddLootTableToNPC
// maxdrops = size of the array npcd
void ZoneDatabase::AddLootDropToNPC(NPC* npc, uint32 lootdrop_id, ItemList* itemlist, uint8 droplimit, uint8 mindrop) {
	const LootDrop_Struct* lds = GetLootDrop(lootdrop_id);
	if (!lds) {
		return;
	}

	if (lds->NumEntries == 0)
		return;

	if (droplimit == 0 && mindrop == 0) {
		for (uint32 i = 0; i < lds->NumEntries; ++i) {
			int multiplier = lds->Entries[i].multiplier;
			for (int j = 0; j < multiplier; ++j) {
				if (zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance) {
					uint32 itemid = lds->Entries[i].item_id;
					int8 charges = lds->Entries[i].item_charges;
					const Item_Struct* db_item = GetItem(itemid);
					if (database.ItemQuantityType(itemid) == Quantity_Charges)
					{
						if (charges <= 1)
							charges = db_item->MaxCharges;
					}
					npc->AddLootDrop(db_item, itemlist, charges, lds->Entries[i].minlevel,
						lds->Entries[i].maxlevel, lds->Entries[i].equip_item > 0 ? true : false, false);
				}
			}
		}
		return;
	}

	if (lds->NumEntries > 100 && droplimit == 0) {
		droplimit = 10;
	}

	if (droplimit < mindrop) {
		droplimit = mindrop;
	}

	float roll_t = 0.0f;
	float roll_t_min = 0.0f;
	bool active_item_list = false;
	for (uint32 i = 0; i < lds->NumEntries; ++i) {
		uint32 itemid = lds->Entries[i].item_id;
		int8 charges = lds->Entries[i].item_charges;
		const Item_Struct* db_item = GetItem(itemid);
		if (database.ItemQuantityType(itemid) == Quantity_Charges)
		{
			if (charges <= 1)
				charges = db_item->MaxCharges;
		}
		if (db_item) {
			roll_t += lds->Entries[i].chance;
			active_item_list = true;
		}
	}

	roll_t_min = roll_t;
	roll_t = EQEmu::ClampLower(roll_t, 100.0f);

	if (!active_item_list) {
		return;
	}

	for(int i = 0; i < mindrop; ++i) {
		float roll = (float)zone->random.Real(0.0, roll_t_min);
		for(uint32 j = 0; j < lds->NumEntries; ++j) {
			uint32 itemid = lds->Entries[j].item_id;
			int8 charges = lds->Entries[j].item_charges;
			const Item_Struct* db_item = GetItem(itemid);
			if (database.ItemQuantityType(itemid) == Quantity_Charges)
			{
				if (charges <= 1)
					charges = db_item->MaxCharges;
			}
			if (db_item) {
				if(roll < lds->Entries[j].chance) {
					npc->AddLootDrop(db_item, itemlist, charges, lds->Entries[j].minlevel,
									 lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false);

					int multiplier = (int)lds->Entries[i].multiplier;
					multiplier = EQEmu::ClampLower(multiplier, 1);

					for (int k = 1; k < multiplier; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if(c_roll <= lds->Entries[i].chance) {
							npc->AddLootDrop(db_item, itemlist, charges, lds->Entries[j].minlevel,
											 lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false);
						}
					}

					j = lds->NumEntries;
					break;
				}
				else {
					roll -= lds->Entries[j].chance;
				}
			}
		}
	}

	for(int i = mindrop; i < droplimit; ++i) {
		float roll = (float)zone->random.Real(0.0, roll_t);
		for (uint32 j = 0; j < lds->NumEntries; ++j) {
			uint32 itemid = lds->Entries[j].item_id;
			int8 charges = lds->Entries[j].item_charges;
			const Item_Struct* db_item = GetItem(itemid);
			if (database.ItemQuantityType(itemid) == Quantity_Charges)
			{
				if (charges <= 1)
					charges = db_item->MaxCharges;
			}
			if (db_item) {
				if (roll < lds->Entries[j].chance) {
					npc->AddLootDrop(db_item, itemlist, charges, lds->Entries[j].minlevel,
						lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false);

					int multiplier = (int)lds->Entries[i].multiplier;
					multiplier = EQEmu::ClampLower(multiplier, 1);

					for (int k = 1; k < multiplier; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if (c_roll <= lds->Entries[i].chance) {
							npc->AddLootDrop(db_item, itemlist, charges, lds->Entries[j].minlevel,
								lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false);
						}
					}

					j = lds->NumEntries;
					break;
				}
				else {
					roll -= lds->Entries[j].chance;
				}
			}
		}
	} // We either ran out of items or reached our limit.

	npc->UpdateEquipmentLight();
	// no wearchange associated with this function..so, this should not be needed
	//if (npc->UpdateActiveLightValue())
	//	npc->SendAppearancePacket(AT_Light, npc->GetActiveLightValue());
}

//if itemlist is null, just send wear changes
void NPC::AddLootDrop(const Item_Struct *item2, ItemList* itemlist, int16 charges, uint8 minlevel, uint8 maxlevel, bool equipit, bool wearchange, bool quest, bool pet) {
	if(item2 == nullptr)
		return;

	//make sure we are doing something...
	if(!itemlist && !wearchange)
		return;

	
	if(CountQuestItems() >= 8)
		return;

	ServerLootItem_Struct* item = new ServerLootItem_Struct;

	if(quest)
		Log.Out(Logs::Detail, Logs::Trading, "Adding %s to npc: %s. Wearchange: %d Equipit: %d Multiquest: %d", item2->Name, GetName(), wearchange, equipit, quest);

	EQApplicationPacket* outapp = nullptr;
	WearChange_Struct* wc = nullptr;
	if(wearchange) {
		outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
		wc = (WearChange_Struct*)outapp->pBuffer;
		wc->spawn_id = GetID();
		wc->material=0;
	}

	item->item_id = item2->ID;
	item->charges = charges;
	item->min_level = minlevel;
	item->max_level = maxlevel;
	item->quest = quest;
	item->equip_slot = MainGeneral1; //Set default slot to general inventory. NPCs can have multiple items in the same slot.
	item->pet = pet;

	if(pet && quest)
	{
		Log.Out(Logs::Detail, Logs::Trading, "Error: Item %s is being added to %s as both a pet and a quest.", item2->Name, GetName());
		item->pet = 0;
	}

	if (equipit) {
		uint8 eslot = 0xFF;
		char newid[20];
		const Item_Struct* compitem = nullptr;
		bool found = false; // track if we found an empty slot we fit into
		int32 foundslot = -1; // for multi-slot items
		bool upgrade = false;

		// Equip rules are as follows:
		// If the item has the NoPet flag set it will not be equipped.
		// An empty slot takes priority. The first empty one that an item can
		// fit into will be the one picked for the item.
		// AC is the primary choice for which item gets picked for a slot.
		// If AC is identical HP is considered next.
		// If an item can fit into multiple slots we'll pick the last one where
		// it is an improvement.

		if (!item2->NoPet) 
		{
			for (int i = 0; !found && i<EmuConstants::EQUIPMENT_SIZE; i++) 
			{
				uint32 slots = (1 << i);

				// If the item is ranged, and has other equip slots prefer to use another slot so
				// the weapon graphic shows in-game.
				if(i == MainRange && item2->Slots != slots)
				{
					continue;
				}

				if (item2->Slots & slots) 
				{
					if(equipment[i])
					{
						compitem = database.GetItem(equipment[i]);
						const Item_Struct* pri_item = nullptr;
						if (item->pet == 0 &&
							(i != MainPrimary && i != MainSecondary && (item2->AC > compitem->AC || (item2->AC == compitem->AC && item2->HP > compitem->HP))) ||
							((i == MainPrimary || i == MainSecondary) && item2->Damage > compitem->Damage)
							)
						{
							// item would be an upgrade
							// check if we're multi-slot, if yes then we have to keep
							// looking in case any of the other slots we can fit into are empty.
							if (item2->Slots != slots) 
							{
								foundslot = i;
								upgrade = true;
							}
							else 
							{
								equipment[i] = item2->ID;
								foundslot = i;
								found = true;
								upgrade = true;
							}
						} // end if ac
					}
					else
					{
						equipment[i] = item2->ID;
						foundslot = i;
						found = true;
					}
				} // end if (slots)
			} // end for
		} // end if NoPet

		// Possible slot was found but not selected. Pick it now.
		if (!found && foundslot >= 0) {
			equipment[foundslot] = item2->ID;
			found = true;
		}

		// @merth: IDFile size has been increased, this needs to change
		uint16 emat;
		if(item2->Material <= 0
			|| item2->Slots & (1 << MainPrimary | 1 << MainSecondary)) {
			memset(newid, 0, sizeof(newid));
			for(int i=0;i<7;i++){
				if (!isalpha(item2->IDFile[i])){
					strn0cpy(newid, &item2->IDFile[i],6);
					i=8;
				}
			}

			emat = atoi(newid);
		} else {
			emat = item2->Material;
		}

		if (foundslot == MainPrimary) {
			// This prevents us from equipping a 2H item when a shield or misc item is already in the off-hand.
			if(GetEquipment(MaterialSecondary) == 0 || (item2->ItemType != ItemType2HBlunt && item2->ItemType != ItemType2HPiercing && item2->ItemType != ItemType2HSlash))
			{ 
				if (item2->ItemType == ItemType1HSlash || item2->ItemType == ItemType1HBlunt || item2->ItemType == ItemType1HPiercing)
					can_equip_secondary = true;
				else
					can_equip_secondary = false;

				if (item2->Proc.Effect != 0)
					CastToMob()->AddProcToWeapon(item2->Proc.Effect, true);

				if(upgrade)
				{
					ServerLootItem_Struct* sitem = GetItem(foundslot);
					if(sitem)
						UnEquipItem(sitem->item_id, foundslot);
				}

				eslot = MaterialPrimary;
				item->equip_slot = MainPrimary;
			}
		}
		else if (foundslot == MainSecondary &&
			((item2->ItemType == ItemTypeShield || item2->ItemType == ItemTypeMisc) ||
			(can_equip_secondary && GetLevel() >= DUAL_WIELD_LEVEL &&
			(item2->ItemType == ItemType1HSlash || item2->ItemType == ItemType1HBlunt || item2->ItemType == ItemType1HPiercing))))
		{
			if (item2->Proc.Effect!=0)
				CastToMob()->AddProcToWeapon(item2->Proc.Effect, true);

			can_dual_wield = false;
			if(item2->ItemType == ItemTypeShield)
			{
				ShieldEquiped(true);
			}
			else if(item2->ItemType != ItemTypeMisc)
				can_dual_wield = true;

			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}

			eslot = MaterialSecondary;
			item->equip_slot = MainSecondary;
		}
		else if (foundslot == MainHead) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialHead;
			item->equip_slot = MainHead;
		}
		else if (foundslot == MainChest) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialChest;
			item->equip_slot = MainChest;
		}
		else if (foundslot == MainArms) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialArms;
			item->equip_slot = MainArms;
		}
		else if (foundslot == MainWrist1 || foundslot == MainWrist2) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialWrist;
			if(foundslot == MainWrist1)
				item->equip_slot = MainWrist1;
			if(foundslot == MainWrist2)
				item->equip_slot = MainWrist2;
		}
		else if (foundslot == MainHands) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialHands;
			item->equip_slot = MainHands;
		}
		else if (foundslot == MainLegs) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialLegs;
			item->equip_slot = MainLegs;
		}
		else if (foundslot == MainFeet) 
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			eslot = MaterialFeet;
			item->equip_slot = MainFeet;
		}
		else if (foundslot == MainEar1 || foundslot == MainEar2 || foundslot == MainFace || foundslot ==  MainNeck || foundslot == MainShoulders ||
			foundslot == MainBack || foundslot == MainRange || foundslot == MainFinger1 || foundslot == MainFinger2 || foundslot == MainWaist ||
			foundslot == MainAmmo)
		{
			if(upgrade)
			{
				ServerLootItem_Struct* sitem = GetItem(foundslot);
				if(sitem)
					UnEquipItem(sitem->item_id, foundslot);
			}
			item->equip_slot = foundslot;
		}

		//if we found an open slot it goes in...
		if(eslot != 0xFF) {
			if(wearchange) {
				wc->wear_slot_id = eslot;
				wc->material = emat;
			}

		}
		if (found) {
			CalcBonuses(); // This is less than ideal for bulk adding of items
		}
	}

	if(itemlist != nullptr)
		itemlist->push_back(item);
	else
		safe_delete(item);

	if(wearchange && outapp) {
		entity_list.QueueClients(this, outapp);
		safe_delete(outapp);
	}

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AT_Light, GetActiveLightType());
}

void NPC::AddItem(const Item_Struct* item, uint16 charges, bool equipitem, bool quest) {
	//slot isnt needed, its determined from the item.
	AddLootDrop(item, &itemlist, charges, 1, 127, equipitem, equipitem, quest);
}

void NPC::AddItem(uint32 itemid, uint16 charges, bool equipitem, bool quest) {
	//slot isnt needed, its determined from the item.
	const Item_Struct * i = database.GetItem(itemid);
	if(i == nullptr)
		return;
	AddLootDrop(i, &itemlist, charges, 1, 127, equipitem, equipitem, quest);
}

void NPC::AddLootTable() {
	if (npctype_id != 0) { // check if it's a GM spawn
		database.AddLootTableToNPC(this,loottable_id, &itemlist, &copper, &silver, &gold, &platinum);
	}
}

void NPC::AddLootTable(uint32 ldid) {
	if (npctype_id != 0) { // check if it's a GM spawn
	  database.AddLootTableToNPC(this,ldid, &itemlist, &copper, &silver, &gold, &platinum);
	}
}

bool NPC::GetQuestLootItem(int16 itemid) 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1 && sitem->item_id == itemid) 
		{
			return true;
		}
	}

	return false;
}

bool NPC::QuestLootCount(uint16 itemid1, uint16 itemid2, uint16 itemid3, uint16 itemid4)
{
	if(itemid2 == 0 && itemid3 == 0 && itemid4 == 0)
		return true;

	uint8 item2count = 0, item3count = 0, item4count = 0, item1npc = 0, item2npc = 0, item3npc = 0, item4npc = 0;
	uint8 item1count = 1; 
	if(itemid2 > 0)
		item2count = 1;
	if(itemid3 > 0)
		item3count = 1;
	if(itemid4 > 0)
		item4count = 1;

	if(itemid1 == itemid2 && itemid2 > 0)
	{
		item2count = item1count;
		++item1count;
		++item2count;
	}
	if(itemid1 == itemid3 && itemid3 > 0)
	{
		item3count = item1count;
		++item1count;
		++item3count;
	}
	if(itemid1 == itemid4 && itemid4 > 0)
	{
		item4count = item1count;
		++item1count;
		++item4count;
	}

	if(itemid2 == itemid3  && itemid2 > 0 && itemid3 > 0)
	{
		item3count = item2count;
		++item2count;
		++item3count;
	}
	if(itemid2 == itemid4  && itemid2 > 0 && itemid4 > 0)
	{
		item4count = item2count;
		++item2count;
		++item4count;
	}

	if(itemid3 == itemid4 && itemid3 > 0 && itemid4 > 0)
	{
		item4count = item3count;
		++item3count;
		++item4count;
	}

	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1) 
		{
			if(sitem->item_id == itemid1)
				++item1npc;

			if(sitem->item_id == itemid2 && itemid2 > 0)
				++item2npc;

			if(sitem->item_id == itemid3 && itemid3 > 0)
				++item3npc;

			if(sitem->item_id == itemid4 && itemid4 > 0)
				++item4npc;
		}
	}

	if(item1npc < item1count)
	{
		return false;
	}

	if(itemid2 > 0 && item2npc < item2count)
		return false;

	if(itemid3 > 0 && item3npc < item3count)
		return false;

	if(itemid4 > 0 && item4npc < item4count)
		return false;

	return true;
}

bool NPC::HasQuestLootItem() 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1) 
		{
			return true;
		}
	}

	return false;
}

void NPC::CleanQuestLootItems() 
{
	//Removes nodrop or multiple quest loot items from a NPC before sending the corpse items to the client.

	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for(; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && (sitem->quest == 1 || sitem->pet == 1))
		{
			uint8 count = CountQuestItem(sitem->item_id);
			if(count > 1 && sitem->pet != 1)
			{
				RemoveItem(sitem->item_id);
				return;
			}
			else
			{
				const Item_Struct* item = database.GetItem(sitem->item_id);
				if(item && item->NoDrop == 0)
				{
					RemoveItem(sitem->item_id);
					return;
				}
			}
		}
	}
}

uint8 NPC::CountQuestItem(uint16 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for(; cur != end; ++cur) 
	{
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->item_id == itemid)
		{
			++count;
		}
	}

	return count;
}

uint8 NPC::CountQuestItems()
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for(; cur != end; ++cur) 
	{
		ServerLootItem_Struct* sitem = *cur;
		if(sitem && sitem->quest == 1)
		{
			++count;
		}
	}

	return count;
}

bool NPC::RemoveQuestLootItems(int16 itemid) 
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1) {
			if(itemid == 0)
			{
				RemoveItem(sitem);
				return true;
			}
			else if(itemid == sitem->item_id)
			{
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

void NPC::RemoveItem(ServerLootItem_Struct* item_data)
{
	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter) {
		auto sitem = *iter;
		if (sitem != item_data) { continue; }

		itemlist.erase(iter);

		uint8 material = Inventory::CalcMaterialFromSlot(sitem->equip_slot); // autos to unsigned char
		if (material != _MaterialInvalid)
			SendWearChange(material);

		UpdateEquipmentLight();
		if (UpdateActiveLight())
			SendAppearancePacket(AT_Light, GetActiveLightType());

		safe_delete(sitem);
		return;
	}
}

bool NPC::IsEquipped(int16 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->item_id == itemid && sitem->equip_slot <= MainAmmo) 
		{
			return true;
		}
	}

	return false;
}

bool NPC::UnEquipItem(int16 itemid, uint16 slotid)
{
	bool found = false;
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->item_id == itemid && sitem->equip_slot == slotid) 
		{
			RemoveItem(sitem);
			found = true;
			break;
		}
	}

	if(found)
	{
		ItemInst* inst = database.CreateItem(itemid);
		if(inst)
		{
			const Item_Struct* item = inst->GetItem();
			AddLootDrop(item, &itemlist, 0, 1, 127, false);
		}
	}

	return found;
}