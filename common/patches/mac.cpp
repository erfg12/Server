#include "../global_define.h"
#include "../eqemu_logsys.h"
#include "mac.h"
#include "../opcodemgr.h"
#include "../eq_stream_ident.h"
#include "../crc32.h"

#include "../eq_packet_structs.h"
#include "../packet_dump_file.h"
#include "../misc_functions.h"
#include "../packet_functions.h"
#include "../string_util.h"
#include "../item.h"
#include "mac_structs.h"
#include "../rulesys.h"

namespace Mac {

	static const char *name = "Mac";
	static OpcodeManager *opcodes = nullptr;
	static Strategy struct_strategy;

	structs::Item_Struct* MacItem(const ItemInst *inst, int16 slot_id_in, int type = 0);
	structs::Spawn_Struct* MacSpawns(struct Spawn_Struct*, int type);

	static inline int16 ServerToMacSlot(uint32 ServerSlot);
	static inline int16 ServerToMacCorpseSlot(uint32 ServerCorpse);

	static inline uint32 MacToServerSlot(int16 MacSlot);
	static inline uint32 MacToServerCorpseSlot(int16 MacCorpse);

	void Register(EQStreamIdentifier &into)
	{
		//create our opcode manager if we havent already
		if(opcodes == nullptr) 
		{
			std::string opfile = "patch_";
			opfile += name;
			opfile += ".conf";
			//load up the opcode manager.
			//TODO: figure out how to support shared memory with multiple patches...
			opcodes = new RegularOpcodeManager();
			if(!opcodes->LoadOpcodes(opfile.c_str())) 
			{
				Log.Out(Logs::General, Logs::Netcode, "[OPCODES] Error loading opcodes file %s. Not registering patch %s.", opfile.c_str(), name);
				return;
			}
		}

		//ok, now we have what we need to register.

		EQStream::Signature signature;
		std::string pname;

		pname = std::string(name) + "_world";
		//register our world signature.
		signature.first_length = sizeof(structs::LoginInfo_Struct);
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_SendLoginInfo);
		into.RegisterOldPatch(signature, pname.c_str(), &opcodes, &struct_strategy);

