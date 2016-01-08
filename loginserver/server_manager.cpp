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
#include "server_manager.h"
#include "login_server.h"
#include "error_log.h"
#include "login_structures.h"
#include <stdlib.h>

#pragma warning( disable : 4996 4267 )

extern ErrorLog *server_log;
extern LoginServer server;
extern bool run_server;
extern Database db;

ServerManager::ServerManager()
{
	char error_buffer[TCPConnection_ErrorBufferSize];

	server_log->Log(log_debug, "ServerManager Entered.");
	server_log->Trace("ServerManager Got listen_port value from db.");

	int listen_port = atoul(db.LoadServerSettings("options", "listen_port").c_str());
	tcps = new EmuTCPServer(listen_port, true);
	if(tcps->Open(listen_port, error_buffer))
	{
		server_log->Log(log_network, "ServerManager listening on port %u", listen_port);
	}
	else
	{
		server_log->Log(log_error, "ServerManager fatal error opening port on %u: %s", listen_port, error_buffer);
		run_server = false;
	}
}

ServerManager::~ServerManager()
{
	if(tcps)
	{
		tcps->Close();
		delete tcps;
	}
}

void ServerManager::Process()
{
	ProcessDisconnect();
	EmuTCPConnection *tcp_c = nullptr;
	while(tcp_c = tcps->NewQueuePop())
	{
		in_addr tmp;
		tmp.s_addr = tcp_c->GetrIP();
		server_log->Log(log_network, "New world server connection from %s:%d", inet_ntoa(tmp), tcp_c->GetrPort());

		WorldServer *cur = GetServerByAddress(tcp_c->GetrIP());
		if(cur)
		{
			server_log->Log(log_network, "World server already existed for %s, removing existing connection and updating current.", inet_ntoa(tmp));
			cur->GetConnection()->Free();
			cur->SetConnection(tcp_c);
			cur->Reset();
		}
		else
		{
			WorldServer *w = new WorldServer(tcp_c);
			world_servers.push_back(w);
		}
	}

	list<WorldServer*>::iterator iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		if((*iter)->Process() == false)
		{
			server_log->Log(log_world, "World server %s had a fatal error and had to be removed from the login.", (*iter)->GetLongName().c_str());
			delete (*iter);
			iter = world_servers.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

void ServerManager::ProcessDisconnect()
{
	list<WorldServer*>::iterator iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		EmuTCPConnection *c = (*iter)->GetConnection();
		if(!c->Connected())
		{
			in_addr tmp;
			tmp.s_addr = c->GetrIP();
			server_log->Log(log_network, "World server disconnected from the server, removing server and freeing connection.");
			c->Free();
			delete (*iter);
			iter = world_servers.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

WorldServer* ServerManager::GetServerByAddress(unsigned int address)
{
	list<WorldServer*>::iterator iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		if((*iter)->GetConnection()->GetrIP() == address)
		{
			return (*iter);
		}
		++iter;
	}
	return nullptr;
}

EQApplicationPacket* ServerManager::CreateOldServerListPacket(Client* c)
{

	unsigned int packet_size = sizeof(ServerList_Struct);
	unsigned int server_count = 0;
	in_addr in;
	in.s_addr = c->GetConnection()->GetRemoteIP();
	string client_ip = inet_ntoa(in);
	list<WorldServer*>::iterator iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		if((*iter)->IsAuthorized() == false)
		{
			++iter;
			continue;
		}

		in.s_addr = (*iter)->GetConnection()->GetrIP();
		string world_ip = inet_ntoa(in);
		string servername = (*iter)->GetLongName().c_str();
		servername.append(" Server");

		if(world_ip.compare(client_ip) == 0)
		{
			packet_size += servername.size() + 1 + (*iter)->GetLocalIP().size() + 1 + sizeof(ServerListServerFlags_Struct);
		}
		else if (client_ip.find(db.LoadServerSettings("options", "local_network").c_str()) != string::npos)
		{
			packet_size += servername.size() + 1 + (*iter)->GetLocalIP().size() + 1 + sizeof(ServerListServerFlags_Struct);
		}
		else
		{
			packet_size += servername.size() + 1 + (*iter)->GetRemoteIP().size() + 1 + sizeof(ServerListServerFlags_Struct);
		}
		server_count++;
		++iter;
	}

	packet_size += sizeof(ServerListEndFlags_Struct); // flags and unknowns
	packet_size += 1; // flags and unknowns
	EQApplicationPacket *outapp = new EQApplicationPacket(OP_ServerListRequest, packet_size);
	ServerList_Struct *sl = (ServerList_Struct*)outapp->pBuffer;
	sl->numservers = server_count;

	uint8 showcount = 0x0;
	if (db.CheckExtraSettings("pop_count"))
	{
		if (db.LoadServerSettings("options", "pop_count") == "1")
		{
			showcount = 0xFF;
		}
	}
	sl->showusercount = showcount;

	unsigned char *data_ptr = outapp->pBuffer;
	data_ptr += sizeof(ServerList_Struct);

	iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		if((*iter)->IsAuthorized() == false)
		{
			++iter;
			continue;
		}

		in.s_addr = (*iter)->GetConnection()->GetrIP();
		string world_ip = inet_ntoa(in);

		string servername = (*iter)->GetLongName().c_str();
		servername.append(" Server");

		memcpy(data_ptr, servername.c_str(), servername.size());
		data_ptr += (servername.size() + 1);

		if(world_ip.compare(client_ip) == 0)
		{
			memcpy(data_ptr, (*iter)->GetLocalIP().c_str(), (*iter)->GetLocalIP().size());
			data_ptr += ((*iter)->GetLocalIP().size() + 1);
		}
		else if (client_ip.find(db.LoadServerSettings("options", "local_network").c_str()) != string::npos)
		{
			memcpy(data_ptr, (*iter)->GetLocalIP().c_str(), (*iter)->GetLocalIP().size());
			data_ptr += ((*iter)->GetLocalIP().size() + 1);
		}
		else
		{
			memcpy(data_ptr, (*iter)->GetRemoteIP().c_str(), (*iter)->GetRemoteIP().size());
			data_ptr += ((*iter)->GetRemoteIP().size() + 1);
		}
		
		ServerListServerFlags_Struct* slsf = (ServerListServerFlags_Struct*)data_ptr;

		// bit 0x1 is set name green - higher bits set language
		slsf->greenname = 0;
		if (db.GetWorldPreferredStatus((*iter)->GetRuntimeID()))
		{
			slsf->greenname = 1;
		}

		slsf->flags = 0x1;
		slsf->worldid = (*iter)->GetRuntimeID();
		slsf->usercount = (*iter)->GetStatus();
		data_ptr += sizeof(ServerListServerFlags_Struct);
		++iter;
	}
	ServerListEndFlags_Struct* slef = (ServerListEndFlags_Struct*)data_ptr;
	return outapp;
}

void ServerManager::SendOldUserToWorldRequest(const char* server_id, unsigned int client_account_id, uint32 ip)
{
	list<WorldServer*>::iterator iter = world_servers.begin();
	bool found = false;
	while(iter != world_servers.end())
	{
		if((*iter)->GetRemoteIP() == server_id)
		{
			ServerPacket *outapp = new ServerPacket(ServerOP_UsertoWorldReq, sizeof(UsertoWorldRequest_Struct));
			UsertoWorldRequest_Struct *utwr = (UsertoWorldRequest_Struct*)outapp->pBuffer;

			//utwr->worldid = (*iter)->GetServerListID(); //This pulls preffered status instead of actual ID? That does not seem right.
			utwr->worldid = (*iter)->GetRuntimeID();

			utwr->lsaccountid = client_account_id;
			utwr->ip = ip;
			(*iter)->GetConnection()->SendPacket(outapp);
			found = true;

			if (server_log->DumpIn())
			{
				DumpPacket(outapp);
			}
			delete outapp;
		}
		++iter;
	}
	server_log->Trace("Client requested a user to world but supplied an invalid id");
	server_log->Trace(server_id);
}

bool ServerManager::ServerExists(string l_name, string s_name, WorldServer *ignore)
{
	list<WorldServer*>::iterator iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		if((*iter) == ignore)
		{
			++iter;
			continue;
		}

		if((*iter)->GetLongName().compare(l_name) == 0 && (*iter)->GetShortName().compare(s_name) == 0)
		{
			return true;
		}
		++iter;
	}
	return false;
}

void ServerManager::DestroyServerByName(string l_name, string s_name, WorldServer *ignore)
{
	list<WorldServer*>::iterator iter = world_servers.begin();
	while(iter != world_servers.end())
	{
		if((*iter) == ignore)
		{
			++iter;
		}

		if((*iter)->GetLongName().compare(l_name) == 0 && (*iter)->GetShortName().compare(s_name) == 0)
		{
			EmuTCPConnection *c = (*iter)->GetConnection();
			if(c->Connected())
			{
				c->Disconnect();
			}
			c->Free();
			delete (*iter);
			iter = world_servers.erase(iter);
		}
		++iter;
	}
}
