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

#include "../common/eq_packet_structs.h"
#include "../common/extprofile.h"
#include "../common/string_util.h"
#include "database.h"
#include "login_server.h"

extern ErrorLog *server_log;

Database::Database()
{
}

/*
* Establish a connection to a mysql database with the supplied parameters
*/
Database::Database(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	Connect(host, user, passwd, database, port);
}

bool Database::Connect(const char* host, const char* user, const char* passwd, const char* database, uint32 port)
{
	server_log->Log(log_database_trace, "Connect got: host=%s user=%s database=%s port=%u", host, user, database, port);
	uint32 errnum = 0;
	char errbuf[MYSQL_ERRMSG_SIZE];
	if (!Open(host, user, passwd, database, port, &errnum, errbuf))
	{
		server_log->Log(log_error, "Failed to connect to database: Error: %s", errbuf);
		return false;
	}
	else
	{
		server_log->Log(log_database_trace, "Using database '%s' at %s:%u", database, host, port);
		return true;
	}
}

#pragma region Load Server Setup
std::string Database::LoadServerSettings(std::string category, std::string type)
{
	std::string query;
	std::string value = "";

	query = StringFormat(
		"SELECT * FROM tblloginserversettings "
		"WHERE "
			"type = '%s' "
		"AND "
			"category = '%s'",
		type.c_str(),
		category.c_str()
		);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "LoadServerSettings Mysql query return no results: %s", query.c_str());
	}
	auto row = results.begin();
	if (row[1] != nullptr)
	{
		value = row[1];
	}
	return value.c_str();
}
#pragma endregion

#pragma region Create Server Setup
bool Database::CheckSettings(int type)
{
	std::string query;
	server_log->Log(log_database_trace, "Checking for database structure with type %s", std::to_string(type).c_str());
	if (type == 1)
	{
		query = StringFormat("SHOW TABLES LIKE 'tblloginserversettings'");

		auto results = QueryDatabase(query);

		if (!results.Success())
		{
			server_log->Log(log_database_error, "CheckSettings Mysql query return no results: %s", query.c_str());
			return false;
		}
		server_log->Log(log_database_trace, "tblloginserversettings exists sending continue.");
		return true;
	}
	if (type == 2)
	{
		query = StringFormat("SELECT * FROM tblloginserversettings");

		auto results = QueryDatabase(query);

		if (!results.Success())
		{
			server_log->Log(log_database_error, "CheckSettings Mysql query return no results: %s", query.c_str());
			return false;
		}
		server_log->Log(log_database_trace, "tblloginserversettings entries exist sending continue.");
		return true;
	}
	else
	{
		server_log->Log(log_database_error, "tblloginserversettings does not exist.");
		return false;
	}
}

