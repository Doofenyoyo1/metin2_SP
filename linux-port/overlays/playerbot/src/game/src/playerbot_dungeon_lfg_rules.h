#ifndef __INC_METIN2_PLAYERBOT_DUNGEON_LFG_RULES_H__
#define __INC_METIN2_PLAYERBOT_DUNGEON_LFG_RULES_H__

// MT2009_PLUS_BOT_DUNGEON_LFG_V1 - the bots' dungeon finder (the pure half).
//
// The owner, 4 October: "Robimy wyszukiwarke botow na dungi" - a person writes
// "chce isc na dunga biblioteka", "ktos na biblioteke?", "szukam ekipy na
// smoka", "lf dung malpy" in the chat, and bots of the dungeon's level whisper
// "Czesc, moge isc z toba, mam 47 lvl, sura wp, moge przyjsc?". A yes and the
// bot is at the entrance ("Czekam pod wejsciem, bede tutaj 5 minut"), a no and
// it lets it be.
//
// This file is the words, and nothing but the words, so it can be tested
// without the engine:
//   - ParseCall: is a public line a call for a dungeon party, and which
//     dungeon (the players' names for them, their Polish endings, typos, no
//     Polish letters). A trade line ("kupie ksiege biblioteki", "K> klucz do
//     biblioteki", "szukam klucza") is never one, a past tense ("bylem na
//     bibliotece") never, a question how to do it ("jak przejsc biblioteke?")
//     never; "szukam" is a call only with a group's word after it ("szukam
//     ekipy").
//   - ParseYesNo / FindDungeonKey: the person's whispered answer to the
//     bot's offer ("tak", "jasne", "chodz", "dawaj", "ok" / "nie", "nie
//     trzeba", "juz mam ekipe"), and the dungeon named to "na jaki?".
//   - the bot's lines: the offer, "na jaki dung?", the wait at the entrance,
//     the okay to a no, the leave after five minutes, each from parts that
//     combine into hundreds of variants - the greeting, how the level and the
//     class's path are said ("sura wp", "woj body", "ninja luk", "szaman
//     heal"), the question - in the bots' chat hand: lowercase often, Polish
//     letters for some bots and not for others (the markup below).
//
// The engine half (playerbot_dungeon_lfg.h) hears the chat, picks the bots,
// teleports and holds them; the conversation (playerbot_conv_memory.h,
// TConvMemory::lfg) reads the answer.
//
// Polish letters are written here as the base letter after a backtick: "`a"
// is a-ogonek, "`z" z-dot, "`x" z-acute, "`S" S-acute. Render() turns them
// into CP1250 - what the client shows - for a bot whose hand writes them, and
// drops the backtick for the rest. This file itself stays ASCII.

#include "playerbot_conv_text.h"

namespace playerbot_lfg
{
	typedef unsigned int u32;
	using playerbot_conv::TTokens;
	using playerbot_conv::TRng;

	// An offer (or "na jaki dung?") is answered within this, or it lapses.
	const u32 OFFER_TTL_MS = 3 * 60 * 1000;
	// The wait at the entrance.
	const u32 WAIT_MS = 5 * 60 * 1000;
	// A whispered answer longer than this is a conversation, not a yes or no.
	const size_t MAX_ANSWER_WORDS = 14;
	// A call longer than this is a story, not a call.
	const size_t MAX_CALL_WORDS = 24;

	// ------------------------------------------------------------ the talk

	// What the bot has asked the person, kept in the conversation's memory of
	// the pair (TConvMemory::lfg) so the next whisper is read as its answer.
	enum ETalk
	{
		TALK_NONE = 0,
		TALK_ASKED,     // "na jaki dung?" - a call that named none
		TALK_OFFERED,   // "moge przyjsc?"
		TALK_WAITING    // at the entrance, the five minutes
	};

	struct TTalk
	{
		unsigned char state;
		std::string key;
		u32 at;
		TTalk() : state(TALK_NONE), at(0) {}
		bool Live(u32 now) const
		{
			if (state == TALK_NONE || at == 0)
				return false;
			if (state == TALK_WAITING)
				return now - at < WAIT_MS + 3 * 60 * 1000;
			return now - at < OFFER_TTL_MS;
		}
	};

	// The answer a whisper carries (TAnalysis::lfgAnswer).
	enum EAnswer
	{
		ANSWER_NONE = 0,
		ANSWER_YES,
		ANSWER_NO,
		ANSWER_CHOOSE,  // a dungeon named after "na jaki?"
		ANSWER_WHICH    // a yes to "na jaki?" that names none
	};

	// What the engine did with a yes (IConvWorld::LfgAccept).
	enum EGo
	{
		GO_GONE = 0,      // the offer is no longer the engine's: the bot moved on
		GO_TELEPORTED,    // at the entrance now
		GO_WALKING,       // on the entrance's map already, walking there
		GO_ALREADY,       // waiting there already
		GO_FAILED         // the move was refused
	};

	// What the engine said to a dungeon named after "na jaki?"
	// (IConvWorld::LfgChoose).
	enum EChoose
	{
		CHOOSE_OK = 0,
		CHOOSE_LOW,       // the bot is below the dungeon's level
		CHOOSE_HIGH,      // ... or more than fifteen above it
		CHOOSE_UNKNOWN    // not a dungeon this core can send a bot to
	};

	// ------------------------------------------------------------ the dungeons

	// The dungeons the bots know by name. The panel's (dungeon_info.txt) by
	// its key; the open maps that left the panel on 30 September - the Monkey
	// Dungeons and the Spider Dungeon - by keys of their own. Whether one is
	// there at all (Classic strips rows, the Arezzo module may be closed, a
	// core may not host the map) is the engine's to ask; a name here that the
	// world does not have is answered by nobody.
	//   na:  "na X" (the accusative), how a bot says where it goes
	//   gen: "do X" (the genitive), how it says the entrance
	struct TDungeonWords
	{
		const char* key;
		const char* na[3];
		const char* gen[2];
	};

