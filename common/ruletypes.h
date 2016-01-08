


#ifndef RULE_CATEGORY
#define RULE_CATEGORY(name)
#endif
#ifndef RULE_INT
#define RULE_INT(cat, rule, default_value)
#endif
#ifndef RULE_REAL
#define RULE_REAL(cat, rule, default_value)
#endif
#ifndef RULE_BOOL
#define RULE_BOOL(cat, rule, default_value)
#endif
#ifndef RULE_CATEGORY_END
#define RULE_CATEGORY_END()
#endif




RULE_CATEGORY(Character)
RULE_BOOL(Character, CanCreate, true)
RULE_INT ( Character, MaxLevel, 65 )
RULE_BOOL ( Character, PerCharacterQglobalMaxLevel, false) // This will check for qglobal 'CharMaxLevel' character qglobal (Type 5), if player tries to level beyond that point, it will not go beyond that level
RULE_INT ( Character, MaxExpLevel, 0 ) //Sets the Max Level attainable via Experience
RULE_INT ( Character, DeathExpLossLevel, 10 )	// Any level greater than this will lose exp on death
RULE_INT ( Character, DeathExpLossMaxLevel, 255 )	// Any level greater than this will no longer lose exp on death
RULE_INT ( Character, DeathItemLossLevel, 10 )
RULE_INT ( Character, CorpseDecayTimeMS, 604800000 ) // 7 days
RULE_INT ( Character, EmptyCorpseDecayTimeMS, 10800000 ) // 3 hours
RULE_INT ( Character, CorpseResTimeMS, 10800000 ) // time before cant res corpse(3 hours)
RULE_INT ( Character, DuelCorpseResTimeMS, 600000 ) // time before cant res corpse after a duel (10 minutes)
RULE_INT ( Character, CorpseOwnerOnlineTimeMS, 30000 ) // how often corpse will check if its owner is online
RULE_BOOL( Character, LeaveCorpses, true )
RULE_BOOL( Character, LeaveNakedCorpses, true )
RULE_INT ( Character, MaxDraggedCorpses, 2 )
RULE_REAL( Character, DragCorpseDistance, 400) // If the corpse is <= this distance from the player, it won't move
RULE_REAL( Character, ExpMultiplier, 1.0 )
RULE_REAL( Character, AAExpMultiplier, 1.0 )
RULE_REAL( Character, GroupExpMultiplier, 1.0 )
RULE_REAL( Character, RaidExpMultiplier, 0.2 )
RULE_BOOL ( Character, SmoothEXPLoss, true)
RULE_REAL ( Character, EXPLossMultiplier, 1.0)
RULE_INT ( Character, AutosaveIntervalS, 240 )	//0=disabled
RULE_INT ( Character, HPRegenMultiplier, 100)
RULE_INT ( Character, ManaRegenMultiplier, 100)
RULE_INT ( Character, EnduranceRegenMultiplier, 100)
RULE_INT ( Character, ConsumptionValue, 6000) //How "full" each consumption of food or drink will make the player. EQEmu default is 6000.
RULE_REAL ( Character, FoodLossPerUpdate, 75) // How much food/water you lose per stamina update
RULE_INT ( Character, FamishedLevel, 120) // When famished reaches this value, we lose endurance and stop regen on HP/mana
RULE_BOOL( Character, HealOnLevel, false)
RULE_BOOL( Character, ManaOnLevel, false)
RULE_BOOL( Character, FeignKillsPet, false)
RULE_INT(Character, ItemManaRegenCap, 15)
RULE_INT(Character, ItemHealthRegenCap, 35)
RULE_INT(Character, ItemDamageShieldCap, 30)
RULE_INT(Character, ItemATKCap, 250)
RULE_INT(Character, ItemHealAmtCap, 250)
RULE_INT ( Character, ItemEnduranceRegenCap, 15)
RULE_INT ( Character, ItemExtraDmgCap, 150) // Cap for bonuses to melee skills like Bash, Frenzy, etc
RULE_INT ( Character, HasteCap, 100) // Haste cap for non-v3(overhaste) haste.
RULE_BOOL ( Character, BindAnywhere, false)
RULE_INT ( Character, RestRegenPercent, 0) // Set to >0 to enable rest state bonus HP and mana regen.
RULE_INT ( Character, RestRegenTimeToActivate, 30) // Time in seconds for rest state regen to kick in.
RULE_INT ( Character, RestRegenRaidTimeToActivate, 300) // Time in seconds for rest state regen to kick in with a raid target.
RULE_BOOL ( Character, RestRegenEndurance, false) // Whether rest regen will work for endurance or not.
RULE_INT ( Character, KillsPerGroupLeadershipAA, 250) // Number of dark blues or above per Group Leadership AA
RULE_INT ( Character, KillsPerRaidLeadershipAA, 250) // Number of dark blues or above per Raid Leadership AA
RULE_INT ( Character, MaxFearDurationForPlayerCharacter, 4) //4 tics, each tic calculates every 6 seconds.
RULE_INT ( Character, MaxCharmDurationForPlayerCharacter, 15)
RULE_INT ( Character, BaseHPRegenBonusRaces, 4352)	//a bitmask of race(s) that receive the regen bonus. Iksar (4096) & Troll (256) = 4352. see common/races.h for the bitmask values
RULE_BOOL ( Character, ItemCastsUseFocus, false) // If true, this allows item clickies to use focuses that have limited max levels on them
RULE_INT ( Character, MinStatusForNoDropExemptions, 80) // This allows status x and higher to trade no drop items.
RULE_INT ( Character, SkillCapMaxLevel, 65 )	// Sets the Max Level used for Skill Caps (from skill_caps table). -1 makes it use MaxLevel rule value. It is set to 75 because PEQ only has skillcaps up to that level, and grabbing the players' skill past 75 will return 0, breaking all skills past that level. This helps servers with obsurd level caps (75+ level cap) function without any modifications.
RULE_INT ( Character, StatCap, 0 )
RULE_BOOL ( Character, CheckCursorEmptyWhenLooting, true ) // If true, a player cannot loot a corpse (player or NPC) with an item on their cursor
RULE_BOOL ( Character, MaintainIntoxicationAcrossZones, true ) // If true, alcohol effects are maintained across zoning and logging out/in.
RULE_BOOL ( Character, EnableDiscoveredItems, false ) // If enabled, it enables EVENT_DISCOVER_ITEM and also saves character names and timestamps for the first time an item is discovered.
RULE_BOOL ( Character, KeepLevelOverMax, false) // Don't delevel a character that has somehow gone over the level cap
RULE_INT ( Character, BaseInstrumentSoftCap, 36) // Softcap for instrument mods, 36 commonly referred to as "3.6" as well.
RULE_INT ( Character, BaseRunSpeedCap, 150) // Base Run Speed Cap, on live it's 158% which will give you a runspeed of 1.580 hard capped to 225.
RULE_REAL (Character, BaseRunSpeed, 0.7)
RULE_REAL (Character, BaseWalkSpeed, 0.46)
RULE_REAL(Character, EnvironmentDamageMulipliter, 1)
RULE_BOOL(Character, ForageNeedFoodorDrink, false)
RULE_BOOL(Character, ForageCommonFoodorDrink, false)
RULE_BOOL (Character, DisableAAs, true) // Disables server side AA support, since the client allows some AA activity through even with a pre-Luclin expansion set.
RULE_CATEGORY_END()

