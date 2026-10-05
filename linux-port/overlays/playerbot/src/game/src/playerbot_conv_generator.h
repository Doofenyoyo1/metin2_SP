#ifndef __INC_METIN2_PLAYERBOT_CONV_GENERATOR_H__
#define __INC_METIN2_PLAYERBOT_CONV_GENERATOR_H__

// PlayerBot Conversation v6 - RESPONSE GENERATOR (pure).
//
// Intent + Context + AI State + Persona + Mood + Relationship + Memory
//   -> candidate set -> one natural line.
//
// Rules the generators keep:
//   - every fact comes from TBotSnapshot (no guild -> never "nasza gildia",
//     alone -> never "moi koledzy z PT", 20% HP -> never "swietna forma"),
//   - answers are spoken, not reported: "Wlasnie bije orki w Dolinie", not
//     "Moim aktualnym dzialaniem jest walka",
//   - the voice (persona) colours a line, it does not limit the topic,
//   - the mood shows sometimes, not in every line.
//
// One function per intent; GenerateOne() dispatches. The merger and the queue
// are playerbot_conv_engine.h.

#include "playerbot_conv_general.h"
#include "playerbot_conv_banks.h" // MT2009_PLUS_BOT_CHAT_V2

namespace playerbot_conv
{
	// ------------------------------------------------------------- helpers

	inline bool Fighting(const TGen& g) { return g.s.action == A_FIGHT || g.s.action == A_LURE; }

	inline bool HasFamily(const TGen& g)
	{
		return *MobFamilyPlural(FoldName(g.s.targetName.c_str())) != 0;
	}

	inline const char* GoalPhrase(const TGen& g)
	{
		switch (g.s.goal)
		{
			case G_SURVIVE: return "odzyskac sily";
			case G_PROFESSION: return "wybrac profesje";
			case G_EQUIPMENT: return "zdobyc lepszy sprzet";
			case G_RESTOCK: return "uzupelnic zapasy";
			case G_REFINE: return "ulepszyc bron";
			case G_SKILL: return "podbic umiejetnosci";
			case G_METIN: return "polowac na Metiny";
			case G_PARTY_CHALLENGE: return "znalezc cos mocniejszego dla ekipy";
			case G_BIOLOGIST: return "zrobic zadania dla Biologa";
			case G_HUNTING: return "dokonczyc polowanie";
			case G_HORSE: return "zajac sie koniem";
			case G_FISHING: return "troche polowic";
			default: return "wbijac kolejne poziomy";
		}
	}

	inline std::string BagClause(TGen& g)
	{
		if (g.s.bagCells > 0 && g.s.freeCells <= 2 && !g.s.inTown)
		{
			static const char* const k[] = {
				"Zaraz musze wracac do miasta, EQ mam pelne.",
				"Tylko EQ mi sie konczy, zaraz trzeba bedzie sprzedac drop." };
			return PBC_SAY(g, k);
		}
		return std::string();
	}

	inline std::string VoiceFlavour(TGen& g)
	{
		if (g.Merged() || g.Bad() || !g.rng.Chance(25))
			return std::string();
		switch (g.voice)
		{
			case V_GRINDER:
			{
				static const char* const k[] = { "Byle do nastepnego poziomu.", "Exp sam sie nie zrobi." };
				return PBC_SAY(g, k);
			}
			case V_MERCHANT:
			{
				static const char* const k[] = { "Moze cos cennego wypadnie na sprzedaz.", "Drop pojdzie potem na targ." };
				return PBC_SAY(g, k);
			}
			case V_FIGHTER:
				if (!g.s.targetStone)
				{
					static const char* const k[] = { "Szkoda, ze nie ma tu Metinow.", "Moglyby byc mocniejsze." };
					return PBC_SAY(g, k);
				}
				return std::string();
			case V_SOCIAL:
				if (!g.s.inParty)
				{
					static const char* const k[] = { "Samemu troche nudno.", "Przydalby sie ktos do towarzystwa." };
					return PBC_SAY(g, k);
				}
				return std::string();
			default:
			{
				static const char* const k[] = { "Ladna okolica swoja droga.", "Spokojnie tu." };
				return PBC_SAY(g, k);
			}
		}
	}

	// --------------------------------------------------------------- state

