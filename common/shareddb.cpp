#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include "shareddb.h"
#include "mysql.h"
#include "item.h"
#include "classes.h"
#include "rulesys.h"
#include "seperator.h"
#include "string_util.h"
#include "eq_packet_structs.h"
#include "guilds.h"
#include "extprofile.h"
#include "memory_mapped_file.h"
#include "ipc_mutex.h"
#include "eqemu_exception.h"
#include "loottable.h"
#include "faction.h"
#include "features.h"

#pragma warning( disable : 4305 4309 4244 4800 )

SharedDatabase::SharedDatabase()
: Database()
{
}

SharedDatabase::SharedDatabase(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
: Database(host, user, passwd, database, port)
{
}

SharedDatabase::~SharedDatabase() {
}

bool SharedDatabase::SetHideMe(uint32 account_id, uint8 hideme)
{
	std::string query = StringFormat("UPDATE account SET hideme = %i WHERE id = %i", hideme, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

uint8 SharedDatabase::GetGMSpeed(uint32 account_id)
{
	std::string query = StringFormat("SELECT gmspeed FROM account WHERE id = '%i'", account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return 0;
	}

    if (results.RowCount() != 1)
        return 0;

    auto row = results.begin();

	return atoi(row[0]);
}

bool SharedDatabase::SetGMSpeed(uint32 account_id, uint8 gmspeed)
{
	std::string query = StringFormat("UPDATE account SET gmspeed = %i WHERE id = %i", gmspeed, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::SetGMInvul(uint32 account_id, bool gminvul)
{
	std::string query = StringFormat("UPDATE account SET gminvul = %i WHERE id = %i", gminvul, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::SetGMFlymode(uint32 account_id, uint8 flymode)
{
	std::string query = StringFormat("UPDATE account SET flymode = %i WHERE id = %i", flymode, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

bool SharedDatabase::SetGMIgnoreTells(uint32 account_id, uint8 ignoretells)
{
	std::string query = StringFormat("UPDATE account SET ignore_tells = %i WHERE id = %i", ignoretells, account_id);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

	return true;
}

uint32 SharedDatabase::GetTotalTimeEntitledOnAccount(uint32 AccountID) {
	uint32 EntitledTime = 0;
	std::string query = StringFormat("SELECT `time_played` FROM `character_data` WHERE `account_id` = %u", AccountID);
	auto results = QueryDatabase(query);
	for (auto row = results.begin(); row != results.end(); ++row) {
		EntitledTime += atoi(row[0]);
	}
	return EntitledTime;
}

bool SharedDatabase::SaveCursor(uint32 char_id, std::list<ItemInst*>::const_iterator &start, std::list<ItemInst*>::const_iterator &end)
{
	// Delete cursor items
	std::string query = StringFormat("DELETE FROM character_inventory WHERE id = %i "
                                    "AND ((slotid >= %i AND slotid <= %i) "
                                    "OR slotid = %i OR (slotid >= %i AND slotid <= %i) )",
                                    char_id, EmuConstants::CURSOR_QUEUE_BEGIN, EmuConstants::CURSOR_QUEUE_END, 
									MainCursor, EmuConstants::CURSOR_BAG_BEGIN, EmuConstants::CURSOR_BAG_END);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        std::cout << "Clearing cursor failed: " << results.ErrorMessage() << std::endl;
        return false;
    }

    int i = EmuConstants::CURSOR_QUEUE_BEGIN;
    for(auto it = start; it != end; ++it, i++) {
        ItemInst *inst = *it;
		int16 use_slot = (i == EmuConstants::CURSOR_QUEUE_BEGIN) ? MainCursor : i;
		if(inst)
			Log.Out(Logs::Moderate, Logs::Inventory, "SaveCursor: Attempting to save item %s for char %d in slot %d", inst->GetItem()->Name, char_id, use_slot);
		else
			Log.Out(Logs::Moderate, Logs::Inventory, "SaveCursor: No inst found. This is either an error, or we've reached the end of the list.");
		if (!SaveInventory(char_id, inst, use_slot)) {
			return false;
		}
    }
	return true;
}

bool SharedDatabase::SaveInventory(uint32 char_id, const ItemInst* inst, int16 slot_id)
{
	if (!inst) // All other inventory
        return DeleteInventorySlot(char_id, slot_id);

    return UpdateInventorySlot(char_id, inst, slot_id);
}

bool SharedDatabase::UpdateInventorySlot(uint32 char_id, const ItemInst* inst, int16 slot_id)
{
    uint16 charges = 0;
	if(inst->GetCharges() >= 0)
		charges = inst->GetCharges();
	else
		charges = 0x7FFF;

	// Update/Insert item
	std::string query = StringFormat("REPLACE INTO character_inventory "
		"(id, slotid, itemid, charges, instnodrop, custom_data, color)"
		" VALUES(%lu,%lu,%lu,%lu,%lu,'%s',%lu)",
		(unsigned long)char_id, (unsigned long)slot_id, (unsigned long)inst->GetItem()->ID,
		(unsigned long)charges, (unsigned long)(inst->IsInstNoDrop() ? 1 : 0),
		inst->GetCustomDataString().c_str(), (unsigned long)inst->GetColor());
	auto results = QueryDatabase(query);

    // Save bag contents, if slot supports bag contents
	if (inst && inst->IsType(ItemClassContainer) && Inventory::SupportsContainers(slot_id))
		for (uint8 idx = SUB_BEGIN; idx < EmuConstants::ITEM_CONTAINER_SIZE; idx++) {
			const ItemInst* baginst = inst->GetItem(idx);
			SaveInventory(char_id, baginst, Inventory::CalcSlotId(slot_id, idx));
		}

    if (!results.Success()) {
        return false;
    }

	return true;
}

bool SharedDatabase::DeleteInventorySlot(uint32 char_id, int16 slot_id) {

	// Delete item
	std::string query = StringFormat("DELETE FROM character_inventory WHERE id = %i AND slotid = %i", char_id, slot_id);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

    // Delete bag slots, if need be
    if (!Inventory::SupportsContainers(slot_id))
        return true;

    int16 base_slot_id = Inventory::CalcSlotId(slot_id, SUB_BEGIN);
    query = StringFormat("DELETE FROM character_inventory WHERE id = %i AND slotid >= %i AND slotid < %i",
                        char_id, base_slot_id, (base_slot_id+10));
    results = QueryDatabase(query);
    if (!results.Success()) {
        return false;
    }

    // @merth: need to delete augments here
    return true;
}

bool SharedDatabase::SetStartingItems(PlayerProfile_Struct* pp, Inventory* inv, uint32 si_race, uint32 si_class, uint32 si_deity, uint32 si_current_zone, char* si_name, int admin_level) {

	const Item_Struct* myitem;

    std::string query = StringFormat("SELECT itemid, item_charges, slot FROM starting_items "
                                    "WHERE (race = %i or race = 0) AND (class = %i or class = 0) AND "
                                    "(deityid = %i or deityid = 0) AND (zoneid = %i or zoneid = 0) AND "
                                    "gm <= %i ORDER BY id",
                                    si_race, si_class, si_deity, si_current_zone, admin_level);
    auto results = QueryDatabase(query);
    if (!results.Success())
        return false;


	for (auto row = results.begin(); row != results.end(); ++row) {
		int32 itemid = atoi(row[0]);
		int32 charges = atoi(row[1]);
		int32 slot = atoi(row[2]);
		myitem = GetItem(itemid);

		if(!myitem)
			continue;

		ItemInst* myinst = CreateBaseItem(myitem, charges);

		if(slot < 0)
			slot = inv->FindFreeSlot(0, 0);

		inv->PutItem(slot, *myinst);
		safe_delete(myinst);
	}

	return true;
}

// Overloaded: Retrieve character inventory based on character id
bool SharedDatabase::GetInventory(uint32 char_id, Inventory* inv) {
	// Retrieve character inventory
	std::string query = StringFormat("SELECT slotid, itemid, charges, color, instnodrop, custom_data "
                                    "FROM character_inventory WHERE id = %i ORDER BY slotid", char_id);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
            Log.Out(Logs::General, Logs::Error, "If you got an error related to the 'instnodrop' field, run the following SQL Queries:\nalter table inventory add instnodrop tinyint(1) unsigned default 0 not null;\n");
        return false;
    }

    for (auto row = results.begin(); row != results.end(); ++row) {
        int16 slot_id	= atoi(row[0]);
        uint32 item_id	= atoi(row[1]);
        uint16 charges	= atoi(row[2]);
        uint32 color	= atoul(row[3]);

        bool instnodrop	= (row[4] && (uint16)atoi(row[4]))? true: false;

        const Item_Struct* item = GetItem(item_id);

        if (!item) {
            Log.Out(Logs::General, Logs::Error,"Warning: charid %i has an invalid item_id %i in inventory slot %i", char_id, item_id, slot_id);
            continue;
        }

        int16 put_slot_id = INVALID_INDEX;

        ItemInst* inst = CreateBaseItem(item, charges);

		if (inst == nullptr)
			continue;

        if(row[5]) {
            std::string data_str(row[5]);
            std::string idAsString;
            std::string value;
            bool use_id = true;

            for(int i = 0; i < data_str.length(); ++i) {
                if(data_str[i] == '^') {
                    if(!use_id) {
                        inst->SetCustomData(idAsString, value);
                        idAsString.clear();
                        value.clear();
                    }

                    use_id = !use_id;
                    continue;
                }

                char v = data_str[i];
                if(use_id)
                    idAsString.push_back(v);
                else
                    value.push_back(v);
            }
        }

		if (instnodrop)
			inst->SetInstNoDrop(true);

        if (color > 0)
            inst->SetColor(color);

        if(charges==0x7FFF)
            inst->SetCharges(-1);
        else
            inst->SetCharges(charges);

        if (slot_id >= EmuConstants::CURSOR_QUEUE_BEGIN && slot_id <= EmuConstants::CURSOR_QUEUE_END)
            put_slot_id = inv->PushCursor(*inst);
        else if (slot_id >= 3110 && slot_id <= 3179) {
            // Admins: please report any occurrences of this error
            Log.Out(Logs::General, Logs::Error, "Warning: Defunct location for item in inventory: charid=%i, item_id=%i, slot_id=%i .. pushing to cursor...", char_id, item_id, slot_id);
            put_slot_id = inv->PushCursor(*inst);
        } else
            put_slot_id = inv->PutItem(slot_id, *inst);

        safe_delete(inst);

        // Save ptr to item in inventory
        if (put_slot_id == INVALID_INDEX) {
            Log.Out(Logs::General, Logs::Error, "Warning: Invalid slot_id for item in inventory: charid=%i, item_id=%i, slot_id=%i",char_id, item_id, slot_id);
        }
    }

	return true;
}

// Overloaded: Retrieve character inventory based on account_id and character name
bool SharedDatabase::GetInventory(uint32 account_id, char* name, Inventory* inv) {
	// Retrieve character inventory
	std::string query = StringFormat("SELECT ci.slotid, ci.itemid, ci.charges, ci.color, ci.instnodrop, ci.custom_data "
                                    "FROM character_inventory ci INNER JOIN character_data ch "
                                    "ON ch.id = ci.id WHERE ch.name = '%s' AND ch.account_id = %i ORDER BY ci.slotid",
                                    name, account_id);
    auto results = QueryDatabase(query);
    if (!results.Success()){
		Log.Out(Logs::General, Logs::Error, "If you got an error related to the 'instnodrop' field, run the following SQL Queries:\nalter table inventory add instnodrop tinyint(1) unsigned default 0 not null;\n");
        return false;
	}


    for (auto row = results.begin(); row != results.end(); ++row) {
        int16 slot_id	= atoi(row[0]);
        uint32 item_id	= atoi(row[1]);
        int8 charges	= atoi(row[2]);
        uint32 color	= atoul(row[3]);

        bool instnodrop	= (row[4] && (uint16)atoi(row[4])) ? true : false;
        const Item_Struct* item = GetItem(item_id);
        int16 put_slot_id = INVALID_INDEX;
        if(!item)
            continue;

        ItemInst* inst = CreateBaseItem(item, charges);

		if (inst == nullptr)
			continue;

        inst->SetInstNoDrop(instnodrop);

        if(row[5]) {
            std::string data_str(row[5]);
            std::string idAsString;
            std::string value;
            bool use_id = true;

            for(int i = 0; i < data_str.length(); ++i) {
                if(data_str[i] == '^') {
                    if(!use_id) {
                        inst->SetCustomData(idAsString, value);
                        idAsString.clear();
                        value.clear();
                    }

                    use_id = !use_id;
                    continue;
                }

                char v = data_str[i];
                if(use_id)
                    idAsString.push_back(v);
                else
                    value.push_back(v);

            }
        }

        if (color > 0)
            inst->SetColor(color);

        inst->SetCharges(charges);

        if (slot_id>=EmuConstants::CURSOR_QUEUE_BEGIN && slot_id <= EmuConstants::CURSOR_QUEUE_END)
            put_slot_id = inv->PushCursor(*inst);
        else
            put_slot_id = inv->PutItem(slot_id, *inst);

        safe_delete(inst);

        // Save ptr to item in inventory
        if (put_slot_id == INVALID_INDEX)
            Log.Out(Logs::General, Logs::Error, "Warning: Invalid slot_id for item in inventory: name=%s, acctid=%i, item_id=%i, slot_id=%i", name, account_id, item_id, slot_id);

    }

	return true;
}


void SharedDatabase::GetItemsCount(int32 &item_count, uint32 &max_id) {
	item_count = -1;
	max_id = 0;

	const std::string query = "SELECT MAX(id), count(*) FROM items";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return;
	}

	if (results.RowCount() == 0)
        return;

    auto row = results.begin();

    if(row[0])
        max_id = atoi(row[0]);

    if (row[1])
		item_count = atoi(row[1]);
}

bool SharedDatabase::LoadItems(const std::string &prefix) {
	items_mmf.reset(nullptr);

	try {
		EQEmu::IPCMutex mutex("items");
		mutex.Lock();
		std::string file_name = std::string("shared/") + prefix + std::string("items");
		items_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name));
		items_hash = std::unique_ptr<EQEmu::FixedMemoryHashSet<Item_Struct>>(new EQEmu::FixedMemoryHashSet<Item_Struct>(reinterpret_cast<uint8*>(items_mmf->Get()), items_mmf->Size()));
		mutex.Unlock();
	} catch(std::exception& ex) {
		Log.Out(Logs::General, Logs::Error, "Error Loading Items: %s", ex.what());
		return false;
	}

	return true;
}

void SharedDatabase::LoadItems(void *data, uint32 size, int32 items, uint32 max_item_id) {
	EQEmu::FixedMemoryHashSet<Item_Struct> hash(reinterpret_cast<uint8*>(data), size, items, max_item_id);

	char ndbuffer[4];
	bool disableNoRent = false;
	if(GetVariable("disablenorent", ndbuffer, 4)) {
		if(ndbuffer[0] == '1' && ndbuffer[1] == '\0') {
			disableNoRent = true;
		}
	}
	bool disableNoDrop = false;
	if(GetVariable("disablenodrop", ndbuffer, 4)) {
		if(ndbuffer[0] == '1' && ndbuffer[1] == '\0') {
			disableNoDrop = true;
		}
	}
	bool disableNoTransfer = false;
	if(GetVariable("disablenotransfer", ndbuffer, 4)) {
		if(ndbuffer[0] == '1' && ndbuffer[1] == '\0') {
			disableNoTransfer = true;
		}
	}

    Item_Struct item;

	const std::string query = "SELECT source,"
#define F(x) "`"#x"`,"
#include "item_fieldlist.h"
#undef F
		"updated FROM items ORDER BY id";
	auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for(auto row = results.begin(); row != results.end(); ++row) {
        memset(&item, 0, sizeof(Item_Struct));

		item.ItemClass = (uint8)atoi(row[ItemField::itemclass]);
		strcpy(item.Name, row[ItemField::name]);
		strcpy(item.Lore, row[ItemField::lore]);
		strcpy(item.IDFile, row[ItemField::idfile]);
		item.ID = (uint32)atoul(row[ItemField::id]);
		item.Weight = (uint8)atoi(row[ItemField::weight]);
		item.NoRent = disableNoRent ? (uint8)atoi("255") : (uint8)atoi(row[ItemField::norent]);
		item.NoDrop = disableNoDrop ? (uint8)atoi("255") : (uint8)atoi(row[ItemField::nodrop]);
		item.Size = (uint8)atoi(row[ItemField::size]);
		item.Slots = (uint32)atoul(row[ItemField::slots]);
		item.Price = (uint32)atoul(row[ItemField::price]);
		item.Icon = (uint32)atoul(row[ItemField::icon]);
		item.BenefitFlag = (atoul(row[ItemField::benefitflag]) != 0);
		item.Tradeskills = (atoi(row[ItemField::tradeskills]) == 0) ? false : true;
		item.CR = (int8)atoi(row[ItemField::cr]);
		item.DR = (int8)atoi(row[ItemField::dr]);
		item.PR = (int8)atoi(row[ItemField::pr]);
		item.MR = (int8)atoi(row[ItemField::mr]);
		item.FR = (int8)atoi(row[ItemField::fr]);
		item.AStr = (int8)atoi(row[ItemField::astr]);
		item.ASta = (int8)atoi(row[ItemField::asta]);
		item.AAgi = (int8)atoi(row[ItemField::aagi]);
		item.ADex = (int8)atoi(row[ItemField::adex]);
		item.ACha = (int8)atoi(row[ItemField::acha]);
		item.AInt = (int8)atoi(row[ItemField::aint]);
		item.AWis = (int8)atoi(row[ItemField::awis]);
		item.HP = (int32)atoul(row[ItemField::hp]);
		item.Mana = (int32)atoul(row[ItemField::mana]);
		item.AC = (int32)atoul(row[ItemField::ac]);
		item.Deity = (uint32)atoul(row[ItemField::deity]);
		item.SkillModValue = (int32)atoul(row[ItemField::skillmodvalue]);
		//item.Unk033 = (int32)atoul(row[ItemField::UNK033]);
		item.SkillModType = (uint32)atoul(row[ItemField::skillmodtype]);
		item.BaneDmgRace = (uint32)atoul(row[ItemField::banedmgrace]);
		item.BaneDmgAmt = (int8)atoi(row[ItemField::banedmgamt]);
		item.BaneDmgBody = (uint32)atoul(row[ItemField::banedmgbody]);
		item.Magic = (atoi(row[ItemField::magic]) == 0) ? false : true;
		item.CastTime_ = (int32)atoul(row[ItemField::casttime_]);
		item.ReqLevel = (uint8)atoi(row[ItemField::reqlevel]);
		item.BardType = (uint32)atoul(row[ItemField::bardtype]);
		item.BardValue = (int32)atoul(row[ItemField::bardvalue]);
		item.Light = (int8)atoi(row[ItemField::light]);
		item.Delay = (uint8)atoi(row[ItemField::delay]);
		item.RecLevel = (uint8)atoi(row[ItemField::reclevel]);
		item.RecSkill = (uint8)atoi(row[ItemField::recskill]);
		item.ElemDmgType = (uint8)atoi(row[ItemField::elemdmgtype]);
		item.ElemDmgAmt = (uint8)atoi(row[ItemField::elemdmgamt]);
		item.Range = (uint8)atoi(row[ItemField::range]);
		item.Damage = (uint32)atoi(row[ItemField::damage]);
		item.Color = (uint32)atoul(row[ItemField::color]);
		item.Classes = (uint32)atoul(row[ItemField::classes]);
		item.Races = (uint32)atoul(row[ItemField::races]);
		//item.Unk054 = (uint32)atoul(row[ItemField::UNK054]);
		item.MaxCharges = (int16)atoi(row[ItemField::maxcharges]);
		item.ItemType = (uint8)atoi(row[ItemField::itemtype]);
		item.Material = (uint8)atoi(row[ItemField::material]);
		item.SellRate = (float)atof(row[ItemField::sellrate]);
		//item.Unk059 = (uint32)atoul(row[ItemField::UNK059]);
		item.CastTime = (uint32)atoul(row[ItemField::casttime]);
		item.ProcRate = (int32)atoi(row[ItemField::procrate]);
		item.CharmFileID = (uint32)atoul(row[ItemField::charmfileid]);
		item.FactionMod1 = (int32)atoul(row[ItemField::factionmod1]);
		item.FactionMod2 = (int32)atoul(row[ItemField::factionmod2]);
		item.FactionMod3 = (int32)atoul(row[ItemField::factionmod3]);
		item.FactionMod4 = (int32)atoul(row[ItemField::factionmod4]);
		item.FactionAmt1 = (int32)atoul(row[ItemField::factionamt1]);
		item.FactionAmt2 = (int32)atoul(row[ItemField::factionamt2]);
		item.FactionAmt3 = (int32)atoul(row[ItemField::factionamt3]);
		item.FactionAmt4 = (int32)atoul(row[ItemField::factionamt4]);
		strcpy(item.CharmFile, row[ItemField::charmfile]);
		item.BagType = (uint8)atoi(row[ItemField::bagtype]);
		item.BagSlots = (uint8)atoi(row[ItemField::bagslots]);
		item.BagSize = (uint8)atoi(row[ItemField::bagsize]);
		item.BagWR = (uint8)atoi(row[ItemField::bagwr]);
		item.Book = (uint8)atoi(row[ItemField::book]);
		item.BookType = (uint32)atoul(row[ItemField::booktype]);
		strcpy(item.Filename, row[ItemField::filename]);
		item.FVNoDrop = (atoi(row[ItemField::fvnodrop]) == 0) ? false : true;
		item.RecastDelay = (uint32)atoul(row[ItemField::recastdelay]);
		item.RecastType = (uint32)atoul(row[ItemField::recasttype]);
		item.NoPet = (atoi(row[ItemField::nopet]) == 0) ? false : true;
		item.StackSize = (uint16)atoi(row[ItemField::stacksize]);
		item.NoTransfer = disableNoTransfer ? false : (atoi(row[ItemField::notransfer]) == 0) ? false : true;
		item.Stackable = (atoi(row[ItemField::stackable]) == 3) ? false : true;
		item.Stackable_ = (uint8)atoul(row[ItemField::stackable]);
		item.Click.Effect = (uint32)atoul(row[ItemField::clickeffect]);
		item.Click.Type = (uint8)atoul(row[ItemField::clicktype]);
		item.Click.Level = (uint8)atoul(row[ItemField::clicklevel]);
		item.Click.Level2 = (uint8)atoul(row[ItemField::clicklevel2]);
		strcpy(item.CharmFile, row[ItemField::charmfile]);
		item.Proc.Effect = (uint16)atoul(row[ItemField::proceffect]);
		item.Proc.Type = (uint8)atoul(row[ItemField::proctype]);
		item.Proc.Level = (uint8)atoul(row[ItemField::proclevel]);
		item.Proc.Level2 = (uint8)atoul(row[ItemField::proclevel2]);
		item.Worn.Effect = (uint16)atoul(row[ItemField::worneffect]);
		item.Worn.Type = (uint8)atoul(row[ItemField::worntype]);
		item.Worn.Level = (uint8)atoul(row[ItemField::wornlevel]);
		item.Worn.Level2 = (uint8)atoul(row[ItemField::wornlevel2]);
		item.Focus.Effect = (uint16)atoul(row[ItemField::focuseffect]);
		item.Focus.Type = (uint8)atoul(row[ItemField::focustype]);
		item.Focus.Level = (uint8)atoul(row[ItemField::focuslevel]);
		item.Focus.Level2 = (uint8)atoul(row[ItemField::focuslevel2]);
		item.Scroll.Effect = (uint16)atoul(row[ItemField::scrolleffect]);
		item.Scroll.Type = (uint8)atoul(row[ItemField::scrolltype]);
		item.Scroll.Level = (uint8)atoul(row[ItemField::scrolllevel]);
		item.Scroll.Level2 = (uint8)atoul(row[ItemField::scrolllevel2]);
		item.Bard.Effect = (uint16)atoul(row[ItemField::bardeffect]);
		item.Bard.Type = (uint8)atoul(row[ItemField::bardtype]);
		item.Bard.Level = (uint8)atoul(row[ItemField::bardlevel]);
		item.Bard.Level2 = (uint8)atoul(row[ItemField::bardlevel2]);
		item.QuestItemFlag = (atoi(row[ItemField::questitemflag]) == 0) ? false : true;
		item.GMFlag = (int8)atoi(row[ItemField::gmflag]);
		item.Soulbound = (int8)atoi(row[ItemField::soulbound]);

        try {
            hash.insert(item.ID, item);
        } catch(std::exception &ex) {
            Log.Out(Logs::General, Logs::Error, "Database::LoadItems: %s", ex.what());
            break;
        }
    }

}

const Item_Struct* SharedDatabase::GetItem(uint32 id) {
	if(!items_hash || id > items_hash->max_key()) {
		return nullptr;
	}

	if(items_hash->exists(id)) {
		return &(items_hash->at(id));
	}

	return nullptr;
}

const Item_Struct* SharedDatabase::IterateItems(uint32* id) {
	if(!items_hash || !id) {
		return nullptr;
	}

	for(;;) {
		if(*id > items_hash->max_key()) {
			break;
		}

		if(items_hash->exists(*id)) {
			return &(items_hash->at((*id)++));
		} else {
			++(*id);
		}
	}

	return nullptr;
}

std::string SharedDatabase::GetBook(const char *txtfile)
{
	char txtfile2[20];
	std::string txtout;
	strcpy(txtfile2, txtfile);

	std::string query = StringFormat("SELECT txtfile FROM books WHERE name = '%s'", txtfile2);
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		txtout.assign(" ",1);
		return txtout;
	}

    if (results.RowCount() == 0) {
        Log.Out(Logs::General, Logs::Error, "No book to send, (%s)", txtfile);
        txtout.assign(" ",1);
        return txtout;
    }

    auto row = results.begin();
    txtout.assign(row[0],strlen(row[0]));

    return txtout;
}

void SharedDatabase::GetFactionListInfo(uint32 &list_count, uint32 &max_lists) {
	list_count = 0;
	max_lists = 0;

	const std::string query = "SELECT COUNT(*), MAX(id) FROM npc_faction";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return;
	}

	if (results.RowCount() == 0)
        return;

    auto row = results.begin();

    list_count = static_cast<uint32>(atoul(row[0]));
    max_lists = static_cast<uint32>(atoul(row[1] ? row[1] : "0"));
}

const NPCFactionList* SharedDatabase::GetNPCFactionEntry(uint32 id) {
	if(!faction_hash) {
		return nullptr;
	}

	if(faction_hash->exists(id)) {
		return &(faction_hash->at(id));
	}

	return nullptr;
}

void SharedDatabase::LoadNPCFactionLists(void *data, uint32 size, uint32 list_count, uint32 max_lists) {
	EQEmu::FixedMemoryHashSet<NPCFactionList> hash(reinterpret_cast<uint8*>(data), size, list_count, max_lists);
	NPCFactionList faction;

	const std::string query = "SELECT npc_faction.id, npc_faction.primaryfaction, npc_faction.ignore_primary_assist, "
                            "npc_faction_entries.faction_id, npc_faction_entries.value, npc_faction_entries.npc_value, "
                            "npc_faction_entries.temp FROM npc_faction LEFT JOIN npc_faction_entries "
                            "ON npc_faction.id = npc_faction_entries.npc_faction_id ORDER BY npc_faction.id;";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
		return;
    }

    uint32 current_id = 0;
    uint32 current_entry = 0;

    for(auto row = results.begin(); row != results.end(); ++row) {
        uint32 id = static_cast<uint32>(atoul(row[0]));
        if(id != current_id) {
            if(current_id != 0) {
                hash.insert(current_id, faction);
            }

            memset(&faction, 0, sizeof(faction));
            current_entry = 0;
            current_id = id;
            faction.id = id;
            faction.primaryfaction = static_cast<uint32>(atoul(row[1]));
            faction.assistprimaryfaction = (atoi(row[2]) == 0);
        }

        if(!row[3])
            continue;

        if(current_entry >= MAX_NPC_FACTIONS)
				continue;

        faction.factionid[current_entry] = static_cast<uint32>(atoul(row[3]));
        faction.factionvalue[current_entry] = static_cast<int32>(atoi(row[4]));
        faction.factionnpcvalue[current_entry] = static_cast<int8>(atoi(row[5]));
        faction.factiontemp[current_entry] = static_cast<uint8>(atoi(row[6]));
        ++current_entry;
    }

    if(current_id != 0)
        hash.insert(current_id, faction);

}

bool SharedDatabase::LoadNPCFactionLists(const std::string &prefix) {
	faction_mmf.reset(nullptr);
	faction_hash.reset(nullptr);

	try {
		EQEmu::IPCMutex mutex("faction");
		mutex.Lock();
		std::string file_name = std::string("shared/") + prefix + std::string("faction");
		faction_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name));
		faction_hash = std::unique_ptr<EQEmu::FixedMemoryHashSet<NPCFactionList>>(new EQEmu::FixedMemoryHashSet<NPCFactionList>(reinterpret_cast<uint8*>(faction_mmf->Get()), faction_mmf->Size()));
		mutex.Unlock();
	} catch(std::exception& ex) {
		Log.Out(Logs::General, Logs::Error, "Error Loading npc factions: %s", ex.what());
		return false;
	}

	return true;
}

