/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2010 EQEMu Development Team (http://eqemulator.net)

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
#include "client.h"
#include "error_log.h"
#include "login_server.h"
#include "login_structures.h"
#include "../common/md5.h"
#include "../common/misc_functions.h"
#include "EQCrypto.h"
#include "../common/sha1.h"

#pragma warning(disable : 4267 4244 4996 4101 4700)

extern EQCrypto eq_crypto;
extern ErrorLog *server_log;
extern LoginServer server;
extern Database db;
extern Saltme mysalt;

Client::Client(EQStreamInterface *c, ClientVersion v)
{
	connection = c;
	version = v;
	status = cs_not_sent_session_ready;
	account_id = 0;
	sentsessioninfo = false;
	play_server_id = 0;
	play_sequence_id = 0;
}

bool Client::Process()
{
	EQApplicationPacket *app = connection->PopPacket();
	while(app)
	{
		server_log->Trace("Application packet received from client.");
		if (server_log->DumpIn())
		{
			DumpPacket(app);
		}

		switch(app->GetOpcode())
		{
		case OP_SessionReady:
			{
				server_log->Trace("Session ready received from client.");
				Handle_SessionReady((const char*)app->pBuffer, app->Size());
				break;
			}
		case OP_LoginOSX:
			{
				server_log->Trace("Login received from OSX client.");
				Handle_Login((const char*)app->pBuffer, app->Size(), "OSX");
				break;
			}
		case OP_LoginPC:
			{
				if(app->Size() < 20)
				{
					server_log->Log(log_network_error, "Login received but it is too small, discarding.");
					break;
				}
				server_log->Trace("Login received from PC client.");
				Handle_Login((const char*)app->pBuffer, app->Size(), "PC");
				break;
			}
		case OP_LoginComplete:
			{
				server_log->Trace("Login complete received from client.");
				Handle_LoginComplete((const char*)app->pBuffer, app->Size());
				break;
			}
		case OP_LoginUnknown1: //Seems to be related to world status in older clients; we use our own logic for that though.
			{
				server_log->Trace("OP_LoginUnknown1 received from client.");
				EQApplicationPacket *outapp = new EQApplicationPacket(OP_LoginUnknown2, 0);
				connection->QueuePacket(outapp);
				break;
			}
		case OP_ServerListRequest:
			{
				server_log->Trace("Server list request received from client.");
				SendServerListPacket();
				break;
			}
		case OP_PlayEverquestRequest:
			{
				server_log->Log(log_debug, "OP_PlayEverquestRequest started...");
				if (app->Size() < sizeof(PlayEverquestRequest_Struct) && (version != cv_old && version != cv_tri))
				{
					server_log->Log(log_network_error, "Play received but it is too small, discarding.");
					break;
				}
				Handle_Play((const char*)app->pBuffer);
				break;
			}
		case OP_LoginBanner:
			{
				Handle_Banner(app->Size());
				break;
			}
		default:
			{
				char dump[64];
				app->build_header_dump(dump);
				server_log->Log(log_network_error, "Received unhandled application packet from the client: %s.", dump);
			}
		}
		delete app;
		app = connection->PopPacket();
	}
	return true;
}

void Client::Handle_SessionReady(const char* data, unsigned int size)
{
	if(status != cs_not_sent_session_ready)
	{
		server_log->Log(log_network_error, "Session ready received again after already being received.");
		return;
	}

	if (version != cv_old && version != cv_tri)
	{
		if(size < sizeof(unsigned int))
		{
			server_log->Log(log_network_error, "Session ready was too small.");
			return;
		}

		unsigned int mode = *((unsigned int*)data);
		if(mode == (unsigned int)lm_from_world)
		{
			server_log->Log(log_network, "Session ready indicated logged in from world(unsupported feature), disconnecting.");
			connection->Close();
			return;
		}
	}

	status = cs_waiting_for_login;

	server_log->Log(log_debug, "Checking client version date...");

	if (version == cv_old)
	{
		//Special logic for old streams.
		server_log->Log(log_debug, "Client has cv_old version flag...");
		char buf[20];
		strcpy(buf, "12-4-2002 1800");
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_SessionReady, strlen(buf) + 1);
		strcpy((char*) outapp->pBuffer, buf);
		connection->QueuePacket(outapp);
		delete outapp;
	}
	else if (version == cv_tri)
	{
		//Special logic for trilogy streams.
		server_log->Log(log_debug, "Client has cv_tri version flag...");
		char buf[20];
		strcpy(buf, "8-09-2001 14:25");
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_SessionReady, strlen(buf) + 1);
		strcpy((char*)outapp->pBuffer, buf);
		connection->QueuePacket(outapp);
		delete outapp;
	}
}

