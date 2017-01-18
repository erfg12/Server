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
#include "world_server.h"
#include "error_log.h"
#include "login_server.h"
#include "login_structures.h"

#pragma warning( disable : 4996 )

extern ErrorLog *server_log;
extern LoginServer server;
extern Database db;

WorldServer::WorldServer(EmuTCPConnection *c)
{
	connection = c;
	zones_booted = 0;
	players_online = 0;
	status = 0;
	runtime_id = 0;
	server_list_type = 0;
	server_type = 0;
	authorized = false;
	trusted = false;
	logged_in = false;
}

WorldServer::~WorldServer()
{
	if(connection)
	{
		connection->Free();
	}
}

void WorldServer::Reset()
{
	zones_booted = 0;
	players_online = 0;
	status = 0;
	runtime_id;
	server_list_type = 0;
	server_type = 0;
	authorized = false;
	logged_in = false;
}

bool WorldServer::Process()
{
	ServerPacket *app = nullptr;
	while(app = connection->PopPacket())
	{
		server_log->WorldTrace("Application packet received from server: 0x%.4X, (size %u)", app->opcode, app->size);

		if (server_log->DumpIn())
		{
			DumpPacket(app);
		}

		switch(app->opcode)
		{
		case ServerOP_NewLSInfo:
			{
				if(app->size < sizeof(ServerNewLSInfo_Struct))
				{
					server_log->Log(log_network_error, "Received application packet from server that had opcode ServerOP_NewLSInfo, "
						"but was too small. Discarded to avoid buffer overrun.");
					break;
				}

				server_log->WorldTrace("New Login Info Received.");
				ServerNewLSInfo_Struct *info = (ServerNewLSInfo_Struct*)app->pBuffer;
				Handle_NewLSInfo(info);
				break;
			}
		case ServerOP_LSStatus:
			{
				if(app->size < sizeof(ServerLSStatus_Struct))
				{
					server_log->Log(log_network_error, "Received application packet from server that had opcode ServerOP_LSStatus, "
						"but was too small. Discarded to avoid buffer overrun.");
					break;
				}

				server_log->WorldTrace("World Server Status Received.");

				ServerLSStatus_Struct *ls_status = (ServerLSStatus_Struct*)app->pBuffer;
				Handle_LSStatus(ls_status);
				break;
			}
		case ServerOP_LSZoneInfo:
		case ServerOP_LSZoneShutdown:
		case ServerOP_LSZoneStart:
		case ServerOP_LSZoneBoot:
		case ServerOP_LSZoneSleep:
		case ServerOP_LSPlayerLeftWorld:
		case ServerOP_LSPlayerJoinWorld:
		case ServerOP_LSPlayerZoneChange:
			{
				//Not logging these to cut down on spam until we implement them
				break;
			}

		case ServerOP_UsertoWorldResp:
			{
				if(app->size < sizeof(UsertoWorldResponse_Struct))
				{
					server_log->Log(log_network_error, "Received application packet from server that had opcode ServerOP_UsertoWorldResp, "
						"but was too small. Discarded to avoid buffer overrun.");
					break;
				}

				//I don't use world trace for this and here is why:
				//Because this is a part of the client login procedure it makes tracking client errors
				//While keeping world server spam with multiple servers connected almost impossible.
				server_log->Trace("User-To-World Response received.");

				UsertoWorldResponse_Struct *utwr = (UsertoWorldResponse_Struct*)app->pBuffer;
				server_log->Log(log_client, "Trying to find client with user id of %u.", utwr->lsaccountid);
				Client *c = server.CM->GetClient(utwr->lsaccountid);
				if (c && (c->GetClientVersion() == cv_old || c->GetClientVersion() == cv_tri))
				{
					if(utwr->response > 0)
					{
						SendClientAuth(c->GetConnection()->GetRemoteIP(), c->GetAccountName(), c->GetKey(), c->GetAccountID(), c->GetMacClientVersion());
					}

					switch(utwr->response)
					{
						case 1:
							break;
						case 0:
							c->FatalError("\nError 1020: Your chosen World Server is DOWN.\n\nPlease select another.");
							break;
						case -1:
							c->FatalError("You have been suspended from the worldserver.");
							break;
						case -2:
							c->FatalError("You have been banned from the worldserver.");
							break;
						case -3:
							c->FatalError("That server is full.");
							break;
						case -4:
							c->FatalError("Error 1018: You currently have an active character on that EverQuest Server, please allow a minute for synchronization and try again.");
							break;
						case -5:
							c->FatalError("Error IP Limit Exceeded: \n\nYou have exceeded the maximum number of allowed IP addresses for this account.");
							break;
					}
					server_log->Log(log_client, "Found client with user id of %u and account name of %s.", utwr->lsaccountid, c->GetAccountName().c_str());
					EQApplicationPacket *outapp = new EQApplicationPacket(OP_PlayEverquestRequest, 17);
					strncpy((char*) &outapp->pBuffer[1], c->GetKey().c_str(), c->GetKey().size());

					c->SendPlayResponse(outapp);
				}
				else if(c)
				{
					server_log->Log(log_client, "Found client with user id of %u and account name of %s.", utwr->lsaccountid, c->GetAccountName().c_str());
					EQApplicationPacket *outapp = new EQApplicationPacket(OP_PlayEverquestResponse, sizeof(PlayEverquestResponse_Struct));
					PlayEverquestResponse_Struct *per = (PlayEverquestResponse_Struct*)outapp->pBuffer;
					per->Sequence = c->GetPlaySequence();
					per->ServerNumber = c->GetPlayServerID();
					server_log->Log(log_client, "Found sequence and play of %u %u", c->GetPlaySequence(), c->GetPlayServerID());
					server_log->LogPacket(log_network_trace, (const char*)outapp->pBuffer, outapp->size);

					if (utwr->response > 0)
					{
						per->Allowed = 1;
						SendClientAuth(c->GetConnection()->GetRemoteIP(), c->GetAccountName(), c->GetKey(), c->GetAccountID());
					}

					switch (utwr->response)
					{
					case 1:
						per->Message = 101;
						break;
					case 0:
						per->Message = 326;
						break;
					case -1:
						per->Message = 337;
						break;
					case -2:
						per->Message = 338;
						break;
					case -3:
						per->Message = 303;
						break;
					case -4:
						per->Message = 111;
						break;
					case -5:
						per->Message = 198;
						break;
					}

					server_log->Trace("Sending play response to client.");
					server_log->TracePacket((const char*)outapp->pBuffer, outapp->size);

					if (server_log->DumpOut())
					{
						DumpPacket(outapp);
					}
					c->SendPlayResponse(outapp);
					delete outapp;
				}
				else
				{
					server_log->Log(log_client_error, "Received User-To-World Response for %u but could not find the client referenced!.", utwr->lsaccountid);
				}
				break;
			}
		case ServerOP_LSAccountUpdate:
			{
				if(app->size < sizeof(ServerLSAccountUpdate_Struct))
				{
					server_log->Log(log_network_error, "Received application packet from server that had opcode ServerLSAccountUpdate_Struct, "
						"but was too small. Discarded to avoid buffer overrun.");
					break;
				}
			
				server_log->Log(log_network_trace, "ServerOP_LSAccountUpdate packet received from: %s", short_name.c_str());
				ServerLSAccountUpdate_Struct *lsau = (ServerLSAccountUpdate_Struct*)app->pBuffer;
				if(trusted)
				{
					server_log->Log(log_network_trace, "ServerOP_LSAccountUpdate update processed for: %s", lsau->useraccount);
					string name;
					string password;
					string email;
					name.assign(lsau->useraccount);
					password.assign(lsau->userpassword);
					email.assign(lsau->useremail);
					db.CreateLSAccount(name, password, email, 0, "", "");
				}
				break;
			}
		default:
			{
				server_log->Log(log_network_error, "Received application packet from server that had an unknown operation code 0x%.4X.", app->opcode);
			}
		}
		delete app;
		app = nullptr;
	}
	return true;
}