// Create appropriate ItemInst class
ItemInst* SharedDatabase::CreateItem(uint32 item_id, int16 charges)
{
	const Item_Struct* item = nullptr;
	ItemInst* inst = nullptr;

	item = GetItem(item_id);
	if (item) {
		inst = CreateBaseItem(item, charges);

		if (inst == nullptr) {
			Log.Out(Logs::General, Logs::Error, "Error: valid item data returned a null reference for ItemInst creation in SharedDatabase::CreateItem()");
			Log.Out(Logs::General, Logs::Error, "Item Data = ID: %u, Name: %s, Charges: %i", item->ID, item->Name, charges);
			return nullptr;
		}
	}

	return inst;
}


// Create appropriate ItemInst class
ItemInst* SharedDatabase::CreateItem(const Item_Struct* item, int16 charges)
{
	ItemInst* inst = nullptr;
	if (item) {
		inst = CreateBaseItem(item, charges);

		if (inst == nullptr) {
			Log.Out(Logs::General, Logs::Error, "Error: valid item data returned a null reference for ItemInst creation in SharedDatabase::CreateItem()");
			Log.Out(Logs::General, Logs::Error, "Item Data = ID: %u, Name: %s, Charges: %i", item->ID, item->Name, charges);
			return nullptr;
		}
	}

	return inst;
}

