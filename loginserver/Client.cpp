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
#include "Client.h"
#include "ErrorLog.h"
#include "LoginServer.h"
#include "LoginStructures.h"
#include "../common/md5.h"
#include "../common/MiscFunctions.h"
#include "EQCrypto.h"
#include "../common/sha1.h"

extern EQCrypto eq_crypto;
extern ErrorLog *server_log;
extern LoginServer server;

Client::Client(EQStreamInterface *c, ClientVersion v)
{
	connection = c;
	version = v;
	status = cs_not_sent_session_ready;
	account_id = 0;
	play_server_id = 0;
	play_sequence_id = 0;
}

bool Client::Process()
{
	EQApplicationPacket *app = connection->PopPacket();
	while(app)
	{
		if(server.options.IsTraceOn())
		{
			server_log->Log(log_network, "Application packet received from client (size %u)", app->Size());
		}

		if(server.options.IsDumpInPacketsOn())
		{
			DumpPacket(app);
		}

		switch(app->GetOpcode())
		{
		case OP_SessionReady:
			{
				if(server.options.IsTraceOn())
				{
					server_log->Log(log_network, "Session ready received from client.");
				}
				Handle_SessionReady((const char*)app->pBuffer, app->Size());
				break;
			}

		case OP_SessionLogin:
			{
				if(server.options.IsTraceOn())
				{
					server_log->Log(log_network, "Session ready received from client.");
				}
				Handle_SessionLogin((const char*)app->pBuffer, app->Size());
				break;
			}
		case OP_Login:
			{
				if(app->Size() < 20)
				{
					server_log->Log(log_network_error, "Login received but it is too small, discarding.");
					break;
				}

				if(server.options.IsTraceOn())
				{
					server_log->Log(log_network, "Login received from client.");
				}

				if(version != cv_old)
					Handle_Login((const char*)app->pBuffer, app->Size());
				else
					Handle_OldLogin((const char*)app->pBuffer, app->Size());
				break;
			}
		case OP_LoginComplete:
			{
				Handle_LoginComplete((const char*)app->pBuffer, app->Size());
				break;
			}
		case OP_LoginUnknown1: //Seems to be related to world status in older clients; we use our own logic for that though.
			{
				EQApplicationPacket *outapp = new EQApplicationPacket(OP_LoginUnknown2, 0);
				connection->QueuePacket(outapp);
				break;
			}
		case OP_ServerListRequest:
			{
				if(server.options.IsTraceOn())
				{
					server_log->Log(log_network, "Server list request received from client.");
				}

				SendServerListPacket();
				break;
			}
		case OP_PlayEverquestRequest:
			{
				if(app->Size() < sizeof(PlayEverquestRequest_Struct) && version != cv_old)
				{
					server_log->Log(log_network_error, "Play received but it is too small, discarding.");
					break;
				}

				Handle_Play((const char*)app->pBuffer);
				break;
			}
		default:
			{
				char dump[64];
				app->build_header_dump(dump);
				server_log->Log(log_network_error, "Recieved unhandled application packet from the client: %s.", dump);
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

	if(version != cv_old)
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

	/**
	* The packets are mostly the same but slightly different between the two versions.
	*/
	if(version == cv_sod)
	{
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_ChatMessage, 17);
		outapp->pBuffer[0] = 0x02;
		outapp->pBuffer[10] = 0x01;
		outapp->pBuffer[11] = 0x65;

		if(server.options.IsDumpOutPacketsOn())
		{
			DumpPacket(outapp);
		}

		connection->QueuePacket(outapp);
		delete outapp;
	}
	else if (version == cv_titanium)
	{
		const char *msg = "ChatMessage";
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_ChatMessage, 16 + strlen(msg));
		outapp->pBuffer[0] = 0x02;
		outapp->pBuffer[10] = 0x01;
		outapp->pBuffer[11] = 0x65;
		strcpy((char*)(outapp->pBuffer + 15), msg);

		if(server.options.IsDumpOutPacketsOn())
		{
			DumpPacket(outapp);
		}

		connection->QueuePacket(outapp);
		delete outapp;
	}
	else if(version == cv_old)
	{
		//Special logic for old streams.
		char buf[20];
		strcpy(buf, "12-4-2002 1800");
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_SessionReady, strlen(buf) + 1);
		strcpy((char*) outapp->pBuffer, buf);
		connection->QueuePacket(outapp);
		delete outapp;
	}
}