		pname = std::string(name) + "_zone";
		//register our zone signature.
		signature.first_length = sizeof(structs::SetDataRate_Struct);
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_DataRate);
		into.RegisterOldPatch(signature, pname.c_str(), &opcodes, &struct_strategy);
		
		Log.Out(Logs::General, Logs::Netcode, "[IDENTIFY] Registered patch %s", name);
	}

	void Reload() 
	{

		//we have a big problem to solve here when we switch back to shared memory
		//opcode managers because we need to change the manager pointer, which means
		//we need to go to every stream and replace it's manager.

		if(opcodes != nullptr) 
		{
			//TODO: get this file name from the config file
			std::string opfile = "patch_";
			opfile += name;
			opfile += ".conf";
			if(!opcodes->ReloadOpcodes(opfile.c_str()))
			{
				Log.Out(Logs::General, Logs::Netcode, "[OPCODES] Error reloading opcodes file %s for patch %s.", opfile.c_str(), name);
				return;
			}
			Log.Out(Logs::General, Logs::Netcode, "[OPCODES] Reloaded opcodes for patch %s", name);
		}
	}



	Strategy::Strategy()
	: StructStrategy()
	{
		//all opcodes default to passthrough.
		#include "ss_register.h"
		#include "mac_ops.h"
	}

	std::string Strategy::Describe() const 
	{
		std::string r;
		r += "Patch ";
		r += name;
		return(r);
	}

	#include "ss_define.h"

	const EQClientVersion Strategy::ClientVersion() const
	{
		return EQClientMac;
	}

	DECODE(OP_SendLoginInfo)
	{
		DECODE_LENGTH_EXACT(structs::LoginInfo_Struct);
		SETUP_DIRECT_DECODE(LoginInfo_Struct, structs::LoginInfo_Struct);
		memcpy(emu->login_info, eq->AccountName, 64);
		IN(zoning);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_EnterWorld) 
	{
		unsigned char *__eq_buffer = __packet->pBuffer;
		if(!__eq_buffer)
		{
			__packet->SetOpcode(OP_Unknown);
			return;
		}

		__packet->size = sizeof(structs::EnterWorld_Struct);
		__packet->pBuffer = new unsigned char[__packet->size]; 
		EnterWorld_Struct *emu = (EnterWorld_Struct*) __packet->pBuffer;
		structs::EnterWorld_Struct *eq = (structs::EnterWorld_Struct *) __eq_buffer;
		strn0cpy(emu->name, eq->charname, 64);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ZoneServerInfo) 
	{
		SETUP_DIRECT_ENCODE(ZoneServerInfo_Struct, structs::ZoneServerInfo_Struct);
		strcpy(eq->ip, emu->ip);
		eq->port = ntohs(emu->port);

		FINISH_ENCODE();
	}

	ENCODE(OP_ZoneEntry)
	{ 
		SETUP_DIRECT_ENCODE(ServerZoneEntry_Struct, structs::ServerZoneEntry_Struct);

			int k = 0;
			eq->zoneID = emu->player.spawn.zoneID;
			eq->anon = emu->player.spawn.anon;
			strncpy(eq->name, emu->player.spawn.name, 64);
			eq->deity = emu->player.spawn.deity;
			eq->race = emu->player.spawn.race;
			eq->size = emu->player.spawn.size;
			if(emu->player.spawn.race == 42)
				eq->size = emu->player.spawn.size + 2.0f;
			eq->NPC = emu->player.spawn.NPC;
			eq->invis = emu->player.spawn.invis;
			eq->max_hp = emu->player.spawn.max_hp;
			eq->curHP = emu->player.spawn.curHp;
			eq->x_pos = (float)emu->player.spawn.x;
			eq->y_pos = (float)emu->player.spawn.y;
			eq->z_pos = (float)emu->player.spawn.z + (eq->size * 0.625f - 4);
			eq->animation = emu->player.spawn.animation;
			eq->heading = (float)emu->player.spawn.heading;
			eq->haircolor = emu->player.spawn.haircolor;
			eq->beardcolor = emu->player.spawn.beardcolor;
			eq->eyecolor1 = emu->player.spawn.eyecolor1;
			eq->eyecolor2 = emu->player.spawn.eyecolor2;
			eq->hairstyle = emu->player.spawn.hairstyle;
			eq->beard = emu->player.spawn.beard;
			eq->face = emu->player.spawn.face;
			eq->level = emu->player.spawn.level;
			for(k = 0; k < 9; k++) 
			{
				eq->equipment[k] = emu->player.spawn.equipment[k];
				eq->equipcolors[k].color = emu->player.spawn.colors[k].color;
			}
			eq->AFK = emu->player.spawn.afk;
			eq->title = emu->player.spawn.aaitle;
			eq->anim_type = 0x64;
			eq->texture = emu->player.spawn.equip_chest2;
			eq->helm = emu->player.spawn.helm;
			eq->GM = emu->player.spawn.gm;
			eq->GuildID = emu->player.spawn.guildID;
			if(eq->GuildID == 0)
				eq->GuildID = 0xFFFF;
			eq->guildrank = emu->player.spawn.guildrank;
			if(eq->guildrank == 0)
				eq->guildrank = 0xFF;
			strncpy(eq->Surname, emu->player.spawn.lastName, 32);
			eq->walkspeed = emu->player.spawn.walkspeed;
			eq->runspeed = emu->player.spawn.runspeed;
			eq->light = emu->player.spawn.light;
			if(emu->player.spawn.class_ > 19 && emu->player.spawn.class_ < 35)
				eq->class_ = emu->player.spawn.class_-3;
			else if(emu->player.spawn.class_ == 40)
				eq->class_ = 16;
			else if(emu->player.spawn.class_ == 41)
				eq->class_ = 32;
			else 
				eq->class_ = emu->player.spawn.class_;
			eq->gender = emu->player.spawn.gender;
			eq->flymode = emu->player.spawn.flymode;
			eq->prev = 0xa0ae0e00;
			eq->next = 0xa0ae0e00;
			eq->view_height = 0x6666c640;
			eq->sprite_oheight = 0x00004840;
			eq->extra[10] = 0xFF;
			eq->extra[11] = 0xFF;
			eq->extra[12] = 0xFF;
			eq->extra[13] = 0xFF;
			eq->type = 0;
			eq->petOwnerId = emu->player.spawn.petOwnerId;

			CRC32::SetEQChecksum(__packet->pBuffer, sizeof(structs::ServerZoneEntry_Struct));

		FINISH_ENCODE();
	}

	ENCODE(OP_PlayerProfile) 
	{
		SETUP_DIRECT_ENCODE(PlayerProfile_Struct, structs::PlayerProfile_Struct);

		eq->available_slots=0xffff;

		int r = 0;
		OUT(gender);
		OUT(race);
		OUT(class_);
		OUT(level);
		for(r = 0; r < 5; r++) {
			eq->bind_point_zone[r] = emu->binds[r].zoneId;
			eq->bind_x[r] = emu->binds[r].x;
			eq->bind_y[r] = emu->binds[r].y;
			eq->bind_z[r] = emu->binds[r].z;
			eq->bind_heading[r] = emu->binds[r].heading;
		}
		OUT(deity);
		OUT(intoxication);
		OUT(haircolor);
		OUT(beardcolor);
		OUT(eyecolor1);
		OUT(eyecolor2);
		OUT(hairstyle);
		OUT(beard);
		OUT(points);
		OUT(mana);
		OUT(cur_hp);
		OUT(STR);
		OUT(STA);
		OUT(CHA);
		OUT(DEX);
		OUT(INT);
		OUT(AGI);
		OUT(WIS);
		OUT(face);
		eq->oldface = emu->face;
		OUT_array(spell_book, 256);
		OUT_array(mem_spells, 8);
		OUT(platinum);
		OUT(gold);
		OUT(silver);
		OUT(copper);
		OUT(platinum_cursor);
		OUT(gold_cursor);
		OUT(silver_cursor);
		OUT(copper_cursor);
		OUT_array(skills, structs::MAX_PP_SKILL);  // 1:1 direct copy (100 dword)

		//for(r = 0; r < 50; r++) {
		//	eq->innate[r] = 255;
		//}
		//OUT(ATR_PET_LOH_timer);
		//OUT(UnknownTimer);
		//OUT(HarmTouchTimer);
		int value = RuleI(Character,ConsumptionValue);

		float tpercent = (float)emu->thirst_level/(float)value;
		float tlevel =  127.0f*tpercent;
		eq->thirst_level = (int8)(tlevel + 0.5);

		float hpercent = (float)emu->hunger_level/(float)value;
		float hlevel =  127.0f*hpercent;
		eq->hunger_level = (int8)(hlevel + 0.5);

		for(r = 0; r < 15; r++)
		{
			eq->buffs[r].visable = (emu->buffs[r].spellid == 0xFFFFFFFF || emu->buffs[r].spellid == 0) ? 0 : 2;
			OUT(buffs[r].level);
			OUT(buffs[r].bard_modifier);
			OUT(buffs[r].spellid);
			OUT(buffs[r].duration);
		}
		OUT_str(name);
		strcpy(eq->Surname, emu->last_name);
		OUT(guild_id);
		OUT(birthday);
		OUT(lastlogin);
		OUT(timePlayedMin);
		OUT(pvp);
		OUT(anon);
		OUT(gm);
		OUT(guildrank);
		OUT(exp);
		OUT_array(languages, 26);
		OUT(x);
		OUT(y);
		OUT(z);
		OUT(heading);
		OUT(platinum_bank);
		OUT(gold_bank);
		OUT(silver_bank);
		OUT(copper_bank);
		OUT(level2);
		OUT(autosplit);
		eq->current_zone = emu->zone_id;
		OUT_str(boat);
		OUT(aapoints);
		OUT(expAA);
		OUT(perAA);
		OUT(air_remaining);
		if(emu->expansions > 15)
			eq->expansions = 15;
		else
			OUT(expansions);
		for(r = 0; r < structs::MAX_PP_AA_ARRAY; r++)
		{
			OUT(aa_array[r].AA);
			OUT(aa_array[r].value);
		}
		for(r = 0; r < 6; r++) 
		{
			OUT_str(groupMembers[r]);
		}
		for(r = 0; r < 9; r++) 
		{
			OUT(item_material[r]);
		}
		OUT(abilitySlotRefresh);
		OUT_array(spellSlotRefresh, structs::MAX_PP_MEMSPELL);
		eq->eqbackground = 0;

		//Log.Out(Logs::General, Logs::Netcode, "[STRUCTS] Player Profile Packet is %i bytes uncompressed", sizeof(structs::PlayerProfile_Struct));

		CRC32::SetEQChecksum(__packet->pBuffer, sizeof(structs::PlayerProfile_Struct)-4);
		EQApplicationPacket* outapp = new EQApplicationPacket();
		outapp->SetOpcode(OP_PlayerProfile);
		outapp->pBuffer = new uchar[10000];
		outapp->size = DeflatePacket((unsigned char*)__packet->pBuffer, sizeof(structs::PlayerProfile_Struct), outapp->pBuffer, 10000);
		EncryptProfilePacket(outapp->pBuffer, outapp->size);
		//Log.Out(Logs::General, Logs::Netcode, "[STRUCTS] Player Profile Packet is %i bytes compressed", outapp->size);
		dest->FastQueuePacket(&outapp);
		delete[] __emu_buffer;
		delete __packet;
	}

	DECODE(OP_CharacterCreate)
	{
		DECODE_LENGTH_EXACT(structs::CharCreate_Struct);
		SETUP_DIRECT_DECODE(CharCreate_Struct, structs::CharCreate_Struct);
		IN(class_);
		IN(beardcolor);
		IN(beard);
		IN(haircolor);
		IN(gender);
		IN(race);
		IN(start_zone);
		IN(hairstyle);
		IN(deity);
		IN(STR);
		IN(STA);
		IN(AGI);
		IN(DEX);
		IN(WIS);
		IN(INT);
		IN(CHA);
		IN(face);
		IN(eyecolor1);
		IN(eyecolor2);
		IN(oldface);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ApproveWorld)
	{
		SETUP_DIRECT_ENCODE(ApproveWorld_Struct, structs::ApproveWorld_Struct);
		eq->response = 0;
		FINISH_ENCODE();	
	}

	ENCODE(OP_EnterWorld)
	{
		SETUP_DIRECT_ENCODE(ApproveWorld_Struct, structs::ApproveWorld_Struct);
		eq->response = 0;
		FINISH_ENCODE();	
	}

	ENCODE(OP_ExpansionInfo)
	{
		SETUP_DIRECT_ENCODE(ExpansionInfo_Struct, structs::ExpansionInfo_Struct);
		if(emu->Expansions > 15)
			eq->Expansions = 15;
		else
			OUT(Expansions);
		FINISH_ENCODE();	
	}

	ENCODE(OP_SendCharInfo)
	{
		int r;
		ENCODE_LENGTH_EXACT(CharacterSelect_Struct);
		SETUP_DIRECT_ENCODE(CharacterSelect_Struct, structs::CharacterSelect_Struct);
		for(r = 0; r < 10; r++) 
		{
			OUT(zone[r]);
			OUT(eyecolor1[r]);
			OUT(eyecolor2[r]);
			OUT(hairstyle[r]);
			OUT(primary[r]);
			if(emu->race[r] > 300)
				eq->race[r] = 1;
			else
				eq->race[r] = emu->race[r];
			OUT(class_[r]);
			OUT_str(name[r]);
			OUT(gender[r]);
			OUT(level[r]);
			OUT(secondary[r]);
			OUT(face[r]);
			OUT(beard[r]);
			int k;
			for(k = 0; k < 9; k++) 
			{
				OUT(equip[r][k]);
				OUT(cs_colors[r][k].color);
			}
			OUT(haircolor[r]);
			OUT(deity[r]);
			OUT(beardcolor[r]);
		}
		FINISH_ENCODE();	
	}

	DECODE(OP_SetGuildMOTD)
	{
		SETUP_DIRECT_DECODE(GuildMOTD_Struct, structs::GuildMOTD_Struct);
		strcpy(emu->name,eq->name);
		strcpy(emu->motd,eq->motd);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_GuildMOTD) 
	{
		SETUP_DIRECT_ENCODE(GuildMOTD_Struct, structs::GuildMOTD_Struct);
		strcpy(eq->name,emu->name);
		strcpy(eq->motd,emu->motd);
		FINISH_ENCODE();
	}

	DECODE(OP_GuildInviteAccept)
	{
		SETUP_DIRECT_DECODE(GuildInviteAccept_Struct, structs::GuildInviteAccept_Struct);
		strcpy(emu->inviter,eq->inviter);
		strcpy(emu->newmember,eq->newmember);
		IN(response);
		emu->guildeqid = (int16)eq->guildeqid;
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_NewZone) 
	{
		SETUP_DIRECT_ENCODE(NewZone_Struct, structs::NewZone_Struct);
		OUT_str(char_name);
		OUT_str(zone_short_name);
		OUT_str(zone_long_name);
		OUT(ztype);
		OUT_array(fog_red, 4);
		OUT_array(fog_green, 4);
		OUT_array(fog_blue, 4);
		OUT_array(fog_minclip, 4);
		OUT_array(fog_maxclip, 4);
		OUT(gravity);
		OUT(time_type);
		OUT(sky);
		OUT(zone_exp_multiplier);
		OUT(safe_y);
		OUT(safe_x);
		OUT(safe_z);
		OUT(max_z);
		eq->underworld=emu->underworld;
		OUT(minclip);
		OUT(maxclip);
		OUT(skylock);
		OUT(timezone);
		OUT_array(snow_chance, 4);
		OUT_array(snow_duration, 4);
		OUT_array(rain_chance, 4);
		OUT_array(rain_duration, 4);
		FINISH_ENCODE();	
	}

	ENCODE(OP_SpecialMesg)
	{
		EQApplicationPacket *__packet = *p; 
		*p = nullptr; 
		unsigned char *__emu_buffer = __packet->pBuffer; 
		SpecialMesg_Struct *emu = (SpecialMesg_Struct *) __emu_buffer; 
		uint32 __i = 0; 
		__i++; /* to shut up compiler */
	
		int msglen = __packet->size - sizeof(structs::SpecialMesg_Struct);
		int len = sizeof(structs::SpecialMesg_Struct) + msglen + 1;
		__packet->pBuffer = new unsigned char[len]; 
		__packet->size = len; 
		memset(__packet->pBuffer, 0, len); 
		structs::SpecialMesg_Struct *eq = (structs::SpecialMesg_Struct *) __packet->pBuffer; 
		eq->msg_type = emu->msg_type;
		strcpy(eq->message, emu->message);
		FINISH_ENCODE();
	}

	DECODE(OP_TargetMouse)
	{
		SETUP_DIRECT_DECODE(ClientTarget_Struct, structs::ClientTarget_Struct);
		IN(new_target);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_TargetCommand)
	{
		SETUP_DIRECT_DECODE(ClientTarget_Struct, structs::ClientTarget_Struct);
		IN(new_target);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_Taunt)
	{
		SETUP_DIRECT_DECODE(ClientTarget_Struct, structs::ClientTarget_Struct);
		IN(new_target);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ZoneSpawns)
	{

		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;
		Spawn_Struct *emu = (Spawn_Struct *) __emu_buffer;

		//determine and verify length
		int entrycount = in->size / sizeof(Spawn_Struct);
		if(entrycount == 0 || (in->size % sizeof(Spawn_Struct)) != 0) 
		{
			Log.Out(Logs::General, Logs::Netcode, "[STRUCTS] Wrong size on outbound %s: Got %d, expected multiple of %d", opcodes->EmuToName(in->GetOpcode()), in->size, sizeof(Spawn_Struct));
			delete in;
			return;
		}
		EQApplicationPacket* out = new EQApplicationPacket();
		out->SetOpcode(OP_ZoneSpawns);
		//make the EQ struct.
		out->size = sizeof(structs::Spawn_Struct)*entrycount;
		out->pBuffer = new unsigned char[out->size];
		structs::Spawn_Struct *eq = (structs::Spawn_Struct *) out->pBuffer;

		//zero out the packet. We could avoid this memset by setting all fields (including unknowns)
		memset(out->pBuffer, 0, out->size);

		//do the transform...
		for(int r = 0; r < entrycount; r++, eq++, emu++) 
		{

			struct structs::Spawn_Struct* spawns = MacSpawns(emu,0);
			memcpy(eq,spawns,sizeof(structs::Spawn_Struct));
			safe_delete(spawns);

		}
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ZoneSpawns, sizeof(structs::Spawn_Struct)*entrycount);
		outapp->pBuffer = new uchar[sizeof(structs::Spawn_Struct)*entrycount];
		outapp->size = DeflatePacket((unsigned char*)out->pBuffer, out->size, outapp->pBuffer, sizeof(structs::Spawn_Struct)*entrycount);
		EncryptZoneSpawnPacket(outapp->pBuffer, outapp->size);
		delete[] __emu_buffer;
		delete out;
		dest->FastQueuePacket(&outapp, ack_req);

	}

	ENCODE(OP_NewSpawn) 
	{
		SETUP_DIRECT_ENCODE(Spawn_Struct, structs::Spawn_Struct);

		struct structs::Spawn_Struct* spawns = MacSpawns(emu,1);
		memcpy(eq,spawns,sizeof(structs::Spawn_Struct));
		safe_delete(spawns);

		EQApplicationPacket* outapp = new EQApplicationPacket(OP_NewSpawn, sizeof(structs::Spawn_Struct));
		outapp->pBuffer = new uchar[sizeof(structs::Spawn_Struct)];
		outapp->size = DeflatePacket((unsigned char*)__packet->pBuffer, __packet->size, outapp->pBuffer, sizeof(structs::Spawn_Struct));
		EncryptZoneSpawnPacket(outapp->pBuffer, outapp->size);
		dest->FastQueuePacket(&outapp, ack_req);
		delete[] __emu_buffer;
		safe_delete(__packet);

	}

	DECODE(OP_ZoneChange)
	{
		DECODE_LENGTH_EXACT(structs::ZoneChange_Struct);
		SETUP_DIRECT_DECODE(ZoneChange_Struct, structs::ZoneChange_Struct);
		memcpy(emu->char_name, eq->char_name, sizeof(emu->char_name));
		IN(zone_reason);
		IN(zoneID);
		IN(success);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ZoneChange)
	{
		ENCODE_LENGTH_EXACT(ZoneChange_Struct);
		SETUP_DIRECT_ENCODE(ZoneChange_Struct, structs::ZoneChange_Struct);

		memcpy(emu->char_name, eq->char_name, sizeof(emu->char_name));
		OUT(zoneID);
		OUT(success);
		if(emu->success != 1)
		{
			memset(eq->error,0xff,sizeof(eq->error));
		}
		FINISH_ENCODE();
	}

	ENCODE(OP_CancelTrade)
	{
		ENCODE_LENGTH_EXACT(CancelTrade_Struct);
		SETUP_DIRECT_ENCODE(CancelTrade_Struct, structs::CancelTrade_Struct);
		OUT(fromid);
		eq->action=1665;
		FINISH_ENCODE();
	}

	DECODE(OP_CancelTrade)
	{
		DECODE_LENGTH_EXACT(structs::TradeRequest_Struct);
		SETUP_DIRECT_DECODE(TradeRequest_Struct, structs::TradeRequest_Struct);
		IN(from_mob_id);
		IN(to_mob_id);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_MemorizeSpell)
	{
		SETUP_DIRECT_ENCODE(MemorizeSpell_Struct, structs::MemorizeSpell_Struct);
		OUT(slot);
		OUT(spell_id);
		OUT(scribing);
		FINISH_ENCODE();	
	}

	DECODE(OP_MemorizeSpell) 
	{
		DECODE_LENGTH_EXACT(structs::MemorizeSpell_Struct);
		SETUP_DIRECT_DECODE(MemorizeSpell_Struct, structs::MemorizeSpell_Struct);
		IN(slot);
		IN(spell_id);
		IN(scribing);
		FINISH_DIRECT_DECODE();	
	}

	ENCODE(OP_Buff) 
	{
		SETUP_DIRECT_ENCODE(SpellBuffFade_Struct, structs::SpellBuffFade_Struct);
		OUT(entityid);
		OUT(spellid);
		OUT(slotid);
		OUT(bufffade);
		OUT(duration);
		OUT(slot);
		OUT(level);
		OUT(effect);
		FINISH_ENCODE();	
	}

	DECODE(OP_Buff) 
	{
		DECODE_LENGTH_EXACT(structs::Buff_Struct);
		SETUP_DIRECT_DECODE(SpellBuffFade_Struct, structs::SpellBuffFade_Struct);
		IN(entityid);
		IN(spellid);
		IN(slotid);
		IN(bufffade);
		IN(duration);
		IN(slot);
		IN(level);
		IN(effect);
		FINISH_DIRECT_DECODE();	
	}

	ENCODE(OP_BeginCast)
	{
		SETUP_DIRECT_ENCODE(BeginCast_Struct, structs::BeginCast_Struct);
		OUT(spell_id);
		OUT(caster_id);
		OUT(cast_time);
		FINISH_ENCODE();
	}

	ENCODE(OP_CastSpell)
	{
		SETUP_DIRECT_ENCODE(CastSpell_Struct, structs::CastSpell_Struct);
		OUT(slot);
		OUT(spell_id);
		OUT(inventoryslot);
		OUT(target_id);
		FINISH_ENCODE();
	}

	DECODE(OP_CastSpell) {
		DECODE_LENGTH_EXACT(structs::CastSpell_Struct);
		SETUP_DIRECT_DECODE(CastSpell_Struct, structs::CastSpell_Struct);
		IN(slot);
		IN(spell_id);
		emu->inventoryslot = MacToServerSlot(eq->inventoryslot);
		IN(target_id);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_Damage) 
	{
		ENCODE_LENGTH_EXACT(CombatDamage_Struct);
		SETUP_DIRECT_ENCODE(CombatDamage_Struct, structs::CombatDamage_Struct);
		OUT(target);
		OUT(source);
		OUT(type);
		OUT(spellid);
		OUT(damage);
		//OUT(unknown11);
		OUT(sequence);
		//OUT(unknown19);
		FINISH_ENCODE();
	}

	ENCODE(OP_Action2) { ENCODE_FORWARD(OP_Action); }
	ENCODE(OP_Action) 
	{
		ENCODE_LENGTH_EXACT(Action_Struct);
		SETUP_DIRECT_ENCODE(Action_Struct, structs::Action_Struct);
		OUT(target);
		OUT(source);
		OUT(level);
		eq->unknown6 = 0x41; //Think this is target level.
		OUT(instrument_mod);
		OUT(bard_focus_id);
		OUT(sequence);
		OUT(type);
		OUT(spell);
		OUT(buff_unknown);
		FINISH_ENCODE();
	}

	DECODE(OP_Damage) { DECODE_FORWARD(OP_EnvDamage); }
	DECODE(OP_EnvDamage) 
	{
		DECODE_LENGTH_EXACT(structs::EnvDamage2_Struct);
		SETUP_DIRECT_DECODE(EnvDamage2_Struct, structs::EnvDamage2_Struct);
		IN(id);
		IN(dmgtype);
		IN(damage);
		IN(constant);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_ConsiderCorpse) { DECODE_FORWARD(OP_Consider); }
	DECODE(OP_Consider)
	{
		DECODE_LENGTH_EXACT(structs::Consider_Struct);
		SETUP_DIRECT_DECODE(Consider_Struct, structs::Consider_Struct);
		IN(playerid);
		IN(targetid);
		IN(faction);
		IN(level);
		IN(cur_hp);
		IN(max_hp);
		IN(pvpcon);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_Consider) 
	{
		ENCODE_LENGTH_EXACT(Consider_Struct);
		SETUP_DIRECT_ENCODE(Consider_Struct, structs::Consider_Struct);
		OUT(playerid);
		OUT(targetid);
		OUT(faction);
		OUT(level);
		OUT(cur_hp);
		OUT(max_hp);
		OUT(pvpcon);
		FINISH_ENCODE();
	}

	DECODE(OP_ClickDoor) 
	{
		DECODE_LENGTH_EXACT(structs::ClickDoor_Struct);
		SETUP_DIRECT_DECODE(ClickDoor_Struct, structs::ClickDoor_Struct);
		IN(doorid);
		IN(item_id);
		IN(player_id);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_GMEndTraining)
	{
		DECODE_LENGTH_EXACT(structs::GMTrainEnd_Struct);
		SETUP_DIRECT_DECODE(GMTrainEnd_Struct, structs::GMTrainEnd_Struct);
		IN(npcid);
		IN(playerid);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ItemLinkResponse) {  ENCODE_FORWARD(OP_ItemPacket); }
	ENCODE(OP_ItemPacket) 
	{
		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;
		ItemPacket_Struct *old_item_pkt=(ItemPacket_Struct *)__emu_buffer;
		InternalSerializedItem_Struct *int_struct=(InternalSerializedItem_Struct *)(old_item_pkt->SerializedItem);

		const ItemInst * item = (const ItemInst *)int_struct->inst;
	
		if(item)
		{
			uint8 type = 0;
			if(old_item_pkt->PacketType == ItemPacketViewLink)
				type = 2;

			structs::Item_Struct* mac_item = MacItem((ItemInst*)int_struct->inst,int_struct->slot_id, type);

			if(mac_item == 0)
			{
				delete in;
				return;
			}

			EQApplicationPacket* outapp = new EQApplicationPacket(OP_ItemPacket,sizeof(structs::Item_Struct));
			memcpy(outapp->pBuffer,mac_item,sizeof(structs::Item_Struct));

			outapp->SetOpcode(OP_Unknown);
		
			if(old_item_pkt->PacketType == ItemPacketSummonItem)
				outapp->SetOpcode(OP_SummonedItem);
			else if(old_item_pkt->PacketType == ItemPacketViewLink)
				outapp->SetOpcode(OP_ItemLinkResponse);
			else if(old_item_pkt->PacketType == ItemPacketTrade || old_item_pkt->PacketType == ItemPacketMerchant)
				outapp->SetOpcode(OP_MerchantItemPacket);
			else if(old_item_pkt->PacketType == ItemPacketLoot)
				outapp->SetOpcode(OP_LootItemPacket);
			else if(old_item_pkt->PacketType == ItemPacketWorldContainer)
				outapp->SetOpcode(OP_ObjectItemPacket);
			else if(item->GetItem()->ItemClass == ItemClassContainer)
				outapp->SetOpcode(OP_ContainerPacket);
			else if(item->GetItem()->ItemClass == ItemClassBook)
				outapp->SetOpcode(OP_BookPacket);
			else if(int_struct->slot_id == MainCursor)
				outapp->SetOpcode(OP_SummonedItem);
			else
				outapp->SetOpcode(OP_ItemPacket);

			if(outapp->size != sizeof(structs::Item_Struct))
				Log.Out(Logs::Detail, Logs::Zone_Server, "Invalid size on OP_ItemPacket packet. Expected: %i, Got: %i", sizeof(structs::Item_Struct), outapp->size);

			dest->FastQueuePacket(&outapp);
			delete[] __emu_buffer;
		}
	}

	ENCODE(OP_TradeItemPacket)
	{
			//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;
		ItemPacket_Struct *old_item_pkt=(ItemPacket_Struct *)__emu_buffer;
		InternalSerializedItem_Struct *int_struct=(InternalSerializedItem_Struct *)(old_item_pkt->SerializedItem);

		const ItemInst * item = (const ItemInst *)int_struct->inst;
	
		if(item)
		{
			structs::Item_Struct* mac_item = MacItem((ItemInst*)int_struct->inst,int_struct->slot_id);

			if(mac_item == 0)
			{
				delete in;
				return;
			}

			EQApplicationPacket* outapp = new EQApplicationPacket(OP_TradeItemPacket,sizeof(structs::TradeItemsPacket_Struct));
			structs::TradeItemsPacket_Struct* myitem = (structs::TradeItemsPacket_Struct*) outapp->pBuffer;
			myitem->fromid = old_item_pkt->fromid;
			myitem->slotid = int_struct->slot_id;
			memcpy(&myitem->item,mac_item,sizeof(structs::Item_Struct));
		
			if(outapp->size != sizeof(structs::TradeItemsPacket_Struct))
				Log.Out(Logs::Detail, Logs::Zone_Server, "Invalid size on OP_TradeItemPacket packet. Expected: %i, Got: %i", sizeof(structs::TradeItemsPacket_Struct), outapp->size);

			dest->FastQueuePacket(&outapp);
			delete[] __emu_buffer;
		}
	}

	ENCODE(OP_CharInventory)
	{

		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;

		int16 itemcount = in->size / sizeof(InternalSerializedItem_Struct);
		if(itemcount == 0 || (in->size % sizeof(InternalSerializedItem_Struct)) != 0)
		{
			Log.Out(Logs::General, Logs::Netcode, "[STRUCTS] Wrong size on outbound %s: Got %d, expected multiple of %d", opcodes->EmuToName(in->GetOpcode()), in->size, sizeof(InternalSerializedItem_Struct));
			delete in;
			return;
		}

		int pisize = sizeof(structs::PlayerItems_Struct) + (250 * sizeof(structs::PlayerItemsPacket_Struct));
		structs::PlayerItems_Struct* pi = (structs::PlayerItems_Struct*) new uchar[pisize];
		memset(pi, 0, pisize);

		InternalSerializedItem_Struct *eq = (InternalSerializedItem_Struct *) in->pBuffer;
		//do the transform...
		std::string mac_item_string;
		int r;
		//std::string mac_item_string;
		for(r = 0; r < itemcount; r++, eq++) 
		{
			structs::Item_Struct* mac_item = MacItem((ItemInst*)eq->inst,eq->slot_id);

			if(mac_item != 0)
			{
				char *mac_item_char = reinterpret_cast<char*>(mac_item);
				mac_item_string.append(mac_item_char,sizeof(structs::Item_Struct));
				safe_delete(mac_item);	
			}
		}
		int32 length = 5000;
		int buffer = 2;

		memcpy(pi->packets,mac_item_string.c_str(),mac_item_string.length());
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_CharInventory, length);
		outapp->size = buffer + DeflatePacket((uchar*) pi->packets, itemcount * sizeof(structs::Item_Struct), &outapp->pBuffer[buffer], length-buffer);
		outapp->pBuffer[0] = itemcount;
		safe_delete_array(pi);

		dest->FastQueuePacket(&outapp);
		delete[] __emu_buffer;
	}

	ENCODE(OP_ShopInventoryPacket)
	{
		//consume the packet
		EQApplicationPacket *in = *p;
		*p = nullptr;

		//store away the emu struct
		unsigned char *__emu_buffer = in->pBuffer;

		int16 itemcount = in->size / sizeof(InternalSerializedItem_Struct);
		if(itemcount == 0 || (in->size % sizeof(InternalSerializedItem_Struct)) != 0) 
		{
			Log.Out(Logs::Detail, Logs::Zone_Server, "Wrong size on outbound %s: Got %d, expected multiple of %d", opcodes->EmuToName(in->GetOpcode()), in->size, sizeof(InternalSerializedItem_Struct));
			delete in;
			return;
		}
		if(itemcount > 79)
			itemcount = 79;

		int pisize = sizeof(structs::MerchantItems_Struct) + (79 * sizeof(structs::MerchantItemsPacket_Struct));
		structs::MerchantItems_Struct* pi = (structs::MerchantItems_Struct*) new uchar[pisize];
		memset(pi, 0, pisize);

		InternalSerializedItem_Struct *eq = (InternalSerializedItem_Struct *) in->pBuffer;
		//do the transform...
		std::string mac_item_string;
		int r = 0;
		for(r = 0; r < itemcount; r++, eq++) 
		{
			structs::Item_Struct* mac_item = MacItem((ItemInst*)eq->inst,eq->slot_id,1);

			if(mac_item != 0)
			{
				structs::MerchantItemsPacket_Struct* merchant = new structs::MerchantItemsPacket_Struct;
				memset(merchant,0,sizeof(structs::MerchantItemsPacket_Struct));
				memcpy(&merchant->item,mac_item,sizeof(structs::Item_Struct));
				merchant->itemtype = mac_item->ItemClass;

				char *mac_item_char = reinterpret_cast<char*>(merchant);
				mac_item_string.append(mac_item_char,sizeof(structs::MerchantItemsPacket_Struct));
				safe_delete(mac_item);	
				safe_delete(merchant);
			}
		}
		int32 length = 5000;
		int buffer = 2;

		memcpy(pi->packets,mac_item_string.c_str(),mac_item_string.length());
		EQApplicationPacket* outapp = new EQApplicationPacket(OP_ShopInventoryPacket, length);
		outapp->size = buffer + DeflatePacket((uchar*) pi->packets, itemcount * sizeof(structs::MerchantItemsPacket_Struct), &outapp->pBuffer[buffer], length-buffer);
		outapp->pBuffer[0] = itemcount;

		dest->FastQueuePacket(&outapp);
		delete[] __emu_buffer;
		safe_delete_array(pi);
	}

	DECODE(OP_DeleteCharge) {  DECODE_FORWARD(OP_MoveItem); }
	DECODE(OP_MoveItem)
	{
		SETUP_DIRECT_DECODE(MoveItem_Struct, structs::MoveItem_Struct);

		emu->from_slot = MacToServerSlot(eq->from_slot);
		emu->to_slot = MacToServerSlot(eq->to_slot);
		IN(number_in_stack);

		Log.Out(Logs::Detail, Logs::Inventory, "EQMAC DECODE OUTPUT to_slot: %i, from_slot: %i, number_in_stack: %i", emu->to_slot, emu->from_slot, emu->number_in_stack);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_DeleteCharge) {  ENCODE_FORWARD(OP_MoveItem); }
	ENCODE(OP_MoveItem)
	{
		ENCODE_LENGTH_EXACT(MoveItem_Struct);
		SETUP_DIRECT_ENCODE(MoveItem_Struct, structs::MoveItem_Struct);

		eq->from_slot = ServerToMacSlot(emu->from_slot);
		eq->to_slot = ServerToMacSlot(emu->to_slot);
		OUT(to_slot);
		OUT(number_in_stack);
		Log.Out(Logs::Detail, Logs::Inventory, "EQMAC ENCODE OUTPUT to_slot: %i, from_slot: %i, number_in_stack: %i", eq->to_slot, eq->from_slot, eq->number_in_stack);

		FINISH_ENCODE();
	}

	ENCODE(OP_HPUpdate)
	{
		ENCODE_LENGTH_EXACT(SpawnHPUpdate_Struct);
		SETUP_DIRECT_ENCODE(SpawnHPUpdate_Struct, structs::SpawnHPUpdate_Struct);
		OUT(spawn_id);
		OUT(cur_hp);
		OUT(max_hp);
		FINISH_ENCODE();
	}

	ENCODE(OP_MobHealth)
	{
		ENCODE_LENGTH_EXACT(SpawnHPUpdate_Struct2);
		SETUP_DIRECT_ENCODE(SpawnHPUpdate_Struct2, structs::SpawnHPUpdate_Struct);
		OUT(spawn_id);
		eq->cur_hp=emu->hp;
		eq->max_hp=100;
		FINISH_ENCODE();
	}

	DECODE(OP_Consume) 
	{
		DECODE_LENGTH_EXACT(structs::Consume_Struct);
		SETUP_DIRECT_DECODE(Consume_Struct, structs::Consume_Struct);

		emu->slot = MacToServerSlot(eq->slot);
		IN(type);
		IN(auto_consumed);

		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ReadBook) 
	{
		// no apparent slot translation needed -U
		EQApplicationPacket *in = *p;
		*p = nullptr;

		unsigned char *__emu_buffer = in->pBuffer;
		BookText_Struct *emu_BookText_Struct = (BookText_Struct *)__emu_buffer;
		in->size = sizeof(structs::BookText_Struct) + strlen(emu_BookText_Struct->booktext);
		in->pBuffer = new unsigned char[in->size];
		structs::BookText_Struct *eq_BookText_Struct = (structs::BookText_Struct*)in->pBuffer;

		eq_BookText_Struct->type = emu_BookText_Struct->type;
		strcpy(eq_BookText_Struct->booktext, emu_BookText_Struct->booktext);

		delete[] __emu_buffer;
		dest->FastQueuePacket(&in, ack_req);

	}

	DECODE(OP_ReadBook) 
	{
		DECODE_LENGTH_ATLEAST(structs::BookRequest_Struct);
		SETUP_DIRECT_DECODE(BookRequest_Struct, structs::BookRequest_Struct);

		IN(type);
		strn0cpy(emu->txtfile, eq->txtfile, sizeof(emu->txtfile));

		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_Illusion) 
	{
		ENCODE_LENGTH_EXACT(Illusion_Struct);
		SETUP_DIRECT_ENCODE(Illusion_Struct, structs::Illusion_Struct);
		OUT(spawnid);
		OUT(race);
		OUT(gender);
		OUT(texture);
		OUT(helmtexture);
		OUT(face);
		OUT(hairstyle);
		OUT(haircolor);
		OUT(beard);
		OUT(beardcolor);
		eq->size = (int16) (emu->size + 0.5);
		eq->unknown007=0xFF;
		eq->unknown_void=0xFFFFFFFF;

		FINISH_ENCODE();
	}

	DECODE(OP_Illusion) 
	{
		DECODE_LENGTH_EXACT(structs::Illusion_Struct);
		SETUP_DIRECT_DECODE(Illusion_Struct, structs::Illusion_Struct);
		IN(spawnid);
		IN(race);
		IN(gender);
		IN(texture);
		IN(helmtexture);
		IN(face);
		IN(hairstyle);
		IN(haircolor);
		IN(beard);
		IN(beardcolor);
		IN(size);

		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ShopRequest)
	{
		ENCODE_LENGTH_EXACT(Merchant_Click_Struct);
		SETUP_DIRECT_ENCODE(Merchant_Click_Struct, structs::Merchant_Click_Struct);
		eq->npcid=emu->npcid;
		OUT(playerid);
		OUT(command);
		eq->unknown[0] = 0x71;
		eq->unknown[1] = 0x54;
		eq->unknown[2] = 0x00;
		OUT(rate);
		FINISH_ENCODE();
	}

	DECODE(OP_ShopRequest) 
	{
		DECODE_LENGTH_EXACT(structs::Merchant_Click_Struct);
		SETUP_DIRECT_DECODE(Merchant_Click_Struct, structs::Merchant_Click_Struct);
		emu->npcid=eq->npcid;
		IN(playerid);
		IN(command);
		IN(rate);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_ShopPlayerBuy)
	{
		DECODE_LENGTH_EXACT(structs::Merchant_Sell_Struct);
		SETUP_DIRECT_DECODE(Merchant_Sell_Struct, structs::Merchant_Sell_Struct);
		emu->npcid=eq->npcid;
		IN(playerid);
		emu->itemslot = MacToServerSlot(eq->itemslot);
		IN(quantity);
		IN(price);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ShopPlayerBuy)
	{
		ENCODE_LENGTH_EXACT(Merchant_Sell_Struct);
		SETUP_DIRECT_ENCODE(Merchant_Sell_Struct, structs::Merchant_Sell_Struct);
		eq->npcid=emu->npcid;
		eq->playerid=emu->playerid;
		eq->itemslot = ServerToMacSlot(emu->itemslot);
		OUT(quantity);
		OUT(price);
		FINISH_ENCODE();
	}

	DECODE(OP_ShopPlayerSell)
	{
		DECODE_LENGTH_EXACT(structs::Merchant_Purchase_Struct);
		SETUP_DIRECT_DECODE(Merchant_Purchase_Struct, structs::Merchant_Purchase_Struct);
		emu->npcid=eq->npcid;
		//IN(playerid);
		emu->itemslot = MacToServerSlot(eq->itemslot);
		IN(quantity);
		IN(price);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ShopPlayerSell)
	{
		ENCODE_LENGTH_EXACT(Merchant_Purchase_Struct);
		SETUP_DIRECT_ENCODE(Merchant_Purchase_Struct, structs::Merchant_Purchase_Struct);
		eq->npcid=emu->npcid;
		//eq->playerid=emu->playerid;
		eq->itemslot = ServerToMacSlot(emu->itemslot);
		OUT(quantity);
		OUT(price);
		FINISH_ENCODE();
	}

	ENCODE(OP_ShopDelItem)
	{
		ENCODE_LENGTH_EXACT(Merchant_DelItem_Struct);
		SETUP_DIRECT_ENCODE(Merchant_DelItem_Struct, structs::Merchant_DelItem_Struct);
		eq->npcid=emu->npcid;
		OUT(playerid);
		eq->itemslot = ServerToMacSlot(emu->itemslot);
		if(emu->type == 0)
			eq->type=64;
		else
			OUT(type);
		FINISH_ENCODE();
	}

	ENCODE(OP_LootItem)
	{
		ENCODE_LENGTH_EXACT(LootingItem_Struct);
		SETUP_DIRECT_ENCODE(LootingItem_Struct, structs::LootingItem_Struct);
		OUT(lootee);
		OUT(looter);
		eq->slot_id = ServerToMacCorpseSlot(emu->slot_id);
		OUT(auto_loot);

		FINISH_ENCODE();
	}

	DECODE(OP_LootItem)
	{
		DECODE_LENGTH_EXACT(structs::LootingItem_Struct);
		SETUP_DIRECT_DECODE(LootingItem_Struct, structs::LootingItem_Struct);
		IN(lootee);
		IN(looter);
		emu->slot_id = MacToServerCorpseSlot(eq->slot_id);
		IN(auto_loot);

		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_AAExpUpdate)
	{
		ENCODE_LENGTH_EXACT(AltAdvStats_Struct);
		SETUP_DIRECT_ENCODE(AltAdvStats_Struct, structs::AltAdvStats_Struct);
		OUT(experience);
		OUT(unspent);
		OUT(percentage);
		FINISH_ENCODE();
	}

	ENCODE(OP_AAAction)
	{
		ENCODE_LENGTH_EXACT(UseAA_Struct);
		SETUP_DIRECT_ENCODE(UseAA_Struct, structs::UseAA_Struct);
		OUT(end);
		OUT(ability);
		OUT(begin);
		eq->unknown_void=2154;

		FINISH_ENCODE();
	}

	ENCODE(OP_GroundSpawn) 
	{

		ENCODE_LENGTH_EXACT(Object_Struct);
		SETUP_DIRECT_ENCODE(Object_Struct, structs::Object_Struct);
		OUT(drop_id);
		OUT(zone_id);
		OUT(heading);
		OUT(z);
		OUT(y);
		OUT(x);
		strncpy(eq->object_name,emu->object_name,16);
		OUT(object_type);
		OUT(charges);
		OUT(maxcharges);
		int g;
		for(g=0; g<10; g++)
		{
			if(eq->itemsinbag[g] > 0)
			{
				eq->itemsinbag[g] = emu->itemsinbag[g];
				Log.Out(Logs::Detail, Logs::Inventory, "Found a container item %i in slot: %i", emu->itemsinbag[g], g);
			}
			else
				eq->itemsinbag[g] = 0xFFFF;
		}
		eq->unknown208 = 0xFFFFFFFF;
		eq->unknown216[0] = 0xFFFF;
		FINISH_ENCODE();
	}

	DECODE(OP_GroundSpawn)
	{
		DECODE_LENGTH_EXACT(structs::Object_Struct);
		SETUP_DIRECT_DECODE(Object_Struct, structs::Object_Struct);
		IN(drop_id);
		IN(zone_id);
		IN(heading);
		IN(z);
		IN(y);
		IN(x);
		strncpy(emu->object_name,eq->object_name,16);
		IN(object_type);

		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ClickObjectAction) 
	{

		ENCODE_LENGTH_EXACT(ClickObjectAction_Struct);
		SETUP_DIRECT_ENCODE(ClickObjectAction_Struct, structs::ClickObjectAction_Struct);
		OUT(player_id);
		OUT(drop_id);
		OUT(open);
		OUT(type);
		OUT(icon);
		eq->slot=emu->unknown16;
		FINISH_ENCODE();
	}

	DECODE(OP_ClickObjectAction)
	{
		DECODE_LENGTH_EXACT(structs::ClickObjectAction_Struct);
		SETUP_DIRECT_DECODE(ClickObjectAction_Struct, structs::ClickObjectAction_Struct);
		IN(player_id);
		IN(drop_id);
		IN(open);
		IN(type);
		IN(icon);
		emu->unknown16=eq->slot;
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_TradeSkillCombine) 
	{
		DECODE_LENGTH_EXACT(structs::Combine_Struct);
		SETUP_DIRECT_DECODE(NewCombine_Struct, structs::Combine_Struct);
		emu->container_slot = MacToServerSlot(eq->container_slot);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_TradeRequest)
	{
		DECODE_LENGTH_EXACT(structs::TradeRequest_Struct);
		SETUP_DIRECT_DECODE(TradeRequest_Struct, structs::TradeRequest_Struct);
		IN(from_mob_id);
		IN(to_mob_id);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_TradeRequest)
	{

		ENCODE_LENGTH_EXACT(TradeRequest_Struct);
		SETUP_DIRECT_ENCODE(TradeRequest_Struct, structs::TradeRequest_Struct);
		OUT(from_mob_id);
		OUT(to_mob_id);
		FINISH_ENCODE();
	}

	DECODE(OP_TradeRequestAck)
	{
		DECODE_LENGTH_EXACT(structs::TradeRequest_Struct);
		SETUP_DIRECT_DECODE(TradeRequest_Struct, structs::TradeRequest_Struct);
		IN(from_mob_id);
		IN(to_mob_id);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_TradeRequestAck)
	{

		ENCODE_LENGTH_EXACT(TradeRequest_Struct);
		SETUP_DIRECT_ENCODE(TradeRequest_Struct, structs::TradeRequest_Struct);
		OUT(from_mob_id);
		OUT(to_mob_id);
		FINISH_ENCODE();
	}

	ENCODE(OP_ManaChange)
	{
		ENCODE_LENGTH_EXACT(ManaChange_Struct);
		SETUP_DIRECT_ENCODE(ManaChange_Struct, structs::ManaChange_Struct);
		OUT(new_mana);
		OUT(spell_id);
		FINISH_ENCODE();
	}

	ENCODE(OP_DeleteSpawn)
	{
		SETUP_DIRECT_ENCODE(DeleteSpawn_Struct, structs::DeleteSpawn_Struct);
		OUT(spawn_id);
		FINISH_ENCODE();
	}

	ENCODE(OP_TimeOfDay)
	{
		SETUP_DIRECT_ENCODE(TimeOfDay_Struct, structs::TimeOfDay_Struct);
		OUT(hour);
		OUT(minute);
		OUT(day);
		OUT(month);
		OUT(year);
		FINISH_ENCODE();
	}

	ENCODE(OP_GroupInvite2) { ENCODE_FORWARD(OP_GroupInvite); }
	ENCODE(OP_GroupInvite)
	{
		ENCODE_LENGTH_EXACT(GroupInvite_Struct);
		SETUP_DIRECT_ENCODE(GroupInvite_Struct, structs::GroupInvite_Struct);
		strcpy(eq->invitee_name,emu->invitee_name);
		strcpy(eq->inviter_name,emu->inviter_name);
		FINISH_ENCODE();
	}

	DECODE(OP_GroupInvite2) { DECODE_FORWARD(OP_GroupInvite); }
	DECODE(OP_GroupInvite) 
	{
		DECODE_LENGTH_EXACT(structs::GroupInvite_Struct);
		SETUP_DIRECT_DECODE(GroupInvite_Struct, structs::GroupInvite_Struct);
		strcpy(emu->invitee_name,eq->invitee_name);
		strcpy(emu->inviter_name,eq->inviter_name);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_TradeCoins) 
	{
		SETUP_DIRECT_ENCODE(TradeCoin_Struct, structs::TradeCoin_Struct);
		OUT(trader);
		OUT(slot);
		OUT(amount);
		FINISH_ENCODE();
	}

	DECODE(OP_ItemLinkResponse)
	{
		DECODE_LENGTH_EXACT(structs::ItemViewRequest_Struct);
		SETUP_DIRECT_DECODE(ItemViewRequest_Struct, structs::ItemViewRequest_Struct);
		IN(item_id);
		strcpy(emu->item_name,eq->item_name);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_LogServer) 
	{
		ENCODE_LENGTH_EXACT(LogServer_Struct);
		SETUP_DIRECT_ENCODE(LogServer_Struct, structs::LogServer_Struct);
		strcpy(eq->worldshortname, "alkabor");
		strcpy(eq->unknown096, "pacman");
		OUT(enable_pvp);
		OUT(enable_FV);
		eq->NameGen = 1;
		eq->Gibberish = 1;
		eq->ProfanityFilter = 0;
		FINISH_ENCODE();
	}

	ENCODE(OP_RequestClientZoneChange)
	{
		SETUP_DIRECT_ENCODE(RequestClientZoneChange_Struct, structs::RequestClientZoneChange_Struct);
		OUT(zone_id);
		OUT(x);
		OUT(y);
		OUT(z);
		OUT(heading);
		OUT(type);
		FINISH_ENCODE();
	}

	ENCODE(OP_Trader) 
	{
		if((*p)->size == sizeof(Trader_Struct)) 
		{
			ENCODE_LENGTH_EXACT(Trader_Struct);
			SETUP_DIRECT_ENCODE(Trader_Struct, structs::Trader_Struct);
			OUT(Code);
			int k;
			for(k = 0; k < 80; k++) 
			{
				eq->Items[k] = emu->Items[k];
			}
			for(k = 0; k < 80; k++)
			{
				eq->ItemCost[k] = emu->ItemCost[k];
			}
			FINISH_ENCODE();
		}
		else if((*p)->size == sizeof(Trader_ShowItems_Struct)) 
		{
			ENCODE_LENGTH_EXACT(Trader_ShowItems_Struct);
			SETUP_DIRECT_ENCODE(Trader_ShowItems_Struct, structs::Trader_ShowItems_Struct);
			OUT(Code);
			OUT(TraderID);
			if(emu->SubAction == 0)
				eq->SubAction = emu->Code;
			else
				OUT(SubAction);
			eq->Items[0] = emu->Unknown08[0];
			eq->Items[1] = emu->Unknown08[1];
			FINISH_ENCODE();
		}
		else if((*p)->size == sizeof(TraderPriceUpdate_Struct))
		{
			ENCODE_LENGTH_EXACT(TraderPriceUpdate_Struct);
			SETUP_DIRECT_ENCODE(TraderPriceUpdate_Struct, structs::TraderPriceUpdate_Struct);
			OUT(Action);
			OUT(SubAction);
			OUT(SerialNumber);
			OUT(NewPrice);
			FINISH_ENCODE();
		}
		else if((*p)->size == sizeof(TraderBuy_Struct))
		{
			ENCODE_LENGTH_EXACT(TraderBuy_Struct);
			SETUP_DIRECT_ENCODE(TraderBuy_Struct, structs::TraderBuy_Struct);
			OUT(Action);
			OUT(TraderID);
			OUT(ItemID);
			OUT(Price);
			OUT(Quantity);
			//OUT(Slot);
			strcpy(eq->ItemName,emu->ItemName);
			FINISH_ENCODE();
		}
	}

	DECODE(OP_Trader) 
	{
		if(__packet->size == sizeof(structs::Trader_Struct)) 
		{
			DECODE_LENGTH_EXACT(structs::Trader_Struct);
			SETUP_DIRECT_DECODE(Trader_Struct, structs::Trader_Struct);
			IN(Code);
			int k;
			for(k = 0; k < 80; k++)
			{
				emu->Items[k] = eq->Items[k];
			}
			for(k = 0; k < 80; k++) 
			{
				emu->ItemCost[k] = eq->ItemCost[k];
			}
			FINISH_DIRECT_DECODE();
		}
		else if(__packet->size == sizeof(structs::TraderStatus_Struct)) 
		{
			DECODE_LENGTH_EXACT(structs::TraderStatus_Struct);
			SETUP_DIRECT_DECODE(TraderStatus_Struct, structs::TraderStatus_Struct);
			IN(Code);
			IN(TraderID);
			FINISH_DIRECT_DECODE();
		}
		else if(__packet->size == sizeof(structs::TraderPriceUpdate_Struct))
		{
			DECODE_LENGTH_EXACT(structs::TraderPriceUpdate_Struct);
			SETUP_DIRECT_DECODE(TraderPriceUpdate_Struct, structs::TraderPriceUpdate_Struct);
			IN(Action);
			IN(SubAction);
			IN(SerialNumber);
			IN(NewPrice);
			FINISH_DIRECT_DECODE();
		}
	}

	ENCODE(OP_BecomeTrader)
	{
		ENCODE_LENGTH_EXACT(BecomeTrader_Struct);
		SETUP_DIRECT_ENCODE(BecomeTrader_Struct, structs::BecomeTrader_Struct);
		OUT(Code);
		OUT(ID);
		FINISH_ENCODE();
	}

	ENCODE(OP_TraderBuy)
	{
		ENCODE_LENGTH_EXACT(TraderBuy_Struct);
		SETUP_DIRECT_ENCODE(TraderBuy_Struct, structs::TraderBuy_Struct);
		OUT(Action);
		OUT(TraderID);
		OUT(ItemID);
		OUT(Price);
		OUT(Quantity);
		eq->Slot = emu->AlreadySold;
		strcpy(eq->ItemName,emu->ItemName);
		FINISH_ENCODE();
	}

	DECODE(OP_TraderBuy)
	{
		DECODE_LENGTH_EXACT(structs::TraderBuy_Struct);
		SETUP_DIRECT_DECODE(TraderBuy_Struct, structs::TraderBuy_Struct);
		IN(Action);
		IN(TraderID);
		IN(ItemID);
		IN(Price);
		IN(Quantity);
		emu->AlreadySold = eq->Slot;
		strcpy(emu->ItemName,eq->ItemName);
		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_BazaarSearch){
		if(__packet->size == sizeof(structs::BazaarSearch_Struct))
		{
			DECODE_LENGTH_EXACT(structs::BazaarSearch_Struct);
			SETUP_DIRECT_DECODE(BazaarSearch_Struct, structs::BazaarSearch_Struct);
			IN(Beginning.Action);
			IN(TraderID);
			IN(Class_);
			IN(Race);
			IN(ItemStat);
			IN(Slot);
			IN(Type);
			IN(MinPrice);
			IN(MaxPrice);
			strcpy(emu->Name,eq->Name);
			FINISH_DIRECT_DECODE();
		}
		else if(__packet->size == sizeof(structs::BazaarWelcome_Struct))
		{
			DECODE_LENGTH_EXACT(structs::BazaarWelcome_Struct);
			SETUP_DIRECT_DECODE(BazaarWelcome_Struct, structs::BazaarWelcome_Struct);
			emu->Beginning.Action = eq->Action;
			IN(Traders);
			IN(Items);
			FINISH_DIRECT_DECODE();
		}
		else if(__packet->size == sizeof(BazaarInspect_Struct))
		{
			DECODE_LENGTH_EXACT(BazaarInspect_Struct);
		}
	}

	ENCODE(OP_WearChange)
	{
		ENCODE_LENGTH_EXACT(WearChange_Struct);
		SETUP_DIRECT_ENCODE(WearChange_Struct, structs::WearChange_Struct);
		OUT(spawn_id);
		OUT(material);
		eq->color = emu->color.color;
		eq->blue = emu->color.rgb.blue;
		eq->green = emu->color.rgb.green;
		eq->red = emu->color.rgb.red;
		eq->use_tint = emu->color.rgb.use_tint;
		OUT(wear_slot_id);
		FINISH_ENCODE();
	}

	DECODE(OP_WearChange)
	{
		DECODE_LENGTH_EXACT(structs::WearChange_Struct);
		SETUP_DIRECT_DECODE(WearChange_Struct, structs::WearChange_Struct);
		IN(spawn_id);
		IN(material);
		emu->color.color = eq->color;
		emu->color.rgb.blue = eq->blue;
		emu->color.rgb.green = eq->green;
		emu->color.rgb.red = eq->red;
		emu->color.rgb.use_tint = eq->use_tint;
		IN(wear_slot_id);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_ExpUpdate) 
	{
		ENCODE_LENGTH_EXACT(ExpUpdate_Struct);
		SETUP_DIRECT_ENCODE(ExpUpdate_Struct, structs::ExpUpdate_Struct);
		OUT(exp);
		FINISH_ENCODE();
	}

	ENCODE(OP_Death)
	{
		ENCODE_LENGTH_EXACT(Death_Struct);
		SETUP_DIRECT_ENCODE(Death_Struct, structs::Death_Struct);
		OUT(spawn_id);
		OUT(killer_id);
		OUT(corpseid);
		OUT(spell_id);
		OUT(attack_skill);
		OUT(damage);
		FINISH_ENCODE();
	}

	DECODE(OP_CombatAbility) 
	{
		DECODE_LENGTH_EXACT(structs::CombatAbility_Struct);
		SETUP_DIRECT_DECODE(CombatAbility_Struct, structs::CombatAbility_Struct);
		IN(m_target);
		IN(m_atk);
		IN(m_skill);
		FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_Projectile)
	{
		ENCODE_LENGTH_EXACT(Arrow_Struct);
		SETUP_DIRECT_ENCODE(Arrow_Struct, structs::Arrow_Struct);
		OUT(type);
		OUT(src_x);
		OUT(src_y);
		OUT(src_z);
		OUT(velocity);
		OUT(launch_angle);
		OUT(tilt);
		OUT(arc);
		OUT(source_id);
		OUT(target_id);
		OUT(skill);
		OUT(object_id);
		OUT(effect_type);
		OUT(yaw);
		OUT(pitch);
		OUT(behavior);
		OUT(light);
		strcpy(eq->model_name,emu->model_name);
		FINISH_ENCODE();
	}

	ENCODE(OP_Charm)
	{
		ENCODE_LENGTH_EXACT(Charm_Struct);
		SETUP_DIRECT_ENCODE(Charm_Struct, structs::Charm_Struct);
		OUT(owner_id);
		OUT(pet_id);
		OUT(command);
		FINISH_ENCODE();
	}

	ENCODE(OP_Sound)
	{
		ENCODE_LENGTH_EXACT(QuestReward_Struct);
		SETUP_DIRECT_ENCODE(QuestReward_Struct, structs::QuestReward_Struct);
		OUT(mob_id);
		OUT(target_id);
		OUT(exp_reward);
		OUT(copper);
		OUT(silver);
		OUT(gold);
		OUT(platinum);
		OUT(item_id);
		OUT(faction);
		OUT(faction_mod);
		FINISH_ENCODE();
	}

	ENCODE(OP_FaceChange)
	{
		ENCODE_LENGTH_EXACT(FaceChange_Struct);
		SETUP_DIRECT_ENCODE(FaceChange_Struct, structs::FaceChange_Struct);
		OUT(haircolor);
		OUT(beardcolor);
		OUT(eyecolor1);
		OUT(eyecolor2);
		OUT(hairstyle);
		OUT(beard);
		OUT(face);
		FINISH_ENCODE();
	}

	DECODE(OP_FaceChange)
	{
		DECODE_LENGTH_EXACT(structs::FaceChange_Struct);
		SETUP_DIRECT_DECODE(FaceChange_Struct, structs::FaceChange_Struct);
		IN(haircolor);
		IN(beardcolor);
		IN(eyecolor1);
		IN(eyecolor2);
		IN(hairstyle);
		IN(beard);
		IN(face);
	FINISH_DIRECT_DECODE();
	}

	ENCODE(OP_Assist)
	{
		ENCODE_LENGTH_EXACT(EntityId_Struct);
		SETUP_DIRECT_ENCODE(EntityId_Struct, structs::EntityId_Struct);
		OUT(entity_id);
		FINISH_ENCODE();
	}

	DECODE(OP_Assist)
	{
		DECODE_LENGTH_EXACT(structs::EntityId_Struct);
		SETUP_DIRECT_DECODE(EntityId_Struct, structs::EntityId_Struct);
		IN(entity_id);
	FINISH_DIRECT_DECODE();
	}

	structs::Item_Struct* MacItem(const ItemInst *inst, int16 slot_id_in, int type)
	{

		if(!inst)
			return 0;

		const Item_Struct *item=inst->GetItem();

		if(item->ID > 32767)
			return 0;

		structs::Item_Struct *mac_pop_item = new structs::Item_Struct;
		memset(mac_pop_item,0,sizeof(structs::Item_Struct));

		if(item->GMFlag == -1)
			Log.Out(Logs::Detail, Logs::EQMac, "Item %s is flagged for GMs.", item->Name);

		// General items
  		if(type == 0)
  		{
			mac_pop_item->Charges = inst->GetCharges();
  			mac_pop_item->equipSlot = ServerToMacSlot(slot_id_in);
			if(item->NoDrop == 0)
				mac_pop_item->Price = 0; 
			else
				mac_pop_item->Price = item->Price;
			mac_pop_item->SellRate = item->SellRate;
  		}
		// Items on a merchant
  		else if(type == 1)
  		{ 
  			mac_pop_item->Charges = 1;
  			mac_pop_item->equipSlot = inst->GetMerchantSlot();
			mac_pop_item->Price = inst->GetPrice();  //This handles sellrate for us. 
			mac_pop_item->SellRate = 1;
		}
		// Item links
		else if(type == 2)
		{
			mac_pop_item->Charges = item->MaxCharges;
			mac_pop_item->equipSlot = ServerToMacSlot(slot_id_in);
			mac_pop_item->Price = item->Price;
			mac_pop_item->SellRate = item->SellRate;
		}
  
			mac_pop_item->ItemClass = item->ItemClass;
			strcpy(mac_pop_item->Name,item->Name);
			strcpy(mac_pop_item->Lore,item->Lore);       
			strcpy(mac_pop_item->IDFile,item->IDFile);  
			mac_pop_item->Weight = item->Weight;      
			mac_pop_item->NoRent = item->NoRent;         
			mac_pop_item->NoDrop = item->NoDrop;         
			mac_pop_item->Size = item->Size;           
			mac_pop_item->ID = item->ID;        
			mac_pop_item->Icon = item->Icon;       
			mac_pop_item->Slots = item->Slots;  
			mac_pop_item->CastTime = item->CastTime;  
			mac_pop_item->SkillModType = item->SkillModType;
			mac_pop_item->SkillModValue = item->SkillModValue;
			mac_pop_item->BaneDmgRace = item->BaneDmgRace;
			mac_pop_item->BaneDmgBody = item->BaneDmgBody;
			mac_pop_item->BaneDmgAmt = item->BaneDmgAmt;
			mac_pop_item->RecLevel = item->RecLevel;       
			mac_pop_item->RecSkill = item->RecSkill;   
			mac_pop_item->ProcRate = item->ProcRate; 
			mac_pop_item->ElemDmgType = item->ElemDmgType; 
			mac_pop_item->ElemDmgAmt = item->ElemDmgAmt;
			mac_pop_item->FactionMod1 = item->FactionMod1;
			mac_pop_item->FactionMod2 = item->FactionMod2;
			mac_pop_item->FactionMod3 = item->FactionMod3;
			mac_pop_item->FactionMod4 = item->FactionMod4;
			mac_pop_item->FactionAmt1 = item->FactionAmt1;
			mac_pop_item->FactionAmt2 = item->FactionAmt2;
			mac_pop_item->FactionAmt3 = item->FactionAmt3;
			mac_pop_item->FactionAmt4 = item->FactionAmt4;
			mac_pop_item->Deity = item->Deity;
			mac_pop_item->ReqLevel = item->ReqLevel; 
			mac_pop_item->BardType = item->BardType;
			mac_pop_item->BardValue = item->BardValue;
			if(item->Focus.Effect < 0)
				mac_pop_item->FocusEffect = 0;
			else
				mac_pop_item->FocusEffect = item->Focus.Effect;

			if(item->ItemClass == 1)
			{
				mac_pop_item->container.BagType = item->BagType; 
				mac_pop_item->container.BagSlots = item->BagSlots;         
				mac_pop_item->container.BagSize = item->BagSize;    
				mac_pop_item->container.BagWR = item->BagWR; 
			}
			else if(item->ItemClass == 2)
			{
				strcpy(mac_pop_item->book.Filename,item->Filename);
				mac_pop_item->book.Book = item->Book;         
				mac_pop_item->book.BookType = item->BookType; 
			}
			else
			{
			mac_pop_item->common.AStr = item->AStr;           
			mac_pop_item->common.ASta = item->ASta;           
			mac_pop_item->common.ACha = item->ACha;           
			mac_pop_item->common.ADex = item->ADex;           
			mac_pop_item->common.AInt = item->AInt;           
			mac_pop_item->common.AAgi = item->AAgi;           
			mac_pop_item->common.AWis = item->AWis;           
			mac_pop_item->common.MR = item->MR;             
			mac_pop_item->common.FR = item->FR;             
			mac_pop_item->common.CR = item->CR;             
			mac_pop_item->common.DR = item->DR;             
			mac_pop_item->common.PR = item->PR;             
			mac_pop_item->common.HP = item->HP;             
			mac_pop_item->common.Mana = item->Mana;           
			mac_pop_item->common.AC = item->AC;		
			mac_pop_item->common.MaxCharges = item->MaxCharges;    
			mac_pop_item->common.GMFlag = item->GMFlag;
			mac_pop_item->common.Light = item->Light;          
			mac_pop_item->common.Delay = item->Delay;          
			mac_pop_item->common.Damage = item->Damage;               
			mac_pop_item->common.Range = item->Range;          
			mac_pop_item->common.ItemType = item->ItemType;          
			mac_pop_item->common.Magic = item->Magic;          
			mac_pop_item->common.Material = item->Material;   
			mac_pop_item->common.Color = item->Color;    
			//mac_pop_item->common.Faction = item->Faction;   
			mac_pop_item->common.Classes = item->Classes;  
			mac_pop_item->common.Races = item->Races;  
			mac_pop_item->common.Stackable = item->Stackable_; 
			}

			//FocusEffect and BardEffect is already handled above. Now figure out click, scroll, proc, and worn.

			if(item->Click.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Click.Effect;
				mac_pop_item->Effect2 = item->Click.Effect; 
				mac_pop_item->EffectType2 = item->Click.Type;  
				mac_pop_item->common.EffectType1 = item->Click.Type;
				if(item->Click.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Click.Level; 
					mac_pop_item->EffectLevel2 = item->Click.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Click.Level2; 
					mac_pop_item->EffectLevel2 = item->Click.Level2;  
				}
			}
			else if(item->Scroll.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Scroll.Effect;
				mac_pop_item->Effect2 = item->Scroll.Effect; 
				mac_pop_item->EffectType2 = item->Scroll.Type;  
				mac_pop_item->common.EffectType1 = item->Scroll.Type;
				if(item->Scroll.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Scroll.Level; 
					mac_pop_item->EffectLevel2 = item->Scroll.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Scroll.Level2; 
					mac_pop_item->EffectLevel2 = item->Scroll.Level2;  
				}
			}
			//We have some worn effect items (Lodizal Shell Shield) as proceffect in db.
			else if(item->Proc.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Proc.Effect;
				mac_pop_item->Effect2 = item->Proc.Effect; 
				if(item->Worn.Type > 0)
				{
					mac_pop_item->EffectType2 = item->Worn.Type;  
					mac_pop_item->common.EffectType1 = item->Worn.Type;
				}
				else
				{
					mac_pop_item->EffectType2 = item->Proc.Type;  
					mac_pop_item->common.EffectType1 = item->Proc.Type;
				}
				if(item->Proc.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Proc.Level; 
					mac_pop_item->EffectLevel2 = item->Proc.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Proc.Level2; 
					mac_pop_item->EffectLevel2 = item->Proc.Level2;  
				}
			}
			else if(item->Worn.Effect > 0)
			{
				mac_pop_item->common.Effect1 = item->Worn.Effect;
				mac_pop_item->Effect2 = item->Worn.Effect; 
				mac_pop_item->EffectType2 = item->Worn.Type;  
				mac_pop_item->common.EffectType1 = item->Worn.Type;
				if(item->Worn.Level > 0)
				{
					mac_pop_item->common.EffectLevel1 = item->Worn.Level; 
					mac_pop_item->EffectLevel2 = item->Worn.Level;
				}
				else
				{
					mac_pop_item->common.EffectLevel1 = item->Worn.Level2; 
					mac_pop_item->EffectLevel2 = item->Worn.Level2;  
				}
			}

		return mac_pop_item;
	}

	structs::Spawn_Struct* MacSpawns(struct Spawn_Struct* emu, int type) 
	{

		if(sizeof(emu) == 0)
			return 0;

		structs::Spawn_Struct *eq = new structs::Spawn_Struct;
		memset(eq,0,sizeof(structs::Spawn_Struct));

		if(type == 0)
			eq->deltaHeading = emu->deltaHeading;
		if(type == 1)
			eq->deltaHeading = 0;
		eq->GM = emu->gm;
		eq->title = emu->aaitle;
		eq->anon = emu->anon;
		memcpy(eq->name, emu->name, 64);
		eq->deity = emu->deity;
		if(emu->race == 42)
			eq->size = emu->size + 2.0f;
		else
			eq->size = emu->size;
		eq->NPC = emu->NPC;
		eq->invis = emu->invis;
		//eq->sneaking = 0;
		eq->cur_hp = emu->curHp;
		eq->x_pos = (int16)emu->x;
		eq->y_pos = (int16)emu->y;
		eq->animation = emu->animation;
		eq->z_pos = (int16)emu->z*10;
		eq->deltaY = 0;
		eq->deltaX = 0;
		eq->heading = (uint8)emu->heading;
		eq->deltaZ = 0;
		eq->anim_type = 0x64;
		eq->level = emu->level;
		eq->petOwnerId = emu->petOwnerId;
		eq->guildrank = emu->guildrank;
		if(emu->NPC == 1)
		{
			eq->guildrank = 0;
			eq->LD=1;
		}
		eq->texture = emu->equip_chest2;
		for(int k = 0; k < 9; k++) 
		{
			eq->equipment[k] = emu->equipment[k];
			eq->equipcolors[k].color = emu->colors[k].color;
		}
		eq->runspeed = emu->runspeed;
		eq->AFK = emu->afk;
		eq->GuildID = emu->guildID;
		if(eq->GuildID == 0)
			eq->GuildID = 0xFFFF;
		eq->helm = emu->helm;
		eq->race = emu->race;
		strncpy(eq->Surname, emu->lastName, 32);
		eq->walkspeed = emu->walkspeed;
		eq->light = emu->light;
		if(emu->class_ > 19 && emu->class_ < 35)
			eq->class_ = emu->class_-3;
		else if(emu->class_ == 40)
			eq->class_ = 16;
		else if(emu->class_ == 41)
			eq->class_ = 32;
		else 
			eq->class_ = emu->class_;
		eq->haircolor = emu->haircolor;
		eq->beardcolor = emu->beardcolor;
		eq->eyecolor1 = emu->eyecolor1;
		eq->eyecolor2 = emu->eyecolor2;
		eq->hairstyle = emu->hairstyle;
		eq->beard = emu->beard;
		eq->face = emu->face;
		eq->gender = emu->gender;
		eq->bodytype = emu->bodytype;
		eq->spawn_id = emu->spawnId;
		eq->flymode = emu->flymode;

		return eq;
	}

	ENCODE(OP_DisciplineUpdate) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_Dye) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_RaidJoin) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_RemoveAllDoors) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_SendAAStats) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_SpellEffect) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_TraderDelItem) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_TraderItemUpdate) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_Untargetable) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_UpdateAA) { ENCODE_FORWARD(OP_Unknown); }
	ENCODE(OP_Unknown)
	{
		EQApplicationPacket *in = *p;
		*p = nullptr;

		Log.Out(Logs::Detail, Logs::Client_Server_Packet, "Dropped an invalid packet: %s", opcodes->EmuToName(in->GetOpcode()));

		delete in;
		return;
	}

	static inline int16 ServerToMacSlot(uint32 ServerSlot)
	{
			 //int16 MacSlot;
			if (ServerSlot == INVALID_INDEX)
				 return INVALID_INDEX;
			
			return ServerSlot; // deprecated
	}

	static inline int16 ServerToMacCorpseSlot(uint32 ServerCorpse)
	{
		return ServerCorpse;
	}

	static inline uint32 MacToServerSlot(int16 MacSlot)
	{
		//uint32 ServerSlot;
		if (MacSlot == INVALID_INDEX)
			 return INVALID_INDEX;
		
		return MacSlot; // deprecated
	}

	static inline uint32 MacToServerCorpseSlot(int16 MacCorpse)
	{
		return MacCorpse;
	}

} //end namespace Mac