ItemInst* SharedDatabase::CreateBaseItem(const Item_Struct* item, int16 charges) {
	ItemInst* inst = nullptr;
	if (item) {
		// if maxcharges is -1 that means it is an unlimited use item.
		// set it to 1 charge so that it is usable on creation
		if (charges == 0 && item->MaxCharges == -1)
			charges = 1;

		inst = new ItemInst(item, charges);

		if (inst == nullptr) {
			Log.Out(Logs::General, Logs::Error, "Error: valid item data returned a null reference for ItemInst creation in SharedDatabase::CreateBaseItem()");
			Log.Out(Logs::General, Logs::Error, "Item Data = ID: %u, Name: %s, Charges: %i", item->ID, item->Name, charges);
			return nullptr;
		}

		if(item->CharmFileID != 0) {
			inst->Initialize(this);
		}
	}
	return inst;
}

int32 SharedDatabase::DeleteStalePlayerCorpses() {
	int32 rows_affected = 0;
	if(RuleB(Zone, EnableShadowrest)) {
        std::string query = StringFormat(
			"UPDATE `character_corpses` SET `is_buried` = 1 WHERE `is_buried` = 0 AND "
            "(UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) > %d AND NOT time_of_death = 0",
             (RuleI(Character, CorpseDecayTimeMS) / 1000));
        auto results = QueryDatabase(query);
		if (!results.Success())
			return -1;

		rows_affected += results.RowsAffected();

		std::string sr_query = StringFormat(
			"DELETE FROM `character_corpses` WHERE `is_buried` = 1 AND (UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) > %d "
			"AND NOT time_of_death = 0", (RuleI(Character, CorpseDecayTimeMS) / 1000)*2);
		 auto sr_results = QueryDatabase(sr_query);
		 if (!sr_results.Success())
			 return -1;

		rows_affected += sr_results.RowsAffected();

	}
	else
	{
		std::string query = StringFormat(
			"DELETE FROM `character_corpses` WHERE (UNIX_TIMESTAMP() - UNIX_TIMESTAMP(time_of_death)) > %d "
			"AND NOT time_of_death = 0", (RuleI(Character, CorpseDecayTimeMS) / 1000));
		auto results = QueryDatabase(query);
		if (!results.Success())
			return -1;

		rows_affected += results.RowsAffected();
	}

	if(RuleB(Character, UsePlayerCorpseBackups))
	{
		std::string cb_query = StringFormat(
			"SELECT id FROM `character_corpses_backup`");
		auto cb_results = QueryDatabase(cb_query);
		for (auto row = cb_results.begin(); row != cb_results.end(); ++row) {
			uint32 corpse_id = atoi(row[0]);
			std::string cbd_query = StringFormat(
				"DELETE from character_corpses_backup where id = %d AND ( "
				"SELECT COUNT(*) from character_corpse_items_backup where corpse_id = %d) "
				" = 0", corpse_id, corpse_id);
			auto cbd_results = QueryDatabase(cbd_query);
			if(!cbd_results.Success())
				return -1;

			rows_affected += cbd_results.RowsAffected();
		}
	}

    return rows_affected;
}

