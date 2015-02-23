/*
EQEMu:  Everquest Server Emulator

Copyright (C) 2001-2014 EQEMu Development Team (http://eqemulator.net)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY except by those people which sell it, which
are required to give you total support for your newly bought product;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "eq_dictionary.h"
#include "string_util.h"

//
// class EmuConstants
//
uint16 EmuConstants::InventoryMapSize(int16 map) {
	switch (map) {
	case MapPossessions:
		return MAP_POSSESSIONS_SIZE;
	case MapBank:
		return MAP_BANK_SIZE;
	case MapSharedBank:
		return MAP_SHARED_BANK_SIZE;
	case MapTrade:
		return MAP_TRADE_SIZE;
	case MapWorld:
		return MAP_WORLD_SIZE;
	case MapLimbo:
		return MAP_LIMBO_SIZE;
	case MapMerchant:
		return MAP_MERCHANT_SIZE;
	case MapDeleted:
		return MAP_DELETED_SIZE;
	case MapCorpse:
		return MAP_CORPSE_SIZE;
	case MapBazaar:
		return MAP_BAZAAR_SIZE;
	case MapInspect:
		return MAP_INSPECT_SIZE;
	case MapRealEstate:
		return MAP_REAL_ESTATE_SIZE;
	case MapViewMODPC:
		return MAP_VIEW_MOD_PC_SIZE;
	case MapViewMODBank:
		return MAP_VIEW_MOD_BANK_SIZE;
	case MapViewMODSharedBank:
		return MAP_VIEW_MOD_SHARED_BANK_SIZE;
	case MapViewMODLimbo:
		return MAP_VIEW_MOD_LIMBO_SIZE;
	case MapAltStorage:
		return MAP_ALT_STORAGE_SIZE;
	case MapArchived:
		return MAP_ARCHIVED_SIZE;
	case MapMail:
		return MAP_MAIL_SIZE;
	case MapOther:
		return MAP_OTHER_SIZE;
	default:
		return NOT_USED;
	}
}

/*
std::string EmuConstants::InventoryLocationName(Location_Struct location) {
	// not ready for implementation...
	std::string ret_str;
	StringFormat(ret_str, "%s, %s, %s, %s", InventoryMapName(location.map), InventoryMainName(location.main), InventorySubName(location.sub), InventoryAugName(location.aug));
	return  ret_str;
}
*/

std::string EmuConstants::InventoryMapName(int16 map) {
	switch (map) {
	case INVALID_INDEX:
		return "Invalid Map";
	case MapPossessions:
		return "Possessions";
	case MapBank:
		return "Bank";
	case MapSharedBank:
		return "Shared Bank";
	case MapTrade:
		return "Trade";
	case MapWorld:
		return "World";
	case MapLimbo:
		return "Limbo";
	case MapTribute:
		return "Tribute";
	case MapTrophyTribute:
		return "Trophy Tribute";
	case MapGuildTribute:
		return "Guild Tribute";
	case MapMerchant:
		return "Merchant";
	case MapDeleted:
		return "Deleted";
	case MapCorpse:
		return "Corpse";
	case MapBazaar:
		return "Bazaar";
	case MapInspect:
		return "Inspect";
	case MapRealEstate:
		return "Real Estate";
	case MapViewMODPC:
		return "View MOD PC";
	case MapViewMODBank:
		return "View MOD Bank";
	case MapViewMODSharedBank:
		return "View MOD Shared Bank";
	case MapViewMODLimbo:
		return "View MOD Limbo";
	case MapAltStorage:
		return "Alt Storage";
	case MapArchived:
		return "Archived";
	case MapMail:
		return "Mail";
	case MapGuildTrophyTribute:
		return "Guild Trophy Tribute";
	case MapKrono:
		return "Krono";
	case MapOther:
		return "Other";
	default:
		return "Unknown Map";
	}
}