RULE_CATEGORY( Guild )
RULE_INT ( Guild, MaxMembers, 2048 )
RULE_BOOL ( Guild, PlayerCreationAllowed, false)	// Allow players to create a guild using the window in Underfoot+
RULE_INT ( Guild, PlayerCreationLimit, 1)		// Only allow use of the UF+ window if the account has < than this number of guild leaders on it
RULE_INT ( Guild, PlayerCreationRequiredStatus, 0)	// Required admin status.
RULE_INT ( Guild, PlayerCreationRequiredLevel, 0)	// Required Level of the player attempting to create the guild.
RULE_INT ( Guild, PlayerCreationRequiredTime, 0)	// Required Time Entitled On Account (in Minutes) to create the guild.
RULE_INT ( Guild, MaxGuilds, 32)
RULE_CATEGORY_END()

RULE_CATEGORY( Skills )
RULE_INT ( Skills, MaxTrainTradeskills, 21 )
RULE_BOOL ( Skills, UseLimitTradeskillSearchSkillDiff, true )
RULE_INT ( Skills, MaxTradeskillSearchSkillDiff, 50 )
RULE_INT ( Skills, MaxTrainSpecializations, 50 )	// Max level a GM trainer will train casting specializations
RULE_INT ( Skills, LangSkillUpModifier, 70) //skill ups for skills with value under 100
RULE_REAL ( Skills, TradeskillSkillUpModifier, 0) //0 was the setting for takp rule.
RULE_REAL ( Skills, SkillUpModifier, 1.0)
RULE_REAL ( Skills, HighStatSkillUpModifier, 0.75)
RULE_CATEGORY_END()

RULE_CATEGORY( Pets )
RULE_REAL( Pets, AttackCommandRange, 150 )
RULE_BOOL( Pets, UnTargetableSwarmPet, false )
RULE_BOOL( Pets, SwarmPetNotTargetableWithHotKey, false ) //On SOF+ clients this a semi-hack to make swarm pets not F8 targetable.
RULE_CATEGORY_END()

RULE_CATEGORY(GM)
RULE_INT(GM, GMWhoList, 80)
RULE_INT(GM, NoCombatLow, 2)
RULE_INT(GM, NoCombatHigh, 2)	// effectively disables this for now
RULE_INT ( GM, MinStatusToUseGMItem, 80)
RULE_INT ( GM, MinStatusToZoneAnywhere, 250 )
RULE_CATEGORY_END()

RULE_CATEGORY( World )
RULE_INT ( World, ZoneAutobootTimeoutMS, 60000 )
RULE_INT ( World, ClientKeepaliveTimeoutMS, 65000 )
RULE_BOOL ( World, UseBannedIPsTable, false ) // Toggle whether or not to check incoming client connections against the Banned_IPs table. Set this value to false to disable this feature.
RULE_INT ( World, MinOfflineTimeToReturnHome, 21600) // 21600 seconds is 6 Hours
RULE_INT ( World, MaxClientsPerIP, -1 ) // Maximum number of clients allowed to connect per IP address if account status is < AddMaxClientsStatus. Default value: -1 (feature disabled)
RULE_INT ( World, ExemptMaxClientsStatus, -1 ) // Exempt accounts from the MaxClientsPerIP and AddMaxClientsStatus rules, if their status is >= this value. Default value: -1 (feature disabled)
RULE_INT ( World, AddMaxClientsPerIP, -1 ) // Maximum number of clients allowed to connect per IP address if account status is < ExemptMaxClientsStatus. Default value: -1 (feature disabled)
RULE_INT ( World, AddMaxClientsStatus, -1 ) // Accounts with status >= this rule will be allowed to use the amount of accounts defined in the AddMaxClientsPerIP. Default value: -1 (feature disabled)
RULE_BOOL ( World, MaxClientsSetByStatus, false) // If True, IP Limiting will be set to the status on the account as long as the status is > MaxClientsPerIP
RULE_BOOL ( World, ClearTempMerchantlist, false) // Clears temp merchant items when world boots.
RULE_BOOL ( World, DeleteStaleCorpeBackups, true) // Deletes stale corpse backups older than 2 weeks.
RULE_INT ( World, AccountSessionLimit, 1 ) //Max number of characters allowed on at once from a single account (-1 is disabled)
RULE_INT ( World, ExemptAccountLimitStatus, 100 ) //Min status required to be exempt from multi-session per account limiting (-1 is disabled)
RULE_BOOL ( World, GMAccountIPList, false) // Check ip list against GM Accounts, AntiHack GM Accounts.
RULE_INT ( World, MinGMAntiHackStatus, 1 ) //Minimum GM status to check against AntiHack list
RULE_INT ( World, SoFStartZoneID, -1 ) //Sets the Starting Zone for SoF Clients separate from Titanium Clients (-1 is disabled)
RULE_INT ( World, PVPSettings, 0) // Sets the PVP settings for the server, 1 = Rallos Zek RuleSet, 2 = Tallon/Vallon Zek Ruleset, 4 = Sullon Zek Ruleset, 6 = Discord Ruleset, anything above 6 is the Discord Ruleset without the no-drop restrictions removed. TODO: Edit IsAttackAllowed in Zone to accomodate for these rules.
RULE_INT (World, FVNoDropFlag, 0) // Sets the Firiona Vie settings on the client. If set to 2, the flag will be set for GMs only, allowing trading of no-drop items.
RULE_BOOL (World, IPLimitDisconnectAll, false)
RULE_INT (World, TellQueueSize, 20) 
RULE_BOOL (World, UseDBUpdate, false) //Automatic Database Upgrade Script
RULE_BOOL (World, AdjustRespawnTimes, true) //Determines if spawntimes with a boot time variable take effect or not. Set to false in the db for emergency patches.
RULE_INT (World, BootHour, 0) // Sets the in-game hour world will set when it first boots. 0-24 are valid options, where 0 disables this rule.
RULE_CATEGORY_END()