bool SharedDatabase::GetCommandSettings(std::map<std::string,uint8> &commands) {

	const std::string query = "SELECT command, access FROM commands";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

    commands.clear();

    for (auto row = results.begin(); row != results.end(); ++row)
        commands[row[0]]=atoi(row[1]);

    return true;
}

bool SharedDatabase::LoadSkillCaps(const std::string &prefix) {
	skill_caps_mmf.reset(nullptr);

	uint32 class_count = PLAYER_CLASS_COUNT;
	uint32 skill_count = HIGHEST_SKILL + 1;
	uint32 level_count = HARD_LEVEL_CAP + 1;
	uint32 size = (class_count * skill_count * level_count * sizeof(uint16));

	try {
		EQEmu::IPCMutex mutex("skill_caps");
		mutex.Lock();
		std::string file_name = std::string("shared/") + prefix + std::string("skill_caps");
		skill_caps_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name));
		mutex.Unlock();
	} catch(std::exception &ex) {
		Log.Out(Logs::General, Logs::Error, "Error loading skill caps: %s", ex.what());
		return false;
	}

	return true;
}

void SharedDatabase::LoadSkillCaps(void *data) {
	uint32 class_count = PLAYER_CLASS_COUNT;
	uint32 skill_count = HIGHEST_SKILL + 1;
	uint32 level_count = HARD_LEVEL_CAP + 1;
	uint16 *skill_caps_table = reinterpret_cast<uint16*>(data);

	const std::string query = "SELECT skillID, class, level, cap FROM skill_caps ORDER BY skillID, class, level";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        Log.Out(Logs::General, Logs::Error, "Error loading skill caps from database: %s", results.ErrorMessage().c_str());
        return;
	}

    for(auto row = results.begin(); row != results.end(); ++row) {
        uint8 skillID = atoi(row[0]);
        uint8 class_ = atoi(row[1]) - 1;
        uint8 level = atoi(row[2]);
        uint16 cap = atoi(row[3]);

        if(skillID >= skill_count || class_ >= class_count || level >= level_count)
            continue;

        uint32 index = (((class_ * skill_count) + skillID) * level_count) + level;
        skill_caps_table[index] = cap;
    }
}