std::string EmuConstants::InventoryMainName(int16 main) {
	switch (main) {
	case INVALID_INDEX:
		return "Invalid Main";
	//case MainCursor:
	//	return "Charm";
	case MainEar1:
		return "Ear 1";
	case MainHead:
		return "Head";
	case MainFace:
		return "Face";
	case MainEar2:
		return "Ear 2";
	case MainNeck:
		return "Neck";
	case MainShoulders:
		return "Shoulders";
	case MainArms:
		return "Arms";
	case MainBack:
		return "Back";
	case MainWrist1:
		return "Wrist 1";
	case MainWrist2:
		return "Wrist 2";
	case MainRange:
		return "Range";
	case MainHands:
		return "Hands";
	case MainPrimary:
		return "Primary";
	case MainSecondary:
		return "Secondary";
	case MainFinger1:
		return "Finger 1";
	case MainFinger2:
		return "Finger 2";
	case MainChest:
		return "Chest";
	case MainLegs:
		return "Legs";
	case MainFeet:
		return "Feet";
	case MainWaist:
		return "Waist";
	case MainPowerSource:
		return "Power Source";
	case MainAmmo:
		return "Ammo";
	case MainGeneral1:
		return "General 1";
	case MainGeneral2:
		return "General 2";
	case MainGeneral3:
		return "General 3";
	case MainGeneral4:
		return "General 4";
	case MainGeneral5:
		return "General 5";
	case MainGeneral6:
		return "General 6";
	case MainGeneral7:
		return "General 7";
	case MainGeneral8:
		return "General 8";
	/*
	case MainGeneral9:
		return "General 9";
	case MainGeneral10:
		return "General 10";
	*/
	case MainCursor:
		return "Cursor";
	default:
		return "Unknown Main";
	}
}

std::string EmuConstants::InventorySubName(int16 sub) {
	if (sub == INVALID_INDEX)
		return "Invalid Sub";

	if ((uint16)sub >= ITEM_CONTAINER_SIZE)
		return "Unknown Sub";

	std::string ret_str;
	ret_str = StringFormat("Container %i", (sub + 1)); // zero-based index..but, count starts at one

	return ret_str;
}

