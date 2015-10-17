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
#include "config.h"
#include "login_server.h"
#include "error_log.h"

#include <fstream>
#include <iostream>

#pragma warning( disable : 4267 )

extern ErrorLog *server_log;
extern LoginServer server;
Database db;

std::string Config::LoadOption(std::string title, std::string parameter, std::string filename)
{
	Parse(filename.c_str());
	std::map<std::string, std::map<std::string, std::string> >::iterator iter = vars.find(title);
	if (iter != vars.end())
	{
		std::map<std::string, std::string>::iterator arg_iter = iter->second.find(parameter);
		if (arg_iter != iter->second.end())
		{
			return arg_iter->second;
		}
	}
	return std::string("");
}

/**
* Runs all configuration routines and sets loginserver settings.
*/
bool Config::ConfigSetup()
{
	std::string configFile;

	bool login = std::ifstream("login.ini").good();
	bool dbini = std::ifstream("db.ini").good();

#pragma region condition: login.ini exists db.ini does not, create db.ini, copy login.ini then continue.
	if (login && !dbini) //tested 9/3/2015 good. File conditions and database conditions in Windows.
	{
		server_log->Log(log_debug, "login.ini found but db.ini missing.");

		server_log->Log(log_database, "Connecting to database to create db.ini and settings...");
		if (!db.Connect(
			server.config->LoadOption("database", "host", "login.ini").c_str(),
			server.config->LoadOption("database", "user", "login.ini").c_str(),
			server.config->LoadOption("database", "password", "login.ini").c_str(),
			server.config->LoadOption("database", "db", "login.ini").c_str(),
			atoul(server.config->LoadOption("database", "port", "login.ini").c_str())))
		{
			server_log->Log(log_error, "Unable to connect to the database, cannot continue without a database connection");
			return false;
		}

		//write db.ini based on login.ini entries
		server.config->WriteDBini();
		//check if table exists and set the field values based on the login.ini
		if (!db.CreateServerSettings())
		{
			//send shutdown to main.cpp critical failure.
			server_log->Log(log_error, "Create settings in tblloginserversettings failed.");
			return false;
		}
		//db.ini now set up. Settings are fine in database, continue on to config.
	}
#pragma endregion
#pragma region condition: login.ini does not exist but db.ini does. If table exists-continue. If not write defaults, prompt user with warning and halt.
	else if (!login && dbini) //tested 9/3/2015 good. File conditions and database conditions in Windows.
	{
		server_log->Log(log_debug, "db.ini found but login.ini missing. We are able to continue.");

		server_log->Log(log_database, "Connecting to database to check or repair settings...");
		if (!db.Connect(
			server.config->LoadOption("LoginServerDatabase", "host", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "user", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "password", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "db", "db.ini").c_str(),
			atoul(server.config->LoadOption("LoginServerDatabase", "port", "db.ini").c_str())))
		{
			server_log->Log(log_error, "Unable to connect to the database, cannot continue without a database connection");
			return false;
		}

		//check if settings exist in database.
		if (!db.CheckSettings(2))
		{
			if (!db.CreateServerSettings())
			{
				//send shutdown to main.cpp critical failure.
				server_log->Log(log_error, "Missing settings in tblloginserversettings.");
				return false;
			}
			server_log->Log(log_error, "WARNING: Defaults loaded, you may need to edit tblloginserversettings.");
			//defaults written to db, prompt user to edit and send shutdown to main.cpp.
			return false;
		}
		//db.ini was already set up. Settings are fine in database, continue on to config.
	}
#pragma endregion
#pragma region condition: both login.ini and db.ini exist. If table exists-continue. If not write login.ini contents to database and continue.
	else if (login && dbini) //tested 9/3/2015 good. File conditions and database conditions in Windows.
	{
		server_log->Log(log_debug, "db.ini and login.ini found. We are able to continue.");

		server_log->Log(log_database, "Connecting to database to check or repair settings...");
		if (!db.Connect(
			server.config->LoadOption("LoginServerDatabase", "host", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "user", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "password", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "db", "db.ini").c_str(),
			atoul(server.config->LoadOption("LoginServerDatabase", "port", "db.ini").c_str())))
		{
			server_log->Log(log_error, "Unable to connect to the database, cannot continue without a database connection");
			return false;
		}

		//check if settings exist in database.
		if (!db.CheckSettings(2))
		{
			if (db.CheckSettings(1))
			{
				server_log->Log(log_database, "Settings entries not found in database but table was.");
				//db.ResetDBSettings();
			}
			server_log->Log(log_database, "Settings entries not found in database creating base settings from ini files.");
			if (!db.CreateServerSettings())
			{
				server_log->Log(log_error, "Missing settings in tblloginserversettings.");
				//send shutdown to main.cpp critical failure.
				return false;
			}
		}
		//db.ini was already set up. Settings are fine in database, continue on to config.
		server_log->Log(log_debug, "login.ini and db.ini found. Database is set up.");
	}
#pragma endregion
#pragma region condition: no ini found. Write default db.ini and prompt user to edit and shut down.
	else //tested 9/3/2015 good. File conditions and database conditions in Windows.
	{
		server.config->WriteDBini();
		//send shutdown to main.cpp critical failure.
		server_log->Log(log_error, "Missing settings in the ini files, please edit and try again.");
		return false;
	}
#pragma endregion

	//ini processing succeeded, continue on.

	//Create our DB from options.
	server_log->Log(log_database, "MySQL Database Init.");

	bool config = std::ifstream("db.ini").good();

	if (config)
	{
		server_log->Log(log_database, "Connecting to database for game server...");
		if (!db.Connect(
			server.config->LoadOption("LoginServerDatabase", "host", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "user", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "password", "db.ini").c_str(),
			server.config->LoadOption("LoginServerDatabase", "db", "db.ini").c_str(),
			atoul(server.config->LoadOption("LoginServerDatabase", "port", "db.ini").c_str())))
		{
			//send shutdown to main.cpp critical failure.
			server_log->Log(log_error, "Unable to connect to the database, cannot continue without a database connection");
			return false;
		}
		server_log->Log(log_database, "Database connected for game server.");
	}
	else
	{
		//send shutdown to main.cpp critical failure.
		server_log->Log(log_error, "db.ini not processed, unknown error allowed us to get this far.");
		return false;
	}
	UpdateSettings();
	db.DBUpdate();
	return true;
}