uint16 SharedDatabase::GetSkillCap(uint8 Class_, SkillUseTypes Skill, uint8 Level) {
	if(!skill_caps_mmf) {
		return 0;
	}

	if(Class_ == 0)
		return 0;

	int SkillMaxLevel = RuleI(Character, SkillCapMaxLevel);
	if(SkillMaxLevel < 1) {
		SkillMaxLevel = RuleI(Character, MaxLevel);
	}

	uint32 class_count = PLAYER_CLASS_COUNT;
	uint32 skill_count = HIGHEST_SKILL + 1;
	uint32 level_count = HARD_LEVEL_CAP + 1;
	if(Class_ > class_count || static_cast<uint32>(Skill) > skill_count || Level > level_count) {
		return 0;
	}

	if(Level > static_cast<uint8>(SkillMaxLevel)){
		Level = static_cast<uint8>(SkillMaxLevel);
	}

	uint32 index = ((((Class_ - 1) * skill_count) + Skill) * level_count) + Level;
	uint16 *skill_caps_table = reinterpret_cast<uint16*>(skill_caps_mmf->Get());
	return skill_caps_table[index];
}

uint8 SharedDatabase::GetTrainLevel(uint8 Class_, SkillUseTypes Skill, uint8 Level) {
	if(!skill_caps_mmf) {
		return 0;
	}

	if(Class_ == 0)
		return 0;

	int SkillMaxLevel = RuleI(Character, SkillCapMaxLevel);
	if (SkillMaxLevel < 1) {
		SkillMaxLevel = RuleI(Character, MaxLevel);
	}

	uint32 class_count = PLAYER_CLASS_COUNT;
	uint32 skill_count = HIGHEST_SKILL + 1;
	uint32 level_count = HARD_LEVEL_CAP + 1;
	if(Class_ > class_count || static_cast<uint32>(Skill) > skill_count || Level > level_count) {
		return 0;
	}

	uint8 ret = 0;
	if(Level > static_cast<uint8>(SkillMaxLevel)) {
		uint32 index = ((((Class_ - 1) * skill_count) + Skill) * level_count);
		uint16 *skill_caps_table = reinterpret_cast<uint16*>(skill_caps_mmf->Get());
		for(uint8 x = 0; x < Level; x++){
			if(skill_caps_table[index + x]){
				ret = x;
				break;
			}
		}
	}
	else
	{
		uint32 index = ((((Class_ - 1) * skill_count) + Skill) * level_count);
		uint16 *skill_caps_table = reinterpret_cast<uint16*>(skill_caps_mmf->Get());
		for(int x = 0; x < SkillMaxLevel; x++){
			if(skill_caps_table[index + x]){
				ret = x;
				break;
			}
		}
	}

	if(ret > GetSkillCap(Class_, Skill, Level))
		ret = static_cast<uint8>(GetSkillCap(Class_, Skill, Level));

	return ret;
}

