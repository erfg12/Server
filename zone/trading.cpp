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
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "../common/string_util.h"

#include "client.h"
#include "entity.h"
#include "mob.h"

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"

class QueryServ;

extern WorldServer worldserver;
extern QueryServ* QServ;

// The maximum amount of a single bazaar transaction expressed in copper.
// Equivalent to 2 Million plat
#define MAX_TRANSACTION_VALUE 2000000000
// ##########################################
// Trade implementation
// ##########################################

Trade::Trade(Mob* in_owner)
{
	owner = in_owner;
	Reset();
}

Trade::~Trade()
{
	Reset();
}

void Trade::Reset()
{
	state = TradeNone;
	with_id = 0;
	pp=0; gp=0; sp=0; cp=0;
}

void Trade::SetTradeCash(uint32 in_pp, uint32 in_gp, uint32 in_sp, uint32 in_cp)
{
	pp=in_pp; gp=in_gp; sp=in_sp; cp=in_cp;
}

// Initiate a trade with another mob
// initiate_with specifies whether to start trade with other mob as well
void Trade::Start(uint32 mob_id, bool initiate_with)
{
	Reset();
	state = Trading;
	with_id = mob_id;

	// Autostart on other mob?
	if (initiate_with) {
		Mob* with = With();
		if (with)
			with->trade->Start(owner->GetID(), false);
	}
}

// Add item from a given slot to trade bucket (automatically does bag data too)
void Trade::AddEntity(uint16 trade_slot_id, uint32 stack_size) {
	// TODO: review for inventory saves / consider changing return type to bool so failure can be passed to desync handler

	if (!owner || !owner->IsClient()) {
		// This should never happen
		Log.Out(Logs::General, Logs::Trading, "Programming error: NPC's should not call Trade::AddEntity()");
		return;
	}

	// If one party accepted the trade then an item was added, their state needs to be reset
	owner->trade->state = Trading;
	Mob* with = With();
	if (with)
		with->trade->state = Trading;

	// Item always goes into trade bucket from cursor
	Client* client = owner->CastToClient();
	ItemInst* inst = client->GetInv().GetItem(MainCursor);

	if (!inst) {
		client->Message(CC_Red, "Error: Could not find item on your cursor!");
		return;
	}

	ItemInst* inst2 = client->GetInv().GetItem(trade_slot_id);

	// it looks like the original code attempted to allow stacking...
	// (it just didn't handle partial stack move actions -U)
	if (stack_size > 0) {
		if (!inst->IsStackable() || !inst2 || !inst2->GetItem() || (inst->GetID() != inst2->GetID()) || (stack_size > inst->GetCharges())) {
			client->Kick();
			return;
		}

		uint32 _stack_size = 0;

		if ((stack_size + inst2->GetCharges()) > inst2->GetItem()->StackSize) {
			_stack_size = (stack_size + inst2->GetCharges()) - inst->GetItem()->StackSize;
			inst2->SetCharges(inst2->GetItem()->StackSize);
		}
		else {
			_stack_size = inst->GetCharges() - stack_size;
			inst2->SetCharges(stack_size + inst2->GetCharges());
		}

		Log.Out(Logs::Detail, Logs::Trading, "%s added partial item '%s' stack (qty: %i) to trade slot %i", owner->GetName(), inst->GetItem()->Name, stack_size, trade_slot_id);

		if (_stack_size > 0)
			inst->SetCharges(_stack_size);
		else
			client->DeleteItemInInventory(MainCursor);

		SendItemData(inst2, trade_slot_id);
	}
	else {
		if (inst2 && inst2->GetID()) {
			client->Kick();
			return;
		}

		SendItemData(inst, trade_slot_id);

		Log.Out(Logs::Detail, Logs::Trading, "%s added item '%s' to trade slot %i", owner->GetName(), inst->GetItem()->Name, trade_slot_id);

		client->PutItemInInventory(trade_slot_id, *inst);
		client->DeleteItemInInventory(MainCursor);
	}
}

// Retrieve mob the owner is trading with
// Done like this in case 'with' mob goes LD and Mob* becomes invalid
Mob* Trade::With()
{
	return entity_list.GetMob(with_id);
}

// Private Method: Send item data for trade item to other person involved in trade
void Trade::SendItemData(const ItemInst* inst, int16 dest_slot_id)
{
	if (inst == nullptr)
		return;
	
	// @merth: This needs to be redone with new item classes
	Mob* mob = With();
	if (!mob->IsClient())
		return; // Not sending packets to NPCs!

	Client* with = mob->CastToClient();
	Client* trader = owner->CastToClient();
	if (with && with->IsClient()) {
		int16 fromid = 0;
		fromid = trader->GetID();
		with->SendItemPacket(dest_slot_id -IDX_TRADE,inst,ItemPacketTradeView,fromid);
		if (inst->GetItem()->ItemClass == 1) {
			for (uint16 i = SUB_BEGIN; i < EmuConstants::ITEM_CONTAINER_SIZE; i++) {
				uint16 bagslot_id = Inventory::CalcSlotId(dest_slot_id, i);
				const ItemInst* bagitem = trader->GetInv().GetItem(bagslot_id);
				if (bagitem) {
					with->SendItemPacket(bagslot_id - EmuConstants::TRADE_BEGIN, bagitem, ItemPacketTradeView);
				}
			}
		}

		//safe_delete(outapp);
	}
}

// Audit trade: The part logged is what travels owner -> with
void Trade::LogTrade()
{
	Mob* with = With();
	if (!owner->IsClient() || !with)
		return; // Should never happen

	Client* trader = owner->CastToClient();
	bool logtrade = false;
	int admin_level = 0;
	uint8 item_count = 0;

	if (zone->tradevar != 0) {
		for (uint16 i = EmuConstants::TRADE_BEGIN; i <= EmuConstants::TRADE_END; i++) {
			if (trader->GetInv().GetItem(i))
				item_count++;
		}

		if (((this->cp + this->sp + this->gp + this->pp)>0) || (item_count>0))
			admin_level = trader->Admin();
		else
			admin_level = 999;

		if (zone->tradevar == 7) {
			logtrade = true;
		}
		else if ((admin_level>=10) && (admin_level<20)) {
			if ((zone->tradevar<8) && (zone->tradevar>5))
				logtrade = true;
		}
		else if (admin_level<=20) {
			if ((zone->tradevar<8) && (zone->tradevar>4))
				logtrade = true;
		}
		else if (admin_level<=80) {
			if ((zone->tradevar<8) && (zone->tradevar>3))
				logtrade = true;
		}
		else if (admin_level<=100){
			if ((zone->tradevar<9) && (zone->tradevar>2))
				logtrade = true;
		}
		else if (admin_level<=150){
			if (((zone->tradevar<8) && (zone->tradevar>1)) || (zone->tradevar==9))
				logtrade = true;
		}
		else if (admin_level<=255){
			if ((zone->tradevar<8) && (zone->tradevar>0))
				logtrade = true;
		}
	}

	if (logtrade == true) {
		char logtext[1000] = {0};
		uint32 cash = 0;
		bool comma = false;

		// Log items offered by owner
		cash = this->cp + this->sp + this->gp + this->pp;
		if ((cash>0) || (item_count>0)) {
			sprintf(logtext, "%s gave %s ", trader->GetName(), with->GetName());

			if (item_count > 0) {
				strcat(logtext, "items {");

				for (uint16 i = EmuConstants::TRADE_BEGIN; i <= EmuConstants::TRADE_END; i++) {
					const ItemInst* inst = trader->GetInv().GetItem(i);

					if (!comma)
						comma = true;
					else {
						if (inst)
							strcat(logtext, ",");
					}

					if (inst) {
						char item_num[15] = {0};
						sprintf(item_num, "%i", inst->GetItem()->ID);
						strcat(logtext, item_num);

						if (inst->IsType(ItemClassContainer)) {
							for (uint8 j = SUB_BEGIN; j < EmuConstants::ITEM_CONTAINER_SIZE; j++) {
								inst = trader->GetInv().GetItem(i, j);
								if (inst) {
									strcat(logtext, ",");
									sprintf(item_num, "%i", inst->GetItem()->ID);
									strcat(logtext, item_num);
								}
							}
						}
					}
				}
			}

			if (cash > 0) {
				char money[100] = {0};
				sprintf(money, " %ipp, %igp, %isp, %icp", trader->trade->pp, trader->trade->gp, trader->trade->sp, trader->trade->cp);
				strcat(logtext, money);
			}

			database.logevents(trader->AccountName(), trader->AccountID(),
				trader->Admin(), trader->GetName(), with->GetName(), "Trade", logtext, 6);
		}
	}
}


void Trade::DumpTrade()
{
	Mob* with = With();
	Log.Out(Logs::Detail, Logs::Trading, "Dumping trade data: '%s' in TradeState %i with '%s'",
		this->owner->GetName(), state, ((with==nullptr)?"(null)":with->GetName()));

	if (!owner->IsClient())
		return;

	Client* trader = owner->CastToClient();
	for (uint16 i = EmuConstants::TRADE_BEGIN; i <= EmuConstants::TRADE_END; i++) {
		const ItemInst* inst = trader->GetInv().GetItem(i);

		if (inst) {
			Log.Out(Logs::Detail, Logs::Trading, "Item %i (Charges=%i, Slot=%i, IsBag=%s)",
				inst->GetItem()->ID, inst->GetCharges(),
				i, ((inst->IsType(ItemClassContainer)) ? "True" : "False"));

			if (inst->IsType(ItemClassContainer)) {
				for (uint8 j = SUB_BEGIN; j < EmuConstants::ITEM_CONTAINER_SIZE; j++) {
					inst = trader->GetInv().GetItem(i, j);
					if (inst) {
						Log.Out(Logs::Detail, Logs::Trading, "\tBagItem %i (Charges=%i, Slot=%i)",
							inst->GetItem()->ID, inst->GetCharges(),
							Inventory::CalcSlotId(i, j));
					}
				}
			}
		}
	}

	Log.Out(Logs::Detail, Logs::Trading, "\tpp:%i, gp:%i, sp:%i, cp:%i", pp, gp, sp, cp);
}