RULE_CATEGORY( Zone )
RULE_INT ( Zone, NPCPositonUpdateTicCount, 32 ) //ms between intervals of sending a position update to the entire zone.
RULE_INT ( Zone, ClientLinkdeadMS, 180000) //the time a client remains link dead on the server after a sudden disconnection
RULE_INT ( Zone, GraveyardTimeMS, 1200000) //ms time until a player corpse is moved to a zone's graveyard, if one is specified for the zone
RULE_BOOL ( Zone, EnableShadowrest, true ) // enables or disables the shadowrest zone feature for player corpses. Default is turned on.
RULE_BOOL ( Zone, UsePlayerCorpseBackups, true) // Keeps backups of player corpses.
RULE_INT ( Zone, MQWarpExemptStatus, -1 ) // Required status level to exempt the MQWarpDetector. Set to -1 to disable this feature.
RULE_INT ( Zone, MQZoneExemptStatus, -1 ) // Required status level to exempt the MQZoneDetector. Set to -1 to disable this feature.
RULE_INT ( Zone, MQGateExemptStatus, -1 ) // Required status level to exempt the MQGateDetector. Set to -1 to disable this feature.
RULE_INT ( Zone, MQGhostExemptStatus, -1 ) // Required status level to exempt the MGhostDetector. Set to -1 to disable this feature.
RULE_BOOL ( Zone, EnableMQWarpDetector, false ) // Enable the MQWarp Detector. Set to False to disable this feature.
RULE_BOOL ( Zone, EnableMQZoneDetector, true ) // Enable the MQZone Detector. Set to False to disable this feature.
RULE_BOOL ( Zone, EnableMQGateDetector, true ) // Enable the MQGate Detector. Set to False to disable this feature.
RULE_BOOL ( Zone, EnableMQGhostDetector, true ) // Enable the MQGhost Detector. Set to False to disable this feature.
RULE_REAL ( Zone, MQWarpDetectionDistanceFactor, 9.0) //clients move at 4.4 about if in a straight line but with movement and to acct for lag we raise it a bit
RULE_BOOL ( Zone, MarkMQWarpLT, false )
RULE_INT ( Zone, AutoShutdownDelay, 5000 ) //How long a dynamic zone stays loaded while empty
RULE_INT ( Zone, PEQZoneReuseTime, 900 )	//How long, in seconds, until you can reuse the #peqzone command.
RULE_INT ( Zone, PEQZoneDebuff1, 4454 )		//First debuff casted by #peqzone Default is Cursed Keeper's Blight.
RULE_INT ( Zone, PEQZoneDebuff2, 2209 )		//Second debuff casted by #peqzone Default is Tendrils of Apathy.
RULE_BOOL ( Zone, UsePEQZoneDebuffs, true )	//Will determine if #peqzone will debuff players or not when used.
RULE_REAL ( Zone, HotZoneBonus, 0.5 )
RULE_INT ( Zone, ReservedInstances, 30 ) //Will reserve this many instance ids for globals... probably not a good idea to change this while a server is running.
RULE_BOOL ( Zone, LevelBasedEXPMods, false) // Allows you to use the level_exp_mods table in consideration to your players EXP hits
RULE_INT ( Zone, WeatherTimer, 600) // Weather timer when no duration is available
RULE_INT (Zone, SpawnEventMin, 5) // When strict is set in spawn_events, specifies the max EQ minutes into the trigger hour a spawn_event will fire.
RULE_REAL ( Zone, GroupEXPRange, 500 )
RULE_BOOL ( Zone, IdleWhenEmpty, true) // After timer is expired, if zone is empty it will idle. Boat zones are excluded, as this will break boat functionality.
RULE_INT ( Zone, IdleTimer, 600000) // 10 minutes
RULE_INT ( Zone, BoatDistance, 50) //In zones where boat name is not set in the PP, this is how far away from the boat the client must be to move them to the boat's current location.
RULE_CATEGORY_END()

RULE_CATEGORY( AlKabor )
RULE_BOOL( AlKabor, AllowPetPull, false) // Allow Green Pet Pull (AK behavior is true)
RULE_BOOL( AlKabor, AllowTickSplit, false) //AK behavior is true
RULE_BOOL ( AlKabor, StripBuffsOnLowHP, true) //AK behavior is true
RULE_BOOL ( AlKabor, OutOfRangeGroupXPBonus, true) //AK behavior is true
RULE_BOOL ( AlKabor, GroupEXPBonuses, false) //AK behavior is true
RULE_BOOL ( AlKabor, Count6thGroupMember, false) //AK behavior is false
RULE_BOOL ( AlKabor, GreensGiveXPToGroup, true) //AK behavior is true
RULE_BOOL( AlKabor, AllowCharmPetRaidTanks, true) // AK behavior is true.  If false, NPCs will ignore charmed pets once MaxEntitiesCharmTanks players get on an NPC's hate list as per April 2003 patch.
RULE_INT( AlKabor, MaxEntitiesCharmTanks, 8) // If AllowCharmPetRaidTanks is false, this is the max number of entities on an NPC's hate list before the NPC will ignore charmed pets.  April 2003 patch set this to 4 on Live.
RULE_CATEGORY_END()