void SharedDatabase::LoadDamageShieldTypes(SPDat_Spell_Struct* sp, int32 iMaxSpellID) {

	std::string query = StringFormat("SELECT `spellid`, `type` FROM `damageshieldtypes` WHERE `spellid` > 0 "
                                    "AND `spellid` <= %i", iMaxSpellID);
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    for(auto row = results.begin(); row != results.end(); ++row) {
        int spellID = atoi(row[0]);
        if((spellID > 0) && (spellID <= iMaxSpellID))
            sp[spellID].DamageShieldType = atoi(row[1]);
    }

}

int SharedDatabase::GetMaxSpellID() {
	std::string query = "SELECT MAX(id) FROM spells_new";
	auto results = QueryDatabase(query);
    if (!results.Success()) {
        return -1;
    }

    auto row = results.begin();

	return atoi(row[0]);
}

bool SharedDatabase::LoadSpells(const std::string &prefix, int32 *records, const SPDat_Spell_Struct **sp) {
	spells_mmf.reset(nullptr);

	try {
		EQEmu::IPCMutex mutex("spells");
		mutex.Lock();
	
		std::string file_name = std::string("shared/") + prefix + std::string("spells");
		spells_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name));
		*records = *reinterpret_cast<uint32*>(spells_mmf->Get());
		*sp = reinterpret_cast<const SPDat_Spell_Struct*>((char*)spells_mmf->Get() + 4);
		mutex.Unlock();
	}
	catch(std::exception& ex) {
		Log.Out(Logs::General, Logs::Error, "Error Loading Spells: %s", ex.what());
		return false;
	}
	return true;
}

void SharedDatabase::LoadSpells(void *data, int max_spells) {
	*(uint32*)data = max_spells;
	SPDat_Spell_Struct *sp = reinterpret_cast<SPDat_Spell_Struct*>((char*)data + sizeof(uint32));

	const std::string query = "SELECT * FROM spells_new ORDER BY id ASC";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    if(results.ColumnCount() <= SPELL_LOAD_FIELD_COUNT) {
		Log.Out(Logs::Detail, Logs::Spells, "Fatal error loading spells: Spell field count < SPELL_LOAD_FIELD_COUNT(%u)", SPELL_LOAD_FIELD_COUNT);
		return;
    }

    int tempid = 0;
    int counter = 0;

    for (auto row = results.begin(); row != results.end(); ++row) {
        tempid = atoi(row[0]);
        if(tempid >= max_spells) {
            Log.Out(Logs::Detail, Logs::Spells, "Non fatal error: spell.id >= max_spells, ignoring.");
            continue;
        }

        ++counter;
        sp[tempid].id = tempid;
        strn0cpy(sp[tempid].name, row[1], sizeof(sp[tempid].name));
        strn0cpy(sp[tempid].player_1, row[2], sizeof(sp[tempid].player_1));
		strn0cpy(sp[tempid].teleport_zone, row[3], sizeof(sp[tempid].teleport_zone));
		strn0cpy(sp[tempid].you_cast, row[4], sizeof(sp[tempid].you_cast));
		strn0cpy(sp[tempid].other_casts, row[5], sizeof(sp[tempid].other_casts));
		strn0cpy(sp[tempid].cast_on_you, row[6], sizeof(sp[tempid].cast_on_you));
		strn0cpy(sp[tempid].cast_on_other, row[7], sizeof(sp[tempid].cast_on_other));
		strn0cpy(sp[tempid].spell_fades, row[8], sizeof(sp[tempid].spell_fades));

		sp[tempid].range=static_cast<float>(atof(row[9]));
		sp[tempid].aoerange=static_cast<float>(atof(row[10]));
		sp[tempid].pushback=static_cast<float>(atof(row[11]));
		sp[tempid].pushup=static_cast<float>(atof(row[12]));
		sp[tempid].cast_time=atoi(row[13]);
		sp[tempid].recovery_time=atoi(row[14]);
		sp[tempid].recast_time=atoi(row[15]);
		sp[tempid].buffdurationformula=atoi(row[16]);
		sp[tempid].buffduration=atoi(row[17]);
		sp[tempid].AEDuration=atoi(row[18]);
		sp[tempid].mana=atoi(row[19]);

		int y=0;
		for(y=0; y< EFFECT_COUNT;y++)
			sp[tempid].base[y]=atoi(row[20+y]); // effect_base_value

		for(y=0; y < EFFECT_COUNT; y++)
			sp[tempid].base2[y]=atoi(row[32+y]); // effect_limit_value

		for(y=0; y< EFFECT_COUNT;y++)
			sp[tempid].max[y]=atoi(row[44+y]);

		for(y=0; y< 4;y++)
			sp[tempid].components[y]=atoi(row[58+y]);

		for(y=0; y< 4;y++)
			sp[tempid].component_counts[y]=atoi(row[62+y]);

		for(y=0; y< 4;y++)
			sp[tempid].NoexpendReagent[y]=atoi(row[66+y]);

		for(y=0; y< EFFECT_COUNT;y++)
			sp[tempid].formula[y]=atoi(row[70+y]);

		sp[tempid].goodEffect=atoi(row[83]);
		sp[tempid].Activated=atoi(row[84]);
		sp[tempid].resisttype=atoi(row[85]);

		for(y=0; y< EFFECT_COUNT;y++)
			sp[tempid].effectid[y]=atoi(row[86+y]);

		sp[tempid].targettype = (SpellTargetType) atoi(row[98]);
		sp[tempid].basediff=atoi(row[99]);

		int tmp_skill = atoi(row[100]);

		if(tmp_skill < 0 || tmp_skill > HIGHEST_SKILL)
            sp[tempid].skill = SkillBegging; /* not much better we can do. */ // can probably be changed to client-based 'SkillNone' once activated
        else
			sp[tempid].skill = (SkillUseTypes) tmp_skill;

		sp[tempid].zonetype=atoi(row[101]);
		sp[tempid].EnvironmentType=atoi(row[102]);
		sp[tempid].TimeOfDay=atoi(row[103]);

		for(y=0; y < PLAYER_CLASS_COUNT;y++)
			sp[tempid].classes[y]=atoi(row[104+y]);

		sp[tempid].CastingAnim=atoi(row[120]);
		sp[tempid].SpellAffectIndex=atoi(row[123]);
		sp[tempid].disallow_sit=atoi(row[124]);
		sp[tempid].diety_agnostic=atoi(row[125]);

		for (y = 0; y < 16; y++)
			sp[tempid].deities[y]=atoi(row[126+y]);

		sp[tempid].uninterruptable=atoi(row[146]) != 0;
		sp[tempid].ResistDiff=atoi(row[147]);
		sp[tempid].dot_stacking_exempt=atoi(row[148]);
		sp[tempid].RecourseLink = atoi(row[150]);
		sp[tempid].no_partial_resist = atoi(row[151]) != 0;

		sp[tempid].short_buff_box = atoi(row[154]);
		sp[tempid].descnum = atoi(row[155]);
		sp[tempid].effectdescnum = atoi(row[157]);

		sp[tempid].npc_no_los = atoi(row[159]) != 0;
		sp[tempid].reflectable = atoi(row[161]) != 0;
		sp[tempid].bonushate=atoi(row[162]);

		sp[tempid].EndurCost=atoi(row[166]);
		sp[tempid].EndurTimerIndex=atoi(row[167]);
		sp[tempid].IsDisciplineBuff = atoi(row[168]) != 0;
		sp[tempid].HateAdded=atoi(row[173]);
		sp[tempid].EndurUpkeep=atoi(row[174]);
		sp[tempid].numhitstype = atoi(row[175]);
		sp[tempid].numhits = atoi(row[176]);
		sp[tempid].pvpresistbase=atoi(row[177]);
		sp[tempid].pvpresistcalc=atoi(row[178]);
		sp[tempid].pvpresistcap=atoi(row[179]);
		sp[tempid].spell_category=atoi(row[180]);
		sp[tempid].can_mgb=atoi(row[185]);
		sp[tempid].dispel_flag = atoi(row[186]);
		sp[tempid].MinResist = atoi(row[189]);
		sp[tempid].MaxResist = atoi(row[190]);
		sp[tempid].viral_targets = atoi(row[191]);
		sp[tempid].viral_timer = atoi(row[192]);
		sp[tempid].NimbusEffect = atoi(row[193]);
		sp[tempid].directional_start = static_cast<float>(atoi(row[194]));
		sp[tempid].directional_end = static_cast<float>(atoi(row[195]));
		sp[tempid].sneak = atoi(row[196]) != 0;
		sp[tempid].not_extendable = atoi(row[197]) != 0;
		sp[tempid].suspendable = atoi(row[200]) != 0;
		sp[tempid].viral_range = atoi(row[201]);
		sp[tempid].no_block = atoi(row[205]);
		sp[tempid].spellgroup=atoi(row[207]);
		sp[tempid].rank = atoi(row[208]);
		sp[tempid].powerful_flag=atoi(row[209]);
		sp[tempid].CastRestriction = atoi(row[211]);
		sp[tempid].AllowRest = atoi(row[212]) != 0;
		sp[tempid].InCombat = atoi(row[213]) != 0;
		sp[tempid].OutofCombat = atoi(row[214]) != 0;
		sp[tempid].aemaxtargets = atoi(row[218]);
		sp[tempid].maxtargets = atoi(row[219]);
		sp[tempid].persistdeath = atoi(row[224]) != 0;
		sp[tempid].min_dist = atof(row[227]);
		sp[tempid].min_dist_mod = atof(row[228]);
		sp[tempid].max_dist = atof(row[229]);
		sp[tempid].max_dist_mod = atof(row[230]);
		sp[tempid].min_range = static_cast<float>(atoi(row[231]));
		sp[tempid].DamageShieldType = 0;
    }

    LoadDamageShieldTypes(sp, max_spells);
}