void WorldServer::Handle_NewLSInfo(ServerNewLSInfo_Struct* i)
{
	if(logged_in)
	{
		server_log->Log(log_network_error, "WorldServer::Handle_NewLSInfo called but the login server was already marked as logged in, aborting.");
		return;
	}

	if(strlen(i->account) <= 30)
	{
		account_name = i->account;
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, account name was too long.");
		return;
	}

	if(strlen(i->password) <= 30)
	{
		account_password = i->password;
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, account password was too long.");
		return;
	}

	if(strlen(i->name) <= 200)
	{
		long_name = i->name;
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, long name was too long.");
		return;
	}

	if(strlen(i->shortname) <= 50)
	{
		short_name = i->shortname;
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, short name was too long.");
		return;
	}

	if(strlen(i->local_address) <= 125)
	{
		if(strlen(i->local_address) == 0)
		{
			server_log->Log(log_network_error, "Handle_NewLSInfo error, local address was null, defaulting to localhost");
			local_ip = "127.0.0.1";
		}
		else
		{
			local_ip = i->local_address;
		}
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, local address was too long.");
		return;
	}

	if(strlen(i->remote_address) <= 125)
	{
		if(strlen(i->remote_address) == 0)
		{
			in_addr in;
			in.s_addr = GetConnection()->GetrIP();
			remote_ip = inet_ntoa(in);
			server_log->Log(log_network_error, "Handle_NewLSInfo error, remote address was null, defaulting to stream address %s.", remote_ip.c_str());
		}
		else
		{
			remote_ip = i->remote_address;
		}
	}
	else
	{
		in_addr in;
		in.s_addr = GetConnection()->GetrIP();
		remote_ip = inet_ntoa(in);
		server_log->Log(log_network_error, "Handle_NewLSInfo error, remote address was too long, defaulting to stream address %s.", remote_ip.c_str());
	}

	if(strlen(i->serverversion) <= 64)
	{
		version = i->serverversion;
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, server version was too long.");
		return;
	}

	if(strlen(i->protocolversion) <= 25)
	{
		protocol = i->protocolversion;
	}
	else
	{
		server_log->Log(log_network_error, "Handle_NewLSInfo error, protocol version was too long.");
		return;
	}

	server_type = i->servertype;
	logged_in = true;

	if (db.LoadServerSettings("options", "reject_duplicate_servers") == "TRUE")
	{
		if(server.SM->ServerExists(long_name, short_name, this))
		{
			server_log->Log(log_world_error, "World tried to login but there already exists a server that has that name.");
			return;
		}
	}
	else
	{
		if(server.SM->ServerExists(long_name, short_name, this))
		{
			server_log->Log(log_world_error, "World tried to login but there already exists a server that has that name.");
			server.SM->DestroyServerByName(long_name, short_name, this);
		}
	}

	if (db.LoadServerSettings("options", "unregistered_allowed") == "FALSE")
	{
		if(account_name.size() > 0 && account_password.size() > 0)
		{
			unsigned int s_id = 0;
			unsigned int s_list_type = 0;
			unsigned int s_trusted = 0;
			string s_desc;
			string s_list_desc;
			string s_acct_name;
			string s_acct_pass;
			if(db.GetWorldRegistration(s_id, s_desc, s_trusted, s_list_type, s_acct_name, s_acct_pass, long_name, short_name))
			{
				if(s_acct_name.size() == 0 || s_acct_pass.size() == 0)
				{
					server_log->Log(log_world, "Server %s(%s) successfully logged into account that had no user/password requirement.",
						long_name.c_str(), short_name.c_str());
					authorized = true;
					SetRuntimeID(s_id);
					server_list_type = s_list_type;
					desc = s_desc;
				}
				else if(s_acct_name.compare(account_name) == 0 && s_acct_pass.compare(account_password) == 0)
				{
					server_log->Log(log_world, "Server %s(%s) successfully logged in.",
						long_name.c_str(), short_name.c_str());
					authorized = true;
					SetRuntimeID(s_id);
					server_list_type = s_list_type;
					desc = s_desc;
					if(s_trusted)
					{
						server_log->Log(log_network_trace, "ServerOP_LSAccountUpdate sent to world");
						trusted = true;
						ServerPacket *outapp = new ServerPacket(ServerOP_LSAccountUpdate, 0);
						connection->SendPacket(outapp);
					}
				}
				else
				{
					server_log->Log(log_world, "Server %s(%s) attempted to log in but account and password did not match the entry in the database, and only"
						" registered servers are allowed.", long_name.c_str(), short_name.c_str());
					return;
				}
			}
			else
			{
				server_log->Log(log_world, "Server %s(%s) attempted to log in but database couldn't find an entry and only registered servers are allowed.",
					long_name.c_str(), short_name.c_str());
				return;
			}
		}
		else
		{
			server_log->Log(log_world, "Server %s(%s) did not attempt to log in but only registered servers are allowed.",
				long_name.c_str(), short_name.c_str());
			return;
		}
	}
	else
	{
		unsigned int s_id = 0;
		unsigned int s_list_type = 0;
		unsigned int s_trusted = 0;
		string s_desc;
		string s_list_desc;
		string s_acct_name;
		string s_acct_pass;

		if (db.GetWorldRegistration(s_id, s_desc, s_trusted, s_list_type, s_acct_name, s_acct_pass, long_name, short_name))
		{
			if(account_name.size() > 0 && account_password.size() > 0)
			{
				if(s_acct_name.compare(account_name) == 0 && s_acct_pass.compare(account_password) == 0)
				{
					server_log->Log(log_world, "Server %s(%s) successfully logged in.", long_name.c_str(), short_name.c_str());
					authorized = true;
					SetRuntimeID(s_id);
					server_list_type = s_list_type;
					desc = s_desc;
					if(s_trusted)
					{
						server_log->Log(log_network_trace, "ServerOP_LSAccountUpdate sent to world");
						trusted = true;
						ServerPacket *outapp = new ServerPacket(ServerOP_LSAccountUpdate, 0);
						connection->SendPacket(outapp);
					}
				}
				else
				{
					// this is the first of two cases where we should deny access even if unregistered is allowed
					server_log->Log(log_world, "Server %s(%s) attempted to log in but account and password did not match the entry in the database.",
						long_name.c_str(), short_name.c_str());
				}
			}
			else
			{
				if(s_acct_name.size() > 0 || s_acct_pass.size() > 0)
				{
					// this is the second of two cases where we should deny access even if unregistered is allowed
					server_log->Log(log_world, "Server %s(%s) did not attempt to log in but this server requires a password.",
						long_name.c_str(), short_name.c_str());
				}
				else
				{
					server_log->Log(log_world, "Server %s(%s) did not attempt to log in but unregistered servers are allowed.",
						long_name.c_str(), short_name.c_str());
					authorized = true;
					SetRuntimeID(s_id);
					server_list_type = 0;
				}
			}
		}
		else
		{
			s_id = 0;
			server_log->Log(log_world, "Server %s(%s) attempted to log in but database couldn't find an entry but unregistered servers are allowed.",
				long_name.c_str(), short_name.c_str());
			if(db.CreateWorldRegistration(long_name, short_name, s_id))
			{
				authorized = true;
				SetRuntimeID(s_id);
				server_list_type = 0;
			}
		}
	}

	in_addr in;
	in.s_addr = connection->GetrIP();
	db.UpdateWorldRegistration(GetRuntimeID(), long_name, string(inet_ntoa(in)));

	if(authorized)
	{
		server.CM->UpdateServerList();
	}
}