RULE_CATEGORY( Map )
//enable these to help prevent mob hopping when they are pathing
RULE_BOOL ( Map, FixPathingZWhenLoading, true )		//increases zone boot times a bit to reduce hopping.
RULE_BOOL ( Map, FixPathingZAtWaypoints, false )	//alternative to `WhenLoading`, accomplishes the same thing but does it at each waypoint instead of once at boot time.
RULE_BOOL ( Map, FixPathingZWhenMoving, true )		//very CPU intensive, but helps hopping with widely spaced waypoints.
RULE_BOOL ( Map, FixPathingZOnSendTo, true )		//try to repair Z coords in the SendTo routine as well.
RULE_REAL ( Map, FixPathingZMaxDeltaMoving, 20 )	//at runtime while pathing: max change in Z to allow the BestZ code to apply.
RULE_REAL ( Map, FixPathingZMaxDeltaWaypoint, 20 )	//at runtime at each waypoint: max change in Z to allow the BestZ code to apply.
RULE_REAL ( Map, FixPathingZMaxDeltaSendTo, 20 )	//at runtime in SendTo: max change in Z to allow the BestZ code to apply.
RULE_REAL ( Map, FixPathingZMaxDeltaLoading, 45 )	//while loading each waypoint: max change in Z to allow the BestZ code to apply.
RULE_INT ( Map, FindBestZHeightAdjust, 1)		// Adds this to the current Z before seeking the best Z position. If this is too high, mobs bounce when pathing.
RULE_REAL ( Map, BestZSizeMax, 10.0) // When calculating bestz using size, this is our size cap. Setting this too high causes dragons and giants to hop.
RULE_REAL ( Map, BestZMultiplier, 0.625) // This is our multiplier for the bestz calculation.
RULE_CATEGORY_END()

RULE_CATEGORY( Pathing )
// Some of these rules may benefit by being made into columns in the zone table,
// for instance, in dungeons, the min LOS distances could be substantially lowered.
RULE_BOOL ( Pathing, Aggro, true )		// Enable pathing for aggroed mobs.
RULE_BOOL ( Pathing, AggroReturnToGrid, true )	// Enable pathing for aggroed roaming mobs returning to their previous waypoint.
RULE_BOOL ( Pathing, Guard, true )		// Enable pathing for mobs moving to their guard point.
RULE_BOOL ( Pathing, Find, true )		// Enable pathing for FindPerson requests from the client.
RULE_BOOL ( Pathing, Fear, true )		// Enable pathing for fear
RULE_REAL ( Pathing, ZDiffThreshold, 10)	// If a mob las LOS to it's target, it will run to it if the Z difference is < this.
RULE_INT ( Pathing, LOSCheckFrequency, 1000)	// A mob will check for LOS to it's target this often (milliseconds).
RULE_INT ( Pathing, RouteUpdateFrequencyShort, 1000)	// How often a new route will be calculated if the target has moved.
RULE_INT ( Pathing, RouteUpdateFrequencyLong, 5000)	// How often a new route will be calculated if the target has moved.
// When a path has a path node route and it's target changes position, if it has RouteUpdateFrequencyNodeCount or less nodes to go on it's
// current path, it will recalculate it's path based on the RouteUpdateFrequencyShort timer, otherwise it will use the
// RouteUpdateFrequencyLong timer.
RULE_INT ( Pathing, RouteUpdateFrequencyNodeCount, 5)
RULE_REAL ( Pathing, MinDistanceForLOSCheckShort, 40000) // (NoRoot). While following a path, only check for LOS to target within this distance.
RULE_REAL ( Pathing, MinDistanceForLOSCheckLong, 1000000) // (NoRoot). Min distance when initially attempting to acquire the target.
RULE_INT ( Pathing, MinNodesLeftForLOSCheck, 4)	// Only check for LOS when we are down to this many path nodes left to run.
// This next rule was put in for situations where the mob and it's target may be on different sides of a 'hazard', e.g. a pit
// If the mob has LOS to it's target, even though there is a hazard in it's way, it may break off from the node path and run at
// the target, only to later detect the hazard and re-acquire a node path. Depending upon the placement of the path nodes, this
// can lead to the mob looping. The rule is intended to allow the mob to at least get closer to it's target each time before
// checking LOS and trying to head straight for it.
RULE_INT ( Pathing, MinNodesTraversedForLOSCheck, 3)	// Only check for LOS after we have traversed this many path nodes.
RULE_INT ( Pathing, CullNodesFromStart, 1)		// Checks LOS from Start point to second node for this many nodes and removes first node if there is LOS
RULE_INT ( Pathing, CullNodesFromEnd, 1)		// Checks LOS from End point to second to last node for this many nodes and removes last node if there is LOS
RULE_REAL ( Pathing, CandidateNodeRangeXY, 400)		// When searching for path start/end nodes, only nodes within this range will be considered.
RULE_REAL ( Pathing, CandidateNodeRangeZ, 10)		// When searching for path start/end nodes, only nodes within this range will be considered.
RULE_CATEGORY_END()

RULE_CATEGORY( Watermap )
// enable these to use the water detection code. Requires Water Maps generated by awater utility
// WARNING: Bestz in water is the ocean floor, so if you have NPCs that float near the top (sharks, boats) enabling these rules will break them!
RULE_BOOL ( Watermap, CheckWaypointsInWaterWhenLoading, false ) // Does not apply BestZ as waypoints are loaded if they are in water
RULE_BOOL ( Watermap, CheckForWaterAtWaypoints, false)		// Check if a mob has moved into/out of water when at waypoints and sets flymode
RULE_BOOL ( Watermap, CheckForWaterWhenMoving, false)		// Checks if a mob has moved into/out of water each time it's loc is recalculated
RULE_BOOL ( Watermap, CheckForWaterOnSendTo, false)		// Checks if a mob has moved into/out of water on SendTo
RULE_BOOL ( Watermap, CheckForWaterWhenFishing, true)		// Only lets a player fish near water (if a water map exists for the zone)
RULE_REAL ( Watermap, FishingRodLength, 30)			// How far in front of player water must be for fishing to work
RULE_REAL ( Watermap, FishingLineLength, 28)			// If water is more than this far below the player, it is considered too far to fish
RULE_REAL ( Watermap, FishingLineExtension, 12)		// In some zones, setting a longer length causes the line to go underworld. This gives us a variable to work with in areas that need a longer line.
RULE_CATEGORY_END()