int SharedDatabase::GetMaxBaseDataLevel() {
	const std::string query = "SELECT MAX(level) FROM base_data";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return -1;
	}

	if (results.RowCount() == 0)
        return -1;

    auto row = results.begin();

	return atoi(row[0]);
}

bool SharedDatabase::LoadBaseData(const std::string &prefix) {
	base_data_mmf.reset(nullptr);

	try {
		EQEmu::IPCMutex mutex("base_data");
		mutex.Lock();

		std::string file_name = std::string("shared/") + prefix + std::string("base_data");
		base_data_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name));
		mutex.Unlock();
	} catch(std::exception& ex) {
		Log.Out(Logs::General, Logs::Error, "Error Loading Base Data: %s", ex.what());
		return false;
	}

	return true;
}

void SharedDatabase::LoadBaseData(void *data, int max_level) {
	char *base_ptr = reinterpret_cast<char*>(data);

	const std::string query = "SELECT * FROM base_data ORDER BY level, class ASC";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
        return;
	}

    int lvl = 0;
    int cl = 0;

    for (auto row = results.begin(); row != results.end(); ++row) {
        lvl = atoi(row[0]);
        cl = atoi(row[1]);

        if(lvl <= 0) {
            Log.Out(Logs::General, Logs::Error, "Non fatal error: base_data.level <= 0, ignoring.");
            continue;
        }

        if(lvl >= max_level) {
            Log.Out(Logs::General, Logs::Error, "Non fatal error: base_data.level >= max_level, ignoring.");
            continue;
        }

        if(cl <= 0) {
            Log.Out(Logs::General, Logs::Error, "Non fatal error: base_data.cl <= 0, ignoring.");
            continue;
        }

        if(cl > 16) {
            Log.Out(Logs::General, Logs::Error, "Non fatal error: base_data.class > 16, ignoring.");
            continue;
        }

        BaseDataStruct *bd = reinterpret_cast<BaseDataStruct*>(base_ptr + (((16 * (lvl - 1)) + (cl - 1)) * sizeof(BaseDataStruct)));
		bd->base_hp = atof(row[2]);
		bd->base_mana = atof(row[3]);
		bd->base_end = atof(row[4]);
		bd->unk1 = atof(row[5]);
		bd->unk2 = atof(row[6]);
		bd->hp_factor = atof(row[7]);
		bd->mana_factor = atof(row[8]);
		bd->endurance_factor = atof(row[9]);
    }
}

const BaseDataStruct* SharedDatabase::GetBaseData(int lvl, int cl) {
	if(!base_data_mmf) {
		return nullptr;
	}

	if(lvl <= 0) {
		return nullptr;
	}

	if(cl <= 0) {
		return nullptr;
	}

	if(cl > 16) {
		return nullptr;
	}

	char *base_ptr = reinterpret_cast<char*>(base_data_mmf->Get());

	uint32 offset = ((16 * (lvl - 1)) + (cl - 1)) * sizeof(BaseDataStruct);

	if(offset >= base_data_mmf->Size()) {
		return nullptr;
	}

	BaseDataStruct *bd = reinterpret_cast<BaseDataStruct*>(base_ptr + offset);
	return bd;
}

void SharedDatabase::GetLootTableInfo(uint32 &loot_table_count, uint32 &max_loot_table, uint32 &loot_table_entries) {
	loot_table_count = 0;
	max_loot_table = 0;
	loot_table_entries = 0;
	const std::string query = "SELECT COUNT(*), MAX(id), (SELECT COUNT(*) FROM loottable_entries) FROM loottable";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

	if (results.RowCount() == 0)
        return;

	auto row = results.begin();

    loot_table_count = static_cast<uint32>(atoul(row[0]));
	max_loot_table = static_cast<uint32>(atoul(row[1] ? row[1] : "0"));
	loot_table_entries = static_cast<uint32>(atoul(row[2]));
}

void SharedDatabase::GetLootDropInfo(uint32 &loot_drop_count, uint32 &max_loot_drop, uint32 &loot_drop_entries) {
	loot_drop_count = 0;
	max_loot_drop = 0;
	loot_drop_entries = 0;

	const std::string query = "SELECT COUNT(*), MAX(id), (SELECT COUNT(*) FROM lootdrop_entries) FROM lootdrop";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

	if (results.RowCount() == 0)
        return;

    auto row =results.begin();

    loot_drop_count = static_cast<uint32>(atoul(row[0]));
	max_loot_drop = static_cast<uint32>(atoul(row[1] ? row[1] : "0"));
	loot_drop_entries = static_cast<uint32>(atoul(row[2]));
}

