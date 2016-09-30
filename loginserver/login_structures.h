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
#ifndef EQEMU_LOGINSTRUCTURES_H
#define EQEMU_LOGINSTRUCTURES_H

#pragma pack(1)

//mac
struct ServerList_Struct {
	uint16	numservers;
	uint8	padding[2];
	uint8	showusercount; // 0xFF = show numbers, 0x0 = show "UP"
	uchar	data[0];
};

//trilogy
struct ServerList_Trilogy_Struct {
	uint8	numservers;
	uint8	unknown1;
	uint8	unknown2;
	uint8	showusercount; // 0xFF = show numbers, 0x0 = show "UP"
	uchar	data[0];
};

//mac
struct ServerListServerFlags_Struct {
	uint8 greenname;
	int32 flags; // if 0x8 then server is hidden on list
	int32 worldid;
	uint32 usercount;
};

//trilogy
struct ServerListServerFlags_Trilogy_Struct {
	int8 greenname;
	int32 usercount;
	int8 unknown[8];
};

struct ServerListEndFlags_Struct {
	uint32 admin;
	uint8 zeroes_a[8];
	uint8 kunark;
	uint8 velious;
	uint8 zeroes_b[11];
};

struct SessionId_Struct {
	char	session_id[10];
	char	unused[7];
	uint32	unknown; // legends? dunno, seen a 4 here, so gonna use that for now
};

struct LoginServerInfo_Struct {
	uint8	crypt[40];
	uint8	unknown[0];	// in here is the server name you just logged out of, variable length
};

struct Update_Struct {
	uint32 flag;
	char description[0];
};

struct LoginCrypt_struct {
	char	username[20];
	char	password[20];
};

struct PlayEverquestRequest_Struct
{
	uint16 Sequence;
	uint32 Unknown1;
	uint32 Unknown2;
	uint32 ServerNumber;
};

struct PlayEverquestResponse_Struct {
	uint8 Sequence;
	uint8 Unknown1[9];
	uint8 Allowed;
	uint16 Message;
	uint8 Unknown2[3];
	uint32 ServerNumber;
};

struct Registration_Struct {
	/*0000*/	char	ChatHandel[20];
	/*0020*/	char	ChatHandel2[17];	// *shrug*
	/*0037*/	char	LastName[32];
	/*0069*/	char	FirstName[32];
	/*0101*/	char	MiddleInitial[4];
	/*0105*/	char	Address[128];
	/*0233*/	char	City[64];
	/*0297*/	char	State[64];			// 2 character postal code
	/*0361*/	char	ZIP[32];
	/*0393*/	char	Country[32];
	/*0425*/	char	PhoneDay[40];
	/*0465*/	char	PhoneEve[40];
	/*0505*/	char	EMail[128];
	/*0633*/	char	WorkSchool[64];
	/*0697*/	char	WhereDidYouHear[64];	// looks like spaces are changed to +'s (like in URLs)
	/*0761*/	char	_2dVideoCard[32];		// looks like spaces are changed to +'s (like in URLs)
	/*0793*/	char	ISP[64];
	/*0857*/	char	AccountKey[26];
	/*0883*/	char	DOB[12];
	/*0895*/	int8	unknown895[16];
	/*0911*/	char	CPUSpeed[64];
	/*0975*/	char	CPUType[64];
	/*1039*/	char	MainMemory[64];
	/*1103*/	char	_3dVideoCard[64];
	/*1167*/	char	VideoMemory[64];
	/*1231*/	char	SoundCard[64];
	/*1295*/	char	Modem[64];
	/*1359*/	int8	unknown1359[256];
	/*1615*/	char	StationName[20];
	/*1635*/	char	Password[21];
	/*1655*/	int8	Checksum[4];
};

struct Register_SendPrice_Struct {
	int8 unknown000[10]; // 0x01 0f 44 16 62 01 01 1b 00
	int8 pricingoptions; // 0x03
	int8 padding[3];
	char price_name1[7]; // Normally: 1-Month
	char price_name2[7]; // Normally: 3-Month
	char price_name3[7]; // Normally: 6-Month
	char price_num1[7]; // Normally: $9.89
	char price_num2[7]; // Normally: $25.89
	char price_num3[7]; // Normally: $49.89
};

static const unsigned char FailedLoginResponseData[] = {
	0xf6, 0x85, 0x9c, 0x23, 0x57, 0x7e, 0x3e, 0x55, 0xb3, 0x4c, 0xf8, 0xc8, 0xcb, 0x77, 0xd5, 0x16,
	0x09, 0x7a, 0x63, 0xdc, 0x57, 0x7e, 0x3e, 0x55, 0xb3, 0x4c, 0xf8, 0xc8, 0xcb, 0x77, 0xd5, 0x16,
	0x09, 0x7a, 0x63, 0xdc, 0x57, 0x7e, 0x3e, 0x55, 0xb3 };

#pragma pack()

#endif