// legacy-related functions
//
// these should work for the first-stage coversions..but, once the new system is up and going..conversions will be incompatible
// without limiting server functions and other aspects, such as augment control, bag size, etc. A complete remapping will be
// required when bag sizes over 10 and direct manipulation of augments are implemented, due to the massive increase in range usage.
//
// (current personal/cursor bag range {251..340}, or 90 slots ... conversion translated range would be {10000..12804}, or 2805 slots...
// these would have to be hard-coded into all perl code to allow access to full range of the new system - actual limits would be
// less, based on bag size..but, the range must be full and consistent to avoid translation issues -- similar changes to other ranges
// (240 versus 6120 slots for bank bags, for example...))
/*
int EmuConstants::ServerToPerlSlot(int server_slot) { // set r/s
	switch (server_slot) {
	case legacy::MainCursor_END:
		return legacy::MainCursor_END;
	case INVALID_INDEX:
		return legacy::INVALID_INDEX;
	case MainPowerSource:
		return legacy::SLOT_POWER_SOURCE;
	case MainAmmo:
		return legacy::SLOT_AMMO;
	case MainCursor:
		return legacy::MainCursor;
	case legacy::SLOT_TRADESKILL:
		return legacy::SLOT_TRADESKILL;
	case legacy::SLOT_AUGMENT:
		return legacy::SLOT_AUGMENT;
	default:
		int perl_slot = legacy::INVALID_INDEX;

		// activate the internal checks as needed
		if (server_slot >= EmuConstants::EQUIPMENT_BEGIN && server_slot <= MainWaist) {
			perl_slot = server_slot;
		}
		else if (server_slot >= EmuConstants::GENERAL_BEGIN && server_slot <= EmuConstants::GENERAL_END) {
			perl_slot = server_slot;// + legacy::SLOT_PERSONAL_BEGIN - EmuConstants::GENERAL_BEGIN;
			//if (perl_slot < legacy::SLOT_PERSONAL_BEGIN || perl_slot > legacy::SLOT_PERSONAL_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::GENERAL_BAGS_BEGIN && server_slot <= EmuConstants::GENERAL_BAGS_END) {
			perl_slot = server_slot;// + legacy::SLOT_PERSONAL_BAGS_BEGIN - EmuConstants::GENERAL_BAGS_BEGIN;
			//if (perl_slot < legacy::SLOT_PERSONAL_BAGS_BEGIN || perl_slot > legacy::SLOT_PERSONAL_BAGS_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::CURSOR_BAG_BEGIN && server_slot <= EmuConstants::CURSOR_BAG_END) {
			perl_slot = server_slot;// + legacy::MainCursor_BAG_BEGIN - EmuConstants::CURSOR_BAG_BEGIN;
			//if (perl_slot < legacy::MainCursor_BAG_BEGIN || perl_slot > legacy::MainCursor_BAG_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::TRIBUTE_BEGIN && server_slot <= EmuConstants::TRIBUTE_END) {
			perl_slot = server_slot;// + legacy::SLOT_TRIBUTE_BEGIN - EmuConstants::TRADE_BEGIN;
			//if (perl_slot < legacy::SLOT_TRIBUTE_BEGIN || perl_slot > legacy::SLOT_TRIBUTE_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::BANK_BEGIN && server_slot <= EmuConstants::BANK_END) {
			perl_slot = server_slot;// + legacy::SLOT_BANK_BEGIN - EmuConstants::BANK_BEGIN;
			//if (perl_slot < legacy::SLOT_BANK_BEGIN || perl_slot > legacy::SLOT_BANK_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::BANK_BAGS_BEGIN && server_slot <= EmuConstants::BANK_BAGS_END) {
			perl_slot = server_slot;// + legacy::SLOT_BANK_BAGS_BEGIN - EmuConstants::BANK_BAGS_BEGIN;
			//if (perl_slot < legacy::SLOT_BANK_BAGS_BEGIN || perl_slot > legacy::SLOT_BANK_BAGS_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::SHARED_BANK_BEGIN && server_slot <= EmuConstants::SHARED_BANK_END) {
			perl_slot = server_slot;// + legacy::SLOT_SHARED_BANK_BEGIN - EmuConstants::SHARED_BANK_BEGIN;
			//if (perl_slot < legacy::SLOT_SHARED_BANK_BEGIN || perl_slot > legacy::SLOT_SHARED_BANK_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::SHARED_BANK_BAGS_BEGIN && server_slot <= EmuConstants::SHARED_BANK_BAGS_END) {
			perl_slot = server_slot;// + legacy::SLOT_SHARED_BANK_BAGS_BEGIN - EmuConstants::SHARED_BANK_BAGS_BEGIN;
			//if (perl_slot < legacy::SLOT_SHARED_BANK_BAGS_BEGIN || perl_slot > legacy::SLOT_SHARED_BANK_BAGS_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::TRADE_BEGIN && server_slot <= EmuConstants::TRADE_END) {
			perl_slot = server_slot;// + legacy::SLOT_TRADE_BEGIN - EmuConstants::TRADE_BEGIN;
			//if (perl_slot < legacy::SLOT_TRADE_BEGIN || perl_slot > legacy::SLOT_TRADE_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::TRADE_BAGS_BEGIN && server_slot <= EmuConstants::TRADE_BAGS_END) {
			perl_slot = server_slot;// + legacy::SLOT_TRADE_BAGS_BEGIN - EmuConstants::TRADE_BAGS_BEGIN;
			//if (perl_slot < legacy::SLOT_TRADE_BAGS_BEGIN || perl_slot > legacy::SLOT_TRADE_BAGS_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= EmuConstants::WORLD_BEGIN && server_slot <= EmuConstants::WORLD_END) {
			perl_slot = server_slot;// + legacy::SLOT_WORLD_BEGIN - EmuConstants::WORLD_BEGIN;
			//if (perl_slot < legacy::SLOT_WORLD_BEGIN || perl_slot > legacy::SLOT_WORLD_END)
			//	perl_slot = legacy::INVALID_INDEX;
		}
		else if (server_slot >= 8000 && server_slot <= 8999) { // this range may be limited to 36 in the future (client size)
			perl_slot = server_slot;
		}

		return perl_slot;
	}
}
*/
/*
int EmuConstants::PerlToServerSlot(int perl_slot) { // set r/s
	switch (perl_slot) {
	case legacy::MainCursor_END:
		return legacy::MainCursor_END;
	case legacy::INVALID_INDEX:
		return INVALID_INDEX;
	case legacy::SLOT_POWER_SOURCE:
		return MainPowerSource;
	case legacy::SLOT_AMMO:
		return MainAmmo;
	case legacy::MainCursor:
		return MainCursor;
	case legacy::SLOT_TRADESKILL:
		return legacy::SLOT_TRADESKILL;
	case legacy::SLOT_AUGMENT:
		return legacy::SLOT_AUGMENT;
	default:
		int server_slot = INVALID_INDEX;

		// activate the internal checks as needed
		if (perl_slot >= legacy::SLOT_EQUIPMENT_BEGIN && perl_slot <= legacy::SLOT_WAIST) {
			server_slot = perl_slot;
		}
		else if (perl_slot >= legacy::SLOT_PERSONAL_BEGIN && perl_slot <= legacy::SLOT_PERSONAL_END) {
			server_slot = perl_slot;// + EmuConstants::GENERAL_BEGIN - legacy::SLOT_PERSONAL_BEGIN;
			//if (server_slot < EmuConstants::GENERAL_BEGIN || server_slot > EmuConstants::GENERAL_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_PERSONAL_BAGS_BEGIN && perl_slot <= legacy::SLOT_PERSONAL_BAGS_END) {
			server_slot = perl_slot;// + EmuConstants::GENERAL_BAGS_BEGIN - legacy::SLOT_PERSONAL_BAGS_BEGIN;
			//if (server_slot < EmuConstants::GENERAL_BAGS_BEGIN || server_slot > EmuConstants::GENERAL_BAGS_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::MainCursor_BAG_BEGIN && perl_slot <= legacy::MainCursor_BAG_END) {
			server_slot = perl_slot;// + EmuConstants::CURSOR_BAG_BEGIN - legacy::MainCursor_BAG_BEGIN;
			//if (server_slot < EmuConstants::CURSOR_BAG_BEGIN || server_slot > EmuConstants::CURSOR_BAG_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_TRIBUTE_BEGIN && perl_slot <= legacy::SLOT_TRIBUTE_END) {
			server_slot = perl_slot;// + EmuConstants::TRIBUTE_BEGIN - legacy::SLOT_TRIBUTE_BEGIN;
			//if (server_slot < EmuConstants::TRIBUTE_BEGIN || server_slot > EmuConstants::TRIBUTE_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_BANK_BEGIN && perl_slot <= legacy::SLOT_BANK_END) {
			server_slot = perl_slot;// + EmuConstants::BANK_BEGIN - legacy::SLOT_BANK_BEGIN;
			//if (server_slot < EmuConstants::BANK_BEGIN || server_slot > EmuConstants::BANK_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_BANK_BAGS_BEGIN && perl_slot <= legacy::SLOT_BANK_BAGS_END) {
			server_slot = perl_slot;// + EmuConstants::BANK_BAGS_BEGIN - legacy::SLOT_BANK_BAGS_BEGIN;
			//if (server_slot < EmuConstants::BANK_BAGS_BEGIN || server_slot > EmuConstants::BANK_BAGS_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_SHARED_BANK_BEGIN && perl_slot <= legacy::SLOT_SHARED_BANK_END) {
			server_slot = perl_slot;// + EmuConstants::SHARED_BANK_BEGIN - legacy::SLOT_SHARED_BANK_BEGIN;
			//if (server_slot < EmuConstants::SHARED_BANK_BEGIN || server_slot > EmuConstants::SHARED_BANK_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_SHARED_BANK_BAGS_BEGIN && perl_slot <= legacy::SLOT_SHARED_BANK_BAGS_END) {
			server_slot = perl_slot;// + EmuConstants::SHARED_BANK_BAGS_BEGIN - legacy::SLOT_SHARED_BANK_BAGS_END;
			//if (server_slot < EmuConstants::SHARED_BANK_BAGS_BEGIN || server_slot > EmuConstants::SHARED_BANK_BAGS_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_TRADE_BEGIN && perl_slot <= legacy::SLOT_TRADE_END) {
			server_slot = perl_slot;// + EmuConstants::TRADE_BEGIN - legacy::SLOT_TRADE_BEGIN;
			//if (server_slot < EmuConstants::TRADE_BEGIN || server_slot > EmuConstants::TRADE_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_TRADE_BAGS_BEGIN && perl_slot <= legacy::SLOT_TRADE_BAGS_END) {
			server_slot = perl_slot;// + EmuConstants::TRADE_BAGS_BEGIN - legacy::SLOT_TRADE_BAGS_BEGIN;
			//if (server_slot < EmuConstants::TRADE_BAGS_BEGIN || server_slot > EmuConstants::TRADE_BAGS_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= legacy::SLOT_WORLD_BEGIN && perl_slot <= legacy::SLOT_WORLD_END) {
			server_slot = perl_slot;// + EmuConstants::WORLD_BEGIN - legacy::SLOT_WORLD_BEGIN;
			//if (server_slot < EmuConstants::WORLD_BEGIN || server_slot > EmuConstants::WORLD_END)
			//	server_slot = INVALID_INDEX;
		}
		else if (perl_slot >= 8000 && perl_slot <= 8999) { // this range may be limited to 36 in the future (client size)
			server_slot = perl_slot;
		}

		return server_slot;
	}
}
*/

