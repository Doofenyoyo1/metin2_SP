#ifndef __INC_METIN2_PLAYERBOT_NEWPET_H__
#define __INC_METIN2_PLAYERBOT_NEWPET_H__

// MT2009 PLUS New Pet System (the operator, 28 September: "nowy pet system,
// ktory bedzie dzialal obok obecnego systemu petow. Gracz bedzie mogl miec
// przywolane dwa pety - jeden z itemshopa, drugi wykluty z jajka i expiony").
//
// dracaryS' "New Pet System" (Metin2Dev) rebuilt for an engine and a client
// that cannot change: the mod kept the pet in an item (sockets, attributes,
// a WEAR_PET slot, new packets and a new client binary); here the pet lives
// in the database and the client window (uinewpet.py) is fed over chat
// commands. The ItemShop pets (PetSystem.cpp, CPetSystem) are not touched -
// this pet is a second, separate follower spawned beside them.
//
// What it keeps of the mod:
//   - eggs (55401-55411) hatched for 100 000 yang and a name;
//   - levels 1-120 from the owner's kill experience (the player table,
//     exp_table_common), capped per evolution: 40 / 75 / 100 / 120;
//   - evolution (Mlody, Dziki, Odwazny, Heroiczny) for materials and yang,
//     each opening one skill slot; the heroic pet changes form (the "_2" mob);
//   - three main bonuses (max HP 4000, strong vs monsters 20%, critical 10%),
//     levels 0-20 raised by the elixirs S/M/L/XL (55101-55118), each size
//     from the pet level the mod asked;
//   - 22 skills from books 55010-55031 (skill = vnum - 55009), levels 1-20;
//     Pet Reverti (55033) forgets them all, Pet Revertus (55034) one, 55036
//     opens one more slot for a heroic pet of level 100+;
//   - trading: Transporter Peta (55002) packs the chosen pet into a
//     Transporter z Petem (55007), which trades as any item; its user gets
//     the pet (a conditional UPDATE on the row, so one pet, one owner);
//   - life energy: 7 days at hatching, +7 per Proteinowa Przekaska (55001),
//     at most 84; an exhausted pet cannot be summoned until fed;
//   - Smakolyk (55032, value0 exp) and Smakolyk+ (55035, value0 % of the
//     level) feed experience; Zwoj imienia (55008) renames.
// The bonuses are the owner's while the pet is out (ComputePoints hook).
//
// Every character may keep MAX_PETS pets, one of them chosen (active) and at
// most that one summoned. A player's character moves between game cores (each
// hosts other maps), so the database is the truth: player.newpet_pet holds
// every value; a core never writes a value it only remembers - experience is
// added (exp = exp + delta), a level is taken with a conditional UPDATE
// (WHERE level = the old one AND exp >= the need), and every item's effect is
// a compare-and-set on the row, the item taken only when the row changed.
// The pet itself (a mob, IsPet) is this core's alone and goes when the owner
// leaves the core; the client asks "/newpet sync" after every loading screen
// and the pet comes back where the owner is.
//
// Bots never take part. The engine calls in through server-patches/playerqol
// (MT2009_PLUS_NEW_PET_V1): the item use, the kill experience, ComputePoints
// and the /newpet command (declared in cmd.cpp, defined at the end here).
namespace mt2009_newpet
{
	enum
	{
		SKILL_SLOTS = 15,
		SKILL_LOCKED = 99,
		SKILL_COUNT = 22,
		SKILL_LEVEL_MAX = 20,
		BONUS_COUNT = 3,
		BONUS_LEVEL_MAX = 20,
		EVOLUTION_MAX = 3,
		MAX_PETS = 3,
		PET_NAME_LEN_MAX = 12,
	};

	// The mod's numbers (constants.cpp, item.cpp, char_item.cpp of the mod).
	const long long HATCH_PRICE = 100000;
	const long long RENAME_PRICE = 100000;
	const DWORD DAY = 24 * 60 * 60;
	const DWORD LIFE_AT_HATCH = 7 * DAY;
	const DWORD LIFE_PER_PROTEIN = 7 * DAY;
	const DWORD LIFE_MAX = 84 * DAY;
	// The owner's kill experience the pet takes, in percent, and the share of
	// the player table's level a pet level needs.
	const int EXP_GAIN_PERCENT = 100;
	const int EXP_NEED_PERCENT = 100;
	const int LEVEL_CAPS[EVOLUTION_MAX + 1] = { 40, 75, 100, 120 };
	// The elixirs S/M/L/XL: the pet level each size needs (the mod: 0/40/75/100).
	const int DEW_PET_LEVEL[4] = { 0, 40, 75, 100 };
	// Pet Open Slot (55036): a heroic pet of this level.
	const int SLOT_UNLOCK_LEVEL = 100;
	const DWORD FLUSH_MS = 30 * 1000;

	const DWORD ITEM_PROTEIN = 55001;
	const DWORD ITEM_RENAME = 55008;
	const DWORD ITEM_BOOK_CHEST = 55009;
	const DWORD ITEM_BOOK_FIRST = 55010;	// skill 1; skill n is 55009 + n
	const DWORD ITEM_TREAT = 55032;
	const DWORD ITEM_REVERTI = 55033;	// forget every skill
	const DWORD ITEM_REVERTUS = 55034;	// forget one skill (the window)
	const DWORD ITEM_TREAT_PLUS = 55035;
	const DWORD ITEM_OPEN_SLOT = 55036;
	// The elixirs: 55101-55104 max HP, 55108-55111 monsters, 55115-55118 critical.
	const DWORD DEW_FIRST[BONUS_COUNT] = { 55101, 55108, 55115 };

	struct Species
	{
		DWORD egg;
		const char* name;	// CP1250
		DWORD young;
		DWORD hero;
	};

	// The mod's eggs and their mobs (its item_proto 55401-55411 -> seals
	// 55701-55711 -> mob_proto); the client's npclist.txt has every one of
	// them. 55407 (Azrael) and 55408 (Executor) are left out: the mod has no
	// icon or model for them.
	const Species SPECIES[] = {
		{ 55401, "Ma\xb3pka", 34041, 34042 },
		{ 55402, "Paj\xb9" "czek", 34045, 34046 },
		{ 55403, "Mini Razador", 34049, 34050 },
		{ 55404, "Mini Nemere", 34053, 34054 },
		{ 55405, "Smoczek", 34036, 34037 },
		{ 55406, "Czerwony Smoczek", 34064, 34065 },
		{ 55409, "Ma\xb3y Baashido", 34080, 34081 },
		{ 55410, "Nessie", 34082, 34083 },
		{ 55411, "Piskl\xea Exedyara", 34047, 34048 },
	};
	const size_t SPECIES_COUNT = sizeof(SPECIES) / sizeof(SPECIES[0]);

	struct BonusDef
	{
		BYTE point;
		int max;
	};

	const BonusDef BONUSES[BONUS_COUNT] = {
		{ POINT_MAX_HP, 4000 },
		{ POINT_ATTBONUS_MONSTER, 20 },
		{ POINT_CRITICAL_PCT, 10 },
	};

	struct SkillDef
	{
		BYTE points[3];
		int max;
	};

	// Index = the book's value (vnum - 55009); the mod's petSkillBonus.
	const SkillDef SKILLS[SKILL_COUNT + 1] = {
		{ { 0, 0, 0 }, 0 },
		{ { POINT_ATTBONUS_STONE, 0, 0 }, 10 },
		{ { POINT_ATTBONUS_BOSS, 0, 0 }, 10 },
		{ { POINT_ATTBONUS_UNDEAD, POINT_ATTBONUS_DEVIL, 0 }, 15 },
		{ { POINT_ATTBONUS_HUMAN, 0, 0 }, 5 },
		{ { POINT_CASTING_SPEED, 0, 0 }, 15 },
		{ { POINT_ATTBONUS_MONSTER, 0, 0 }, 10 },
		{ { POINT_ST, 0, 0 }, 10 },
		{ { POINT_IQ, 0, 0 }, 10 },
		{ { POINT_DX, 0, 0 }, 10 },
		{ { POINT_SKILL_DAMAGE_BONUS, 0, 0 }, 5 },
		{ { POINT_NORMAL_HIT_DAMAGE_BONUS, 0, 0 }, 10 },
		{ { POINT_RESIST_CRITICAL, 0, 0 }, 10 },
		{ { POINT_RESIST_PENETRATE, 0, 0 }, 10 },
		{ { POINT_RESIST_HUMAN, 0, 0 }, 5 },
		{ { POINT_BLOCK, 0, 0 }, 10 },
		{ { POINT_ATTBONUS_BOSS, 0, 0 }, 10 },
		{ { POINT_RESIST_ICE, POINT_RESIST_EARTH, POINT_RESIST_DARK }, 10 },
		{ { POINT_NORMAL_HIT_DEFEND_BONUS, 0, 0 }, 10 },
		{ { POINT_SKILL_DEFEND_BONUS, 0, 0 }, 5 },
		{ { POINT_EXP_DOUBLE_BONUS, POINT_ITEM_DROP_BONUS, POINT_GOLD_DOUBLE_BONUS }, 10 },
		{ { POINT_POISON_REDUCE, 0, 0 }, 10 },
		{ { POINT_HT, 0, 0 }, 10 },
	};