bool Database::CreateServerSettings()
{
	std::string query;
	std::string query2;
	server_log->Log(log_database_trace, "Entering Server Settings database setup.");

	query = StringFormat("SHOW TABLES LIKE 'tblloginserversettings'");

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "CreateServerSettings Mysql query failed to search for table: %s", query.c_str());
		return false;
	}

	server_log->Log(log_database, "CreateServerSettings: table does not exist, creating: %s", query.c_str());

	if (results.RowCount() <= 0)
	{
		server_log->Log(log_database, "Server Settings table does not exist, creating.");

		query = StringFormat(
				"CREATE TABLE `tblloginserversettings` ("
				"`type` varchar(50) NOT NULL,"
				"`value` varchar(50),"
				"`category` varchar(20) NOT NULL,"
				"`description` varchar(99) NOT NULL,"
				"`defaults` varchar(50)"
				") ENGINE=InnoDB DEFAULT CHARSET=latin1");

		auto results = QueryDatabase(query);

		if (!results.Success())
		{
			server_log->Log(log_database_error, "CreateServerSettings Mysql query failed to create table: %s", query.c_str());
			return false;
		}

		query2 = StringFormat(
			"INSERT INTO `tblloginserversettings` (type, value, category, description, defaults)"
			"VALUES"
			"('access_log_table', '', 'schema', 'location for access logs, failed logins and goodIP.', 'tblaccountaccesslog'),"
			"('account_table', '', 'schema', 'location of all client account info for login server only.', 'tblLoginServerAccounts'),"
			"('auto_account_activate', '', 'options', 'set this to TRUE to allow new accounts to log in.', 'TRUE'),"
			"('auto_account_create', '', 'options', 'set this to TRUE to auto create accounts on player first log in.', 'TRUE'),"
			"('port', '', 'Old', 'port for clients to connect to.', '6000'),"
			"('dump_packets_in', '', 'options', 'debugging', 'FALSE'),"
			"('dump_packets_out', '', 'options', 'debugging', 'FALSE'),"
			"('failed_login_log', '', 'options', 'set this to TRUE to log failed log in attempts.', 'TRUE'),"
			"('good_loginIP_log', '', 'options', 'set this to TRUE to log successful account log ins.', 'TRUE'),"
			"('listen_port', '', 'options', 'this is the port we listen on for world connection.', '5998'),"
			"('local_network', '', 'options', 'set to the network ip that world server is on.', '127.0.0.1'),"
			"('mode', '', 'security', 'encryption mode the plugin uses.', '5'),"
			"('network_ip', '', 'options', 'set to the network ip that world server is on.', '127.0.0.1'),"
			"('opcodes', '', 'Old', 'opcode file for client compatibility. (Old means classic/mac)', 'login_opcodes_oldver.conf'),"
			"('plugin', '', 'security', 'the encryption type the login server uses.', 'EQEmuAuthCrypto'),"
			"('pop_count', '', 'options', '0 to only display UP or DOWN or 1 to show population count in server select.', '0'),"
			"('reject_duplicate_servers', '', 'options', 'set this to TRUE to force unique server name connections.', 'TRUE'),"
			"('salt', '', 'options', 'for account security make this a numeric random number.', '12345678'),"
			"('ticker', '', 'options', 'Sets the welcome message in server select.', 'Welcome to EQMacEmu'),"
			"('trace', '', 'options', 'debugging', 'FALSE'),"
			"('unregistered_allowed', '', 'options', 'set this to TRUE to allow any server to connect.', 'TRUE'),"
			"('world_admin_registration_table', '', 'schema', 'location of administrator account info for this login server.', 'tblServerAdminRegistration'),"
			"('world_registration_table', '', 'schema', 'location of registered or connection records of servers connecting to this loginserver.', 'tblWorldServerRegistration'),"
			"('world_server_type_table', '', 'schema', 'location of server type descriptions.', 'tblServerListType'),"
			"('world_trace', '', 'options', 'debugging', 'FALSE');");

		auto results2 = QueryDatabase(query2);

		if (!results2.Success())
		{
			server_log->Log(log_database_error, "CreateServerSettings Mysql query failed: %s", query2.c_str());
			return false;
		}			
			
		// type, category, default
		bool failed = false;
		failed |= !SetServerSettings("access_log_table", "schema", "tblaccountaccesslog");
		failed |= !SetServerSettings("account_table", "schema", "tblLoginServerAccounts");
		failed |= !SetServerSettings("auto_account_activate", "options", "TRUE");
		failed |= !SetServerSettings("auto_account_create", "options", "TRUE");
		failed |= !SetServerSettings("port", "Old", "6000");
		failed |= !SetServerSettings("dump_packets_in", "options", "FALSE");
		failed |= !SetServerSettings("dump_packets_out", "options", "FALSE");
		failed |= !SetServerSettings("failed_login_log", "options", "TRUE");
		failed |= !SetServerSettings("good_loginIP_log", "options", "TRUE");
		failed |= !SetServerSettings("listen_port", "options", "5998");
		failed |= !SetServerSettings("local_network", "options", "127.0.0.1");
		failed |= !SetServerSettings("mode", "security", "5");
		failed |= !SetServerSettings("network_ip", "options", "127.0.0.1");
		failed |= !SetServerSettings("opcodes", "Old", "login_opcodes_oldver.conf");
		failed |= !SetServerSettings("plugin", "security", "EQEmuAuthCrypto");
		failed |= !SetServerSettings("pop_count", "options", "0");
		failed |= !SetServerSettings("reject_duplicate_servers", "options", "TRUE");
		failed |= !SetServerSettings("salt", "options", "12345678");
		failed |= !SetServerSettings("ticker", "options", "Welcome to EQMacEmu");
		failed |= !SetServerSettings("trace", "options", "FALSE");
		failed |= !SetServerSettings("unregistered_allowed", "options", "TRUE");
		failed |= !SetServerSettings("world_admin_registration_table", "schema", "tblServerAdminRegistration");
		failed |= !SetServerSettings("world_registration_table", "schema", "tblWorldServerRegistration");
		failed |= !SetServerSettings("world_server_type_table", "schema", "tblServerListType");
		failed |= !SetServerSettings("world_trace", "options", "FALSE");

		if (failed)
		{
			return false;
		}
		server_log->Log(log_database_trace, "Server Settings table created, continuing.");
	}
	else
	{
		server_log->Log(log_database_trace, "CreateServerSettings found existing settings, continuing on.");
	}
	if (GetServerSettings())
	{
		return true;
	}
	return false;
}