void Client::ResetTrade() {
	AddMoneyToPP(trade->cp, trade->sp, trade->gp, trade->pp, true);

	// step 1: process bags
	for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_END; ++trade_slot) {
		const ItemInst* inst = m_inv[trade_slot];

		if (inst && inst->IsType(ItemClassContainer)) {
			int16 free_slot = m_inv.FindFreeSlotForTradeItem(inst);

			if (free_slot != INVALID_INDEX) {
				PutItemInInventory(free_slot, *inst);
				SendItemPacket(free_slot, inst, ItemPacketTrade);
			}
			else {
				entity_list.CreateGroundObject(inst->GetID(), glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime));
			}

			DeleteItemInInventory(trade_slot);
		}
	}

	// step 2a: process stackables
	for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_END; ++trade_slot) {
		ItemInst* inst = GetInv().GetItem(trade_slot);

		if (inst && inst->IsStackable()) {
			while (true) {
				// there's no built-in safety check against an infinite loop..but, it should break on one of the conditional checks
				int16 free_slot = m_inv.FindFreeSlotForTradeItem(inst);

				if ((free_slot == MainCursor) || (free_slot == INVALID_INDEX))
					break;

				ItemInst* partial_inst = GetInv().GetItem(free_slot);

				if (!partial_inst)
					break;

				if (partial_inst->GetID() != inst->GetID()) {
					Log.Out(Logs::Detail, Logs::Trading, "[CLIENT] Client::ResetTrade() - an incompatible location reference was returned by Inventory::FindFreeSlotForTradeItem()");

					break;
				}

				if ((partial_inst->GetCharges() + inst->GetCharges()) > partial_inst->GetItem()->StackSize) {
					int16 new_charges = (partial_inst->GetCharges() + inst->GetCharges()) - partial_inst->GetItem()->StackSize;

					partial_inst->SetCharges(partial_inst->GetItem()->StackSize);
					inst->SetCharges(new_charges);
				}
				else {
					partial_inst->SetCharges(partial_inst->GetCharges() + inst->GetCharges());
					inst->SetCharges(0);
				}

				PutItemInInventory(free_slot, *partial_inst);
				SendItemPacket(free_slot, partial_inst, ItemPacketTrade);

				if (inst->GetCharges() == 0) {
					DeleteItemInInventory(trade_slot);

					break;
				}
			}
		}
	}

	// step 2b: adjust trade stack bias
	// (if any partial stacks exist before the final stack, FindFreeSlotForTradeItem() will return that slot in step 3 and an overwrite will occur)
	for (int16 trade_slot = EmuConstants::TRADE_END; trade_slot >= EmuConstants::TRADE_BEGIN; --trade_slot) {
		ItemInst* inst = GetInv().GetItem(trade_slot);

		if (inst && inst->IsStackable()) {
			for (int16 bias_slot = EmuConstants::TRADE_BEGIN; bias_slot <= EmuConstants::TRADE_END; ++bias_slot) {
				if (bias_slot >= trade_slot)
					break;

				ItemInst* bias_inst = GetInv().GetItem(bias_slot);

				if (!bias_inst || (bias_inst->GetID() != inst->GetID()) || (bias_inst->GetCharges() >= bias_inst->GetItem()->StackSize))
					continue;

				if ((bias_inst->GetCharges() + inst->GetCharges()) > bias_inst->GetItem()->StackSize) {
					int16 new_charges = (bias_inst->GetCharges() + inst->GetCharges()) - bias_inst->GetItem()->StackSize;

					bias_inst->SetCharges(bias_inst->GetItem()->StackSize);
					inst->SetCharges(new_charges);
				}
				else {
					bias_inst->SetCharges(bias_inst->GetCharges() + inst->GetCharges());
					inst->SetCharges(0);
				}

				if (inst->GetCharges() == 0) {
					DeleteItemInInventory(trade_slot);

					break;
				}
			}
		}
	}

	// step 3: process everything else
	for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_END; ++trade_slot) {
		const ItemInst* inst = m_inv[trade_slot];

		if (inst) {
			int16 free_slot = m_inv.FindFreeSlotForTradeItem(inst);

			if (free_slot != INVALID_INDEX) {
				PutItemInInventory(free_slot, *inst);
				SendItemPacket(free_slot, inst, ItemPacketTrade);
			}
			else {
				entity_list.CreateGroundObject(inst->GetID(), glm::vec4(GetX(), GetY(), GetZ(), 0), RuleI(Groundspawns, FullInvDecayTime));
			}

			DeleteItemInInventory(trade_slot);
		}
	}
}