// 
// class EQLimits
//
// client validation
bool EQLimits::IsValidClientVersion(uint32 version) {
	if (version < _EQClientCount)
		return true;

	return false;
}

uint32 EQLimits::ValidateClientVersion(uint32 version) {
	if (version < _EQClientCount)
		return version;

	return EQClientUnknown;
}

EQClientVersion EQLimits::ValidateClientVersion(EQClientVersion version) {
	if (version >= EQClientUnknown)
		if (version < _EQClientCount)
			return version;

	return EQClientUnknown;
}

// npc validation
bool EQLimits::IsValidNPCVersion(uint32 version) {
	if (version >= _EQClientCount)
		if (version < _EmuClientCount)
			return true;

	return false;
}

uint32 EQLimits::ValidateNPCVersion(uint32 version) {
	if (version >= _EQClientCount)
		if (version < _EmuClientCount)
			return version;

	return EQClientUnknown;
}

EQClientVersion EQLimits::ValidateNPCVersion(EQClientVersion version) {
	if (version >= _EQClientCount)
		if (version < _EmuClientCount)
			return version;

	return EQClientUnknown;
}

// mob validation
bool EQLimits::IsValidMobVersion(uint32 version) {
	if (version < _EmuClientCount)
		return true;

	return false;
}

uint32 EQLimits::ValidateMobVersion(uint32 version) {
	if (version < _EmuClientCount)
		return version;

	return EQClientUnknown;
}