bool Database::GetServerSettings()
{
	server_log->Log(log_database, "Entered GetServerSettings.");
	std::string query;
	query = StringFormat("SELECT * FROM tblloginserversettings");

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "GetServerSettings Mysql check_query failed: %s", query.c_str());
		return false;
	}

	bool result = true;
	auto row = results.begin();

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		std::string type = row[0];
		std::string value = row[1];
		std::string category = row[2];
		std::string description = row[3];
		std::string defaults = row[4];

		if (value.empty())
		{
			server_log->Log(log_database, "Mysql check_query returns NO value for type %s", type.c_str());
			result = false;
		}
		else if (!value.empty())
		{
			server_log->Log(log_database, "Mysql check_query returns `value`: %s for type %s", value.c_str(), type.c_str());
		}
	}
	return result;
}

bool Database::SetServerSettings(std::string type, std::string category, std::string defaults)
{
	std::string query;

	Config* newval = new Config;
	std::string loadINIvalue = newval->LoadOption(category, type, "login.ini");
	query = StringFormat(
		"UPDATE tblloginserversettings "
		"SET value = '%s' "
		"WHERE type = '%s' "
		"AND category = '%s' "
		"LIMIT 1", 
		loadINIvalue.empty() ? defaults.c_str() : loadINIvalue.c_str(), type.c_str(), category.c_str());
	safe_delete(newval);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "SetServerSettings Mysql check_query failed: %s", query.c_str());
		return false;
	}
	return true;
}

bool Database::CheckExtraSettings(std::string type)
{
	std::string query;

	server_log->Log(log_database_trace, "Entered CheckExtraSettings using type: %s.", type.c_str());

	query = StringFormat("SELECT * FROM `tblloginserversettings` WHERE `type` = '%s';", type.c_str());

	auto check_results = QueryDatabase(query);

	if (!check_results.Success())
	{
		server_log->Log(log_database_error, "CheckExtraSettings query failed: %s", query.c_str());
		return false;
	}

	auto row = check_results.begin();
	if (check_results.RowCount() > 0)
	{
		server_log->Log(log_database_trace, "CheckExtraSettings type: %s exists.", row[0]);
		return true;
	}
	else
	{
		server_log->Log(log_database_trace, "CheckExtraSettings type: %s does not exist.", type.c_str());
		return false;
	}
}

void Database::InsertExtraSettings(std::string type, std::string value, std::string category, std::string description, std::string defaults)
{
	std::string query;
	if (!CheckExtraSettings(type))
	{
		query = StringFormat("INSERT INTO `tblloginserversettings` (`type`, `value`, category, description, defaults)"
								"VALUES('%s', '%s', '%s', '%s', '%s');", 
									type.c_str(),
									value.c_str(),
									category.c_str(),
									description.c_str(),
									defaults.c_str());

		auto results = QueryDatabase(query);

		if (!results.Success())
		{
			server_log->Log(log_database_error, "InsertMissingSettings query failed: %s", query.c_str());
		}
	}
	return;
}

bool Database::DBUpdate()
{
	server_log->Log(log_database, "Database bootstrap check entered, adjusting database to current requirements...");
	DBSetup_SetEmailDefault(); //Legacy compatibility for mariahdb
	return true;
}

bool Database::DBSetup_SetEmailDefault()
{
	std::string check_query = StringFormat("SHOW COLUMNS FROM `tblloginserveraccounts` LIKE 'AccountEmail'");
	auto results = QueryDatabase(check_query);
	if (results.RowCount() != 0)
	{
		std::string create_query = StringFormat("ALTER TABLE `tblloginserveraccounts` CHANGE `AccountEmail` `AccountEmail` varchar(100) not null default '0'");
		auto create_results = QueryDatabase(create_query);
		if (!create_results.Success())
		{
			server_log->Log(log_database_error, "Error adjusting AccountEmail column.");
			return false;
		}
	}
	server_log->Log(log_database, "Adjusted AccountEmail column.");
	return true;
}
#pragma endregion