void Config::WriteDBini()
{
	bool loginexist = std::ifstream("login.ini").good();
	bool dbexist = std::ifstream("db.ini").good();

	// db.ini was previously setup, we return to continue.
	if (dbexist && !loginexist)
	{
		return;
	}
	// No db.ini exists, create with settings from login.ini and continue.
	else if (!dbexist && loginexist)
	{
		std::ofstream dbini("db.ini");
		dbini << "[LoginServerDatabase]\n";
		dbini << "host = " + LoadOption("database", "host", "login.ini") + "\n";
		dbini << "port = " + LoadOption("database", "port", "login.ini") + "\n";
		dbini << "db = " + LoadOption("database", "db", "login.ini") + "\n";
		dbini << "user = " + LoadOption("database", "user", "login.ini") + "\n";
		dbini << "password = " + LoadOption("database", "password", "login.ini") + "\n";
		dbini << "\n";
		dbini << "[GameServerDatabase]\n";
		dbini << "#Game Server section not used yet.\n";
		dbini << "host = " + LoadOption("database", "host", "login.ini") + "\n";
		dbini << "port = " + LoadOption("database", "port", "login.ini") + "\n";
		dbini << "db = " + LoadOption("database", "db", "login.ini") + "\n";
		dbini << "user = " + LoadOption("database", "user", "login.ini") + "\n";
		dbini << "password = " + LoadOption("database", "password", "login.ini");
		dbini.close();
		return;
	}
	/** 
	* Place holder for reading from database for values.
	* Only writes this if all file checks fail.
	* User must edit db.ini that is created if this fires.
	*/
	else if (!dbexist && !loginexist)
	{
		std::ofstream dbini("db.ini");
		dbini << "[LoginServerDatabase]\n";
		dbini << "host = \n";
		dbini << "port = \n";
		dbini << "db = \n";
		dbini << "user = \n";
		dbini << "password = \n";
		dbini << "\n";
		dbini << "[GameServerDatabase]\n";
		dbini << "#Game Server section not used yet.\n";
		dbini << "host = \n";
		dbini << "port = \n";
		dbini << "db = \n";
		dbini << "user = \n";
		dbini << "password = ";
		dbini.close();
		return;
	}
	return;
}