void Client::Handle_SessionLogin(const char* data, unsigned int size)
{

	if(version != cv_old)
	{
		//Not old client, gtfo haxxor!
		return;
	}
	
	status = cs_logged_in;

	string ourdata = data;
	if(size < strlen("eqworld-52.989studios.com") + 1)
		return;

	//Get rid of that 989 studios part of the string, plus remove null term zero.
	string userpass = ourdata.substr(0, ourdata.find("eqworld-52.989studios.com") - 1);

	string username = userpass.substr(0, userpass.find("/"));
	string password = userpass.substr(userpass.find("/") + 1);
	string salt = server.options.GetSalt();

	server_log->Log(log_network, "Username: %s", username.c_str());
	server_log->Log(log_network, "Password: %s", password.c_str());

	unsigned int d_account_id = 0;
	in_addr in;
	string d_pass_hash;
	uchar sha1pass[40];
	char sha1hash[41];
	in.s_addr = connection->GetRemoteIP();
	bool result = false;

	string userandpass = username + password + salt;
	if(server.db->GetLoginDataFromAccountName(username, d_pass_hash, d_account_id) == false)
	{
		server_log->Log(log_client_error, "Error logging in, user %s does not exist in the database.", username.c_str());
		
		if (server.options.IsLoginFailsOn() && !server.options.IsCreateOn())
		{
			server.db->UpdateAccessLog(d_account_id, username, string(inet_ntoa(in)), time(nullptr), "Account not exist, Mac");
		}
		if (server.options.IsCreateOn())
		{
			if (server.options.IsLoginFailsOn())
			{
				server.db->UpdateAccessLog(d_account_id, username, string(inet_ntoa(in)), time(nullptr), "Account created, Mac");
			}
			/*eventually add a unix time stamp calculator from last id that matches IP
			to limit account creations per time specified by an interval set in the ini.*/
			server.db->UpdateLSAccountInfo(NULL, username, userandpass, "", 2, string(inet_ntoa(in)));
			FatalError("Account did not exist so it was created. Hit connect again to login.");

			return;
		}
		result = false;
	}
	else
	{
		sha1::calc(userandpass.c_str(), userandpass.length(), sha1pass);
		sha1::toHexString(sha1pass,sha1hash);
		if(d_pass_hash.compare((char*)sha1hash) == 0)
		{
			result = true;
		}
		else
		{
			if (server.options.IsLoginFailsOn())
			{
				server.db->UpdateAccessLog(d_account_id, username, string(inet_ntoa(in)), time(nullptr), "Mac bad password");
			}
			server_log->Log(log_client_error, "%s", sha1hash);
			result = false;
		}
	}

	if(result)
	{
		server.db->UpdateLSAccountData(d_account_id, string(inet_ntoa(in)));
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

		string buf = server.options.GetNetworkIP();
		EQApplicationPacket *outapp2 = new EQApplicationPacket(OP_ServerName, buf.length() + 1);
		strncpy((char*)outapp2->pBuffer, buf.c_str(), buf.length() + 1);
		connection->QueuePacket(outapp2);
		delete outapp2;

	}
	else
	{
		FatalError("Invalid username or password.");
	}

	return;

}

