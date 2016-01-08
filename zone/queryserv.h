#ifndef QUERYSERV_ZONE_H
#define QUERYSERV_ZONE_H

/*
	enum PlayerGenericLogEventTypes
	These Enums are for the generic logging table that are not complex and require more advanced logic
*/

enum PlayerGenericLogEventTypes {
	Player_Log_Quest = 1,
	Player_Log_Zoning,
	Player_Log_Connect_State,
	Player_Log_Levels,
	Player_Log_Keyring_Addition,
	//Player_Log_QGlobal_Update,
	//Player_Log_Task_Updates,
	//Player_Log_AA_Purchases,
	//Player_Log_Trade_Skill_Events,
	//Player_Log_Issued_Commands,
	//Player_Log_Money_Transactions,
	//Player_Log_Alternate_Currency_Transactions,
};

class QueryServ {
	public:
		QueryServ();
		~QueryServ();
		void SendQuery(std::string Query);
		void PlayerLogEvent(int Event_Type, int Character_ID, std::string Event_Desc);

		void QSQGlobalUpdate(uint32 char_id, uint32 zone_id, int32 instance_id, const char* varname, const char* newvalue);
		void QSAAPurchases(uint32 char_id, uint32 zone_id, int32 instance_id, char aa_type[8], char aa_name[128], uint32 aa_id, uint32 cost);
		void QSAARate(uint32 char_id, uint32 aapoints, uint32 last_unspentAA);
		void QSDeathBy(uint32 char_id, uint32 zone_id, int32 instance_id, char killed_by[128], uint16 spell, int32 damage);
		void QSTSEvents(uint32 char_id, uint32 zone_id, int32 instance_id, const char results[8], uint32 recipe, uint32 tradeskill, uint16 trivial, float chance);
		void QSMerchantTransactions(uint32 char_id, uint32 zone_id, int16 slot_id, uint32 item_id, uint8 charges,
			uint32 merchant_id, int32 merchant_plat, int32 merchant_gold, int32 merchant_silver, int32 merchant_copper, uint16 merchant_count,
			int32 char_plat, int32 char_gold, int32 char_silver, int32 char_copper, uint16 char_count);
		void QSLootRecords(uint32 char_id, const char* corpsename, const char* type, uint32 zone_id, uint32 item_id, const char* item_name, int16 charges, int32 platinum, int32 gold, int32 silver, int32 copper);
		void QSNPCKills();
		void QSTradeItems();
		void QSHandinItems();
		void QSDeleteItems();
		void QSMoveItems();
};

#endif /* QUERYSERV_ZONE_H */