#pragma region World Server Account Info
bool Database::CreateWorldRegistration(std::string long_name, std::string short_name, unsigned int &id)
{
	char escaped_long_name[201];
	char escaped_short_name[101];
	DoEscapeString(escaped_long_name, long_name.substr(0, 100).c_str(), (int)strlen(long_name.substr(0, 100).c_str()));
	DoEscapeString(escaped_short_name, short_name.substr(0, 100).c_str(), (int)strlen(short_name.substr(0, 100).c_str()));

	std::string query;

	query = StringFormat("SELECT max(ServerID) FROM %s", LoadServerSettings("schema", "world_registration_table").c_str());

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
		return false;
	}

	auto row = results.begin();
	if (row[0] != nullptr)
	{
		id = atoi(row[0]) + 1;
	}
	else
	{
		id = 0;
	}

	query = StringFormat("INSERT INTO %s SET "
		"ServerID = '%i', "
		"ServerLongName = '%s', "
		"ServerShortName = '%s', "
		"ServerListTypeID = 0, "
		"ServerAdminID = 0, "
		"ServerTrusted = 0, "
		"ServerTagDescription = ''", 
		LoadServerSettings("schema", "world_registration_table").c_str(),
		id,
		escaped_long_name,
		escaped_short_name
		);

	auto results2 = QueryDatabase(query);

	if (!results2.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
		return false;
	}
	return true;
}

void Database::UpdateWorldRegistration(unsigned int id, std::string long_name, std::string ip_address)
{
	char escaped_long_name[101];
	DoEscapeString(escaped_long_name, long_name.substr(0, 100).c_str(), (int)strlen(long_name.substr(0, 100).c_str()));

	std::string query;

	query = StringFormat("UPDATE %s "		
		"SET "
		"ServerLastLoginDate = now(), "
		"ServerLastIPAddr = '%s', "
		"ServerLongName = '%s' "
		"WHERE "
		"ServerID = '%s'",
		LoadServerSettings("schema", "world_registration_table").c_str(),
		ip_address.c_str(),
		escaped_long_name,
		std::to_string(id).c_str()
		);
	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
	}
}

bool Database::GetWorldRegistration(unsigned int &id, std::string &desc, unsigned int &trusted, unsigned int &list_id, 
									std::string &account, std::string &password, std::string long_name, std::string short_name)	
{
	char escaped_short_name[101];
	DoEscapeString(escaped_short_name, short_name.substr(0, 100).c_str(), (int)strlen(short_name.substr(0, 100).c_str()));

	std::string query;

	query = StringFormat("SELECT "
		"ServerID, ServerTagDescription, ServerTrusted, ServerListTypeID, ServerAdminID "
		"FROM %s WHERE ServerShortName = '%s'",
		LoadServerSettings("schema", "world_registration_table").c_str(),
		escaped_short_name
		);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
		return false;
	}

	auto row = results.begin();

	if (results.RowCount() > 0)
	{
		id = atoi(row[0]);
		desc = row[1];
		trusted = atoi(row[2]);
		list_id = atoi(row[3]);
		int db_account_id = atoi(row[5]);

		if (db_account_id > 0)
		{
			std::string query;
			query = StringFormat("SELECT AccountName, AccountPassword "
				"FROM %s WHERE ServerAdminID = %s",
				LoadServerSettings("schema", "world_admin_registration_table").c_str(),
				std::to_string(db_account_id).c_str()
				);
			auto results = QueryDatabase(query);

			if (!results.Success())
			{
				server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
				return false;
			}

			auto row = results.begin();
			if (results.RowCount() > 0)
			{
				account = row[0];
				password = row[1];
				return true;
			}
			server_log->Log(log_database_trace, "Mysql query returned no result: %s", query.c_str());
			return false;
		}
		return true;
	}
	server_log->Log(log_database_error, "Mysql query returned no result: %s", query.c_str());
	return false;
}