void Client::Handle_Login(const char* data, unsigned int size)
{
	//Login method not supported in PEQMac.
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

void Client::Handle_OldLogin(const char* data, unsigned int size)
{
	if(status != cs_waiting_for_login)
	{
		server_log->Log(log_network_error, "Login received after already having logged in.");
		return;
	}

	if (size < sizeof(LoginServerInfo_Struct)) {
		return;
	}

	status = cs_logged_in;

	string e_hash;
	char *e_buffer = nullptr;
	unsigned int d_account_id = 0;
	string d_pass_hash;
	uchar eqlogin[40];
	uchar sha1pass[40];
	char sha1hash[41];
	eq_crypto.DoEQDecrypt((unsigned char*)data, eqlogin, 40);
	LoginCrypt_struct* lcs = (LoginCrypt_struct*)eqlogin;

	bool result;
	in_addr in;
	in.s_addr = connection->GetRemoteIP();

	string username = lcs->username;
	string password = lcs->password;
	string salt = server.options.GetSalt();

	string userandpass = password + salt;

	if(server.db->GetLoginDataFromAccountName(username, d_pass_hash, d_account_id) == false)
	{
		server_log->Log(log_client_error, "Error logging in, user %s does not exist in the database.", username.c_str());
		
		if (server.options.IsLoginFailsOn() && !server.options.IsCreateOn())
		{
			server.db->UpdateAccessLog(d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "Account not exist, Mac");
		}
		if (server.options.IsCreateOn())
		{
			if (server.options.IsLoginFailsOn())
			{
				server.db->UpdateAccessLog(d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "Account created, Mac");
			}
			/*eventually add a unix time stamp calculator from last id that matches IP
			to limit account creations per time specified by an interval set in the ini.*/
			server.db->UpdateLSAccountInfo(NULL, username.c_str(), userandpass.c_str(), "", 2, string(inet_ntoa(in)));
			FatalError("Account did not exist so it was created. Hit connect again to login.");

			return;
		}
		result = false;
	}
	else
	{
		sha1::calc(userandpass.c_str(), userandpass.length(), sha1pass);
		sha1::toHexString(sha1pass,sha1hash);
		if(d_pass_hash.compare((char*)sha1hash) == 0)
		{
			result = true;
		}
		else
		{
			if (server.options.IsLoginFailsOn())
			{
				server.db->UpdateAccessLog(d_account_id, username.c_str(), string(inet_ntoa(in)), time(nullptr), "Mac bad password");
			}
			server_log->Log(log_client_error, "%s", sha1hash);
			result = false;
		}
	}

	if(result)
	{
		server.db->UpdateLSAccountData(d_account_id, string(inet_ntoa(in)));
		GenerateKey();
		account_id = d_account_id;
		account_name = username;
		EQApplicationPacket *outapp = new EQApplicationPacket(OP_LoginAccepted, sizeof(SessionId_Struct));
		SessionId_Struct* s_id = (SessionId_Struct*)outapp->pBuffer;
		// this is submitted to world server as "username"
		sprintf(s_id->session_id, "LS#%i", account_id);
		strcpy(s_id->unused, "unused");
		s_id->unknown = 4;
		connection->QueuePacket(outapp);
		delete outapp;
	}
	else
	{
		FatalError("Invalid username or password.");
	}
}

void Client::Handle_Play(const char* data)
{
	if(status != cs_logged_in)
	{
		server_log->Log(log_client_error, "Client sent a play request when they either were not logged in, discarding.");
		return;
	}

	if(version == cv_old)
	{
		if(data)
		{
		server.SM->SendOldUserToWorldRequest(data, account_id);
		}
	}
	else
	{
		const PlayEverquestRequest_Struct *play = (const PlayEverquestRequest_Struct*)data;
		unsigned int server_id_in = (unsigned int)play->ServerNumber;
		unsigned int sequence_in = (unsigned int)play->Sequence;

		if(server.options.IsTraceOn())
		{
			server_log->Log(log_network, "Play received from client, server number %u sequence %u.", server_id_in, sequence_in);
		}

		this->play_server_id = (unsigned int)play->ServerNumber;
		play_sequence_id = sequence_in;
		play_server_id = server_id_in;
		server.SM->SendUserToWorldRequest(server_id_in, account_id);
	}
}

void Client::SendServerListPacket()
{
	if(version == cv_old)
	{
		EQApplicationPacket *outapp = server.SM->CreateOldServerListPacket(this);

		if(server.options.IsDumpOutPacketsOn())
		{
			DumpPacket(outapp);
		}

		connection->QueuePacket(outapp);
		delete outapp;
	}
	else
	{
		EQApplicationPacket *outapp = server.SM->CreateServerListPacket(this);

		if(server.options.IsDumpOutPacketsOn())
		{
			DumpPacket(outapp);
		}

		connection->QueuePacket(outapp);
		delete outapp;
	}
}

void Client::SendPlayResponse(EQApplicationPacket *outapp)
{
	if(server.options.IsTraceOn())
	{
		server_log->Log(log_network_trace, "Sending play response for %s.", GetAccountName().c_str());
		server_log->LogPacket(log_network_trace, (const char*)outapp->pBuffer, outapp->size);
	}
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

		key.append((const char*)&key_selection[MakeRandomInt(0, 35)], 1);
		count++;
	}
}