EQClientVersion EQLimits::ValidateMobVersion(EQClientVersion version) {
	if (version >= EQClientUnknown)
		if (version < _EmuClientCount)
			return version;

	return EQClientUnknown;
}

// inventory
uint16 EQLimits::InventoryMapSize(int16 map, uint32 version) {
	// not all maps will have an instantiated container..some are references for queue generators (i.e., bazaar, mail, etc...)
	// a zero '0' indicates a needed value..otherwise, change to '_NOTUSED' for a null value so indices requiring research can be identified
	// ALL of these values need to be verified before pushing to live
	//
	// make sure that you transcribe the actual value from 'defaults' to here before updating or client crashes will ensue..and/or...
	// insert older clients inside of the progression of client order
	//
	// MAP_POSSESSIONS_SIZE does not reflect all actual <client>_constants size due to bitmask-use compatibility
	//
	// when setting NPC-based values, try to adhere to an EmuConstants::<property> or NOT_USED value to avoid unnecessary issues

	static const uint16 local[_MapCount][_EmuClientCount] = {
		// server and database are sync'd to current MapPossessions's client as set in 'using namespace RoF::slots;' and
		// 'EmuConstants::MAP_POSSESSIONS_SIZE' - use/update EquipmentBitmask(), GeneralBitmask() and CursorBitmask()
		// for partial range validation checks and 'EmuConstants::MAP_POSSESSIONS_SIZE' for full range iterations
		{ // local[MainPossessions]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			EmuConstants::MAP_POSSESSIONS_SIZE,
/*evolution*/	0,


/*NPC*/			EmuConstants::MAP_POSSESSIONS_SIZE,
/*Pet*/			EmuConstants::MAP_POSSESSIONS_SIZE
		},
		{ // local[MapBank]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			MAC::consts::MAP_BANK_SIZE,
/*evolution*/	0,

/*NPC*/			NOT_USED,
/*Pet*/			NOT_USED
		},
		{ // local[MapSharedBank]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	NOT_USED,

/*NPC*/			NOT_USED,
/*Pet*/			NOT_USED
		},
		{ // local[MapTrade]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			EmuConstants::MAP_TRADE_SIZE,
/*evolution*/	0,

/*NPC*/			4,
/*Pet*/			4
		},
		{ // local[MapWorld]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			EmuConstants::MAP_WORLD_SIZE,
/*evolution*/	NOT_USED,

/*NPC*/			NOT_USED,
/*Pet*/			NOT_USED
		},
		{ // local[MapLimbo]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			EmuConstants::MAP_LIMBO_SIZE,
/*evolution*/	NOT_USED,

/*NPC*/			NOT_USED,
/*Pet*/			NOT_USED
		},
		{ // local[MapTribute]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	NOT_USED,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapTrophyTribute]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	NOT_USED,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapGuildTribute]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	NOT_USED,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapMerchant]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapDeleted]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapCorpse]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			MAC::consts::MAP_CORPSE_SIZE,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapBazaar]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			EmuConstants::MAP_BAZAAR_SIZE,
/*evolution*/	0,

/*NPC*/			0, // this may need to be 'EmuConstants::MAP_BAZAAR_SIZE' if offline client traders respawn as an npc
/*Pet*/			0
		},
		{ // local[MapInspect]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			MAC::consts::MAP_INSPECT_SIZE,
/*evolution*/	0,

/*NPC*/			NOT_USED,
/*Pet*/			NOT_USED
		},
		{ // local[MapRealEstate]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapViewMODPC]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapViewMODBank]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapViewMODSharedBank]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapViewMODLimbo]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapAltStorage]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapArchived]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapMail]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapGuildTrophyTribute]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapKrono]
/*Unknown*/		NOT_USED,
/*Unused*/		NOT_USED,
/*Mac*/			NOT_USED,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		},
		{ // local[MapOther]
/*Unknown*/		NOT_USED,
/*Unused*/		0,
/*Mac*/			0,
/*evolution*/	0,

/*NPC*/			0,
/*Pet*/			0
		}
	};

	if ((uint16)map < _MapCount)
		return local[map][ValidateMobVersion(version)];

	return NOT_USED;
}

