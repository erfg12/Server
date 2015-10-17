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

/*
 * Uncomment these as you wish to use. They can all be used for increased security.
 * If you run with no salt on local dev system, you can likely comment all of them.
 */
#ifndef DBSALT
//#define DBSALT
#endif

#ifndef INISALT
#define INISALT
#endif

#ifndef HARDSALT
//#define HARDSALT
#endif

#include "salt.h"
#include "database.h"

#include <fstream>
#include <iostream>

extern Database db;
extern Config cfg;
extern ErrorLog *server_log;

/*
* This is where you set the salt variables.
* You determine how you want salt handled.
* You can specify an ini file read, db read, BOTH combined, hard coded in the source, etc..
* If you have lax security here, it is your own fault.
*/

std::string Saltme::Salt(std::string password)
{
	std::string salt;
	std::string presalt;
	// for database value of salt.
#ifdef DBSALT
	if (db.CheckExtraSettings("salt"))
	{
		presalt += db.LoadServerSettings("options", "salt").c_str();
	}
#endif
	// for ini based salt.
#ifdef INISALT
	bool saltini = std::ifstream("salt.ini").good();
	bool loginini = std::ifstream("login.ini").good();
	std::string loadINIvalue = "12345678";
	if (!saltini)
	{
		if (loginini)
		{
			loadINIvalue = cfg.LoadOption("options", "salt", "login.ini");
		}
		std::ofstream dbini("salt.ini");
		dbini << "[Salt]\n";
		dbini << "salt = " + loadINIvalue + "\n";
		dbini.close();
	}
	if (saltini)
	{
		presalt += cfg.LoadOption("Salt", "salt", "salt.ini");
	}
#endif
	// if you want to hard code a salt in code.
#ifdef HARDSALT
	presalt += "12345678";
#endif
	salt = password + presalt;
	return salt;
}