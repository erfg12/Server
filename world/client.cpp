#include "../common/global_define.h"
#include "../common/eq_packet.h"
#include "../common/eq_stream_intf.h"
#include "../common/misc.h"
#include "../common/rulesys.h"
#include "../common/emu_opcodes.h"
#include "../common/eq_packet_structs.h"
#include "../common/packet_dump.h"
#include "../common/item.h"
#include "../common/races.h"
#include "../common/classes.h"
#include "../common/languages.h"
#include "../common/skills.h"
#include "../common/extprofile.h"
#include "../common/string_util.h"
#include "../common/clientversions.h"
#include "../common/random.h"

#include "client.h"
#include "worlddb.h"
#include "world_config.h"
#include "login_server.h"
#include "login_server_list.h"
#include "zoneserver.h"
#include "zonelist.h"
#include "clientlist.h"
#include "wguild_mgr.h"
#include "sof_char_create_data.h"
#include "console.h"

#include <iostream>
#include <iomanip>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <limits.h>

//FatherNitwit: uncomment to enable my IP based authentication hack
//#define IPBASED_AUTH_HACK

// Disgrace: for windows compile
#ifdef _WINDOWS
	#include <windows.h>
	#include <winsock.h>
#else
	
	#ifdef FREEBSD //Timothy Whitman - January 7, 2003
		#include <sys/types.h>
	#endif

	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
#endif

std::vector<RaceClassAllocation> character_create_allocations;
std::vector<RaceClassCombos> character_create_race_class_combos;

extern ZSList zoneserver_list;
extern LoginServerList loginserverlist;
extern ClientList client_list;
extern EQEmu::Random emu_random;
extern uint32 numclients;
extern volatile bool RunLoops;

Client::Client(EQStreamInterface* ieqs)
:	autobootup_timeout(RuleI(World, ZoneAutobootTimeoutMS)),
	CLE_keepalive_timer(RuleI(World, ClientKeepaliveTimeoutMS)),
	connect(1000),
	eqs(ieqs)
{

	// Live does not send datarate as of 3/11/2005
	//eqs->SetDataRate(7);
	ip = eqs->GetRemoteIP();
	port = ntohs(eqs->GetRemotePort());

	autobootup_timeout.Disable();
	connect.Disable();
	seencharsel = false;
	cle = 0;
	zoneID = 0;
	char_name[0] = 0;
	charid = 0;
	pwaitingforbootup = 0;
	ClientVersionBit = 0;
	numclients++;
}

Client::~Client() {
	if (RunLoops && cle && zoneID == 0)
		cle->SetOnline(CLE_Status_Offline);

	numclients--;

	//let the stream factory know were done with this stream
	eqs->Close();
	eqs->ReleaseFromUse();
}