uint64 EQLimits::PossessionsBitmask(uint32 version) {
	// these are for the new inventory system (RoF)..not the current (Ti) one...
	// 0x0000000000200000 is SlotPowerSource (SoF+)
	// 0x0000000080000000 is SlotGeneral9 (RoF+)
	// 0x0000000100000000 is SlotGeneral10 (RoF+)

	static const uint64 local[_EmuClientCount] = {
		/*Unknown*/		NOT_USED,
		/*Unused*/		0,
		/*Mac*/			0x000000027FDFFFFF,
		/*Evolution*/	0,

		/*NPC*/			0,
		/*Pet*/			0
	};

	return NOT_USED;
	//return local[ValidateMobVersion(version)];
}

uint64 EQLimits::EquipmentBitmask(uint32 version) {
	static const uint64 local[_EmuClientCount] = {
		/*Unknown*/		NOT_USED,
		/*Unused*/		0x00000000005FFFFF,
		/*Mac*/			0x00000000005FFFFF,
		/*Evolution*/	0x00000000005FFFFF,

		/*NPC*/			0,
		/*Pet*/			0
	};

	return NOT_USED;
	//return local[ValidateMobVersion(version)];
}

uint64 EQLimits::GeneralBitmask(uint32 version) {
	static const uint64 local[_EmuClientCount] = {
		/*Unknown*/		NOT_USED,
		/*Unused*/		0,
		/*Mac*/			0x000000007F800000,
		/*Evolution*/	0,

		/*NPC*/			0,
		/*Pet*/			0
	};

	return NOT_USED;
	//return local[ValidateMobVersion(version)];
}