RULE_CATEGORY( Spells )
RULE_INT ( Spells, AutoResistDiff, 15)
RULE_REAL ( Spells, ResistChance, 2.0) //chance to resist given no resists and same level
RULE_REAL ( Spells, ResistMod, 0.40) //multiplier, chance to resist = this * ResistAmount
RULE_REAL ( Spells, PartialHitChance, 0.7) //The chance when a spell is resisted that it will partial hit.
RULE_REAL ( Spells, PartialHitChanceFear, 0.25) //The chance when a fear spell is resisted that it will partial hit.
RULE_INT ( Spells, BaseCritChance, 0) //base % chance that everyone has to crit a spell
RULE_INT ( Spells, BaseCritRatio, 100) //base % bonus to damage on a successful spell crit. 100 = 2x damage
RULE_INT ( Spells, WizCritLevel, 12) //level wizards first get spell crits
RULE_INT ( Spells, WizCritChance, 7) //wiz's crit chance, on top of BaseCritChance
RULE_INT ( Spells, WizCritRatio, 0) //wiz's crit bonus, on top of BaseCritRatio (should be 0 for Live-like)
RULE_INT ( Spells, ResistPerLevelDiff, 85) //8.5 resist per level difference.
RULE_INT ( Spells, TranslocateTimeLimit, 0) // If not zero, time in seconds to accept a Translocate.
RULE_INT ( Spells, SacrificeMinLevel, 46)	//first level Sacrifice will work on
RULE_INT ( Spells, SacrificeMaxLevel, 69)	//last level Sacrifice will work on
RULE_INT ( Spells, SacrificeItemID, 9963)	//Item ID of the item Sacrifice will return (defaults to an EE)
RULE_BOOL ( Spells, EnableSpellGlobals, false)	// If Enabled, spells check the spell_globals table and compare character data from the quest globals before allowing that spell to scribe with scribespells
RULE_INT ( Spells, MaxBuffSlotsNPC, 25)
RULE_INT ( Spells, MaxSongSlotsNPC, 10)
RULE_INT ( Spells, MaxTotalSlotsNPC, 36)
RULE_INT ( Spells, MaxTotalSlotsPET, 15)	// do not set this higher than 25 until the player profile is removed from the blob
RULE_INT ( Spells, MaxSpellDifficulty, 255)
RULE_INT ( Spells, ReflectType, 1) //0 = disabled, 1 = single target player spells only, 2 = all player spells, 3 = all single target spells, 4 = all spells
RULE_INT ( Spells, VirusSpreadDistance, 30) // The distance a viral spell will jump to its next victim
RULE_BOOL( Spells, LiveLikeFocusEffects, true) // Determines whether specific healing, dmg and mana reduction focuses are randomized
RULE_INT ( Spells, BaseImmunityLevel, 55) // The level that targets start to be immune to stun, fear and mez spells with a max level of 0.
RULE_BOOL ( Spells, NPCIgnoreBaseImmunity, true) // Whether or not NPCs get to ignore the BaseImmunityLevel for their spells.
RULE_REAL ( Spells, AvgSpellProcsPerMinute, 6.0) //Adjust rate for sympathetic spell procs
RULE_INT ( Spells, ResistFalloff, 67) //Max that level that will adjust our resist chance based on level modifiers
RULE_INT ( Spells, CharismaEffectiveness, 10) // Deterimes how much resist modification charisma applies to charm/pacify checks. Default 10 CHA = -1 resist mod.
RULE_INT ( Spells, CharismaEffectivenessCap, 200) // Deterimes how much resist modification charisma applies to charm/pacify checks. Default 10 CHA = -1 resist mod.
RULE_BOOL ( Spells, CharismaCharmDuration, false) // Allow CHA resist mod to extend charm duration.
RULE_INT ( Spells, CharmBreakCheckChance, 55) //Determines chance for a charm break check to occur each buff tick.
RULE_INT ( Spells, MaxCastTimeReduction, 50) //Max percent your spell cast time can be reduced by spell haste
RULE_INT ( Spells, RootBreakFromSpells, 55) //Chance for root to break when cast on.
RULE_INT ( Spells, DeathSaveCharismaMod, 3) //Determines how much charisma effects chance of death save firing.
RULE_INT ( Spells, DivineInterventionHeal, 8000) //Divine intervention heal amount.
RULE_BOOL ( Spells, AdditiveBonusValues, false) //Allow certain bonuses to be calculated by adding together the value from each item, instead of taking the highest value. (ie Add together all Cleave Effects)
RULE_BOOL ( Spells, UseCHAScribeHack, false) //ScribeSpells and TrainDiscs quest functions will ignore entries where field 12 is CHA.  What's the best way to do this?
RULE_BOOL ( Spells, BuffLevelRestrictions, true) //Buffs will not land on low level toons like live
RULE_INT ( Spells, RootBreakCheckChance, 70) //Determines chance for a root break check to occur each buff tick.
RULE_INT ( Spells, FearBreakCheckChance, 70) //Determines chance for a fear break check to occur each buff tick.
RULE_REAL (Spells, SuccorFailChance, 0.005) //Determines chance for a succor spell not to teleport an invidual player. 0.5% is the default
RULE_BOOL ( Spells, FocusCombatProcs, false) //Allow all combat procs to receive focus effects.
RULE_INT ( Spells, BaseFizzleChance, 20) //Base percentage you will fizzle. The chance then is modified by skill to go up or down.
RULE_BOOL ( Spells, PreNerfBardAEDoT, false) //Allow bard AOE dots to damage targets when moving.
RULE_INT ( Spells, AI_SpellCastFinishedFailRecast, 800) // AI spell recast time(MS) when an spell is cast but fails (ie stunned).
RULE_INT ( Spells, AI_EngagedNoSpellMinRecast, 500) // AI spell recast time(MS) check when no spell is cast while engaged. (min time in random)
RULE_INT ( Spells, AI_EngagedNoSpellMaxRecast, 1000) // AI spell recast time(MS) check when no spell is cast engaged.(max time in random)
RULE_INT ( Spells, AI_EngagedBeneficialSelfChance, 100) // Chance during first AI Cast check to do a beneficial spell on self.
RULE_INT ( Spells, AI_EngagedBeneficialOtherChance, 25) // Chance during second AI Cast check to do a beneficial spell on others.
RULE_INT ( Spells, AI_EngagedDetrimentalChance, 20) // Chance during third AI Cast check to do a determental spell on others.
RULE_INT ( Spells, AI_PursueNoSpellMinRecast, 500) // AI spell recast time(MS) check when no spell is cast while chasing target. (min time in random)
RULE_INT ( Spells, AI_PursueNoSpellMaxRecast, 2000) // AI spell recast time(MS) check when no spell is cast while chasing target. (max time in random)
RULE_INT ( Spells, AI_PursueDetrimentalChance, 90) // Chance while chasing target to cast a detrimental spell.
RULE_INT ( Spells, AI_IdleNoSpellMinRecast, 500) // AI spell recast time(MS) check when no spell is cast while idle. (min time in random)
RULE_INT ( Spells, AI_IdleNoSpellMaxRecast, 2000) // AI spell recast time(MS) check when no spell is cast while chasing target. (max time in random)
RULE_INT ( Spells, AI_IdleBeneficialChance, 100) // Chance while idle to do a beneficial spell on self or others.
RULE_BOOL ( Spells, SHDProcIDOffByOne, true) // pre June 2009 SHD spell procs were off by 1, they stopped doing this in June 2009 (so UF+ spell files need this false)
RULE_BOOL ( Spells, SwarmPetTargetLock, false) // Use old method of swarm pets target locking till target dies then despawning.
RULE_BOOL ( Spells, NPCUseRecastVariance, false) // If recast_delay is set to >= 0 NPCs will use a variance of between 1000 and 4000 ticks.
RULE_CATEGORY_END()

