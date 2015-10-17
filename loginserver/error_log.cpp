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
#include <string.h>
#include "error_log.h"

#include "database.h"
#include <algorithm>

#ifdef _WINDOWS
#include <iostream>
#include <limits>
#include <windows.h>
#endif


#define BLACK			0
#define BLUE			1
#define GREEN			2
#define CYAN			3
#define RED				4
#define MAGENTA			5
#define BROWN			6
#define LIGHTGRAY		7
#define DARKGRAY		8
#define LIGHTBLUE		9
#define LIGHTGREEN		10
#define LIGHTCYAN		11
#define LIGHTRED		12
#define LIGHTMAGENTA	13
#define YELLOW			14
#define WHITE			15

extern Database db;

const char *eqLogTypes[_log_largest_type] =
{
	"Debug",
	"Error",
	"Database",
	"Database Trace",
	"Database Error",
	"Network",
	"Network Trace",
	"Network Error",
	"World",
	"World Trace",
	"World Error",
	"Client",
	"Client Trace",
	"Client Error"
};

ErrorLog::ErrorLog(const char* file_name)
{
	log_mutex = new Mutex();
	error_log = fopen(file_name, "w");
}

ErrorLog::~ErrorLog()
{
	log_mutex->lock();
	if(error_log)
	{
		fclose(error_log);
	}
	log_mutex->unlock();
	delete log_mutex;
}

void ErrorLog::Trace(const char *message, ...)
{
	std::string trace = db.LoadServerSettings("options", "trace").c_str();
	std::transform(trace.begin(), trace.end(), trace.begin(), ::toupper);
	if (trace == "TRUE")
	{
		Log(log_network_trace, message);
	}
}

void ErrorLog::TracePacket(const char *packetlog, size_t size, ...)
{
	std::string trace = db.LoadServerSettings("options", "trace").c_str();
	std::transform(trace.begin(), trace.end(), trace.begin(), ::toupper);
	if (trace == "TRUE")
	{
		LogPacket(log_network_trace, packetlog, size);
	}
}

void ErrorLog::WorldTrace(const char *message, ...)
{
	std::string worldtrace = db.LoadServerSettings("options", "world_trace").c_str();
	std::transform(worldtrace.begin(), worldtrace.end(), worldtrace.begin(), ::toupper);
	if (worldtrace == "TRUE")
	{
		Log(log_network_trace, message);
	}
}

bool ErrorLog::DumpIn()
{
	std::string dumpin = db.LoadServerSettings("options", "dump_packets_in").c_str();
	std::transform(dumpin.begin(), dumpin.end(), dumpin.begin(), ::toupper);
	if (dumpin == "TRUE")
	{
		return true;
	}
	return false;
}

bool ErrorLog::DumpOut()
{
	std::string dumpout = db.LoadServerSettings("options", "dump_packets_in").c_str();
	std::transform(dumpout.begin(), dumpout.end(), dumpout.begin(), ::toupper);
	if (dumpout == "TRUE")
	{
		return true;
	}
	return false;
}

