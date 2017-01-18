
#include "../global_define.h"
#include "patches.h"

#include "mac.h"
#include "evolution.h"
#include "trilogy.h"

void RegisterAllPatches(EQStreamIdentifier &into) {
	Trilogy::Register(into);
	Mac::Register(into);
	Evolution::Register(into);

}

void ReloadAllPatches() {
	Trilogy::Reload();
	Mac::Reload();
	Evolution::Reload();
}

