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

	if (!server.config->ConfigSetup())
	{
		server_log->Log(log_error, "Server Config Setup failed, LS shutting down.");
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
	server_log->Log(log_debug, "Config System Shutdown.");
	delete server.config;
	server_log->Log(log_debug, "Log System Shutdown.");
	delete server_log;
	return 0;
}