	enum { EVOLUTION_ITEMS = 5 };

	struct EvolutionCost
	{
		DWORD vnum[EVOLUTION_ITEMS];
		DWORD count[EVOLUTION_ITEMS];
		long long gold;
	};

	// Young -> Wild -> Brave -> Heroic. The mod's own lists are its server's
	// items; these are this world's (pearls, spirit stones, the Unknown
	// Medicine, crystal essences, raw Cor Draconis). MT2009_PLUS_NEW_PET_V1
	// (evolution 2): Wild -> Brave takes 10 Kamien Duchowy (50513), 10 Nieznane
	// Lekarstwo (30009, the one monsters drop) and 5 of each pearl (27992-27994)
	// instead of the dragon scales and claws (the owner, 29 September). Heroic takes
	// our rough Cor Draconis (50255), the one that drops, not the 515xx one.
	const EvolutionCost EVOLUTION_COSTS[EVOLUTION_MAX] = {
		{ { 27992, 27993, 27994, 0, 0 }, { 5, 5, 5, 0, 0 }, 5000000LL },
		{ { 50513, 30009, 27992, 27993, 27994 }, { 10, 10, 5, 5, 5 }, 20000000LL },
		{ { 31005, 31006, 50255, 0, 0 }, { 10, 10, 10, 0, 0 }, 50000000LL },
	};

	// The eggs: per kill, one in N (0 = never), from monsters of level 10 or
	// more and at most 15 levels below the killer. [normal, Metin, boss].
	const int EGG_ONE_IN[3] = { 20000, 1500, 150 };

	// Metin stones above level 50 (the operator, 28 September: "z Metinow
	// powyzej 50 lvl dropily ksiazki do petow, eliksiry i przekaski, ale
	// ogolnie malo"): one roll per Metin, METIN_ITEM_PER_MILLE of them give one
	// pet item (4%), chosen by the weights below (their sum is 100, so a
	// weight is that item's share: a skill book 1.0% of Metins, an elixir S
	// 0.8%, M 0.4%, L 0.16%, XL 0.04%, a protein snack 0.8%, a treat 0.6%, a
	// treat+ 0.2%). The killer's level does not matter here.
	const int METIN_ITEM_MIN_LEVEL = 51;
	const int METIN_ITEM_PER_MILLE = 40;
	enum
	{
		DROP_BOOK = 1,		// a random skill book 55010-55031
		DROP_ELIXIR_S = 2,	// an elixir of a random bonus, S..XL = 2..5
		DROP_ELIXIR_M = 3,
		DROP_ELIXIR_L = 4,
		DROP_ELIXIR_XL = 5,
	};

	struct WeightedDrop
	{
		DWORD vnum;	// a DROP_* above, or an item vnum
		int weight;
	};

	const WeightedDrop METIN_DROPS[] = {
		{ DROP_BOOK, 25 },
		{ DROP_ELIXIR_S, 20 },
		{ DROP_ELIXIR_M, 10 },
		{ DROP_ELIXIR_L, 4 },
		{ DROP_ELIXIR_XL, 1 },
		{ 55001, 20 },	// Proteinowa Przekaska
		{ 55032, 15 },	// Smakolyk
		{ 55035, 5 },	// Smakolyk+
	};

	// The transporter (the operator: "handel petem ma byc mozliwy"): the empty
	// one packs the chosen pet into the full one, which trades as any item
	// (trade window, offline shop, storeroom) and gives the pet to whoever
	// uses it. The pet's row keeps pid 0 and packed 1 meanwhile; socket 0 is
	// its id, socket 1 level + 1000 * evolution, socket 2 its egg.
	const DWORD ITEM_TRANSPORT_EMPTY = 55002;
	const DWORD ITEM_TRANSPORT_FULL = 55007;

	const char* const EVOLUTION_NAMES[EVOLUTION_MAX + 1] = { "M\xb3ody", "Dziki", "Odwa\xbfny", "Heroiczny" };

	struct Pet
	{
		DWORD id;
		DWORD egg;
		std::string name;
		int level;
		unsigned long long exp;
		int evolution;
		DWORD lifeUntil;
		int bonus[BONUS_COUNT];
		BYTE skillType[SKILL_SLOTS];
		BYTE skillLevel[SKILL_SLOTS];
		bool active;
		bool summoned;
		std::string skillsText;
	};

	struct Owner
	{
		DWORD pid;
		std::vector<Pet> pets;
		DWORD petVid;
		long lastMap;
		unsigned long long expDelta;
		DWORD nextFlush;
		DWORD lastCommand;
		DWORD lastRefresh;
		DWORD lastInfo;
		// Where the pet stands beside its owner: left of the ItemShop pet.
		int side;
		Owner() : pid(0), petVid(0), lastMap(0), expDelta(0), nextFlush(0), lastCommand(0), lastRefresh(0), lastInfo(0), side(1) {}
	};

	std::map<DWORD, Owner> s_owners;
	LPEVENT s_pkTick = NULL;
	bool s_bTables = false;

	bool Eligible(LPCHARACTER ch)
	{
		return ch && ch->IsPC() && ch->GetDesc() && !ch->GetDesc()->IsBot();
	}

	void Say(LPCHARACTER ch, const char* fmt, ...)
	{
		char text[400];
		va_list args;
		va_start(args, fmt);
		vsnprintf(text, sizeof(text), fmt, args);
		va_end(args);
		ch->ChatPacket(CHAT_TYPE_INFO, "[Pet] %s", text);
	}

	std::string HexOf(const std::string& data)
	{
		static const char* const digits = "0123456789abcdef";
		std::string out;
		for (size_t i = 0; i < data.size(); ++i)
		{
			const unsigned char c = (unsigned char)data[i];
			out += digits[c >> 4];
			out += digits[c & 15];
		}
		return out.empty() ? std::string("-") : out;
	}

	std::string Money(long long n)
	{
		char raw[32];
		snprintf(raw, sizeof(raw), "%lld", n);
		std::string text(raw), out;
		int k = 0;
		for (int i = (int)text.size() - 1; i >= 0; --i)
		{
			out.insert(out.begin(), text[i]);
			if (++k % 3 == 0 && i > 0)
				out.insert(out.begin(), '.');
		}
		return out;
	}