RULE_CATEGORY( Combat )
RULE_INT ( Combat, MeleeBaseCritChance, 0 ) //The base crit chance for non warriors, NOTE: This will apply to NPCs as well
RULE_INT ( Combat, WarBerBaseCritChance, 3 ) //The base crit chance for warriors and berserkers, only applies to clients
RULE_INT ( Combat, BerserkBaseCritChance, 6 ) //The bonus base crit chance you get when you're berserk
RULE_INT ( Combat, NPCBashKickLevel, 6 ) //The level that npcs can KICK/BASH
RULE_INT ( Combat, RogueCritThrowingChance, 25) //Rogue throwing crit bonus
RULE_INT ( Combat, RogueDeadlyStrikeChance, 80) //Rogue chance throwing from behind crit becomes a deadly strike
RULE_INT ( Combat, RogueDeadlyStrikeMod, 2) //Deadly strike modifier to crit damage
RULE_INT ( Combat, ClientBaseCritChance, 0 ) //The base crit chance for all clients, this will stack with warrior's/zerker's crit chance.
RULE_INT ( Combat, PetAttackMagicLevel, 30)
RULE_BOOL ( Combat, EnableFearPathing, true)
RULE_INT ( Combat, FleeHPRatio, 21)	  //HP % under which an NPC starts to flee.
RULE_REAL ( Combat, FleeMultiplier, 3.25) // Determines how quickly a NPC will slow down while fleeing. Decrease multiplier to slow NPC down quicker.
RULE_BOOL ( Combat, FleeIfNotAlone, false) // If false, mobs won't flee if other mobs are in combat with it.
RULE_BOOL ( Combat, AdjustSpecialProcPerMinute, true)  //Set PPM for special abilities like HeadShot (Live does this as of 4-14)
RULE_REAL ( Combat, AvgSpecialProcsPerMinute, 2.0) //Unclear what best value is atm.
RULE_REAL ( Combat, ArcheryHitPenalty, 0.25) //Archery has a hit penalty to try to help balance it with the plethora of long term +hit modifiers for it
RULE_INT ( Combat, MinRangedAttackDist, 25) //Minimum Distance to use Ranged Attacks
RULE_BOOL ( Combat, ArcheryBonusRequiresStationary, true) //does the 2x archery bonus chance require a stationary npc
RULE_REAL ( Combat, ArcheryBaseDamageBonus, 1) // % Modifier to Base Archery Damage (.5 = 50% base damage, 1 = 100%, 2 = 200%)
RULE_REAL ( Combat, ArcheryNPCMultiplier, 1.0) // this is multiplied by the regular dmg to get the archery dmg
RULE_BOOL ( Combat, AssistNoTargetSelf, true) //when assisting a target that does not have a target: true = target self, false = leave target as was before assist (false = live like)
RULE_INT ( Combat, MaxRampageTargets, 3) //max number of people hit with rampage
RULE_INT ( Combat, DefaultRampageTargets, 1) // default number of people to hit with rampage
RULE_BOOL ( Combat, RampageHitsTarget, false) // rampage will hit the target if it still has targets left
RULE_INT ( Combat, MaxFlurryHits, 2) //max number of extra hits from flurry
RULE_INT ( Combat, MonkDamageTableBonus, 5) //% bonus monks get to their damage table calcs
RULE_INT ( Combat, FlyingKickBonus, 25) //% Modifier that this skill gets to str and skill bonuses
RULE_INT ( Combat, DragonPunchBonus, 20) //% Modifier that this skill gets to str and skill bonuses
RULE_INT ( Combat, EagleStrikeBonus, 15) //% Modifier that this skill gets to str and skill bonuses
RULE_INT ( Combat, TigerClawBonus, 10) //% Modifier that this skill gets to str and skill bonuses
RULE_INT ( Combat, RoundKickBonus, 5) //% Modifier that this skill gets to str and skill bonuses
RULE_INT ( Combat, FrenzyBonus, 0) //% Modifier to damage
RULE_BOOL ( Combat, ProcTargetOnly, true) //true = procs will only affect our target, false = procs will affect all of our targets
RULE_INT ( Combat, HitCapPre20, 40) // live has it capped at 40 for whatever dumb reason... this is mainly for custom servers
RULE_INT ( Combat, HitCapPre10, 20) // live has it capped at 20, see above :p
RULE_INT ( Combat, MinHastedDelay, 400) // how fast we can get with haste.
RULE_REAL ( Combat, AvgDefProcsPerMinute, 2.0)
RULE_REAL ( Combat, DefProcPerMinAgiContrib, 0.075) //How much agility contributes to defensive proc rate
RULE_INT ( Combat, SpecialAttackACBonus, 15) //Percent amount of damage per AC gained for certain special attacks (damage = AC*SpecialAttackACBonus/100).
RULE_INT ( Combat, NPCFlurryChance, 20) // Chance for NPC to flurry.
RULE_BOOL (Combat,TauntOverLevel, 1) //Allows you to taunt NPC's over warriors level.
RULE_BOOL (Combat,EXPFromDmgShield, false) //Determine if damage from a damage shield counts for EXP gain.
RULE_INT ( Combat, QuiverWRHasteDiv, 3) //Weight Reduction is divided by this to get haste contribution for quivers
RULE_BOOL ( Combat, UseArcheryBonusRoll, false) //Make the 51+ archery bonus require an actual roll
RULE_INT ( Combat, ArcheryBonusChance, 50)
RULE_INT ( Combat, BerserkerFrenzyStart, 35)
RULE_INT ( Combat, BerserkerFrenzyEnd, 45)
RULE_REAL ( Combat, PushBackAmount, 1.5)
RULE_INT ( Combat, NPCAssistCap, 5) // Maxiumium number of NPCs that will assist another NPC at once
RULE_INT ( Combat, NPCAssistCapTimer, 6000) // Time in milliseconds a NPC will take to clear assist aggro cap space
RULE_CATEGORY_END()