uint64 EQLimits::CursorBitmask(uint32 version) {
	static const uint64 local[_EmuClientCount] = {
		/*Unknown*/		NOT_USED,
		/*Unused*/		0,
		/*Mac*/			0x0000000200000000,
		/*Evolution*/	0,

		/*NPC*/			0,
		/*Pet*/			0
	};

	return NOT_USED;
	//return local[ValidateMobVersion(version)];
}

bool EQLimits::AllowsEmptyBagInBag(uint32 version) {
	static const bool local[_EmuClientCount] = {
		/*Unknown*/		false,
		/*Unused*/		false,
		/*Mac*/			MAC::limits::ALLOWS_EMPTY_BAG_IN_BAG,
		/*Evolution*/	false,

		/*NPC*/			false,
		/*Pet*/			false
	};

	return false; // not implemented
	//return local[ValidateMobVersion(version)];
}

bool EQLimits::AllowsClickCastFromBag(uint32 version) {
	static const bool local[_EmuClientCount] = {
/*Unknown*/		false,
/*Unused*/		false,
/*Mac*/			MAC::limits::ALLOWS_CLICK_CAST_FROM_BAG,
/*Evolution*/	false,

/*NPC*/			false,
/*Pet*/			false
	};

	return local[ValidateMobVersion(version)];
}

// items
uint16 EQLimits::ItemCommonSize(uint32 version) {
	static const uint16 local[_EmuClientCount] = {
		/*Unknown*/		NOT_USED,
		/*Unused*/		0,
		/*Mac*/			EmuConstants::ITEM_COMMON_SIZE,
		/*Evolution*/	0,

		/*NPC*/			EmuConstants::ITEM_COMMON_SIZE,
		/*Pet*/			EmuConstants::ITEM_COMMON_SIZE
	};

	return local[ValidateMobVersion(version)];
}

uint16 EQLimits::ItemContainerSize(uint32 version) {
	static const uint16 local[_EmuClientCount] = {
		/*Unknown*/		NOT_USED,
		/*Unused*/		0,
		/*Mac*/			EmuConstants::ITEM_CONTAINER_SIZE,
		/*Evolution*/	0,

		/*NPC*/			EmuConstants::ITEM_CONTAINER_SIZE,
		/*Pet*/			EmuConstants::ITEM_CONTAINER_SIZE
	};

	return local[ValidateMobVersion(version)];
}

bool EQLimits::CoinHasWeight(uint32 version) {
	static const bool local[_EmuClientCount] = {
		/*Unknown*/		true,
		/*Unused*/		true,
		/*Mac*/			MAC::limits::COIN_HAS_WEIGHT,
		/*Evolution*/	true,

		/*NPC*/			true,
		/*Pet*/			true
	};

	return local[ValidateMobVersion(version)];
}