	// The rows an UPDATE changed, -1 on an error.
	int Exec(const char* query)
	{
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get())
		{
			sys_err("NEWPET: query failed errno=%u: %s", msg.get() ? msg->uiSQLErrno : 0U, query);
			return -1;
		}
		return (int)msg->Get()->uiAffectedRows;
	}

	bool EnsureTables()
	{
		if (s_bTables)
			return true;
		const int r = Exec(
				"CREATE TABLE IF NOT EXISTS player.newpet_pet ("
				"id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
				"pid INT UNSIGNED NOT NULL, "
				"egg INT UNSIGNED NOT NULL, "
				"name VARBINARY(24) NOT NULL DEFAULT '', "
				"level SMALLINT UNSIGNED NOT NULL DEFAULT 1, "
				"exp BIGINT UNSIGNED NOT NULL DEFAULT 0, "
				"evolution TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"life_until INT UNSIGNED NOT NULL DEFAULT 0, "
				"bonus0 TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"bonus1 TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"bonus2 TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"skills VARCHAR(128) NOT NULL DEFAULT '', "
				"active TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"summoned TINYINT UNSIGNED NOT NULL DEFAULT 0, "
				"created INT UNSIGNED NOT NULL DEFAULT 0, "
				"KEY pid (pid)) ENGINE=InnoDB");
		// The transporter's state (a pet in a transporter has pid 0).
		const int packed = r >= 0 ? Exec("ALTER TABLE player.newpet_pet ADD COLUMN IF NOT EXISTS packed TINYINT UNSIGNED NOT NULL DEFAULT 0") : -1;
		s_bTables = r >= 0 && packed >= 0;
		return s_bTables;
	}

	const Species* FindSpecies(DWORD egg)
	{
		for (size_t i = 0; i < SPECIES_COUNT; ++i)
			if (SPECIES[i].egg == egg)
				return &SPECIES[i];
		return NULL;
	}

	unsigned long long Need(int level)
	{
		if (level < 1)
			level = 1;
		if (level > PLAYER_MAX_LEVEL_CONST)
			level = PLAYER_MAX_LEVEL_CONST;
		const unsigned long long need = (unsigned long long)exp_table_common[level] * EXP_NEED_PERCENT / 100;
		return need ? need : 1;
	}

	int Cap(int evolution)
	{
		return LEVEL_CAPS[MINMAX(0, evolution, (int)EVOLUTION_MAX)];
	}

	int BonusValue(int level, int max)
	{
		if (level <= 0)
			return 0;
		if (level >= 20)
			return max;
		return level * max / 20;
	}

	std::string SkillsText(const Pet& pet)
	{
		std::string out;
		char part[16];
		for (int i = 0; i < SKILL_SLOTS; ++i)
		{
			snprintf(part, sizeof(part), "%s%u.%u", i ? "," : "", (unsigned int)pet.skillType[i], (unsigned int)pet.skillLevel[i]);
			out += part;
		}
		return out;
	}

	void ParseSkills(Pet& pet, const char* text)
	{
		for (int i = 0; i < SKILL_SLOTS; ++i)
		{
			pet.skillType[i] = SKILL_LOCKED;
			pet.skillLevel[i] = 0;
		}
		int slot = 0;
		const char* p = text ? text : "";
		while (*p && slot < SKILL_SLOTS)
		{
			unsigned int type = 0, level = 0;
			if (sscanf(p, "%u.%u", &type, &level) == 2)
			{
				pet.skillType[slot] = (BYTE)(type == SKILL_LOCKED || type <= SKILL_COUNT ? type : 0);
				pet.skillLevel[slot] = (BYTE)std::min<unsigned int>(level, (unsigned int)SKILL_LEVEL_MAX);
			}
			++slot;
			const char* comma = strchr(p, ',');
			if (!comma)
				break;
			p = comma + 1;
		}
	}

	// Every pet of the character, read again from the database.
	bool Reload(Owner& owner)
	{
		if (!EnsureTables())
			return false;
		char query[320];
		snprintf(query, sizeof(query),
				"SELECT id, egg, name, level, exp, evolution, life_until, bonus0, bonus1, bonus2, skills, active, summoned "
				"FROM player.newpet_pet WHERE pid=%u ORDER BY id", owner.pid);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return false;
		std::vector<Pet> pets;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			unsigned long* lengths = mysql_fetch_lengths(msg->Get()->pSQLResult);
			Pet pet;
			pet.id = pet.egg = pet.lifeUntil = 0;
			pet.level = pet.evolution = 0;
			pet.exp = 0;
			unsigned int active = 0, summoned = 0;
			str_to_number(pet.id, row[0]);
			str_to_number(pet.egg, row[1]);
			pet.name = (row[2] && lengths) ? std::string(row[2], lengths[2]) : std::string();
			str_to_number(pet.level, row[3]);
			pet.exp = row[4] ? strtoull(row[4], NULL, 10) : 0;
			str_to_number(pet.evolution, row[5]);
			str_to_number(pet.lifeUntil, row[6]);
			for (int b = 0; b < BONUS_COUNT; ++b)
			{
				pet.bonus[b] = 0;
				str_to_number(pet.bonus[b], row[7 + b]);
			}
			ParseSkills(pet, row[10]);
			pet.skillsText = row[10] ? row[10] : "";
			str_to_number(active, row[11]);
			str_to_number(summoned, row[12]);
			pet.active = active != 0;
			pet.summoned = summoned != 0;
			pet.level = MINMAX(1, pet.level, LEVEL_CAPS[EVOLUTION_MAX]);
			pet.evolution = MINMAX(0, pet.evolution, (int)EVOLUTION_MAX);
			pets.push_back(pet);
		}
		// Exactly one chosen pet when there is any.
		bool any = false;
		for (size_t i = 0; i < pets.size(); ++i)
			any = any || pets[i].active;
		if (!any && !pets.empty())
		{
			pets[0].active = true;
			snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET active = (id = %u) WHERE pid=%u", pets[0].id, owner.pid);
			Exec(query);
		}
		owner.pets.swap(pets);
		return true;
	}

	Pet* ActivePet(Owner& owner)
	{
		for (size_t i = 0; i < owner.pets.size(); ++i)
			if (owner.pets[i].active)
				return &owner.pets[i];
		return NULL;
	}

	Owner* FindOwner(DWORD pid)
	{
		std::map<DWORD, Owner>::iterator it = s_owners.find(pid);
		return it == s_owners.end() ? NULL : &it->second;
	}

	void EnsureTick();

	Owner& GetOwner(LPCHARACTER ch)
	{
		const DWORD pid = ch->GetPlayerID();
		std::map<DWORD, Owner>::iterator it = s_owners.find(pid);
		if (it != s_owners.end())
			return it->second;
		Owner& owner = s_owners[pid];
		owner.pid = pid;
		Reload(owner);
		EnsureTick();
		return owner;
	}

	LPCHARACTER PetChar(Owner& owner)
	{
		if (!owner.petVid)
			return NULL;
		LPCHARACTER pet = CHARACTER_MANAGER::instance().Find(owner.petVid);
		if (!pet || !pet->IsPet())
		{
			owner.petVid = 0;
			return NULL;
		}
		return pet;
	}

	bool IsOut(Owner& owner)
	{
		return PetChar(owner) != NULL;
	}

	void DestroyPetChar(Owner& owner)
	{
		LPCHARACTER pet = PetChar(owner);
		if (pet)
			M2_DESTROY_CHARACTER(pet);
		owner.petVid = 0;
	}

	DWORD MobOf(const Pet& pet)
	{
		const Species* sp = FindSpecies(pet.egg);
		if (!sp)
			return 0;
		return pet.evolution >= EVOLUTION_MAX ? sp->hero : sp->young;
	}

	const char* SpeciesName(DWORD egg)
	{
		const Species* sp = FindSpecies(egg);
		return sp ? sp->name : "Pet";
	}

	// Beside the owner, on the other side than the ItemShop pet comes to.
	void PlaceBeside(LPCHARACTER ch, int side, long& x, long& y)
	{
		const float rot = (ch->GetRotation() + 90.0f * side) * 3.141592f / 180.0f;
		x = ch->GetX() + (long)(150.0f * cos(rot));
		y = ch->GetY() + (long)(150.0f * sin(rot));
	}

	bool SpawnPetChar(LPCHARACTER ch, Owner& owner, const Pet& pet)
	{
		DestroyPetChar(owner);
		const DWORD vnum = MobOf(pet);
		if (!vnum || !CMobManager::instance().Get(vnum))
		{
			sys_err("NEWPET: no mob_proto row for the pet %u (egg %u)", vnum, pet.egg);
			Say(ch, "Tego peta nie da si\xea teraz przywo\xb3" "a\xe6 (brak potwora %u).", vnum);
			return false;
		}
		long x = 0, y = 0;
		PlaceBeside(ch, owner.side, x, y);
		LPCHARACTER mob = CHARACTER_MANAGER::instance().SpawnMob(vnum, ch->GetMapIndex(), x, y, ch->GetZ(),
				false, (int)(ch->GetRotation() + 180), false);
		if (!mob)
		{
			sys_err("NEWPET: SpawnMob %u failed for %s", vnum, ch->GetName());
			return false;
		}
		mob->SetPet();
		mob->SetEmpire(ch->GetEmpire());
		mob->SetName(pet.name.empty() ? std::string(SpeciesName(pet.egg)) : pet.name);
		mob->SetLevel((BYTE)MINMAX(1, pet.level, 250));
		mob->Show(ch->GetMapIndex(), x, y, ch->GetZ());
		owner.petVid = mob->GetVID();
		owner.lastMap = ch->GetMapIndex();
		return true;
	}

	// SpecificEffectPacket copies MAX_EFFECT_FILE_NAME bytes of its argument.
	void Effect(LPCHARACTER mob, const char* file)
	{
		char buffer[MAX_EFFECT_FILE_NAME];
		memset(buffer, 0, sizeof(buffer));
		strlcpy(buffer, file, sizeof(buffer));
		mob->SpecificEffectPacket(buffer);
	}

	void SendData(LPCHARACTER ch, Owner& owner, bool open)
	{
		const DWORD now = (DWORD)time(NULL);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet Begin %u %d %d", (unsigned int)owner.pets.size(), open ? 1 : 0, (int)MAX_PETS);
		for (size_t i = 0; i < owner.pets.size(); ++i)
		{
			const Pet& pet = owner.pets[i];
			const Species* sp = FindSpecies(pet.egg);
			const bool out = pet.active && IsOut(owner);
			unsigned long long exp = pet.exp + (pet.active ? owner.expDelta : 0);
			const unsigned long long need = Need(pet.level);
			if (exp > need)
				exp = need;
			const DWORD lifeLeft = pet.lifeUntil > now ? pet.lifeUntil - now : 0;
			ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet Pet %u %u %u %u %d %llu %llu %d %d %u %u %d %d %d %d %d %s %s %s",
					pet.id, pet.egg, sp ? sp->young : 0, sp ? sp->hero : 0, pet.level, exp, need, pet.evolution,
					Cap(pet.evolution), lifeLeft, LIFE_MAX, pet.active ? 1 : 0, out ? 1 : 0,
					pet.bonus[0], pet.bonus[1], pet.bonus[2], SkillsText(pet).c_str(),
					HexOf(pet.name).c_str(), HexOf(sp ? sp->name : "").c_str());
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet End");
	}

	void SendEvolutionCosts(LPCHARACTER ch)
	{
		for (int e = 0; e < EVOLUTION_MAX; ++e)
		{
			const EvolutionCost& c = EVOLUTION_COSTS[e];
			// Five pairs; a client that reads three shows the first three.
			ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet Evo %d %d %lld %u %u %u %u %u %u %u %u %u %u", e, LEVEL_CAPS[e], c.gold,
					c.vnum[0], c.count[0], c.vnum[1], c.count[1], c.vnum[2], c.count[2],
					c.vnum[3], c.count[3], c.vnum[4], c.count[4]);
		}
	}

	// The owner's bonuses again (ComputePoints calls NewPetApplyPoints).
	void Recompute(LPCHARACTER ch)
	{
		if (ch && ch->GetDesc())
			ch->ComputePoints();
	}

	// The chosen pet's level ups: the experience added to the row, then each
	// level taken with a conditional UPDATE - whichever core holds the owner
	// now, a level is never taken twice nor a count written over.
	void Flush(LPCHARACTER ch, Owner& owner, bool announce)
	{
		owner.nextFlush = get_dword_time() + FLUSH_MS;
		Pet* pet = ActivePet(owner);
		if (!pet)
		{
			owner.expDelta = 0;
			return;
		}
		char query[320];
		if (owner.expDelta)
		{
			snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET exp = exp + %llu WHERE id=%u AND pid=%u",
					owner.expDelta, pet->id, owner.pid);
			owner.expDelta = 0;
			if (Exec(query) < 0)
				return;
		}
		const DWORD id = pet->id;
		if (!Reload(owner))
			return;
		pet = ActivePet(owner);
		if (!pet || pet->id != id)
			return;
		const int oldLevel = pet->level;
		for (int guard = 0; guard < 130 && pet->level < Cap(pet->evolution); ++guard)
		{
			const unsigned long long need = Need(pet->level);
			if (pet->exp < need)
				break;
			snprintf(query, sizeof(query),
					"UPDATE player.newpet_pet SET level = level + 1, exp = exp - %llu WHERE id=%u AND level=%d AND exp >= %llu",
					need, pet->id, pet->level, need);
			if (Exec(query) != 1)
				break;
			pet->level += 1;
			pet->exp -= need;
		}
		// At the evolution's cap the experience waits, never more than a level.
		if (pet->level >= Cap(pet->evolution) && pet->exp > Need(pet->level))
		{
			snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET exp = LEAST(exp, %llu) WHERE id=%u",
					Need(pet->level), pet->id);
			Exec(query);
			pet->exp = Need(pet->level);
		}
		if (pet->level != oldLevel)
		{
			Reload(owner);
			pet = ActivePet(owner);
			if (!pet)
				return;
			LPCHARACTER mob = PetChar(owner);
			if (mob)
			{
				mob->SetLevel((BYTE)MINMAX(1, pet->level, 250));
				Effect(mob, "d:/ymir work/effect/etc/levelup_1/level_up.mse");
			}
			if (ch && announce)
			{
				Say(ch, "%s osi\xb9gn\xb9\xb3 %d poziom!", pet->name.c_str(), pet->level);
				if (pet->level >= Cap(pet->evolution) && pet->evolution < EVOLUTION_MAX)
					Say(ch, "%s potrzebuje ewolucji, \xbf" "eby rosn\xb9\xe6 dalej.", pet->name.c_str());
				SendData(ch, owner, false);
			}
		}
	}

	void AddExp(LPCHARACTER ch, Owner& owner, unsigned long long amount)
	{
		Pet* pet = ActivePet(owner);
		if (!pet || !amount)
			return;
		if (pet->level >= Cap(pet->evolution) && pet->exp + owner.expDelta >= Need(pet->level))
			return;
		owner.expDelta += amount;
		if (pet->exp + owner.expDelta >= Need(pet->level) || get_dword_time() >= owner.nextFlush)
			Flush(ch, owner, true);
	}

	void MarkSummoned(Owner& owner, DWORD id)
	{
		char query[200];
		snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET summoned = (id = %u) WHERE pid=%u", id, owner.pid);
		Exec(query);
		for (size_t i = 0; i < owner.pets.size(); ++i)
			owner.pets[i].summoned = owner.pets[i].id == id;
	}

	bool Summon(LPCHARACTER ch, Owner& owner, bool quiet)
	{
		Pet* pet = ActivePet(owner);
		if (!pet)
		{
			if (!quiet)
				Say(ch, "Nie masz jeszcze peta. Wykluj go z jajka.");
			return false;
		}
		if (ch->IsObserverMode() || ch->IsDead())
			return false;
		if (pet->lifeUntil <= (DWORD)time(NULL))
		{
			if (!quiet)
			{
				TItemTable* p = ITEM_MANAGER::instance().GetTable(ITEM_PROTEIN);
				Say(ch, "%s nie ma si\xb3 (energia \xbfyciowa 0). Nakarm go: %s.", pet->name.c_str(), p ? p->szLocaleName : "Proteinowa Przek\xb9ska");
			}
			if (pet->summoned)
				MarkSummoned(owner, 0);
			return false;
		}
		if (!SpawnPetChar(ch, owner, *pet))
			return false;
		if (!pet->summoned)
			MarkSummoned(owner, pet->id);
		LPCHARACTER mob = PetChar(owner);
		if (mob)
			Effect(mob, "d:/ymir work/effect/etc/appear_die/npc2_appear.mse");
		Recompute(ch);
		return true;
	}

	void Unsummon(LPCHARACTER ch, Owner& owner, bool persist)
	{
		const bool wasOut = IsOut(owner);
		if (owner.expDelta)
			Flush(ch, owner, true);
		DestroyPetChar(owner);
		if (persist)
			MarkSummoned(owner, 0);
		if (wasOut)
			Recompute(ch);
	}

	// The pet walks after its owner as the ItemShop pet does
	// (CPetActor::_UpdateFollowAI): stands when near, walks, runs when far,
	// and is put beside the owner when very far or on another map.
	void Follow(LPCHARACTER ch, Owner& owner, LPCHARACTER pet)
	{
		if (!pet->m_pkMobData)
			return;
		long tx = 0, ty = 0;
		PlaceBeside(ch, owner.side, tx, ty);
		const float dist = DISTANCE_APPROX(pet->GetX() - ch->GetX(), pet->GetY() - ch->GetY());
		if (dist >= 4500.0f)
		{
			pet->Show(ch->GetMapIndex(), tx, ty);
			return;
		}
		if (dist < 300.0f)
			return;
		pet->SetNowWalking(dist < 900.0f);
		const float toTarget = DISTANCE_SQRT(tx - pet->GetX(), ty - pet->GetY());
		if (toTarget < 60.0f)
			return;
		pet->SetRotationToXY(tx, ty);
		if (pet->Goto(tx, ty))
			pet->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);
	}

	EVENTINFO(newpet_tick_info)
	{
		int dummy;
	};

	EVENTFUNC(newpet_tick)
	{
		const DWORD now = get_dword_time();
		const DWORD unixNow = (DWORD)time(NULL);
		for (std::map<DWORD, Owner>::iterator it = s_owners.begin(); it != s_owners.end(); )
		{
			Owner& owner = it->second;
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(owner.pid);
			if (!ch || !ch->GetDesc() || !ch->GetSectree())
			{
				// Gone from this core (logout, a warp): the experience written,
				// the pet away; the core the owner comes to brings it back.
				if (owner.expDelta)
					Flush(NULL, owner, false);
				DestroyPetChar(owner);
				s_owners.erase(it++);
				continue;
			}
			Pet* pet = ActivePet(owner);
			LPCHARACTER mob = PetChar(owner);
			if (pet && pet->summoned)
			{
				if (pet->lifeUntil <= unixNow)
				{
					TItemTable* p = ITEM_MANAGER::instance().GetTable(ITEM_PROTEIN);
					Say(ch, "%s opad\xb3 z si\xb3 i wr\xf3" "ci\xb3. Nakarm go (%s), aby m\xf3g\xb3 wr\xf3" "ci\xe6.",
							pet->name.c_str(), p ? p->szLocaleName : "Proteinowa Przek\xb9ska");
					Unsummon(ch, owner, true);
					SendData(ch, owner, false);
				}
				else if (!mob || mob->GetMapIndex() != ch->GetMapIndex())
				{
					if (!ch->IsObserverMode() && !ch->IsDead())
						Summon(ch, owner, true);
				}
				else if (!ch->IsDead())
					Follow(ch, owner, mob);
			}
			else if (mob)
			{
				DestroyPetChar(owner);
				Recompute(ch);
			}
			if (owner.expDelta && now >= owner.nextFlush)
				Flush(ch, owner, true);
			++it;
		}
		if (s_owners.empty())
		{
			s_pkTick = NULL;
			return 0;
		}
		return PASSES_PER_SEC(1) / 4 ? PASSES_PER_SEC(1) / 4 : 1;
	}

	void EnsureTick()
	{
		if (s_pkTick)
			return;
		newpet_tick_info* info = AllocEventInfo<newpet_tick_info>();
		s_pkTick = event_create(newpet_tick, info, PASSES_PER_SEC(1) / 4 ? PASSES_PER_SEC(1) / 4 : 1);
	}

	bool ValidName(const char* name)
	{
		const size_t len = name ? strlen(name) : 0;
		if (len < 2 || len > PET_NAME_LEN_MAX)
			return false;
		for (size_t i = 0; i < len; ++i)
			if (!isalnum((unsigned char)name[i]))
				return false;
		return !CBanwordManager::instance().CheckString(name, len);
	}

	// A compare-and-set on the chosen pet's row; the memory read again after.
	bool Change(Owner& owner, const Pet& pet, const char* set, const char* where)
	{
		char query[512];
		snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET %s WHERE id=%u AND pid=%u AND %s", set, pet.id, owner.pid, where);
		const bool changed = Exec(query) == 1;
		Reload(owner);
		return changed;
	}

	void TakeOne(LPITEM item)
	{
		if (item)
			item->SetCount(item->GetCount() - 1);
	}

	// ---- the items -------------------------------------------------------

	bool UseEgg(LPCHARACTER ch, LPITEM item)
	{
		if (!FindSpecies(item->GetVnum()))
		{
			Say(ch, "Z tego jajka nic si\xea nie wykluje.");
			return false;
		}
		Owner& owner = GetOwner(ch);
		if (owner.pets.size() >= (size_t)MAX_PETS)
		{
			Say(ch, "Masz ju\xbf %d pety - wypu\x9c\xe6 jednego w oknie peta, aby wykluwa\xe6 nast\xea" "pnego.", (int)MAX_PETS);
			return false;
		}
		if (ch->GetGold() < HATCH_PRICE)
		{
			Say(ch, "Wyklucie kosztuje %s yang.", Money(HATCH_PRICE).c_str());
			return false;
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet Hatch %u %u %lld", (unsigned int)item->GetCell(), item->GetVnum(), HATCH_PRICE);
		return false;
	}

	bool UseProtein(LPCHARACTER ch, LPITEM item, Owner& owner, Pet& pet)
	{
		const DWORD now = (DWORD)time(NULL);
		if (pet.lifeUntil >= now + LIFE_MAX - 3600)
		{
			Say(ch, "Energia \xbfyciowa %s jest pe\xb3na.", pet.name.c_str());
			return false;
		}
		char set[200], where[64];
		snprintf(set, sizeof(set), "life_until = LEAST(GREATEST(life_until, %u) + %u, %u)", now, LIFE_PER_PROTEIN, now + LIFE_MAX);
		snprintf(where, sizeof(where), "life_until = %u", pet.lifeUntil);
		const std::string name = pet.name;
		if (!Change(owner, pet, set, where))
			return false;
		TakeOne(item);
		Say(ch, "%s zjad\xb3 przek\xb9sk\xea: +%u dni energii \xbfyciowej.", name.c_str(), LIFE_PER_PROTEIN / DAY);
		return true;
	}

	bool UseTreat(LPCHARACTER ch, LPITEM item, Owner& owner, Pet& pet, bool percent)
	{
		if (pet.level >= Cap(pet.evolution) && pet.exp + owner.expDelta >= Need(pet.level))
		{
			Say(ch, "%s ma najwy\xbfszy poziom swojej ewolucji - najpierw ewolucja.", pet.name.c_str());
			return false;
		}
		long long amount = item->GetValue(0);
		if (percent)
			amount = (long long)(Need(pet.level) * (unsigned long long)std::max<long>(1L, item->GetValue(0)) / 100);
		if (amount <= 0)
			amount = percent ? (long long)(Need(pet.level) / 20) : 800000;
		TakeOne(item);
		owner.expDelta += (unsigned long long)amount;
		Flush(ch, owner, true);
		Say(ch, "Pet zjad\xb3 smako\xb3yk: +%s do\x9cwiadczenia.", Money(amount).c_str());
		return true;
	}

	bool UseDew(LPCHARACTER ch, LPITEM item, Owner& owner, Pet& pet, int bonus, int step)
	{
		static const char* const SIZE[4] = { "S", "M", "L", "XL" };
		const int have = pet.bonus[bonus];
		if (pet.level < DEW_PET_LEVEL[step])
		{
			Say(ch, "Eliksir (%s) wymaga peta na %d poziomie.", SIZE[step], DEW_PET_LEVEL[step]);
			return false;
		}
		if (have < step * 5 || have >= (step + 1) * 5)
		{
			if (have >= BONUS_LEVEL_MAX)
				Say(ch, "Ten bonus ma ju\xbf najwy\xbfszy poziom.");
			else
				Say(ch, "Ten bonus (poziom %d) podnosi eliksir (%s).", have, SIZE[std::min<int>(3, have / 5)]);
			return false;
		}
		char set[64], where[64];
		snprintf(set, sizeof(set), "bonus%d = bonus%d + 1", bonus, bonus);
		snprintf(where, sizeof(where), "bonus%d = %d", bonus, have);
		if (!Change(owner, pet, set, where))
			return false;
		TakeOne(item);
		Say(ch, "Bonus peta podniesiony do %d poziomu.", have + 1);
		if (IsOut(owner))
			Recompute(ch);
		return true;
	}

	bool ChangeSkills(Owner& owner, Pet& pet, const Pet& changed)
	{
		std::string set = "skills = '" + SkillsText(changed) + "'";
		std::string where = "skills = '" + pet.skillsText + "'";
		return Change(owner, pet, set.c_str(), where.c_str());
	}

	const char* SkillName(int skill, char* buffer, size_t size)
	{
		TItemTable* p = ITEM_MANAGER::instance().GetTable(ITEM_BOOK_FIRST + skill - 1);
		snprintf(buffer, size, "%s", p ? p->szLocaleName : "umiej\xeatno\x9c\xe6");
		return buffer;
	}

	bool UseBook(LPCHARACTER ch, LPITEM item, Owner& owner, Pet& pet)
	{
		const int skill = (int)(item->GetVnum() - ITEM_BOOK_FIRST + 1);
		if (skill < 1 || skill > SKILL_COUNT)
			return false;
		char name[64];
		SkillName(skill, name, sizeof(name));
		Pet changed = pet;
		int slot = -1;
		for (int i = 0; i < SKILL_SLOTS && slot < 0; ++i)
			if (pet.skillType[i] == skill)
				slot = i;
		if (slot >= 0)
		{
			if (pet.skillLevel[slot] >= SKILL_LEVEL_MAX)
			{
				Say(ch, "%s ma ju\xbf najwy\xbfszy poziom.", name);
				return false;
			}
			changed.skillLevel[slot] += 1;
		}
		else
		{
			for (int i = 0; i < SKILL_SLOTS && slot < 0; ++i)
				if (pet.skillType[i] == 0)
					slot = i;
			if (slot < 0)
			{
				Say(ch, "Pet nie ma wolnego miejsca na umiej\xeatno\x9c\xe6 (miejsca otwiera ewolucja).");
				return false;
			}
			changed.skillType[slot] = (BYTE)skill;
			changed.skillLevel[slot] = 1;
		}
		if (!ChangeSkills(owner, pet, changed))
			return false;
		TakeOne(item);
		Say(ch, "%s: poziom %d.", name, (int)changed.skillLevel[slot]);
		if (IsOut(owner))
			Recompute(ch);
		return true;
	}

	bool UseReverti(LPCHARACTER ch, LPITEM item, Owner& owner, Pet& pet)
	{
		Pet changed = pet;
		bool any = false;
		for (int i = 0; i < SKILL_SLOTS; ++i)
			if (pet.skillType[i] != 0 && pet.skillType[i] != SKILL_LOCKED)
			{
				changed.skillType[i] = 0;
				changed.skillLevel[i] = 0;
				any = true;
			}
		if (!any)
		{
			Say(ch, "Pet nie zna \xbf" "adnej umiej\xeatno\x9c" "ci.");
			return false;
		}
		if (!ChangeSkills(owner, pet, changed))
			return false;
		TakeOne(item);
		Say(ch, "Pet zapomnia\xb3 wszystkie umiej\xeatno\x9c" "ci.");
		if (IsOut(owner))
			Recompute(ch);
		return true;
	}

	bool UseOpenSlot(LPCHARACTER ch, LPITEM item, Owner& owner, Pet& pet)
	{
		if (pet.evolution < EVOLUTION_MAX || pet.level < SLOT_UNLOCK_LEVEL)
		{
			Say(ch, "Nowe miejsce na umiej\xeatno\x9c\xe6 otwiera si\xea heroicznemu petowi od %d poziomu.", SLOT_UNLOCK_LEVEL);
			return false;
		}
		Pet changed = pet;
		int slot = -1;
		for (int i = 0; i < SKILL_SLOTS && slot < 0; ++i)
			if (pet.skillType[i] == SKILL_LOCKED)
				slot = i;
		if (slot < 0)
		{
			Say(ch, "Wszystkie miejsca s\xb9 ju\xbf otwarte.");
			return false;
		}
		changed.skillType[slot] = 0;
		changed.skillLevel[slot] = 0;
		if (!ChangeSkills(owner, pet, changed))
			return false;
		TakeOne(item);
		Say(ch, "Otwarto nowe miejsce na umiej\xeatno\x9c\xe6 peta.");
		return true;
	}

	bool UseBookChest(LPCHARACTER ch, LPITEM item)
	{
		const DWORD vnum = ITEM_BOOK_FIRST + number(0, SKILL_COUNT - 1);
		if (!ITEM_MANAGER::instance().GetTable(vnum))
			return false;
		TakeOne(item);
		ch->AutoGiveItem(vnum, 1);
		return true;
	}

	// The chosen pet into a transporter: the row goes to pid 0 (packed) with
	// a conditional UPDATE, and only then the full transporter is given.
	bool UsePack(LPCHARACTER ch, LPITEM item)
	{
		Owner& owner = GetOwner(ch);
		if (owner.expDelta)
			Flush(ch, owner, false);
		Reload(owner);
		Pet* pet = ActivePet(owner);
		if (!pet)
		{
			Say(ch, "Nie masz peta do zapakowania.");
			return false;
		}
		const int cell = ch->GetEmptyInventory(1);
		if (cell < 0)
		{
			Say(ch, "Nie masz miejsca w ekwipunku.");
			return false;
		}
		if (IsOut(owner) || pet->summoned)
			Unsummon(ch, owner, true);
		pet = ActivePet(owner);
		if (!pet)
			return false;
		const DWORD id = pet->id;
		const int level = pet->level;
		const int evolution = pet->evolution;
		const DWORD egg = pet->egg;
		const std::string name = pet->name;
		LPITEM box = ITEM_MANAGER::instance().CreateItem(ITEM_TRANSPORT_FULL, 1);
		if (!box)
		{
			Say(ch, "Transporter jest chwilowo niedost\xeapny.");
			return false;
		}
		char query[256];
		snprintf(query, sizeof(query),
				"UPDATE player.newpet_pet SET pid = 0, packed = 1, active = 0, summoned = 0 WHERE id=%u AND pid=%u AND packed = 0",
				id, owner.pid);
		if (Exec(query) != 1)
		{
			M2_DESTROY_ITEM(box);
			Reload(owner);
			return false;
		}
		if (!box->AddToCharacter(ch, TItemPos(INVENTORY, (WORD)cell)))
		{
			// Never a pet without its transporter: back to its owner.
			snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET pid = %u, packed = 0 WHERE id=%u AND pid = 0", owner.pid, id);
			Exec(query);
			M2_DESTROY_ITEM(box);
			Reload(owner);
			return false;
		}
		box->SetSocket(0, (long)id);
		box->SetSocket(1, (long)(level + 1000 * evolution));
		box->SetSocket(2, (long)egg);
		TakeOne(item);
		Reload(owner);
		Say(ch, "%s jest w transporterze - mo\xbf" "esz go sprzeda\xe6 albo odda\xe6.", name.c_str());
		sys_log(0, "NEWPET: %s packed pet %u (%s, lv %d)", ch->GetName(), id, name.c_str(), level);
		SendData(ch, owner, false);
		return true;
	}

	// A full transporter used: the pet becomes this character's, once.
	bool UseClaim(LPCHARACTER ch, LPITEM item)
	{
		const DWORD id = (DWORD)item->GetSocket(0);
		if (!id)
			return false;
		Owner& owner = GetOwner(ch);
		Reload(owner);
		if (owner.pets.size() >= (size_t)MAX_PETS)
		{
			Say(ch, "Masz ju\xbf %d pety - wypu\x9c\xe6 albo zapakuj jednego, aby przyj\xb9\xe6 nowego.", (int)MAX_PETS);
			return false;
		}
		char query[256];
		snprintf(query, sizeof(query),
				"UPDATE player.newpet_pet SET pid = %u, packed = 0, active = 0, summoned = 0 WHERE id=%u AND pid = 0 AND packed = 1",
				owner.pid, id);
		if (Exec(query) != 1)
		{
			Say(ch, "Ten transporter jest pusty.");
			return false;
		}
		TakeOne(item);
		Reload(owner);
		const Pet* pet = NULL;
		for (size_t i = 0; i < owner.pets.size(); ++i)
			if (owner.pets[i].id == id)
				pet = &owner.pets[i];
		Say(ch, "Masz nowego peta: %s! Wybierz go w oknie peta (U).", pet ? pet->name.c_str() : "pet");
		sys_log(0, "NEWPET: %s took pet %u from a transporter", ch->GetName(), id);
		SendData(ch, owner, true);
		return true;
	}

	// A pet item used from the inventory (the engine's ITEM_PET branch).
	bool UseItem(LPCHARACTER ch, LPITEM item)
	{
		const DWORD vnum = item->GetVnum();
		if (item->GetSubType() == PET_EGG)
			return UseEgg(ch, item);
		if (vnum == ITEM_BOOK_CHEST)
			return UseBookChest(ch, item);
		if (vnum == ITEM_TRANSPORT_EMPTY)
			return UsePack(ch, item);
		if (vnum == ITEM_TRANSPORT_FULL)
			return UseClaim(ch, item);
		Owner& owner = GetOwner(ch);
		Pet* pet = ActivePet(owner);
		if (!pet)
		{
			Say(ch, "Nie masz peta. Wykluj go z jajka.");
			return false;
		}
		if (vnum == ITEM_PROTEIN)
			return UseProtein(ch, item, owner, *pet);
		if (vnum == ITEM_TREAT || vnum == ITEM_TREAT_PLUS)
			return UseTreat(ch, item, owner, *pet, vnum == ITEM_TREAT_PLUS);
		for (int b = 0; b < BONUS_COUNT; ++b)
			if (vnum >= DEW_FIRST[b] && vnum < DEW_FIRST[b] + 4)
				return UseDew(ch, item, owner, *pet, b, (int)(vnum - DEW_FIRST[b]));
		if (vnum >= ITEM_BOOK_FIRST && vnum < ITEM_BOOK_FIRST + SKILL_COUNT)
			return UseBook(ch, item, owner, *pet);
		if (vnum == ITEM_REVERTI)
			return UseReverti(ch, item, owner, *pet);
		if (vnum == ITEM_OPEN_SLOT)
			return UseOpenSlot(ch, item, owner, *pet);
		if (vnum == ITEM_REVERTUS)
		{
			Say(ch, "Otw\xf3rz okno peta i kliknij prawym przyciskiem umiej\xeatno\x9c\xe6 do zapomnienia.");
			SendData(ch, owner, true);
			return false;
		}
		if (vnum == ITEM_RENAME)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet Rename %u %lld", pet->id, RENAME_PRICE);
			return false;
		}
		return false;
	}

	// ---- the commands ----------------------------------------------------

	void Hatch(LPCHARACTER ch, const char* cellText, const char* name)
	{
		int cell = -1;
		str_to_number(cell, cellText);
		LPITEM egg = cell >= 0 ? ch->GetInventoryItem((WORD)cell) : NULL;
		if (!egg || egg->GetType() != ITEM_PET || egg->GetSubType() != PET_EGG || egg->IsExchanging() || !FindSpecies(egg->GetVnum()))
		{
			Say(ch, "Nie ma tu jajka.");
			return;
		}
		if (!ValidName(name))
		{
			Say(ch, "Imi\xea peta: od 2 do %d liter lub cyfr.", (int)PET_NAME_LEN_MAX);
			return;
		}
		Owner& owner = GetOwner(ch);
		Reload(owner);
		if (owner.pets.size() >= (size_t)MAX_PETS)
		{
			Say(ch, "Masz ju\xbf %d pety.", (int)MAX_PETS);
			return;
		}
		if (ch->GetGold() < HATCH_PRICE)
		{
			Say(ch, "Wyklucie kosztuje %s yang.", Money(HATCH_PRICE).c_str());
			return;
		}
		const DWORD eggVnum = egg->GetVnum();
		Pet fresh;
		for (int i = 0; i < SKILL_SLOTS; ++i)
		{
			fresh.skillType[i] = SKILL_LOCKED;
			fresh.skillLevel[i] = 0;
		}
		const DWORD now = (DWORD)time(NULL);
		char query[512];
		snprintf(query, sizeof(query),
				"INSERT INTO player.newpet_pet (pid, egg, name, level, exp, evolution, life_until, skills, active, summoned, created) "
				"VALUES (%u, %u, '%s', 1, 0, 0, %u, '%s', %d, 0, %u)",
				owner.pid, eggVnum, name, now + LIFE_AT_HATCH, SkillsText(fresh).c_str(), ActivePet(owner) ? 0 : 1, now);
		if (Exec(query) != 1)
		{
			Say(ch, "Nie uda\xb3o si\xea wyklu\xe6 peta, spr\xf3" "buj ponownie.");
			return;
		}
		TakeOne(egg);
		ch->ChangeGold(-HATCH_PRICE);
		Reload(owner);
		Say(ch, "Z jajka wykluwa si\xea %s - %s! Przywo\xb3" "aj go w oknie peta.", SpeciesName(eggVnum), name);
		sys_log(0, "NEWPET: %s hatched %u as %s", ch->GetName(), eggVnum, name);
		SendData(ch, owner, true);
	}

	void Rename(LPCHARACTER ch, const char* name)
	{
		Owner& owner = GetOwner(ch);
		Pet* pet = ActivePet(owner);
		if (!pet)
			return;
		if (!ValidName(name))
		{
			Say(ch, "Imi\xea peta: od 2 do %d liter lub cyfr.", (int)PET_NAME_LEN_MAX);
			return;
		}
		if (ch->CountSpecifyItem(ITEM_RENAME) < 1)
		{
			TItemTable* p = ITEM_MANAGER::instance().GetTable(ITEM_RENAME);
			Say(ch, "Potrzebujesz: %s.", p ? p->szLocaleName : "Zw\xf3j Imienia Peta");
			return;
		}
		if (ch->GetGold() < RENAME_PRICE)
		{
			Say(ch, "Zmiana imienia kosztuje %s yang.", Money(RENAME_PRICE).c_str());
			return;
		}
		char set[64];
		snprintf(set, sizeof(set), "name = '%s'", name);
		if (!Change(owner, *pet, set, "1"))
			return;
		ch->RemoveSpecifyItem(ITEM_RENAME, 1);
		ch->ChangeGold(-RENAME_PRICE);
		Say(ch, "Pet nazywa si\xea teraz %s.", name);
		pet = ActivePet(owner);
		if (pet && IsOut(owner))
			SpawnPetChar(ch, owner, *pet);
		SendData(ch, owner, false);
	}

	void Evolve(LPCHARACTER ch)
	{
		Owner& owner = GetOwner(ch);
		if (owner.expDelta)
			Flush(ch, owner, true);
		Pet* pet = ActivePet(owner);
		if (!pet)
			return;
		if (pet->evolution >= EVOLUTION_MAX)
		{
			Say(ch, "%s ma ju\xbf najwy\xbfsz\xb9 ewolucj\xea.", pet->name.c_str());
			return;
		}
		const int cap = Cap(pet->evolution);
		if (pet->level < cap)
		{
			Say(ch, "Ewolucja wymaga %d poziomu peta.", cap);
			return;
		}
		const EvolutionCost& cost = EVOLUTION_COSTS[pet->evolution];
		for (int i = 0; i < EVOLUTION_ITEMS; ++i)
		{
			if (!cost.vnum[i] || !cost.count[i])
				continue;
			if (ch->CountSpecifyItem(cost.vnum[i]) < (ITEM_COUNT)cost.count[i])
			{
				TItemTable* p = ITEM_MANAGER::instance().GetTable(cost.vnum[i]);
				Say(ch, "Ewolucja wymaga: %s x%u.", p ? p->szLocaleName : "?", cost.count[i]);
				return;
			}
		}
		if (ch->GetGold() < cost.gold)
		{
			Say(ch, "Ewolucja kosztuje %s yang.", Money(cost.gold).c_str());
			return;
		}
		// The evolution opens the next skill slot (the mod: slot evolution-1).
		Pet changed = *pet;
		const int slot = pet->evolution;
		if (slot < SKILL_SLOTS && changed.skillType[slot] == SKILL_LOCKED)
		{
			changed.skillType[slot] = 0;
			changed.skillLevel[slot] = 0;
		}
		char set[256], where[256];
		snprintf(set, sizeof(set), "evolution = evolution + 1, skills = '%s'", SkillsText(changed).c_str());
		snprintf(where, sizeof(where), "evolution = %d AND level >= %d AND skills = '%s'", pet->evolution, cap, pet->skillsText.c_str());
		const std::string name = pet->name;
		const int newEvolution = pet->evolution + 1;
		if (!Change(owner, *pet, set, where))
		{
			Say(ch, "Ewolucja nie powiod\xb3" "a si\xea - spr\xf3" "buj ponownie.");
			sys_log(0, "NEWPET: %s evolve of pet %s failed (row changed)", ch->GetName(), name.c_str());
			SendData(ch, owner, false);
			return;
		}
		for (int i = 0; i < EVOLUTION_ITEMS; ++i)
			if (cost.vnum[i] && cost.count[i])
				ch->RemoveSpecifyItem(cost.vnum[i], (ITEM_COUNT)cost.count[i]);
		ch->ChangeGold(-cost.gold);
		Say(ch, "%s ewoluowa\xb3: %s! Otwarto nowe miejsce na umiej\xeatno\x9c\xe6.", name.c_str(), EVOLUTION_NAMES[newEvolution]);
		sys_log(0, "NEWPET: %s evolved pet %s to %d", ch->GetName(), name.c_str(), newEvolution);
		pet = ActivePet(owner);
		if (pet && IsOut(owner))
		{
			SpawnPetChar(ch, owner, *pet);
			LPCHARACTER mob = PetChar(owner);
			if (mob)
				Effect(mob, "d:/ymir work/effect/jin_han/work/efect_duel_jin_han_sender.mse");
		}
		SendData(ch, owner, false);
	}

	void ForgetOne(LPCHARACTER ch, const char* slotText)
	{
		int slot = -1;
		str_to_number(slot, slotText);
		Owner& owner = GetOwner(ch);
		Pet* pet = ActivePet(owner);
		if (!pet || slot < 0 || slot >= SKILL_SLOTS)
			return;
		const int skill = pet->skillType[slot];
		if (skill == 0 || skill == SKILL_LOCKED)
			return;
		if (ch->CountSpecifyItem(ITEM_REVERTUS) < 1)
		{
			TItemTable* p = ITEM_MANAGER::instance().GetTable(ITEM_REVERTUS);
			Say(ch, "Potrzebujesz: %s.", p ? p->szLocaleName : "Pet Revertus");
			return;
		}
		char name[64];
		SkillName(skill, name, sizeof(name));
		Pet changed = *pet;
		changed.skillType[slot] = 0;
		changed.skillLevel[slot] = 0;
		if (!ChangeSkills(owner, *pet, changed))
			return;
		ch->RemoveSpecifyItem(ITEM_REVERTUS, 1);
		Say(ch, "Pet zapomnia\xb3: %s.", name);
		if (IsOut(owner))
			Recompute(ch);
		SendData(ch, owner, false);
	}

	void Select(LPCHARACTER ch, const char* idText)
	{
		DWORD id = 0;
		str_to_number(id, idText);
		Owner& owner = GetOwner(ch);
		bool found = false;
		for (size_t i = 0; i < owner.pets.size(); ++i)
			found = found || owner.pets[i].id == id;
		if (!found)
			return;
		Pet* current = ActivePet(owner);
		if (current && current->id == id)
			return;
		const bool wasOut = IsOut(owner);
		Unsummon(ch, owner, true);
		char query[200];
		snprintf(query, sizeof(query), "UPDATE player.newpet_pet SET active = (id = %u), summoned = 0 WHERE pid=%u", id, owner.pid);
		Exec(query);
		Reload(owner);
		if (wasOut)
			Summon(ch, owner, false);
		SendData(ch, owner, false);
	}

	void Release(LPCHARACTER ch, const char* idText)
	{
		DWORD id = 0;
		str_to_number(id, idText);
		Owner& owner = GetOwner(ch);
		Pet* current = ActivePet(owner);
		if (current && current->id == id)
			Unsummon(ch, owner, true);
		char query[200];
		snprintf(query, sizeof(query), "DELETE FROM player.newpet_pet WHERE id=%u AND pid=%u", id, owner.pid);
		if (Exec(query) == 1)
		{
			Say(ch, "Pet zosta\xb3 wypuszczony na wolno\x9c\xe6.");
			sys_log(0, "NEWPET: %s released pet %u", ch->GetName(), id);
		}
		Reload(owner);
		SendData(ch, owner, false);
	}

	void Command(LPCHARACTER ch, const char* argument)
	{
		if (!Eligible(ch))
			return;
		char sub[32], arg1[64], arg2[64];
		const char* rest = one_argument(argument, sub, sizeof(sub));
		rest = one_argument(rest, arg1, sizeof(arg1));
		one_argument(rest, arg2, sizeof(arg2));
		if (!EnsureTables())
		{
			Say(ch, "System pet\xf3w jest chwilowo niedost\xeapny.");
			return;
		}
		Owner& owner = GetOwner(ch);
		const DWORD now = get_dword_time();
		const bool refresh = !strcmp(sub, "refresh");
		if (refresh)
		{
			if (now - owner.lastRefresh < 1000)
				return;
			owner.lastRefresh = now;
			SendData(ch, owner, false);
			return;
		}
		if (!strcmp(sub, "info"))
		{
			// The name of a pet in a transporter, for its tooltip.
			DWORD id = 0;
			str_to_number(id, arg1);
			if (!id || now - owner.lastInfo < 200)
				return;
			owner.lastInfo = now;
			char query[160];
			snprintf(query, sizeof(query), "SELECT name FROM player.newpet_pet WHERE id=%u AND packed = 1", id);
			std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
			std::string name;
			if (msg.get() && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->pSQLResult)
			{
				MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
				unsigned long* lengths = row ? mysql_fetch_lengths(msg->Get()->pSQLResult) : NULL;
				if (row && row[0] && lengths)
					name.assign(row[0], lengths[0]);
			}
			ch->ChatPacket(CHAT_TYPE_COMMAND, "NewPet Info %u %s", id, HexOf(name).c_str());
			return;
		}
		if (now - owner.lastCommand < 300)
			return;
		owner.lastCommand = now;
		if (!*sub || !strcmp(sub, "open"))
		{
			Reload(owner);
			SendEvolutionCosts(ch);
			SendData(ch, owner, true);
		}
		else if (!strcmp(sub, "sync"))
		{
			// After every loading screen: the chosen pet back if it was out.
			Reload(owner);
			Pet* pet = ActivePet(owner);
			if (pet && pet->summoned && !IsOut(owner))
				Summon(ch, owner, true);
			SendEvolutionCosts(ch);
			SendData(ch, owner, false);
		}
		else if (!strcmp(sub, "summon") || !strcmp(sub, "toggle"))
		{
			if (IsOut(owner))
				Unsummon(ch, owner, true);
			else
			{
				Reload(owner);
				Summon(ch, owner, false);
			}
			SendData(ch, owner, false);
		}
		else if (!strcmp(sub, "unsummon"))
		{
			Unsummon(ch, owner, true);
			SendData(ch, owner, false);
		}
		else if (!strcmp(sub, "hatch"))
			Hatch(ch, arg1, arg2);
		else if (!strcmp(sub, "rename"))
			Rename(ch, arg1);
		else if (!strcmp(sub, "evolve"))
		{
			// "evolve <id>": the window's shown pet, chosen first (a "select"
			// sent right before it fell under the 300 ms limit above).
			if (*arg1)
				Select(ch, arg1);
			Evolve(ch);
		}
		else if (!strcmp(sub, "skilldel"))
			ForgetOne(ch, arg1);
		else if (!strcmp(sub, "select"))
			Select(ch, arg1);
		else if (!strcmp(sub, "release"))
			Release(ch, arg1);
		else if (ch->GetGMLevel() > GM_PLAYER && !strcmp(sub, "gmexp"))
		{
			unsigned long long amount = strtoull(arg1, NULL, 10);
			if (ActivePet(owner) && amount)
			{
				owner.expDelta += amount;
				Flush(ch, owner, true);
				SendData(ch, owner, false);
			}
		}
		else if (ch->GetGMLevel() > GM_PLAYER && !strcmp(sub, "gmlevel"))
		{
			int level = 0;
			str_to_number(level, arg1);
			Pet* pet = ActivePet(owner);
			if (pet && level >= 1 && level <= LEVEL_CAPS[EVOLUTION_MAX])
			{
				char set[64];
				snprintf(set, sizeof(set), "level = %d, exp = 0", level);
				Change(owner, *pet, set, "1");
				owner.expDelta = 0;
				SendData(ch, owner, false);
			}
		}
		else if (ch->GetGMLevel() > GM_PLAYER && !strcmp(sub, "gmegg"))
		{
			for (size_t i = 0; i < SPECIES_COUNT; ++i)
				ch->AutoGiveItem(SPECIES[i].egg, 1);
			ch->AutoGiveItem(ITEM_TRANSPORT_EMPTY, 2);
		}
	}

	void GroundDrop(LPCHARACTER victim, LPCHARACTER killer, DWORD vnum)
	{
		if (!ITEM_MANAGER::instance().GetTable(vnum))
			return;
		LPITEM item = ITEM_MANAGER::instance().CreateItem(vnum, 1);
		if (!item)
			return;
		PIXEL_POSITION pos;
		pos.x = victim->GetX() + number(-80, 80);
		pos.y = victim->GetY() + number(-80, 80);
		pos.z = 0;
		if (!item->AddToGround(victim->GetMapIndex(), pos))
		{
			M2_DESTROY_ITEM(item);
			return;
		}
		item->SetOwnership(killer);
		item->StartDestroyEvent();
		sys_log(0, "NEWPET: drop %u for %s from %u", vnum, killer->GetName(), victim->GetRaceNum());
	}

	DWORD DropVnum(DWORD code)
	{
		if (code == DROP_BOOK)
			return ITEM_BOOK_FIRST + number(0, SKILL_COUNT - 1);
		if (code >= DROP_ELIXIR_S && code <= DROP_ELIXIR_XL)
			return DEW_FIRST[number(0, BONUS_COUNT - 1)] + (code - DROP_ELIXIR_S);
		return code;
	}

	// Once per victim: GiveExp runs for every member of a party that shares
	// the kill, and the first of them rolls (and owns) the drop.
	bool FirstRoll(LPCHARACTER victim)
	{
		static DWORD s_lastVid = 0;
		static DWORD s_lastAt = 0;
		const DWORD now = get_dword_time();
		if (victim->GetVID() == s_lastVid && now - s_lastAt < 3000)
			return false;
		s_lastVid = victim->GetVID();
		s_lastAt = now;
		return true;
	}

	void RollDrops(LPCHARACTER victim, LPCHARACTER killer)
	{
		if (!victim || victim->IsPC() || !(victim->IsMonster() || victim->IsStone()) || victim->GetMapIndex() <= 0)
			return;
		if (!FirstRoll(victim))
			return;
		const int kind = victim->IsStone() ? 1 : (victim->GetMobRank() >= MOB_RANK_BOSS ? 2 : 0);
		if (victim->GetLevel() >= 10 && victim->GetLevel() + 15 >= killer->GetLevel()
				&& EGG_ONE_IN[kind] > 0 && number(1, EGG_ONE_IN[kind]) == 1)
			GroundDrop(victim, killer, SPECIES[number(0, (int)SPECIES_COUNT - 1)].egg);
		if (victim->IsStone() && victim->GetLevel() >= METIN_ITEM_MIN_LEVEL && number(1, 1000) <= METIN_ITEM_PER_MILLE)
		{
			int total = 0;
			for (size_t i = 0; i < sizeof(METIN_DROPS) / sizeof(METIN_DROPS[0]); ++i)
				total += METIN_DROPS[i].weight;
			int roll = number(1, std::max(1, total));
			for (size_t i = 0; i < sizeof(METIN_DROPS) / sizeof(METIN_DROPS[0]); ++i)
			{
				roll -= METIN_DROPS[i].weight;
				if (roll <= 0)
				{
					GroundDrop(victim, killer, DropVnum(METIN_DROPS[i].vnum));
					break;
				}
			}
		}
	}

}