	inline std::string ActivityClause(TGen& g, bool withMap)
	{
		const TBotSnapshot& s = g.s;
		const bool map = withMap && IsKnownMap(s.mapIndex);
		if (s.dead)
		{
			static const char* const k[] = { "Wlasnie zginalem, czekam az wstane.", "Leze na ziemi, ktos mnie ubil." };
			return PBC_SAY(g, k);
		}
		if (s.afk)
		{
			static const char* const k[] = { "Mam chwile przerwy.", "Robie sobie krotka przerwe." };
			return PBC_SAY(g, k);
		}
		switch (s.action)
		{
			case A_FIGHT:
			{
				if (g.LowHp())
				{
					static const char* const k[] = {
						"Walcze, ale mam juz malo HP. Zaraz bede musial odpoczac.",
						"Bije sie, ale ledwo stoje. Zaraz sie wycofam." };
					return PBC_SAY(g, k);
				}
				if (s.targetStone)
				{
					static const char* const kMap[] = { "Rozbijam Metina $MAPIN.", "Bije Metina $MAPINSHORT, zaraz padnie." };
					static const char* const kNo[] = { "Rozbijam Metina.", "Bije Metina, zaraz padnie." };
					return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
				}
				if (s.targetBoss)
				{
					static const char* const k[] = { "Bije bossa, trzymaj kciuki!", "Walcze z bossem, zaraz pogadamy." };
					return PBC_SAY(g, k);
				}
				if (s.targetPlayer)
				{
					static const char* const k[] = { "Bije sie z kims, chwila.", "Mam pojedynek, zaraz." };
					return PBC_SAY(g, k);
				}
				if (HasFamily(g))
				{
					static const char* const kMap[] = { "Bije $FAMILY $MAPIN.", "Expie $MAPIN, leca $FAMILY.", "Wlasnie ubijam $FAMILY $MAPINSHORT." };
					static const char* const kNo[] = { "Bije $FAMILY.", "Wlasnie ubijam $FAMILY.", "Leca $FAMILY, expie." };
					return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
				}
				static const char* const kMap[] = { "Wlasnie expie $MAPIN.", "Bije moby $MAPIN.", "Expie sobie $MAPINSHORT." };
				static const char* const kNo[] = { "Wlasnie bije moby.", "Expie.", "Bije, co popadnie." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			case A_TRAVEL:
				if (IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
				{
					if (s.riding)
					{
						static const char* const k[] = { "Jade na koniu $DEST.", "Jestem w drodze $DEST, na koniu." };
						return PBC_SAY(g, k);
					}
					static const char* const k[] = { "Jestem w drodze $DEST.", "Ide $DEST.", "Zmierzam $DEST." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const kMap[] = { "Ide na inny spot $MAPIN.", "Przemieszczam sie $MAPINSHORT." };
					static const char* const kNo[] = { "Ide na inny spot.", "Jestem w drodze." };
					return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
				}
			case A_LOOT:
			{
				static const char* const k[] = { "Zbieram drop po walce.", "Podnosze, co wypadlo." };
				return PBC_SAY(g, k);
			}
			case A_RECOVER:
			{
				static const char* const k[] = { "Odpoczywam chwile, zbieram HP.", "Siadlem na chwile, regeneruje sie." };
				return PBC_SAY(g, k);
			}
			case A_TOWN_REST:
			{
				static const char* const k[] = { "Siedze w miescie i odpoczywam.", "Odpoczywam $MAPIN." };
				return PBC_SAY(g, k);
			}
			case A_TRAIN:
			{
				static const char* const k[] = { "Zalatwiam sprawy u trenera.", "Ogarniam profesje." };
				return PBC_SAY(g, k);
			}
			case A_SHOP:
			{
				static const char* const k[] = { "Robie zakupy u handlarza.", "Kupuje potki i takie tam." };
				return PBC_SAY(g, k);
			}
			case A_REFINE:
			{
				static const char* const k[] = { "Ulepszam sprzet u kowala. Trzymaj kciuki.", "Stoje u kowala, ulepszam bron." };
				return PBC_SAY(g, k);
			}
			case A_READ_BOOK:
			{
				static const char* const k[] = { "Czytam ksiegi umiejetnosci.", "Ucze sie z ksiag." };
				return PBC_SAY(g, k);
			}
			case A_SOCKET:
			{
				static const char* const k[] = { "Wkladam kamienie duszy do broni.", "Bawie sie kamieniami duszy." };
				return PBC_SAY(g, k);
			}
			case A_PARTY_ASSEMBLE:
			{
				static const char* const k[] = { "Zbieram ekipe na cos wiekszego.", "Czekam, az sie ekipa zbierze." };
				return PBC_SAY(g, k);
			}
			case A_BIOLOGIST:
				// An item's name stands after a colon: "zbieram Ksiega Klatw"
				// is a case no Pole would use.
				if (!s.bioWanted.empty())
				{
					static const char* const k[] = { "Zbieram dla Biologa: $BIO.", "Robie zadanie Biologa. Brakuje mi jeszcze: $BIO." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const k[] = { "Robie zadanie dla Biologa.", "Zalatwiam sprawy u Biologa." };
					return PBC_SAY(g, k);
				}
			case A_STABLE:
			{
				static const char* const k[] = { "Jestem u Stajennego, ogarniam konia.", "Zajmuje sie koniem." };
				return PBC_SAY(g, k);
			}
			case A_STALL:
			{
				static const char* const kMap[] = { "Stoje ze straganem $MAPIN.", "Handluje, mam stragan $MAPIN." };
				static const char* const kNo[] = { "Stoje ze straganem.", "Handluje przy straganie." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			case A_FISHING:
			{
				static const char* const kMap[] = { "Lowie ryby $MAPIN.", "Siedze z wedka $MAPINSHORT." };
				static const char* const kNo[] = { "Lowie ryby.", "Siedze z wedka." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			case A_MARKET:
			{
				static const char* const k[] = { "Chodze po targu i szukam okazji.", "Robie zakupy na targu." };
				return PBC_SAY(g, k);
			}
			case A_LURE:
				if (s.luringForAsker)
				{
					static const char* const k[] = { "Przyciagam dla ciebie moby, stoj w miejscu.", "Zbieram ci paczke mobow." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const k[] = { "Podciagam moby dla ekipy.", "Luruje moby dla grupy." };
					return PBC_SAY(g, k);
				}
			case A_MINING:
			{
				static const char* const kMap[] = { "Kopie rude $MAPIN.", "Siedze w kopalni, kopie." };
				static const char* const kNo[] = { "Kopie rude.", "Kopie, ile sie da." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			default:
				if (s.inTown)
				{
					static const char* const k[] = { "Krece sie po miescie.", "Nic specjalnego, stoje $MAPIN." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const k[] = { "Rozgladam sie, co tu porobic.", "Nic konkretnego, zastanawiam sie, co dalej." };
					return PBC_SAY(g, k);
				}
		}
	}

	// The map the bot last named to this person, when it has changed map
	// since: "Przemieszczam sie w Joan", a teleport, and "co robisz?" again.
	// 0 when it has not (or said so already).
	inline long RecentOtherMap(const TGen& g)
	{
		const TConvMemory& m = g.m;
		if (m.lastSaidMap != 0 && m.lastSaidMap != g.s.mapIndex && IsKnownMap(m.lastSaidMap) &&
				IsKnownMap(g.s.mapIndex) && g.now - m.lastSaidMapAt < CONV_SAID_MAP_TTL_MS)
			return m.lastSaidMap;
		return 0;
	}

	// Whether the bot named `map` to this person lately, the newest or the
	// one before it: "przeciez mowiles, ze w Joan" after the move was told.
	inline bool SaidMapLately(const TGen& g, long map)
	{
		const TConvMemory& m = g.m;
		if (map == 0)
			return false;
		return (map == m.lastSaidMap && g.now - m.lastSaidMapAt < CONV_SAID_MAP_TTL_MS) ||
				(map == m.prevSaidMap && g.now - m.prevSaidMapAt < CONV_SAID_MAP_TTL_MS);
	}

	// "$WASAT" is where the bot was: "w Joan".
	inline std::string WithOldMap(const TGen& g, const char* tpl, long old)
	{
		std::string out = Fill(g, tpl);
		ReplaceAll(out, "$WASAT", GetMapWords(old).at);
		return out;
	}

	inline std::string GenActivity(TGen& g)
	{
		const bool yesNoExp = g.a && g.a->concepts.Has(C_EXP) && !g.a->concepts.Has(C_WHAT);
		std::string out;
		if (yesNoExp)
		{
			if (Fighting(g))
				out = g.rng.Chance(50) ? "Tak. " : "No, expie. ";
			else
				out = "Teraz nie. ";
		}
		// It named another map a moment ago: the move is part of the answer.
		if (const long old = RecentOtherMap(g))
			out += WithOldMap(g, "Juz nie jestem $WASAT. ", old);
		out += ActivityClause(g, !g.saidMap);
		if (!g.saidMap && IsKnownMap(g.s.mapIndex) && out.find(GetMapWords(g.s.mapIndex).atShort) != std::string::npos)
			g.saidMap = true;
		g.saidActivity = true;
		Append(out, BagClause(g));
		Append(out, VoiceFlavour(g));
		// A reason ready for "dlaczego?".
		if (Fighting(g))
		{
			static const char* const kWhy[V_COUNT] = {
				"Bo tu jest dobry exp na moj poziom.", "Bo akurat tu trafilem i jest spokojnie.",
				"Bo z tych mobow leci cos, co sie dobrze sprzedaje.", "Bo lubie walke, a tu jest z kim.",
				"Bo tu zawsze ktos jest." };
			g.reason = kWhy[g.voice];
		}
		else if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
			g.reason = "Bo mialem juz malo HP.";
		else if (g.s.action == A_TRAVEL)
			g.reason = "Bo tam mam lepszy spot na moj poziom.";
		else if (g.s.action == A_SHOP || g.s.action == A_MARKET)
			g.reason = "Bo potrzebuje zapasow.";
		// A question back, now and then.
		if (!g.Merged() && !g.Bad() && g.askBack.empty() && g.tier >= TIER_KNOWN && g.rng.Chance(g.voice == V_SOCIAL ? 45 : 20))
		{
			static const char* const k[] = { "A ty co robisz?", "A ty co porabiasz?" };
			g.askBack = PBC_SAY(g, k);
			g.askBackKind = ASK_ACTIVITY;
		}
		return out;
	}

	// "jestes w v1?", "expisz na sohan?", "idziesz do m1?" - the place a player
	// named, resolved to this bot's kingdom. 0 when none was named.
	inline long MentionedMap(const TGen& g)
	{
		if (!g.a || g.a->mentionMap == 0)
			return 0;
		const long m = ResolveMapAlias(g.a->mentionMap, g.s.empire);
		return m == 0 ? MAP_ALIAS_UNLISTED : m;
	}

	inline std::string MapYesNo(TGen& g, long mentioned)
	{
		const TBotSnapshot& s = g.s;
		g.saidMap = true;
		if (mentioned == s.mapIndex && IsKnownMap(s.mapIndex))
		{
			static const char* const k[] = { "Tak, jestem $MAPIN.", "No, $MAPIN." };
			std::string out = PBC_SAY(g, k);
			CapitalizeFirst(out);
			return out;
		}
		if (!IsKnownMap(s.mapIndex))
			return "Nie, jestem gdzie indziej.";
		// "jestes w joan?" right after it said it was, and a teleport since.
		if (mentioned == RecentOtherMap(g) && mentioned != 0)
			return WithOldMap(g, "Juz nie - bylem $WASAT, a teraz jestem $MAPIN.", mentioned);
		static const char* const k[] = { "Nie, jestem $MAPIN.", "Nie, teraz $MAPIN." };
		return PBC_SAY(g, k);
	}

	inline std::string GenLocation(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (const long mentioned = MentionedMap(g))
			return MapYesNo(g, mentioned);
		if (!IsKnownMap(s.mapIndex))
		{
			static const char* const k[] = { "Gdzies na uboczu, nawet nie wiem, jak to miejsce sie nazywa.", "W jakims dziwnym miejscu, nie znam nazwy." };
			return PBC_SAY(g, k);
		}
		g.saidMap = true;
		if (const long old = RecentOtherMap(g))
		{
			static const char* const k[] = { "Bylem $WASAT, ale juz jestem $MAPIN.", "Juz nie $WASAT - teraz jestem $MAPIN." };
			return WithOldMap(g, Pick(g, k, 2), old);
		}
		if (s.askerNear)
		{
			static const char* const k[] = { "Tuz obok ciebie :)", "Przeciez stoje niedaleko ciebie, $MAPIN." };
			return PBC_SAY(g, k);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
		{
			static const char* const k[] = { "Jestem $MAPIN, ale ide $DEST.", "Teraz $MAPIN, zmierzam $DEST." };
			return PBC_SAY(g, k);
		}
		if (g.saidActivity)
		{
			static const char* const k[] = { "Jestem $MAPIN.", "A jestem $MAPIN." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Jestem teraz $MAPIN.", "Siedze $MAPIN.", "Aktualnie $MAPIN.", "Jestem $MAPINSHORT." };
		return PBC_SAY(g, k);
	}

	inline std::string GenActivityLocation(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (const long mentioned = MentionedMap(g))
			return MapYesNo(g, mentioned);
		if (!IsKnownMap(s.mapIndex))
			return GenLocation(g);
		if (const long old = RecentOtherMap(g))
		{
			g.saidMap = true;
			return WithOldMap(g, Fighting(g) ? "Juz nie $WASAT, teraz expie $MAPIN." :
					"Juz nie $WASAT, teraz jestem $MAPIN.", old);
		}
		g.saidMap = true;
		// The last whisper already named the map: say so, do not recite it.
		if (!g.Merged() && g.m.lastReply.find(GetMapWords(s.mapIndex).atShort) != std::string::npos &&
				g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
		{
			static const char* const k[] = { "No mowie, $MAPIN :)", "Tutaj, $MAPIN.", "$MAPIN, tak jak pisalem." };
			return PBC_SAY(g, k);
		}
		if (Fighting(g))
		{
			if (g.saidActivity)
			{
				static const char* const k[] = { "$MAPIN.", "Tutaj, $MAPIN." };
				return PBC_SAY(g, k);
			}
			if (HasFamily(g))
			{
				static const char* const k[] = { "$MAPIN, bije $FAMILY.", "Expie $MAPIN.", "$MAPIN, tu jest niezly spot na $FAMILY." };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Expie $MAPIN.", "$MAPIN, tu jest dobry spot.", "Teraz $MAPIN." };
			return PBC_SAY(g, k);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap))
		{
			static const char* const k[] = { "Teraz nigdzie, dopiero ide $DEST.", "Ide wlasnie $DEST, tam bede expil." };
			return PBC_SAY(g, k);
		}
		if (s.inTown)
		{
			static const char* const k[] = { "Teraz nigdzie, jestem $MAPIN. Potem pewnie wroce na exp.", "Na razie nie expie, stoje $MAPIN." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Teraz nie expie, ale jestem $MAPIN.", "Na razie przerwa, jestem $MAPIN." };
		return PBC_SAY(g, k);
	}

	inline std::string GenMobCount(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.inTown && !Fighting(g))
		{
			static const char* const k[] = { "W miescie? Tu nie ma mobow :)", "Tu sa sami gracze i handlarze, zadnych mobow." };
			return PBC_SAY(g, k);
		}
		const int n = s.mobsNear;
		const bool askedFew = g.a && g.a->polarityNegative;
		std::string out;
		if (n < 0)
		{
			static const char* const k[] = { "Troche jest.", "Da sie expic." };
			out = PBC_SAY(g, k);
		}
		else if (n == 0)
		{
			static const char* const k[] = { "Pusto teraz, wszystko wybite.", "Nic nie ma, czekam na respawn." };
			out = PBC_SAY(g, k);
		}
		else if (n <= 3)
		{
			static const char* const k[] = { "Malo. Ktos tu chyba przede mna czyscil.", "Kilka sztuk, nic wielkiego." };
			out = PBC_SAY(g, k);
		}
		else if (n <= 9)
		{
			static const char* const k[] = { "Troche jest, ale spot jest spokojny.", "Troche ich jest, w sam raz.", "Jest co bic, ale bez szalu." };
			out = PBC_SAY(g, k);
		}
		else if (n <= 19)
		{
			static const char* const kFew[] = { "Wcale nie malo, sporo ich.", "Nie, jest ich calkiem sporo." };
			static const char* const k[] = { "Sporo, jest co bic.", "Sporo ich, exp leci." };
			out = askedFew ? PBC_SAY(g, kFew) : PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Duzo! Ledwo nadazam.", "Mnostwo, az sie roi." };
			out = PBC_SAY(g, k);
		}
		if (s.playersNear >= 3 && g.rng.Chance(60))
			Append(out, "Tylko tloczno troche, sporo ludzi.");
		g.reason = n > 9 ? "Bo szybko sie respia." : "Bo ktos tu pewnie wczesniej byl.";
		return out;
	}

	inline std::string GenTarget(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (!Fighting(g) || s.targetName.empty())
		{
			if (Fighting(g))
			{
				static const char* const k[] = { "Nic konkretnego, szukam czegos do bicia.", "Akurat nic, rozgladam sie." };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Teraz nic nie bije.", "Nic, mam przerwe od walki." };
			return PBC_SAY(g, k);
		}
		if (s.targetStone)
		{
			static const char* const k[] = { "Metina! Zaraz go rozwale.", "Kamien Metina, zaraz padnie." };
			return PBC_SAY(g, k);
		}
		if (HasFamily(g))
		{
			static const char* const k[] = { "Bije $FAMILY. Teraz akurat $TARGET.", "$FAMILY, glownie.", "Teraz $TARGET." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Teraz na celowniku: $TARGET.", "Aktualnie $TARGET.", "Bije sie z tym tu, $TARGET." };
		return PBC_SAY(g, k);
	}

	inline std::string GenLevel(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		// Asked again a moment after it said it: the same number, not the same
		// sentence. "Mam 64 poziom" three times over reads as a recording.
		const bool again = g.m.levelSaidAt != 0 && g.now - g.m.levelSaidAt < CONV_FACT_TTL_MS &&
				g.m.levelSaid == s.level;
		g.m.levelSaidAt = g.now != 0 ? g.now : 1;
		g.m.levelSaid = s.level;
		g.reason = "Bo tyle wyexpilem.";
		if (again)
		{
			static const char* const k[] = { "Dalej $LVL :)", "$LVL, nic sie nie zmienilo.", "Wciaz $LVL." };
			return PBC_SAY(g, k);
		}
		if (s.expPct >= 85)
		{
			static const char* const k[] = { "Mam $LVL, zaraz wbijam $NEXTLVL.", "$LVL, ale juz prawie $NEXTLVL." };
			out = PBC_SAY(g, k);
		}
		else if (s.expPct >= 60 && g.voice == V_GRINDER)
			out = Fill(g, "Mam $LVL. Jeszcze troche i powinienem wbic kolejny poziom.");
		else
		{
			static const char* const k[] = { "Mam $LVL poziom.", "$LVL lvl.", "Mam $LVL. Powoli do przodu.", "$LVL, na razie." };
			out = PBC_SAY(g, k);
		}
		if (s.askerLevel > 0 && g.rng.Chance(35))
		{
			if (s.askerLevel >= s.level + 10)
				Append(out, "Ty mnie juz troche przegoniles.");
			else if (s.level >= s.askerLevel + 10)
				Append(out, "Troche wyzej od ciebie.");
			else if (s.askerLevel >= s.level - 3 && s.askerLevel <= s.level + 3)
				Append(out, "Mamy podobnie.");
		}
		return out;
	}

	// The class and its path in the instrumental a sentence needs, with the
	// players' word and the game's in brackets where they differ.
	inline const char* BuildPhrase(int build)
	{
		switch (build)
		{
			case B_BODY: return "wojownikiem body";
			case B_MENTAL: return "wojownikiem mental";
			case B_DAGGER: return "ninja na sztyletach (dagger)";
			case B_ARCHER: return "ninja lucznikiem (archer)";
			case B_WEAPON: return "sura WP (magiczna bron)";
			case B_BLACK_MAGIC: return "sura BM (czarna magia)";
			case B_DRAGON: return "szamanem smok";
			case B_HEAL: return "szamanem heal (leczenie)";
			default: return "";
		}
	}

	// The path's skills, best first - the build's own first skill ahead of an
	// equal one - as "Aura Miecza M3, Wir Miecza 17". Empty before any is
	// learnt.
	inline std::string SkillsList(const TBotSnapshot& s, size_t maxItems)
	{
		int order[6];
		int n = 0;
		for (int i = 0; i < 6; ++i)
		{
			if (s.skillVnums[i] == 0 || s.skillLevels[i] <= 0 || !*SkillNameOf(s.skillVnums[i]))
				continue;
			int at = n++;
			while (at > 0)
			{
				const int prev = order[at - 1];
				const bool before = s.skillLevels[i] > s.skillLevels[prev] ||
						(s.skillLevels[i] == s.skillLevels[prev] && s.skillVnums[i] == s.mainSkill);
				if (!before)
					break;
				order[at] = prev;
				--at;
			}
			order[at] = i;
		}
		std::string out;
		for (int k = 0; k < n && (size_t)k < maxItems; ++k)
		{
			if (!out.empty())
				out += ", ";
			out += SkillNameOf(s.skillVnums[order[k]]);
			out += ' ';
			out += SkillGradeText(s.skillLevels[order[k]]);
		}
		return out;
	}

	inline std::string GenClass(TGen& g)
	{
		// MT2009_PLUS_BOT_CHAT_V2: "jaka klasa najlepsza?", "sura czy ninja?" -
		// an opinion, from the bot's own class.
		if (g.a && (g.a->concepts.Has(C_RECOMMEND) || g.a->concepts.Has(C_OR) || g.a->concepts.Has(C_ADVICE) ||
				g.a->tokens.Has("najlepsza") || g.a->tokens.Has("najmocniejsza") || g.a->tokens.Has("lepsza")))
		{
			static const char* const k[] = { "Kazda ma swoje. Ja gram $KLASA i nie narzekam.",
				"Na start woj najprostszy, a potem jak lubisz. Ja gram $KLASA.",
				"Sura w pvp robi robote, szaman zawsze znajdzie pt. Ja wybralem gre $KLASA.",
				"Zalezy co lubisz - bic z bliska, z luku czy buffowac. Ja gram $KLASA od poczatku." };
			std::string out = PBC_SAY(g, k);
			ReplaceAll(out, "$KLASA", ClassNameInstr(g.s.job));
			return out;
		}
		static const char* const k[] = { "Gram $CLASSI.", "Jestem $CLASSI.", "$CLASSI, od poczatku." };
		std::string out = Pick(g, k, 3);
		const int build = g.s.Build();
		ReplaceAll(out, "$CLASSI", build != B_NONE ? BuildPhrase(build) : ClassNameInstr(g.s.job));
		CapitalizeFirst(out);
		return out;
	}

	// "jaka masz profesje?", "jestes body czy mental?", "grasz archerem?" - the
	// path, a yes or a no when the line named one, the level and the best
	// skill of the path.
	inline std::string GenBuild(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		const int build = s.Build();
		if (build == B_NONE)
		{
			g.reason = "Bo sciezke wybiera sie u trenera od piatego poziomu.";
			if (s.level < 5)
				return Fill(g, "Jeszcze nie mam sciezki, mam dopiero $LVL poziom. Wybiera sie ja od piatego.");
			return "Jeszcze nie wybralem sciezki, musze isc do trenera.";
		}
		const unsigned int named = g.a ? NamedBuildsInLine(g.a->tokens, g.a->concepts) : 0;
		const unsigned int mine = 1u << build;
		std::string out;
		if (named != 0 && (named & mine) == 0)
			out = std::string("Nie, gram ") + BuildPhrase(build) + ".";
		else if (named == mine)
		{
			static const char* const k[] = { "Tak, gram $P.", "Zgadza sie, jestem $P.", "Tak, jestem $P." };
			out = Pick(g, k, 3);
			ReplaceAll(out, "$P", BuildPhrase(build));
		}
		else
		{
			static const char* const k[] = { "Gram $P.", "Jestem $P.", "Gram $P, tak wybralem u trenera." };
			out = Pick(g, k, 3);
			ReplaceAll(out, "$P", BuildPhrase(build));
		}
		CapitalizeFirst(out);
		std::string tail = Fill(g, "Mam $LVL poziom");
		const std::string best = SkillsList(s, 1);
		if (!best.empty())
			tail += ", najwyzej " + best;
		Append(out, tail + ".");
		g.reason = "Tak wybralem u trenera i tak juz zostalo.";
		return out;
	}

	inline std::string GenEmpire(TGen& g)
	{
		if (!*EmpireName(g.s.empire))
			return "Sam juz nie wiem, heh.";
		static const char* const k[] = { "Jestem z $EMPIRE.", "$EMPIRE.", "Z $EMPIRE, od zawsze." };
		return PBC_SAY(g, k);
	}

	inline std::string GenName(TGen& g)
	{
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Przeciez mnie znasz, $NAME :)", "No $NAME, a kto?" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Jestem $NAME.", "$NAME. Milo mi.", "$NAME, a ty to $PLAYER, prawda?" };
		return PBC_SAY(g, k);
	}

	inline std::string GenGoal(TGen& g)
	{
		static const char* const k[] = { "Ogolnie chce $G.", "Moj plan to $G.", "Na dluzsza mete chce $G.", "Glownie chce $G." };
		std::string out = Pick(g, k, 4);
		ReplaceAll(out, "$G", GoalPhrase(g));
		if (g.s.goal == G_HUNTING && !g.s.huntMob.empty())
			Append(out, Fill(g, "Zostalo mi $HUNTN sztuk: $HUNT."));
		static const char* const kWhy[V_COUNT] = {
			"Bo chce byc mocniejszy.", "Bo chce zobaczyc, co jest dalej.", "Bo to sie oplaci.",
			"Bo lubie wyzwania.", "Bo wtedy moge wiecej pomoc ekipie." };
		g.reason = kWhy[g.voice];
		return out;
	}

	inline std::string GenNextPlan(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.dead)
			return "Najpierw musze wstac :)";
		if (g.LowHp())
		{
			g.reason = "Bo mam malo HP.";
			return "Najpierw odpoczne, bo mam malo HP.";
		}
		if (s.bagCells > 0 && s.freeCells <= 2 && !s.inTown)
		{
			g.reason = "Bo mam pelne EQ.";
			return "Zaraz wracam do miasta, EQ mam pelne. Potem pewnie znowu na exp.";
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
		{
			static const char* const k[] = { "Najpierw dojde $DEST, potem sie zobaczy.", "Jak dojde $DEST, to tam zostane na troche." };
			return PBC_SAY(g, k);
		}
		if (s.askerInParty && Fighting(g))
		{
			static const char* const k[] = { "Jak skonczymy tutaj, pewnie wracam do miasta.", "Jeszcze troche tu pobijemy, a potem sie zobaczy." };
			return PBC_SAY(g, k);
		}
		if (s.shopStanding)
			return "Postoje jeszcze troche ze straganem, a potem pewnie na exp.";
		if (s.fishing)
			return "Jeszcze troche polowie, potem zobaczymy.";
		if (Fighting(g) && s.goal == G_LEVEL)
		{
			static const char* const k[] = { "Dalej expic, moze zmienie spot, jak wbije poziom.", "Jeszcze troche tu, potem pewnie do miasta sprzedac drop." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Potem pewnie $G.", "Pozniej chce $G.", "Dalej? Chyba $G." };
		std::string out = Pick(g, k, 3);
		ReplaceAll(out, "$G", GoalPhrase(g));
		return out;
	}

	inline std::string GenHp(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.dead)
			return "Nie zyje wlasnie, czekam az wstane.";
		if (s.hpPct < 15)
			out = "Bardzo malo, ledwo stoje.";
		else if (s.hpPct < 30)
			out = "Malo, zaraz musze sie podleczyc.";
		else if (s.hpPct < 60)
			out = Fill(g, "Moze byc, okolo $HP%.");
		else if (s.hpPct < 95)
		{
			static const char* const k[] = { "Dobrze, okolo $HP%.", "W porzadku, $HP%." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Pelne, jestem w formie.", "Full HP." };
			out = PBC_SAY(g, k);
		}
		if (s.spPct < 20)
			Append(out, "Gorzej z SP, prawie pusto.");
		return out;
	}

	inline std::string GenGold(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.tier <= TIER_STRANGER && s.gold >= 1000000)
		{
			static const char* const k[] = { "Wystarczy mi na potrzeby :)", "Troche jest, nie narzekam." };
			return PBC_SAY(g, k);
		}
		std::string out;
		if (s.gold < 10000)
			out = "Prawie nic, bieda :(";
		else if (s.gold < 1000000)
			out = Fill(g, "Troche drobnych, $GOLD.");
		else if (s.gold < 50000000)
		{
			static const char* const k[] = { "Mam $GOLD, nie narzekam.", "Okolo $GOLD." };
			out = PBC_SAY(g, k);
		}
		else
			out = Fill(g, "Sporo, $GOLD. Ale nie mow nikomu :)");
		if (g.voice == V_MERCHANT && g.rng.Chance(40))
			Append(out, "I caly czas obracam tym na targu.");
		g.reason = g.voice == V_MERCHANT ? "Bo handluje." : "Bo sprzedaje drop.";
		return out;
	}

	inline std::string GenHorse(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.horseLevel <= 0)
			return "Nie mam jeszcze konia.";
		if (s.riding)
		{
			static const char* const k[] = { "Jade teraz na nim, ma $HORSELVL poziom.", "Siedze na nim wlasnie. $HORSELVL poziom." };
			return PBC_SAY(g, k);
		}
		if (s.goal == G_HORSE)
			return Fill(g, "Mam, $HORSELVL poziom. Wlasnie go rozwijam.");
		static const char* const k[] = { "Mam, $HORSELVL poziom.", "Jest, poziom $HORSELVL." };
		return PBC_SAY(g, k);
	}

	// The gear line. The names stand in the nominative, after "w rece:" or
	// "bron to" - "Mam Pajecza Wlocznia" is a case nobody speaks - and a
	// question asked again a moment later gets "dalej to samo", not the
	// line recited a second and a third time.
	inline std::string GenEquipment(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.weaponName.empty())
			return "Na razie bez porzadnej broni.";
		// MT2009_PLUS_BOT_CHAT_V2: "skad masz taki eq?" - where it came from.
		if (g.a && g.a->concepts.Has(C_ORIGIN))
		{
			if (s.weaponPlus >= 7)
			{
				static const char* const k[] = { "$WEAPON? Z kowala, po kilku probach i paru lzach xd",
					"Kupilem bazowke na targu i pchalem u kowala. Kosztowalo :P" };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Polowa z dropu, reszta z targu.", "Z mobow i ze straganow, nic specjalnego.",
				"Sam wyexpilem, troche dokupilem na targu." };
			return PBC_SAY(g, k);
		}
		const bool again = g.m.gearSaidAt != 0 && g.now - g.m.gearSaidAt < CONV_FACT_TTL_MS;
		g.m.gearSaidAt = g.now != 0 ? g.now : 1;
		if (again)
		{
			static const char* const k[] = { "Dalej to samo: $WEAPON.", "Nic sie nie zmienilo, dalej $WEAPON.", "Wciaz $WEAPON w rece." };
			return PBC_SAY(g, k);
		}
		if (g.a && g.a->concepts.Has(C_BONUS))
		{
			std::string out = "Bonusow nie licze co do punktu.";
			Append(out, Fill(g, s.armorName.empty() ? "W rece: $WEAPON." : "W rece: $WEAPON, na sobie: $ARMOR."));
			return out;
		}
		std::string out;
		if (!s.armorName.empty())
		{
			static const char* const k[] = { "W rece $WEAPON, na sobie $ARMOR.", "Bron to $WEAPON, a zbroja $ARMOR.",
				"W rece: $WEAPON. Na sobie: $ARMOR." };
			out = PBC_SAY(g, k);
		}
		else
			out = Fill(g, "W rece: $WEAPON.");
		if (s.weaponPlus >= 7)
			Append(out, "Nie narzekam.");
		else if (s.weaponPlus <= 2 && g.rng.Chance(50))
			Append(out, "Trzeba by to ulepszyc.");
		return out;
	}

	inline std::string GenInventory(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out = s.bagSummary.empty() ? std::string("Nic ciekawego w EQ.") : "W EQ m.in.: " + s.bagSummary + ".";
		if (s.bagCells > 0)
			Append(out, Fill(g, "Wolnych miejsc $FREE."));
		return out;
	}

	inline std::string GenInventorySpace(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.bagCells <= 0)
			return "Nie wiem, jeszcze nie ogarnalem plecaka.";
		if (s.freeCells == 0)
		{
			g.reason = "Bo za duzo dropu nazbieralem.";
			return "Zero, EQ pelne. Musze do miasta.";
		}
		if (s.freeCells <= 5)
			return Fill(g, "Malo, $FREE wolnych miejsc.");
		static const char* const k[] = { "Mam $FREE wolnych miejsc.", "Jeszcze $FREE miejsc wolnych, spokojnie." };
		return PBC_SAY(g, k);
	}

	inline std::string ShopWhere(const TGen& g);
	inline std::string SayMoney(long long v);

	inline std::string GenItemOwn(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return "Co konkretnie?";
		if (obj == "czas" || obj == "chwile" || obj == "moment" || obj == "chwilke")
			return g.Bad() ? "Troche mam, o co chodzi?" : "Mam chwile, o co chodzi?";
		if (obj == "racje" || obj == "racja")
			return "No wiem :)";
		if (obj == "ochote" || obj == "ochota")
			return "Na co?";
		if (obj == "pomysl" || obj == "pomysly")
			return "Moze pobijemy cos razem?";
		std::string name;
		unsigned int count = 0;
		if (g.world && g.world->FindItem(obj, name, count))
		{
			std::string out = "Tak, mam: " + name;
			if (count > 1)
				out += " x" + ToString(count);
			out += ".";
			return out;
		}
		long long price = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return "W EQ nie, ale na straganie " + ShopWhere(g) + " stoi " + name + " za " + SayMoney(price) + ".";
		static const char* const k[] = { "Nie, nie mam tego.", "Nie mam czegos takiego w EQ.", "Niestety nie." };
		return PBC_SAY(g, k);
	}

	inline std::string GenParty(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		g.saidParty = true;
		if (!s.inParty)
		{
			if (g.voice == V_SOCIAL && g.tier >= TIER_STRANGER && g.askBack.empty())
			{
				g.askBack = "Zagramy razem?";
				g.askBackKind = ASK_JOIN;
			}
			static const char* const k[] = { "Teraz jestem sam.", "Sam, nikt mnie nie zaprosil :)", "Solo na razie." };
			g.reason = g.voice == V_GRINDER ? "Bo sam szybciej expie." : "Bo nikt mnie nie zaprosil :)";
			return PBC_SAY(g, k);
		}
		if (s.askerInParty && !(g.a && g.a->concepts.Has(C_WHO)))
			return "No z toba przeciez :)";
		if (g.a && (g.a->follow == F_WHO || g.a->concepts.Has(C_WHO)))
			return Fill(g, "Liderem jest $LEADER.");
		if (s.leaderIsMe)
		{
			static const char* const k[] = { "Prowadze PT, jest nas $PARTYN.", "Mam swoje PT, $PARTYN osob." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Jestem w PT z $LEADER, razem $PARTYN osob.", "W grupie, $PARTYN osob. Lider to $LEADER." };
		return PBC_SAY(g, k);
	}

	inline std::string GenGuild(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (!s.inGuild)
		{
			static const char* const kSoc[] = { "Nie mam gildii. Szukam jakiejs fajnej.", "Jeszcze nie, ale chetnie bym do jakiejs wstapil." };
			static const char* const k[] = { "Nie mam gildii.", "Bez gildii na razie.", "Nie, jestem bez gildii." };
			g.reason = "Jakos nie trafilem na odpowiednia.";
			return g.voice == V_SOCIAL ? PBC_SAY(g, kSoc) : PBC_SAY(g, k);
		}
		std::string out;
		if (g.a && (g.a->follow == F_COUNT || g.a->concepts.Has(C_HOWMUCH) || g.a->concepts.Has(C_MANY)))
			out = Fill(g, "W $GUILD jest nas $GUILDN.");
		else
		{
			static const char* const k[] = { "Jestem w $GUILD, jest nas $GUILDN.", "$GUILD. Fajna ekipa.", "Tak, $GUILD." };
			out = PBC_SAY(g, k);
		}
		if (s.guildWar)
			Append(out, "Akurat mamy wojne!");
		return out;
	}

	inline std::string GenFishing(TGen& g)
	{
		if (g.s.fishing)
		{
			static const char* const k[] = { "Tak, lowie teraz ryby.", "No, siedze z wedka." };
			return PBC_SAY(g, k);
		}
		if (g.s.style == S_FISHER)
			return "Teraz nie, ale lubie polowic.";
		return "Nie, teraz nie lowie.";
	}

	inline std::string GenMining(TGen& g)
	{
		if (g.s.mining)
		{
			static const char* const k[] = { "Tak, kopie rude.", "No, kopie. Ciezka robota." };
			return PBC_SAY(g, k);
		}
		if (g.s.style == S_MINER)
			return "Teraz nie, ale kopanie to moja dzialka.";
		return "Nie, teraz nie kopie.";
	}

	inline std::string GenHerbalism(TGen& g)
	{
		return g.s.herbUnlocked ? "Tak, znam sie troche na ziolach." : "Nie, zielarstwa jeszcze nie ogarniam.";
	}

	inline std::string GenBiologist(TGen& g)
	{
		if (!g.s.bioWanted.empty())
			return Fill(g, g.s.action == A_BIOLOGIST ? "Wlasnie zbieram dla Biologa: $BIO." : "Dla Biologa szukam teraz: $BIO.");
		if (g.s.action == A_BIOLOGIST)
			return "Wlasnie robie jego zadanie.";
		return "Teraz nic dla Biologa nie zbieram.";
	}

	inline std::string GenMetin(TGen& g)
	{
		if (Fighting(g) && g.s.targetStone)
			return "Wlasnie jednego bije!";
		if (g.s.metinHunter || g.s.goal == G_METIN)
			return "Poluje na nie, jak tylko sie jakis pojawi.";
		if (g.voice == V_FIGHTER)
			return "Uwielbiam je bic, ale teraz akurat zadnego nie widze.";
		return "Teraz nie, ale jak jakis sie trafi, to bije.";
	}

	inline std::string GenDemonTower(TGen& g)
	{
		// MT2009_PLUS_BOT_CHAT_V2: "warto isc do wiezy na 45?" - advice.
		if (g.a && (g.a->concepts.Has(C_RECOMMEND) || g.a->levelAsked > 0))
		{
			const int level = g.a->levelAsked > 0 ? g.a->levelAsked : (g.s.askerLevel > 0 ? g.s.askerLevel : g.s.level);
			if (level < 50)
				return "Na " + ToString((long long)level) + " to za wczesnie, wieza zjada slabych. Lepiej od 55+ i z ekipa.";
			if (level < 65)
				return "Na " + ToString((long long)level) + " juz mozna, ale tylko z ekipa i z potkami.";
			return "Jasne, na " + ToString((long long)level) + " spokojnie dasz rade, gora wiezy daje dobry drop.";
		}
		if (g.s.demonTower)
			return g.s.mapIndex == 66 ? "Tak, jestem w Wiezy Demonow." : "Tak, mam sprawy w Wiezy Demonow.";
		return g.s.level >= 40 ? "Teraz nie, moze kiedys z ekipa." : "Jeszcze za slaby jestem na Wieze.";
	}

	inline std::string GenGuildWar(TGen& g)
	{
		if (g.s.guildWar)
			return "Tak, mamy wojne gildii!";
		if (g.s.inGuild)
			return "Teraz spokoj, zadnej wojny.";
		return "Nie mam gildii, wiec i wojen nie ma.";
	}

	inline std::string GenMercenary(TGen& g)
	{
		if (g.s.mercContract)
			return "Tak, mam teraz kontrakt.";
		if (g.s.style == S_MERC)
			return "Teraz nie mam kontraktu. Ale jakby co, jestem do wynajecia.";
		return "Nie, nie mam teraz zadnego kontraktu.";
	}

	inline std::string GenPartyRequest(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.tier == TIER_HOSTILE)
			return "Nie, dzieki.";
		if (s.askerInParty)
			return "Przeciez juz jestesmy razem :)";
		if (g.a && g.a->follow == F_HOW)
			return "Po prostu zapros mnie do grupy.";
		if (s.inParty)
			return "Jestem teraz w innym PT, moze pozniej.";
		if (s.shopStanding)
			return "Teraz stoje ze straganem, moze pozniej.";
		if (s.dead || g.LowHp())
			return "Chwila, najpierw sie podlecze.";
		// An invitation reaches nobody on the other channel.
		if (AskerOnOtherChannel(s))
			return Fill(g, "Chetnie, ale jestem $MAPIN. Przejdz na moj kanal, to mnie zaprosisz.");
		std::string out;
		if (s.askerLevel > 0 && (s.askerLevel > s.level + 15 || s.level > s.askerLevel + 15))
			out = "Chetnie, ale mamy duza roznice poziomow. Zapros, zobaczymy.";
		else if (g.tier == TIER_STRANGER)
		{
			static const char* const k[] = { "Nie znamy sie jeszcze, ale czemu nie. Zapros mnie do PT.", "Mozemy sprobowac. Zapros mnie, zobaczymy." };
			out = PBC_SAY(g, k);
		}
		else if (g.tier == TIER_KNOWN)
		{
			static const char* const k[] = { "Jasne, mozemy sprobowac. Zapros mnie.", "Jasne, zapros mnie do PT." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Z toba zawsze mozna isc. Dawaj zaproszenie.", "Jasne! Zapros mnie, juz ide." };
			out = PBC_SAY(g, k);
		}
		return out;
	}

	// ------------------------------------------------------------- trade

	inline std::string SayMoney(long long v)
	{
		return FormatYang(v);
	}

	// Where the stall stands, as exactly as a person needs to walk up to it:
	// "w M1 Joan przy kowalu", the channel when it is not the asker's. The
	// offline one stays there while the bot hunts elsewhere.
	inline std::string ShopWhere(const TGen& g)
	{
		const char* village = VillageTag(g.s.shopMapIndex);
		std::string out = *village ? "w " + std::string(village) : Fill(g, "$SHOPAT");
		if (!g.s.shopSpot.empty())
			out += " " + g.s.shopSpot;
		if (g.s.shopOtherChannel)
			out += g.s.shopChannel > 0 ? " na CH" + ToString((long long)g.s.shopChannel) : std::string(" (inny kanal)");
		return out;
	}

	// MT2009_PLUS_BOT_CHAT_V2 (deals): "ku", "ksiega", "ksiazke" with no
	// skill after it - a skill book, but which one?
	inline bool IsBareBookObject(const std::string& obj)
	{
		std::vector<std::string> words;
		SplitWords(obj, words);
		if (words.empty())
			return false;
		static const char* const kBook[] = { "ku", "ksiega", "ksiege", "ksiegi", "ksiag", "ksiazka", "ksiazke", "ksiazki",
			"ksiazek", "instr", "instr.", "umiejetnosci", "skill", "skilla", "book", "booka" };
		for (size_t i = 0; i < words.size(); ++i)
		{
			bool book = false;
			for (size_t k = 0; k < sizeof(kBook) / sizeof(kBook[0]) && !book; ++k)
				book = words[i] == kBook[k];
			if (!book)
				return false;
		}
		return true;
	}

	// The skill books of the stall's summary ("Instr. Aura Miecza za 1kk"),
	// for "ktora?".
	inline std::string ShopBooks(const TGen& g)
	{
		std::string out;
		const std::string& sum = g.s.shopSummary;
		for (size_t at = 0; at < sum.size(); )
		{
			size_t end = sum.find(", ", at);
			if (end == std::string::npos)
				end = sum.size();
			const std::string piece = sum.substr(at, end - at);
			if (piece.compare(0, 7, "Instr. ") == 0 || piece.compare(0, 3, "KU ") == 0)
			{
				if (!out.empty())
					out += ", ";
				out += piece;
			}
			at = end + 2;
		}
		return out;
	}

	// "kupie ku" - which skill? The ones the bot has to offer, if any.
	inline std::string AskWhichBook(TGen& g, bool personBuys, const std::string& booksHad)
	{
		const std::string onStall = personBuys ? ShopBooks(g) : std::string();
		if (!onStall.empty())
		{
			static const char* const k[] = { "Ktora? Na straganie mam: $LIST.", "A jaka umiejetnosc? Mam na straganie $LIST." };
			std::string out = PBC_PICK(g, k);
			ReplaceAll(out, "$LIST", onStall);
			return out;
		}
		if (personBuys && !booksHad.empty())
		{
			static const char* const k[] = { "Ktora? Mam przy sobie $LIST.", "Jaka umiejetnosc? Z ksiag mam $LIST." };
			std::string out = PBC_PICK(g, k);
			ReplaceAll(out, "$LIST", booksHad);
			return out;
		}
		if (personBuys)
		{
			static const char* const k[] = { "A jaka ksiega? Napisz umiejetnosc, np. KU Aura Miecza.",
				"Ktora KU? Napisz jaka umiejetnosc, to sprawdze." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "A jaka to ksiega? Napisz umiejetnosc, to powiem czy wezme.",
			"Jaka KU? Napisz jaka umiejetnosc, np. KU Aura Miecza." };
		return PBC_SAY(g, k);
	}

	// One line of the bot's own stall, and what to say about it - with a
	// player's offer weighed against the asking price when one was named.
	inline std::string ShopLineAnswer(TGen& g, const std::string& name, long long price, unsigned int count)
	{
		const long long offer = g.a ? g.a->offerYang : 0;
		std::string out;
		if (offer > 0 && price > 0)
		{
			if (offer > price + price / 20)
			{
				static const char* const k[] = {
					"Stoi nawet taniej, za $PRICE. Kup normalnie na straganie $WHERE.",
					"Nie musisz przeplacac, na straganie $WHERE stoi za $PRICE." };
				out = PBC_PICK(g, k);
			}
			// The price as the bot says it ("1.5kk" for 1 523 000) is the
			// price: an offer of what the stall shows was answered "troche
			// malo" to the bot's own words (Setnil).
			else if (offer >= price || SayMoney(offer) == SayMoney(price))
			{
				static const char* const k[] = {
					"Za $OFFER moze byc. $ITEM stoi na moim straganie $WHERE, kup normalnie.",
					"$OFFER? Pasuje. Masz to na straganie $WHERE za $PRICE." };
				out = PBC_PICK(g, k);
			}
			else if (offer * 100 >= price * 80)
			{
				static const char* const k[] = {
					"Troche malo. Stoi za $PRICE, taniej raczej nie zejde.",
					"Blisko, ale stoi za $PRICE. Taniej nie oddam." };
				out = PBC_PICK(g, k);
			}
			else
			{
				static const char* const k[] = { "Za $OFFER? Nie, stoi za $PRICE.", "Za malo. Chce $PRICE." };
				out = PBC_PICK(g, k);
			}
		}
		else if (count > 1)
		{
			static const char* const k[] = { "Tak, na straganie $WHERE stoi $ITEM x$COUNT, $PRICE za calosc. Kup normalnie ze straganu.",
				"Mam, $ITEM x$COUNT za $PRICE calosc - stragan $WHERE, wejdz i kup." };
			out = PBC_PICK(g, k);
		}
		else
		{
			// MT2009_PLUS_BOT_CHAT_V2: said as a seller says it - yes, where
			// exactly, how much, and how to buy (the owner, 4 October).
			static const char* const k[] = { "Tak, $ITEM stoi na moim straganie $WHERE, $PRICE. Kup normalnie ze straganu.",
				"Mam $ITEM za $PRICE, stragan $WHERE - kupisz prosto ze straganu.",
				"Jest, $ITEM za $PRICE. Stragan stoi $WHERE, wejdz i kup.",
				"Aktualne, $ITEM dalej stoi $WHERE za $PRICE. Kupuj smialo ze straganu." };
			out = PBC_PICK(g, k);
		}
		ReplaceAll(out, "$OFFER", SayMoney(offer));
		ReplaceAll(out, "$PRICE", SayMoney(price));
		ReplaceAll(out, "$COUNT", ToString(count));
		ReplaceAll(out, "$WHERE", ShopWhere(g));
		ReplaceAll(out, "$ITEM", name);
		CapitalizeFirst(out);
		return out;
	}

	// "masz na straganie fms?", "sprzedasz mi 12d za 5kk?"
	inline std::string ShopItemAnswer(TGen& g, const std::string& obj)
	{
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return ShopLineAnswer(g, name, price, count);
		if (g.s.shopOpen)
		{
			std::string out = "Na straganie tego nie mam.";
			if (!g.s.shopSummary.empty())
				Append(out, "Mam za to: " + g.s.shopSummary + ".");
			return out;
		}
		unsigned int bagCount = 0;
		if (g.world && g.world->FindItem(obj, name, bagCount))
			return "Straganu teraz nie mam, ale w EQ lezy " + name + ".";
		return "Nie mam tego, a straganu teraz tez nie.";
	}

	inline std::string GenShop(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.a && !g.a->object.empty())
			return ShopItemAnswer(g, g.a->object);
		if (s.shopOpen && !s.shopSummary.empty())
		{
			std::string out = "Mam stragan " + ShopWhere(g) + ". Na nim m.in.: " + s.shopSummary + ".";
			if (s.shopItems > 3)
				Append(out, "I jeszcze troche innych rzeczy.");
			return out;
		}
		if (s.shopOpen)
			return "Mam stragan, ale juz prawie wszystko zeszlo.";
		if (g.voice == V_MERCHANT)
			return "Teraz nie mam straganu, ale niedlugo cos wystawie.";
		return "Nie mam teraz straganu.";
	}

	inline std::string GenPrice(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
		{
			if (g.s.shopOpen && !g.s.shopSummary.empty())
				return "U mnie: " + g.s.shopSummary + ".";
			return "Ale czego cena?";
		}
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
		{
			if (g.a->offerYang > 0)
				return ShopLineAnswer(g, name, price, count);
			std::string out = "U mnie na straganie " + name + " stoi za " + SayMoney(price) + ".";
			if (count > 1)
				out = "U mnie " + name + " x" + ToString(count) + " za " + SayMoney(price) + " calosc.";
			return out;
		}
		unsigned int sellers = 0;
		if (g.world && g.world->FindMarketPrice(obj, name, price, sellers))
		{
			static const char* const k[] = {
				"Na targu widzialem $ITEM po $PRICE.", "$ITEM chodzi teraz po jakies $PRICE.",
				"Najtaniej widzialem $ITEM za $PRICE." };
			std::string out = PBC_PICK(g, k);
			ReplaceAll(out, "$ITEM", name);
			ReplaceAll(out, "$PRICE", SayMoney(price));
			CapitalizeFirst(out);
			if (g.voice == V_MERCHANT && g.rng.Chance(50))
				Append(out, "Ceny sie jednak zmieniaja.");
			return out;
		}
		static const char* const k[] = {
			"Nie wiem, dawno nie widzialem tego na targu.", "Ciezko powiedziec, nikt tego ostatnio nie wystawial." };
		return PBC_SAY(g, k);
	}

	inline std::string GenMarket(TGen& g)
	{
		if (g.s.marketTrip || g.s.action == A_MARKET)
			return "Wlasnie ide na targ, zobacze co jest.";
		if (g.voice == V_MERCHANT)
		{
			static const char* const k[] = { "Ceny ostatnio skacza, trzeba uwazac.", "Handel to moja dzialka. Kupuje tanio, sprzedaje drozej." };
			return PBC_SAY(g, k);
		}
		return "Handluje troche, jak mam cos na zbyciu.";
	}

	// MT2009_PLUS_BOT_CHAT_V2 (deals): defined with the deal talk below.
	inline std::string OpenSellDeal(TGen& g, const std::string& obj);
	inline std::string OpenBuyDeal(TGen& g, const std::string& obj);
	inline const TPublicLine* PostOf(const TGen& g);

	inline std::string GenBuy(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return g.s.shopOpen && !g.s.shopSummary.empty()
					? "Na straganie mam: " + g.s.shopSummary + ". Co cie interesuje?"
					: std::string("Co konkretnie chcesz kupic?");
		// MT2009_PLUS_BOT_CHAT_V2 (deals): "kupie ku" - which skill?
		if (IsBareBookObject(obj))
		{
			TDealQuote q;
			if (g.world)
				g.world->QuoteItem(obj, 0, 0, q);
			return AskWhichBook(g, true, q.booksHad);
		}
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return ShopLineAnswer(g, name, price, count);
		// MT2009_PLUS_BOT_CHAT_V2 (deals): in the bag, not on the counter -
		// sold by hand, through the exchange window.
		{
			const std::string deal = OpenSellDeal(g, obj);
			if (!deal.empty())
				return deal;
		}
		if (g.world && g.world->FindItem(obj, name, count))
			return "W EQ lezy " + name + ", ale tego nie sprzedaje.";
		// Its own "S>" post, gone from the counter since: sold.
		if (const TPublicLine* post = PostOf(g))
			if (post->kind == PL_SELL && !post->itemName.empty())
			{
				static const char* const k[] = { "Sorki, $ITEM juz zeszlo, ktos byl szybszy.",
					"$ITEM juz sprzedane, nie zdazyles. Sorki." };
				std::string out = PBC_PICK(g, k);
				ReplaceAll(out, "$ITEM", post->itemName);
				CapitalizeFirst(out);
				return out;
			}
		if (g.s.shopOpen)
		{
			std::string out = "Tego nie mam na straganie.";
			if (!g.s.shopSummary.empty())
				Append(out, "Mam za to: " + g.s.shopSummary + ".");
			return out;
		}
		static const char* const k[] = { "Nie mam tego teraz na sprzedaz.", "Nie mam, sorki. Popytaj na @, ktos pewnie ma.",
			"Nie, tego nie mam. Zerknij w wyszukiwarke sklepow." };
		return PBC_SAY(g, k);
	}

	inline std::string GenSell(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return "Co chcesz mi sprzedac?";
		// MT2009_PLUS_BOT_CHAT_V2 (deals): "sprzedam ku" - which skill?
		if (IsBareBookObject(obj))
			return AskWhichBook(g, false, std::string());
		// MT2009_PLUS_BOT_CHAT_V2 (deals): what the bot would pay, and the talk.
		return OpenBuyDeal(g, obj);
	}

	inline std::string GenSkills(TGen& g)
	{
		std::string out;
		if (g.s.action == A_READ_BOOK)
			out = "Wlasnie czytam ksiegi, podbijam skille.";
		else if (g.s.goal == G_SKILL)
			out = "Teraz glownie podbijam umiejetnosci.";
		else
		{
			static const char* const k[] = { "Rozwijam, jak mam ksiegi. Tanio nie jest.", "Powoli do przodu z umiejetnosciami." };
			out = PBC_SAY(g, k);
		}
		// What they are at, from the character sheet.
		const std::string list = SkillsList(g.s, 3);
		if (!list.empty())
			Append(out, "Najwyzej mam " + list + ".");
		return out;
	}

	inline std::string GenPvp(TGen& g)
	{
		if (g.LowHp() || g.s.dead)
			return "Nie teraz, mam malo HP.";
		if (g.voice == V_FIGHTER)
			return "Chetnie! Wyzwij mnie normalnie przez PvP.";
		return "Mozemy, ale wyzwij mnie normalnie przez PvP.";
	}

	inline std::string GenTravel(TGen& g)
	{
		if (const long mentioned = MentionedMap(g))
		{
			if (mentioned == g.s.mapIndex && IsKnownMap(g.s.mapIndex))
			{
				g.saidMap = true;
				return Fill(g, "Juz jestem $MAPIN.");
			}
			if (g.s.action == A_TRAVEL && mentioned == g.s.travelMap)
				return Fill(g, "Tak, ide $DEST.");
			std::string out = "Nie. ";
			out += ActivityClause(g, true);
			g.saidMap = true;
			return out;
		}
		if (g.s.action == A_TRAVEL && IsKnownMap(g.s.travelMap) && g.s.travelMap != g.s.mapIndex)
		{
			g.saidMap = true;
			return ActivityClause(g, true);
		}
		if (IsKnownMap(g.s.mapIndex))
		{
			g.saidMap = true;
			return Fill(g, "Nigdzie, zostaje $MAPIN.");
		}
		return "Nigdzie, zostaje tutaj.";
	}

	inline std::string GenRest(TGen& g)
	{
		if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
			return "Tak, odpoczywam chwile.";
		if (g.LowHp())
			return "Zaraz bede musial, HP mi siada.";
		return "Nie, jeszcze mam sile.";
	}

	inline std::string GenRefine(TGen& g)
	{
		if (g.s.action == A_REFINE)
			return "Wlasnie ulepszam, trzymaj kciuki.";
		if (g.s.euphoria)
			return "Ostatnio weszlo mi niezle ulepszenie!";
		if (g.s.goal == G_REFINE)
			return "Planuje ulepszyc bron, jak tylko zbiore materialy.";
		if (!g.s.weaponName.empty())
			return Fill(g, "Ulepszam, jak mam materialy. Teraz w rece: $WEAPON.");
		return "Jak bedzie z czego, to ulepsze.";
	}

	inline std::string GenMissions(TGen& g)
	{
		if (!g.s.huntMob.empty())
			return Fill(g, "Mam polowanie: $HUNT, zostalo $HUNTN.");
		if (!g.s.bioWanted.empty())
			return Fill(g, "Zbieram dla Biologa: $BIO.");
		return "Teraz zadnej misji nie mam.";
	}

	inline std::string GenDeath(TGen& g)
	{
		if (g.s.dead)
			return "No wlasnie leze, ktos mnie ubil.";
		if (g.s.recentDeaths > 0 && g.s.minutesSinceDeath < 60)
			return g.s.recentDeaths > 1 ? "Kilka razy juz dzis padlem. Bywa." : "Tak, niedawno zginalem. Bywa.";
		return "Nie, dzis jeszcze nie :)";
	}

	inline std::string GenRelationship(TGen& g)
	{
		switch (g.tier)
		{
			case TIER_HOSTILE: return "Szczerze? Nie bardzo po tym, co pisales.";
			case TIER_STRANGER: return "Dopiero sie poznajemy, ale wydajesz sie spoko.";
			case TIER_KNOWN: return "Jasne, spoko jestes.";
			case TIER_FRIEND: return "Pewnie! Dobrze sie z toba gada.";
			default: return "No jasne, jestes jednym z moich ulubionych ludzi tutaj :)";
		}
	}

	inline std::string GenTimeHere(TGen& g)
	{
		const u32 m = g.s.actionMinutes > 0 ? g.s.actionMinutes : g.s.goalMinutes;
		if (m >= 60)
			return Fill(g, "Dluzsza chwile, ponad godzine.");
		if (m >= 10)
			return "Z kilkanascie minut, moze wiecej.";
		if (m > 0)
			return "Dopiero przyszedlem.";
		return "Nie liczylem, chwile.";
	}

	inline std::string GenMapOpinion(TGen& g)
	{
		const long mentioned = MentionedMap(g);
		if (mentioned && mentioned != g.s.mapIndex)
		{
			const TMapWords& w = GetMapWords(mentioned);
			const int roll = OpinionRoll(g, *w.name ? w.name : "gdzies", 23);
			std::string place = *w.name ? w.name : "Tam";
			if (roll < 45)
				return place + "? Lubie, dobre miejsce.";
			if (roll < 80)
				return place + "? Moze byc, zalezy na co.";
			return place + "? Srednio, wole inne miejsca.";
		}
		if (g.s.inTown)
			return "Miasto jak miasto. Lubie tu wrocic po expie.";
		if (!IsKnownMap(g.s.mapIndex))
			return "Dziwne miejsce, ale moze byc.";
		static const char* const k[V_COUNT][2] = {
			{ "$MAPNAME jest ok, exp leci.", "Lubie, jesli exp dobry." },
			{ "Lubie, ladnie tu.", "$MAPNAME ma swoj klimat." },
			{ "Moze byc, drop sie dobrze sprzedaje.", "Jest ok, jak cos wypada." },
			{ "Jest ok, byle bylo z czym walczyc.", "Lubie, tu sa mocne moby." },
			{ "Lubie, czesto kogos tu spotykam.", "Fajnie, zwlaszcza z ekipa." },
		};
		return Say(g, k[g.voice], 2);
	}

	inline std::string GenDropLuck(TGen& g)
	{
		if (g.s.unlucky)
			return "Kiepsko, dawno nic dobrego nie wypadlo.";
		if (g.s.euphoria)
			return "Swietnie! Ostatnio mialem farta.";
		if (g.Good())
			return "Calkiem niezle, nie narzekam.";
		static const char* const k[] = { "Normalnie, nic specjalnego.", "Jak zwykle, raz lepiej, raz gorzej." };
		return PBC_SAY(g, k);
	}

	inline std::string GenProgressToday(TGen& g)
	{
		if (g.s.onlineMinutes >= 120)
			return "Troche sie zrobilo, gram juz dobrych kilka godzin.";
		if (g.s.onlineMinutes >= 30)
			return "Troche expa wpadlo, gram od jakiegos czasu.";
		return "Dopiero zaczalem, jeszcze nic wielkiego.";
	}

	inline std::string GenPersonality(TGen& g)
	{
		static const char* const k[V_COUNT][2] = {
			{ "Lubie konkret. Exp, poziom i dalej.", "Raczej malo gadam, wiecej expie." },
			{ "Lubie pochodzic, pozwiedzac i pogadac.", "Ciekawy swiata, tak bym powiedzial." },
			{ "Bardziej handlarz niz wojownik.", "Lubie dobre interesy." },
			{ "Lubie dobra walke, im mocniej, tym lepiej.", "Raczej nie uciekam od ryzyka." },
			{ "Lubie grac z ludzmi. Samemu jest nudno.", "Towarzyski jestem, chyba to widac." },
		};
		return Say(g, k[g.voice], 2);
	}

	inline std::string GenMood(TGen& g)
	{
		if (g.s.dead)
			return "Kiepsko, wlasnie zginalem.";
		if (g.Good())
			return g.LowHp() ? "Humor dobry, tylko HP mniej :)" : "Swietny! Wszystko idzie jak trzeba.";
		if (g.Bad())
		{
			g.reason = g.s.unlucky ? "Bo dawno nic dobrego nie wypadlo." : "Jakos nic nie idzie.";
			return g.s.unlucky ? "Kiepski. Pech mnie przesladuje." : "Kiepski, szczerze mowiac.";
		}
		return "Normalnie, spokojnie.";
	}

	// --------------------------------------------------------------- social

	inline std::string GenGreeting(TGen& g, bool shortForm)
	{
		const bool again = g.m.greetedAt != 0 && g.now - g.m.greetedAt < CONV_SESSION_GAP_MS;
		g.m.greetedAt = g.now;
		g.saidGreeting = true;
		if (again)
		{
			static const char* const k[] = { "No siema jeszcze raz :)", "Hej hej.", "Juz sie witalismy :)" };
			return PBC_SAY(g, k);
		}
		if (shortForm)
		{
			static const char* const k[] = { "Hej!", "Siema!", "Czesc!" };
			return PBC_SAY(g, k);
		}
		const bool longGap = g.a && g.a->gapBefore > 6u * 3600u * 1000u;
		if (g.tier >= TIER_FRIEND)
		{
			if (longGap)
				return Fill(g, "Siema $PLAYER! Dawno cie nie bylo.");
			static const char* const k[] = { "O, siema $PLAYER!", "Hej $PLAYER! Co tam?", "Siemano $PLAYER." };
			return PBC_SAY(g, k);
		}
		if (g.tier == TIER_KNOWN)
		{
			static const char* const k[] = { "O, siema $PLAYER.", "Hej $PLAYER.", "Czesc, co tam?" };
			return PBC_SAY(g, k);
		}
		if (g.tier == TIER_HOSTILE)
			return "No.";
		static const char* const kNight[] = { "Hej. Pozno juz, a ty dalej grasz?", "Siema. Nocna zmiana?" };
		if (g.s.hour >= 0 && g.s.hour < 5 && g.rng.Chance(40))
			return PBC_SAY(g, kNight);
		static const char* const k[] = { "Hej.", "Siema!", "Czesc.", "Hej, co tam?" };
		return PBC_SAY(g, k);
	}

	inline std::string GenFarewell(TGen& g)
	{
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Nara $PLAYER, do nastepnego!", "Trzymaj sie $PLAYER!" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Na razie!", "Trzymaj sie.", "Do zobaczenia.", "Powodzenia na expie." };
		return PBC_SAY(g, k);
	}

	inline std::string GenHowAreYou(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.dead)
			out = "Kiepsko, wlasnie zginalem :(";
		else if (g.LowHp())
			out = "Moglo byc lepiej, mam malo HP.";
		else if (g.Bad())
		{
			static const char* const k[] = { "Tak sobie. Dzis jakos nie idzie.", "Srednio, szczerze mowiac." };
			out = PBC_SAY(g, k);
		}
		else if (g.Good() || s.euphoria)
		{
			static const char* const k[] = { "Swietnie! Wszystko idzie jak trzeba.", "Super, dzis mi wszystko wychodzi." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Spoko, robie swoje.", "W porzadku.", "Dobrze, nie narzekam." };
			out = PBC_SAY(g, k);
			if (!g.saidActivity && !g.groupHasActivity && g.rng.Chance(60))
			{
				const std::string act = ActivityClause(g, !g.saidMap);
				if (IsKnownMap(s.mapIndex) && act.find(GetMapWords(s.mapIndex).atShort) != std::string::npos)
					g.saidMap = true;
				Append(out, act);
				g.saidActivity = true;
			}
		}
		if (g.askBack.empty() && !g.Bad() && g.tier != TIER_HOSTILE && g.rng.Chance(g.voice == V_SOCIAL ? 70 : 45))
		{
			static const char* const k[] = { "A u ciebie?", "A ty jak?", "A co u ciebie?" };
			g.askBack = PBC_SAY(g, k);
			g.askBackKind = ASK_HOW_ARE_YOU;
		}
		return out;
	}

	inline std::string GenHelp(TGen& g)
	{
		static const char* const k[] = {
			"Pytaj normalnie, jak czlowieka :) Co robie, gdzie jestem, ile jest mobow, jaki mam lvl, EQ, gildie, PT. Moge ci tez powiedziec gdzie expic albo gdzie sa metki na twoj lvl, ile cos chodzi, a jak chcesz, to opowiem kawal xd",
			"Pisz jak do kumpla :) Moge powiedziec co robie i gdzie, gdzie expic albo bic metiny na jakis lvl, ile stoi item, co mam na straganie. Mozemy tez pogadac o czymkolwiek." };
		return PBC_SAY(g, k);
	}

	// MT2009_PLUS_BOT_CHAT_V2: asked again and again, the answer moves on -
	// and each voice says it its own way.
	inline std::string GenIsBot(TGen& g)
	{
		if (g.m.botAsked >= 4)
		{
			static const char* const k[] = { "Tak, jestem botem, beep boop xd", "No jasne, a ty jestes kosmita :P",
				"Pytasz juz ktorys raz, odpowiedz sie nie zmieni xd", "Bot, robot, terminator, co tylko chcesz :D" };
			return PBC_SAY(g, k);
		}
		if (g.m.botAsked >= 2)
		{
			static const char* const k[] = { "Przeciez pisze z toba, bot by tak nie umial :P", "Znowu? Nie, normalnie gram xd",
				"Serio, ile razy jeszcze? :D", "Nie, po prostu malo spie xd" };
			return PBC_SAY(g, k);
		}
		if (Fighting(g) && g.rng.Chance(35))
		{
			static const char* const k[] = { "Bot? To czemu odpisuje ci w trakcie bicia mobow xd",
				"Gdybym byl botem, to bym ci nie odpisywal tylko bil dalej :P" };
			return PBC_SAY(g, k);
		}
		switch (g.voice)
		{
			case V_GRINDER:
			{
				static const char* const k[] = { "Bot? Po prostu duzo expie xd", "Nie, po prostu nie mam zycia :P",
					"Jakbym byl botem, to bym juz mial 99 lvl xd" };
				return PBC_SAY(g, k);
			}
			case V_MERCHANT:
			{
				static const char* const k[] = { "Bot by tak dobrze nie handlowal :P", "Nie, ale chetnie ci cos sprzedam xd" };
				return PBC_SAY(g, k);
			}
			case V_FIGHTER:
			{
				static const char* const k[] = { "Bot? Chodz na pvp, to zobaczysz :P", "Bot to ty jestes xd" };
				return PBC_SAY(g, k);
			}
			case V_SOCIAL:
			{
				static const char* const k[] = { "Hehe nie, czemu tak myslisz?", "Nie, normalny czlowiek :) A co, dziwnie pisze?" };
				return PBC_SAY(g, k);
			}
			default:
			{
				static const char* const k[] = { "A co, tak slabo gram? xD", "Bot to ty jestes :P", "Hehe, gram po prostu duzo.",
					"Nie, czemu?" };
				return PBC_SAY(g, k);
			}
		}
	}

	// MT2009_PLUS_BOT_CHAT_V2: an insult, by the bot's voice and temper, by
	// how many there were, and in the middle of a quarrel over its spot.
	inline std::string GenInsult(TGen& g)
	{
		if (g.m.negative >= 5)
			return std::string(); // stops answering an abusive line
		const int temper = TemperOf(g.s.style);
		if (g.s.spotQuarrel > 0)
		{
			if (temper >= 2)
			{
				static const char* const k[] = { "Ciekawe kto tu komu moby kradnie", "Wyzywaj dalej, a moby i tak moje xd",
					"I co, lepiej ci? Dalej to moj spot", "Krzycz sobie, ja tu zostaje" };
				return PBC_SAY(g, k);
			}
			if (temper == 0)
			{
				static const char* const k[] = { "Dobra, juz sobie ide, nie musisz od razu wyzywac",
					"Spokojnie, juz zmieniam spot" };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Bez wyzwisk. Po prostu nie bij moich mobow", "Wyzywac umiesz, a spota sobie nie znajdziesz?" };
			return PBC_SAY(g, k);
		}
		if (g.m.negative >= 4)
		{
			static const char* const k[] = { "Pa.", "Nie gadam z toba.", "Dobra, koniec." };
			return PBC_SAY(g, k);
		}
		if (g.m.negative >= 3)
		{
			static const char* const k[] = { "Nie mam ochoty tak rozmawiac.", "Dobra, koniec rozmowy.", "Jak sie uspokoisz, to pogadamy." };
			return PBC_SAY(g, k);
		}
		if (g.a && g.a->concepts.Has(C_WHY))
		{
			static const char* const k[] = { "A czemu ty taki niemily?", "Bo tak :P A ty czemu taki zly?",
				"Nie wiem, moze masz gorszy dzien xd" };
			return PBC_SAY(g, k);
		}
		if (g.m.negative >= 2)
		{
			if (temper >= 2)
			{
				static const char* const k[] = { "Sam jestes.", "Chyba sie zapomniales.", "Uwazaj troche." };
				return PBC_SAY(g, k);
			}
			if (temper == 0)
			{
				static const char* const k[] = { "Ej, cos sie stalo?", "Czemu jestes taki zly?" };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Serio? To nie bylo mile.", "Znowu to samo?", "Mhm. Cos jeszcze?" };
			return PBC_SAY(g, k);
		}
		switch (g.voice)
		{
			case V_FIGHTER:
			{
				static const char* const k[] = { "Powiedz to na arenie :P", "Uwazaj, bo sie doigrasz.", "Sam taki jestes." };
				return PBC_SAY(g, k);
			}
			case V_GRINDER:
			{
				static const char* const k[] = { "Nie mam czasu na takie gadki, expie.", "Ok, to ja wracam do expa." };
				return PBC_SAY(g, k);
			}
			case V_MERCHANT:
			{
				static const char* const k[] = { "Obrazanie klientow to slaby biznes :P", "Taki grzeczny, a nic nie kupi xd" };
				return PBC_SAY(g, k);
			}
			case V_SOCIAL:
			{
				static const char* const k[] = { "Ej, czemu taki niemily?", "Spokojnie, co ci zrobilem?" };
				return PBC_SAY(g, k);
			}
			default:
			{
				static const char* const k[] = { "Spokojnie, bez nerwow.", "Nie musisz tak od razu.", "A co ja ci zrobilem?",
					"Ok, jak uwazasz." };
				return PBC_SAY(g, k);
			}
		}
	}

	inline std::string GenPraise(TGen& g)
	{
		if (g.voice == V_FIGHTER && g.rng.Chance(40))
		{
			static const char* const k[] = { "Wiem, ze jestem dobry xd", "No ba :P" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Dzieki! Ty tez spoko.", "Hehe, dzieki.", "Milo slyszec.", "Oo dzieki, od razu lepszy dzien :)",
			"Dzieki, staram sie.", "Hehe, przestan, bo sie zarumienie xd" };
		return PBC_SAY(g, k);
	}

	// MT2009_PLUS_BOT_CHAT_V2: a bot has an age of its own (AgeOf), said to
	// somebody it knows; a stranger may get a joke instead.
	inline std::string GenAge(TGen& g)
	{
		if (g.tier <= TIER_STRANGER && OpinionRoll(g, "age", 7) < 40)
		{
			static const char* const k[] = { "Wystarczajaco, zeby grac do rana :)", "O wieku sie nie rozmawia, hehe.",
				"A co, ile dajesz? :P" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Mam $AGE lat.", "$AGE, a ty?", "$AGE na karku xd", "$AGE, stary juz jestem :P" };
		std::string out = PBC_SAY(g, k);
		ReplaceAll(out, "$AGE", ToString((long long)AgeOf(g.s.name)));
		return out;
	}

	inline std::string GenOrigin(TGen& g)
	{
		if (!*EmpireName(g.s.empire))
			return "Stad i stamtad.";
		static const char* const k[] = { "Z $EMPIRE.", "Jestem z $EMPIRE. A ty?", "$EMPIRE, od urodzenia.",
			"W grze z $EMPIRE, a w realu to inna sprawa :P" };
		return PBC_SAY(g, k);
	}

	inline std::string GenItemShop(TGen& g)
	{
		if (!g.s.dragonKnown)
			return "Nie sprawdzalem ostatnio, ile mam SM.";
		if (g.s.dragonCoins <= 0)
			return g.voice == V_MERCHANT ? "Zero SM. Wole yang, IS to nie moja bajka." : "Zero SM, wszystko wydalem.";
		static const char* const k[] = { "Mam $SM SM.", "Jakies $SM SM, nie wiecej." };
		return PBC_SAY(g, k);
	}

	inline std::string GenKs(TGen& g)
	{
		// MT2009_PLUS_BOT_CHAT_V2: accused in the middle of a quarrel over
		// its own spot (playerbot_spot_defense.h).
		if (g.s.spotGaveUp)
			return "Juz ci zostawilem ten spot, wiec o co chodzi?";
		if (g.s.spotQuarrel > 0)
		{
			g.reason = "Bo bylem tu pierwszy i bije te moby od dawna.";
			const int temper = TemperOf(g.s.style);
			if (temper >= 2)
			{
				static const char* const k[] = { "Ja ci kradne? Bylem tu pierwszy xd", "To ty mi kradniesz, nie odwracaj kota ogonem",
					"Hahaha dobre, to moj spot od godziny" };
				return PBC_SAY(g, k);
			}
			if (temper == 0)
			{
				static const char* const k[] = { "Sorki, juz ide gdzie indziej.", "Dobra, zostawiam ci ten spot." };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Bylem tu pierwszy, ale dobra, mozemy sie podzielic.", "Expie tu od dawna, poszukaj czegos obok." };
			return PBC_SAY(g, k);
		}
		if (g.tier == TIER_HOSTILE)
			return "Nie widzialem tam twojego imienia.";
		static const char* const k[] = {
			"Sorki, nie zauwazylem, ze go bijesz.", "Oj, wybacz, nie widzialem ciebie.", "Sorry, nie chcialem ci ksowac." };
		return PBC_SAY(g, k);
	}

	inline std::string GenReady(TGen& g)
	{
		if (g.s.dead)
			return "Chwila, jeszcze leze.";
		if (g.LowHp())
			return "Chwila, najpierw sie podlecze.";
		static const char* const k[] = { "Gotowy!", "Jasne, rdy.", "Moge isc." };
		return PBC_SAY(g, k);
	}

	inline std::string GenGoodLuck(TGen& g)
	{
		static const char* const k[] = { "Dzieki, tobie tez!", "Nawzajem!", "Dzieki, przyda sie." };
		return PBC_SAY(g, k);
	}

	inline std::string GenBrb(TGen& g)
	{
		static const char* const k[] = { "Jasne, czekam.", "Ok, bede tu.", "Spoko." };
		return PBC_SAY(g, k);
	}

	// ------------------------------------------------------------- buffs

	inline std::string DurationText(int seconds)
	{
		if (seconds < 120)
			return ToString(seconds) + " s";
		const int minutes = seconds / 60;
		const int rest = seconds % 60;
		return rest ? ToString(minutes) + " min " + ToString(rest) + " s" : ToString(minutes) + " min";
	}

	// What one buff does, in the words a player reads it in. The numbers are
	// the engine's (TBuffLine); only the words are chosen here.
	inline std::string BuffEffectText(const TBuffLine& l)
	{
		char buf[160];
		buf[0] = 0;
		switch (l.skill)
		{
			case CONV_SKILL_BLESSING:
				snprintf(buf, sizeof(buf), "o %d%% mniej obrazen", l.amount);
				break;
			case CONV_SKILL_REFLECT:
				snprintf(buf, sizeof(buf), "odbija %d%% obrazen wrecz", l.amount);
				break;
			case CONV_SKILL_DRAGON_AID:
				snprintf(buf, sizeof(buf), "+%d%% szansy na cios krytyczny", l.amount);
				break;
			case CONV_SKILL_CURE:
				if (l.amountMax > l.amount)
					snprintf(buf, sizeof(buf), "leczy %d-%d HP", l.amount, l.amountMax);
				else
					snprintf(buf, sizeof(buf), "leczy %d HP", l.amount);
				break;
			case CONV_SKILL_SWIFTNESS:
				if (l.amount2 > 0)
					snprintf(buf, sizeof(buf), "+%d do szybkosci ruchu i +%d%% do szybkosci czarowania",
							l.amount, l.amount2);
				else
					snprintf(buf, sizeof(buf), "+%d do szybkosci ruchu", l.amount);
				break;
			case CONV_SKILL_ATTACK_UP:
				snprintf(buf, sizeof(buf), "+%d do wartosci ataku", l.amount);
				break;
			default:
				break;
		}
		std::string out = buf;
		if (l.seconds > 0)
			out += " przez " + DurationText(l.seconds);
		if (l.skill == CONV_SKILL_CURE && l.amount3 > 0)
		{
			out += ", do tego oslona na " + ToString(l.amount3) + " obrazen od potworow";
			if (l.seconds3 > 0)
				out += " przez " + DurationText(l.seconds3);
		}
		return out;
	}

	// "zbuffuj mnie", "dasz buffa?" - asked for, not asked about.
	inline bool IsBuffRequest(const TAnalysis& a)
	{
		for (size_t i = 0; i < a.tokens.words.size(); ++i)
		{
			const std::string& w = a.tokens.words[i];
			if (StartsWith(w, "zbuf") || StartsWith(w, "buffuj") || StartsWith(w, "bufuj") ||
					StartsWith(w, "buffnij") || StartsWith(w, "bufnij"))
				return true;
		}
		return a.concepts.Has(C_BUFF) && (a.tokens.Has("daj") || a.tokens.Has("dasz") ||
				a.tokens.Has("potrzebuje") || (a.concepts.Has(C_CAN) && a.concepts.Has(C_ME)));
	}

	// "co daja twoje buffy?", "ile daje blogoslawienstwo?" - the Shaman's
	// buffs of its path as the engine would cast them on the person asking.
	inline std::string GenBuffs(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		int askedWord = -1;
		const unsigned int asked = g.a && g.a->concepts.Has(C_BUFFNAME) ? NamedBuffSkill(g.a->tokens, askedWord) : 0;
		g.reason = "Tyle wychodzi z moich skilli i inteligencji.";
		if (s.job != 3)
		{
			std::string out = std::string("Nie mam buffow, gram ") + ClassNameInstr(s.job) + ".";
			Append(out, "Buffy daje szaman.");
			return out;
		}
		const int build = s.Build();
		if (build == B_NONE)
			return "Nie wybralem jeszcze sciezki, wiec buffow jeszcze nie mam.";
		if (asked && BuffBuildOf(asked) != build)
			return std::string("Tego nie mam, jestem ") + BuildPhrase(build) + ". " + SkillNameOf(asked) +
					" ma szaman " + BuildShortName(BuffBuildOf(asked)) + ".";
		TBuffReport report;
		if (!g.world || !g.world->DescribeBuffs(report) || report.count <= 0)
			return "Nie umiem ci teraz tego policzyc.";
		std::string list;
		int shown = 0;
		int unlearnt = 0;
		for (int i = 0; i < report.count && i < 3; ++i)
		{
			const TBuffLine& l = report.lines[i];
			if (asked && l.skill != asked)
				continue;
			if (l.level <= 0)
			{
				if (asked)
					return std::string("Tego jeszcze sie nie nauczylem (") + SkillNameOf(l.skill) + ").";
				++unlearnt;
				continue;
			}
			if (!l.known)
				continue;
			Append(list, std::string(SkillNameOf(l.skill)) + " (" + SkillGradeText(l.level) + ") - " +
					BuffEffectText(l) + ".");
			++shown;
		}
		if (shown == 0)
			return unlearnt ? "Zadnego buffa jeszcze sie nie nauczylem." : "Nie umiem ci teraz tego policzyc.";
		std::string out = std::string(report.onAsker ? "Na tobie: " : "Moje buffy: ") + list;
		if (unlearnt)
			Append(out, "Reszty jeszcze nie umiem.");
		if (g.a && IsBuffRequest(*g.a))
			Append(out, s.askerInParty ? "Jestesmy w PT, to pilnuje twoich buffow." :
					"Zapros mnie do PT, to bede cie buffowac.");
		return out;
	}

	// --------------------------------------------------------- coming over

	// Whether the line gives a reason a stranger would come for.
	inline bool SummonHasReason(const TAnalysis& a)
	{
		static const int kReasons[] = {
			C_EXP, C_HIT, C_MOB, C_METIN, C_BOSS, C_DT, C_HELP, C_PARTY, C_TRADE, C_SHOP, C_BUYME,
			C_SELLYOU, C_GIVE, C_QUEST, C_BIO, C_WAR, C_PVP, C_BUFF, C_BUFFNAME, C_FISH, C_MINE, C_ITEMWORD,
			C_GEAR, C_GOLD, C_UPGRADE, C_GUILD, C_HORSE };
		for (size_t i = 0; i < sizeof(kReasons) / sizeof(kReasons[0]); ++i)
			if (a.concepts.Has(kReasons[i]))
				return true;
		// "chodz" is a joining word too, and the one the summon itself is said
		// with: only another one ("razem", "zaprosze", "dolacz") is a reason.
		if (a.concepts.Has(C_JOIN))
		{
			const int w = a.concepts.firstWord[C_JOIN];
			const std::string word = w >= 0 && w < (int)a.tokens.words.size() ? a.tokens.words[w] : std::string();
			if (word != "chodz" && word != "choc" && word != "chodzze")
				return true;
		}
		static const char* const kWords[] = {
			"dam", "dac", "dostaniesz", "prezent", "pokaze", "pokazac", "pomoc", "pomoz", "pomozesz",
			"potrzebuje", "sprawa", "sprawe", "pogadac", "porozmawiac", "zobaczysz", "zobacz" };
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (a.tokens.Has(kWords[i]))
				return true;
		return a.offerYang > 0;
	}

	// The same answer to the same stranger: the pair decides, not a roll per
	// line, or asking three times would be the way round a refusal.
	inline int SummonStrangerRoll(const TGen& g)
	{
		return (int)(HashStr("summon", g.m.botPID * 2654435761u ^ (g.m.playerPID * 40503u)) % 100u);
	}

	// How many strangers a voice sends away without asking what for.
	inline int SummonRefuseShare(const TGen& g)
	{
		int share = 30;
		switch (g.voice)
		{
			case V_SOCIAL: share = 10; break;
			case V_WANDERER: share = 25; break;
			case V_GRINDER: share = 45; break;
			default: break;
		}
		if (g.Bad())
			share += 20;
		return share;
	}

	inline std::string SummonRefusal(TGen& g, int block)
	{
		switch (block)
		{
			case SB_OTHER_MAP:
				g.reason = "Bo jestem na innej mapie.";
				if (IsKnownMap(g.s.mapIndex))
					return Fill(g, "Jestem daleko, $MAPIN. Stad nie dam rady przyjsc.");
				return "Jestem daleko, na innej mapie. Stad nie dam rady przyjsc.";
			case SB_OTHER_CHANNEL:
				// $MAPIN names the channel to a person on the other one.
				g.reason = "Bo jestem na innym kanale.";
				return Fill(g, "Jestem $MAPIN, a ty na innym kanale. Stad nie dam rady przyjsc.");
			case SB_STALL:
				g.reason = "Bo pilnuje straganu.";
				return "Stoje teraz ze straganem, nie moge odejsc.";
			case SB_FISHING:
				g.reason = "Bo lowie.";
				return "Wlasnie lowie, nie zostawie wedki.";
			case SB_MINING:
				g.reason = "Bo kopie rude.";
				return "Kopie teraz rude, nie moge odejsc.";
			case SB_DUEL:
				g.reason = "Bo mam pojedynek.";
				return "Mam teraz pojedynek, pozniej.";
			case SB_GUILD_WAR:
				g.reason = "Bo moja gildia ma wojne.";
				return "Moja gildia ma teraz wojne, nie moge.";
			case SB_TOWER:
				g.reason = "Bo jestem w Wiezy Demonow.";
				return "Jestem z gildia w Wiezy Demonow, teraz nie wyjde.";
			case SB_DUNGEON:
				g.reason = "Bo jestem w lochu.";
				return "Jestem w lochu, teraz stad nie wyjde.";
			case SB_MERC:
				g.reason = "Bo mam kontrakt.";
				return "Mam kontrakt, najpierw musze go skonczyc.";
			case SB_OTHER_PARTY:
				g.reason = "Bo jestem z kims innym.";
				return "Jestem teraz z kims w druzynie, nie moge odejsc.";
			case SB_OTHER_SUMMON:
				g.reason = "Bo juz ide do kogos innego.";
				return "Juz ide do kogos innego, sorki.";
			case SB_DEAD:
				return "Chwila, najpierw wstane.";
			default:
				return "Teraz nie moge, sorki.";
		}
	}

	// The walk begins - or the engine, asked once more, says no.
	inline std::string SummonGo(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.summonBlock != SB_NONE)
			return SummonRefusal(g, s.summonBlock);
		const int code = g.world ? g.world->StartSummon() : (int)SUMMON_START_FAILED;
		g.reason = "Bo mnie zawolales.";
		switch (code)
		{
			case SUMMON_START_OK:
			case SUMMON_START_RENEWED:
				if (s.askerOnMap && s.askerDistance >= 0 && s.askerDistance <= CONV_SUMMON_NEAR_DISTANCE)
					return "Jestem obok, poczekam chwile.";
				if (g.tier >= TIER_FRIEND)
				{
					static const char* const k[] = { "Jasne, juz lece!", "Dla ciebie zawsze, juz ide!" };
					return PBC_SAY(g, k);
				}
				{
					static const char* const k[] = { "Juz ide!", "Dobra, zaraz bede.", "Ok, ide do ciebie." };
					return PBC_SAY(g, k);
				}
			case SUMMON_START_BLOCKED:
				return "Teraz nie moge, sorki.";
			default:
				return "Nie widze cie, gdzie jestes?";
		}
	}

	// "chodz do mnie", "przyjdz", "podejdz". Somebody the bot knows it comes
	// to; a stranger is asked what for (or, by the pair's own roll, sent away);
	// what the bot cannot leave it says it cannot leave.
	inline std::string GenSummon(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.summonedByAsker)
		{
			const int code = g.world ? g.world->StartSummon() : (int)SUMMON_START_FAILED;
			g.reason = "Bo mnie zawolales.";
			if (code == SUMMON_START_OK || code == SUMMON_START_RENEWED)
				return s.summonArrived ? "Przeciez jestem obok :) Zostane jeszcze chwile." : "Juz ide, juz!";
			return "Teraz nie moge, sorki.";
		}
		if (g.tier == TIER_HOSTILE)
		{
			g.reason = "Bo mnie obrazasz.";
			return "Po tym, jak mnie traktujesz? Nie.";
		}
		if (s.summonBlock != SB_NONE)
			return SummonRefusal(g, s.summonBlock);
		if (g.tier == TIER_STRANGER && !(g.a && SummonHasReason(*g.a)))
		{
			const bool askedAlready = g.m.botAsk == ASK_SUMMON && g.now - g.m.botAskAt < CONV_BOT_ASK_TTL_MS;
			if (askedAlready || SummonStrangerRoll(g) < SummonRefuseShare(g))
			{
				g.reason = "Bo sie nie znamy.";
				static const char* const k[] = {
					"Nie znamy sie, a ja mam swoje sprawy.", "Sorki, nie chodze do obcych bez powodu." };
				return PBC_SAY(g, k);
			}
			// The question belongs to this reply and outranks an "a ty?" another
			// line of the same batch may have put there: without it the answer
			// is not read as one, and a stranger with no reason must not be
			// walked to by default.
			g.askBack = "Po co mam przyjsc?";
			g.askBackKind = ASK_SUMMON;
			g.askBackTopic = T_NONE;
			return "Hm, nie znamy sie.";
		}
		return SummonGo(g);
	}

	// "mozesz isc", "wracaj do siebie".
	inline std::string GenDismiss(TGen& g)
	{
		if (g.s.summonedByAsker)
		{
			if (g.world)
				g.world->EndSummon();
			static const char* const k[] = {
				"Dobra, to wracam do swoich spraw.", "Ok, to lece. Na razie!", "Jasne. Gdyby co, pisz." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Przeciez nigdzie za toba nie chodze :)", "Dobra, i tak mam swoje sprawy." };
		return PBC_SAY(g, k);
	}

	// Thanks, goodbye or an insult from the person who called the bot over
	// ends the stay as well.
	inline bool ReleaseSummonFor(TGen& g)
	{
		if (!g.s.summonedByAsker || !g.world)
			return false;
		g.world->EndSummon();
		return true;
	}

	// ------------------------------------------------ the bot's gear argued about

	// The item a line names, as a reply says it back: the link as it came,
	// the players' word with its plus ("FMS +9"), or nothing.
	inline std::string ShownItem(const TGen& g)
	{
		return g.a ? g.a->itemShown : std::string();
	}

	// "$ITEM? ..." with the item filled in.
	inline std::string SayWithItem(TGen& g, const char* const* variants, size_t n, const std::string& item)
	{
		std::string out = Say(g, variants, n);
		ReplaceAll(out, "$ITEM", item);
		CapitalizeFirst(out);
		return out;
	}

	// Whether the line names the family the AI is playing for: "rib" when the
	// goal is Ostrze Czerwonej Stali.
	inline bool LineNamesGoal(const TGen& g)
	{
		if (!g.a || g.a->object.empty() || g.s.weaponGoal.empty())
			return false;
		return ItemNameMatches(g.s.weaponGoal.c_str(), g.a->object);
	}

	// Why the bot holds the weapon it holds, from what the AI actually has in
	// mind: the goal of playerbot_weapon_goal.h against the purse, the plus.
	// Said in full once; asked again within CONV_FACT_TTL_MS it is a short
	// "jak mowilem", because four replies ending in the same sentence is the
	// repetition the gear line had - and within CONV_REASON_RECENT_MS it is
	// not said at all beside a reply that has something of its own
	// (`haveContent`). An empty answer leaves the last reason standing for a
	// "a czemu?" that follows.
	inline bool GearReasonSaidLately(const TGen& g, u32 window)
	{
		return g.m.gearReasonAt != 0 && g.now - g.m.gearReasonAt < window;
	}

	inline std::string GearReason(TGen& g, bool haveContent)
	{
		const TBotSnapshot& s = g.s;
		const bool again = GearReasonSaidLately(g, CONV_FACT_TTL_MS);
		if (again && haveContent && GearReasonSaidLately(g, CONV_REASON_RECENT_MS))
			return std::string();
		g.m.gearReasonAt = g.now != 0 ? g.now : 1;
		const bool saving = s.weaponOutclassed && !s.weaponGoal.empty() && s.weaponGoalPrice > 0 &&
				s.gold < s.weaponGoalPrice;
		if (again)
		{
			if (saving || s.gold < 1000000)
			{
				static const char* const k[] = { "Tylko na razie kasy brak, jak pisalem.", "Kasa, jak mowilem. Zbieram.",
					"Tylko najpierw musze uzbierac.", "Na razie zbieram, jak mowilem." };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Ale na razie zostaje przy swojej.", "Jak trafie cos w normalnej cenie, to zmienie.",
				"Na razie ta mi wystarcza." };
			return PBC_SAY(g, k);
		}
		if (s.weaponName.empty())
			return "Na razie w ogole nie mam porzadnej broni, zbieram na cos.";
		if (s.goal == G_EQUIPMENT || s.marketTrip || s.action == A_MARKET)
			return "Wlasnie sie za czyms lepszym rozgladam.";
		if (s.weaponIsGoal)
			return "Na moj poziom lepszej za bardzo nie ma, sprawdzalem.";
		if (saving)
			return g.tier >= TIER_KNOWN ? Fill(g, "Odkladam na nowa bron ($GOAL), ale kosztuje z $GOALPRICE, a mam $GOLD.") :
					Fill(g, "Odkladam na nowa bron ($GOAL), ale jeszcze mnie nie stac.");
		if (s.weaponOutclassed && !s.weaponGoal.empty())
			return Fill(g, "Celuje w nowa bron ($GOAL), tylko nikt jej nie wystawia w normalnej cenie.");
		if (s.weaponPlus >= 7)
			return Fill(g, "Moja ma +$WPLUS i jeszcze daje rade, szkoda mi jej.");
		if (s.gold < 1000000)
			return "Na lepsza mnie jeszcze nie stac.";
		return "Jeszcze nie trafilem na nic lepszego w normalnej cenie.";
	}

	// The family the bot is playing for, named by the person: it holds one
	// already, it is saving for one (named again: "no mowie" - the reason it
	// gave said so), or it would take one some day but is not chasing it.
	inline std::string GoalNamedReaction(TGen& g, const std::string& shown)
	{
		const TBotSnapshot& s = g.s;
		if (s.weaponIsGoal)
		{
			static const char* const k[] = { "$ITEM? Przeciez taka mam :)", "Przeciez ja mam $ITEM :)" };
			return SayWithItem(g, k, 2, shown);
		}
		if (s.weaponOutclassed)
		{
			if (GearReasonSaidLately(g, CONV_FACT_TTL_MS))
			{
				static const char* const k[] = { "No mowie, na $ITEM zbieram :)", "Przeciez pisze, ze na $ITEM odkladam :)" };
				return SayWithItem(g, k, 2, shown);
			}
			static const char* const k[] = { "$ITEM? No wlasnie na to zbieram!", "$ITEM? Na to wlasnie odkladam!" };
			return SayWithItem(g, k, 2, shown);
		}
		static const char* const k[] = { "$ITEM? Kiedys na pewno.", "$ITEM to by bylo cos, kiedys." };
		return SayWithItem(g, k, 2, shown);
	}

	// "czemu nie wymienisz broni?", "czemu nie kupisz sobie riba?", "czemu,
	// przeciez ta bron ma srednie": the point granted when it is a fair one,
	// the item they named taken up, and the bot's own reason.
	inline std::string GenGearWhy(TGen& g)
	{
		const TAnalysis* a = g.a;
		const TBotSnapshot& s = g.s;
		std::string out;
		const std::string shown = ShownItem(g);
		if (a && a->concepts.Has(C_UPGRADE) && !s.weaponName.empty())
		{
			out = s.weaponPlus >= 7 ? Fill(g, "Moja ma juz +$WPLUS, dalej to juz loteria u kowala.") :
					"Ulepszam, jak mam materialy i kase na kowala.";
			g.reason = out;
			return out;
		}
		if (a && a->levelNamed > 0 && s.weaponLevel > 0 &&
				(a->levelNamed + 5 < s.weaponLevel || a->levelNamed > s.weaponLevel + 5))
			out = Fill(g, "To bron na $WLVL poziom, nie na ") + ToString((long long)a->levelNamed) + " :)";
		else if (a && (a->tokens.Has("przeciez") || a->tokens.Has("ale")) && a->concepts.Has(C_BONUS))
		{
			static const char* const k[] = { "No ma, nie przecze.", "Racja, srednie robia robote.", "Wiem, wiem." };
			out = PBC_SAY(g, k);
		}
		else if (s.weaponLevel > 0 && s.weaponLevel + 15 <= s.level)
		{
			static const char* const k[] = { "Wiem, stara jest.", "No wiem, juz troche odstaje.", "Tak, dawno jej nie zmienialem." };
			out = PBC_SAY(g, k);
		}
		if (!shown.empty())
		{
			if (LineNamesGoal(g))
				Append(out, GoalNamedReaction(g, shown));
			else
			{
				static const char* const k[] = { "$ITEM? Dobry pomysl.", "$ITEM? Tez o tym myslalem.", "$ITEM to niezly wybor." };
				Append(out, SayWithItem(g, k, 3, shown));
			}
		}
		const std::string reason = GearReason(g, out.size() >= 25);
		Append(out, reason);
		g.reason = reason.empty() ? g.m.lastReason : reason;
		return out;
	}

	// "zmien bron", "potrzebne ci sa obrazenia", "kup sobie riba".
	inline std::string GenGearAdvice(TGen& g)
	{
		const TAnalysis* a = g.a;
		std::string out;
		const std::string shown = ShownItem(g);
		if (a && a->concepts.Has(C_BONUS))
		{
			static const char* const k[] = { "Wiem, obrazenia sie licza.", "No tak, bez obrazen daleko nie zajde.",
				"Masz racje, z lepsza bronia szybciej by szlo." };
			out = PBC_SAY(g, k);
		}
		else if (!shown.empty())
		{
			if (LineNamesGoal(g))
				out = GoalNamedReaction(g, shown);
			else
			{
				static const char* const k[] = { "$ITEM? Moze to dobry pomysl.", "$ITEM? Pomysle o tym." };
				out = SayWithItem(g, k, 2, shown);
			}
		}
		else
		{
			static const char* const k[] = { "Moze masz racje.", "Pomysle o tym.", "Wiem, przydaloby sie cos lepszego." };
			out = PBC_SAY(g, k);
		}
		const std::string reason = GearReason(g, out.size() >= 25);
		Append(out, reason);
		g.reason = reason.empty() ? g.m.lastReason : reason;
		return out;
	}

	// "co myslisz o broni ze srednimi?", "jaka bron jest najlepsza?"
	inline std::string GenGearOpinion(TGen& g)
	{
		const TAnalysis* a = g.a;
		const TBotSnapshot& s = g.s;
		const std::string shown = ShownItem(g);
		const bool which = a && a->concepts.Has(C_ADVICE) &&
				(a->concepts.Has(C_WHICH) || a->concepts.Has(C_WHAT) || a->concepts.Has(C_OR));
		if (which)
		{
			if (a->concepts.Has(C_ME) && !a->concepts.Has(C_YOU))
			{
				static const char* const k[] = { "Zalezy od klasy i poziomu, ale taka ze srednimi zawsze sie oplaca.",
					"Na twoj poziom? Bierz cos ze srednimi, to sie zawsze oplaca." };
				return PBC_SAY(g, k);
			}
			if (s.weaponIsGoal && !s.weaponName.empty())
				return Fill(g, "Na moj poziom chyba ta, ktora mam: $WEAPON.");
			if (!s.weaponGoal.empty())
				return Fill(g, "Na moj poziom chyba $GOAL. Na nia odkladam.");
			static const char* const k[] = { "Zalezy od poziomu i klasy. Ja bym bral cos ze srednimi.",
				"Kazda ma swoje plusy. Byle ze srednimi.", "Zalezy, do czego. Na expa te ze srednimi." };
			return PBC_SAY(g, k);
		}
		std::string out;
		if (a && a->concepts.Has(C_BONUS))
		{
			static const char* const k[] = { "Srednie to podstawa na expie, wszystko szybciej schodzi.",
				"Bron ze srednimi to swietna sprawa, tylko dobre sa drogie.", "Bez srednich ani rusz, to wiem." };
			out = PBC_SAY(g, k);
			if (!shown.empty())
			{
				if (!LineNamesGoal(g))
					Append(out, shown + " to solidna sprawa.");
				else if (s.weaponIsGoal)
					Append(out, "Sam taka mam.");
				else if (s.weaponOutclassed)
					Append(out, "Sam na " + shown + " odkladam.");
				else
					Append(out, shown + " to by bylo cos.");
			}
		}
		else if (!shown.empty())
		{
			static const char* const k[] = { "$ITEM? Z dobrymi srednimi to marzenie :)", "$ITEM to solidna sprawa." };
			out = SayWithItem(g, k, 2, shown);
		}
		else
		{
			static const char* const k[] = { "Dobra bron to podstawa, reszta to dodatki.", "Liczy sie bron i bonusy, jak dla mnie." };
			out = PBC_SAY(g, k);
		}
		return out;
	}

	// "a jakbym ci dal riba +9 ze srednimi, wymienilbys?" - of course.
	inline std::string GenGiftOffer(TGen& g)
	{
		if (g.tier == TIER_HOSTILE)
			return "Od ciebie? Watpie :P";
		const std::string shown = ShownItem(g);
		const bool swap = g.a && g.a->concepts.Has(C_SWAP);
		if (!shown.empty())
		{
			if (swap)
			{
				static const char* const k[] = { "Jasne, ze bym wymienil! $ITEM to by bylo cos :D", "Od razu bym wymienil, $ITEM to by bylo cos." };
				return SayWithItem(g, k, 2, shown);
			}
			static const char* const k[] = { "$ITEM? Bralbym w ciemno :D", "$ITEM? Jasne, ze tak! Od razu bym zalozyl.",
				"Pewnie! $ITEM to by bylo cos." };
			return SayWithItem(g, k, 3, shown);
		}
		if (swap)
		{
			static const char* const k[] = { "Jasne, ze bym wymienil!", "Od razu bym wymienil :D" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Pewnie, ze tak!", "Bralbym bez zastanowienia :D", "No ba! Kto by nie wzial." };
		return PBC_SAY(g, k);
	}

	// "zoba jaki fms 9", a shift-clicked "[Miecz Pelni Ksiezyca+9]".
	inline std::string GenShowItem(TGen& g)
	{
		const TAnalysis* a = g.a;
		const std::string shown = ShownItem(g);
		const int plus = a ? a->objectPlus : -1;
		std::string out;
		// "moglas byc tu z nami, zoba..."
		if (a && (a->concepts.Has(C_WE) || a->concepts.Has(C_WITHME)))
			out = "Szkoda, ze mnie nie bylo!";
		// Shown again a moment later: seen it, and said so, not the same
		// admiration twice.
		if (g.m.lastAnswered == I_SHOW_ITEM && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
		{
			static const char* const k[] = { "No widze, widze :) Piekna sztuka.", "Juz widzialem, szacun :D", "No, robi wrazenie." };
			Append(out, PBC_SAY(g, k));
			return out;
		}
		if (shown.empty())
		{
			static const char* const k[] = { "O, ladne. Gratki!", "Fajne, gratki!", "No no, niezle." };
			Append(out, PBC_SAY(g, k));
			return out;
		}
		if (plus >= 8)
		{
			static const char* const k[] = { "Ale sztuka! $ITEM, szacun.", "O kurcze, $ITEM! Zazdroszcze.", "No no, $ITEM. Ile w to wlozyles?" };
			Append(out, SayWithItem(g, k, 3, shown));
		}
		else if (plus >= 5)
		{
			static const char* const k[] = { "Niezle! $ITEM to juz cos.", "O, $ITEM. Ladnie." };
			Append(out, SayWithItem(g, k, 2, shown));
		}
		else
		{
			static const char* const k[] = { "O, $ITEM. Ladne.", "Fajne, gratki!" };
			Append(out, SayWithItem(g, k, 2, shown));
		}
		if (LineNamesGoal(g) && !g.s.weaponIsGoal && g.s.weaponOutclassed)
			Append(out, "Sam na taka odkladam.");
		return out;
	}

	// ------------------------------------------- a person talking at the bot

	// "przestan do mnie pisac": said once, and the bot keeps to it
	// (TConvMemory::quietUntil keeps it from starting anything).
	inline std::string GenStopTalking(TGen& g)
	{
		const bool released = ReleaseSummonFor(g);
		static const char* const k[] = { "Dobra, juz nie pisze.", "Ok, nie przeszkadzam.", "Spoko, juz daje spokoj." };
		std::string out = PBC_SAY(g, k);
		if (released)
			Append(out, "Wracam do swoich spraw.");
		return out;
	}

	// "bana ci daje": a question back about what for - and after a sum it had
	// just got right, that sum.
	inline std::string GenThreat(TGen& g)
	{
		if (g.m.lastAnswered == I_MATH && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
			return "Za co? Przeciez dobrze policzylem :P";
		if (g.m.negative >= 3)
			return "Rob, co chcesz.";
		static const char* const k[] = { "Za co? Przeciez nic ci nie zrobilem.", "Hej, spokojnie, za co od razu ban?",
			"Ban? A za co, za pisanie? :(" };
		return PBC_SAY(g, k);
	}

	// Banter answered as banter: "bieda", "zawijaj stad", "tyle jestes
	// warta", "daleko w zyciu zajdziesz". After "przestan do mnie pisac" or
	// from somebody hostile, only a short "jak uwazasz".
	inline std::string GenMock(TGen& g)
	{
		const TAnalysis* a = g.a;
		if (a && a->answeredAsk == ASK_JOIN)
			return "Haha, dobra, to sam sobie pobije :P";
		if (IsQuiet(g.m, g.now) || g.tier == TIER_HOSTILE)
		{
			static const char* const k[] = { "Jak uwazasz.", "Niech ci bedzie.", "Ok." };
			return PBC_SAY(g, k);
		}
		bool leave = false, worth = false, future = false, poor = false, weak = false;
		if (a)
		{
			for (size_t i = 0; i < a->tokens.words.size(); ++i)
			{
				const std::string& w = a->tokens.words[i];
				if (w == "zawijaj" || w == "zawijajcie" || w == "wypad" || w == "spadaj" || w == "wypadaj" ||
						w == "zjezdzaj" || w == "spieprzaj" || w == "zmiataj" || w == "won" || w == "sio" ||
						w == "wynocha" || StartsWith(w, "spierd") || StartsWith(w, "wypierd"))
					leave = true;
				if (w == "wart" || w == "warta" || w == "warty")
					worth = true;
				if (StartsWith(w, "zajdziesz"))
					future = true;
				if (StartsWith(w, "bied"))
					poor = true;
				if (StartsWith(w, "slab") || StartsWith(w, "cienk") || StartsWith(w, "cieniut") || StartsWith(w, "zenad") ||
						StartsWith(w, "zenuj"))
					weak = true;
			}
		}
		if (leave)
		{
			// MT2009_PLUS_BOT_CHAT_V2: "spadaj stad" said to a bot that is
			// hunting beside the person - its spot - is answered by its temper.
			// It already gave the spot up (playerbot_spot_defense.h).
			if (g.s.spotGaveUp)
			{
				static const char* const k[] = { "Przeciez juz ide, spokojnie", "No ide juz, ide. Masz ten spot" };
				return PBC_SAY(g, k);
			}
			if (g.s.spotQuarrel > 0 || (Fighting(g) && g.s.askerNear))
			{
				g.reason = "Bo to moj spot, bylem tu pierwszy.";
				const int temper = TemperOf(g.s.style);
				if (temper >= 2)
				{
					static const char* const k[] = { "Sam spadaj, bylem tu pierwszy", "Chyba ty xd to moj spot",
						"Nigdzie nie ide, szukaj se innego spota", "Hahaha nie. Ty spadaj" };
					return PBC_SAY(g, k);
				}
				if (temper == 0)
				{
					static const char* const k[] = { "Dobra, dobra, juz ide gdzie indziej", "Spoko, nie bede przeszkadzal" };
					return PBC_SAY(g, k);
				}
				static const char* const k[] = { "Bylem tu pierwszy, ale niech ci bedzie", "Eh, dobra, poszukam innego spota",
					"A moze grzeczniej? Ale dobra, ide" };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Haha, dobra, juz sie zwijam :P", "Dobra, dobra, juz mnie nie ma :D",
				"A gdzie mam isc? xd", "Spokojnie, i tak zaraz ide" };
			return PBC_SAY(g, k);
		}
		if (worth)
		{
			static const char* const k[] = { "Moze i niewiele, ale uczciwie zarobione :P", "Auc. Ale sie nie poddaje :P" };
			return PBC_SAY(g, k);
		}
		if (future)
		{
			static const char* const k[] = { "Hehe, na razie zajde do nastepnego poziomu.", "Krok po kroku, jak na expie :P" };
			return PBC_SAY(g, k);
		}
		if (poor)
		{
			static const char* const k[] = { "Kazdy kiedys zaczynal :P", "Bieda, ale uczciwa.", "Spokojnie, jeszcze sie odkuje." };
			return PBC_SAY(g, k);
		}
		if (weak)
		{
			static const char* const k[] = { "Kazdy jest slaby na poczatku :P", "Jeszcze zobaczysz :P" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Haha, dobra, dobra.", "Hehe, jak tam chcesz :P" };
		return PBC_SAY(g, k);
	}

	// "ile to 2+2" - the number, the way a person writes it.
	inline std::string GenMath(TGen& g)
	{
		const TAnalysis* a = g.a;
		if (!a)
			return "Hm?";
		if (a->mathDivZero)
			return "Przez zero sie nie dzieli :P";
		if (a->mathTooBig || a->mathText.empty())
			return "Za duze liczby jak na moja glowe :D";
		const std::string r = a->mathText;
		if (r == "4" && a->tokens.norm.find("2 +2") != std::string::npos)
			return "4. To akurat wiem :D";
		if (a->mathMixed)
			return r + ". Najpierw mnozenie :P";
		static const char* const k[] = { "$R.", "Wychodzi $R.", "$R :)", "Hmm... $R." };
		std::string out = Pick(g, k, 4);
		ReplaceAll(out, "$R", r);
		return out;
	}

	// "to powiedziales mi, ze w Joan": it did say so - and moved since.
	inline std::string GenContradiction(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		const long mentioned = MentionedMap(g);
		if (!IsKnownMap(s.mapIndex))
			return "Nie klamie :) Po prostu sie przenioslem.";
		g.saidMap = true;
		if (mentioned > 0 && mentioned != s.mapIndex && IsKnownMap(mentioned))
		{
			if (SaidMapLately(g, mentioned))
			{
				static const char* const k[] = { "Bo wtedy bylem $WASAT :) Teraz jestem juz $MAPIN.",
					"Bylem $WASAT, ale juz sie przenioslem - teraz jestem $MAPIN." };
				return WithOldMap(g, Pick(g, k, 2), mentioned);
			}
			return Fill(g, "Nie, jestem $MAPIN. Moze cos ci sie pomylilo?");
		}
		if (mentioned == s.mapIndex)
			return Fill(g, "No tak, dalej jestem $MAPIN.");
		long old = RecentOtherMap(g);
		if (!old && g.m.prevSaidMap != 0 && g.m.prevSaidMap != s.mapIndex && IsKnownMap(g.m.prevSaidMap) &&
				SaidMapLately(g, g.m.prevSaidMap))
			old = g.m.prevSaidMap;
		if (old)
			return WithOldMap(g, "Wczesniej bylem $WASAT, teraz jestem juz $MAPIN.", old);
		static const char* const k[] = { "Nie klamie :) Moze sie zle wyrazilem.", "Hm, chyba sie nie zrozumielismy." };
		return PBC_SAY(g, k);
	}

	// "to nie lepiej na jakas wyzsza mape isc?", "czemu zmieniles mape?"
	inline std::string GenMapAdvice(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		const bool why = g.a && g.a->concepts.Has(C_WHY);
		g.saidMap = true;
		if (const long old = RecentOtherMap(g))
		{
			if (why)
				return Fill(g, "Bo $MAPNAME lepiej pasuje na moj poziom.");
			return WithOldMap(g, "Wlasnie tak zrobilem - bylem $WASAT, a teraz jestem juz $MAPIN.", old);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
			return Fill(g, why ? "Bo tam jest lepszy exp. Wlasnie ide $DEST." : "Wlasnie ide $DEST.");
		if (s.inTown)
			return why ? "Bo mam sprawy w miescie." : "Pewnie tak. Jak zalatwie sprawy w miescie, to sie przeniose.";
		if (why)
			return "Bo tu jest dobry exp na moj poziom.";
		static const char* const k[] = { "Tu mi pasuje, exp leci na moj poziom.", "Moze i tak. Zobacze, jak wbije pare poziomow." };
		return PBC_SAY(g, k);
	}

	// ------------------------------------------------------------- follow-ups

	inline std::string GenerateOne(TGen& g, const TAnalysis& a);

	// MT2009_PLUS_BOT_DUNGEON_LFG_V1: the answer to the bot's dungeon offer
	// (I_LFG_ANSWER). The engine does what the answer asks at the moment the
	// reply is composed - the teleport to the entrance on a yes, the wait
	// called off on a no (IConvWorld::LfgAccept / LfgDecline / LfgChoose) -
	// and the reply says what came of it, in the words of
	// playerbot_dungeon_lfg_rules.h: "Czekam pod wejsciem, bede tutaj 5
	// minut", "ok, spoko", "kurde, nie dam rady teraz przyjsc".
	inline std::string GenLfgAnswer(TGen& g, const TAnalysis& a)
	{
		playerbot_lfg::TFacts f;
		f.level = g.s.level;
		f.job = g.s.job;
		f.group = g.s.skillGroup;
		f.botName = g.s.name;
		f.playerName = g.s.askerName;
		f.key = g.m.lfg.key;
		switch (a.lfgAnswer)
		{
			case playerbot_lfg::ANSWER_YES:
			{
				const int go = g.world ? g.world->LfgAccept() : playerbot_lfg::GO_GONE;
				switch (go)
				{
					case playerbot_lfg::GO_TELEPORTED:
					case playerbot_lfg::GO_WALKING:
						g.m.lfg.state = playerbot_lfg::TALK_WAITING;
						g.m.lfg.at = g.now;
						return playerbot_lfg::WaitLine(g.rng, f, go == playerbot_lfg::GO_WALKING);
					case playerbot_lfg::GO_ALREADY:
						return playerbot_lfg::AlreadyLine(g.rng, f);
					case playerbot_lfg::GO_FAILED:
						g.m.lfg = playerbot_lfg::TTalk();
						return playerbot_lfg::CantComeLine(g.rng, f);
					default:
						g.m.lfg = playerbot_lfg::TTalk();
						return playerbot_lfg::GoneLine(g.rng, f);
				}
			}
			case playerbot_lfg::ANSWER_NO:
			{
				const bool waiting = g.m.lfg.state == playerbot_lfg::TALK_WAITING;
				if (g.world)
					g.world->LfgDecline();
				g.m.lfg = playerbot_lfg::TTalk();
				return waiting ? playerbot_lfg::LeaveLine(g.rng, f) : playerbot_lfg::DeclineLine(g.rng, f);
			}
			case playerbot_lfg::ANSWER_CHOOSE:
			{
				std::string resolved;
				const int fit = g.world ? g.world->LfgChoose(a.lfgKey, a.lfgDifficulty, resolved)
						: playerbot_lfg::CHOOSE_UNKNOWN;
				f.key = resolved.empty() ? a.lfgKey : resolved;
				if (fit == playerbot_lfg::CHOOSE_OK)
				{
					g.m.lfg.state = playerbot_lfg::TALK_OFFERED;
					g.m.lfg.key = f.key;
					g.m.lfg.at = g.now;
					return playerbot_lfg::ChosenLine(g.rng, f);
				}
				g.m.lfg = playerbot_lfg::TTalk();
				return playerbot_lfg::ChosenWrongLine(g.rng, f, fit);
			}
			case playerbot_lfg::ANSWER_WHICH:
				// The question stands, on the clock it was asked on.
				return playerbot_lfg::WhichAgainLine(g.rng, f);
			default:
				return std::string();
		}
	}

	inline std::string GenWhy(TGen& g, EIntent subject)
	{
		if (!g.m.lastReason.empty() && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
			return g.m.lastReason;
		switch (subject)
		{
			case I_ACTIVITY: case I_ACTIVITY_LOCATION: case I_LOCATION: case I_TARGET: case I_MOB_COUNT:
				if (Fighting(g))
				{
					static const char* const kWhy[V_COUNT] = {
						"Bo tu jest dobry exp na moj poziom.", "Bo akurat tu trafilem i jest spokojnie.",
						"Bo z tych mobow leci cos, co sie sprzedaje.", "Bo lubie walke, a tu jest z kim.",
						"Bo tu zawsze ktos jest." };
					return kWhy[g.voice];
				}
				if (g.s.inTown && g.s.bagCells > 0 && g.s.freeCells <= 5)
					return "Bo mialem pelne EQ i trzeba bylo sprzedac.";
				if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
					return "Bo mialem malo HP.";
				return "Tak wyszlo. Nie wszystko trzeba planowac.";
			case I_PARTY:
				return g.s.inParty ? "Bo razem sie lepiej expi." : (g.voice == V_GRINDER ? "Bo sam szybciej expie." : "Bo nikt mnie nie zaprosil :)");
			case I_GUILD:
				return g.s.inGuild ? "Bo sa tam fajni ludzie." : "Jakos nie trafilem na odpowiednia.";
			case I_REST: case I_HP:
				return "Bo mnie moby porzadnie obily.";
			case I_GOAL: case I_NEXT_PLAN:
			{
				static const char* const kWhy[V_COUNT] = {
					"Bo chce byc mocniejszy.", "Bo chce zobaczyc, co jest dalej.", "Bo to sie oplaci.",
					"Bo lubie wyzwania.", "Bo wtedy moge wiecej pomoc ekipie." };
				return kWhy[g.voice];
			}
			case I_GENERAL:
			{
				const TTopicPack* pack = FindTopicPack(g.m.lastTopic);
				if (pack)
					return PickField(g, pack->why, 2);
				break;
			}
			// MT2009_PLUS_BOT_CHAT_V2
			case I_INSULT: case I_THREAT: case I_MOCK:
			{
				static const char* const k[] = { "Bo mnie wyzywasz bez powodu.", "Bo tak sie nie rozmawia.",
					"A jak myslisz? Bo jestes niemily." };
				return PBC_SAY(g, k);
			}
			case I_JOKE:
				return "Bo poprosiles o kawal xd";
			case I_BEG:
				return "Bo sam na wszystko zapracowalem.";
			default:
				break;
		}
		static const char* const k[] = { "Tak wyszlo.", "Po prostu tak.", "Dobre pytanie. Tak jakos." };
		return PBC_SAY(g, k);
	}

	inline std::string GenFollowUp(TGen& g, const TAnalysis& a)
	{
		switch (a.follow)
		{
			case F_WHY:
				return GenWhy(g, a.subject);
			case F_CONFIRM:
			{
				static const char* const k[] = { "Serio.", "No mowie ci.", "Naprawde.", "Tak, na serio." };
				std::string out = PBC_SAY(g, k);
				if (IsGameIntent(a.subject) && g.rng.Chance(50))
				{
					TAnalysis sub = a;
					sub.intent = a.subject;
					sub.follow = F_NONE;
					std::string more = GenerateOne(g, sub);
					if (!more.empty() && more != g.m.lastReply)
						Append(out, more);
				}
				return out;
			}
			case F_HOW:
				switch (a.subject)
				{
					case I_PARTY_REQUEST: return "Po prostu zapros mnie do grupy.";
					case I_ACTIVITY: case I_ACTIVITY_LOCATION: return "Normalnie, bije i zbieram drop.";
					case I_GOLD: return g.voice == V_MERCHANT ? "Handel. Kupic tanio, sprzedac drozej." : "Drop i troche handlu.";
					case I_LEVEL: return "Expem, jak kazdy.";
					case I_HOW_ARE_YOU: return GenHowAreYou(g);
					default: return "Normalnie, po swojemu.";
				}
			case F_WHEN:
				if (a.subject == I_NEXT_PLAN || a.subject == I_TRAVEL || a.subject == I_REST || a.subject == I_GOAL)
				{
					static const char* const k[] = { "Pewnie za chwile.", "Niedlugo, zobaczymy." };
					return PBC_SAY(g, k);
				}
				return "Nie wiem dokladnie, zobaczymy.";
			case F_NEXT:
			{
				const TTopicPack* pack = FindTopicPack(g.m.lastTopic);
				if (pack && g.askBack.empty())
				{
					const std::string q = PickField(g, pack->ask, 2);
					if (!q.empty())
					{
						g.askBackKind = ASK_TOPIC;
						g.askBackTopic = (ETopic)pack->topic;
						return "No i tyle :) " + q;
					}
				}
				return "No i tyle :)";
			}
			case F_COUNT:
				return "Ale czego?";
			case F_THIS:
				return "Ale ktory?";
			case F_WHO:
				return "Kto? O kim mowisz?";
			case F_WHAT:
			default:
			{
				static const char* const k[] = { "Co masz na mysli?", "Ale co dokladnie?", "Hm?" };
				return PBC_SAY(g, k);
			}
		}
	}

	// MT2009_PLUS_BOT_CHAT_V2: "slaby", "suchar", "znam go" right after the
	// bot's joke - wherever the line ends up, it is about the joke.
	inline bool IsJokeCritique(const TGen& g, const TAnalysis& a)
	{
		if (g.m.lastAnswered != I_JOKE || g.now - g.m.lastAnsweredAt >= CONV_CONTEXT_TTL_MS)
			return false;
		const TTokens& t = a.tokens;
		return a.concepts.Has(C_NEGATIVE) || t.Has("slaby") || t.Has("slabe") || t.Has("suchy") || t.Has("suchar") ||
				t.Has("cienki") || t.Has("znam") || t.Has("stary") || t.Has("nudny") || t.Has("niesmieszny") ||
				(t.Has("nie") && t.Has("smieszne"));
	}

	inline std::string GenJokeCritique(TGen& g)
	{
		static const char* const k[] = { "No dobra, nie kazdy kawal musi byc smieszny xd", "Pff, wybredny :P",
			"To sam opowiedz lepszy xd", "Ej, staralem sie :(" };
		return PBC_SAY(g, k);
	}

	inline std::string GenAnswerToBot(TGen& g, const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		if (IsJokeCritique(g, a))
			return GenJokeCritique(g);
		const bool yes = c.Has(C_YES) || c.Has(C_ACK) || c.Has(C_POSITIVE) || c.Has(C_HAPPY);
		const bool no = c.Has(C_NO) || c.Has(C_NEGATIVE) || c.Has(C_SAD);
		// The question the line answers travels with it: the memory has
		// already closed it by the time the reply is composed.
		switch (a.answeredAsk != ASK_NONE ? (int)a.answeredAsk : (int)g.m.botAsk)
		{
			case ASK_HOW_ARE_YOU:
				if (no)
				{
					static const char* const k[] = { "Oj, szkoda. Bedzie lepiej.", "Kiepsko... Trzymaj sie." };
					return PBC_SAY(g, k);
				}
				if (yes || c.Has(C_POSITIVE))
				{
					static const char* const k[] = { "To dobrze!", "Super.", "No i git." };
					return PBC_SAY(g, k);
				}
				return "No, to jak u mnie.";
			case ASK_ACTIVITY:
				if (c.Has(C_EXP) || c.Has(C_HIT) || c.Has(C_MOB))
					return "O, to powodzenia na expie!";
				if (c.Has(C_FISH))
					return "Lowienie to relaks. Powodzenia.";
				if (c.Has(C_TRADE) || c.Has(C_SHOP))
					return "Handel to dobra rzecz. Obys dobrze sprzedal.";
				if (a.tokens.Has("nic"))
					return "Tez czasem tak mam.";
				{
					static const char* const k[] = { "Aha, rozumiem.", "No to spoko.", "Tez fajnie." };
					return PBC_SAY(g, k);
				}
			case ASK_JOIN:
			{
				// "teraz to najwyzej mozesz mi zbic konia" is a no with a joke in
				// it; asking "to jak, zapraszasz?" after it was not listening.
				const TTokens& t = a.tokens;
				const bool softNo = t.Has("najwyzej") || t.Has("raczej") || t.Has("potem") || t.Has("innym") ||
						t.Has("zajety") || t.Has("zajeta") || t.Has("sorry") || t.Has("sory") || t.Has("sorki");
				if (yes && !no && !softNo)
					return "To zapros mnie do PT!";
				if (no || softNo)
				{
					static const char* const k[] = { "No trudno, moze innym razem.", "Dobra, innym razem :)" };
					return PBC_SAY(g, k);
				}
				static const char* const k[] = { "Haha, no dobra, to innym razem :)", "Dobra, jakbys zmienil zdanie, to pisz." };
				return PBC_SAY(g, k);
			}
			case ASK_FOUND:
				if (yes || c.Has(C_POSITIVE))
					return "O, gratki!";
				return "Nastepnym razem sie uda.";
			case ASK_REAL:
			{
				// MT2009_PLUS_BOT_CHAT_V2: "a ty skad jestes?" answered.
				const std::string mine = FoldName(CityOf(g.s.name));
				for (size_t i = 0; i < a.tokens.words.size(); ++i)
				{
					const std::string& w = a.tokens.words[i];
					if (w.size() >= 4 && mine.find(w.substr(0, w.size() - 1)) != std::string::npos &&
							w != "polski" && w != "polska" && w != "jestem")
						return "No co ty, ja tez! Swiat jest maly xd";
				}
				if (a.tokens.Has("polski") || a.tokens.Has("polska") || a.tokens.Has("pl"))
				{
					static const char* const k[] = { "No to jak ja xd", "Hehe, tu chyba kazdy z polski :P" };
					return PBC_SAY(g, k);
				}
				if (a.tokens.Has("nie") && a.tokens.Has("powiem"))
					return "Hehe, tajemniczy :P";
				static const char* const k[] = { "O, kawal drogi ode mnie.", "Fajnie, nigdy tam nie bylem.",
					"Spoko, slyszalem ze ladnie tam.", "Aha, to wcale nie tak daleko." };
				return PBC_SAY(g, k);
			}
			case ASK_SUMMON:
				// "Po co mam przyjsc?" - a reason is what was asked for.
				if (SummonHasReason(a))
					return SummonGo(g);
				if (no)
					return "No to zostaje przy swoim.";
				return "Hm, to jednak zostane przy swoim.";
			case ASK_TOPIC:
			default:
			{
				const TTopicPack* pack = FindTopicPack(g.m.botAskTopic);
				if (yes && !no)
				{
					static const char* const k[] = { "No to mamy cos wspolnego.", "O, fajnie.", "Tez tak mam." };
					return PBC_SAY(g, k);
				}
				if (no && !yes)
				{
					static const char* const k[] = { "A widzisz, kazdy ma inaczej.", "Aha, rozumiem." };
					return PBC_SAY(g, k);
				}
				if (pack)
					return PickField(g, pack->react, 4);
				static const char* const k[] = { "Aha, rozumiem.", "No, jasne.", "Mhm, rozumiem." };
				return PBC_SAY(g, k);
			}
		}
	}

	inline std::string GenReaction(TGen& g, const TAnalysis& a)
	{
		// A line that needs nothing back gets nothing back, often.
		switch (a.intent)
		{
			case I_LAUGH:
			{
				// MT2009_PLUS_BOT_CHAT_V2: a laugh at the bot's own joke.
				if (g.m.lastAnswered == I_JOKE && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
				{
					static const char* const kJoke[] = { "Hehe, wiedzialem ze sie spodoba", "Mam ich wiecej xd",
						"No nie? :D", "Hehe, dawno go slyszalem" };
					return PBC_SAY(g, kJoke);
				}
				if (g.rng.Chance(g.Bad() ? 70 : 40))
					return std::string();
				static const char* const k[] = { "Hehe", "xD", "Haha", ":D" };
				return PBC_SAY(g, k);
			}
			case I_ACK:
			{
				if (g.rng.Chance(g.Bad() ? 75 : 55))
					return std::string();
				static const char* const k[] = { "No.", "Mhm.", "No wlasnie.", "Dokladnie." };
				return PBC_SAY(g, k);
			}
			case I_YES:
			{
				if (g.rng.Chance(50))
					return std::string();
				static const char* const k[] = { "No dobra.", "Ok.", "Tez tak mysle." };
				return PBC_SAY(g, k);
			}
			default:
			{
				if (g.rng.Chance(50))
					return std::string();
				static const char* const k[] = { "Aha.", "No dobra.", "Ok, rozumiem." };
				return PBC_SAY(g, k);
			}
		}
	}

	// A question nothing understood. Twice in a row it stops pretending and
	// says what it can talk about.
	inline std::string GenUnknownQuestion(TGen& g, const TAnalysis& a)
	{
		// MT2009_PLUS_BOT_CHAT_V2: short of patience, short answers.
		if (g.m.patience < 35)
		{
			static const char* const k[] = { "Nie wiem.", "Nwm.", "Nie mam pojecia." };
			return PBC_SAY(g, k);
		}
		if (g.m.fallbackStreak >= 2)
		{
			static const char* const k[] = {
				"Dobra, nie ogarniam :D Zapytaj o exp, metki, drop, ceny albo co robie. Moge tez opowiedziec kawal xd",
				"Hmm, dalej nie wiem o co ci chodzi xd Napisz prosciej?" };
			return PBC_SAY(g, k);
		}
		if (g.m.fallbackStreak >= 1)
		{
			static const char* const k[] = { "Chyba sie nie rozumiemy :) Zapytaj mnie o exp, sprzet albo mape.",
				"Nie lapie, o co chodzi. Zapytaj jakos inaczej?", "Hm? Nie bardzo wiem, o co pytasz xd" };
			return PBC_SAY(g, k);
		}
		if (a.concepts.Has(C_YOU))
		{
			static const char* const k[] = { "Hm, nie bardzo rozumiem, o co pytasz. Mozesz inaczej?", "A czemu pytasz? :)",
				"A co, ciekawy jestes? :P" };
			return PBC_SAY(g, k);
		}
		static const char* const kSteer[] = {
			"Dobre pytanie. Sam nie wiem.", "Nie wiem, nigdy sie nad tym nie zastanawialem.", "Nie mam pojecia, szczerze.",
			"A skad mam wiedziec xd", "Pierwsze slysze.", "Hmm, ciezko powiedziec." };
		std::string out = PBC_SAY(g, kSteer);
		if (g.rng.Chance(25) && g.askBack.empty())
		{
			g.askBack = "A czemu pytasz?";
			g.askBackKind = ASK_NONE;
		}
		return out;
	}

	inline std::string GenUnknownStatement(TGen& g, const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		// MT2009_PLUS_BOT_CHAT_V2: "a ja z Krakowa", "ja z polski" after the
		// bot told where it is from.
		if (g.m.lastAnswered == I_REAL_LIFE && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS &&
				(a.tokens.Has("ja") || c.Has(C_CITY)))
		{
			if (a.tokens.Has("polski") || a.tokens.Has("polska"))
			{
				static const char* const k[] = { "No to jak ja xd", "Hehe, tu chyba kazdy z polski :P" };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "O, fajnie.", "Spoko, to niedaleko.", "O, nigdy tam nie bylem." };
			return PBC_SAY(g, k);
		}
		// MT2009_PLUS_BOT_CHAT_V2: "slaby ten kawal", "suchar" after the bot's joke.
		if (IsJokeCritique(g, a))
			return GenJokeCritique(g);
		if (c.Has(C_POSITIVE))
		{
			static const char* const k[] = { "O, fajnie!", "Gratki!", "No to super." };
			return PBC_SAY(g, k);
		}
		if (c.Has(C_NEGATIVE))
		{
			static const char* const k[] = { "Oj, szkoda.", "Bywa... Nastepnym razem bedzie lepiej.", "Kiepsko." };
			return PBC_SAY(g, k);
		}
		if (a.tokens.Has("tez") && a.tokens.Has("ja"))
			return "No to tak jak ja.";
		if (c.Has(C_EXP) && c.Has(C_ME))
			return "O, to powodzenia na expie.";
		// Two lines in a row nothing understood: say what the bot is doing,
		// something the person can pick up, instead of another "aha".
		if (g.m.fallbackStreak >= 1)
		{
			std::string out = "Aha.";
			Append(out, ActivityClause(g, !g.saidMap));
			g.saidActivity = true;
			return out;
		}
		// About the bot itself: a shrug with a smile, not "ciekawe".
		if (c.Has(C_YOU))
		{
			static const char* const k[] = { "Moze troche :P", "Tak myslisz? :)", "Hehe, moze." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Aha, rozumiem.", "Mhm, jasne.", "No, rozumiem.", "Jasne." };
		std::string out = PBC_SAY(g, k);
		if (!g.Bad() && g.askBack.empty() && g.rng.Chance(g.voice == V_SOCIAL ? 40 : 20))
		{
			static const char* const kq[] = { "A co u ciebie?", "A ty co teraz robisz?" };
			g.askBack = PBC_SAY(g, kq);
			g.askBackKind = g.askBack[2] == 'c' ? ASK_HOW_ARE_YOU : ASK_ACTIVITY;
		}
		return out;
	}

	// ------------------------------------------- MT2009_PLUS_BOT_CHAT_V2

	// "opowiedz kawal", "jeszcze jeden": a joke from the bank, never the same
	// twice in a row to one person (Pick remembers what the pair heard).
	inline std::string GenJoke(TGen& g)
	{
		if (g.tier == TIER_HOSTILE)
			return "Nie mam nastroju do zartow.";
		if (g.s.dead)
			return "Lezac na ziemi to ja zartow nie opowiadam xd";
		if (g.LowHp())
			return "Chwila, najpierw sie wylecze, ledwo zyje xd";
		if (g.m.jokesTold >= 6)
		{
			g.m.jokesTold = 0;
			static const char* const k[] = { "Dobra, koniec kabaretu na dzis xd", "Skonczyly mi sie, serio :D",
				"Ej, nie jestem stand-uperem xd" };
			return PBC_SAY(g, k);
		}
		size_t n = 0;
		const char* const* jokes = JokeBank(n);
		std::string joke = Pick(g, jokes, n);
		++g.m.jokesTold;
		std::string intro;
		if (g.rng.Chance(45))
		{
			static const char* const k[] = { "Dobra, sluchaj: ", "Znasz ten? ", "Ok: ", "Hehe, mam jeden: ", "Masz: " };
			intro = Pick(g, k, sizeof(k) / sizeof(k[0]));
		}
		if (g.voice == V_GRINDER && g.rng.Chance(25))
			intro = "Szybki, bo expie: ";
		if (!intro.empty())
			LowerFirst(joke);
		if (g.askBack.empty() && g.m.jokesTold == 1 && g.rng.Chance(20))
		{
			g.askBack = "A ty znasz jakis?";
			g.askBackKind = ASK_TOPIC;
			g.askBackTopic = T_HUMOR;
		}
		return intro + joke;
	}

	// "a w real?", "z jakiego miasta jestes", "gdzie mieszkasz", "ile masz lat
	// w realu": the bot's own city and age, fixed for the bot.
	inline std::string GenRealLife(TGen& g)
	{
		const TAnalysis* a = g.a;
		if (a && a->concepts.Has(C_AGE))
			return GenAge(g);
		if (a && a->concepts.Has(C_GENDER))
		{
			if (IsGirl(g.s.name))
				return "Dziewczyna, a co? :P";
			return "Chlopak, a co?";
		}
		if (g.tier == TIER_HOSTILE)
			return "Nie twoja sprawa.";
		std::string city = CityOf(g.s.name);
		if (g.tier <= TIER_STRANGER && OpinionRoll(g, "city", 3) < 25)
		{
			static const char* const k[] = { "Hehe, takich rzeczy obcym nie mowie :P Z polski, tyle ci powiem.",
				"A co, chcesz mnie odwiedzic? xd Z polski." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "$CITY.", "Jestem $CITY.", "W realu? $CITY.", "$CITY, a co?", "Mieszkam $CITY." };
		std::string out = PBC_SAY(g, k);
		// "Mieszkam z Krakowa" is not Polish: the city as "from", said as one.
		if (out.find("Mieszkam") != std::string::npos)
			out = "Jestem $CITY.";
		ReplaceAll(out, "$CITY", city);
		CapitalizeFirst(out);
		if (g.askBack.empty() && g.rng.Chance(60))
		{
			static const char* const kq[] = { "A ty skad jestes?", "A ty skad?", "A ty z jakiego miasta?" };
			g.askBack = PBC_SAY(g, kq);
			g.askBackKind = ASK_REAL;
		}
		g.reason = "Bo tam sie urodzilem.";
		return out;
	}

	inline std::string GenGender(TGen& g)
	{
		if (g.tier <= TIER_STRANGER && OpinionRoll(g, "plec", 5) < 15)
			return "A co to zmienia? :P";
		if (IsGirl(g.s.name))
		{
			static const char* const k[] = { "Dziewczyna, a co? :P", "Dziewczyna, zdziwiony? xd", "Dziewczyna. Tak, dziewczyny tez graja :P" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Chlopak.", "Facet, a co?", "Chlopak xd A co, wygladam na dziewczyne?" };
		return PBC_SAY(g, k);
	}

	// "daj yang", "pozycz 100k", "dasz cos?": nobody gets anything for free,
	// and each voice says no its own way - grounded in what is in its purse.
	inline std::string GenBeg(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		g.reason = "Bo sam na wszystko zapracowalem.";
		if (g.m.begAsked >= 3)
		{
			static const char* const k[] = { "Nie, i nie pytaj juz.", "Juz mowilem, ze nie xd", "Zebrac to idz pod magazyn :P" };
			return PBC_SAY(g, k);
		}
		if (s.gold < 50000)
		{
			static const char* const k[] = { "Sam jestem goly, mam $GOLD xd", "Bieda u mnie, $GOLD w kieszeni.",
				"Z czego? Mam $GOLD yang :D" };
			return PBC_SAY(g, k);
		}
		switch (g.voice)
		{
			case V_MERCHANT:
			{
				static const char* const k[] = { "Za darmo to w morde mozna dostac :P Ale moge ci cos tanio sprzedac.",
					"Nic za darmo, ale ceny mam dobre xd" };
				return PBC_SAY(g, k);
			}
			case V_GRINDER:
			{
				static const char* const k[] = { "Sam sobie wyexp, ja tez musialem.", "Idz bij moby, yang sam wpadnie." };
				return PBC_SAY(g, k);
			}
			case V_FIGHTER:
			{
				static const char* const k[] = { "Wygraj ze mna na pvp, to pogadamy xd", "Zasluz sobie najpierw :P" };
				return PBC_SAY(g, k);
			}
			case V_SOCIAL:
				if (g.tier >= TIER_FRIEND)
				{
					static const char* const k[] = { "Sorki, sam mam malo, ale moge pomoc na expie.",
						"Nie mam za duzo, ale jak bede mial wiecej, to cos wymyslimy." };
					return PBC_SAY(g, k);
				}
				break;
			default:
				break;
		}
		static const char* const k[] = { "Nie mam za duzo, sorki.", "A co ja, bank? xd", "Nie rozdaje, sorki.",
			"Hehe, dobry jestes :P Nie." };
		return PBC_SAY(g, k);
	}

	// The level an advice question is about: the one it names, the asker's
	// own, or the bot's.
	inline int AdviceLevel(const TGen& g)
	{
		if (g.a && g.a->levelAsked > 0)
			return g.a->levelAsked;
		if (g.s.askerLevel > 0)
			return g.s.askerLevel;
		return g.s.level;
	}

	// "gdzie sa metki na 30?" - where the stones of a level stand.
	inline std::string GenWhereMetin(TGen& g)
	{
		const int level = AdviceLevel(g);
		std::string place;
		if (!g.world || !g.world->MetinPlaceFor(level, place) || place.empty())
			place = MetinPlaceFallback(level);
		g.reason = "Bo tam stoja metiny na ten poziom.";
		std::string out;
		if (g.a && g.a->levelAsked == 0 && g.s.askerLevel > 0)
		{
			static const char* const k[] = { "Na twoj lvl metki stoja $P.", "Na $L lvl szukaj $P." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Metki na $L stoja $P.", "Na $L lvl metiny masz $P.", "$P, tam sa metiny na $L.",
				"Szukaj $P, tam sie respia na $L." };
			out = PBC_SAY(g, k);
		}
		ReplaceAll(out, "$L", ToString((long long)level));
		ReplaceAll(out, "$P", place);
		CapitalizeFirst(out);
		if (Fighting(g) && g.s.targetStone && g.rng.Chance(50))
			Append(out, "Ja wlasnie jednego bije xd");
		else if (level > g.s.level + 15)
			Append(out, "Ja tam jeszcze nie bywam, za wysoko dla mnie.");
		return out;
	}

	// "gdzie expic na 45?" - where the bots of that level hunt.
	inline std::string GenWhereExp(TGen& g)
	{
		const int level = AdviceLevel(g);
		std::string place;
		if (!g.world || !g.world->ExpPlaceFor(level, place) || place.empty())
			place = ExpPlaceFallback(level);
		g.reason = "Bo tam sa moby na ten poziom i dobry exp.";
		static const char* const k[] = { "Na $L lvl najlepiej $P.", "$P, tam jest dobry exp na $L.", "Ja na $L expilem $P.",
			"Polecam $P, na $L akurat." };
		std::string out = PBC_SAY(g, k);
		ReplaceAll(out, "$L", ToString((long long)level));
		ReplaceAll(out, "$P", place);
		CapitalizeFirst(out);
		if (level >= g.s.level - 2 && level <= g.s.level + 2 && Fighting(g) && IsKnownMap(g.s.mapIndex))
			Append(out, Fill(g, "Sam teraz expie $MAPIN."));
		return out;
	}

	inline std::string GenChannel(TGen& g)
	{
		if (AskerOnOtherChannel(g.s))
			return Fill(g, "Na CH$CH, ty chyba jestes na innym.");
		static const char* const k[] = { "Na CH$CH.", "CH$CH.", "Jestem na CH$CH, tak jak ty." };
		if (g.s.askerChannel > 0 && g.s.askerChannel == g.s.channel)
			return PBC_SAY(g, k);
		return Fill(g, "Na CH$CH.");
	}

	// "dawaj na ch1 m1", "wbijaj do Joan", "przyjdz na m2", "spotkajmy sie":
	// the bot goes only where it already is, and says honestly why not.
	inline std::string GenMeet(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		const TAnalysis* a = g.a;
		if (g.tier == TIER_HOSTILE)
			return "Nie, dzieki.";
		if (s.summonedByAsker)
			return s.summonArrived ? "Przeciez jestem obok :)" : "Juz ide, juz!";
		const long target = MentionedMap(g);
		const int channel = a ? a->channelNamed : 0;
		if (target == MAP_ALIAS_UNLISTED)
			return "Tam to ja nawet nie bywam xd";
		const bool sameMap = target == 0 || target == s.mapIndex;
		const bool sameChannel = channel == 0 || s.channel == 0 || channel == s.channel;
		g.saidMap = true;
		if (sameMap && sameChannel && target != 0)
		{
			if (s.askerNear)
				return "Przeciez stoje obok ciebie xd";
			if (s.askerOnMap && s.summonBlock == SB_NONE && g.tier >= TIER_KNOWN)
				return SummonGo(g);
			static const char* const k[] = { "Jestem juz $MAPIN, napisz gdzie dokladnie stoisz.", "Jestem $MAPIN, gdzie cie szukac?" };
			return PBC_SAY(g, k);
		}
		g.reason = "Bo mam swoje sprawy tam, gdzie jestem.";
		if (!sameChannel && sameMap && target != 0)
			return Fill(g, "Jestem $MAPIN na CH$CH, kanalu nie zmieniam w trakcie expa, sorki.");
		if (s.shopStanding)
			return "Stoje ze straganem, nie moge odejsc.";
		if (Fighting(g))
		{
			static const char* const k[] = { "Nie moge teraz, expie $MAPIN na CH$CH.", "Teraz nie dam rady, bije sie $MAPIN.",
				"Expie teraz $MAPIN, moze pozniej." };
			return PBC_SAY(g, k);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap))
			return Fill(g, "Ide wlasnie $DEST, moze pozniej.");
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Teraz nie dam rady, jestem $MAPIN na CH$CH. Moze pozniej wpadne.",
				"Chcialbym, ale siedze $MAPIN. Zgadamy sie pozniej?" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "A po co? :P Jestem $MAPIN, mam swoje sprawy.", "Nie, siedze $MAPIN na CH$CH.",
			"Raczej nie, jestem $MAPIN i tu zostaje." };
		return PBC_SAY(g, k);
	}

	// "jak zrobic konia?", "jak sie robi biologa" - the game's own ways, as a
	// player who has done them tells them.
	inline std::string GenHowTo(TGen& g)
	{
		const TConceptSet& c = g.a ? g.a->concepts : TConceptSet();
		if (c.Has(C_BIO))
			return c.Has(C_WHERE) ? "Biolog stoi w wiosce, zerknij na mape. Misje daje od 30 lvl."
					: "U Biologa od 30 lvl: przynosisz mu rzeczy z mobow (najpierw zeby orkow), czasem odrzuca, wiec trzeba miec zapas. Nagroda to staly bonus.";
		if (c.Has(C_HORSE))
			return "Kon jest u stajennego w wiosce. Najpierw egzamin na jezdzca, potem medale konskie z Lochu Malp na kolejne poziomy.";
		if (c.Has(C_UPGRADE) || c.Has(C_GEAR) || c.Has(C_ITEMWORD))
			return "U kowala w wiosce albo zwojem blogoslawienstwa (bodzie). Z bodziem mniej ryzykujesz, ale i tak potrafi spalic xd";
		if (c.Has(C_SKILL))
			return "Do M1 wbijasz punktami, od M1 czytasz ksiegi (KU), a od G1 potrzebne kamienie duszy. Ksiegi czytasz raz na jakis czas.";
		if (c.Has(C_GOLD) || c.Has(C_MONEY))
			return "Bij metki i sprzedawaj drop na straganie. Handel na targu w wiosce robi najwiecej yang.";
		if (c.Has(C_GUILD))
			return "Gildie zaklada sie u straznika w wiosce, trzeba miec troche lvl i yang. Albo dolacz do jakiejs, latwiej.";
		if (c.Has(C_FISH))
			return "Kupujesz wedke i przynete u rybaka, stajesz przy wodzie i lowisz. Spacja jak sie zatrzesie xd";
		if (c.Has(C_MINE))
			return "Kilof do reki i szukasz zyl rudy na mapach. Ruda idzie do kowala albo na targ.";
		if (c.Has(C_DT))
			return "Do Wiezy Demonow wchodzi sie z Doliny Orkow. Bez ekipy przed 55 lvl nie ma sensu.";
		if (c.Has(C_LEVEL) || c.Has(C_EXP))
		{
			const int level = AdviceLevel(g);
			std::string place;
			if (!g.world || !g.world->ExpPlaceFor(level, place) || place.empty())
				place = ExpPlaceFallback(level);
			return "Bij moby na swoj lvl w pt, metki daja duzo expa, a pd pomaga. Na " + ToString((long long)level) +
					" najlepiej " + place + ".";
		}
		if (c.Has(C_METIN))
			return "Metiny respia sie na mapach co jakis czas. Bij te na swoj lvl, najlepiej w pt.";
		static const char* const k[] = { "Hmm, nie wiem dokladnie. Zapytaj na wolaj, ktos na pewno wie.",
			"Nie pamietam juz jak to bylo xd Zerknij w pomoc gry." };
		return PBC_SAY(g, k);
	}

	// "o ktorej event?", "jaki jest event?" - the ones running now, else the
	// calendar (F11).
	inline std::string GenEvent(TGen& g)
	{
		if (!g.s.eventsNow.empty())
		{
			static const char* const k[] = { "Teraz trwa: $EV. Reszta w kalendarzu (F11).", "Teraz jest $EV, korzystaj xd",
				"Leci $EV. Kolejne zobaczysz w kalendarzu pod F11." };
			std::string out = PBC_SAY(g, k);
			ReplaceAll(out, "$EV", g.s.eventsNow);
			return out;
		}
		static const char* const k[] = { "Teraz nic nie trwa. Zerknij w kalendarz eventow (F11).",
			"Chyba nic teraz nie ma, sprawdz pod F11 kiedy nastepny." };
		return PBC_SAY(g, k);
	}

	// "co dropi z metina?", "z czego leci kosc?" - what the game is known to drop.
	inline std::string GenDropInfo(TGen& g)
	{
		const TConceptSet& c = g.a ? g.a->concepts : TConceptSet();
		if (c.Has(C_METIN))
			return "Z metinow leca kamienie duszy, ksiegi, czasem bodzie i perly. I duzo expa xd";
		if (c.Has(C_BOSS))
			return "Z bossow skrzynie i bron, jak masz szczescie. Zawsze warto isc z ekipa.";
		if (c.Has(C_DT))
			return "W wiezy na gorze dobre skrzynie i bron, nizej glownie smieci.";
		if (c.Has(C_ITEMWORD) || c.Has(C_WHERE) || c.Has(C_WHO))
		{
			static const char* const k[] = { "Nie pamietam dokladnie, zerknij w wiki dropu w grze, tam wszystko pisze.",
				"Hmm, nwm na pewno. Wiki dropu w grze ci powie." };
			return PBC_SAY(g, k);
		}
		return "Z mobow glownie yang i smieci, czasem cos dla Biologa xd";
	}

	inline std::string GenPing(TGen& g)
	{
		static const char* const k[] = { "U mnie ping spoko, nie laguje.", "Nie, u mnie dziala dobrze.",
			"Czasem przytnie, ale ogolnie ok." };
		return PBC_SAY(g, k);
	}

	// ------------------------------------------- MT2009_PLUS_BOT_CHAT_V2: deals

	// A price as a person names one: two figures that matter.
	inline long long RoundDealPrice(long long v)
	{
		if (v <= 0)
			return 0;
		// Steps the way FormatYang says it ("13k", "1.5kk"), so the price
		// said is the price paid.
		long long step = 1;
		if (v >= 10000000) step = 1000000;
		else if (v >= 1000000) step = 100000;
		else if (v >= 1000) step = 1000;
		else if (v >= 100) step = 10;
		const long long r = (v + step / 2) / step * step;
		return r > 0 ? r : step;
	}

	// The bot's own post this line answers, when there is one.
	inline const TPublicLine* PostOf(const TGen& g)
	{
		if (!g.a || g.a->postRef <= 0 || (size_t)g.a->postRef > g.s.publicLines.size())
			return NULL;
		return &g.s.publicLines[g.a->postRef - 1];
	}

	inline std::string DealText(const TGen& g, const char* tpl)
	{
		const TDeal& d = g.m.deal;
		std::string out = Fill(g, tpl);
		ReplaceAll(out, "$ITEM", d.name);
		ReplaceAll(out, "$UNIT", FormatYang(d.offer));
		ReplaceAll(out, "$LIMIT", FormatYang(d.limit));
		ReplaceAll(out, "$ASK", FormatYang(d.ask));
		ReplaceAll(out, "$N", ToString((long long)d.count));
		ReplaceAll(out, "$MAXN", ToString((long long)d.maxCount));
		ReplaceAll(out, "$TOTAL", FormatYang(d.offer * (d.count > 0 ? d.count : 1)));
		return out;
	}

	inline std::string DealSay(TGen& g, const char* const* variants, size_t n)
	{
		return DealText(g, Pick(g, variants, n));
	}

#define PBC_DEAL(g, arr) DealSay((g), (arr), sizeof(arr) / sizeof((arr)[0]))

	// MT2009_PLUS_BOT_CHAT_V2 (deals): a bot in a dungeon, or in a party with
	// somebody else, is playing - it does not haggle and walk off for a trade
	// (the owner, 4 October: "mowi ze kupi bojowy, jest realnie na dungeonie
	// z swoim pt"). Empty when it is free to trade.
	inline std::string DealBusyAnswer(TGen& g)
	{
		if (g.s.inDungeon)
		{
			g.reason = "Bo jestem w dungeonie.";
			static const char* const k[] = { "Teraz nie, jestem na dungu z ekipa. Napisz pozniej.",
				"Siedze w dungu, nie handluje teraz. Odezwij sie jak wyjde.", "Jestem w srodku dunga, pozniej pogadamy o tym.",
				"Nie teraz, robimy dunga. Za jakis czas." };
			return PBC_SAY(g, k);
		}
		if (g.s.partyWithOtherPerson)
		{
			g.reason = "Bo gram z kims w grupie.";
			static const char* const k[] = { "Teraz gram z ekipa, nie odejde na handel. Pozniej?",
				"Sorki, jestem w pt i expimy razem. Napisz pozniej.", "Nie teraz, gram w grupie. Odezwe sie jak skonczymy." };
			return PBC_SAY(g, k);
		}
		return std::string();
	}

	// Where to come for the window when the bot cannot move: the map (with
	// the channel when it is not the asker's - Fill adds it to $MAPIN), or
	// only the channel on a map it has no name for. "na CH2 na CH2" was both
	// at once.
	inline std::string DealMeetPlace(TGen& g)
	{
		if (IsKnownMap(g.s.mapIndex))
			return Fill(g, "Jestem $MAPIN");
		if (AskerOnOtherChannel(g.s))
			return Fill(g, "Jestem na CH$CH");
		return "Jestem niedaleko";
	}

	// The meeting as it stands now: the engine's word when it has one (the
	// bot may have reached the smith since), else what was agreed.
	inline const TDealMeetPlace& CurrentDealMeet(TGen& g)
	{
		TDealMeetPlace now;
		if (g.world && g.world->DealMeet(now) && now.kind >= 0)
			g.m.deal.meet = now;
		return g.m.deal.meet;
	}

	// The exact place in words: $VIL "M1 Yongan", $SPOTTO "do kowala",
	// $SPOT "przy kowalu", $CHN "CH2" - the channel only ever in the lines
	// written for a person on the other one.
	inline std::string DealPlaceText(const TGen& g, const TDealMeetPlace& p, const char* tpl)
	{
		// Before Fill, whose $SP and $CH would eat $SPOT and $CHN.
		std::string out = tpl ? tpl : "";
		const char* village = VillageTag(p.map);
		ReplaceAll(out, "$VIL", *village ? std::string(village) : std::string(GetMapWords(p.map).name));
		ReplaceAll(out, "$SPOTTO", p.spot == DEAL_SPOT_STALL ? "do mojego straganu" : "do kowala");
		ReplaceAll(out, "$SPOT", p.spot == DEAL_SPOT_STALL ? "przy moim straganie" : "przy kowalu");
		ReplaceAll(out, "$CHN", "CH" + ToString((long long)(p.channel > 0 ? p.channel : 1)));
		return DealText(g, out.c_str());
	}

#define PBC_PLACE(g, p, arr) DealPlaceText((g), (p), Pick((g), (arr), sizeof(arr) / sizeof((arr)[0])))

	enum EDealPlaceLine
	{
		DPL_AGREED = 0,   // said with the agreement
		DPL_WHERE,        // "gdzie jestes?"
		DPL_COMING        // "czekaj ide", "juz ide", "zaraz bede"
	};

	// The meeting said: where the bot waits (or walks), for the agreement,
	// for "gdzie?" and for "juz ide". The owner, 4 October: "jestem w m1
	// yongan bede czekac przy kowalu"; a bot on the other channel says which
	// one once, and how to get there ("przelacz sie na CH2").
	inline std::string DealPlaceLine(TGen& g, int which)
	{
		const TDeal& d = g.m.deal;
		const TDealMeetPlace& p = CurrentDealMeet(g);
		const bool sells = d.side == DEAL_BOT_SELLS;
		switch (p.kind)
		{
			case DEAL_MEET_NEAR:
			{
				if (which == DPL_AGREED)
					return sells ? "Stoje obok, otwieram wymiane." : "Stoje obok, daj wymiane.";
				static const char* const k[] = { "Jestem obok ciebie, daj wymiane.", "Stoje tuz obok, daj wymiane :)",
					"Przeciez stoje obok, dawaj wymiane." };
				return PBC_SAY(g, k);
			}
			case DEAL_MEET_COMING:
			{
				if (which == DPL_AGREED)
					return sells ? "Ide do ciebie, otworze wymiane jak bede obok." : "Ide do ciebie, daj wymiane jak bede obok.";
				if (which == DPL_WHERE)
				{
					static const char* const k[] = { "Ide do ciebie, zaraz bede obok.", "Juz ide w twoja strone, stoj gdzie stoisz.",
						"Zaraz bede przy tobie, nie odchodz." };
					return PBC_SAY(g, k);
				}
				static const char* const k[] = { "Stoj, ja do ciebie ide.", "Nie ruszaj sie, zaraz bede obok.",
					"Spoko, ja ide do ciebie - stoj w miejscu." };
				return PBC_SAY(g, k);
			}
			case DEAL_MEET_AT_SPOT:
				break;
			default:
			{
				// It cannot move: the person comes to where it stands.
				if (which == DPL_AGREED)
					return DealMeetPlace(g) + ", podejdz i daj wymiane.";
				if (which == DPL_WHERE)
					return DealMeetPlace(g) + ". Podejdz i daj wymiane.";
				return "Ok, czekam. " + DealMeetPlace(g) + ".";
			}
		}
		if (which == DPL_AGREED)
		{
			if (p.otherChannel)
			{
				if (!p.arrived)
				{
					static const char* const k[] = {
						"Jestem na $CHN - ide $SPOTTO w $VIL i tam poczekam. Przelacz sie na $CHN i daj wymiane.",
						"Spotkajmy sie na $CHN w $VIL $SPOT, juz tam ide. Zmien kanal na $CHN.",
						"Gram na $CHN. Ide $SPOTTO w $VIL, bede tam czekac - przelacz sie na $CHN i podejdz." };
					return PBC_PLACE(g, p, k);
				}
				static const char* const k[] = {
					"Jestem na $CHN, w $VIL - czekam $SPOT. Przelacz sie na $CHN i daj wymiane.",
					"Czekam $SPOT w $VIL, ale na $CHN - zmien kanal na $CHN i podejdz.",
					"Stoje $SPOT w $VIL na $CHN. Przelacz sie na $CHN i daj wymiane." };
				return PBC_PLACE(g, p, k);
			}
			if (!p.arrived)
			{
				static const char* const k[] = {
					"Ide $SPOTTO w $VIL, bede za chwile. Podejdz tam i daj wymiane.",
					"Spotkajmy sie w $VIL $SPOT, juz tam ide. Daj wymiane jak mnie zobaczysz.",
					"Lece do $VIL, bede czekac $SPOT. Tam daj wymiane." };
				return PBC_PLACE(g, p, k);
			}
			static const char* const k[] = {
				"Jestem w $VIL, bede czekac $SPOT. Podejdz i daj wymiane.",
				"Stoje $SPOT w $VIL, czekam na ciebie. Daj wymiane jak podejdziesz.",
				"Czekam w $VIL $SPOT, podejdz i daj wymiane." };
			return PBC_PLACE(g, p, k);
		}
		if (which == DPL_WHERE)
		{
			if (p.otherChannel)
			{
				if (!p.arrived)
				{
					static const char* const k[] = { "Na $CHN, ide $SPOTTO w $VIL - tam poczekam.",
						"Jeszcze ide. $CHN, $VIL, $SPOT - przelacz sie na $CHN." };
					return PBC_PLACE(g, p, k);
				}
				static const char* const k[] = { "Na $CHN, w $VIL $SPOT.", "$CHN, $VIL, stoje $SPOT. Przelacz sie na $CHN.",
					"W $VIL $SPOT, ale na $CHN - zmien kanal." };
				return PBC_PLACE(g, p, k);
			}
			if (!p.arrived)
			{
				static const char* const k[] = { "Ide do $VIL, bede czekac $SPOT.", "Jeszcze ide - bede $SPOT w $VIL.",
					"W drodze, za chwile bede $SPOT w $VIL." };
				return PBC_PLACE(g, p, k);
			}
			static const char* const k[] = { "W $VIL $SPOT.", "Stoje $SPOT w $VIL.", "$VIL, $SPOT - czekam na ciebie." };
			return PBC_PLACE(g, p, k);
		}
		// "czekaj ide", "juz ide", "zaraz bede".
		if (p.otherChannel)
		{
			if (!p.arrived)
			{
				static const char* const k[] = { "Ok, ja tez juz ide - widzimy sie $SPOT w $VIL na $CHN.",
					"Spoko, za chwile bede $SPOT. Pamietaj: $CHN." };
				return PBC_PLACE(g, p, k);
			}
			static const char* const k[] = { "Ok, czekam $SPOT w $VIL na $CHN.", "Jasne, czekam. Pamietaj, ze jestem na $CHN.",
				"Spoko, stoje $SPOT w $VIL - tylko przelacz sie na $CHN." };
			return PBC_PLACE(g, p, k);
		}
		if (!p.arrived)
		{
			static const char* const k[] = { "Ok, ja tez juz ide - widzimy sie $SPOT w $VIL.", "Spoko, za chwile bede $SPOT.",
				"Jasne, spotkamy sie $SPOT w $VIL." };
			return PBC_PLACE(g, p, k);
		}
		static const char* const k[] = { "Ok, czekam $SPOT.", "Jasne, stoje tu $SPOT.", "Spoko, czekam w $VIL $SPOT.",
			"Dobra, czekam na ciebie." };
		return PBC_PLACE(g, p, k);
	}

	// Why fewer pieces than the person named (EDealCap).
	inline std::string DealCapNote(TGen& g)
	{
		switch (g.m.deal.capWhy)
		{
			case DEAL_CAP_PURSE:
			{
				static const char* const k[] = { "Mam yang tylko na $MAXN szt, wiecej nie wezme.",
					"Na wiecej niz $MAXN szt mnie teraz nie stac.", "Wiecej niz $MAXN szt nie kupie, nie mam tyle yang." };
				return PBC_DEAL(g, k);
			}
			case DEAL_CAP_HAVE:
			{
				static const char* const k[] = { "Mam tylko $MAXN szt.", "Wiecej nie mam, moge dac $MAXN szt.",
					"Mam ich $MAXN, wiecej nie dam rady." };
				return PBC_DEAL(g, k);
			}
			case DEAL_CAP_NEED:
			{
				static const char* const k[] = { "Wiecej mi nie trzeba, wezme $MAXN.", "Potrzebuje tylko $MAXN szt.",
					"$MAXN szt mi wystarczy, wiecej nie wezme." };
				return PBC_DEAL(g, k);
			}
			default:
				return std::string();
		}
	}

	// Settled: the engine holds the deal for the window and says how the two
	// meet.
	inline std::string CloseDeal(TGen& g)
	{
		{
			const std::string busy = DealBusyAnswer(g);
			if (!busy.empty())
			{
				g.m.deal = TDeal();
				return busy;
			}
		}
		TDeal& d = g.m.deal;
		d.state = DEAL_AGREED;
		d.at = g.now;
		d.meet = TDealMeetPlace();
		const int meet = g.world ? g.world->DealAgreed(d.side, d.vnum, d.skill, d.count, d.offer, d.meet) : (int)DEAL_MEET_FAILED;
		d.meet.kind = meet;
		g.reason = "Bo sie dogadalismy.";
		std::string head;
		{
			static const char* const k[] = { "Dobra, $N szt po $UNIT, razem $TOTAL.", "Stoi. $N x $ITEM po $UNIT, razem $TOTAL.",
				"Umowa: $N szt, $TOTAL za wszystko." };
			static const char* const kOne[] = { "Dobra, $ITEM za $UNIT.", "Stoi, $UNIT za $ITEM." };
			head = d.count > 1 ? PBC_DEAL(g, k) : PBC_DEAL(g, kOne);
		}
		switch (meet)
		{
			case DEAL_MEET_NEAR:
			case DEAL_MEET_COMING:
			case DEAL_MEET_COME_TO_ME:
			case DEAL_MEET_AT_SPOT:
				Append(head, DealPlaceLine(g, DPL_AGREED));
				break;
			default:
				d.state = DEAL_FAILED;
				Append(head, "Hmm, cos mi nie pasuje, sprobujmy pozniej.");
				break;
		}
		return head;
	}

	inline std::string GenDeal(TGen& g, const TAnalysis& a);

	// The person sells, the bot buys: what it pays, if it wants it at all.
	inline std::string OpenBuyDeal(TGen& g, const std::string& obj)
	{
		{
			const std::string busy = DealBusyAnswer(g);
			if (!busy.empty())
			{
				g.m.deal = TDeal();
				return busy;
			}
		}
		const TPublicLine* post = PostOf(g);
		TDealQuote q;
		const bool known = g.world && g.world->QuoteItem(obj, post ? post->vnum : 0, post ? post->skill : 0, q);
		// "sprzedam ksiege" - a skill book, but of what?
		if (q.needSkill)
		{
			g.m.deal = TDeal();
			return AskWhichBook(g, false, std::string());
		}
		if (!known || !q.found)
		{
			static const char* const k[] = { "A co to jest? Nie kojarze takiego itemu xd", "Hm, nie wiem co to, napisz pelna nazwe?" };
			return PBC_SAY(g, k);
		}
		TDeal& d = g.m.deal;
		if (!q.botWants || q.maxBuyUnit <= 0 || q.wantCount <= 0)
		{
			d = TDeal();
			g.reason = "Bo tego nie potrzebuje.";
			if (q.fair > 0)
			{
				static const char* const kFair[] = { "Tego nie kupuje, sorki. Na targu pojdzie po jakies $FAIR.",
					"Nie potrzebuje tego. Wystaw na straganie, za $FAIR ktos wezmie." };
				std::string out = PBC_SAY(g, kFair);
				ReplaceAll(out, "$FAIR", FormatYang(RoundDealPrice(q.fair)));
				return out;
			}
			static const char* const k[] = { "Nie, tego nie potrzebuje.", "Sorki, tego nie kupuje.", "Nie, dzieki. Sprobuj na @." };
			return PBC_SAY(g, k);
		}
		d = TDeal();
		d.state = DEAL_OPEN;
		d.side = DEAL_BOT_BUYS;
		d.vnum = q.vnum;
		d.skill = q.skill;
		d.name = q.name;
		d.fair = q.fair;
		d.limit = q.maxBuyUnit;
		long long start = q.fair > 0 ? q.fair * 85 / 100 : q.maxBuyUnit * 80 / 100;
		if (post && post->unitPrice > 0)
			start = post->unitPrice;
		d.offer = RoundDealPrice(std::min(start, q.maxBuyUnit));
		d.maxCount = q.wantCount;
		d.capWhy = DEAL_CAP_NEED;
		if (post && post->count > 0 && post->count < d.maxCount)
			d.maxCount = post->count;
		// A purse it does not empty for one trade.
		if (d.offer > 0 && q.botGold > 0)
		{
			const long long affordable = q.botGold * 60 / 100 / d.offer;
			if (affordable < d.maxCount)
			{
				d.maxCount = (int)affordable;
				d.capWhy = DEAL_CAP_PURSE;
			}
		}
		if (d.maxCount <= 0)
		{
			d = TDeal();
			return "Chcialbym, ale nie mam teraz tyle yang.";
		}
		if (!q.stackable)
		{
			d.maxCount = 1;
			d.capWhy = DEAL_CAP_NEED;
		}
		d.fromPost = post != NULL;
		d.at = g.now;
		d.count = g.a && g.a->dealCount > 0 ? std::min(g.a->dealCount, d.maxCount) : (q.stackable ? 0 : 1);
		g.reason = "Bo tego potrzebuje.";
		if (g.a && g.a->offerYang > 0)
			return GenDeal(g, *g.a);
		if (d.count > 0 && q.stackable)
		{
			std::string out = g.a && g.a->dealCount > d.maxCount ? DealCapNote(g) : std::string();
			static const char* const k[] = { "$N szt? Dam po $UNIT, razem $TOTAL. Pasuje?", "Wezme $N, po $UNIT za sztuke. Stoi?" };
			Append(out, PBC_DEAL(g, k));
			return out;
		}
		if (q.stackable)
		{
			static const char* const k[] = { "Tak, kupuje $ITEM. Daje $UNIT/szt, ile masz?", "Jasne, biore $ITEM po $UNIT za sztuke. Ile masz?",
				"Kupuje, $UNIT/szt. Ile sztuk masz?" };
			return PBC_DEAL(g, k);
		}
		static const char* const k[] = { "Tak, kupie $ITEM. Dam $UNIT, pasuje?", "Moge wziac $ITEM za $UNIT. Stoi?" };
		return PBC_DEAL(g, k);
	}

	// The person buys, the bot sells from its bag (its counter answers for
	// itself, ShopLineAnswer).
	inline std::string OpenSellDeal(TGen& g, const std::string& obj)
	{
		const TPublicLine* post = PostOf(g);
		TDealQuote q;
		const bool known = g.world && g.world->QuoteItem(obj, post ? post->vnum : 0, post ? post->skill : 0, q);
		if (q.needSkill)
			return AskWhichBook(g, true, q.booksHad);
		if (!known || !q.found || q.botHas <= 0 || q.sellUnit <= 0)
			return std::string();
		{
			const std::string busy = DealBusyAnswer(g);
			if (!busy.empty())
			{
				g.m.deal = TDeal();
				return busy;
			}
		}
		TDeal& d = g.m.deal;
		d = TDeal();
		d.state = DEAL_OPEN;
		d.side = DEAL_BOT_SELLS;
		d.vnum = q.vnum;
		d.skill = q.skill;
		d.name = q.name;
		d.fair = q.fair;
		d.limit = RoundDealPrice(q.minSellUnit);
		d.offer = RoundDealPrice(std::max(q.sellUnit, q.minSellUnit));
		if (post && post->unitPrice > 0)
			d.offer = RoundDealPrice(std::max(post->unitPrice, q.minSellUnit));
		d.maxCount = q.botHas;
		d.capWhy = DEAL_CAP_HAVE;
		if (!q.stackable)
			d.maxCount = 1;
		d.fromPost = post != NULL;
		d.at = g.now;
		d.count = g.a && g.a->dealCount > 0 ? std::min(g.a->dealCount, d.maxCount) : (d.maxCount == 1 ? 1 : 0);
		g.reason = "Bo mam tego za duzo.";
		if (g.a && g.a->offerYang > 0)
			return GenDeal(g, *g.a);
		if (d.count == 0)
		{
			static const char* const k[] = { "Mam $ITEM, $MAXN szt. Po $UNIT za sztuke, ile chcesz?", "Mam, $UNIT/szt. Ile ci trzeba? Mam $MAXN." };
			return PBC_DEAL(g, k);
		}
		std::string out = g.a && g.a->dealCount > d.maxCount ? DealCapNote(g) : std::string();
		static const char* const k[] = { "Mam $ITEM, za $TOTAL moge oddac. Bierzesz?", "Tak, mam. $TOTAL i jest twoje, pasuje?" };
		Append(out, PBC_DEAL(g, k));
		return out;
	}

	inline std::string GenDealTalk(TGen& g, const TAnalysis& a);

	// The talk once a deal is open: a price, a count, a yes, a no. A count
	// over what the bot takes or has is cut, and the reply says why first.
	inline std::string GenDeal(TGen& g, const TAnalysis& a)
	{
		const bool capped = g.m.deal.Live(g.now) && g.m.deal.state == DEAL_OPEN && a.dealCount > 0 &&
				g.m.deal.maxCount > 0 && a.dealCount > g.m.deal.maxCount;
		const std::string out = GenDealTalk(g, a);
		if (!capped || (g.m.deal.state != DEAL_OPEN && g.m.deal.state != DEAL_AGREED))
			return out;
		std::string note = DealCapNote(g);
		Append(note, out);
		return note;
	}

	inline std::string GenDealTalk(TGen& g, const TAnalysis& a)
	{
		TDeal& d = g.m.deal;
		if (!d.Live(g.now))
		{
			d = TDeal();
			static const char* const k[] = { "Ale o co chodzi? Co chcesz kupic albo sprzedac?", "Hm? Napisz jeszcze raz co i za ile." };
			return PBC_SAY(g, k);
		}
		d.at = g.now;
		const TTokens& t = a.tokens;
		const TConceptSet& c = a.concepts;
		if (d.state == DEAL_AGREED)
		{
			const bool channelAsked = t.Has("kanal") || t.Has("kanale") || t.Has("ch") || a.channelNamed > 0;
			if (channelAsked && !CurrentDealMeet(g).otherChannel && d.meet.kind == DEAL_MEET_AT_SPOT)
			{
				// Asked outright: the same channel is said too, once.
				static const char* const k[] = { "Na tym samym co ty, $CHN. Spotkamy sie w $VIL $SPOT.",
					"Ten sam kanal co ty ($CHN), w $VIL $SPOT." };
				return PBC_PLACE(g, d.meet, k);
			}
			if (t.Has("gdzie") || c.Has(C_WHERE) || t.Has("jestes") || channelAsked)
				return DealPlaceLine(g, DPL_WHERE);
			if (DealComingWords(t))
				return DealPlaceLine(g, DPL_COMING);
			static const char* const k[] = { "Juz sie dogadalismy - daj wymiane i po sprawie.", "Czekam na wymiane :)" };
			return PBC_SAY(g, k);
		}
		// How many.
		if (a.dealCount > 0)
		{
			d.count = a.dealCount > d.maxCount ? d.maxCount : a.dealCount;
		}
		// The price, per piece. "150k za wszystko" is a total.
		long long ask = a.offerYang;
		if (ask > 0)
		{
			const bool total = t.Has("wszystko") || t.Has("calosc") || t.Has("razem") || t.Has("lacznie") ||
					t.Has("wszystkie") || (t.Has("sumie"));
			const bool unit = t.Has("szt") || t.Has("sztuke") || t.Has("sztuka") || t.Has("po") || t.Has("kazda") ||
					t.Has("kazdy");
			if (total && d.count > 1)
				ask = ask / d.count;
			else if (!unit && d.count > 1 && d.fair > 0 && ask > d.fair * 3)
				ask = ask / d.count; // plainly the whole sum
			d.ask = ask;
		}
		const bool agree = c.Has(C_AGREE) || c.Has(C_YES) || (c.Has(C_ACK) && !c.Has(C_NO) && t.words.size() <= 3);
		const bool refuse = (c.Has(C_NO) && ask == 0) || t.Has("drogo") || (t.Has("za") && t.Has("malo")) ||
				t.Has("przesadzasz") || t.Has("zdzierstwo");
		std::string out;
		if (ask > 0)
		{
			if (d.side == DEAL_BOT_BUYS)
			{
				if (ask <= d.offer)
				{
					d.offer = ask;
					d.priceSettled = true;
				}
				else if (ask <= d.limit)
				{
					if (d.rounds < 2 && ask > d.offer + d.offer / 20)
					{
						++d.rounds;
						d.offer = RoundDealPrice(std::min(d.limit, (d.offer + ask) / 2));
						if (d.offer < ask)
						{
							static const char* const k[] = { "$ASK to troche duzo. Dam $UNIT/szt.", "Hmm, $UNIT/szt moge dac, nie wiecej.",
								"Spotkajmy sie w polowie - $UNIT za sztuke?" };
							return PBC_DEAL(g, k);
						}
						d.offer = ask;
						d.priceSettled = true;
					}
					d.offer = ask;
					d.priceSettled = true;
				}
				else if (ask <= d.limit * 13 / 10)
				{
					d.offer = d.limit;
					++d.rounds;
					static const char* const k[] = { "Za drogo. Max $LIMIT/szt, wiecej nie dam.", "$ASK? Nie, moge dac najwyzej $LIMIT." };
					return PBC_DEAL(g, k);
				}
				else
				{
					++d.rounds;
					static const char* const k[] = { "Hahaha nie, za tyle to sam kupie na targu xd", "$ASK? Chyba zartujesz. Daje $UNIT.",
						"Za tyle to nie, sorki. Moja cena to $UNIT." };
					return PBC_DEAL(g, k);
				}
			}
			else
			{
				if (ask >= d.offer)
					d.priceSettled = true;
				else if (ask >= d.limit)
				{
					if (d.rounds < 2 && ask < d.offer - d.offer / 20)
					{
						++d.rounds;
						d.offer = RoundDealPrice(std::max(d.limit, (d.offer + ask) / 2));
						if (d.offer > ask)
						{
							static const char* const k[] = { "$ASK to malo. Dla ciebie $UNIT.", "Taniej nie bardzo... $UNIT i jest twoje.",
								"Moze $UNIT? Nizej nie schodze." };
							return PBC_DEAL(g, k);
						}
						d.offer = ask;
						d.priceSettled = true;
					}
					d.offer = ask;
					d.priceSettled = true;
				}
				else if (ask >= d.limit * 7 / 10)
				{
					d.offer = d.limit;
					++d.rounds;
					static const char* const k[] = { "Za malo. Najnizej $LIMIT.", "$ASK? Nie zejde ponizej $LIMIT." };
					return PBC_DEAL(g, k);
				}
				else
				{
					++d.rounds;
					static const char* const k[] = { "Za $ASK? Hahaha nie xd", "Za darmo to ja nie rozdaje. $UNIT.",
						"Nie ma mowy, $UNIT albo nic." };
					return PBC_DEAL(g, k);
				}
			}
		}
		else if (refuse)
		{
			if (d.side == DEAL_BOT_BUYS && d.rounds < 2 && d.offer < d.limit)
			{
				++d.rounds;
				d.offer = RoundDealPrice(std::min(d.limit, d.offer * 115 / 100));
				static const char* const k[] = { "No dobra, dam $UNIT, ale wiecej nie.", "Ok, $UNIT/szt, ostatnia oferta." };
				return PBC_DEAL(g, k);
			}
			if (d.side == DEAL_BOT_SELLS && d.rounds < 2 && d.offer > d.limit)
			{
				++d.rounds;
				d.offer = RoundDealPrice(std::max(d.limit, d.offer * 90 / 100));
				static const char* const k[] = { "Dobra, $UNIT, taniej nie bede.", "No to $UNIT, ostatnia cena." };
				return PBC_DEAL(g, k);
			}
			d.state = DEAL_FAILED;
			static const char* const k[] = { "No trudno, to nie handlujemy.", "Ok, to nie. Jak zmienisz zdanie, pisz.",
				"Szkoda. Gdybys zmienil zdanie, wiesz gdzie mnie szukac." };
			return PBC_SAY(g, k);
		}
		else if (agree)
			d.priceSettled = true;
		else if (a.dealCount == 0 && t.words.size() <= 3 && DealComingWords(t))
		{
			// "czekaj", "chwila": a moment to think (or to look in the bag).
			static const char* const k[] = { "Spoko, czekam. Daj znac.", "Jasne, nie spiesz sie.", "Ok, napisz jak bedziesz wiedzial." };
			return PBC_SAY(g, k);
		}
		else if (a.dealCount == 0)
		{
			// Neither price nor count nor yes: say the offer again.
			static const char* const k[] = { "To jak? $UNIT/szt, pasuje?", "Moja oferta dalej stoi: $UNIT za sztuke." };
			return PBC_DEAL(g, k);
		}
		if (d.priceSettled && d.count > 0)
			return CloseDeal(g);
		if (d.count == 0)
		{
			static const char* const kBuy[] = { "Ok, $UNIT/szt. Ile masz? Wezme do $MAXN.", "Dobra, po $UNIT. Ile sztuk?" };
			static const char* const kSell[] = { "Ok, $UNIT/szt. Ile chcesz? Mam $MAXN.", "Dobra, po $UNIT. Ile ci dac?" };
			return d.side == DEAL_BOT_BUYS ? PBC_DEAL(g, kBuy) : PBC_DEAL(g, kSell);
		}
		static const char* const k[] = { "$N szt po $UNIT, razem $TOTAL. Pasuje?", "To $TOTAL za $N szt. Stoi?" };
		out = PBC_DEAL(g, k);
		return out;
	}

	// "jeszcze szukasz pt?", "co z tymi metkami?" - the bot's own public line.
	inline std::string GenPostRef(TGen& g, const TAnalysis& a)
	{
		const TPublicLine* post = PostOf(g);
		if (!post)
			return "A, to tak sobie pisalem na wolaj xd";
		const bool greet = a.rawIntent == I_GREETING || a.greetingToo;
		std::string out = greet ? "Siema!" : std::string();
		const std::string place = post->map ? std::string(GetMapWords(post->map).at) : std::string();
		const std::string item = post->itemName.empty() ? std::string("to") : post->itemName;
		// "siema" right after the post: the post, offered.
		if (a.rawIntent == I_GREETING && a.intent == I_POST_REF)
		{
			switch (post->kind)
			{
				case PL_BUY:
					Append(out, post->unitPrice > 0 ? "Piszesz w sprawie " + item + "? Kupuje po " + FormatYang(post->unitPrice) + "/szt."
							: "Piszesz w sprawie " + item + "? Dalej kupuje.");
					return out;
				case PL_SELL:
					Append(out, post->unitPrice > 0 ? "Chcesz kupic " + item + "? Oddam za " + FormatYang(post->unitPrice) + "."
							: "Chcesz kupic " + item + "? Dalej mam.");
					return out;
				case PL_PARTY:
					Append(out, place.empty() ? "Chodzi o pt? Dalej szukam, wbijasz?" : "Chodzi o pt? Szukam " + place + ", wbijasz?");
					return out;
				case PL_METIN:
					Append(out, "Pisalem na wolaj, bije metki, wbijasz?");
					return out;
				default:
					break;
			}
		}
		switch (post->kind)
		{
			case PL_PARTY:
				if (g.s.inParty && !g.s.leaderIsMe && !g.s.askerInParty)
					Append(out, "Juz znalazlem pt, sorki.");
				else if (g.s.askerInParty)
					Append(out, "Przeciez juz jestesmy razem :)");
				else
				{
					std::string line = place.empty() ? "Tak, dalej szukam pt. Wbijasz? Zapros mnie albo daj znac."
							: "Tak, dalej szukam pt " + place + ". Wbijasz? Zapros mnie albo daj znac.";
					Append(out, line);
				}
				break;
			case PL_METIN:
				Append(out, Fill(g, "Tak, bije metki $MAPIN. Wbijaj, razem szybciej."));
				break;
			case PL_METIN_ASK:
			case PL_EXP_ASK:
				if (a.mentionMap)
					Append(out, "O, dzieki! Zajrze tam.");
				else
				{
					std::string line = post->kind == PL_METIN_ASK ? "No, szukam metek" : "No, szukam spota na exp";
					if (post->level > 0)
						line += " na " + ToString((long long)post->level);
					line += ". Wiesz gdzie?";
					Append(out, line);
				}
				break;
			case PL_PRICE_ASK:
				if (a.offerYang > 0)
					Append(out, "Dzieki, czyli " + FormatYang(a.offerYang) + ". Dobrze wiedziec.");
				else
					Append(out, "Pytalem ile stoi " + (post->itemName.empty() ? std::string("to") : post->itemName) + ". Wiesz?");
				break;
			case PL_BOSS:
				Append(out, "Zbieramy ekipe na bossa, wbijasz?");
				break;
			case PL_SELL:
				Append(out, "Tak, dalej sprzedaje " + (post->itemName.empty() ? std::string("to") : post->itemName) + ". Chcesz kupic?");
				break;
			case PL_BUY:
				Append(out, "Tak, dalej kupuje " + (post->itemName.empty() ? std::string("to") : post->itemName) + ". Masz?");
				break;
			default:
				Append(out, "A, to tak sobie pisalem na wolaj xd");
				break;
		}
		return out;
	}

	// Whether a line names the item of a post: its words, or its alias.
	inline bool LineNamesPostItem(const TAnalysis& a, const TPublicLine& post)
	{
		if (post.itemName.empty())
			return false;
		if (!a.object.empty() && ItemNameMatches(post.itemName.c_str(), a.object))
			return true;
		const std::string folded = FoldName(post.itemName.c_str());
		std::vector<std::string> nameWords;
		SplitWords(folded, nameWords);
		for (size_t i = 0; i < a.tokens.words.size(); ++i)
		{
			const std::string& w = a.tokens.words[i];
			if (w.size() < 4)
				continue;
			for (size_t k = 0; k < nameWords.size(); ++k)
				if (nameWords[k].size() >= 4 && w.compare(0, 4, nameWords[k], 0, 4) == 0)
					return true;
		}
		return IsItemAliasWord(a.tokens.norm) && ItemNameMatches(post.itemName.c_str(), a.tokens.norm);
	}

	// A line that names an item of its own, other than the post's: "kupie ku
	// czarowane ostrze" after "S> Instr. Ognisty Duch, Instr. Czarowane
	// Ostrze" is the second book, not the first. A pointing word ("ten",
	// "to"), a count, a price or a bare "ku" names nothing of its own.
	inline bool LineNamesOtherItem(const TAnalysis& a, const TPublicLine& post)
	{
		if (a.object.empty() || IsBareBookObject(a.object))
			return false;
		std::vector<std::string> words;
		SplitWords(a.object, words);
		bool own = false;
		for (size_t i = 0; i < words.size() && !own; ++i)
		{
			const std::string& w = words[i];
			if (w.size() < 3 || (w[0] >= '0' && w[0] <= '9') || w[0] == '+' || IsFillerWord(w))
				continue;
			own = true;
		}
		return own && !LineNamesPostItem(a, post);
	}

	// The post's item as a query the world's lookups take: folded, a skill
	// book by its skill ("ku aura miecza"). The name as the post printed it
	// ("Ebonitowy Naszyjnik+4") matched no stall line - those are folded -
	// and the bot answered "tego nie mam na straganie" of its own post.
	inline std::string PostItemQuery(const TPublicLine& post)
	{
		std::string name = FoldName(post.itemName.c_str());
		if (post.skill)
		{
			if (name.compare(0, 7, "instr. ") == 0)
				name = "ku " + name.substr(7);
			else if (name.compare(0, 3, "ku ") != 0)
				name = "ku " + name;
		}
		return name;
	}

	// Before a line is answered: is it about what the bot itself said in
	// public lately? A trade post answered becomes the trade (a sale or a
	// purchase about that item); a party, a Metin or a question post the
	// post's own answer; a bare "siema" soon after a post, the post offered.
	inline void ApplyPublicContext(TGen& g, TAnalysis& b)
	{
		if (b.intent == I_DEAL || b.intent == I_LFG_ANSWER || g.m.deal.Live(g.now) || g.s.publicLines.empty())
			return;
		const TConceptSet& c = b.concepts;
		const bool tradeWords = c.Has(C_SELLYOU) || c.Has(C_BUYME) || c.Has(C_PRICEQ) || c.Has(C_STILL) ||
				c.Has(C_AGREE) || b.tokens.Has("mam") || b.tokens.Has("kupisz") || b.tokens.Has("sprzedam") ||
				b.offerYang > 0 || b.dealCount > 0;
		// A bare "ku" after a post of two books: which one is asked (GenBuy),
		// not taken to be the first.
		if (IsBareBookObject(b.object))
		{
			int books = 0;
			for (size_t i = 0; i < g.s.publicLines.size(); ++i)
				if (g.s.publicLines[i].open && g.s.publicLines[i].skill && g.s.publicLines[i].ageMin <= 60)
					++books;
			if (books > 1)
				return;
		}
		const bool replaceable = b.intent == I_SELL || b.intent == I_BUY || b.intent == I_PRICE || b.intent == I_ITEM_OWN ||
				b.intent == I_UNKNOWN_QUESTION || b.intent == I_UNKNOWN_STATEMENT || b.intent == I_GREETING ||
				b.intent == I_ACK || b.intent == I_YES || b.intent == I_FOLLOW_UP || b.intent == I_EQUIPMENT ||
				b.intent == I_INVENTORY || b.intent == I_SHOP || b.intent == I_MARKET || b.intent == I_HOW_ARE_YOU;
		for (size_t i = 0; i < g.s.publicLines.size(); ++i)
		{
			const TPublicLine& p = g.s.publicLines[i];
			if (!p.open || p.ageMin > 60 || (p.kind != PL_BUY && p.kind != PL_SELL))
				continue;
			const bool names = LineNamesPostItem(b, p);
			const bool fresh = p.ageMin <= 20 && g.m.talks <= 3;
			if (!replaceable && !names)
				continue;
			if (!(names || (tradeWords && fresh)))
				continue;
			if (!names && LineNamesOtherItem(b, p))
				continue;
			// A person selling answers a "K>"; one buying, an "S>".
			const bool personSells = c.Has(C_SELLYOU) || b.tokens.Has("mam") || b.tokens.Has("sprzedam") ||
					b.intent == I_SELL;
			const bool personBuys = c.Has(C_BUYME) || b.intent == I_BUY || b.tokens.Has("kupie") || b.tokens.Has("biore");
			if (p.kind == PL_BUY && (personSells || !personBuys))
			{
				b.intent = I_SELL;
				b.object = PostItemQuery(p);
				b.postRef = (int)i + 1;
				return;
			}
			if (p.kind == PL_SELL && (personBuys || !personSells))
			{
				b.intent = I_BUY;
				b.object = PostItemQuery(p);
				b.postRef = (int)i + 1;
				return;
			}
		}
		for (size_t i = 0; i < g.s.publicLines.size(); ++i)
		{
			const TPublicLine& p = g.s.publicLines[i];
			if (p.ageMin > 30)
				continue;
			bool refers = c.Has(C_STILL) && (b.tokens.Has("szukasz") || b.tokens.Has("pisales") || b.tokens.Has("pisalas") ||
					b.question);
			switch (p.kind)
			{
				case PL_PARTY: refers = refers || c.Has(C_PARTY) || (c.Has(C_JOIN) && b.intent != I_SUMMON); break;
				case PL_METIN: refers = refers || (c.Has(C_METIN) && (c.Has(C_JOIN) || c.Has(C_WHATWITH))); break;
				case PL_METIN_ASK: refers = refers || (c.Has(C_METIN) && (c.Has(C_WHATWITH) || b.mentionMap != 0)) ||
						(b.mentionMap != 0 && b.tokens.words.size() <= 4); break;
				case PL_EXP_ASK: refers = refers || (c.Has(C_EXP) && c.Has(C_WHATWITH)) || (b.mentionMap != 0 && b.tokens.words.size() <= 4); break;
				case PL_PRICE_ASK: refers = refers || (b.offerYang > 0 && b.tokens.words.size() <= 5); break;
				case PL_BOSS: refers = refers || c.Has(C_BOSS); break;
				default: break;
			}
			if (!refers)
				continue;
			if (p.kind == PL_SELL || p.kind == PL_BUY)
				continue; // handled above when it was a trade
			if (!replaceable && b.intent != I_PARTY && b.intent != I_PARTY_REQUEST && b.intent != I_METIN &&
					b.intent != I_WHERE_METIN && b.intent != I_WHERE_EXP && b.intent != I_ACTIVITY_LOCATION)
				continue;
			b.intent = I_POST_REF;
			b.postRef = (int)i + 1;
			return;
		}
		// "siema" from a stranger a few minutes after a post: the post offered.
		if (b.intent == I_GREETING && g.m.talks <= 2 && g.s.publicLines[0].ageMin <= 10 &&
				g.s.publicLines[0].kind != PL_TALK && g.s.publicLines[0].kind != PL_EVENT &&
				g.s.publicLines[0].kind != PL_WAR && g.s.publicLines[0].kind != PL_GEAR)
		{
			b.intent = I_POST_REF;
			b.rawIntent = I_GREETING;
			b.postRef = 1;
		}
	}

	// -------------------------------------------------------------- dispatch

	inline std::string GenerateOne(TGen& g, const TAnalysis& a)
	{
		const TAnalysis* saved = g.a;
		g.a = &a;
		std::string out;
		switch (a.intent)
		{
			case I_GREETING: out = GenGreeting(g, false); break;
			case I_FAREWELL:
				out = GenFarewell(g);
				if (ReleaseSummonFor(g))
					Append(out, "Wracam do swoich spraw.");
				break;
			case I_THANKS:
			{
				if (ReleaseSummonFor(g))
				{
					static const char* const k[] = {
						"Nie ma sprawy! To wracam do swoich spraw.", "Spoko, to ja lece do swoich spraw." };
					out = PBC_SAY(g, k);
					break;
				}
				static const char* const k[] = { "Nie ma sprawy.", "Spoko.", "Nie ma za co.", "Luz." };
				out = PBC_SAY(g, k);
				break;
			}
			case I_APOLOGY:
				// MT2009_PLUS_BOT_CHAT_V2: sorry for the bot's monsters.
				if (g.s.spotQuarrel > 0)
				{
					const int temper = TemperOf(g.s.style);
					if (temper >= 2)
					{
						static const char* const k[] = { "No ja mysle.", "Dobra, tylko zeby to byl ostatni raz." };
						out = PBC_SAY(g, k);
					}
					else if (temper == 0)
					{
						static const char* const k[] = { "Spoko, nic sie nie stalo :)", "Luz, kazdemu sie zdarza." };
						out = PBC_SAY(g, k);
					}
					else
					{
						static const char* const k[] = { "Spoko, tylko nie bij moich mobow.", "Ok, nie ma sprawy, podzielimy sie spotem." };
						out = PBC_SAY(g, k);
					}
					break;
				}
				out = g.m.negative > 0 ? "No dobra, zapomnijmy." : "Spoko, nic sie nie stalo.";
				break;
			case I_JOKE: out = GenJoke(g); break;
			case I_REAL_LIFE: out = GenRealLife(g); break;
			case I_GENDER: out = GenGender(g); break;
			case I_BEG: out = GenBeg(g); break;
			case I_WHERE_METIN: out = GenWhereMetin(g); break;
			case I_WHERE_EXP: out = GenWhereExp(g); break;
			case I_CHANNEL: out = GenChannel(g); break;
			case I_MEET: out = GenMeet(g); break;
			case I_HOWTO: out = GenHowTo(g); break;
			case I_EVENT: out = GenEvent(g); break;
			case I_DROP_INFO: out = GenDropInfo(g); break;
			case I_PING: out = GenPing(g); break;
			case I_DEAL: out = GenDeal(g, a); break;
			case I_POST_REF: out = GenPostRef(g, a); break;
			case I_LFG_ANSWER: out = GenLfgAnswer(g, a); break;
			case I_HOW_ARE_YOU: out = GenHowAreYou(g); break;
			case I_HELP: out = GenHelp(g); break;
			case I_IS_BOT: out = GenIsBot(g); break;
			case I_INSULT:
				out = GenInsult(g);
				if (ReleaseSummonFor(g))
					Append(out, "Radz sobie sam.");
				break;
			case I_PRAISE: out = GenPraise(g); break;
			case I_AGE: out = GenAge(g); break;
			case I_ORIGIN: out = GenOrigin(g); break;
			case I_KS: out = GenKs(g); break;
			case I_READY: out = GenReady(g); break;
			case I_GOODLUCK: out = GenGoodLuck(g); break;
			case I_BRB: out = GenBrb(g); break;
			case I_PRICE: out = GenPrice(g); break;
			case I_ITEMSHOP: out = GenItemShop(g); break;
			case I_NAME: out = GenName(g); break;
			case I_LEVEL: out = GenLevel(g); break;
			case I_CLASS: out = GenClass(g); break;
			case I_EMPIRE: out = GenEmpire(g); break;
			case I_PERSONALITY: out = GenPersonality(g); break;
			case I_MOOD: out = GenMood(g); break;
			case I_ACTIVITY: out = GenActivity(g); break;
			case I_ACTIVITY_LOCATION: out = GenActivityLocation(g); break;
			case I_LOCATION: out = GenLocation(g); break;
			case I_TARGET: out = GenTarget(g); break;
			case I_MOB_COUNT: out = GenMobCount(g); break;
			case I_GOAL: out = GenGoal(g); break;
			case I_NEXT_PLAN: out = GenNextPlan(g); break;
			case I_HP: out = GenHp(g); break;
			case I_GOLD: out = GenGold(g); break;
			case I_HORSE: out = GenHorse(g); break;
			case I_EQUIPMENT: out = GenEquipment(g); break;
			case I_INVENTORY: out = GenInventory(g); break;
			case I_INVENTORY_SPACE: out = GenInventorySpace(g); break;
			case I_ITEM_OWN: out = GenItemOwn(g); break;
			case I_PARTY: out = GenParty(g); break;
			case I_GUILD: out = GenGuild(g); break;
			case I_FISHING: out = GenFishing(g); break;
			case I_MINING: out = GenMining(g); break;
			case I_HERBALISM: out = GenHerbalism(g); break;
			case I_BIOLOGIST: out = GenBiologist(g); break;
			case I_METIN: out = GenMetin(g); break;
			case I_DEMON_TOWER: out = GenDemonTower(g); break;
			case I_GUILD_WAR: out = GenGuildWar(g); break;
			case I_MERCENARY: out = GenMercenary(g); break;
			case I_PARTY_REQUEST: out = GenPartyRequest(g); break;
			case I_SHOP: out = GenShop(g); break;
			case I_MARKET: out = GenMarket(g); break;
			case I_BUY: out = GenBuy(g); break;
			case I_SELL: out = GenSell(g); break;
			case I_SKILLS: out = GenSkills(g); break;
			case I_PVP: out = GenPvp(g); break;
			case I_TRAVEL: out = GenTravel(g); break;
			case I_REST: out = GenRest(g); break;
			case I_REFINE: out = GenRefine(g); break;
			case I_MISSIONS: out = GenMissions(g); break;
			case I_DEATH: out = GenDeath(g); break;
			case I_RELATIONSHIP: out = GenRelationship(g); break;
			case I_TIME_HERE: out = GenTimeHere(g); break;
			case I_MAP_OPINION: out = GenMapOpinion(g); break;
			case I_DROP_LUCK: out = GenDropLuck(g); break;
			case I_PROGRESS_TODAY: out = GenProgressToday(g); break;
			case I_BUILD: out = GenBuild(g); break;
			case I_BUFFS: out = GenBuffs(g); break;
			case I_GEAR_WHY: out = GenGearWhy(g); break;
			case I_GEAR_ADVICE: out = GenGearAdvice(g); break;
			case I_GEAR_OPINION: out = GenGearOpinion(g); break;
			case I_MAP_ADVICE: out = GenMapAdvice(g); break;
			case I_SHOW_ITEM: out = GenShowItem(g); break;
			case I_GIFT_OFFER: out = GenGiftOffer(g); break;
			case I_STOP_TALKING: out = GenStopTalking(g); break;
			case I_THREAT: out = GenThreat(g); break;
			case I_MOCK: out = GenMock(g); break;
			case I_MATH: out = GenMath(g); break;
			case I_CONTRADICTION: out = GenContradiction(g); break;
			case I_SUMMON: out = GenSummon(g); break;
			case I_DISMISS: out = GenDismiss(g); break;
			case I_FOLLOW_UP: out = GenFollowUp(g, a); break;
			case I_ANSWER_TO_BOT: out = GenAnswerToBot(g, a); break;
			case I_ACK: case I_LAUGH: case I_YES: case I_NO: out = GenReaction(g, a); break;
			case I_GENERAL: out = GenGeneral(g); break;
			case I_UNKNOWN_QUESTION: out = GenUnknownQuestion(g, a); break;
			default: out = GenUnknownStatement(g, a); break;
		}
		g.a = saved;
		return out;
	}
}

#endif