void Client::FinishTrade(Mob* tradingWith, bool finalizer, void* event_entry, std::list<void*>* event_details) {
	if(tradingWith && tradingWith->IsClient()) {
		Client* other = tradingWith->CastToClient();
		QSPlayerLogTrade_Struct* qs_audit = nullptr;
		bool qs_log = false;

		if(other) {
			Log.Out(Logs::Detail, Logs::Trading, "Finishing trade with client %s", other->GetName());

			this->AddMoneyToPP(other->trade->cp, other->trade->sp, other->trade->gp, other->trade->pp, true);

			// step 0: pre-processing
			// QS code
			if (RuleB(QueryServ, PlayerLogTrades) && event_entry && event_details) {
				qs_audit = (QSPlayerLogTrade_Struct*)event_entry;
				qs_log = true;

				if (finalizer) {
					qs_audit->char2_id = this->character_id;

					qs_audit->char2_money.platinum = this->trade->pp;
					qs_audit->char2_money.gold = this->trade->gp;
					qs_audit->char2_money.silver = this->trade->sp;
					qs_audit->char2_money.copper = this->trade->cp;
				}
				else {
					qs_audit->char1_id = this->character_id;

					qs_audit->char1_money.platinum = this->trade->pp;
					qs_audit->char1_money.gold = this->trade->gp;
					qs_audit->char1_money.silver = this->trade->sp;
					qs_audit->char1_money.copper = this->trade->cp;
				}
			}

			// step 1: process bags
			for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_END; ++trade_slot) {
				const ItemInst* inst = m_inv[trade_slot];
				bool dropitem = false;

				if (inst && inst->IsType(ItemClassContainer)) {
					Log.Out(Logs::Detail, Logs::Trading, "Giving container %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot, other->GetName());

					if (inst->GetItem()->NoDrop != 0 || Admin() >= RuleI(Character, MinStatusForNoDropExemptions) || RuleI(World, FVNoDropFlag) == 1 || other == this) {
						int16 free_slot = other->GetInv().FindFreeSlotForTradeItem(inst);

						if (free_slot != INVALID_INDEX) 
						{
							if (other->PutItemInInventory(free_slot, *inst, true)) 
							{
								Log.Out(Logs::Detail, Logs::Trading, "Container %s (%d) successfully transferred, deleting from trade slot.", inst->GetItem()->Name, inst->GetItem()->ID);
								
								// step 1a: process items in bags
								uint8 bagidx = 0;
								for (int16 trade_bag_slot = EmuConstants::TRADE_BAGS_BEGIN + (trade_slot - EmuConstants::TRADE_BEGIN) * EmuConstants::ITEM_CONTAINER_SIZE; 
									trade_bag_slot <= EmuConstants::TRADE_BAGS_BEGIN + (trade_slot- EmuConstants::TRADE_BEGIN) * EmuConstants::ITEM_CONTAINER_SIZE + 9; 
									++trade_bag_slot)
								{
									const ItemInst* inst = m_inv[trade_bag_slot];

									if (inst) {
									Log.Out(Logs::Detail, Logs::Trading, "Giving item in container %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_bag_slot, other->GetName());
	
										if (inst->GetItem()->NoDrop != 0 || Admin() >= RuleI(Character, MinStatusForNoDropExemptions) || RuleI(World, FVNoDropFlag) == 1 || other == this) 
										{
											int16 free_bag_slot = other->GetInv().CalcSlotId(free_slot, bagidx);
											Log.Out(Logs::Detail, Logs::Trading, "Free slot is: %i. Slot bag is in: %i Index is: %i", free_bag_slot, free_slot, bagidx);
											if (free_bag_slot != INVALID_INDEX) {
												if (other->PutItemInInventory(free_bag_slot, *inst, true)) 
												{
													Log.Out(Logs::Detail, Logs::Trading, "Container item %s (%d) successfully transferred, deleting from trade slot.", inst->GetItem()->Name, inst->GetItem()->ID);
												}

												else {
													Log.Out(Logs::Detail, Logs::Trading, "Transfer of container item %s (%d) to %s failed, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID, other->GetName());
													dropitem = true;
												}
											}
											else {
												Log.Out(Logs::Detail, Logs::Trading, "%s's inventory is full, returning container item %s (%d) to giver.", other->GetName(), inst->GetItem()->Name, inst->GetItem()->ID);
												dropitem = true;
											}
										}
										else {
											Log.Out(Logs::Detail, Logs::Trading, "Container item %s (%d) is NoDrop, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID);
											PushItemOnCursor(*inst, true);
										}
										DeleteItemInInventory(trade_bag_slot);
									}
									if(dropitem)
									{
										other->Message(CC_Red, "You do not have room for any more items.");
										CreateGroundObject(inst,other->GetX(),other->GetY(),other->GetZ(),0,RuleI(Groundspawns,FullInvDecayTime));
									}
									bagidx++;
								}
								if (qs_log) {
									QSTradeItems_Struct* detail = new QSTradeItems_Struct;

									detail->from_id = this->character_id;
									detail->from_slot = trade_slot;
									detail->to_id = other->CharacterID();
									detail->to_slot = free_slot;
									detail->item_id = inst->GetID();
									detail->charges = 1;

									event_details->push_back(detail);

									if (finalizer)
										qs_audit->char2_count += detail->charges;
									else
										qs_audit->char1_count += detail->charges;

									//for (uint8 sub_slot = SUB_BEGIN; ((sub_slot < inst->GetItem()->BagSlots) && (sub_slot < EmuConstants::ITEM_CONTAINER_SIZE)); ++sub_slot) {
									for (uint8 sub_slot = SUB_BEGIN; (sub_slot < EmuConstants::ITEM_CONTAINER_SIZE); ++sub_slot) { // this is to catch ALL items
										const ItemInst* bag_inst = inst->GetItem(sub_slot);

										if (bag_inst) {
											detail = new QSTradeItems_Struct;

											detail->from_id = this->character_id;
											detail->from_slot = Inventory::CalcSlotId(trade_slot, sub_slot);
											detail->to_id = other->CharacterID();
											detail->to_slot = Inventory::CalcSlotId(free_slot, sub_slot);
											detail->item_id = bag_inst->GetID();
											detail->charges = (!bag_inst->IsStackable() ? 1 : bag_inst->GetCharges());

											event_details->push_back(detail);

											if (finalizer)
												qs_audit->char2_count += detail->charges;
											else
												qs_audit->char1_count += detail->charges;
										}
									}
								}
							}
							else {
								Log.Out(Logs::Detail, Logs::Trading, "Transfer of container %s (%d) to %s failed, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID, other->GetName());
								dropitem = true;
							}
						}
						else {
							Log.Out(Logs::Detail, Logs::Trading, "%s's inventory is full, returning container %s (%d) to giver.", other->GetName(), inst->GetItem()->Name, inst->GetItem()->ID);
							dropitem = true;
						}
					}
					else {
						Log.Out(Logs::Detail, Logs::Trading, "Container %s (%d) is NoDrop, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID);
						PushItemOnCursor(*inst, true);
					}

					DeleteItemInInventory(trade_slot);
				}
				if(dropitem)
				{
					other->Message(CC_Red, "You do not have room for any more items.");
					CreateGroundObject(inst,other->GetX(),other->GetY(),other->GetZ(),0,RuleI(Groundspawns,FullInvDecayTime));
				}
			}

			// step 2a: process stackables
			for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_END; ++trade_slot) {
				ItemInst* inst = GetInv().GetItem(trade_slot);

				if (inst && inst->IsStackable()) {
					while (true) {
						// there's no built-in safety check against an infinite loop..but, it should break on one of the conditional checks
						int16 partial_slot = other->GetInv().FindFreeSlotForTradeItem(inst);

						if ((partial_slot == MainCursor) || (partial_slot == INVALID_INDEX))
							break;

						ItemInst* partial_inst = other->GetInv().GetItem(partial_slot);

						if (!partial_inst)
							break;

						if (partial_inst->GetID() != inst->GetID()) {
							Log.Out(Logs::Detail, Logs::Trading, "[CLIENT] Client::ResetTrade() - an incompatible location reference was returned by Inventory::FindFreeSlotForTradeItem()");
							break;
						}

						int16 old_charges = inst->GetCharges();
						int16 partial_charges = partial_inst->GetCharges();

						if ((partial_inst->GetCharges() + inst->GetCharges()) > partial_inst->GetItem()->StackSize) {
							int16 new_charges = (partial_inst->GetCharges() + inst->GetCharges()) - partial_inst->GetItem()->StackSize;

							partial_inst->SetCharges(partial_inst->GetItem()->StackSize);
							inst->SetCharges(new_charges);
						}
						else {
							partial_inst->SetCharges(partial_inst->GetCharges() + inst->GetCharges());
							inst->SetCharges(0);
						}

						Log.Out(Logs::Detail, Logs::Trading, "Transferring partial stack %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot, other->GetName());

						if (other->PutItemInInventory(partial_slot, *partial_inst, true)) {
							Log.Out(Logs::Detail, Logs::Trading, "Partial stack %s (%d) successfully transferred, deleting %i charges from trade slot.",
								inst->GetItem()->Name, inst->GetItem()->ID, (old_charges - inst->GetCharges()));
							if (qs_log) {
								QSTradeItems_Struct* detail = new QSTradeItems_Struct;

								detail->from_id = this->character_id;
								detail->from_slot = trade_slot;
								detail->to_id = other->CharacterID();
								detail->to_slot = partial_slot;
								detail->item_id = inst->GetID();
								detail->charges = (old_charges - inst->GetCharges());

								event_details->push_back(detail);

								if (finalizer)
									qs_audit->char2_count += detail->charges;
								else
									qs_audit->char1_count += detail->charges;
							}
						}
						else {
							Log.Out(Logs::Detail, Logs::Trading, "Transfer of partial stack %s (%d) to %s failed, returning %i charges to trade slot.",
								inst->GetItem()->Name, inst->GetItem()->ID, other->GetName(), (old_charges - inst->GetCharges()));

							inst->SetCharges(old_charges);
							partial_inst->SetCharges(partial_charges);
							break;
						}

						if (inst->GetCharges() == 0) {
							DeleteItemInInventory(trade_slot);
							break;
						}
					}
				}
			}

			// step 2b: adjust trade stack bias
			// (if any partial stacks exist before the final stack, FindFreeSlotForTradeItem() will return that slot in step 3 and an overwrite will occur)
			for (int16 trade_slot = EmuConstants::TRADE_END; trade_slot >= EmuConstants::TRADE_BEGIN; --trade_slot) {
				ItemInst* inst = GetInv().GetItem(trade_slot);

				if (inst && inst->IsStackable()) {
					for (int16 bias_slot = EmuConstants::TRADE_BEGIN; bias_slot <= EmuConstants::TRADE_END; ++bias_slot) {
						if (bias_slot >= trade_slot)
							break;

						ItemInst* bias_inst = GetInv().GetItem(bias_slot);

						if (!bias_inst || (bias_inst->GetID() != inst->GetID()) || (bias_inst->GetCharges() >= bias_inst->GetItem()->StackSize))
							continue;

						int16 old_charges = inst->GetCharges();

						if ((bias_inst->GetCharges() + inst->GetCharges()) > bias_inst->GetItem()->StackSize) {
							int16 new_charges = (bias_inst->GetCharges() + inst->GetCharges()) - bias_inst->GetItem()->StackSize;

							bias_inst->SetCharges(bias_inst->GetItem()->StackSize);
							inst->SetCharges(new_charges);
						}
						else {
							bias_inst->SetCharges(bias_inst->GetCharges() + inst->GetCharges());
							inst->SetCharges(0);
						}

						if (qs_log) {
							QSTradeItems_Struct* detail = new QSTradeItems_Struct;

							detail->from_id = this->character_id;
							detail->from_slot = trade_slot;
							detail->to_id = this->character_id;
							detail->to_slot = bias_slot;
							detail->item_id = inst->GetID();
							detail->charges = (old_charges - inst->GetCharges());

							event_details->push_back(detail);
						}

						if (inst->GetCharges() == 0) {
							DeleteItemInInventory(trade_slot);
							break;
						}
					}
				}
			}

			// step 3: process everything else
			for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_END; ++trade_slot) {
				const ItemInst* inst = m_inv[trade_slot];
				bool dropitem = false;

				if (inst) {
					Log.Out(Logs::Detail, Logs::Trading, "Giving item %s (%d) in slot %d to %s", inst->GetItem()->Name, inst->GetItem()->ID, trade_slot, other->GetName());

					// TODO: need to check bag items/augments for no drop..everything for attuned...
					if (inst->GetItem()->NoDrop != 0 || Admin() >= RuleI(Character, MinStatusForNoDropExemptions) || RuleI(World, FVNoDropFlag) == 1 || other == this) {
						int16 free_slot = other->GetInv().FindFreeSlotForTradeItem(inst);

						if (free_slot != INVALID_INDEX) {
							if (other->PutItemInInventory(free_slot, *inst, true)) {
								Log.Out(Logs::Detail, Logs::Trading, "Item %s (%d) successfully transferred, deleting from trade slot.", inst->GetItem()->Name, inst->GetItem()->ID);
								if (qs_log) {
									QSTradeItems_Struct* detail = new QSTradeItems_Struct;

									detail->from_id = this->character_id;
									detail->from_slot = trade_slot;
									detail->to_id = other->CharacterID();
									detail->to_slot = free_slot;
									detail->item_id = inst->GetID();
									detail->charges = (!inst->IsStackable() ? 1 : inst->GetCharges());

									event_details->push_back(detail);

									if (finalizer)
										qs_audit->char2_count += detail->charges;
									else
										qs_audit->char1_count += detail->charges;

									// 'step 3' should never really see containers..but, just in case...
									//for (uint8 sub_slot = SUB_BEGIN; ((sub_slot < inst->GetItem()->BagSlots) && (sub_slot < EmuConstants::ITEM_CONTAINER_SIZE)); ++sub_slot) {
									for (uint8 sub_slot = SUB_BEGIN; (sub_slot < EmuConstants::ITEM_CONTAINER_SIZE); ++sub_slot) { // this is to catch ALL items
										const ItemInst* bag_inst = inst->GetItem(sub_slot);

										if (bag_inst) {
											detail = new QSTradeItems_Struct;

											detail->from_id = this->character_id;
											detail->from_slot = trade_slot;
											detail->to_id = other->CharacterID();
											detail->to_slot = free_slot;
											detail->item_id = bag_inst->GetID();
											detail->charges = (!bag_inst->IsStackable() ? 1 : bag_inst->GetCharges());

											event_details->push_back(detail);

											if (finalizer)
												qs_audit->char2_count += detail->charges;
											else
												qs_audit->char1_count += detail->charges;
										}
									}
								}
							}
							else {
								Log.Out(Logs::Detail, Logs::Trading, "Transfer of Item %s (%d) to %s failed, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID, other->GetName());
								dropitem = true;
							}
						}
						else {
							Log.Out(Logs::Detail, Logs::Trading, "%s's inventory is full, returning item %s (%d) to giver.", other->GetName(), inst->GetItem()->Name, inst->GetItem()->ID);
							dropitem = true;
						}
					}
					else {
						Log.Out(Logs::Detail, Logs::Trading, "Item %s (%d) is NoDrop, returning to giver.", inst->GetItem()->Name, inst->GetItem()->ID);
						PushItemOnCursor(*inst, true);
					}

					DeleteItemInInventory(trade_slot);
				}
				if(dropitem)
				{
					other->Message(CC_Red, "You do not have room for any more items.");
					CreateGroundObject(inst,other->GetX(),other->GetY(),other->GetZ(),0,RuleI(Groundspawns,FullInvDecayTime));
				}
			}

			//Do not reset the trade here, done by the caller.
		}
	}
	else if(tradingWith && tradingWith->IsNPC()) {
		QSPlayerLogHandin_Struct* qs_audit = nullptr;
		bool qs_log = false;

		// QS code
		if(RuleB(QueryServ, PlayerLogTrades) && event_entry && event_details) {
			// Currently provides only basic functionality. Calling method will also
			// need to be modified before item returns and rewards can be logged. -U
			qs_audit = (QSPlayerLogHandin_Struct*)event_entry;
			qs_log = true;

			qs_audit->quest_id = 0;
			qs_audit->char_id = character_id;
			qs_audit->char_money.platinum = trade->pp;
			qs_audit->char_money.gold = trade->gp;
			qs_audit->char_money.silver = trade->sp;
			qs_audit->char_money.copper = trade->cp;
			qs_audit->char_count = 0;
			qs_audit->npc_id = tradingWith->GetNPCTypeID();
			qs_audit->npc_money.platinum = 0;
			qs_audit->npc_money.gold = 0;
			qs_audit->npc_money.silver = 0;
			qs_audit->npc_money.copper = 0;
			qs_audit->npc_count = 0;
		}

		if(qs_log) { // This can be incorporated below when revisions are made -U
			for (int16 trade_slot = EmuConstants::TRADE_BEGIN; trade_slot <= EmuConstants::TRADE_NPC_END; ++trade_slot) {
				const ItemInst* trade_inst = m_inv[trade_slot];

				if(trade_inst) {
					QSHandinItems_Struct* detail = new QSHandinItems_Struct;

					strcpy(detail->action_type, "HANDIN");

					detail->char_slot = trade_slot;
					detail->item_id = trade_inst->GetID();
					detail->charges = (!trade_inst->IsStackable() ? 1 : trade_inst->GetCharges());

					event_details->push_back(detail);
					qs_audit->char_count += detail->charges;

					if(trade_inst->IsType(ItemClassContainer)) {
						for (uint8 sub_slot = SUB_BEGIN; sub_slot < trade_inst->GetItem()->BagSlots; ++sub_slot) {
							const ItemInst* trade_baginst = trade_inst->GetItem(sub_slot);

							if(trade_baginst) {
								detail = new QSHandinItems_Struct;

								strcpy(detail->action_type, "HANDIN");

								detail->char_slot = Inventory::CalcSlotId(trade_slot, sub_slot);
								detail->item_id = trade_baginst->GetID();
								detail->charges = (!trade_inst->IsStackable() ? 1 : trade_inst->GetCharges());

								event_details->push_back(detail);
								qs_audit->char_count += detail->charges;
							}
						}
					}
				}
			}
		}

		bool quest_npc = false;
		if(parse->HasQuestSub(tradingWith->GetNPCTypeID(), EVENT_TRADE)) {
			// This is a quest NPC
			quest_npc = true;
		}

		std::vector<EQEmu::Any> item_list;
		uint32 items[4] = { 0 };
		for(int i = EmuConstants::TRADE_BEGIN; i <= EmuConstants::TRADE_NPC_END; ++i) {
			ItemInst *inst = m_inv.GetItem(i);
			if(inst) {
				items[i - EmuConstants::TRADE_BEGIN] = inst->GetItem()->ID;
				item_list.push_back(inst);
			} else {
				item_list.push_back((ItemInst*)nullptr);
				continue;
			}

			const Item_Struct* item = inst->GetItem();
			if(item && quest_npc == false) {
				// if it was not a NO DROP or Attuned item (or if a GM is trading), let the NPC have it
				if(GetGM() || (item->NoDrop != 0 && inst->IsInstNoDrop() == false)) {
					// pets need to look inside bags and try to equip items found there
					if(item->ItemClass == ItemClassContainer && item->BagSlots > 0) {
						for(int16 bslot = SUB_BEGIN; bslot < item->BagSlots; bslot++) {
							const ItemInst* baginst = inst->GetItem(bslot);
							if (baginst) {
								const Item_Struct* bagitem = baginst->GetItem();
								if (bagitem && (GetGM() || (bagitem->NoDrop != 0 && baginst->IsInstNoDrop() == false))) {
									tradingWith->CastToNPC()->AddLootDrop(bagitem, &tradingWith->CastToNPC()->itemlist,
										baginst->GetCharges(), 1, 127, true, true);
								}
								else if (RuleB(NPC, ReturnNonQuestNoDropItems)) {
									PushItemOnCursor(*baginst, true);
								}
							}
						}
					}

					tradingWith->CastToNPC()->AddLootDrop(item, &tradingWith->CastToNPC()->itemlist,
						inst->GetCharges(), 1, 127, true, true);
				}
				// Return NO DROP and Attuned items being handed into a non-quest NPC if the rule is true
				else if (RuleB(NPC, ReturnNonQuestNoDropItems)) {
					PushItemOnCursor(*inst, true);
					DeleteItemInInventory(i);
				}
			}
		}

		char temp1[100] = { 0 };
		char temp2[100] = { 0 };
		snprintf(temp1, 100, "copper.%d", tradingWith->GetNPCTypeID());
		snprintf(temp2, 100, "%u", trade->cp);
		parse->AddVar(temp1, temp2);
		snprintf(temp1, 100, "silver.%d", tradingWith->GetNPCTypeID());
		snprintf(temp2, 100, "%u", trade->sp);
		parse->AddVar(temp1, temp2);
		snprintf(temp1, 100, "gold.%d", tradingWith->GetNPCTypeID());
		snprintf(temp2, 100, "%u", trade->gp);
		parse->AddVar(temp1, temp2);
		snprintf(temp1, 100, "platinum.%d", tradingWith->GetNPCTypeID());
		snprintf(temp2, 100, "%u", trade->pp);
		parse->AddVar(temp1, temp2); 

		if(tradingWith->GetAppearance() != eaDead) {
			tradingWith->FaceTarget(this);
		}

		ItemInst *insts[4] = { 0 };
		for(int i = EmuConstants::TRADE_BEGIN; i <= EmuConstants::TRADE_NPC_END; ++i) {
			insts[i - EmuConstants::TRADE_BEGIN] = m_inv.PopItem(i);
			database.SaveInventory(CharacterID(), nullptr, i);
		}

		parse->EventNPC(EVENT_TRADE, tradingWith->CastToNPC(), this, "", 0, &item_list);

		for(int i = 0; i < 4; ++i) {
			if(insts[i]) {
				safe_delete(insts[i]);
			}
		}
	}
}

bool Client::CheckTradeLoreConflict(Client* other)
{
	if (!other)
		return true;
	// Move each trade slot into free inventory slot
	for (int16 i = EmuConstants::TRADE_BEGIN; i <= EmuConstants::TRADE_END; i++){
		const ItemInst* inst = m_inv[i];

		if (inst && inst->GetItem()) {
			if (other->CheckLoreConflict(inst->GetItem()))
				return true;
		}
	}

	for (int16 i = EmuConstants::TRADE_BAGS_BEGIN; i <= EmuConstants::TRADE_BAGS_END; i++){
		const ItemInst* inst = m_inv[i];

		if (inst && inst->GetItem()) {
			if (other->CheckLoreConflict(inst->GetItem()))
				return true;
		}
	}

	return false;
}

/* Bazaar code begins here */

void Client::Trader_ShowItems(){
	EQApplicationPacket* outapp= new EQApplicationPacket(OP_Trader, sizeof(Trader_Struct));

	Trader_Struct* outints = (Trader_Struct*)outapp->pBuffer;
	Trader_Struct* TraderItems = database.LoadTraderItem(this->CharacterID());

	for(int i = 0; i < 80; i++){
		outints->ItemCost[i] = TraderItems->ItemCost[i];
		outints->Items[i] = TraderItems->Items[i];
	}
	outints->Code = BazaarTrader_ShowItems;

	QueuePacket(outapp);
	safe_delete(outapp);
	safe_delete(TraderItems);
}

void Client::SendTraderPacket(Client* Trader, uint32 Unknown72)
{
	if(!Trader)
		return;

	EQApplicationPacket* outapp= new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = BazaarTrader_StartTraderMode;
	bts->ID = Trader->GetID();
	strn0cpy(bts->Name, Trader->GetName(), sizeof(bts->Name));
	bts->Unknown072 = Unknown72;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::Trader_CustomerBrowsing(Client *Customer) {

	//Not working atm, need to figure out packet.
	EQApplicationPacket* outapp= new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* sis = (Trader_ShowItems_Struct*)outapp->pBuffer;
	sis->Code = BazaarTrader_CustomerBrowsing;
	sis->TraderID = Customer->GetID();
	QueuePacket(outapp);
}

void Client::Trader_CustomerBought(Client *Customer,uint32 Price, uint32 ItemID, uint32 Quantity, const char* ItemName) {

	EQApplicationPacket* outapp= new EQApplicationPacket(OP_Trader, sizeof(TraderBuy_Struct));
	TraderBuy_Struct* sis = (TraderBuy_Struct*)outapp->pBuffer;

	sis->Action = BazaarBuyItem;
	sis->ItemID = ItemID;
	sis->Price = Price;
	sis->Quantity = Quantity;
	strcpy(sis->ItemName,ItemName);
	QueuePacket(outapp);
}

void Client::Trader_StartTrader() {

	Trader=true;

	EQApplicationPacket* outapp= new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* sis = (Trader_ShowItems_Struct*)outapp->pBuffer;
	sis->Code = BazaarTrader_StartTraderMode;
	sis->TraderID = this->GetID();

	QueuePacket(outapp);
	safe_delete(outapp);

	// Notify other clients we are now in trader mode
	outapp= new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = BazaarTrader_StartTraderMode;
	bts->ID = this->GetID();
	strn0cpy(bts->Name, GetName(), sizeof(bts->Name));

	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);
}

void Client::Trader_EndTrader() {

	// If someone is looking at our wares, remove all the items from the window.
	//
	if(CustomerID) {
		Client* Customer = entity_list.GetClientByID(CustomerID);
		GetItems_Struct* gis=GetTraderItems();

		if(Customer && gis) 
		{
			Customer->Message(CC_Red, "The Trader is no longer open for business");
			EQApplicationPacket empty(OP_ShopEndConfirm);
			Customer->QueuePacket(&empty);
			Customer->Save();
		}
	}

	database.DeleteTraderItem(this->CharacterID());

	// Notify other clients we are no longer in trader mode.
	EQApplicationPacket* outapp= new EQApplicationPacket(OP_BecomeTrader, sizeof(BecomeTrader_Struct));
	BecomeTrader_Struct* bts = (BecomeTrader_Struct*)outapp->pBuffer;
	bts->Code = 0;
	bts->ID = this->GetID();
	strn0cpy(bts->Name, GetName(), sizeof(bts->Name));

	entity_list.QueueClients(this, outapp, false);
	safe_delete(outapp);

	outapp= new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* sis = (Trader_ShowItems_Struct*)outapp->pBuffer;
	sis->Code = BazaarTrader_EndTraderMode;

	QueuePacket(outapp);
	safe_delete(outapp);

	WithCustomer(0);
	this->Trader = false;
}

void Client::SendTraderItem(uint32 ItemID, uint16 Quantity) {

	std::string Packet;
	int16 FreeSlotID=0;

	const Item_Struct* item = database.GetItem(ItemID);

	if(!item){
		Log.Out(Logs::Detail, Logs::Bazaar, "Bogus item deleted in Client::SendTraderItem!\n");
		return;
	}

	ItemInst* inst = database.CreateItem(item, Quantity);

	if (inst)
	{
		bool is_arrow = (inst->GetItem()->ItemType == ItemTypeArrow) ? true : false;
		FreeSlotID = m_inv.FindFreeSlot(false, true, inst->GetItem()->Size, is_arrow);

		PutItemInInventory(FreeSlotID, *inst);
		Save();

		SendItemPacket(FreeSlotID, inst, ItemPacketTrade);

		safe_delete(inst);
	}
}

void Client::SendSingleTraderItem(uint32 CharID, int SerialNumber) {

	ItemInst* inst= database.LoadSingleTraderItem(CharID, SerialNumber);

	BulkSendTraderInventory(CharID);

}

void Client::BulkSendTraderInventory(uint32 char_id) {
	const Item_Struct *item;

	TraderCharges_Struct* TraderItems = database.LoadTraderItemWithCharges(char_id);

	uint32 size = 0;
	std::map<uint16, std::string> ser_items;
	std::map<uint16, std::string>::iterator mer_itr;
	for (uint8 i = 0;i < 80; i++) {
		if((TraderItems->ItemID[i] == 0) || (TraderItems->ItemCost[i] <= 0))
			continue;
		else
			item=database.GetItem(TraderItems->ItemID[i]);
		if (item && (item->NoDrop!=0)) {
			ItemInst* inst = database.CreateItem(item);
			if (inst) {
				inst->SetSerialNumber(TraderItems->SerialNumber[i]);
				if(TraderItems->Charges[i] > 0)
					inst->SetCharges(TraderItems->Charges[i]);

				if(inst->IsStackable()) {
					inst->SetMerchantCount(TraderItems->Charges[i]);
					inst->SetMerchantSlot(TraderItems->SerialNumber[i]);
				}
				inst->SetMerchantSlot(i);
				inst->SetPrice(TraderItems->ItemCost[i]);
					std::string packet = inst->Serialize(30);
					ser_items[i] = packet;
					size += packet.length();
			}
			else
				Log.Out(Logs::Detail, Logs::Bazaar, "Client::BulkSendTraderInventory nullptr inst pointer");
		}
		else
			Log.Out(Logs::Detail, Logs::Bazaar, "Client::BulkSendTraderInventory nullptr item pointer or item is NODROP %8X",item);
	}
		int8 count = 0;
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopInventoryPacket, size);
		uchar* ptr = outapp->pBuffer;
		for(mer_itr = ser_items.begin(); mer_itr != ser_items.end(); mer_itr++){
			int length = mer_itr->second.length();
			if(length > 5) {
				memcpy(ptr, mer_itr->second.c_str(), length);
				ptr += length;
				count++;
			}
			if(count >= 79)
				break;
		}

		QueuePacket(outapp);
		safe_delete(outapp);
	safe_delete(TraderItems);
}

ItemInst* Client::FindTraderItemByID(int32 ItemID){

	ItemInst* item = nullptr;
	uint16 SlotID = 0;
	for(int i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_END; i++)
	{
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899) //Traders Satchel
		{ 
			for(int x = SUB_BEGIN; x < EmuConstants::ITEM_CONTAINER_SIZE; x++) 
			{
				SlotID = Inventory::CalcSlotId(i, x);
				item = this->GetInv().GetItem(SlotID);
				if(item && item->GetItem()->ID == ItemID)
					return item;
			}
		}
	}
	Log.Out(Logs::Detail, Logs::Bazaar, "Client::FindTraderItemByID Couldn't find item! Item No. was %i", ItemID);

	return nullptr;
}

GetItems_Struct* Client::GetTraderItems(){

	const ItemInst* item = nullptr;
	uint16 SlotID = 0;
	GetItems_Struct* gis= new GetItems_Struct;
	memset(gis,0,sizeof(GetItems_Struct));
	uint8 ndx = 0;

	for(int i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_END; i++) {
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899){ //Traders Satchel
			for(int x = SUB_BEGIN; x < EmuConstants::ITEM_CONTAINER_SIZE; x++) {
				SlotID = Inventory::CalcSlotId(i, x);

				item = this->GetInv().GetItem(SlotID);

				if(item){
					gis->Items[ndx] = item->GetItem()->ID;
					gis->SerialNumber[ndx] = item->GetSerialNumber();
					gis->Charges[ndx] = item->GetCharges();
					++ndx;
				}
			}
		}
	}
	return gis;
}

GetItem_Struct* Client::RefetchItem(uint16 item_id){

	const ItemInst* item = nullptr;
	uint16 SlotID = 0;
	GetItem_Struct* gis= new GetItem_Struct;
	uint8 count = 0;

	for(int i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_END; i++) 
	{
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899) //Traders Satchel
		{
			for(int x = SUB_BEGIN; x < EmuConstants::ITEM_CONTAINER_SIZE; x++) 
			{
				SlotID = Inventory::CalcSlotId(i, x);

				item = this->GetInv().GetItem(SlotID);

				if(item && item->GetID() == item_id)
				{
					gis->Items = item->GetItem()->ID;
					gis->SerialNumber = item->GetSerialNumber();
					gis->Charges = item->GetCharges();
					++count;
				}
			}
		}
	}
	if(count != 1)
	{
		gis->Items = 0;
		gis->SerialNumber = 0;
		gis->Charges = 0;
	}

	return gis;
}

uint16 Client::FindTraderItem(int32 ItemID, uint16 Quantity){

	const ItemInst* item= nullptr;
	uint16 SlotID = 0;
	for(int i = EmuConstants::GENERAL_BEGIN; i <= EmuConstants::GENERAL_END; i++) {
		item = this->GetInv().GetItem(i);
		if(item && item->GetItem()->ID == 17899){ //Traders Satchel
			for(int x = SUB_BEGIN; x < EmuConstants::ITEM_CONTAINER_SIZE; x++){
				SlotID = Inventory::CalcSlotId(i, x);

				item = this->GetInv().GetItem(SlotID);

				if(item && item->GetID() == ItemID &&
					(item->GetCharges() >= Quantity || (item->GetCharges() <= 0 && Quantity == 1))){

					return SlotID;
				}
			}
		}
	}
	Log.Out(Logs::Detail, Logs::Bazaar, "Could NOT find a match for Item: %i with a quantity of: %i on Trader: %s\n",
					ItemID , Quantity, this->GetName());

	return 0;
}

void Client::NukeTraderItem(uint16 Slot,int16 Charges,uint16 Quantity,Client* Customer,uint16 TraderSlot, int SerialNumber, uint32 SellerID) {

	Client* Seller = entity_list.GetClientByID(SellerID);
	TraderCharges_Struct* gis = database.LoadTraderItemWithCharges(Seller->CharacterID());

	if(!Customer) return;
	Log.Out(Logs::Detail, Logs::Bazaar, "NukeTraderItem(Slot %i, Charges %i, Quantity %i TraderSlot %i", Slot, Charges, Quantity, TraderSlot);
	if(Quantity < Charges) {
		Customer->SendSingleTraderItem(this->CharacterID(), SerialNumber);
		m_inv.DeleteItem(Slot, Quantity);
	}
	else
	{
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
		Merchant_DelItem_Struct* tdis = (Merchant_DelItem_Struct*)outapp->pBuffer;
		tdis->playerid = 0;
		tdis->npcid = Customer->GetID();
		tdis->type=65;
		tdis->itemslot = TraderSlot;

		Log.Out(Logs::Detail, Logs::Bazaar, "Telling customer to remove item %i with %i charges.", SerialNumber, Charges);

		Customer->QueuePacket(outapp);
		safe_delete(outapp);

		m_inv.DeleteItem(Slot);
	}
	// This updates the trader. Removes it from his trading bags.
	const ItemInst* Inst = m_inv[Slot];
	database.SaveInventory(CharacterID(), Inst, Slot);
	EQApplicationPacket* outapp2;

	//if(Quantity < Charges)
	outapp2 = new EQApplicationPacket(OP_MoveItem,sizeof(MoveItem_Struct));
	MoveItem_Struct* mis = (MoveItem_Struct*)outapp2->pBuffer;
	mis->from_slot = Slot;
	mis->to_slot = 0xFFFFFFFF;
	mis->number_in_stack = 0xFFFFFFFF;

	if(Quantity >= Charges)
		Quantity = 1;

	for(int i = 0; i < Quantity; i++) {

		this->QueuePacket(outapp2);
	}
	safe_delete(outapp2);

}

void Client::FindAndNukeTraderItem(int32 ItemID, uint16 Quantity, Client* Customer, uint16 SlotID){

	uint32 SellerID = this->GetID();
	const ItemInst* item= nullptr;
	bool Stackable = false;
	int16 Charges=0;

	if(SlotID > 0){

		item = this->GetInv().GetItem(SlotID);

		if(item) {
			Charges = this->GetInv().GetItem(SlotID)->GetCharges();

			Stackable = item->IsStackable();

			if(!Stackable)
				Quantity = (Charges > 0) ? Charges : 1;

			Log.Out(Logs::Detail, Logs::Bazaar, "FindAndNuke %s, Charges %i, Quantity %i", item->GetItem()->Name, Charges, Quantity);
		}
		if(item && (Charges <= Quantity || (Charges <= 0 && Quantity==1) || !Stackable)){
			this->DeleteItemInInventory(SlotID, Quantity);

			TraderCharges_Struct* GetSlot = database.LoadTraderItemWithCharges(this->CharacterID());

			uint8 Count = 0;

			bool TestSlot = true;

			for(int y = 0;y < 80;y++){

				if(TestSlot && GetSlot->ItemID[y] == ItemID){

					database.DeleteTraderItem(this->CharacterID(),y);
					NukeTraderItem(SlotID, Charges, Quantity, Customer,y,GetSlot->ItemID[y], SellerID);
					TestSlot=false;
				}
				else if(GetSlot->ItemID[y] > 0)
					Count++;
			}
			if(Count == 0)
				Trader_EndTrader();

			return;
		}
		else if(item) {
			TraderCharges_Struct* GetSlot = database.LoadTraderItemWithCharges(this->CharacterID());

			for(int y = 0;y < 80;y++){
				if(GetSlot->ItemID[y] == ItemID){
					database.UpdateTraderItemCharges(this->CharacterID(), item->GetSerialNumber(), Charges-Quantity);
					NukeTraderItem(SlotID, Charges, Quantity, Customer,y,GetSlot->ItemID[y], SellerID);
					return;
				}
			}
		}
	}
	Log.Out(Logs::Detail, Logs::Bazaar, "Could NOT find a match for Item: %i with a quantity of: %i on Trader: %s\n",ItemID,
					Quantity,this->GetName());
}

void Client::ReturnTraderReq(const EQApplicationPacket* app, int16 TraderItemCharges, int TraderSlot, uint32 Price){

	TraderBuy_Struct* tbs = (TraderBuy_Struct*)app->pBuffer;
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_TraderBuy, sizeof(TraderBuy_Struct));
	TraderBuy_Struct* outtbs = (TraderBuy_Struct*)outapp->pBuffer;
	memcpy(outtbs, tbs, app->size);
	outtbs->Price = Price;
	outtbs->Quantity = TraderItemCharges;
	outtbs->TraderID = this->GetID();
	outtbs->AlreadySold = TraderSlot;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::TradeRequestFailed(const EQApplicationPacket* app) {

	TraderBuy_Struct* tbs = (TraderBuy_Struct*)app->pBuffer;
	EQApplicationPacket* outapp = new EQApplicationPacket(OP_TraderBuy, sizeof(TraderBuy_Struct));
	TraderBuy_Struct* outtbs = (TraderBuy_Struct*)outapp->pBuffer;
	memcpy(outtbs, tbs, app->size);
	outtbs->AlreadySold = 0xFFFFFFFF;
	outtbs->TraderID = 0xFFFFFFFF;

	QueuePacket(outapp);
	safe_delete(outapp);
}


static void BazaarAuditTrail(const char *seller, const char *buyer, const char *itemName, int quantity, int totalCost, int tranType) {

	std::string query = StringFormat("INSERT INTO `trader_audit` "
                                    "(`time`, `seller`, `buyer`, `itemname`, `quantity`, `totalcost`, `trantype`) "
                                    "VALUES (NOW(), '%s', '%s', '%s', %i, %i, %i)",
                                    seller, buyer, itemName, quantity, totalCost, tranType);
	database.QueryDatabase(query);
}



void Client::BuyTraderItem(TraderBuy_Struct* tbs,Client* Trader,const EQApplicationPacket* app){

	if(!Trader) return;

	if(!Trader->IsTrader()) {
		TradeRequestFailed(app);
		return;
	}

	EQApplicationPacket* outapp = new EQApplicationPacket(OP_Trader,sizeof(TraderBuy_Struct));

	TraderBuy_Struct* outtbs = (TraderBuy_Struct*)outapp->pBuffer;

	outtbs->ItemID = tbs->ItemID;

	const ItemInst* BuyItem;
	BuyItem = Trader->FindTraderItemByID(tbs->ItemID);

	if(!BuyItem) {
		Log.Out(Logs::Detail, Logs::Bazaar, "Unable to find item on trader.");
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	uint32 priceper = tbs->Price / tbs->Quantity;
	outtbs->Price = tbs->Price;
	Log.Out(Logs::Detail, Logs::Bazaar, "Buyitem: Name: %s, IsStackable: %i, Requested Quantity: %i, Charges on Item %i Price: %i Slot: %i Price per item: %i",
					BuyItem->GetItem()->Name, BuyItem->IsStackable(), tbs->Quantity, BuyItem->GetCharges(), tbs->Price, tbs->AlreadySold, priceper);
	// If the item is not stackable, then we can only be buying one of them.
	if(!BuyItem->IsStackable())
		outtbs->Quantity = tbs->Quantity;
	else {
		// Stackable items, arrows, diamonds, etc
		int ItemCharges = BuyItem->GetCharges();

		// ItemCharges for stackables should not be <= 0
		if(ItemCharges <= 0)
			outtbs->Quantity = 1;
		// If the purchaser requested more than is in the stack, just sell them how many are actually in the stack.
		else if(ItemCharges < (int16)tbs->Quantity)
		{
			outtbs->Price =  tbs->Price - ((tbs->Quantity - ItemCharges) * priceper);
			outtbs->Quantity = ItemCharges;
		}
		else
			outtbs->Quantity = tbs->Quantity;
	}

	Log.Out(Logs::Detail, Logs::Bazaar, "Actual quantity that will be traded is %i for cost: %i", outtbs->Quantity, outtbs->Price);

	if(outtbs->Price <= 0) {
		Message(CC_Red, "Internal error. Aborting trade. Please report this to the ServerOP. Error code is 1");
		Trader->Message(CC_Red, "Internal error. Aborting trade. Please report this to the ServerOP. Error code is 1");
		Log.Out(Logs::General, Logs::Error, "Bazaar: Zero price transaction between %s and %s aborted."
						"Item: %s, Charges: %i, TBS: Qty %i, Price: %i",
						GetName(), Trader->GetName(),
						BuyItem->GetItem()->Name, BuyItem->GetCharges(), outtbs->Quantity, outtbs->Price);
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	//uint64 TotalTransactionValue = static_cast<uint64>(tbs->Price) * static_cast<uint64>(outtbs->Quantity);

	if(outtbs->Price > MAX_TRANSACTION_VALUE) {
		Message(CC_Red, "That would exceed the single transaction limit of %u platinum.", MAX_TRANSACTION_VALUE / 1000);
		TradeRequestFailed(app);
		safe_delete(outapp);
		return;
	}

	int TraderSlot = tbs->AlreadySold;
	ReturnTraderReq(app, outtbs->Quantity, TraderSlot, outtbs->Price);

	outtbs->TraderID = this->GetID();
	outtbs->Action = BazaarBuyItem;
	strn0cpy(outtbs->ItemName, BuyItem->GetItem()->Name, 64);

	int SlotID = 0;

	if(BuyItem->IsStackable())
		SendTraderItem(BuyItem->GetItem()->ID, outtbs->Quantity);
	else
		SendTraderItem(BuyItem->GetItem()->ID, BuyItem->GetCharges());

	EQApplicationPacket* outapp2 = new EQApplicationPacket(OP_MoneyUpdate,sizeof(MoneyUpdate_Struct));
	MoneyUpdate_Struct* mus= (MoneyUpdate_Struct*)outapp2->pBuffer;

	// This cannot overflow assuming MAX_TRANSACTION_VALUE, checked above, is the default of 2000000000
	uint32 TotalCost = outtbs->Price;
	this->TakeMoneyFromPP(TotalCost);

	mus->platinum = TotalCost / 1000;
	TotalCost -= (mus->platinum * 1000);
	mus->gold = TotalCost / 100;
	TotalCost -= (mus->gold * 100);
	mus->silver = TotalCost / 10;
	TotalCost -= (mus->silver * 10);
	mus->copper = TotalCost;

	bool updateclient = true;
	Trader->AddMoneyToPP(mus->copper,mus->silver,mus->gold,mus->platinum,updateclient);

	mus->platinum = Trader->GetPlatinum();
	mus->gold = Trader->GetGold();
	mus->silver = Trader->GetSilver();
	mus->copper = Trader->GetCopper();
	SlotID = Trader->FindTraderItem(tbs->ItemID, outtbs->Quantity);

	if(RuleB(Bazaar, AuditTrail))
		BazaarAuditTrail(Trader->GetName(), GetName(), BuyItem->GetItem()->Name, outtbs->Quantity, outtbs->Price, 0);

	Trader->FindAndNukeTraderItem(tbs->ItemID, outtbs->Quantity, this, SlotID);
	Trader->Trader_CustomerBought(this,TotalCost,tbs->ItemID,outtbs->Quantity,BuyItem->GetItem()->Name);

	Trader->QueuePacket(outapp);

	safe_delete(outapp);
	safe_delete(outapp2);

}

void Client::SendBazaarWelcome()
{
	const std::string query = "SELECT COUNT(DISTINCT char_id), count(char_id) FROM trader";
	auto results = database.QueryDatabase(query);
	if (results.Success() && results.RowCount() == 1){
		auto row = results.begin();

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_BazaarSearch, sizeof(BazaarWelcome_Struct));
		memset(outapp->pBuffer,0,outapp->size);
		BazaarWelcome_Struct* bws = (BazaarWelcome_Struct*)outapp->pBuffer;
		bws->Beginning.Action = BazaarWelcome;
		bws->Traders = atoi(row[0]);
		bws->Items = atoi(row[1]);

		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

void Client::SendBazaarResults(uint32 TraderID, uint32 Class_, uint32 Race, uint32 ItemStat, uint32 Slot, uint32 Type,
					char Name[64], uint32 MinPrice, uint32 MaxPrice) {

	std::string searchValues = " COUNT(item_id), trader.char_id, trader.item_id, trader.item_cost, items.name ";
	std::string searchCriteria = " WHERE trader.item_id = items.id ";

	if(TraderID > 0) {
		Client* trader = entity_list.GetClientByID(TraderID);

		if(trader)
			searchCriteria.append(StringFormat(" AND trader.char_id = %i", trader->CharacterID()));
	}

	if(MinPrice != 0)
		searchCriteria.append(StringFormat(" AND trader.item_cost >= %i", MinPrice));

	if(MaxPrice != 0)
		searchCriteria.append(StringFormat(" AND trader.item_cost <= %i", MaxPrice));

	if(strlen(Name) > 0) {
		char *safeName = RemoveApostrophes(Name);
		searchCriteria.append(StringFormat(" AND items.name LIKE '%%%s%%'", safeName));
		safe_delete_array(safeName);
	}

	if(Class_ != 0xFFFF)
		searchCriteria.append(StringFormat(" AND MID(REVERSE(BIN(items.classes)), %i, 1) = 1", Class_));

	if(Race != 0xFFFF)
		searchCriteria.append(StringFormat(" AND MID(REVERSE(BIN(items.races)), %i, 1) = 1", Race));

	if(Slot != 0xFFFF)
		searchCriteria.append(StringFormat(" AND MID(REVERSE(BIN(items.slots)), %i, 1) = 1", Slot + 1));

	switch(Type){
        case 0xFFFF:
            break;
        case 0:
            // 1H Slashing
            searchCriteria.append(" AND items.itemtype = 0 AND damage > 0");
            break;
        case 31:
            searchCriteria.append(" AND items.itemclass = 2");
            break;
        case 46:
            searchCriteria.append(" AND items.spellid > 0 AND items.spellid < 65000");
            break;
        case 47:
            searchCriteria.append(" AND items.spellid = 998");
            break;
        case 48:
            searchCriteria.append(" AND items.spellid >= 1298 AND items.spellid <= 1307");
            break;
        case 49:
            searchCriteria.append(" AND items.focuseffect > 0");
            break;

        default:
            searchCriteria.append(StringFormat(" AND items.itemtype = %i", Type));
    }

	switch(ItemStat) {

		case STAT_AC:
			searchCriteria.append(" AND items.ac > 0");
			searchValues.append(", items.ac");
			break;

		case STAT_AGI:
			searchCriteria.append(" AND items.aagi > 0");
			searchValues.append(", items.aagi");
			break;

		case STAT_CHA:
			searchCriteria.append(" AND items.acha > 0");
			searchValues.append(", items.acha");
			break;

		case STAT_DEX:
			searchCriteria.append(" AND items.adex > 0");
			searchValues.append(", items.adex");
			break;

		case STAT_INT:
			searchCriteria.append(" AND items.aint > 0");
			searchValues.append(", items.aint");
			break;

		case STAT_STA:
			searchCriteria.append(" AND items.asta > 0");
			searchValues.append(", items.asta");
			break;

		case STAT_STR:
			searchCriteria.append(" AND items.astr > 0");
			searchValues.append(", items.astr");
			break;

		case STAT_WIS:
			searchCriteria.append(" AND items.awis > 0");
			searchValues.append(", items.awis");
			break;

		case STAT_COLD:
			searchCriteria.append(" AND items.cr > 0");
			searchValues.append(", items.cr");
			break;

		case STAT_DISEASE:
			searchCriteria.append(" AND items.dr > 0");
			searchValues.append(", items.dr");
			break;

		case STAT_FIRE:
			searchCriteria.append(" AND items.fr > 0");
			searchValues.append(", items.fr");
			break;

		case STAT_MAGIC:
            searchCriteria.append(" AND items.mr > 0");
			searchValues.append(", items.mr");
			break;

		case STAT_POISON:
			searchCriteria.append(" AND items.pr > 0");
			searchValues.append(", items.pr");
			break;

		case STAT_HP:
			searchCriteria.append(" AND items.hp > 0");
			searchValues.append(", items.hp");
			break;

		case STAT_MANA:
			searchCriteria.append(" AND items.mana > 0");
			searchValues.append(", items.mana");
			break;

		case STAT_ENDURANCE:
			searchCriteria.append(" AND items.endur > 0");
			searchValues.append(", items.endur");
			break;

		case STAT_ATTACK:
			searchCriteria.append(" AND items.attack > 0");
			searchValues.append(", items.attack");
			break;

		case STAT_HP_REGEN:
			searchCriteria.append(" AND items.regen > 0");
			searchValues.append(", items.regen");
			break;

		case STAT_MANA_REGEN:
			searchCriteria.append(" AND items.manaregen > 0");
			searchValues.append(", items.manaregen");
			break;

		case STAT_HASTE:
			searchCriteria.append(" AND items.haste > 0");
			searchValues.append(", items.haste");
			break;

		case STAT_DAMAGE_SHIELD:
			searchCriteria.append(" AND items.damageshield > 0");
			searchValues.append(", items.damageshield");
			break;

		default:
			searchValues.append(", 0");
			break;
	}

    std::string query = StringFormat("SELECT %s, SUM(charges), items.stackable "
                                    "FROM trader, items %s GROUP BY items.id, charges, char_id LIMIT %i",
                                    searchValues.c_str(), searchCriteria.c_str(), RuleI(Bazaar, MaxSearchResults));
    auto results = database.QueryDatabase(query);
	Log.Out(Logs::Detail, Logs:: Bazaar, "Query was: SELECT %s, SUM(charges), items.stackable FROM trader, items %s GROUP BY items.id, charges, char_id LIMIT %i", searchValues.c_str(), searchCriteria.c_str(), RuleI(Bazaar, MaxSearchResults));
    if (!results.Success()) {
		return;
    }

    Log.Out(Logs::Detail, Logs::Trading, "SRCH: %s", query.c_str());

    int Size = 0;
    uint32 ID = 0;

    if (results.RowCount() == static_cast<unsigned long>(RuleI(Bazaar, MaxSearchResults)))
			Message(CC_Yellow, "Your search reached the limit of %i results. Please narrow your search down by selecting more options.",
					RuleI(Bazaar, MaxSearchResults));

    if(results.RowCount() == 0) {
		EQApplicationPacket* outapp2 = new EQApplicationPacket(OP_BazaarSearch, sizeof(BazaarReturnDone_Struct));
		BazaarReturnDone_Struct* brds = (BazaarReturnDone_Struct*)outapp2->pBuffer;
		brds->TraderID = ID;
		brds->Type = BazaarSearchDone;
		brds->Unknown008 = 0xFFFFFFFF;
		brds->Unknown012 = 0xFFFFFFFF;
		brds->Unknown016 = 0xFFFFFFFF;
		this->QueuePacket(outapp2);
		safe_delete(outapp2);
		return;
	}

	Size = sizeof(OldBazaarSearchResults_Struct);
	uchar *buffer = new uchar[Size];

	OldBazaarSearchResults_Struct* bsrs = (OldBazaarSearchResults_Struct*)buffer;

	for (auto row = results.begin(); row != results.end(); ++row) {
		memset(buffer, 0, Size);

		bsrs->Action = BazaarSearchResults;
		bsrs->NumItems = atoi(row[0]);
		Client* Trader2=entity_list.GetClientByCharID(atoi(row[1]));
		if(Trader2)
		{
			bsrs->SellerID = Trader2->GetID();
		}
		else
		{
			Log.Out(Logs::Detail, Logs::Bazaar, "Unable to find trader: %i\n",atoi(row[1]));
		}

		bsrs->ItemID = atoi(row[2]);
		bsrs->Cost = atoul(row[3]);
		memcpy(bsrs->ItemName, row[4], strlen(row[4]));

		Log.Out(Logs::Detail, Logs:: Bazaar, "Adding item: %s (%d) with cost: %d to results.", bsrs->ItemName, bsrs->ItemID, bsrs->Cost);
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_BazaarSearch, Size);
		memcpy(outapp->pBuffer, buffer, Size);

		this->QueuePacket(outapp);
		safe_delete(outapp);
	}

	EQApplicationPacket* outapp2 = new EQApplicationPacket(OP_BazaarSearch, sizeof(BazaarReturnDone_Struct));
	BazaarReturnDone_Struct* brds = (BazaarReturnDone_Struct*)outapp2->pBuffer;

	brds->TraderID = ID;
	brds->Type = BazaarSearchDone;
	brds->Unknown008 = 0xFFFFFFFF;
	brds->Unknown012 = 0xFFFFFFFF;
	brds->Unknown016 = 0xFFFFFFFF;

	this->QueuePacket(outapp2);
	safe_delete(outapp2);
}

static void UpdateTraderCustomerItemsAdded(uint32 SellerID, uint32 CustomerID, TraderCharges_Struct* gis, uint32 ItemID) {

	// Send Item packets to the customer to update the Merchant window with the
	// new items for sale, and give them a message in their chat window.

	Client* Customer = entity_list.GetClientByID(CustomerID);
	Client* Seller = entity_list.GetClientByID(SellerID);
	uint32 SellerCharID = Seller->CharacterID();

	if(!Customer) return;

	const Item_Struct *item = database.GetItem(ItemID);

	if(!item) return;

	ItemInst* inst = database.CreateItem(item);

	if(!inst) return;

	Customer->Message(CC_Red, "The Trader has put up %s for sale.", item->Name);

	for(int i = 0; i < 80; i++) {

		if(gis->ItemID[i] == ItemID) {

			inst->SetCharges(gis->Charges[i]);
			inst->SetPrice(gis->ItemCost[i]);
			inst->SetSerialNumber(gis->SerialNumber[i]);
			inst->SetMerchantSlot(gis->SerialNumber[i]);

			if(inst->IsStackable())
				inst->SetMerchantCount(gis->Charges[i]);

			Log.Out(Logs::Detail, Logs::Bazaar, "Sending price update for %s, Serial No. %i with %i charges",
							item->Name, gis->SerialNumber[i], gis->Charges[i]);

			Customer->SendItemPacket(30, inst, ItemPacketMerchant); // MainCursor?
		}
	}

	safe_delete(inst);
}

static void UpdateTraderCustomerPriceChanged(uint32 SellerID, uint32 CustomerID, TraderCharges_Struct* gis, uint32 ItemID, int32 Charges, uint32 NewPrice) {

	// Send ItemPackets to update the customer's Merchant window with the new price (or remove the item if
	// the new price is 0) and inform them with a chat message.

	Client* Customer = entity_list.GetClientByID(CustomerID);
	Client* Seller = entity_list.GetClientByID(SellerID);
	uint32 SellerCharID = Seller->CharacterID();

	if(!Customer) return;

	const Item_Struct *item = database.GetItem(ItemID);

	if(!item) return;

	if(NewPrice == 0) {

		// If the new price is 0, remove the item(s) from the window.
			EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopDelItem, sizeof(Merchant_DelItem_Struct));
			Merchant_DelItem_Struct* tdis = (Merchant_DelItem_Struct*)outapp->pBuffer;
			
			tdis->playerid = 0;
			tdis->npcid = Customer->GetID();
			tdis->type=65;
			Customer->Message(CC_Red, "The Trader has withdrawn the %s from sale.", item->Name);

			for(int i = 0; i < 80; i++) {
				if(gis->ItemID[i] == ItemID) {
					tdis->itemslot = i;
					Log.Out(Logs::Detail, Logs::Bazaar, "Telling customer to remove item %i with %i charges.", ItemID, Charges);

					Customer->QueuePacket(outapp);
				}
			}
			safe_delete(outapp);
			return;

	}
	Log.Out(Logs::Detail, Logs::Bazaar, "Sending price updates to customer %s", Customer->GetName());

	ItemInst* inst = database.CreateItem(item);

	if(!inst) return;

	if(Charges > 0)
		inst->SetCharges(Charges);

	inst->SetPrice(NewPrice);

	if(inst->IsStackable())
		inst->SetMerchantCount(Charges);

	// Let the customer know the price in the window has suddenly just changed on them.
	Customer->Message(CC_Red, "The Trader has changed the price of %s.", item->Name);

	for(int i = 0; i < 80; i++) {
		if((gis->ItemID[i] != ItemID) ||
			((!item->Stackable) && (gis->Charges[i] != Charges)))
			continue;

		inst->SetSerialNumber(gis->SerialNumber[i]);
		inst->SetMerchantSlot(gis->SerialNumber[i]);

		Log.Out(Logs::Detail, Logs::Bazaar, "Sending price update for %s, Serial No. %i with %i charges",
						item->Name, gis->SerialNumber[i], gis->Charges[i]);

		Customer->SendItemPacket(30, inst, ItemPacketMerchant); // MainCursor??
	}
	safe_delete(inst);
}

void Client::HandleTraderPriceUpdate(const EQApplicationPacket *app) {

	// Handle price updates from the Trader and update a customer browsing our stuff if necessary
	// This method also handles removing items from sale and adding them back up whilst still in
	// Trader mode.
	TraderPriceUpdate_Struct* tpus = (TraderPriceUpdate_Struct*)app->pBuffer;

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_Trader, sizeof(Trader_ShowItems_Struct));
	Trader_ShowItems_Struct* tsis=(Trader_ShowItems_Struct*)outapp->pBuffer;

	tsis->TraderID = this->GetID();
	tsis->Code = tpus->Action;

	Log.Out(Logs::Detail, Logs::Bazaar, "Received Price Update for %s, Item Serial No. %i, New Price %i",
					GetName(), tpus->SerialNumber, tpus->NewPrice);

	// Pull the items this Trader currently has for sale from the trader table.
	//
	TraderCharges_Struct* gis = database.LoadTraderItemWithCharges(CharacterID());

	if(!gis) {
		Log.Out(Logs::Detail, Logs::Bazaar, "[CLIENT] Error retrieving Trader items details to update price.");
		return;
	}

	// The client only sends a single update with the Serial Number of the item whose price has been updated.
	// We must update the price for all the Trader's items that are identical to that one item, i.e.
	// if it is a stackable item like arrows, update the price for all stacks. If it is not stackable, then
	// update the prices for all items that have the same number of charges.
	uint32 IDOfItemToUpdate = 0;
	int32 ChargesOnItemToUpdate = 0;
	uint32 OldPrice = 0;
	uint32 SellerID = this->GetID();

	for(int i = 0; i < 80; i++) {

		if((gis->ItemID[i] > 0) && (gis->ItemID[i] == tpus->SerialNumber)) {
			// We found the item that the Trader wants to change the price of (or add back up for sale).
			//
			Log.Out(Logs::Detail, Logs::Bazaar, "ItemID is %i, Charges is %i", gis->ItemID[i], gis->Charges[i]);

			IDOfItemToUpdate = gis->ItemID[i];
			ChargesOnItemToUpdate = gis->Charges[i];
			OldPrice = gis->ItemCost[i];

			break;
		}
	}

	if(IDOfItemToUpdate == 0) {

		// If the item is not currently in the trader table for this Trader, then they must have removed it from sale while
		// still in Trader mode. Check if the item is in their Trader Satchels, and if so, put it back up.

		// Quick Sanity check. If the item is not currently up for sale, and the new price is zero, just ack the packet
		// and do nothing.
		if(tpus->NewPrice == 0) {
			tsis->SubAction = BazaarPriceChange_RemoveItem;
			QueuePacket(outapp);
			safe_delete(gis);
			return ;
		}

		Log.Out(Logs::Detail, Logs::Bazaar, "Unable to find item to update price for. Rechecking trader satchels. If you removed an item while in trader mode, this message can be ignored!");

		// Find what is in their Trader Satchels
		GetItems_Struct* newgis=GetTraderItems();

		uint32 IDOfItemToAdd = 0;
		int32 ChargesOnItemToAdd = 0;

		for(int i = 0; i < 80; i++) {

			if((newgis->Items[i] > 0) && (newgis->Items[i] == tpus->SerialNumber)) {

				Log.Out(Logs::Detail, Logs::Bazaar, "Found new Item to Add, ItemID is %i, Charges is %i", newgis->Items[i],
								newgis->Charges[i]);

				IDOfItemToAdd = newgis->Items[i];
				ChargesOnItemToAdd = newgis->Charges[i];

				break;
			}
		}


		const Item_Struct *item = 0;

		if(IDOfItemToAdd)
			item = database.GetItem(IDOfItemToAdd);

		if(!IDOfItemToAdd || !item) {

			Log.Out(Logs::Detail, Logs::Bazaar, "Item not found in Trader Satchels either.");
			tsis->SubAction = BazaarPriceChange_Fail;
			QueuePacket(outapp);
			Trader_EndTrader();
			safe_delete(gis);
			safe_delete(newgis);
			return;
		}

		// It is a limitation of the client that if you have multiple of the same item, but with different charges,
		// although you can set different prices for them before entering Trader mode. If you Remove them and then
		// add them back whilst still in Trader mode, they all go up for the same price. We check for this situation
		// and give the Trader a warning message.
		//
		if(!item->Stackable) {

			bool SameItemWithDifferingCharges = false;

			for(int i = 0; i < 80; i++) {
				if((newgis->Items[i] == IDOfItemToAdd) && (newgis->Charges[i] != ChargesOnItemToAdd)) {

					SameItemWithDifferingCharges = true;
					break;
				}
			}

			if(SameItemWithDifferingCharges)
				Message(CC_Red, "Warning: You have more than one %s with different charges. They have all been added for sale "
						"at the same price.", item->Name);
		}

		// Now put all Items with a matching ItemID up for trade.
		//
		for(int i = 0; i < 80; i++) {

			if(newgis->Items[i] == IDOfItemToAdd) {

				database.SaveTraderItem(CharacterID(), newgis->Items[i], newgis->SerialNumber[i], newgis->Charges[i],
							tpus->NewPrice, i);

				gis->ItemID[i] = newgis->Items[i];
				gis->Charges[i] = newgis->Charges[i];
				gis->SerialNumber[i] = newgis->SerialNumber[i];
				gis->ItemCost[i] = tpus->NewPrice;

				Log.Out(Logs::Detail, Logs::Bazaar, "Adding new item for %s. ItemID %i, SerialNumber %i, Charges %i, Price: %i, Slot %i",
							GetName(), newgis->Items[i], newgis->SerialNumber[i], newgis->Charges[i],
							tpus->NewPrice, i);
			}
		}

		// If we have a customer currently browsing, update them with the new items.
		//
		if(CustomerID)
			UpdateTraderCustomerItemsAdded(SellerID, CustomerID, gis, IDOfItemToAdd);

		safe_delete(gis);
		safe_delete(newgis);

		// Acknowledge to the client.
		tsis->SubAction = BazaarPriceChange_AddItem;
		QueuePacket(outapp);

		return;
	}

	// This is a safeguard against a Trader increasing the price of an item while a customer is browsing and
	// unwittingly buying it at a higher price than they were expecting to.
	//
	if((OldPrice != 0) && (tpus->NewPrice > OldPrice) && CustomerID) {

		tsis->SubAction = BazaarPriceChange_Fail;
		QueuePacket(outapp);
		Trader_EndTrader();
		Message(CC_Red, "You must remove the item from sale before you can increase the price while a customer is browsing.");
		Message(CC_Red, "Click 'Begin Trader' to restart Trader mode with the increased price for this item.");
		safe_delete(gis);
		return;
	}

	// Send Acknowledgement back to the client.
	if(OldPrice == 0)
		tsis->SubAction = BazaarPriceChange_AddItem;
	else if(tpus->NewPrice != 0)
		tsis->SubAction = BazaarPriceChange_UpdatePrice;
	else
		tsis->SubAction = BazaarPriceChange_RemoveItem;

	QueuePacket(outapp);

	if(OldPrice == tpus->NewPrice) {
		Log.Out(Logs::Detail, Logs::Bazaar, "The new price is the same as the old one.");
		safe_delete(gis);
		return;
	}
	// Update the price for all items we have for sale that have this ItemID and number of charges, or remove
	// them from the trader table if the new price is zero.
	//
	database.UpdateTraderItemPrice(CharacterID(), IDOfItemToUpdate, ChargesOnItemToUpdate, tpus->NewPrice);

	// If a customer is browsing our goods, send them the updated prices / remove the items from the Merchant window
	if(CustomerID)
		UpdateTraderCustomerPriceChanged(SellerID, CustomerID, gis, IDOfItemToUpdate, ChargesOnItemToUpdate, tpus->NewPrice);

	safe_delete(gis);

}