RULE_CATEGORY( NPC )
RULE_INT ( NPC, MinorNPCCorpseDecayTimeMS, 450000 ) //level<55
RULE_INT ( NPC, MajorNPCCorpseDecayTimeMS, 1500000 ) //level>=55
RULE_INT ( NPC, CorpseUnlockTimer, 150000 )
RULE_INT ( NPC, EmptyNPCCorpseDecayTimeMS, 30000 )
RULE_BOOL (NPC, UseItemBonusesForNonPets, true)
RULE_INT ( NPC, SayPauseTimeInSec, 5)
RULE_INT ( NPC, OOCRegen, 0)
RULE_BOOL ( NPC, BuffFriends, false )
RULE_BOOL ( NPC, EnableNPCQuestJournal, false)
RULE_INT ( NPC, LastFightingDelayMovingMin, 10000)
RULE_INT ( NPC, LastFightingDelayMovingMax, 20000)
RULE_BOOL ( NPC, SmartLastFightingDelayMoving, true)
RULE_BOOL ( NPC, ReturnNonQuestItems, true)	// Returns items on NPCs that don't have an EVENT_TRADE sub in their script
RULE_INT ( NPC, StartEnrageValue, 9) // % HP that an NPC will begin to enrage
RULE_BOOL ( NPC, LiveLikeEnrage, false) // If set to true then only player controlled pets will enrage
RULE_REAL ( NPC, SpeedMultiplier, 50.0 ) //This is used to multiply an NPCs movement rate, yeilding map units..
RULE_INT ( NPC, RunAnimRatio, 10 )	//This changes their animation based on their movement speed.
RULE_REAL ( NPC, WalkSpeedMultiplier, 50.0 ) //Same as above for walking.
RULE_INT ( NPC, WalkAnimRatio, 10 ) //Same as above for walking. Probably okay to combine with the runspeed rule.
RULE_BOOL ( NPC, EnableMeritBasedFaction, false) // If set to true, faction will given in the same way as experience (solo/group/raid)
RULE_INT ( NPC, NPCTemplateID, 153076)
RULE_BOOL ( NPC, BoatsRunByDefault, true) // Mainly to make it easier to adjust boats' timing on the fly.
RULE_BOOL(NPC, CheckSoWBuff, false)
RULE_BOOL( NPC, IgnoreQuestLoot, false)
RULE_CATEGORY_END()

RULE_CATEGORY ( Aggro )
RULE_BOOL ( Aggro, SmartAggroList, true )
RULE_INT ( Aggro, SpellAggroMod, 100 )
RULE_INT ( Aggro, PetSpellAggroMod, 10 )
RULE_REAL ( Aggro, TunnelVisionAggroMod, 0.75 ) //people not currently the top hate generate this much hate on a Tunnel Vision mob
RULE_INT ( Aggro, IntAggroThreshold, 75 ) // Int <= this will aggro regardless of level difference.
RULE_BOOL ( Aggro, UseLevelAggro, true) // Level 18+ and Undead will aggro regardless of level difference. (this will disabled Rule:IntAggroThreshold if set to true)
RULE_CATEGORY_END()

RULE_CATEGORY ( Chat )
RULE_BOOL ( Chat, ServerWideOOC, true)
RULE_BOOL ( Chat, ServerWideAuction, true)
RULE_BOOL ( Chat, EnableMailKeyIPVerification, true)
RULE_BOOL ( Chat, EnableAntiSpam, true)
RULE_BOOL ( Chat, SuppressCommandErrors, false) // Do not suppress by default
RULE_INT ( Chat, MinStatusToBypassAntiSpam, 100)
RULE_INT ( Chat, MinimumMessagesPerInterval, 4)
RULE_INT ( Chat, MaximumMessagesPerInterval, 12)
RULE_INT ( Chat, MaxMessagesBeforeKick, 20)
RULE_INT ( Chat, IntervalDurationMS, 60000)
RULE_INT ( Chat, KarmaUpdateIntervalMS, 1200000)
RULE_INT ( Chat, KarmaGlobalChatLimit, 72) //amount of karma you need to be able to talk in ooc/auction/chat below the level limit
RULE_INT ( Chat, GlobalChatLevelLimit, 8) //level limit you need to of reached to talk in ooc/auction/chat if your karma is too low.
RULE_CATEGORY_END()

