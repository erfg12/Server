#ifndef CLIENTVERSIONS_H
#define CLIENTVERSIONS_H

#include "types.h"

static const uint32 BIT_Trilogy = 1;
static const uint32 BIT_MacPC = 2;
static const uint32 BIT_MacIntel = 4;
static const uint32 BIT_MacPPC = 8;
static const uint32 BIT_Evolution = 16;

static const uint32 BIT_Mac = 14;
static const uint32 BIT_AllClients = 0xFFFFFFFF;

typedef enum
{
	EQClientUnknown = 0,
	EQClientTrilogy,
	EQClientMac,
	EQClientEvolution,
	_EQClientCount,			// place new clients before this point (preferably, in release/attribute order)

	// Values below are not implemented, as yet...

	EmuNPC = _EQClientCount,
	EmuPet,

	_EmuClientCount			// array size for EQLimits
} EQClientVersion;

static const char* EQClientVersionName(EQClientVersion version)
{
	switch (version)
	{
	case EQClientUnknown:
		return "EQClientUnknown";
	case EQClientTrilogy:
		return "EQClientTrilogy";
	case EQClientMac:
		return "EQClientMac";
	case EQClientEvolution:
		return "EQClientEvolution";
	case EmuNPC:
		return "EmuNPC";
	case EmuPet:
		return "EmuPet";
	default:
		return "ERROR: Invalid EQClientVersion";
	};
}

#endif /* CLIENTVERSIONS_H */