void Client::Handle_Login(const char* data, unsigned int size, string client)
{
	in_addr in;
	in.s_addr = connection->GetRemoteIP();

	server_log->Log(log_debug, "Handle_Login started...");

	if (version != cv_old && version != cv_tri)
	{
		//Not old client, gtfo haxxor!
		string error = "Unauthorized client from " + string(inet_ntoa(in)) + " , exiting them.";
		server_log->Log(log_network_error, error.c_str());
		return;
	}
	else if (status != cs_waiting_for_login)
	{
		server_log->Log(log_network_error, "Login received after already having logged in.");
		return;
	}

	else if (size < sizeof(LoginServerInfo_Struct))
	{
		server_log->Log(log_network_error, "Bad Login Struct size.");
		return;
	}

	string username;
	string password;
	string platform;
	int created;

	if (client == "OSX")
	{
		server_log->Log(log_debug, "System is OSX...");
		string ourdata = data;
		if (size < strlen("eqworld-52.989studios.com") + 1)
			return;

		//Get rid of that 989 studios part of the string, plus remove null term zero.
		string userpass = ourdata.substr(0, ourdata.find("eqworld-52.989studios.com") - 1);

		username = userpass.substr(0, userpass.find("/"));
		password = userpass.substr(userpass.find("/") + 1);
		platform = "OSX";
		macversion = intel;
		created = 2;
	}
	else if (client == "PC")
	{
		server_log->Log(log_debug, "System is PC...");
		string e_hash;
		char *e_buffer = nullptr;
		uchar eqlogin[40];
		eq_crypto.DoEQDecrypt((unsigned char*)data, eqlogin, 40);
		LoginCrypt_struct* lcs = (LoginCrypt_struct*)eqlogin;
		username = lcs->username;
		password = lcs->password;
		platform = "PC";
		macversion = pc;
		created = 1;
	}

	string userandpass = mysalt.Salt(password);
	status = cs_logged_in;
	unsigned int d_account_id = 0;
	string d_pass_hash;
	bool result = false;
	uchar sha1pass[40];
	char sha1hash[41];


	if (db.GetLoginDataFromAccountName(username, d_pass_hash, d_account_id) == false)
	{
		server_log->Log(log_client_error, "Error logging in, user %s does not exist in the database.", username.c_str());

		Logs(platform, d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "notexist");

		if (db.LoadServerSettings("options", "auto_account_create") == "TRUE")
		{
			Logs(platform, d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "created");
			db.CreateLSAccount(username.c_str(), userandpass.c_str(), "", created, string(inet_ntoa(in)), string(inet_ntoa(in)));
			if (db.LoadServerSettings("options", "auto_account_activate") == "TRUE")
			{
				FatalError("Account did not exist so it was created.\nHit connect again to login.");
			}
			else
			{
				FatalError("Account did not exist so it was created.\nBut, Account is not activated.\nServer is not allowing open logins at this time.\nContact server management.");
			}
			return;
		}
		else if (db.LoadServerSettings("options", "auto_account_create") == "FALSE")
		{
			FatalError("Account does not exist and auto creation is not enabled.");
			return;
		}
		result = false;
	}
	else
	{
		sha1::calc(userandpass.c_str(), userandpass.length(), sha1pass);
		sha1::toHexString(sha1pass, sha1hash);
		if (d_pass_hash.compare((char*)sha1hash) == 0)
		{
			result = true;
		}
		else
		{
			Logs(platform, d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "badpass");
			server_log->Log(log_client_error, "%s", sha1hash);
			result = false;
		}
	}

	if (result)
	{
		if (!sentsessioninfo)
		{
			if (db.GetAccountLockStatus(username) == false)
			{
				FatalError("Account is not activated.\nServer is not allowing open logins at this time.\nContact server management.");
				return;
			}
			Logs(platform, d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "success");
			db.UpdateLSAccount(d_account_id, string(inet_ntoa(in)));
			GenerateKey();
			account_id = d_account_id;
			account_name = username.c_str();
			EQApplicationPacket *outapp = new EQApplicationPacket(OP_LoginAccepted, sizeof(SessionId_Struct));
			SessionId_Struct* s_id = (SessionId_Struct*)outapp->pBuffer;
			// this is submitted to world server as "username"
			sprintf(s_id->session_id, "LS#%i", account_id);
			strcpy(s_id->unused, "unused");
			s_id->unknown = 4;
			connection->QueuePacket(outapp);
			delete outapp;

			if (client == "OSX")
			{
				string buf = db.LoadServerSettings("options", "network_ip").c_str();
				EQApplicationPacket *outapp2 = new EQApplicationPacket(OP_ServerName, buf.length() + 1);
				strncpy((char*)outapp2->pBuffer, buf.c_str(), buf.length() + 1);
				connection->QueuePacket(outapp2);
				delete outapp2;
				sentsessioninfo = true;
			}
		}
	}
	else
	{
		if (client == "PC")
		{
			FatalError("Invalid username or password.");
		}
	}
	return;
}