RULE_CATEGORY ( Merchant )
RULE_BOOL ( Merchant, UsePriceMod, true) // Use faction/charisma price modifiers.
RULE_REAL ( Merchant, SellCostMod, 1.05) // Modifier for NPC sell price.
RULE_REAL ( Merchant, BuyCostMod, 0.95) // Modifier for NPC buy price.
RULE_INT ( Merchant, PriceBonusPct, 4) // Determines maximum price bonus from having good faction/CHA. Value is a percent.
RULE_INT ( Merchant, PricePenaltyPct, 4) // Determines maximum price penalty from having bad faction/CHA. Value is a percent.
RULE_REAL( Merchant, ChaBonusMod, 3.45) // Determines CHA cap, from 104 CHA. 3.45 is 132 CHA at apprehensive. 0.34 is 400 CHA at apprehensive.
RULE_REAL ( Merchant, ChaPenaltyMod, 1.52) // Determines CHA bottom, up to 102 CHA. 1.52 is 37 CHA at apprehensive. 0.98 is 0 CHA at apprehensive.
RULE_CATEGORY_END()

RULE_CATEGORY ( Bazaar )
RULE_BOOL ( Bazaar, AuditTrail, false)
RULE_INT ( Bazaar, MaxSearchResults, 50)
RULE_BOOL ( Bazaar, EnableWarpToTrader, true)
RULE_CATEGORY_END()

RULE_CATEGORY ( Mail )
RULE_INT ( Mail, ExpireTrash, 0) // Time in seconds. 0 will delete all messages in the trash when the mailserver starts
RULE_INT ( Mail, ExpireRead, 31536000 ) // 1 Year. Set to -1 for never
RULE_INT ( Mail, ExpireUnread, 31536000 ) // 1 Year. Set to -1 for never
RULE_CATEGORY_END()

RULE_CATEGORY ( Channels )
RULE_INT ( Channels, RequiredStatusAdmin, 251) // Required status to administer chat channels
RULE_INT ( Channels, RequiredStatusListAll, 251) // Required status to list all chat channels
RULE_INT ( Channels, DeleteTimer, 1440) // Empty password protected channels will be deleted after this many minutes
RULE_CATEGORY_END()

RULE_CATEGORY ( EventLog )
RULE_BOOL ( EventLog, RecordSellToMerchant, false ) // Record sales from a player to an NPC merchant in eventlog table
RULE_BOOL ( EventLog, RecordBuyFromMerchant, false ) // Record purchases by a player from an NPC merchant in eventlog table
RULE_BOOL ( EventLog, SkipCommonPacketLogging, true ) // Doesn't log OP_MobHealth or OP_ClientUpdate
RULE_CATEGORY_END()

RULE_CATEGORY ( AA )
RULE_INT ( AA, ExpPerPoint, 23976496) //Amount of exp per AA, if AAPercent is 100%. Otherwise, we use the standard XP formula for 52.
RULE_CATEGORY_END()

RULE_CATEGORY( Console )
RULE_INT ( Console, SessionTimeOut, 600000 )	// Amount of time in ms for the console session to time out
RULE_CATEGORY_END()

RULE_CATEGORY( QueryServ )
RULE_BOOL( QueryServ, PlayerLogChat, false) // Logs Player Chat 
RULE_BOOL( QueryServ, PlayerLogTrades, false) // Logs Player Trades
RULE_BOOL( QueryServ, PlayerLogHandins, false) // Logs Player Handins
RULE_BOOL( QueryServ, PlayerLogNPCKills, false) // Logs Player NPC Kills
RULE_BOOL( QueryServ, PlayerLogDeletes, false) // Logs Player Deletes
RULE_BOOL( QueryServ, PlayerLogMoves, false) // Logs Player Moves
RULE_BOOL( QueryServ, PlayerLogMerchantTransactions, false) // Logs Merchant Transactions
RULE_BOOL( QueryServ, PlayerLogPCCoordinates, false) // Logs Player Coordinates with certain events
RULE_BOOL( QueryServ, PlayerLogDropItem, false) // Logs Player Drop Item
RULE_BOOL( QueryServ, PlayerLogZone, false) // Logs Player Zone Events
RULE_BOOL( QueryServ, PlayerLogDeaths, false) // Logs Player Deaths
RULE_BOOL( QueryServ, PlayerLogConnectDisconnect, false) // Logs Player Connect Disconnect State
RULE_BOOL( QueryServ, PlayerLogLevels, false) // Logs Player Leveling/Deleveling
RULE_BOOL( QueryServ, PlayerLogAARate, false) // Logs Player AA Experience Rates 
RULE_BOOL( QueryServ, PlayerLogQGlobalUpdate, false) // Logs Player QGlobal Updates
RULE_BOOL( QueryServ, PlayerLogKeyringAddition, false) // Log PLayer Keyring additions
RULE_BOOL( QueryServ, PlayerLogAAPurchases, false) // Log Player AA Purchases
RULE_BOOL( QueryServ, PlayerLogTradeSkillEvents, false) // Log Player Tradeskill Transactions
RULE_BOOL( QueryServ, PlayerLogIssuedCommandes, false ) // Log Player Issued Commands
RULE_BOOL(QueryServ, PlayerLogMoneyTransactions, false) // Log Player Money Transaction/Splits
RULE_BOOL(QueryServ, PlayerLogAlternateCurrencyTransactions, false) // Log Player Alternate Currency Transactions
RULE_BOOL(QueryServ, PlayerLogLoot, false) // Log Player Looting cash/Items
RULE_CATEGORY_END()

RULE_CATEGORY( Groundspawns )
RULE_INT ( Groundspawns, DecayTime, 300000 )	// Decay time of player dropped items.
RULE_INT ( Groundspawns, DisarmDecayTime, 300000 )	// Decay time of weapons dropped due to disarm
RULE_INT ( Groundspawns, FullInvDecayTime, 300000 )	// Decay time of items dropped due to full inventory (trades/merchants)
RULE_BOOL ( Groundspawns, RandomSpawn, true )	// Determines if groundspawns with random spanw locs will periodically despawn and respawn elsewhere.
RULE_CATEGORY_END()

#undef RULE_CATEGORY
#undef RULE_INT
#undef RULE_REAL
#undef RULE_BOOL
#undef RULE_CATEGORY_END