	inline const TDungeonWords* Words(const std::string& key)
	{
		static const TDungeonWords k[] = {
			{ "biblioteka", { "bibliotek`e", "biblio", "bibliotek`e wiedzy" }, { "biblioteki", "biblioteki" } },
			{ "wieza", { "wie`z`e", "wie`z`e demon`ow", "dt" }, { "wie`zy", "wie`zy demon`ow" } },
			{ "wukong", { "wukonga", "wzg`orze wukonga", "wukonga" }, { "wukonga", "wzg`orza wukonga" } },
			{ "razador", { "razadora", "czy`sciec ognia", "razadora" }, { "razadora", "czy`s`cca ognia" } },
			{ "skorpion", { "skorpiona", "ruiny skorpiona", "skorpiona" }, { "ruin skorpiona", "skorpiona" } },
			{ "katakumby", { "katakumby", "kata", "katakumby diab`la" }, { "katakumb", "katakumb" } },
			{ "nemere", { "nemere", "nemere", "lodow`a krain`e" }, { "nemere", "lodowej krainy" } },
			{ "smok", { "smoka", "smoka", "berana" }, { "le`za smoka", "smoka" } },
			{ "dzungla", { "d`zungl`e", "staro`zytn`a d`zungl`e", "d`zungl`e" }, { "d`zungli", "d`zungli" } },
			{ "malpy1", { "ma`lpy", "`latwe ma`lpy", "dung ma`lp" }, { "ma`lp", "lochu ma`lp" } },
			{ "malpy2", { "`srednie ma`lpy", "ma`lpy", "dung ma`lp" }, { "ma`lp", "lochu ma`lp" } },
			{ "malpy3", { "trudne ma`lpy", "ma`lpy", "dung ma`lp" }, { "ma`lp", "lochu ma`lp" } },
			{ "pajaki", { "paj`aki", "loch paj`ak`ow", "paj`aki" }, { "lochu paj`ak`ow", "paj`ak`ow" } },
			{ "pajaki2", { "paj`aki 2", "baronow`a", "paj`aki" }, { "lochu paj`ak`ow", "paj`ak`ow" } },
		};
		for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i)
			if (key == k[i].key)
				return &k[i];
		return NULL;
	}

	// --------------------------------------------------------- word helpers

	inline bool StartsWith(const std::string& w, const char* prefix)
	{
		const size_t n = strlen(prefix);
		return w.size() >= n && w.compare(0, n, prefix) == 0;
	}

	inline bool IsOneOf(const std::string& w, const char* const* list, size_t n)
	{
		for (size_t i = 0; i < n; ++i)
			if (w == list[i])
				return true;
		return false;
	}

	inline bool StartsWithOneOf(const std::string& w, const char* const* list, size_t n)
	{
		for (size_t i = 0; i < n; ++i)
			if (StartsWith(w, list[i]))
				return true;
		return false;
	}