/**
* For settings not included in legacy login.ini
*/
void Config::UpdateSettings()
{
	//formatting for adding loginserver settings.
	//db.InsertMissingSettings("type", "value", "category", "description", "defaults");

	db.InsertExtraSettings("pop_count", "0", "options", "0 to only display UP or DOWN or 1 to show population count in server select.", "0");
	db.InsertExtraSettings("ticker", "Welcome to EQMacEmu", "options", "Sets the welcome message in server select.", "Welcome to EQMacEmu");
}

/**
* Opens a file and passes it to be tokenized
* Then it parses the tokens returned and puts them into titles and variables.
*/
void Config::Parse(const char *file_name)
{
	//server_log->Log(log_debug, "Parsing file: %s.", file_name);
	if (file_name == nullptr)
	{
		server_log->Log(log_error, "Config::Parse(), file_name passed was null.");
		return;
	}
	vars.clear();
	FILE *input = fopen(file_name, "r");
	if (input)
	{
		std::list<std::string> tokens;
		Tokenize(input, tokens);

		char mode = 0;
		std::string title, param, arg;
		std::list<std::string>::iterator iter = tokens.begin();
		while (iter != tokens.end())
		{
			if ((*iter).compare("[") == 0)
			{
				title.clear();
				bool first = true;
				++iter;
				if (iter == tokens.end())
				{
					server_log->Log(log_error, "Config::Parse(), EOF before title done parsing.");
					fclose(input);
					vars.clear();
					return;
				}

				while ((*iter).compare("]") != 0 && iter != tokens.end())
				{
					if (!first)
					{
						title += " ";
					}
					else
					{
						first = false;
					}
					title += (*iter);
					++iter;
				}
				++iter;
			}

			if (mode == 0)
			{
				param = (*iter);
				mode++;
			}
			else if (mode == 1)
			{
				mode++;
				if ((*iter).compare("=") != 0)
				{
					server_log->Log(log_error, "Config::Parse(), invalid parse token where = should be.");
					fclose(input);
					vars.clear();
					return;
				}
			}
			else
			{
				arg = (*iter);
				mode = 0;
				std::map<std::string, std::map<std::string, std::string> >::iterator map_iter = vars.find(title);
				if (map_iter != vars.end())
				{
					map_iter->second[param] = arg;
					vars[title] = map_iter->second;
				}
				else
				{
					std::map<std::string, std::string> var_map;
					var_map[param] = arg;
					vars[title] = var_map;
				}
			}
			++iter;
		}
		fclose(input);
	}
	else
	{
		server_log->Log(log_error, "Config::Parse(), file was unable to be opened for parsing.");
		server_log->Log(log_error, "%s", file_name);
	}
}

/**
* Pretty basic lexical analyzer
* Breaks up the input character stream into tokens and puts them into the list provided.
* Ignores # as a line comment
*/
void Config::Tokenize(FILE *input, std::list<std::string> &tokens)
{
	char c = fgetc(input);
	std::string lexeme;

	while (c != EOF)
	{
		if (isspace(c))
		{
			if (lexeme.size() > 0)
			{
				tokens.push_back(lexeme);
				lexeme.clear();
			}
			c = fgetc(input);
			continue;
		}

		if (isalnum(c))
		{
			lexeme.append((const char *)&c, 1);
			c = fgetc(input);
			continue;
		}

		switch (c)
		{
		case '#':
		{
			if (lexeme.size() > 0)
			{
				tokens.push_back(lexeme);
				lexeme.clear();
			}

			while (c != '\n' && c != EOF)
			{
				c = fgetc(input);
			}
			break;
		}
		case '[':
		case ']':
		case '=':
		{
			if (lexeme.size() > 0)
			{
				tokens.push_back(lexeme);
				lexeme.clear();
			}
			lexeme.append((const char *)&c, 1);
			tokens.push_back(lexeme);
			lexeme.clear();
			break;
		}
		default:
		{
			lexeme.append((const char *)&c, 1);
		}
		}
		c = fgetc(input);
	}

	if (lexeme.size() > 0)
	{
		tokens.push_back(lexeme);
	}
}