// ---- The engine's calls (server-patches/playerqol, MT2009_PLUS_NEW_PET_V1) --

// char_item.cpp, UseItemEx: an ITEM_PET item other than the ItemShop seals.
bool NewPetUseItem(LPCHARACTER ch, LPITEM item)
{
	if (!ch || !item || !mt2009_newpet::Eligible(ch))
		return false;
	if (!mt2009_newpet::EnsureTables())
		return false;
	return mt2009_newpet::UseItem(ch, item);
}

// char_battle.cpp, GiveExp: the experience a kill gave the owner - the
// chosen pet's too while it is out - and the pet items' drops.
void NewPetOnExp(LPCHARACTER victim, LPCHARACTER to, int exp)
{
	using namespace mt2009_newpet;
	if (!to || !Eligible(to))
		return;
	RollDrops(victim, to);
	if (exp <= 0)
		return;
	Owner* owner = FindOwner(to->GetPlayerID());
	if (!owner || !IsOut(*owner))
		return;
	AddExp(to, *owner, (unsigned long long)exp * EXP_GAIN_PERCENT / 100);
}

// char.cpp, ComputePoints: the pet's bonuses on its owner while it is out.
void NewPetApplyPoints(LPCHARACTER ch)
{
	using namespace mt2009_newpet;
	if (!ch || !ch->IsPC())
		return;
	Owner* owner = FindOwner(ch->GetPlayerID());
	if (!owner || !IsOut(*owner))
		return;
	Pet* pet = ActivePet(*owner);
	if (!pet || pet->lifeUntil <= (DWORD)time(NULL))
		return;
	for (int b = 0; b < BONUS_COUNT; ++b)
	{
		const int value = BonusValue(pet->bonus[b], BONUSES[b].max);
		if (value)
			ch->PointChange(BONUSES[b].point, value);
	}
	for (int i = 0; i < SKILL_SLOTS; ++i)
	{
		const int skill = pet->skillType[i];
		if (skill < 1 || skill > SKILL_COUNT || !pet->skillLevel[i])
			continue;
		const SkillDef& def = SKILLS[skill];
		const int value = BonusValue(pet->skillLevel[i], def.max);
		for (int k = 0; k < 3 && value; ++k)
			if (def.points[k])
				ch->PointChange(def.points[k], value);
	}
}

// "/newpet [open | sync | summon | unsummon | hatch <cell> <name> | rename
// <name> | evolve [<id>] | skilldel <slot> | select <id> | release <id> | refresh |
// info <id>]",
// the window's (uinewpet.py); a GM also has gmexp <n>, gmlevel <n>, gmegg.
ACMD(do_newpet)
{
	if (!ch || !ch->GetDesc())
		return;
	mt2009_newpet::Command(ch, argument);
}

#endif
