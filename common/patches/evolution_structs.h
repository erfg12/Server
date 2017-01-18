#ifndef EVOLUTION_STRUCTS_H_
#define EVOLUTION_STRUCTS_H_

#include <string>

namespace Evolution {
	namespace structs {


//paste contents of eq_packet_structs.h here...

		
/*
** Compiler override to ensure
** byte aligned structures
*/
#pragma pack(1)

// Valus of weapon types
enum SKILLTYPE : uint8
{
	SKILLTYPE_1HSLASHING = 0x00,
	SKILLTYPE_2HSLASHING = 0x01,
	SKILLTYPE_PIERCING = 0x02,
	SKILLTYPE_1HBLUNT = 0x03,
	SKILLTYPE_2HBLUNT = 0x04,
	SKILLTYPE_2HPIERCING = 0x23
};

// Dark-Prince 22/12/2007
// Added this struct for eqemu and started eimplimentation ProcessOP_SendLoginInfo
//TODO: confirm everything in this struct
struct LoginInfo_Struct {
	/*000*/	char	AccountName[127];	// Length confirmed - Dark-Prince 22/12/2007
	/*064*/	uint8	Password[24];		// Length confirmed - Dark-Prince 22/12/2007
	/*189*/	uint8	unknown189[19];		//TODO: Find out wtf is this one and confirm size
	/*196*/
};

struct SetDataRate_Struct 
{
	float newdatarate;	// Comment: 
};


	};	//end namespace structs
};	//end namespace EVOLUTION

#endif /*EVOLUTION_STRUCTS_H_*/