void Client::FatalError(const char* message) {
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_ClientError, strlen(message) + 1);
	if (strlen(message) > 1) {
		strcpy((char*)outapp->pBuffer, message);
	}
	connection->QueuePacket(outapp);
	delete outapp;
	return;
}

void Client::Handle_LoginComplete(const char* data, unsigned int size) {
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_LoginComplete, 20);
	outapp->pBuffer[0] = 1;
	connection->QueuePacket(outapp);
	delete outapp;
	return;
}

void Client::Handle_Play(const char* data)
{
	server_log->Log(log_debug, "Handle_Play starting...");
	if(status != cs_logged_in)
	{
		server_log->Log(log_client_error, "Client sent a play request when they either were not logged in, discarding.");
		return;
	}
	if(data)
	{
		server.SM->SendOldUserToWorldRequest(data, account_id, connection->GetRemoteIP());
	}
}

void Client::SendServerListPacket()
{
	EQApplicationPacket *outapp = server.SM->CreateOldServerListPacket(this);


	if (server_log->DumpOut())
	{
		DumpPacket(outapp);
	}

	connection->QueuePacket(outapp);
	delete outapp;
}

void Client::Handle_Banner(unsigned int size)
{
	std::string ticker = "Welcome to EQMacEmu";
	if (db.CheckExtraSettings("ticker"))
	{
		ticker = db.LoadServerSettings("options", "ticker");
	}

	EQApplicationPacket *outapp = new EQApplicationPacket(OP_LoginBanner, 5);
	outapp->size += strlen(ticker.c_str());
	outapp->pBuffer = new uchar[outapp->size + 3];
	outapp->pBuffer[0] = 1;
	outapp->pBuffer[1] = 0;
	outapp->pBuffer[2] = 0;
	outapp->pBuffer[3] = 0;
	strcpy((char *)&outapp->pBuffer[4], ticker.c_str());
	connection->QueuePacket(outapp);
	delete outapp;
}

void Client::SendPlayResponse(EQApplicationPacket *outapp)
{
	server_log->Trace("Sending play response to client.");
	server_log->TracePacket((const char*)outapp->pBuffer, outapp->size);

	connection->QueuePacket(outapp);
	status = cs_logged_in;
}

void Client::GenerateKey()
{
	key.clear();
	int count = 0;
	while(count < 10)
	{
		static const char key_selection[] =
		{
			'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
			'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
			'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
			'Y', 'Z', '0', '1', '2', '3', '4', '5',
			'6', '7', '8', '9'
		};

		key.append((const char*)&key_selection[random.Int(0, 35)], 1);
		count++;
	}
}

void Client::Logs(std::string platform, unsigned int account_id, std::string account_name, std::string IP, unsigned int accessed, std::string reason)
{
	// valid reason codes are: notexist, created, badpass, success
	if (db.LoadServerSettings("options", "failed_login_log") == "TRUE" && db.LoadServerSettings("options", "auto_account_create") == "FALSE" && reason == "notexist")
	{
		db.UpdateAccessLog(account_id, account_name, IP, accessed, "Account not exist, " + platform);
	}
	if (db.LoadServerSettings("options", "failed_login_log") == "TRUE" && db.LoadServerSettings("options", "auto_account_create") == "TRUE" && reason == "created")
	{
		db.UpdateAccessLog(account_id, account_name, IP, accessed, "Account created, " + platform);
	}
	if (db.LoadServerSettings("options", "failed_login_log") == "TRUE" && reason == "badpass")
	{
		db.UpdateAccessLog(account_id, account_name, IP, accessed, "Bad password, " + platform);
	}
	if (db.LoadServerSettings("options", "good_loginIP_log") == "TRUE" && reason == "success")
	{
		db.UpdateAccessLog(account_id, account_name, IP, accessed, "Logged in Success, " + platform);
	}
}
