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
#include "../common/global_define.h"
#include "../common/types.h"
#include "../common/opcodemgr.h"
#include "../common/eq_stream_factory.h"
#include "../common/timer.h"
#include "../common/platform.h"
#include "../common/crash.h"
#include "../common/eqemu_logsys.h"

#include "EQCrypto.h"
#include "login_server.h"
#include <time.h>
#include <stdlib.h>
#include <string>
#include <sstream>

#include <iostream>     // std::cout
#include <fstream>      // std::ifstream
#include <cstdio>

TimeoutManager timeout_manager;
LoginServer server;
EQEmuLogSys Log;
ErrorLog *server_log;
EQCrypto eq_crypto;
bool run_server = true;

void CatchSignal(int sig_num)
{
}

int main()
{
	RegisterExecutablePlatform(ExePlatformLogin);
	set_exception_handler();

	//Create our error log, is of format login_<number>.log
	time_t current_time = time(nullptr);
	std::stringstream log_name(std::stringstream::in | std::stringstream::out);
#ifdef WIN32
	log_name << ".\\logs\\login_" << (unsigned int)current_time << ".log";
#else
	log_name << "./logs/login_" << (unsigned int)current_time << ".log";
#endif

	server_log = new ErrorLog(log_name.str().c_str());
	server_log->Log(log_debug, "Logging System Init.");

	//Create our subsystem and parse the ini file.
	server.config = new Config();
	server_log->Log(log_debug, "Config System Init.");

	std::string configFile = "login.ini"; 
	ifstream f(configFile.c_str());
	if (f.good())
	{
	}
	else
	{
		f.close();
		server_log->Log(log_debug, "login.ini errors. Incomplete or missing.");
		#ifdef WIN32 //Almost never ran in a terminal on linux, so no need to hold window open to see error.
		server_log->Log(log_debug, "Press any key to close");
		std::getchar();
		#endif
	}

	server.config->Parse("login.ini");

	//Parse auto account creation option.
	if (server.config->GetVariable("options", "auto_account_create").compare("TRUE") == 0)
	{
		server.options.AutoCreate(true);
	}

	//Parse auto account activation option.
	if (server.config->GetVariable("options", "auto_account_activate").compare("TRUE") == 0)
	{
		server.options.AutoActivate(true);
	}

	//Parse failed access log option.
	if (server.config->GetVariable("options", "failed_login_log").compare("TRUE") == 0)
	{
		server.options.LoginFails(true);
	}

	//Parse IP login access log option. For cross checking account/IP "anti cheat/box record"
	if (server.config->GetVariable("options", "good_loginIP_log").compare("TRUE") == 0)
	{
		server.options.LoginGood(true);
	}

	//Parse unregistered allowed option.
	if(server.config->GetVariable("options", "unregistered_allowed").compare("FALSE") == 0)
	{
		server.options.AllowUnregistered(false);
	}

	//Parse trace option.
	if(server.config->GetVariable("options", "trace").compare("TRUE") == 0)
	{
		server.options.Trace(true);
	}

	//Parse trace option.
	if(server.config->GetVariable("options", "world_trace").compare("TRUE") == 0)
	{
		server.options.WorldTrace(true);
	}

	//Parse packet inc dump option.
	if(server.config->GetVariable("options", "dump_packets_in").compare("TRUE") == 0)
	{
		server.options.DumpInPackets(true);
	}

	//Parse packet out dump option.
	if(server.config->GetVariable("options", "dump_packets_out").compare("TRUE") == 0)
	{
		server.options.DumpOutPackets(true);
	}

	//Parse encryption mode option.
	std::string mode = server.config->GetVariable("security", "mode");
	if(mode.size() > 0)
	{
		server.options.EncryptionMode(atoi(mode.c_str()));
	}

	//Parse local network option.
	std::string ln = server.config->GetVariable("options", "local_network");
	if(ln.size() > 0)
	{
		server.options.LocalNetwork(ln);
	}

	//Parse local network option.
	std::string mip = server.config->GetVariable("options", "network_ip");
	if(mip.size() > 0)
	{
		server.options.NetworkIP(mip);
	}

	//Parse local network option.
	std::string pws = server.config->GetVariable("options", "salt");
	if(pws.size() > 0)
	{
		server.options.SetSalt(pws);
	}

	//Parse reject duplicate servers option.
	if(server.config->GetVariable("options", "reject_duplicate_servers").compare("TRUE") == 0)
	{
		server.options.RejectDuplicateServers(true);
	}

	//Parse account table option.
	ln = server.config->GetVariable("schema", "account_table");
	if(ln.size() > 0)
	{
		server.options.AccountTable(ln);
	}

	//Parse access log table option.
	ln = server.config->GetVariable("schema", "access_log_table");
	if (ln.size() > 0)
	{
		server.options.AccountAccessLogTable(ln);
	}

	//Parse world account table option.
	ln = server.config->GetVariable("schema", "world_registration_table");
	if(ln.size() > 0)
	{
		server.options.WorldRegistrationTable(ln);
	}

	//Parse admin world account table option.
	ln = server.config->GetVariable("schema", "world_admin_registration_table");
	if(ln.size() > 0)
	{
		server.options.WorldAdminRegistrationTable(ln);
	}

	//Parse world type table option.
	ln = server.config->GetVariable("schema", "world_server_type_table");
	if(ln.size() > 0)
	{
		server.options.WorldServerTypeTable(ln);
	}

	//Create our DB from options.
	if(server.config->GetVariable("database", "subsystem").compare("MySQL") == 0)
	{
#ifdef EQEMU_MYSQL_ENABLED
		server_log->Log(log_debug, "MySQL Database Init.");
		server.db = (Database*)new DatabaseMySQL(
			server.config->GetVariable("database", "user"),
			server.config->GetVariable("database", "password"),
			server.config->GetVariable("database", "host"),
			server.config->GetVariable("database", "port"),
			server.config->GetVariable("database", "db"));
#endif
	}
	else if(server.config->GetVariable("database", "subsystem").compare("PostgreSQL") == 0)
	{
#ifdef EQEMU_POSTGRESQL_ENABLED
		server_log->Log(log_debug, "PostgreSQL Database Init.");
		server.db = (Database*)new DatabasePostgreSQL(
			server.config->GetVariable("database", "user"),
			server.config->GetVariable("database", "password"),
			server.config->GetVariable("database", "host"),
			server.config->GetVariable("database", "port"),
			server.config->GetVariable("database", "db"));
#endif
	}

	//Make sure our database got created okay, otherwise cleanup and exit.
	if(!server.db)
	{
		server_log->Log(log_error, "Database Initialization Failure.");
		server_log->Log(log_debug, "Config System Shutdown.");
		delete server.config;
		server_log->Log(log_debug, "Log System Shutdown.");
		delete server_log;
		return 1;
	}

	//create our server manager.
	server_log->Log(log_debug, "Server Manager Initialize.");
	server.SM = new ServerManager();
	if(!server.SM)
	{
		//We can't run without a server manager, cleanup and exit.
		server_log->Log(log_error, "Server Manager Failed to Start.");
		server_log->Log(log_debug, "Database System Shutdown.");
		delete server.db;
		server_log->Log(log_debug, "Config System Shutdown.");
		delete server.config;
		server_log->Log(log_debug, "Log System Shutdown.");
		delete server_log;
		return 1;
	}

	//create our client manager.
	server_log->Log(log_debug, "Client Manager Initialize.");
	server.CM = new ClientManager();
	if(!server.CM)
	{
		//We can't run without a client manager, cleanup and exit.
		server_log->Log(log_error, "Client Manager Failed to Start.");
		server_log->Log(log_debug, "Server Manager Shutdown.");
		delete server.SM;
		server_log->Log(log_debug, "Database System Shutdown.");
		delete server.db;
		server_log->Log(log_debug, "Config System Shutdown.");
		delete server.config;
		server_log->Log(log_debug, "Log System Shutdown.");
		delete server_log;
		return 1;
	}

#ifdef WIN32
#ifdef UNICODE
	SetConsoleTitle(L"EQEmu Login Server");
#else
	SetConsoleTitle("EQEmu Login Server");
#endif
#endif

	server_log->Log(log_debug, "Server Started.");
	while(run_server)
	{
		Timer::SetCurrentTime();
		server.CM->Process();
		server.SM->Process();
		timeout_manager.CheckTimeouts();
		Sleep(100);
	}
	server_log->Log(log_debug, "Server Shutdown.");
	server_log->Log(log_debug, "Client Manager Shutdown.");
	delete server.CM;
	server_log->Log(log_debug, "Server Manager Shutdown.");
	delete server.SM;
	server_log->Log(log_debug, "Database System Shutdown.");
	delete server.db;
	server_log->Log(log_debug, "Config System Shutdown.");
	delete server.config;
	server_log->Log(log_debug, "Log System Shutdown.");
	delete server_log;
	return 0;
}