bool Database::GetWorldPreferredStatus(int id)
{
	std::string query;

	query = StringFormat("SELECT "
		"tblWorldServerRegistration.ServerListTypeID "
		"FROM "
		"tblWorldServerRegistration "
		"WHERE "
		"tblWorldServerRegistration.ServerID = %i", id);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
		return false;
	}

	auto row = results.begin();

	if (results.RowCount() > 0)
	{
		int type_id = atoi(row[0]);
		if (type_id > 0)
		{
			return true;
		}
	}
	return false;
}
#pragma endregion

#pragma region Player Account Info
void Database::CreateLSAccount(std::string name, std::string password, std::string email, unsigned int created_by, std::string LastIPAddress, std::string creationIP)
{
	bool activate = 0;
	if (LoadServerSettings("options", "auto_account_activate") == "TRUE")
	{
		activate = 1;
	}
	std::string query;

	char tmpUN[1024];
	DoEscapeString(tmpUN, name.c_str(), (int)name.length());

	query = StringFormat("INSERT INTO %s "
		"SET AccountName = '%s', "
		"AccountPassword = sha('%s'), "
		"AccountCreateDate = now(), "
		"LastLoginDate = now(), "
		"LastIPAddress = '%s', "
		"client_unlock = '%s', "
		"created_by = '%s', "
		"creationIP = '%s'",
		LoadServerSettings("schema", "account_table").c_str(),
		tmpUN,
		password.c_str(),
		LastIPAddress.c_str(),
		std::to_string(activate).c_str(),
		std::to_string(created_by).c_str(),
		creationIP.c_str()
		);

	auto results = QueryDatabase(query);
	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
	}
}

void Database::UpdateLSAccount(unsigned int id, std::string ip_address)
{
	std::string query;

	query = StringFormat("UPDATE %s SET "
		"LastIPAddress = '%s', "
		"LastLoginDate = now() "
		"WHERE "
		"LoginServerID = %s",
		LoadServerSettings("schema", "account_table").c_str(),
		ip_address.c_str(),
		std::to_string(id).c_str()
		);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
	}
}

bool Database::GetAccountLockStatus(std::string name)
{
	std::string query;

	query = StringFormat("SELECT client_unlock FROM %s WHERE "
						"AccountName = '%s'",
						LoadServerSettings("schema", "account_table").c_str(),
						name.c_str()
						);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
		return false;
	}

	auto row = results.begin();
	while (row[0] != nullptr)
	{
		int unlock = atoi(row[0]);
		if (unlock == 1)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return false;
}

void Database::UpdateAccessLog(unsigned int account_id, std::string account_name, std::string IP, unsigned int accessed, std::string reason)
{
	std::string query;

	char tmpUN[1024];
	DoEscapeString(tmpUN, account_name.c_str(), (int)account_name.length());

	query = StringFormat("INSERT INTO %s SET "
		"account_id = %s, "
		"account_name = '%s', "
		"IP = '%s', "
		"accessed = '%s', "
		"reason = '%s'",
		LoadServerSettings("schema", "access_log_table").c_str(),
		std::to_string(account_id).c_str(),
		tmpUN,
		IP.c_str(),
		std::to_string(accessed).c_str(),
		reason.c_str()
		);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
	}
}

bool Database::GetLoginDataFromAccountName(std::string name, std::string &password, unsigned int &id)
{
	std::string query;

	char tmpUN[1024];
	DoEscapeString(tmpUN, name.c_str(), (int)name.length()); (tmpUN, name.c_str(), (int)name.length());

	query = StringFormat("SELECT LoginServerID, AccountPassword "
		"FROM %s WHERE "
		"AccountName = '%s'",
		LoadServerSettings("schema", "account_table").c_str(),
		tmpUN
		);

	auto results = QueryDatabase(query);

	if (!results.Success())
	{
		server_log->Log(log_database_error, "Mysql query failed: %s", query.c_str());
		return false;
	}
	if (results.RowCount() > 0)
	{
		auto row = results.begin();
		if (row[0] != nullptr)
		{
			id = atoi(row[0]);
			password = row[1];
			return true;
		}
		server_log->Log(log_database_error, "Unknown error, no result for: %s", query.c_str());
		return false;
	}
	server_log->Log(log_database_error, "Mysql query returned no result for: %s", query.c_str());
	return false;
}
#pragma endregion