void WorldServer::Handle_LSStatus(ServerLSStatus_Struct *s)
{
	players_online = s->num_players;
	zones_booted = s->num_zones;
	status = s->status;
}

void WorldServer::SendClientAuth(unsigned int ip, string account, string key, unsigned int account_id, uint8 version)
{
	ServerPacket *outapp = new ServerPacket(ServerOP_LSClientAuth, sizeof(ServerLSClientAuth));
	ServerLSClientAuth* slsca = (ServerLSClientAuth*)outapp->pBuffer;

	slsca->lsaccount_id = account_id;
	strncpy(slsca->name, account.c_str(), account.size() > 30 ? 30 : account.size());
	strncpy(slsca->key, key.c_str(), 10);
	slsca->lsadmin = 0;
	slsca->worldadmin = 0;
	slsca->ip = ip;
	slsca->version = version;

	in_addr in;
	in.s_addr = ip;connection->GetrIP();
	string client_address(inet_ntoa(in));
	in.s_addr = connection->GetrIP();
	string world_address(inet_ntoa(in));

	if(client_address.compare(world_address) == 0)
	{
		slsca->local = 1;
	}
	else if (client_address.find(db.LoadServerSettings("options", "local_network").c_str()) != string::npos)
	{
		slsca->local = 1;
	}
	else
	{
		slsca->local = 0;
	}

	connection->SendPacket(outapp);

	if (server_log->DumpIn())
	{
		DumpPacket(outapp);
	}
	delete outapp;
}