void SharedDatabase::LoadLootTables(void *data, uint32 size) {
	EQEmu::FixedMemoryVariableHashSet<LootTable_Struct> hash(reinterpret_cast<uint8*>(data), size);

	uint8 loot_table[sizeof(LootTable_Struct) + (sizeof(LootTableEntries_Struct) * 128)];
	LootTable_Struct *lt = reinterpret_cast<LootTable_Struct*>(loot_table);

	const std::string query = "SELECT loottable.id, loottable.mincash, loottable.maxcash, loottable.avgcoin, "
                            "loottable_entries.lootdrop_id, loottable_entries.multiplier, loottable_entries.droplimit, "
                            "loottable_entries.mindrop, loottable_entries.probability, loottable_entries.multiplier_min FROM "
							"loottable LEFT JOIN loottable_entries ON loottable.id = loottable_entries.loottable_id ORDER BY id";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
        return;
    }

    uint32 current_id = 0;
    uint32 current_entry = 0;

    for (auto row = results.begin(); row != results.end(); ++row) {
        uint32 id = static_cast<uint32>(atoul(row[0]));
        if(id != current_id) {
            if(current_id != 0)
                hash.insert(current_id, loot_table, (sizeof(LootTable_Struct) + (sizeof(LootTableEntries_Struct) * lt->NumEntries)));

            memset(loot_table, 0, sizeof(LootTable_Struct) + (sizeof(LootTableEntries_Struct) * 128));
            current_entry = 0;
            current_id = id;
            lt->mincash = static_cast<uint32>(atoul(row[1]));
            lt->maxcash = static_cast<uint32>(atoul(row[2]));
            lt->avgcoin = static_cast<uint32>(atoul(row[3]));
        }

        if(current_entry > 128)
            continue;

        if(!row[4])
            continue;

        lt->Entries[current_entry].lootdrop_id = static_cast<uint32>(atoul(row[4]));
        lt->Entries[current_entry].multiplier = static_cast<uint8>(atoi(row[5]));
        lt->Entries[current_entry].droplimit = static_cast<uint8>(atoi(row[6]));
        lt->Entries[current_entry].mindrop = static_cast<uint8>(atoi(row[7]));
		lt->Entries[current_entry].probability = static_cast<float>(atof(row[8]));
		lt->Entries[current_entry].multiplier_min = static_cast<uint8>(atoi(row[9]));

        ++(lt->NumEntries);
        ++current_entry;
    }

    if(current_id != 0)
        hash.insert(current_id, loot_table, (sizeof(LootTable_Struct) + (sizeof(LootTableEntries_Struct) * lt->NumEntries)));

}

/*
C6262	Excessive stack usage
Function uses '16608' bytes of stack:  exceeds /analyze:stacksize '16384'. 
Consider moving some data to heap.	common	shareddb.cpp	1489
*/
void SharedDatabase::LoadLootDrops(void *data, uint32 size) {

	EQEmu::FixedMemoryVariableHashSet<LootDrop_Struct> hash(reinterpret_cast<uint8*>(data), size);
	uint8 loot_drop[sizeof(LootDrop_Struct) + (sizeof(LootDropEntries_Struct) * 1260)];
	LootDrop_Struct *ld = reinterpret_cast<LootDrop_Struct*>(loot_drop);

	const std::string query = "SELECT lootdrop.id, lootdrop_entries.item_id, lootdrop_entries.item_charges, "
                            "lootdrop_entries.equip_item, lootdrop_entries.chance, lootdrop_entries.minlevel, "
                            "lootdrop_entries.maxlevel, lootdrop_entries.multiplier FROM lootdrop JOIN lootdrop_entries "
                            "ON lootdrop.id = lootdrop_entries.lootdrop_id ORDER BY lootdrop_id";
    auto results = QueryDatabase(query);
    if (!results.Success()) {
    }

    uint32 current_id = 0;
    uint32 current_entry = 0;

    for (auto row = results.begin(); row != results.end(); ++row) {
        uint32 id = static_cast<uint32>(atoul(row[0]));
        if(id != current_id) {
            if(current_id != 0)
                hash.insert(current_id, loot_drop, (sizeof(LootDrop_Struct) +(sizeof(LootDropEntries_Struct) * ld->NumEntries)));

            memset(loot_drop, 0, sizeof(LootDrop_Struct) + (sizeof(LootDropEntries_Struct) * 1260));
			current_entry = 0;
			current_id = id;
        }

		if(current_entry >= 1260)
            continue;

        ld->Entries[current_entry].item_id = static_cast<uint32>(atoul(row[1]));
        ld->Entries[current_entry].item_charges = static_cast<int8>(atoi(row[2]));
        ld->Entries[current_entry].equip_item = static_cast<uint8>(atoi(row[3]));
        ld->Entries[current_entry].chance = static_cast<float>(atof(row[4]));
        ld->Entries[current_entry].minlevel = static_cast<uint8>(atoi(row[5]));
        ld->Entries[current_entry].maxlevel = static_cast<uint8>(atoi(row[6]));
        ld->Entries[current_entry].multiplier = static_cast<uint8>(atoi(row[7]));

        ++(ld->NumEntries);
        ++current_entry;
    }

    if(current_id != 0)
        hash.insert(current_id, loot_drop, (sizeof(LootDrop_Struct) + (sizeof(LootDropEntries_Struct) * ld->NumEntries)));

}

bool SharedDatabase::LoadLoot(const std::string &prefix) {
	loot_table_mmf.reset(nullptr);
	loot_drop_mmf.reset(nullptr);

	try {
		EQEmu::IPCMutex mutex("loot");
		mutex.Lock();
		std::string file_name_lt = std::string("shared/") + prefix + std::string("loot_table");
		loot_table_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name_lt));
		loot_table_hash = std::unique_ptr<EQEmu::FixedMemoryVariableHashSet<LootTable_Struct>>(new EQEmu::FixedMemoryVariableHashSet<LootTable_Struct>(
			reinterpret_cast<uint8*>(loot_table_mmf->Get()),
			loot_table_mmf->Size()));
		std::string file_name_ld = std::string("shared/") + prefix + std::string("loot_drop");
		loot_drop_mmf = std::unique_ptr<EQEmu::MemoryMappedFile>(new EQEmu::MemoryMappedFile(file_name_ld));
		loot_drop_hash = std::unique_ptr<EQEmu::FixedMemoryVariableHashSet<LootDrop_Struct>>(new EQEmu::FixedMemoryVariableHashSet<LootDrop_Struct>(
			reinterpret_cast<uint8*>(loot_drop_mmf->Get()),
			loot_drop_mmf->Size()));
		mutex.Unlock();
	} catch(std::exception &ex) {
		Log.Out(Logs::General, Logs::Error, "Error loading loot: %s", ex.what());
		return false;
	}

	return true;
}

const LootTable_Struct* SharedDatabase::GetLootTable(uint32 loottable_id) {
	if(!loot_table_hash)
		return nullptr;

	try {
		if(loot_table_hash->exists(loottable_id)) {
			return &loot_table_hash->at(loottable_id);
		}
	} catch(std::exception &ex) {
		Log.Out(Logs::General, Logs::Error, "Could not get loot table: %s", ex.what());
	}
	return nullptr;
}

const LootDrop_Struct* SharedDatabase::GetLootDrop(uint32 lootdrop_id) {
	if(!loot_drop_hash)
		return nullptr;

	try {
		if(loot_drop_hash->exists(lootdrop_id)) {
			return &loot_drop_hash->at(lootdrop_id);
		}
	} catch(std::exception &ex) {
		Log.Out(Logs::General, Logs::Error, "Could not get loot drop: %s", ex.what());
	}
	return nullptr;
}

bool SharedDatabase::VerifyToken(std::string token, int& status) {
	status = 0;
	if (token.length() > 64) 
	{
		token = token.substr(0, 64);
	}

	token = EscapeString(token);

	std::string query = StringFormat("SELECT status FROM tokens WHERE token='%s'", token.c_str());
	auto results = QueryDatabase(query);

	if (!results.Success() || results.RowCount() == 0)
	{
		std::cerr << "Error in SharedDatabase::VerifyToken" << std::endl;
	}

	auto row = results.begin();

	status = atoi(row[0]);

	return results.Success();
}