#define LFG_ONE_OF(w, arr) IsOneOf((w), (arr), sizeof(arr) / sizeof((arr)[0]))
#define LFG_STARTS_ONE_OF(w, arr) StartsWithOneOf((w), (arr), sizeof(arr) / sizeof((arr)[0]))

	// Optimal string alignment distance, for words of a few letters.
	inline int EditDistance(const std::string& a, const std::string& b)
	{
		const size_t n = a.size(), m = b.size();
		if (n > 24 || m > 24)
			return 99;
		int d[25][25];
		for (size_t i = 0; i <= n; ++i)
			d[i][0] = (int)i;
		for (size_t j = 0; j <= m; ++j)
			d[0][j] = (int)j;
		for (size_t i = 1; i <= n; ++i)
			for (size_t j = 1; j <= m; ++j)
			{
				const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
				int v = d[i - 1][j] + 1;
				if (d[i][j - 1] + 1 < v)
					v = d[i][j - 1] + 1;
				if (d[i - 1][j - 1] + cost < v)
					v = d[i - 1][j - 1] + cost;
				if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1] && d[i - 2][j - 2] + 1 < v)
					v = d[i - 2][j - 2] + 1;
				d[i][j] = v;
			}
		return d[n][m];
	}

	// The word begins with the stem, or with something one typo away from it
	// ("bibiloteke", "razdora", "wokonga"). Only for stems of six letters and
	// more, and only with the first letter right: a short word is too easy to
	// mistake for another.
	inline bool NearStem(const std::string& w, const char* stem)
	{
		if (StartsWith(w, stem))
			return true;
		const size_t n = strlen(stem);
		if (n < 6 || w.size() + 1 < n || w.empty() || w[0] != stem[0])
			return false;
		for (size_t len = n - 1; len <= n + 1; ++len)
		{
			if (len > w.size())
				break;
			if (EditDistance(w.substr(0, len), stem) <= 1)
				return true;
		}
		return false;
	}

	// The generic words: "dung", "dunga", "dungeon", "loch", "instancja".
	inline bool IsGenericDungeonWord(const std::string& w)
	{
		static const char* const exact[] = { "loch", "lochu", "lochy", "lochow", "lochach", "lochem", "dg", "dangi" };
		return StartsWith(w, "dung") || StartsWith(w, "dang") || StartsWith(w, "instancj") || LFG_ONE_OF(w, exact);
	}

	// Monkey Dungeon difficulty a word names: 1 easy, 2 medium, 3 hard.
	inline int DifficultyWord(const std::string& w)
	{
		if (StartsWith(w, "latw") || w == "easy" || w == "1" || w == "i")
			return 1;
		if (StartsWith(w, "sredn") || w == "medium" || StartsWith(w, "normal") || w == "2" || w == "ii")
			return 2;
		if (StartsWith(w, "trudn") || w == "hard" || StartsWith(w, "expert") || StartsWith(w, "eksper") || w == "3" ||
				w == "iii")
			return 3;
		return 0;
	}

	// Which dungeon word i of the line names, "" for none. `difficulty` is
	// the Monkey Dungeon's when the line says it. "malpy" comes back as
	// "malpy" - which of the three is the engine's, by the person's level.
	inline std::string DungeonAt(const std::vector<std::string>& w, size_t i, int& difficulty)
	{
		const std::string& x = w[i];
		const std::string prev = i > 0 ? w[i - 1] : std::string();
		const std::string prev2 = i > 1 ? w[i - 2] : std::string();
		const std::string next = i + 1 < w.size() ? w[i + 1] : std::string();
		if (StartsWith(x, "bibl") || NearStem(x, "bibliotek"))
			return "biblioteka";
		{
			static const char* const tower[] = { "wieza", "wieze", "wiezy", "wiezie", "wieza demonow", "dt", "wiezyczke",
				"wiezyczka", "tower" };
			if (LFG_ONE_OF(x, tower))
				return "wieza";
			if ((x == "devil" || x == "devils" || x == "diabla") && (next == "tower" || StartsWith(next, "wiez")))
				return "wieza";
		}
		if (StartsWith(x, "wukon") || NearStem(x, "wukong"))
			return "wukong";
		if (StartsWith(x, "razad") || NearStem(x, "razador") || StartsWith(x, "czysciec") || StartsWith(x, "czyscca"))
			return "razador";
		if (StartsWith(x, "skorp") || NearStem(x, "skorpion"))
			return "skorpion";
		if (StartsWith(x, "katakumb") || NearStem(x, "katakumby") || x == "kata" || x == "katy" || x == "kate" ||
				StartsWith(x, "azrael") || StartsWith(x, "catacomb"))
			return "katakumby";
		if (StartsWith(x, "nemer") || (StartsWith(x, "lodow") && StartsWith(next, "krain")))
			return "nemere";
		{
			static const char* const dragon[] = { "smok", "smoka", "smoku", "smokiem", "smoczka" };
			const bool shaman = StartsWith(prev, "szam") || StartsWith(prev2, "szam");
			if (LFG_ONE_OF(x, dragon) && !shaman)
				return "smok";
			if (StartsWith(x, "beran") || StartsWith(x, "setao"))
				return "smok";
			if ((x == "leze" || x == "lezu" || x == "leza" || x == "lezem") && StartsWith(next, "smok"))
				return "smok";
		}
		if (StartsWith(x, "dzungl") || StartsWith(x, "jungl"))
			return "dzungla";
		if (StartsWith(x, "malp") || StartsWith(x, "monkey"))
		{
			const int d1 = DifficultyWord(prev);
			const int d2 = DifficultyWord(next);
			if (d1)
				difficulty = d1;
			else if (d2)
				difficulty = d2;
			return "malpy";
		}
		if (StartsWith(x, "baron") || StartsWith(x, "barani"))
			return "pajaki2";
		if (StartsWith(x, "pajak") || StartsWith(x, "pajecz") || StartsWith(x, "spider"))
		{
			if (next == "2" || next == "ii" || next == "v2" || StartsWith(next, "drug") || StartsWith(next, "dwojk"))
				return "pajaki2";
			return "pajaki";
		}
		return std::string();
	}

	// The first dungeon a line names, "" for none.
	inline std::string FindDungeonKey(const std::vector<std::string>& w, int& difficulty, size_t* at = NULL)
	{
		difficulty = 0;
		for (size_t i = 0; i < w.size(); ++i)
		{
			const std::string key = DungeonAt(w, i, difficulty);
			if (!key.empty())
			{
				// "malpy" said apart from its difficulty: "trudne dung malpy".
				if (key == "malpy" && difficulty == 0)
					for (size_t k = 0; k < w.size() && difficulty == 0; ++k)
						if (k + 1 != i && k != i + 1 && (StartsWith(w[k], "latw") || StartsWith(w[k], "sredn") ||
								StartsWith(w[k], "trudn") || w[k] == "easy" || w[k] == "hard" || w[k] == "medium"))
							difficulty = DifficultyWord(w[k]);
				if (at)
					*at = i;
				return key;
			}
		}
		return std::string();
	}

	inline std::string FindDungeonKey(const TTokens& t, int& difficulty)
	{
		return FindDungeonKey(t.words, difficulty);
	}

	// ------------------------------------------------------------- the call

	enum ECall
	{
		CALL_NONE = 0,
		CALL_DUNGEON,   // a dungeon named
		CALL_ANY        // "kto na dunga?" - none named
	};

	struct TCall
	{
		int kind;
		std::string key;
		int difficulty;
		TCall() : kind(CALL_NONE), difficulty(0) {}
	};

	// "K> ...", "S> ...", "B> ..." at the start of the raw line - the trade
	// chat's marks, which the normalisation folds away.
	inline bool RawTradeMark(const char* raw)
	{
		if (!raw)
			return false;
		while (*raw == ' ' || *raw == '\t')
			++raw;
		const char c = (char)(*raw >= 'A' && *raw <= 'Z' ? *raw - 'A' + 'a' : *raw);
		if (c != 'k' && c != 's' && c != 'b')
			return false;
		const char* p = raw + 1;
		while (*p == ' ')
			++p;
		return *p == '>';
	}

	// Is the public line a call for company to a dungeon? See the header for
	// what is and what is not.
	inline bool ParseCall(const char* raw, TCall& out)
	{
		out = TCall();
		if (!raw || !*raw || RawTradeMark(raw))
			return false;
		TTokens t;
		playerbot_conv::Normalize(raw, t);
		const std::vector<std::string>& w = t.words;
		if (w.empty() || w.size() > MAX_CALL_WORDS)
			return false;

		// The dungeon, or the generic word.
		size_t at = 0;
		int difficulty = 0;
		const std::string key = FindDungeonKey(w, difficulty, &at);
		bool generic = false;
		size_t genericAt = 0;
		for (size_t i = 0; i < w.size(); ++i)
			if (IsGenericDungeonWord(w[i]))
			{
				generic = true;
				genericAt = i;
				break;
			}
		if (key.empty() && !generic)
			return false;

		// Never a trade line. The normalisation already turned "wts"/"wtb"/"kt"
		// into "sprzedam"/"kupie".
		static const char* const trade[] = { "kupie", "kupuje", "sprzedam", "sprzedaje", "oddam", "wymienie", "zamienie",
			"kupi", "sprzeda", "kupisz", "sprzedasz", "yang", "kk", "won", "wony", "wonow", "cena", "cene", "tanio", "drogo" };
		static const char* const items[] = { "klucz", "ksieg", "zwoj", "kamien", "pierscien", "miecz", "zbroj", "helm",
			"tarcz", "naszyjnik", "bransolet", "kolczyk", "mikstur", "eliksir", "skrzyn", "szkatul" };
		static const char* const past[] = { "bylem", "bylam", "byles", "bylas", "byl", "byla", "bylo", "byliscie",
			"zrobilem", "zrobilam", "zrobiles", "zrobil", "przeszedlem", "przeszlam", "przeszedl", "wbilem", "wbilam",
			"wypadlo", "wylecialo", "dropnelo", "dropilo", "padl", "padlem", "ubilem", "zabilem", "skonczylem" };
		static const char* const groupWords[] = { "pt", "ludzi", "ludzie", "osob", "osoby", "osobe", "kogos", "partnera",
			"kompana", "towarzystwa", "towarzysza", "chetnych", "squad", "squadu" };
		static const char* const groupStems[] = { "ekip", "druzyn", "grup", "team" };
		static const char* const invite[] = { "idziemy", "lecimy", "wbijamy", "robimy", "pojdziemy", "zrobimy",
			"chodzcie", "idzcie", "wchodzimy", "zapraszam", "zbieram", "zbieramy", "skladam", "skladamy", "ogarniamy",
			"bijemy", "ubijemy", "startujemy", "ruszamy", "wbijajcie", "dolaczcie" };
		static const char* const goVerbs[] = { "isc", "pojsc", "idzie", "idziesz", "idziemy", "ide", "pojde", "pojdzie",
			"pojdziesz", "pojdziemy", "wbija", "wbijasz", "wbijam", "wbijamy", "wbic", "wbije", "leci", "lecisz", "lece",
			"lecimy", "robi", "robisz", "robimy", "robie", "robic", "zrobi", "zrobic", "zrobimy", "wejdzie", "wejsc",
			"wchodzi", "wchodzimy", "chodz", "chodzcie", "idzcie", "przejsc", "przejdzie", "bic", "bijemy", "ubic",
			"ubijemy", "zabic", "ogarnac", "ogarniemy", "dolaczy", "dolaczyc", "dolaczysz", "skoczy", "skoczysz",
			"wpadnie", "wpadniesz" };
		static const char* const want[] = { "chce", "chcem", "chcialbym", "chcialabym", "chcemy", "chcecie", "chcesz",
			"ochote", "ochota", "chec" };
		static const char* const goSelf[] = { "ide", "pojde", "wbijam", "lece", "robie", "zaczynam", "ruszam", "startuje" };
		static const char* const help[] = { "pomoze", "pomozecie", "pomozesz", "pomocy", "pomoc", "help", "pomozcie" };
		static const char* const who[] = { "kto", "ktos", "ktokolwiek", "ktoz", "chetny", "chetni", "chetna" };

		bool tradeWord = false, itemWord = false, pastWord = false, group = false, inviteWord = false, goVerb = false;
		bool wantWord = false, goSelfWord = false, helpWord = false, whoWord = false, chetny = false, lf = false;
		bool need = false, kmMa = false, chance = false;
		for (size_t i = 0; i < w.size(); ++i)
		{
			const std::string& x = w[i];
			if (LFG_ONE_OF(x, trade))
				tradeWord = true;
			if (LFG_STARTS_ONE_OF(x, items))
				itemWord = true;
			if (LFG_ONE_OF(x, past))
				pastWord = true;
			if (LFG_ONE_OF(x, groupWords) || LFG_STARTS_ONE_OF(x, groupStems))
				group = true;
			if (LFG_ONE_OF(x, invite))
				inviteWord = true;
			if (LFG_ONE_OF(x, goVerbs))
				goVerb = true;
			if (LFG_ONE_OF(x, want))
				wantWord = true;
			if (LFG_ONE_OF(x, goSelf))
				goSelfWord = true;
			if (LFG_ONE_OF(x, help))
				helpWord = true;
			if (LFG_ONE_OF(x, who))
				whoWord = true;
			if (StartsWith(x, "chetn") && x != "chetnie")
				chetny = true;
			// "lf", "lfg", "lfm", "lf1m", "lf2m"
			if (x == "lfg" || x == "lfm" || x == "lf" || (StartsWith(x, "lf") && x.size() <= 4 && x.size() >= 3 &&
					((x[2] >= '0' && x[2] <= '9') || x[2] == 'p')))
				lf = true;
			if (StartsWith(x, "potrzeb") || StartsWith(x, "brak") || StartsWith(x, "szuk"))
				need = true;
			// "kto ma", "ktos ma", "ma ktos": a trade question, unless it is
			// "kto ma ochote/czas".
			if (x == "ma" && ((i > 0 && (w[i - 1] == "kto" || w[i - 1] == "ktos")) ||
					(i + 1 < w.size() && w[i + 1] == "ktos")))
				kmMa = true;
			if (StartsWith(x, "ochot") || StartsWith(x, "czas") || x == "chec")
				chance = true;
		}
		if (tradeWord || pastWord)
			return false;
		if (kmMa && !chance)
			return false;
		// "na" right before the dungeon ("na biblioteke", "na dunga
		// biblioteke"), or before the generic word.
		bool na = false;
		{
			const size_t pos = !key.empty() ? at : genericAt;
			for (size_t back = 1; back <= 2 && back <= pos; ++back)
			{
				const std::string& b = w[pos - back];
				if (b == "na" || b == "pod")
				{
					na = true;
					break;
				}
				if (!IsGenericDungeonWord(b))
					break;
			}
		}
		// An item named, and nothing that only a call for company says: a
		// trade or a question about the dungeon's loot.
		const bool strong = lf || chetny || group || inviteWord;
		if (itemWord && !strong)
			return false;
		const bool call = lf || chetny || inviteWord ||
				(group && (need || whoWord || goVerb || wantWord || na || t.question)) ||
				(whoWord && (goVerb || na || wantWord || helpWord || chance || (t.question && w.size() <= 6))) ||
				(wantWord && (goVerb || na)) ||
				(goSelfWord && na) ||
				(helpWord && na) ||
				(need && na && !itemWord);
		if (!call)
			return false;
		out.kind = key.empty() ? CALL_ANY : CALL_DUNGEON;
		out.key = key;
		out.difficulty = difficulty;
		return true;
	}

	// ------------------------------------------------------------ yes or no

	// The person's answer to "moge przyjsc?": 1 yes, -1 no, 0 neither (a
	// question of its own, "nie wiem", "dzieki"), which the conversation
	// answers as it would any line.
	inline int ParseYesNo(const TTokens& t)
	{
		std::vector<std::string> w = t.words;
		if (w.empty() || w.size() > MAX_ANSWER_WORDS)
			return 0;
		bool strongNo = false, strongYes = false, weakYes = false, plainNo = false;
		static const char* const strongYesWords[] = { "tak", "jasne", "pewnie", "pewka", "oczywiscie", "chodz", "chodzze",
			"choc", "przyjdz", "przychodz", "przylec", "przylatuj", "lec", "wbijaj", "wbij", "dawaj", "dawej", "zapraszam",
			"wpadaj", "wskakuj", "przybywaj", "teleportuj", "tepaj", "chetnie", "yes", "yep", "jop", "przyjdzcie",
			"dawajcie", "wbijajcie", "chodzcie", "zgoda", "jasna", "pewno", "chodzmy", "lecimy", "idziemy",
			"jak najbardziej", "pewnie ze", "no jasne", "czemu nie", "nie ma problemu", "nie ma sprawy", "moze byc",
			"mozesz przyjsc", "przyjdz pod" };
		static const char* const weakYesWords[] = { "ok", "dobra", "dobrze", "spoko", "git", "luz", "mozesz", "super",
			"ekstra", "swietnie", "czekam", "pasuje", "y", "ta", "no", "jo", "fajnie", "okej", "spox", "gitara",
			"pewnie", "wbijaj", "+" };
		static const char* const strongNoPhrases[] = { "nie trzeba", "nie potrzeba", "juz mam", "mam juz", "mamy juz",
			"juz mamy", "jednak nie", "nie przychodz", "nie chce", "nie musisz", "innym razem", "nie idz", "nie idziemy",
			"nie ide", "juz nie", "nie teraz", "nie dzis", "nie dzisiaj", "nie potrzebuje", "nie potrzebujemy",
			"dzieki ale", "raczej nie", "chyba nie", "no nie", "juz po", "nie bede", "jasne ze nie", "pewnie ze nie" };
		static const char* const strongNoWords[] = { "niepotrzebne", "niepotrzebny", "zbedne", "komplet", "full",
			"pelne", "pelna", "zajete", "odpada", "rezygnuje", "zrezygnowalem", "odwolane", "odwoluje", "nieaktualne",
			"nope", "nah", "zostan", "sorki", "sorry", "sory", "wybacz", "nara", "spadaj", "spierdalaj", "wypad" };
		// Two-word phrases first, on the joined line, then the words left.
		std::string line;
		for (size_t i = 0; i < w.size(); ++i)
		{
			if (i)
				line += ' ';
			line += w[i];
		}
		line = " " + line + " ";
		// "nie wiem" is neither - and takes its "nie" with it.
		{
			size_t pos;
			while ((pos = line.find(" nie wiem ")) != std::string::npos)
				line.replace(pos, 10, " ~ ");
		}
		for (size_t i = 0; i < sizeof(strongNoPhrases) / sizeof(strongNoPhrases[0]); ++i)
			if (line.find(std::string(" ") + strongNoPhrases[i] + " ") != std::string::npos)
				strongNo = true;
		for (size_t i = 0; i < sizeof(strongYesWords) / sizeof(strongYesWords[0]); ++i)
		{
			const std::string probe = std::string(" ") + strongYesWords[i] + " ";
			size_t pos = line.find(probe);
			if (pos == std::string::npos)
				continue;
			// "nie chodz", "nie przychodz": the yes word under a no.
			if (pos >= 4 && line.compare(pos - 4, 4, " nie") == 0)
				continue;
			strongYes = true;
			// The phrases that hold a "nie" mean yes; take it out.
			if (strchr(strongYesWords[i], ' '))
				line.replace(pos, probe.size(), " ~ ");
		}
		std::vector<std::string> rest;
		playerbot_conv::SplitWords(line, rest);
		for (size_t i = 0; i < rest.size(); ++i)
		{
			const std::string& x = rest[i];
			if (LFG_ONE_OF(x, strongNoWords))
				strongNo = true;
			else if (x == "nie" || x == "niee" || x == "nienie")
				plainNo = true;
			else if (LFG_ONE_OF(x, weakYesWords))
				weakYes = true;
		}
		// "ta" and "no" are a yes only alone, or first: "no to chodz", "ta, wbijaj".
		if (weakYes && !strongYes && rest.size() > 2)
		{
			bool other = false;
			for (size_t i = 0; i < rest.size(); ++i)
				if (LFG_ONE_OF(rest[i], weakYesWords) && rest[i] != "ta" && rest[i] != "no" && rest[i] != "y")
					other = true;
			if (!other && rest[0] != "ta" && rest[0] != "no")
				weakYes = false;
		}
		if (strongNo)
			return -1;
		if (strongYes)
			return 1;
		if (plainNo)
			return -1;
		if (weakYes)
			return 1;
		return 0;
	}

	// ------------------------------------------------------------- the lines

	// Markup to text: "`e" is e-ogonek in CP1250 when the bot writes Polish
	// letters, plain "e" when it does not.
	inline std::string Render(const std::string& markup, bool polish)
	{
		std::string out;
		out.reserve(markup.size());
		for (size_t i = 0; i < markup.size(); ++i)
		{
			const char c = markup[i];
			if (c != '`' || i + 1 >= markup.size())
			{
				out += c;
				continue;
			}
			const char b = markup[++i];
			if (!polish)
			{
				// z-acute is a plain z to somebody without the letters, as z-dot is.
				out += b == 'x' ? 'z' : b == 'X' ? 'Z' : b;
				continue;
			}
			unsigned char p = 0;
			switch (b)
			{
				case 'a': p = 0xB9; break;
				case 'A': p = 0xA5; break;
				case 'c': p = 0xE6; break;
				case 'C': p = 0xC6; break;
				case 'e': p = 0xEA; break;
				case 'E': p = 0xCA; break;
				case 'l': p = 0xB3; break;
				case 'L': p = 0xA3; break;
				case 'n': p = 0xF1; break;
				case 'N': p = 0xD1; break;
				case 'o': p = 0xF3; break;
				case 'O': p = 0xD3; break;
				case 's': p = 0x9C; break;
				case 'S': p = 0x8C; break;
				case 'x': p = 0x9F; break;
				case 'X': p = 0x8F; break;
				case 'z': p = 0xBF; break;
				case 'Z': p = 0xAF; break;
				default: break;
			}
			out += p ? (char)p : b;
		}
		return out;
	}

	inline void ReplaceAll(std::string& s, const char* what, const std::string& with)
	{
		const size_t n = strlen(what);
		size_t pos = 0;
		while ((pos = s.find(what, pos)) != std::string::npos)
		{
			s.replace(pos, n, with);
			pos += with.size();
		}
	}

	// What the line is about: the bot (its level, class and path, its name
	// for the hand) and the person and the dungeon.
	struct TFacts
	{
		int level;
		int job;            // 0 warrior, 1 ninja, 2 sura, 3 shaman
		int group;          // the skill group: 1, 2, or 0 before the path
		std::string botName;
		std::string playerName;
		std::string key;    // the dungeon (Words)
		TFacts() : level(0), job(0), group(0) {}
	};

	// Whether this bot's hand writes Polish letters: a third of them, the
	// same ones every time.
	inline bool WritesPolish(const std::string& botName)
	{
		return playerbot_conv::HashStr(botName.c_str(), 0x1f6a3u) % 100u < 35u;
	}

	template <size_t N>
	inline const char* Any(TRng& rng, const char* const (&pool)[N])
	{
		return pool[rng.Range((u32)N)];
	}

	// The class and its path as players write them: "sura wp", "woj body",
	// "ninja luk", "szaman heal".
	inline std::string ClassWords(TRng& rng, int job, int group)
	{
		static const char* const warriorBody[] = { "wojownik body", "woj body", "wojo body", "body woj", "wojownik cia`lo",
			"wojownik na body", "warrior body" };
		static const char* const warriorMental[] = { "wojownik mental", "woj mental", "wojo mental", "mental woj",
			"wojownik umys`l", "wojownik na mentalu" };
		static const char* const ninjaDagger[] = { "ninja sztylet", "ninja sztylety", "ninja dagger", "ninja skryto",
			"ninja na sztyletach", "skrytob`ojca" };
		static const char* const ninjaBow[] = { "ninja `luk", "ninja `lucznik", "`lucznik", "ninja z `lukiem",
			"ninja archer", "`luk ninja" };
		static const char* const suraWeapon[] = { "sura wp", "sura bro`n", "surka wp", "wp sura", "sura magiczna bro`n",
			"sura na wp" };
		static const char* const suraMagic[] = { "sura bm", "sura czarna magia", "surka bm", "bm sura", "sura cm",
			"sura na bm" };
		static const char* const shamanDragon[] = { "szaman smok", "szaman smoka", "szamanka smok", "szaman od smoka",
			"smok szaman", "szaman ze smokiem" };
		static const char* const shamanHeal[] = { "szaman leczenie", "szaman heal", "szamanka heal", "szaman od leczenia",
			"heal szaman", "szaman lecz" };
		switch (job)
		{
			case 0: return group == 2 ? Any(rng, warriorMental) : group == 1 ? Any(rng, warriorBody) : "wojownik";
			case 1: return group == 2 ? Any(rng, ninjaBow) : group == 1 ? Any(rng, ninjaDagger) : "ninja";
			case 2: return group == 2 ? Any(rng, suraMagic) : group == 1 ? Any(rng, suraWeapon) : "sura";
			case 3: return group == 2 ? Any(rng, shamanHeal) : group == 1 ? Any(rng, shamanDragon) : "szaman";
			default: return "wojownik";
		}
	}

	inline std::string NaWords(TRng& rng, const std::string& key)
	{
		const TDungeonWords* d = Words(key);
		if (!d)
			return "dunga";
		return d->na[rng.Range(3)];
	}

	inline std::string GenWords(TRng& rng, const std::string& key)
	{
		const TDungeonWords* d = Words(key);
		if (!d)
			return "dunga";
		return d->gen[rng.Range(2)];
	}

	// The placeholders: $LVL, $CLS, $NA, $GEN, $NICK.
	inline std::string Finish(TRng& rng, const TFacts& f, std::string markup)
	{
		if (markup.find("$CLS") != std::string::npos)
			ReplaceAll(markup, "$CLS", ClassWords(rng, f.job, f.group));
		if (markup.find("$NA") != std::string::npos)
			ReplaceAll(markup, "$NA", NaWords(rng, f.key));
		if (markup.find("$GEN") != std::string::npos)
			ReplaceAll(markup, "$GEN", GenWords(rng, f.key));
		char lvl[16];
		snprintf(lvl, sizeof(lvl), "%d", f.level);
		ReplaceAll(markup, "$LVL", lvl);
		ReplaceAll(markup, "$NICK", f.playerName);
		std::string out = Render(markup, WritesPolish(f.botName));
		// Now and then all in lowercase, as a chat line often is - the names
		// included, which a person typing fast does too.
		if (rng.Chance(30))
			for (size_t i = 0; i < out.size(); ++i)
				if (out[i] >= 'A' && out[i] <= 'Z')
					out[i] = (char)(out[i] - 'A' + 'a');
		return out;
	}

	inline std::string Greeting(TRng& rng)
	{
		static const char* const k[] = { "Cze`s`c", "Siema", "Hej", "Elo", "Siemka", "Witam", "Yo", "Hejka", "Siemano",
			"Cze`s`c $NICK", "Siema $NICK", "Hej $NICK", "Elo $NICK", "Witaj", "Siemanko" };
		return rng.Chance(25) ? std::string() : std::string(Any(rng, k));
	}

	// Who the bot is: the level and the class's path.
	inline std::string SelfWords(TRng& rng)
	{
		static const char* const k[] = { "mam $LVL lvl, $CLS", "$LVL lvl, $CLS", "jestem $CLS, $LVL lvl", "$CLS $LVL lvl",
			"$CLS na $LVL", "mam $LVL, gram $CLS", "$LVL lvl $CLS", "$CLS, $LVL poziom", "mam $LVL poziom i jestem $CLS",
			"$CLS $LVL", "lvl $LVL, $CLS", "jestem $CLS na $LVL lvl" };
		return Any(rng, k);
	}

	// The offer: "Czesc, moge isc z toba, mam 47 lvl, sura wp, moge przyjsc?"
	inline std::string OfferLine(TRng& rng, const TFacts& f)
	{
		static const char* const intro[] = { "mog`e i`s`c z tob`a", "mog`e i`s`c na $NA", "ch`etnie p`ojd`e",
			"ja si`e pisz`e", "pisz`e si`e na $NA", "mog`e si`e do`l`aczy`c", "we`x mnie", "wbij`e z tob`a",
			"te`z chcia`lem i`s`c na $NA", "szukasz jeszcze kogo`s? ja mog`e", "pomog`e ci na $NA",
			"akurat nic nie robi`e, mog`e i`s`c", "mam czas, mog`e i`s`c", "jestem ch`etny", "id`e z tob`a jak chcesz",
			"mog`e wbi`c na $NA", "ch`etnie wpadn`e na $NA" };
		static const char* const ask[] = { "mog`e przyj`s`c?", "przyj`s`c?", "wbi`c?", "to jak, przyj`s`c pod wej`scie?",
			"chcesz?", "pasuje?", "dawa`c?", "co ty na to?", "lecie`c pod wej`scie?", "we`xmiesz mnie?",
			"przylecie`c?", "mam przyj`s`c?", "przyj`s`c pod wej`scie do $GEN?", "lec`e?", "mog`e wpa`s`c?" };
		const std::string g = Greeting(rng);
		const std::string i = Any(rng, intro);
		const std::string s = SelfWords(rng);
		const std::string a = Any(rng, ask);
		std::string out;
		switch (rng.Range(6))
		{
			case 0: out = (g.empty() ? "" : g + ", ") + i + ", " + s + ", " + a; break;
			case 1: out = (g.empty() ? "" : g + ", ") + s + ", " + i + ". " + a; break;
			case 2: out = i + ", " + s + ". " + a; break;
			case 3: out = (g.empty() ? "" : g + ", ") + s + " - " + i + ", " + a; break;
			case 4: out = (g.empty() ? "" : g + "! ") + i + ". " + s + ", " + a; break;
			default: out = (g.empty() ? "" : g + ", ") + i + " - " + s + ". " + a; break;
		}
		return Finish(rng, f, out);
	}

	// A call that named no dungeon: "na jaki dung?"
	inline std::string AskWhichLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "$G, na jaki dung idziesz? $S", "a gdzie dok`ladnie? mog`e i`s`c, $S",
			"$G, mog`e i`s`c, tylko na jaki dung?", "na co idziesz? $S, mog`e si`e do`l`aczy`c",
			"jaki dung? jak co to $S", "$S, ja ch`etnie, tylko powiedz gdzie", "$G, a jaki dung? $S",
			"gdzie idziesz? mog`e z tob`a, $S", "na jaki loch? $S, mam czas" };
		std::string out = Any(rng, k);
		std::string g = Greeting(rng);
		if (g.empty())
			g = "Hej";
		ReplaceAll(out, "$G", g);
		ReplaceAll(out, "$S", SelfWords(rng));
		return Finish(rng, f, out);
	}

	// The dungeon named after "na jaki?" - and it suits the bot.
	inline std::string ChosenLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "$NA? spoko, mog`e i`s`c. przyj`s`c?", "ok, $NA mi pasuje. przyj`s`c pod wej`scie?",
			"dobra, na $NA mog`e. lecie`c?", "$NA? jasne, wbi`c?", "git, $NA to dobry pomys`l. mam przyj`s`c?",
			"spoko, na $NA p`ojd`e. dawa`c?" };
		return Finish(rng, f, Any(rng, k));
	}

	inline std::string ChosenWrongLine(TRng& rng, const TFacts& f, int why)
	{
		static const char* const low[] = { "na $NA mam za ma`ly lvl, sorki", "$NA? za wysoko dla mnie, mam dopiero $LVL",
			"ehh na $NA jeszcze za s`laby jestem, sorki", "tam mnie zabij`a xd mam $LVL, sorki" };
		static const char* const high[] = { "na $NA to ja ju`z za wysoki lvl xd sorki", "$NA to dla mnie za nisko, mam $LVL",
			"tam nic ju`z dla mnie nie ma, mam $LVL, sorki" };
		static const char* const far[] = { "tam nie dam rady dotrze`c, sorki", "tam nie wbij`e, sorki",
			"tam nie mog`e i`s`c, sorki" };
		return Finish(rng, f, why == CHOOSE_LOW ? Any(rng, low) : why == CHOOSE_HIGH ? Any(rng, high) : Any(rng, far));
	}

	inline std::string WhichAgainLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "ale na jaki dung?", "spoko, tylko gdzie? na jaki dung?", "ok, ale gdzie idziemy?",
			"dobra, a jaki loch?", "no ok, ale na co? :D" };
		return Finish(rng, f, Any(rng, k));
	}

	// The yes: at the entrance now ("Czekam pod wejsciem, bede tutaj 5
	// minut"), or walking there on the same map.
	inline std::string WaitLine(TRng& rng, const TFacts& f, bool walking)
	{
		static const char* const lead[] = { "ok, ", "dobra, ", "jasne, ", "spoko, ", "git, ", "super, ", "", "", "" };
		static const char* const there[] = { "Czekam pod wej`sciem, b`ed`e tutaj 5 minut", "jestem ju`z pod wej`sciem do $GEN, czekam 5 min",
			"stoj`e przy wej`sciu, daj pt jak b`edziesz", "jestem na miejscu, poczekam jakie`s 5 minut",
			"ju`z jestem pod wej`sciem, zapro`s mnie do grupy", "czekam przy wej`sciu do $GEN, mam 5 min",
			"jestem pod wej`sciem do $GEN, daj zaproszenie do pt, czekam 5 minut",
			"dotar`lem, czekam pod wej`sciem jakie`s 5 minut", "stoj`e pod wej`sciem do $GEN. 5 minut czekam, potem lec`e",
			"na miejscu :) zapro`s do pt, b`ed`e tu 5 min", "jestem przy wej`sciu, daj pt. czekam 5 minut",
			"ju`z stoj`e pod wej`sciem do $GEN, czekam na ciebie z 5 minut" };
		static const char* const coming[] = { "ju`z id`e pod wej`scie, poczekam tam 5 minut",
			"lec`e pod wej`scie do $GEN, b`ed`e czeka`l 5 min", "id`e pod wej`scie, zapro`s do pt jak b`edziesz",
			"zaraz b`ed`e pod wej`sciem, poczekam 5 minut", "biegn`e pod wej`scie do $GEN, b`ed`e tam czeka`l z 5 min" };
		std::string out = Any(rng, lead);
		const std::string body = walking ? Any(rng, coming) : Any(rng, there);
		// The lead is lowercase; the body after it too.
		std::string b = body;
		if (!out.empty() && !b.empty() && b[0] >= 'A' && b[0] <= 'Z')
			b[0] = (char)(b[0] - 'A' + 'a');
		out += b;
		return Finish(rng, f, out);
	}

	inline std::string AlreadyLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "przecie`z ju`z czekam pod wej`sciem :)", "ju`z tu stoj`e, zapro`s do pt",
			"jestem pod wej`sciem do $GEN, czekam na ciebie", "czekam czekam, daj pt", "no jestem ju`z, zapro`s mnie" };
		return Finish(rng, f, Any(rng, k));
	}

	inline std::string CantComeLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "kurde, nie dam rady teraz przyj`s`c, sorki", "sorki, co`s mi wypad`lo, nie przyjd`e",
			"ehh nie mog`e si`e teraz ruszy`c, sorry", "jednak nie dam rady, sorki :(", "nie wyjdzie, sorki, mo`ze innym razem" };
		return Finish(rng, f, Any(rng, k));
	}

	inline std::string GoneLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "sorki, ju`z jestem zaj`ety, nie dam rady", "eh, ju`z gdzie`s indziej lec`e, sorki",
			"za p`o`xno, ju`z robi`e co`s innego, sorki", "sorry, ju`z nieaktualne, zaj`e`lem si`e czym`s innym" };
		return Finish(rng, f, Any(rng, k));
	}

	// The no to the offer.
	inline std::string DeclineLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "ok, spoko", "jasne, powodzenia", "dobra, to nara", "ok, mi`lego dunga", "luz, gl",
			"rozumiem, mo`ze nast`epnym razem", "spoko, powodzenia :)", "ok, gl", "dobra, to innym razem", "ok to nara",
			"spoko, udanego dropu", "jasne, baw si`e dobrze", "okej, powodzenia na $NA" };
		return Finish(rng, f, Any(rng, k));
	}

	// The no while the bot waits: it leaves.
	inline std::string LeaveLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "ok, to lec`e dalej", "dobra, wracam expi`c", "spoko, to spadam", "ok, nara :)",
			"jasne, to wracam do swoich spraw", "szkoda, no to lec`e", "dobra, jakby co pisz" };
		return Finish(rng, f, Any(rng, k));
	}

	// Five minutes and no party.
	inline std::string TimeoutLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "nie doczeka`lem si`e, lec`e dalej", "czeka`lem 5 minut, nikt nie przyszed`l, wracam expi`c",
			"dobra, nie ma ci`e, to spadam", "ehh nie doczeka`lem si`e, nara", "5 minut min`e`lo, lec`e dalej, jakby co pisz",
			"nie przyszed`le`s, wi`ec wracam na exp", "no nic, nie doczeka`lem si`e. powodzenia",
			"musz`e lecie`c, czeka`lem do`s`c d`lugo", "nikogo nie ma, wracam do swoich spraw",
			"czeka`lem pod wej`sciem do $GEN, ale nic. lec`e" };
		return Finish(rng, f, Any(rng, k));
	}

	// In the person's party: the follow takes it from here.
	inline std::string JoinedLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "jestem :)", "dzi`eki za pt, prowad`x", "ok jestem w pt, idziemy",
			"dzi`eki, to lecimy", "git, idziemy", "jestem, prowad`x", "ok, wchodzimy?" };
		return Finish(rng, f, Any(rng, k));
	}

	// Something with a better claim took the bot away from the entrance.
	inline std::string CalledAwayLine(TRng& rng, const TFacts& f)
	{
		static const char* const k[] = { "sorki, musia`lem odej`s`c, co`s mi wypad`lo", "sorry, musz`e lecie`c, pilna sprawa",
			"wybacz, musia`lem si`e zwin`a`c, mo`ze innym razem" };
		return Finish(rng, f, Any(rng, k));
	}
}

#undef LFG_ONE_OF
#undef LFG_STARTS_ONE_OF

#endif