void Client::SendLogServer()
{
	auto outapp = new EQApplicationPacket(OP_LogServer, sizeof(LogServer_Struct));
	LogServer_Struct *l=(LogServer_Struct *)outapp->pBuffer;
	const char *wsn=WorldConfig::get()->ShortName.c_str();
	memcpy(l->worldshortname,wsn,strlen(wsn));

	if(RuleB(Mail, EnableMailSystem))
		l->enablemail = 1;

	if(RuleB(Chat, EnableVoiceMacros))
		l->enablevoicemacros = 1;

	l->enable_pvp = (RuleI(World, PVPSettings));

	if(RuleB(World, IsGMPetitionWindowEnabled))
		l->enable_petition_wnd = 1;

	if(RuleI(World, FVNoDropFlag) == 1 || RuleI(World, FVNoDropFlag) == 2 && GetAdmin() > RuleI(Character, MinStatusForNoDropExemptions))
		l->enable_FV = 1;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendEnterWorld(std::string name)
{
	char char_name[64] = { 0 };
	if (pZoning && database.GetLiveChar(GetAccountID(), char_name)) {
		if(database.GetAccountIDByChar(char_name) != GetAccountID()) {
			eqs->Close();
			return;
		} else {
			Log.Out(Logs::Detail, Logs::World_Server,"Telling client to continue session.");
		}
	}

	auto outapp = new EQApplicationPacket(OP_EnterWorld, strlen(char_name) + 1);
	memcpy(outapp->pBuffer,char_name,strlen(char_name)+1);
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendExpansionInfo() {
	auto outapp = new EQApplicationPacket(OP_ExpansionInfo, sizeof(ExpansionInfo_Struct));
	ExpansionInfo_Struct *eis = (ExpansionInfo_Struct*)outapp->pBuffer;
	eis->Expansions = GetExpansion();
	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendCharInfo() {
	if (cle) {
		cle->SetOnline(CLE_Status_CharSelect);
	}

	seencharsel = true;


	// Send OP_SendCharInfo
	auto outapp = new EQApplicationPacket(OP_SendCharInfo, sizeof(CharacterSelect_Struct));
	CharacterSelect_Struct* cs = (CharacterSelect_Struct*)outapp->pBuffer;

	database.GetCharSelectInfo(GetAccountID(), cs, ClientVersionBit);

	QueuePacket(outapp);
	safe_delete(outapp);
}

bool Client::HandleSendLoginInfoPacket(const EQApplicationPacket *app) {
	if (app->size != sizeof(LoginInfo_Struct)) {
		return false;
	}

	LoginInfo_Struct *li=(LoginInfo_Struct *)app->pBuffer;

	// Quagmire - max len for name is 18, pass 15
	char name[19] = {0};
	char password[16] = {0};
	strn0cpy(name, (char*)li->login_info,18);
	strn0cpy(password, (char*)&(li->login_info[strlen(name)+1]), 15);

	if (strlen(password) <= 1) {
		// TODO: Find out how to tell the client wrong username/password
		Log.Out(Logs::Detail, Logs::World_Server,"Login without a password");
		return false;
	}

	pZoning=(li->zoning==1);

#ifdef IPBASED_AUTH_HACK
	struct in_addr tmpip;
	tmpip.s_addr = ip;
#endif
	uint32 id=0;
	if (strncasecmp(name, "LS#", 3) == 0)
		id=atoi(&name[3]);
	else if(database.GetAccountIDByName(name))
		id=database.GetAccountIDByName(name);
	else
		id=atoi(name);
#ifdef IPBASED_AUTH_HACK
	if ((cle = zoneserver_list.CheckAuth(inet_ntoa(tmpip), password)))
#else
	if (loginserverlist.Connected() == false && !pZoning) {
		Log.Out(Logs::Detail, Logs::World_Server,"Error: Login server login while not connected to login server.");
		return false;
	}

	if (((cle = client_list.CheckAuth(name, password)) || (cle = client_list.CheckAuth(id, password))))
#endif
	{
		if(GetSessionLimit())
			return false;

		cle->SetOnline();
		
		if(eqs->ClientVersion() == EQClientMac)
		{
			ClientVersionBit = cle->GetMacClientVersion();
		}
		else
		{
			if(eqs->ClientVersion() == EQClientUnused)
				ClientVersionBit = 1;
			else
				ClientVersionBit = 16;
		}

		Log.Out(Logs::Detail, Logs::World_Server, "ClientVersionBit is: %i", ClientVersionBit);
		Log.Out(Logs::Detail, Logs::World_Server,"Logged in. Mode=%s",pZoning ? "(Zoning)" : "(CharSel)");
		Log.Out(Logs::Detail, Logs::World_Server, "LS Account #%d", cle->LSID());

		const WorldConfig *Config=WorldConfig::get();

		if(Config->UpdateStats){
			auto pack = new ServerPacket;
			pack->opcode = ServerOP_LSPlayerJoinWorld;
			pack->size = sizeof(ServerLSPlayerJoinWorld_Struct);
			pack->pBuffer = new uchar[pack->size];
			memset(pack->pBuffer,0,pack->size);
			ServerLSPlayerJoinWorld_Struct* join =(ServerLSPlayerJoinWorld_Struct*)pack->pBuffer;
			strcpy(join->key,GetLSKey());
			join->lsaccount_id = GetLSID();
			loginserverlist.SendPacket(pack);
			safe_delete(pack);
		}

		expansion = database.GetExpansion(cle->AccountID());

		if (!pZoning && ClientVersionBit != 0)
			//SendGuildList();
			SendLogServer();
			SendApproveWorld();
			SendEnterWorld(cle->name());
		if (!pZoning) {
			SendExpansionInfo();
			SendCharInfo();
			database.LoginIP(cle->AccountID(), long2ip(GetIP()).c_str());
		}

	}
	else {
		// TODO: Find out how to tell the client wrong username/password
		Log.Out(Logs::Detail, Logs::World_Server,"Bad/Expired session key '%s'",name);
		return false;
	}

	if (!cle)
		return true;

	cle->SetIP(GetIP());
	return true;
}

bool Client::HandleNameApprovalPacket(const EQApplicationPacket *app)
{
	if (GetAccountID() == 0) {
		Log.Out(Logs::Detail, Logs::World_Server,"Name approval request with no logged in account");
		return false;
	}

	snprintf(char_name, 64, "%s", (char*)app->pBuffer);
	uchar race = app->pBuffer[64];
	uchar clas = app->pBuffer[68];

	Log.Out(Logs::Detail, Logs::World_Server, "Name approval request. Name=%s, race=%s, class=%s", char_name, GetRaceName(race), GetEQClassName(clas));

	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket;
	outapp->SetOpcode(OP_ApproveName);
	outapp->pBuffer = new uchar[1];
	outapp->size = 1;

	Log.Out(Logs::Detail, Logs::World_Server, "common/client.cpp line 292 pass");
	bool valid = false;
	if (!database.CheckNameFilter(char_name)) {
		Log.Out(Logs::Detail, Logs::World_Server, "Invalid name");
		valid = false; 
	}
	/* Name must begin with an upper-case letter. */
	else if (islower(char_name[0])) {
		Log.Out(Logs::Detail, Logs::World_Server, "Must begin with uppercase");
		valid = false; 
	} 
	else if (database.ReserveName(GetAccountID(), char_name)) {
		Log.Out(Logs::Detail, Logs::World_Server, "common/client.cpp line 304 pass");
		valid = true; 	
	}
	else {
		Log.Out(Logs::Detail, Logs::World_Server, "common/client.cpp line 308 pass");
		valid = false; 
	}

	Log.Out(Logs::Detail, Logs::World_Server, "common/client.cpp line 312 pass");
	outapp->pBuffer[0] = valid? 1 : 0;
	QueuePacket(outapp);
	safe_delete(outapp);
	Log.Out(Logs::Detail, Logs::World_Server, "common/client.cpp line 316 pass");

	if (!valid)
		memset(char_name, 0, sizeof(char_name));
	Log.Out(Logs::Detail, Logs::World_Server, "common/client.cpp line 320 pass");

	return true;
}

bool Client::HandleGenerateRandomNamePacket(const EQApplicationPacket *app) {
	// creates up to a 10 char name
	char vowels[18]="aeiouyaeiouaeioe";
	char cons[48]="bcdfghjklmnpqrstvwxzybcdgklmnprstvwbcdgkpstrkd";
	char rndname[17]="\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	char paircons[33]="ngrkndstshthphsktrdrbrgrfrclcr";
	int rndnum=emu_random.Int(0, 75),n=1;
	bool dlc=false;
	bool vwl=false;
	bool dbl=false;
	if (rndnum>63)
	{	// rndnum is 0 - 75 where 64-75 is cons pair, 17-63 is cons, 0-16 is vowel
		rndnum=(rndnum-61)*2;	// name can't start with "ng" "nd" or "rk"
		rndname[0]=paircons[rndnum];
		rndname[1]=paircons[rndnum+1];
		n=2;
	}
	else if (rndnum>16)
	{
		rndnum-=17;
		rndname[0]=cons[rndnum];
	}
	else
	{
		rndname[0]=vowels[rndnum];
		vwl=true;
	}
	int namlen=emu_random.Int(5, 10);
	for (int i=n;i<namlen;i++)
	{
		dlc=false;
		if (vwl)	//last char was a vowel
		{			// so pick a cons or cons pair
			rndnum=emu_random.Int(0, 62);
			if (rndnum>46)
			{	// pick a cons pair
				if (i>namlen-3)	// last 2 chars in name?
				{	// name can only end in cons pair "rk" "st" "sh" "th" "ph" "sk" "nd" or "ng"
					rndnum=emu_random.Int(0, 7)*2;
				}
				else
				{	// pick any from the set
					rndnum=(rndnum-47)*2;
				}
				rndname[i]=paircons[rndnum];
				rndname[i+1]=paircons[rndnum+1];
				dlc=true;	// flag keeps second letter from being doubled below
				i+=1;
			}
			else
			{	// select a single cons
				rndname[i]=cons[rndnum];
			}
		}
		else
		{		// select a vowel
			rndname[i]=vowels[emu_random.Int(0, 16)];
		}
		vwl=!vwl;
		if (!dbl && !dlc)
		{	// one chance at double letters in name
			if (!emu_random.Int(0, i+9))	// chances decrease towards end of name
			{
				rndname[i+1]=rndname[i];
				dbl=true;
				i+=1;
			}
		}
	}

	rndname[0]=toupper(rndname[0]);
	NameGeneration_Struct* ngs = (NameGeneration_Struct*)app->pBuffer;
	memset(ngs->name,0,64);
	strcpy(ngs->name,rndname);

	QueuePacket(app);
	return true;
}

bool Client::HandleCharacterCreatePacket(const EQApplicationPacket *app) {
	if (GetAccountID() == 0) {
		Log.Out(Logs::Detail, Logs::World_Server,"Account ID not set; unable to create character.");
		return false;
	}
	else if (app->size != sizeof(CharCreate_Struct)) {
		Log.Out(Logs::Detail, Logs::World_Server,"Wrong size on OP_CharacterCreate. Got: %d, Expected: %d",app->size,sizeof(CharCreate_Struct));
		DumpPacket(app);
		// the previous behavior was essentially returning true here
		// but that seems a bit odd to me.
		return true;
	}

	CharCreate_Struct *cc = (CharCreate_Struct*)app->pBuffer;
	if(OPCharCreate(char_name, cc) == false) {
		database.DeleteCharacter(char_name);
		auto outapp = new EQApplicationPacket(OP_ApproveName, 1);
		outapp->pBuffer[0] = 0;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
	else
	{
		SendCharInfo();
	}

	return true;
}

bool Client::HandleEnterWorldPacket(const EQApplicationPacket *app) { 
	if (GetAccountID() == 0) {
		Log.Out(Logs::Detail, Logs::World_Server,"Enter world with no logged in account");
		eqs->Close();
		return true;
	}

	if(GetAdmin() < 0)
	{
		Log.Out(Logs::Detail, Logs::World_Server,"Account banned or suspended.");
		eqs->Close();
		return true;
	}

	if (RuleI(World, MaxClientsPerIP) >= 0) {
		client_list.GetCLEIP(this->GetIP()); //Check current CLE Entry IPs against incoming connection
	}

	EnterWorld_Struct *ew=(EnterWorld_Struct *)app->pBuffer;
	strn0cpy(char_name, ew->name, 64);

	EQApplicationPacket *outapp;
	uint32 tmpaccid = 0;
	charid = database.GetCharacterInfo(char_name, &tmpaccid, &zoneID, &instanceID);
	if (charid == 0 || tmpaccid != GetAccountID()) {
		Log.Out(Logs::Detail, Logs::World_Server,"Could not get CharInfo for '%s'",char_name);
		eqs->Close();
		return true;
	}

	// Make sure this account owns this character
	if (tmpaccid != GetAccountID()) {
		Log.Out(Logs::Detail, Logs::World_Server,"This account does not own the character named '%s'",char_name);
		eqs->Close();
		return true;
	}

	if (zoneID == 0 || !database.GetZoneName(zoneID)) {
		// This is to save people in an invalid zone, once it's removed from the DB
		database.MoveCharacterToZone(charid, "arena");
		Log.Out(Logs::Detail, Logs::World_Server, "Zone not found in database zone_id=%i, moveing char to arena character:%s", zoneID, char_name);
	}

	if(instanceID > 0)
	{
		if(!database.VerifyInstanceAlive(instanceID, GetCharID()))
		{
			zoneID = database.MoveCharacterToBind(charid);
			instanceID = 0;
		}
		else
		{
			if(!database.VerifyZoneInstance(zoneID, instanceID))
			{
				zoneID = database.MoveCharacterToBind(charid);
				instanceID = 0;
			}
		}
	}

	if(!pZoning) {
		database.SetGroupID(char_name, 0, charid);
		database.SetFirstLogon(charid, 1);
	}
	else{
		uint32 groupid = database.GetGroupID(char_name);
		if(groupid > 0){
			char* leader = 0;
			char leaderbuf[64] = {0};
			if((leader = database.GetGroupLeaderForLogin(char_name, leaderbuf)) && strlen(leader)>1){
				auto outapp3 = new EQApplicationPacket(OP_GroupUpdate, sizeof(GroupJoin_Struct));
				GroupJoin_Struct* gj=(GroupJoin_Struct*)outapp3->pBuffer;
				gj->action=8;
				strcpy(gj->yourname, char_name);
				strcpy(gj->membername, leader);
				QueuePacket(outapp3);
				safe_delete(outapp3);
			}
		}
	}

	outapp = new EQApplicationPacket(OP_MOTD);
	char tmp[500] = {0};
	if (database.GetVariable("MOTD", tmp, 500)) {
		outapp->size = strlen(tmp)+1;
		outapp->pBuffer = new uchar[outapp->size];
		memset(outapp->pBuffer,0,outapp->size);
		strcpy((char*)outapp->pBuffer, tmp);

	} else {
		// Null Message of the Day. :)
		outapp->size = 1;
		outapp->pBuffer = new uchar[outapp->size];
		outapp->pBuffer[0] = 0;
	}
	QueuePacket(outapp);
	safe_delete(outapp);

	int MailKey = emu_random.Int(1, INT_MAX);

	database.SetMailKey(charid, GetIP(), MailKey);

	char ConnectionType;

	ConnectionType = 'C';

	auto outapp2 = new EQApplicationPacket(OP_SetChatServer);
	char buffer[112];

	const WorldConfig *Config = WorldConfig::get();

	sprintf(buffer,"%s,%i,%s.%s,%c%08X",
		Config->ChatHost.c_str(),
		Config->ChatPort,
		Config->ShortName.c_str(),
		this->GetCharName(), ConnectionType, MailKey
	);
	outapp2->size=strlen(buffer)+1;
	outapp2->pBuffer = new uchar[outapp2->size];
	memcpy(outapp2->pBuffer,buffer,outapp2->size);
	QueuePacket(outapp2);
	safe_delete(outapp2);

	EnterWorld();

	return true;
}

bool Client::HandleDeleteCharacterPacket(const EQApplicationPacket *app) {

	uint32 char_acct_id = database.GetAccountIDByChar((char*)app->pBuffer);
	if(char_acct_id == GetAccountID()) {
		Log.Out(Logs::Detail, Logs::World_Server,"Delete character: %s",app->pBuffer);
		database.DeleteCharacter((char *)app->pBuffer);
		SendCharInfo();
	}

	return true;
}

bool Client::HandlePacket(const EQApplicationPacket *app) {

	EmuOpcode opcode = app->GetOpcode();

	Log.Out(Logs::Detail, Logs::World_Server,"Recevied EQApplicationPacket");

	if (!eqs->CheckState(ESTABLISHED)) {
		Log.Out(Logs::Detail, Logs::World_Server,"Client disconnected (net inactive on send)");
		return false;
	}

	// Voidd: Anti-GM Account hack, Checks source ip against valid GM Account IP Addresses
	if (RuleB(World, GMAccountIPList) && this->GetAdmin() >= (RuleI(World, MinGMAntiHackStatus))) {
		if(!database.CheckGMIPs(long2ip(this->GetIP()).c_str(), this->GetAccountID())) {
			Log.Out(Logs::Detail, Logs::World_Server,"GM Account not permited from source address %s and accountid %i", long2ip(this->GetIP()).c_str(), this->GetAccountID());
			eqs->Close();
		}
	}

	if (GetAccountID() == 0 && opcode != OP_SendLoginInfo) {
		// Got a packet other than OP_SendLoginInfo when not logged in
		Log.Out(Logs::Detail, Logs::World_Server,"Expecting OP_SendLoginInfo, got %s", OpcodeNames[opcode]);
		return false;
	}

	switch(opcode)
	{
		case OP_SendLoginInfo:
		{
			return HandleSendLoginInfoPacket(app);
		}
		case OP_ApproveName: //Name approval
		{
			return HandleNameApprovalPacket(app);
		}
		case OP_RandomNameGenerator:
		{
			return HandleGenerateRandomNamePacket(app);
		}
		case OP_CharacterCreate: //Char create
		{
			return HandleCharacterCreatePacket(app);
		}
		case OP_EnterWorld: // Enter world
		{
			return HandleEnterWorldPacket(app);
		}
		case OP_DeleteCharacter:
		{
			return HandleDeleteCharacterPacket(app);
		}
		case OP_GuildsList:
		{
			SendGuildList();
			return true;
		}
		case OP_WorldLogout:
		{
			eqs->Close();
			return true;
		}
		
		case OP_ZoneChange:
		case OP_LoginUnknown1:
		case OP_LoginUnknown2:
		case OP_WearChange:
		case OP_LoginComplete:
		case OP_ApproveWorld:
		{
			// Essentially we are just 'eating' these packets, indicating
			// they are handled.
			return true;
		}
		case OP_ChecksumExe:
		case OP_ChecksumSpell:
		{
			// Implementation is not open source.
			return true;
		}
		default:
		{
			Log.Out(Logs::Detail, Logs::World_Server,"Received unknown EQApplicationPacket");
			return true;
		}
	}
	return true;
}

bool Client::Process() {
	bool ret = true;
	//bool sendguilds = true;
	sockaddr_in to;

	memset((char *) &to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = port;
	to.sin_addr.s_addr = ip;

	if (autobootup_timeout.Check()) {
		Log.Out(Logs::Detail, Logs::World_Server, "Zone bootup timer expired, bootup failed or too slow.");
		ZoneUnavail();
	}
	if(connect.Check()){
		//SendGuildList();// Send OPCode: OP_GuildsList
		SendApproveWorld();
		connect.Disable();
	}
	if (CLE_keepalive_timer.Check()) {
		if (cle)
			cle->KeepAlive();
	}

	/************ Get all packets from packet manager out queue and process them ************/
	EQApplicationPacket *app = 0;
	while(ret && (app = (EQApplicationPacket *)eqs->PopPacket())) {
		ret = HandlePacket(app);

		delete app;
	}

	if (!eqs->CheckState(ESTABLISHED)) {
		if(WorldConfig::get()->UpdateStats){
			auto pack = new ServerPacket;
			pack->opcode = ServerOP_LSPlayerLeftWorld;
			pack->size = sizeof(ServerLSPlayerLeftWorld_Struct);
			pack->pBuffer = new uchar[pack->size];
			memset(pack->pBuffer,0,pack->size);
			ServerLSPlayerLeftWorld_Struct* logout =(ServerLSPlayerLeftWorld_Struct*)pack->pBuffer;
			strcpy(logout->key,GetLSKey());
			logout->lsaccount_id = GetLSID();
			loginserverlist.SendPacket(pack);
			safe_delete(pack);
		}
		Log.Out(Logs::Detail, Logs::World_Server,"Client disconnected (not active in process)");
		return false;
	}

	return ret;
}

void Client::EnterWorld(bool TryBootup) {
	if (zoneID == 0)
		return;

	//if(GetSessionLimit())
	//	return;

	ZoneServer* zs = nullptr;
	if(instanceID > 0)
	{
		if(database.VerifyInstanceAlive(instanceID, GetCharID()))
		{
			if(database.VerifyZoneInstance(zoneID, instanceID))
			{
				zs = zoneserver_list.FindByInstanceID(instanceID);
			}
			else
			{
				instanceID = 0;
				zs = nullptr;
				database.MoveCharacterToBind(GetCharID());
				ZoneUnavail();
				return;
			}
		}
		else
		{
			instanceID = 0;
			zs = nullptr;
			database.MoveCharacterToBind(GetCharID());
			ZoneUnavail();
			return;
		}
	}
	else
		zs = zoneserver_list.FindByZoneID(zoneID);


	const char *zone_name=database.GetZoneName(zoneID, true);
	if (zs) {
		// warn the world we're comming, so it knows not to shutdown
		zs->IncomingClient(this);
	}
	else {
		if (TryBootup) {
			Log.Out(Logs::Detail, Logs::World_Server,"Attempting autobootup of %s (%d:%d)",zone_name,zoneID,instanceID);
			autobootup_timeout.Start();
			pwaitingforbootup = zoneserver_list.TriggerBootup(zoneID, instanceID);
			if (pwaitingforbootup == 0) {
				Log.Out(Logs::Detail, Logs::World_Server,"No zoneserver available to boot up.");
				ZoneUnavail();
			}
			return;
		}
		else {
			Log.Out(Logs::Detail, Logs::World_Server,"Requested zone %s is not running.",zone_name);
			ZoneUnavail();
			return;
		}
	}
	pwaitingforbootup = 0;

	if(!cle) {
		return;
	}

	cle->SetChar(charid, char_name);

	database.CharacterJoin(charid, char_name);
	database.UpdateLiveChar(char_name, GetAccountID());
	Log.Out(Logs::Detail, Logs::World_Server,"%s %s (%d:%d)",seencharsel ? "Entering zone" : "Zoning to",zone_name,zoneID,instanceID);
//	database.SetAuthentication(account_id, char_name, zone_name, ip);

	if (seencharsel) {
		if (GetAdmin() < 80 && zoneserver_list.IsZoneLocked(zoneID)) {
			Log.Out(Logs::Detail, Logs::World_Server,"Enter world failed. Zone is locked.");
			ZoneUnavail();
			return;
		}

		auto pack = new ServerPacket;
		pack->opcode = ServerOP_AcceptWorldEntrance;
		pack->size = sizeof(WorldToZone_Struct);
		pack->pBuffer = new uchar[pack->size];
		memset(pack->pBuffer, 0, pack->size);
		WorldToZone_Struct* wtz = (WorldToZone_Struct*) pack->pBuffer;
		wtz->account_id = GetAccountID();
		wtz->response = 0;
		zs->SendPacket(pack);
		delete pack;
	}
	else {	// if they havent seen character select screen, we can assume this is a zone
			// to zone movement, which should be preauthorized before they leave the previous zone
		Clearance(1);
	}
}

void Client::Clearance(int8 response)
{
	ZoneServer* zs = nullptr;
	if(instanceID > 0)
	{
		zs = zoneserver_list.FindByInstanceID(instanceID);
	}
	else
	{
		zs = zoneserver_list.FindByZoneID(zoneID);
	}

	if(zs == 0 || response == -1 || response == 0)
	{
		if (zs == 0)
		{
			Log.Out(Logs::Detail, Logs::World_Server,"Unable to find zoneserver in Client::Clearance!!");
		} else {
			Log.Out(Logs::Detail, Logs::World_Server, "Invalid response %d in Client::Clearance", response);
		}

		ZoneUnavail();
		return;
	}

	EQApplicationPacket* outapp;

	if (zs->GetCAddress() == nullptr) {
		Log.Out(Logs::Detail, Logs::World_Server, "Unable to do zs->GetCAddress() in Client::Clearance!!");
		ZoneUnavail();
		return;
	}

	if (zoneID == 0) {
		Log.Out(Logs::Detail, Logs::World_Server, "zoneID is nullptr in Client::Clearance!!");
		ZoneUnavail();
		return;
	}

	const char* zonename = database.GetZoneName(zoneID);
	if (zonename == 0) {
		Log.Out(Logs::Detail, Logs::World_Server, "zonename is nullptr in Client::Clearance!!");
		ZoneUnavail();
		return;
	}

	// @bp This is the chat server
	/*
	char packetData[] = "64.37.148.34.9876,MyServer,Testchar,23cd2c95";
	outapp = new EQApplicationPacket(OP_0x0282, sizeof(packetData));
	strcpy((char*)outapp->pBuffer, packetData);
	QueuePacket(outapp);
	delete outapp;
	*/

	// Send zone server IP data
	outapp = new EQApplicationPacket(OP_ZoneServerInfo, sizeof(ZoneServerInfo_Struct));
	ZoneServerInfo_Struct* zsi = (ZoneServerInfo_Struct*)outapp->pBuffer;
	const char *zs_addr=zs->GetCAddress();
	if (!zs_addr[0]) {
		if (cle->IsLocalClient()) {
			struct in_addr in;
			in.s_addr = zs->GetIP();
			zs_addr=inet_ntoa(in);
			if (!strcmp(zs_addr,"127.0.0.1"))
				zs_addr=WorldConfig::get()->LocalAddress.c_str();
		} else {
			zs_addr=WorldConfig::get()->WorldAddress.c_str();
		}
	}
	strcpy(zsi->ip, zs_addr);
	zsi->port =zs->GetCPort();
	Log.Out(Logs::Detail, Logs::World_Server,"Sending client to zone %s (%d:%d) at %s:%d",zonename,zoneID,instanceID,zsi->ip,zsi->port);
	QueuePacket(outapp);
	safe_delete(outapp);

	if (cle)
	{
		autobootup_timeout.Disable();
		cle->SetOnline(CLE_Status_Zoning);
	}
}

void Client::ZoneUnavail() {
	auto outapp = new EQApplicationPacket(OP_ZoneUnavail, sizeof(ZoneUnavail_Struct));
	ZoneUnavail_Struct* ua = (ZoneUnavail_Struct*)outapp->pBuffer;
	const char* zonename = database.GetZoneName(zoneID);
	if (zonename)
		strcpy(ua->zonename, zonename);
	QueuePacket(outapp);
	delete outapp;

	zoneID = 0;
	pwaitingforbootup = 0;
	autobootup_timeout.Disable();
}

bool Client::GenPassKey(char* key) {
	char* passKey=nullptr;
	*passKey += ((char)('A'+((int)emu_random.Int(0, 25))));
	*passKey += ((char)('A'+((int)emu_random.Int(0, 25))));
	memcpy(key, passKey, strlen(passKey));
	return true;
}

void Client::QueuePacket(const EQApplicationPacket* app, bool ack_req) {
	Log.Out(Logs::Detail, Logs::World_Server, "Sending EQApplicationPacket OpCode 0x%04x",app->GetOpcode());

	//ack_req = true;	// It's broke right now, dont delete this line till fix it. =P
	eqs->QueuePacket(app, ack_req);
}

void Client::SendGuildList() {
	EQApplicationPacket *outapp;
	outapp = new EQApplicationPacket(OP_GuildsList);

	//ask the guild manager to build us a nice guild list packet
	OldGuildsList_Struct* guildstruct = guild_mgr.MakeOldGuildList(outapp->size);
	outapp->pBuffer = reinterpret_cast<uchar*>(guildstruct);
	//safe_delete_array(guildstruct);

	if(outapp->pBuffer == nullptr) {
		return;
	}


	eqs->FastQueuePacket((EQApplicationPacket **)&outapp);
}

// @merth: I have no idea what this struct is for, so it's hardcoded for now
void Client::SendApproveWorld()
{
	EQApplicationPacket* outapp;

	// Send OPCode: OP_ApproveWorld, size: 544
	outapp = new EQApplicationPacket(OP_ApproveWorld, sizeof(ApproveWorld_Struct));
	ApproveWorld_Struct* aw = (ApproveWorld_Struct*)outapp->pBuffer;
	uchar foo[] = {
//0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x95,0x5E,0x30,0xA5,0xCA,0xD4,0xEA,0xF5,
//0xCB,0x14,0xFC,0xF7,0x78,0xE2,0x73,0x15,0x90,0x17,0xCE,0x7A,0xEB,0xEC,0x3C,0x34,
//0x5C,0x6D,0x10,0x05,0xFC,0xEA,0xED,0x19,0xC5,0x0D,0x7A,0x82,0x17,0xCC,0xCC,0x71,
//0x56,0x38,0xDF,0x78,0x8D,0xE6,0x44,0xD3,0x6F,0xDB,0xE3,0xCF,0x21,0x30,0x75,0x2F,
//0xCD,0xDC,0xE9,0xB4,0xA4,0x4E,0x58,0xDE,0xEE,0x54,0xDD,0x87,0xDA,0xE9,0xC6,0xC8,
//0x02,0xDD,0xC4,0xFD,0x94,0x36,0x32,0xAD,0x1B,0x39,0x0F,0x00,0x00,0x00,0x00,0x00,

0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x37,0x87,0x13,0xbe,0xc8,0xa7,0x77,0xcb,
0x27,0xed,0xe1,0xe6,0x5d,0x1c,0xaa,0xd3,0x3c,0x26,0x3b,0x6d,0x8c,0xdb,0x36,0x8d,
0x91,0x72,0xf5,0xbb,0xe0,0x5c,0x50,0x6f,0x09,0x6d,0xc9,0x1e,0xe7,0x2e,0xf4,0x38,
0x1b,0x5e,0xa8,0xc2,0xfe,0xb4,0x18,0x4a,0xf7,0x72,0x85,0x13,0xf5,0x63,0x6c,0x16,
0x69,0xf4,0xe0,0x17,0xff,0x87,0x11,0xf3,0x2b,0xb7,0x73,0x04,0x37,0xca,0xd5,0x77,
0xf8,0x03,0x20,0x0a,0x56,0x8b,0xfb,0x35,0xff,0x59,0x00,0x00,0x00,0x00,0x00,0x00,

//0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0x42,0x69,0x2a,0x87,0xdd,0x04,0x3d,
//0x7f,0xb1,0xb3,0xbb,0xde,0xd5,0x5f,0xfc,0x1f,0xb3,0x25,0x94,0x16,0xd5,0xf3,0x97,
//0x43,0xdf,0xb9,0x69,0x68,0xdf,0x2b,0x64,0x98,0xf5,0x44,0xbe,0x38,0x65,0xef,0xff,
//0x36,0x89,0x90,0xcf,0x26,0xbb,0x9f,0x76,0xd5,0xaf,0x6d,0xf2,0x08,0xbe,0xce,0xd8,
//0x3e,0x4b,0x53,0x8a,0xf3,0x44,0x7c,0x19,0x49,0x5d,0x97,0x99,0xd8,0x8b,0xee,0x10,
//0x1a,0x7d,0xb7,0x8b,0x49,0x9b,0x40,0x8c,0xea,0x49,0x09,0x00,0x00,0x00,0x00,0x00,
//
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x15,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x53,0xC3,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00
};
	memcpy(aw->unknown544, foo, sizeof(foo));
	QueuePacket(outapp);
	safe_delete(outapp);
}

bool Client::OPCharCreate(char *name, CharCreate_Struct *cc)
{
	if (!RuleB(Character, CanCreate))
		return false;
	PlayerProfile_Struct pp;
	ExtendedProfile_Struct ext;
	Inventory inv;
	time_t bday = time(nullptr);
	char startzone[50]={0};
	uint32 i;
	struct in_addr in;

	int stats_sum = cc->STR + cc->STA + cc->AGI + cc->DEX + cc->WIS + cc->INT + cc->CHA;

	in.s_addr = GetIP();

	if(cc->face == 0 && cc->oldface > 0)
		cc->face = cc->oldface;

	Log.Out(Logs::Detail, Logs::World_Server, "Character creation request from %s LS#%d (%s:%d) : ", GetCLE()->LSName(), GetCLE()->LSID(), inet_ntoa(in), GetPort());
	Log.Out(Logs::Detail, Logs::World_Server, "Name: %s", name);
	Log.Out(Logs::Detail, Logs::World_Server, "Race: %d  Class: %d  Gender: %d  Deity: %d  Start zone: %d",
		cc->race, cc->class_, cc->gender, cc->deity, cc->start_zone);
	Log.Out(Logs::Detail, Logs::World_Server, "STR  STA  AGI  DEX  WIS  INT  CHA    Total");
	Log.Out(Logs::Detail, Logs::World_Server, "%3d  %3d  %3d  %3d  %3d  %3d  %3d     %3d",
		cc->STR, cc->STA, cc->AGI, cc->DEX, cc->WIS, cc->INT, cc->CHA,
		stats_sum);
	Log.Out(Logs::Detail, Logs::World_Server, "Face: %d  Eye colors: %d %d", cc->face, cc->eyecolor1, cc->eyecolor2);
	Log.Out(Logs::Detail, Logs::World_Server, "Hairstyle: %d  Haircolor: %d", cc->hairstyle, cc->haircolor);
	Log.Out(Logs::Detail, Logs::World_Server, "Beard: %d  Beardcolor: %d", cc->beard, cc->beardcolor);

	/* Validate the char creation struct */
		if(!CheckCharCreateInfo(cc)) {
			Log.Out(Logs::Detail, Logs::World_Server,"CheckCharCreateInfo did not validate the request (bad race/class/stats)");
			return false;
		}
	/* Convert incoming cc_s to the new PlayerProfile_Struct */
	memset(&pp, 0, sizeof(PlayerProfile_Struct));	// start building the profile

	strn0cpy(pp.name, name, 63);

	pp.race				= cc->race;
	pp.class_			= cc->class_;
	pp.gender			= cc->gender;
	pp.deity			= cc->deity;
	pp.STR				= cc->STR;
	pp.STA				= cc->STA;
	pp.AGI				= cc->AGI;
	pp.DEX				= cc->DEX;
	pp.WIS				= cc->WIS;
	pp.INT				= cc->INT;
	pp.CHA				= cc->CHA;
	pp.face				= cc->face;
	pp.eyecolor1		= cc->eyecolor1;
	pp.eyecolor2		= cc->eyecolor2;
	pp.hairstyle		= cc->hairstyle;
	pp.haircolor		= cc->haircolor;
	pp.beard			= cc->beard;
	pp.beardcolor		= cc->beardcolor;
	pp.birthday		= bday;
	pp.lastlogin	= bday;
	pp.level			= 1;
	pp.points			= 5;
	pp.cur_hp			= 1000; // 1k hp during dev only
	pp.hunger_level = 6000;
	pp.thirst_level = 6000;

	/* Set Racial and Class specific language and skills */
	SetRacialLanguages(&pp);
	SetRaceStartingSkills(&pp);
	SetClassStartingSkills(&pp);
	SetClassLanguages(&pp);

	for (i = 0; i < MAX_PP_REF_SPELLBOOK; i++)
		pp.spell_book[i] = 0xFFFFFFFF;

	for(i = 0; i < MAX_PP_REF_MEMSPELL; i++)
		pp.mem_spells[i] = 0xFFFFFFFF;

	for(i = 0; i < BUFF_COUNT; i++)
		pp.buffs[i].spellid = 0xFFFF;

	/* If server is PVP by default, make all character set to it. */
	pp.pvp = database.GetServerType() == 1 ? 1 : 0;

	// if there's a startzone variable put them in there
	if(database.GetVariable("startzone", startzone, 50))
	{
		Log.Out(Logs::Detail, Logs::World_Server,"Found 'startzone' variable setting: %s", startzone);
		pp.zone_id = database.GetZoneID(startzone);
		if(pp.zone_id)
			database.GetSafePoints(pp.zone_id, 0, &pp.x, &pp.y, &pp.z);
		else
			Log.Out(Logs::Detail, Logs::World_Server,"Error getting zone id for '%s'", startzone);
	}
	else	// otherwise use normal starting zone logic
	{
		bool ValidStartZone = false;

		ValidStartZone = database.GetStartZone(&pp, cc);

		if(!ValidStartZone)
			return false;
	}

	/* just in case  */
	if (!pp.zone_id) {
		pp.zone_id = qeynos;
		pp.x = pp.y = pp.z = -1;
	}

	if (!pp.binds[0].zoneId)
	{
		pp.binds[0].zoneId = pp.zone_id;
		pp.binds[0].x = pp.x;
		pp.binds[0].y = pp.y;
		pp.binds[0].z = pp.z;
		pp.binds[0].heading = pp.heading;
	}

	// set starting city location to the initial bind point
	pp.binds[4] = pp.binds[0];

	Log.Out(Logs::Detail, Logs::World_Server, "Current location: %s  %0.2f, %0.2f, %0.2f, %0.2f",
		database.GetZoneName(pp.zone_id), pp.x, pp.y, pp.z, pp.heading);
	Log.Out(Logs::Detail, Logs::World_Server, "Bind location: %s  %0.2f, %0.2f, %0.2f",
		database.GetZoneName(pp.binds[0].zoneId), pp.binds[0].x, pp.binds[0].y, pp.binds[0].z);

	/* Starting Items inventory */
	database.SetStartingItems(&pp, &inv, pp.race, pp.class_, pp.deity, pp.zone_id, pp.name, GetAdmin());

	// now we give the pp and the inv we made to StoreCharacter
	// to see if we can store it
	if (!database.StoreCharacter(GetAccountID(), &pp, &inv)) {
		Log.Out(Logs::Detail, Logs::World_Server,"Character creation failed: %s", pp.name);
		return false;
	}
	Log.Out(Logs::Detail, Logs::World_Server,"Character creation successful: %s", pp.name);
	return true;
}

// returns true if the request is ok, false if there's an error
bool CheckCharCreateInfo(CharCreate_Struct *cc)
{
	if (!cc)
		return false;

	Log.Out(Logs::Detail, Logs::World_Server, "Validating char creation info...");

	RaceClassCombos class_combo;
	bool found = false;
	int combos = character_create_race_class_combos.size();
	for (int i = 0; i < combos; ++i) {
		if (character_create_race_class_combos[i].Class == cc->class_ &&
				character_create_race_class_combos[i].Race == cc->race &&
				character_create_race_class_combos[i].Deity == cc->deity &&
				character_create_race_class_combos[i].Zone == cc->start_zone) {
			class_combo = character_create_race_class_combos[i];
			found = true;
			break;
		}
	}

	if (!found) {
		Log.Out(Logs::Detail, Logs::World_Server, "Could not find class/race/deity/start_zone combination");
		return false;
	}

	uint32 max_stats = 0;
	uint32 allocs = character_create_allocations.size();
	RaceClassAllocation allocation = {0};
	found = false;
	for (int i = 0; i < allocs; ++i) {
		if (character_create_allocations[i].Index == class_combo.AllocationIndex) {
			allocation = character_create_allocations[i];
			found = true;
			break;
		}
	}

	if (!found) {
		Log.Out(Logs::Detail, Logs::World_Server, "Could not find starting stats for selected character combo, cannot verify stats");
		return false;
	}

	max_stats = allocation.DefaultPointAllocation[0] +
		allocation.DefaultPointAllocation[1] +
		allocation.DefaultPointAllocation[2] +
		allocation.DefaultPointAllocation[3] +
		allocation.DefaultPointAllocation[4] +
		allocation.DefaultPointAllocation[5] +
		allocation.DefaultPointAllocation[6];

	if (cc->STR > allocation.BaseStats[0] + max_stats || cc->STR < allocation.BaseStats[0]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Strength out of range");
		return false;
	}

	if (cc->DEX > allocation.BaseStats[1] + max_stats || cc->DEX < allocation.BaseStats[1]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Dexterity out of range");
		return false;
	}

	if (cc->AGI > allocation.BaseStats[2] + max_stats || cc->AGI < allocation.BaseStats[2]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Agility out of range");
		return false;
	}

	if (cc->STA > allocation.BaseStats[3] + max_stats || cc->STA < allocation.BaseStats[3]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Stamina out of range");
		return false;
	}

	if (cc->INT > allocation.BaseStats[4] + max_stats || cc->INT < allocation.BaseStats[4]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Intelligence out of range");
		return false;
	}

	if (cc->WIS > allocation.BaseStats[5] + max_stats || cc->WIS < allocation.BaseStats[5]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Wisdom out of range");
		return false;
	}

	if (cc->CHA > allocation.BaseStats[6] + max_stats || cc->CHA < allocation.BaseStats[6]) {
		Log.Out(Logs::Detail, Logs::World_Server, "Charisma out of range");
		return false;
	}

	uint32 current_stats = 0;
	current_stats += cc->STR - allocation.BaseStats[0];
	current_stats += cc->DEX - allocation.BaseStats[1];
	current_stats += cc->AGI - allocation.BaseStats[2];
	current_stats += cc->STA - allocation.BaseStats[3];
	current_stats += cc->INT - allocation.BaseStats[4];
	current_stats += cc->WIS - allocation.BaseStats[5];
	current_stats += cc->CHA - allocation.BaseStats[6];
	if (current_stats > max_stats) {
		Log.Out(Logs::Detail, Logs::World_Server, "Current Stats > Maximum Stats");
		return false;
	}

	return true;
}

void Client::SetClassStartingSkills(PlayerProfile_Struct *pp)
{
	for (uint32 i = 0; i <= HIGHEST_SKILL; ++i) {
		if (pp->skills[i] == 0) {
			// Skip specialized, tradeskills (fishing excluded), Alcohol Tolerance, and Bind Wound
			if (EQEmu::IsSpecializedSkill((SkillUseTypes)i) ||
					(EQEmu::IsTradeskill((SkillUseTypes)i) && i != SkillFishing) ||
					i == SkillAlcoholTolerance || i == SkillBindWound)
				continue;

			pp->skills[i] = 0;
		}
	}
}

void Client::SetRaceStartingSkills( PlayerProfile_Struct *pp )
{
	switch( pp->race )
	{
	case BARBARIAN:
	case ERUDITE:
	case HALF_ELF:
	case HIGH_ELF:
	case HUMAN:
	case OGRE:
	case TROLL:
		{
			// No Race Specific Skills
			break;
		}
	case DWARF:
		{
			pp->skills[SkillSenseHeading] = 50; //Even if we set this to 0, Intel client sets this to 50 anyway. Confirmed this is correct for era.
			break;
		}
	case DARK_ELF:
		{
			pp->skills[SkillHide] = 50;
			break;
		}
	case FROGLOK:
		{
			pp->skills[SkillSwimming] = 125;
			break;
		}
	case GNOME:
		{
			pp->skills[SkillTinkering] = 50;
			break;
		}
	case HALFLING:
		{
			pp->skills[SkillHide] = 50;
			pp->skills[SkillSneak] = 50;
			break;
		}
	case IKSAR:
		{
			pp->skills[SkillForage] = 50;
			pp->skills[SkillSwimming] = 100;
			break;
		}
	case WOOD_ELF:
		{
			pp->skills[SkillForage] = 50;
			pp->skills[SkillHide] = 50;
			break;
		}
	case VAHSHIR:
		{
			pp->skills[SkillSafeFall] = 50;
			pp->skills[SkillSneak] = 50;
			break;
		}
	}
}

void Client::SetRacialLanguages( PlayerProfile_Struct *pp )
{
	switch( pp->race )
	{
	case BARBARIAN:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_BARBARIAN] = 100;
			break;
		}
	case DARK_ELF:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DARK_ELVISH] = 100;
			pp->languages[LANG_DARK_SPEECH] = 100;
			pp->languages[LANG_ELDER_ELVISH] = 100;
			pp->languages[LANG_ELVISH] = 25;
			break;
		}
	case DWARF:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DWARVISH] = 100;
			pp->languages[LANG_GNOMISH] = 25;
			break;
		}
	case ERUDITE:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_ERUDIAN] = 100;
			break;
		}
	case FROGLOK:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_FROGLOK] = 100;
			pp->languages[LANG_TROLL] = 25;
			break;
		}
	case GNOME:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DWARVISH] = 25;
			pp->languages[LANG_GNOMISH] = 100;
			break;
		}
	case HALF_ELF:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_ELVISH] = 100;
			break;
		}
	case HALFLING:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_HALFLING] = 100;
			break;
		}
	case HIGH_ELF:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DARK_ELVISH] = 25;
			pp->languages[LANG_ELDER_ELVISH] = 25;
			pp->languages[LANG_ELVISH] = 100;
			break;
		}
	case HUMAN:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			break;
		}
	case IKSAR:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DARK_SPEECH] = 100;
			pp->languages[LANG_LIZARDMAN] = 100;
			break;
		}
	case OGRE:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DARK_SPEECH] = 100;
			pp->languages[LANG_OGRE] = 100;
			break;
		}
	case TROLL:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_DARK_SPEECH] = 100;
			pp->languages[LANG_TROLL] = 100;
			break;
		}
	case WOOD_ELF:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_ELVISH] = 100;
			break;
		}
	case VAHSHIR:
		{
			pp->languages[LANG_COMMON_TONGUE] = 100;
			pp->languages[LANG_COMBINE_TONGUE] = 100;
			pp->languages[LANG_ERUDIAN] = 25;
			pp->languages[LANG_VAH_SHIR] = 100;
			break;
		}
	}
}

void Client::SetClassLanguages(PlayerProfile_Struct *pp)
{
	// we only need to handle one class, but custom server might want to do more
	switch(pp->class_) {
	case ROGUE:
		pp->languages[LANG_THIEVES_CANT] = 100;
		break;
	default:
		break;
	}
}

bool Client::GetSessionLimit()
{
	if (RuleI(World, AccountSessionLimit) >= 0 && cle->Admin() < (RuleI(World, ExemptAccountLimitStatus)) && (RuleI(World, ExemptAccountLimitStatus) != -1)) 
	{
		if(database.CheckAccountActive(cle->AccountID()))
		{
			Log.Out(Logs::Detail, Logs::World_Server,"Account %d attempted to login with an active player in the world.", cle->AccountID());
			return true;
		}
		else
			return false;
	}

	return false;
}