void ErrorLog::Log(eqLogType type, const char *message, ...)
{
	if(type >= _log_largest_type)
	{
		return;
	}

	va_list argptr;
	char *buffer = new char[4096];
	va_start(argptr, message);
	vsnprintf(buffer, 4096, message, argptr);
	va_end(argptr);

	time_t m_clock;
	struct tm *m_time;
	time(&m_clock);
	m_time = localtime(&m_clock);

	log_mutex->lock();

#ifdef _WINDOWS
	//system("color 0F");
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD color = WHITE;

	if (type == log_debug)			{ color = LIGHTGRAY; }
	if (type == log_error)			{ color = RED; }
	if (type == log_database)		{ color = LIGHTGREEN; }
	if (type == log_database_trace)	{ color = GREEN; }
	if (type == log_database_error)	{ color = LIGHTRED; }
	if (type == log_network)		{ color = YELLOW; }
	if (type == log_network_trace)	{ color = LIGHTBLUE; }
	if (type == log_network_error)	{ color = LIGHTRED; }
	if (type == log_world)			{ color = YELLOW; }
	if (type == log_world_trace)	{ color = LIGHTBLUE; }
	if (type == log_world_error)	{ color = LIGHTRED; }
	if (type == log_client)			{ color = YELLOW; }
	if (type == log_client_trace)	{ color = LIGHTBLUE; }
	if (type == log_client_error)	{ color = LIGHTRED; }

	std::string eqLogConsoleFormat = std::string(eqLogTypes[type]);
	std::string addspace = " ";

	while (eqLogConsoleFormat.length() < 14)
	{
		eqLogConsoleFormat += addspace;
	}

	CONSOLE_FONT_INFOEX info = { 0 };
	info.cbSize = sizeof(info);
	info.dwFontSize.Y = 12; // leave X as zero
	info.FontWeight = FW_NORMAL;
	wcscpy(info.FaceName, L"Lucida Console");
	SetCurrentConsoleFontEx(hStdOut, NULL, &info);
	SetConsoleTextAttribute(hStdOut, color);
	printf("[ %s ][ %02d.%02d.%02d - %02d:%02d:%02d ] %s\n",
		eqLogConsoleFormat.c_str(),
		m_time->tm_mon + 1,
		m_time->tm_mday,
		m_time->tm_year % 100,
		m_time->tm_hour,
		m_time->tm_min,
		m_time->tm_sec,
		buffer);
	SetConsoleTextAttribute(hStdOut, color);
#else

	printf("[ %s ][ %02d.%02d.%02d - %02d:%02d:%02d ] %s\n",
		eqLogTypes[type],
		m_time->tm_mon+1,
		m_time->tm_mday,
		m_time->tm_year%100,
		m_time->tm_hour,
		m_time->tm_min,
		m_time->tm_sec,
		buffer);
#endif

	if(error_log)
	{
		fprintf(error_log, "[ %s ] [ %02d.%02d.%02d - %02d:%02d:%02d ] %s\n",
			eqLogTypes[type],
			m_time->tm_mon+1,
			m_time->tm_mday,
			m_time->tm_year%100,
			m_time->tm_hour,
			m_time->tm_min,
			m_time->tm_sec,
			buffer);
		fflush(error_log);
	}

	log_mutex->unlock();
	delete[] buffer;
}

void ErrorLog::LogPacket(eqLogType type, const char *data, size_t size)
{
	if(type >= _log_largest_type)
	{
		return;
	}

	log_mutex->lock();
	time_t m_clock;
	struct tm *m_time;
	time(&m_clock);
	m_time = localtime(&m_clock);

	log_mutex->lock();
	printf("[%s] [%02d.%02d.%02d - %02d:%02d:%02d] dumping packet of size %u:\n",
		eqLogTypes[type],
		m_time->tm_mon+1,
		m_time->tm_mday,
		m_time->tm_year%100,
		m_time->tm_hour,
		m_time->tm_min,
		m_time->tm_sec,
		(unsigned int)size);

	if(error_log)
	{
		fprintf(error_log, "[%s] [%02d.%02d.%02d - %02d:%02d:%02d] dumping packet of size %u\n",
			eqLogTypes[type],
			m_time->tm_mon+1,
			m_time->tm_mday,
			m_time->tm_year%100,
			m_time->tm_hour,
			m_time->tm_min,
			m_time->tm_sec,
			(unsigned int)size);
	}

	char ascii[17]; //16 columns + 1 null term
	memset(ascii, 0, 17);

	size_t j = 0;
	size_t i = 0;
	for(; i < size; ++i)
	{
		if(i % 16 == 0)
		{
			if(i != 0)
			{
				printf(" | %s\n", ascii);
				if(error_log)
				{
					fprintf(error_log, " | %s\n", ascii);
				}
			}
			printf("%.4u: ", (unsigned int)i);
			memset(ascii, 0, 17);
			j = 0;
		}
		else if(i % 8 == 0)
		{
			printf("- ");
			if(error_log)
			{
				fprintf(error_log, "- ");
			}
		}

		printf("%02X ", (unsigned int)data[i]);
		if(error_log)
		{
			fprintf(error_log, "%02X ", (unsigned int)data[i]);
		}

		if(data[i] >= 32 && data[i] < 127)
		{
			ascii[j++] = data[i];
		}
		else
		{
			ascii[j++] = '.';
		}
	}

	size_t k = (i - 1) % 16;
	if(k < 8)
	{
		printf("  ");
		if(error_log)
		{
			fprintf(error_log, "  ");
		}
	}

	for(size_t h = k + 1; h < 16; ++h)
	{
		printf("   ");
		if(error_log)
		{
			fprintf(error_log, "   ");
		}
	}

	printf(" | %s\n", ascii);
	if(error_log)
	{
		fprintf(error_log, " | %s\n", ascii);
		fflush(error_log);
	}
	log_mutex->unlock();
}
