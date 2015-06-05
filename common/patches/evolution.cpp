#include "../global_define.h"
#include "../eqemu_logsys.h"
#include "evolution.h"
#include "../opcodemgr.h"
#include "../eq_stream_ident.h"
#include "../crc32.h"

#include "../eq_packet_structs.h"
#include "../packet_dump_file.h"
#include "../misc_functions.h"
#include "../packet_functions.h"
#include "../string_util.h"
#include "../item.h"
#include "evolution_structs.h"
#include "../rulesys.h"

namespace Evolution {

static const char *name = "Evolution";
static OpcodeManager *opcodes = nullptr;
static Strategy struct_strategy;

void Register(EQStreamIdentifier &into) {
	//create our opcode manager if we havent already
	if(opcodes == nullptr) {
		std::string opfile = "patch_";
		opfile += name;
		opfile += ".conf";
		//load up the opcode manager.
		//TODO: figure out how to support shared memory with multiple patches...
		opcodes = new RegularOpcodeManager();
		if(!opcodes->LoadOpcodes(opfile.c_str())) {
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

void Reload() {

	//we have a big problem to solve here when we switch back to shared memory
	//opcode managers because we need to change the manager pointer, which means
	//we need to go to every stream and replace it's manager.

	if(opcodes != nullptr) {
		//TODO: get this file name from the config file
		std::string opfile = "patch_";
		opfile += name;
		opfile += ".conf";
		if(!opcodes->ReloadOpcodes(opfile.c_str())) {
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
	#include "evolution_ops.h"
}

std::string Strategy::Describe() const {
	std::string r;
	r += "Patch ";
	r += name;
	return(r);
}


#include "ss_define.h"



const EQClientVersion Strategy::ClientVersion() const
{
	return EQClientEvolution;
}

DECODE(OP_SendLoginInfo) {
	DECODE_LENGTH_EXACT(structs::LoginInfo_Struct);
	SETUP_DIRECT_DECODE(LoginInfo_Struct, structs::LoginInfo_Struct);
	memcpy(emu->login_info, eq->AccountName, 64);
	FINISH_DIRECT_DECODE();
}

} //end namespace Evolution
