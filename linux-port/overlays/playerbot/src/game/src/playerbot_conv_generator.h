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
//
// Every line has its English twin beside it (kEn, Txt) for a reader whose
// client reads English (Jeremus-Sama, 28 September): written as an English
// player would whisper, not word for word, and naming the game's things by
// the English game's own names (the maps, skills, classes and families of
// playerbot_conv_state.h; items and monsters as the engine side names them).

#include "playerbot_conv_general.h"

namespace playerbot_conv
{
	// ------------------------------------------------------------- helpers

	inline bool Fighting(const TGen& g) { return g.s.action == A_FIGHT || g.s.action == A_LURE; }

	inline bool HasFamily(const TGen& g)
	{
		return *MobFamilyPlural(FamilyKey(g.s)) != 0;
	}

	inline const char* GoalPhrase(const TGen& g)
	{
		if (g.en)
		{
			switch (g.s.goal)
			{
				case G_SURVIVE: return "get my strength back";
				case G_PROFESSION: return "pick my path";
				case G_EQUIPMENT: return "get better gear";
				case G_RESTOCK: return "restock my supplies";
				case G_REFINE: return "upgrade my weapon";
				case G_SKILL: return "level up my skills";
				case G_METIN: return "hunt Metins";
				case G_PARTY_CHALLENGE: return "find something tougher for the team";
				case G_BIOLOGIST: return "do the Biologist's quests";
				case G_HUNTING: return "finish my hunt";
				case G_HORSE: return "work on my horse";
				case G_FISHING: return "do some fishing";
				default: return "keep leveling up";
			}
		}
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
			static const char* const kEn[] = {
				"Gotta head back to town soon, my bag's full.",
				"My bag's almost full though, gonna have to sell soon." };
			return PBC_SAY2(g, k, kEn);
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
				static const char* const kEn[] = { "Just grinding to the next level.", "The exp won't farm itself." };
				return PBC_SAY2(g, k, kEn);
			}
			case V_MERCHANT:
			{
				static const char* const k[] = { "Moze cos cennego wypadnie na sprzedaz.", "Drop pojdzie potem na targ." };
				static const char* const kEn[] = { "Maybe something worth selling drops.", "The drops go to the market later." };
				return PBC_SAY2(g, k, kEn);
			}
			case V_FIGHTER:
				if (!g.s.targetStone)
				{
					static const char* const k[] = { "Szkoda, ze nie ma tu Metinow.", "Moglyby byc mocniejsze." };
					static const char* const kEn[] = { "Too bad there are no Metins here.", "Could be tougher, honestly." };
					return PBC_SAY2(g, k, kEn);
				}
				return std::string();
			case V_SOCIAL:
				if (!g.s.inParty)
				{
					static const char* const k[] = { "Samemu troche nudno.", "Przydalby sie ktos do towarzystwa." };
					static const char* const kEn[] = { "Kinda boring on my own.", "Could use some company." };
					return PBC_SAY2(g, k, kEn);
				}
				return std::string();
			default:
			{
				static const char* const k[] = { "Ladna okolica swoja droga.", "Spokojnie tu." };
				static const char* const kEn[] = { "Nice area, by the way.", "It's quiet here." };
				return PBC_SAY2(g, k, kEn);
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
			static const char* const kEn[] = { "Just died, waiting to get back up.", "Lying on the ground, something killed me." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.afk)
		{
			static const char* const k[] = { "Mam chwile przerwy.", "Robie sobie krotka przerwe." };
			static const char* const kEn[] = { "Taking a short break.", "On a quick break." };
			return PBC_SAY2(g, k, kEn);
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
					static const char* const kEn[] = {
						"Fighting, but my HP is low. Gonna need a rest soon.",
						"In a fight, barely standing. About to pull back." };
					return PBC_SAY2(g, k, kEn);
				}
				if (s.targetStone)
				{
					static const char* const kMap[] = { "Rozbijam Metina $MAPIN.", "Bije Metina $MAPINSHORT, zaraz padnie." };
					static const char* const kNo[] = { "Rozbijam Metina.", "Bije Metina, zaraz padnie." };
					static const char* const kMapEn[] = { "Breaking a Metin $MAPIN.", "Hitting a Metin $MAPINSHORT, it's almost down." };
					static const char* const kNoEn[] = { "Breaking a Metin.", "Hitting a Metin, it's almost down." };
					return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
				}
				if (s.targetBoss)
				{
					static const char* const k[] = { "Bije bossa, trzymaj kciuki!", "Walcze z bossem, zaraz pogadamy." };
					static const char* const kEn[] = { "Fighting a boss, wish me luck!", "Busy with a boss, talk in a sec." };
					return PBC_SAY2(g, k, kEn);
				}
				if (s.targetPlayer)
				{
					static const char* const k[] = { "Bije sie z kims, chwila.", "Mam pojedynek, zaraz." };
					static const char* const kEn[] = { "Fighting someone, one sec.", "In a duel, hold on." };
					return PBC_SAY2(g, k, kEn);
				}
				if (HasFamily(g))
				{
					static const char* const kMap[] = { "Bije $FAMILY $MAPIN.", "Expie $MAPIN, leca $FAMILY.", "Wlasnie ubijam $FAMILY $MAPINSHORT." };
					static const char* const kNo[] = { "Bije $FAMILY.", "Wlasnie ubijam $FAMILY.", "Leca $FAMILY, expie." };
					static const char* const kMapEn[] = { "Killing $FAMILY $MAPIN.", "Hunting $MAPIN, $FAMILY everywhere.",
						"Just slaying $FAMILY $MAPINSHORT." };
					static const char* const kNoEn[] = { "Killing $FAMILY.", "Just slaying $FAMILY.", "Farming $FAMILY." };
					return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
				}
				static const char* const kMap[] = { "Wlasnie expie $MAPIN.", "Bije moby $MAPIN.", "Expie sobie $MAPINSHORT." };
				static const char* const kNo[] = { "Wlasnie bije moby.", "Expie.", "Bije, co popadnie." };
				static const char* const kMapEn[] = { "Hunting $MAPIN right now.", "Killing mobs $MAPIN.", "Just grinding $MAPINSHORT." };
				static const char* const kNoEn[] = { "Killing mobs right now.", "Grinding.", "Hitting whatever shows up." };
				return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
			}
			case A_TRAVEL:
				if (IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
				{
					if (s.riding)
					{
						static const char* const k[] = { "Jade na koniu $DEST.", "Jestem w drodze $DEST, na koniu." };
						static const char* const kEn[] = { "Riding $DEST.", "On my way $DEST, on horseback." };
						return PBC_SAY2(g, k, kEn);
					}
					static const char* const k[] = { "Jestem w drodze $DEST.", "Ide $DEST.", "Zmierzam $DEST." };
					static const char* const kEn[] = { "On my way $DEST.", "Heading $DEST.", "Going $DEST." };
					return PBC_SAY2(g, k, kEn);
				}
				else
				{
					static const char* const kMap[] = { "Ide na inny spot $MAPIN.", "Przemieszczam sie $MAPINSHORT." };
					static const char* const kNo[] = { "Ide na inny spot.", "Jestem w drodze." };
					static const char* const kMapEn[] = { "Moving to another spot $MAPIN.", "Moving around $MAPINSHORT." };
					static const char* const kNoEn[] = { "Moving to another spot.", "On the move." };
					return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
				}
			case A_LOOT:
			{
				static const char* const k[] = { "Zbieram drop po walce.", "Podnosze, co wypadlo." };
				static const char* const kEn[] = { "Picking up the loot.", "Grabbing what dropped." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_RECOVER:
			{
				static const char* const k[] = { "Odpoczywam chwile, zbieram HP.", "Siadlem na chwile, regeneruje sie." };
				static const char* const kEn[] = { "Resting a bit, getting my HP back.", "Sat down for a sec to regen." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_TOWN_REST:
			{
				static const char* const k[] = { "Siedze w miescie i odpoczywam.", "Odpoczywam $MAPIN." };
				static const char* const kEn[] = { "Chilling in town.", "Resting $MAPIN." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_TRAIN:
			{
				static const char* const k[] = { "Zalatwiam sprawy u trenera.", "Ogarniam profesje." };
				static const char* const kEn[] = { "At the skill trainer.", "Sorting out my path." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_SHOP:
			{
				static const char* const k[] = { "Robie zakupy u handlarza.", "Kupuje potki i takie tam." };
				static const char* const kEn[] = { "Shopping at the merchant.", "Buying pots and stuff." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_REFINE:
			{
				static const char* const k[] = { "Ulepszam sprzet u kowala. Trzymaj kciuki.", "Stoje u kowala, ulepszam bron." };
				static const char* const kEn[] = { "Upgrading my gear at the Blacksmith. Fingers crossed.",
					"At the Blacksmith, upgrading my weapon." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_READ_BOOK:
			{
				static const char* const k[] = { "Czytam ksiegi umiejetnosci.", "Ucze sie z ksiag." };
				static const char* const kEn[] = { "Reading skill books.", "Studying my books." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_SOCKET:
			{
				static const char* const k[] = { "Wkladam kamienie duszy do broni.", "Bawie sie kamieniami duszy." };
				static const char* const kEn[] = { "Putting stones into my weapon.", "Messing around with my stones." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_PARTY_ASSEMBLE:
			{
				static const char* const k[] = { "Zbieram ekipe na cos wiekszego.", "Czekam, az sie ekipa zbierze." };
				static const char* const kEn[] = { "Getting a team together for something big.", "Waiting for the team to gather." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_BIOLOGIST:
				// An item's name stands after a colon: "zbieram Ksiega Klatw"
				// is a case no Pole would use.
				if (!s.bioWanted.empty())
				{
					static const char* const k[] = { "Zbieram dla Biologa: $BIO.", "Robie zadanie Biologa. Brakuje mi jeszcze: $BIO." };
					static const char* const kEn[] = { "Collecting for the Biologist: $BIO.",
						"Doing the Biologist's quest. Still need: $BIO." };
					return PBC_SAY2(g, k, kEn);
				}
				else
				{
					static const char* const k[] = { "Robie zadanie dla Biologa.", "Zalatwiam sprawy u Biologa." };
					static const char* const kEn[] = { "Doing a quest for the Biologist.", "Running an errand for the Biologist." };
					return PBC_SAY2(g, k, kEn);
				}
			case A_STABLE:
			{
				static const char* const k[] = { "Jestem u Stajennego, ogarniam konia.", "Zajmuje sie koniem." };
				static const char* const kEn[] = { "At the Stable Boy, sorting out my horse.", "Working on my horse." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_STALL:
			{
				static const char* const kMap[] = { "Stoje ze straganem $MAPIN.", "Handluje, mam stragan $MAPIN." };
				static const char* const kNo[] = { "Stoje ze straganem.", "Handluje przy straganie." };
				static const char* const kMapEn[] = { "Running my shop $MAPIN.", "Trading, got my shop up $MAPIN." };
				static const char* const kNoEn[] = { "Running my shop.", "Selling stuff at my shop." };
				return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
			}
			case A_FISHING:
			{
				static const char* const kMap[] = { "Lowie ryby $MAPIN.", "Siedze z wedka $MAPINSHORT." };
				static const char* const kNo[] = { "Lowie ryby.", "Siedze z wedka." };
				static const char* const kMapEn[] = { "Fishing $MAPIN.", "Sitting with my rod $MAPINSHORT." };
				static const char* const kNoEn[] = { "Fishing.", "Sitting with my rod." };
				return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
			}
			case A_MARKET:
			{
				static const char* const k[] = { "Chodze po targu i szukam okazji.", "Robie zakupy na targu." };
				static const char* const kEn[] = { "Walking the market, looking for deals.", "Shopping at the market." };
				return PBC_SAY2(g, k, kEn);
			}
			case A_LURE:
				if (s.luringForAsker)
				{
					static const char* const k[] = { "Przyciagam dla ciebie moby, stoj w miejscu.", "Zbieram ci paczke mobow." };
					static const char* const kEn[] = { "Pulling mobs for you, stay put.", "Gathering a pack of mobs for you." };
					return PBC_SAY2(g, k, kEn);
				}
				else
				{
					static const char* const k[] = { "Podciagam moby dla ekipy.", "Luruje moby dla grupy." };
					static const char* const kEn[] = { "Pulling mobs for the team.", "Luring mobs for the group." };
					return PBC_SAY2(g, k, kEn);
				}
			case A_MINING:
			{
				static const char* const kMap[] = { "Kopie rude $MAPIN.", "Siedze w kopalni, kopie." };
				static const char* const kNo[] = { "Kopie rude.", "Kopie, ile sie da." };
				static const char* const kMapEn[] = { "Mining ore $MAPIN.", "Down in the mine, digging." };
				static const char* const kNoEn[] = { "Mining ore.", "Digging as much as I can." };
				return map ? PBC_SAY2(g, kMap, kMapEn) : PBC_SAY2(g, kNo, kNoEn);
			}
			default:
				if (s.inTown)
				{
					static const char* const k[] = { "Krece sie po miescie.", "Nic specjalnego, stoje $MAPIN." };
					static const char* const kEn[] = { "Hanging around town.", "Nothing special, just standing $MAPIN." };
					return PBC_SAY2(g, k, kEn);
				}
				else
				{
					static const char* const k[] = { "Rozgladam sie, co tu porobic.", "Nic konkretnego, zastanawiam sie, co dalej." };
					static const char* const kEn[] = { "Looking around for something to do.", "Nothing much, figuring out what's next." };
					return PBC_SAY2(g, k, kEn);
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
		ReplaceAll(out, "$WASAT", MapWordsFor(g, old).at);
		return out;
	}

	inline std::string GenActivity(TGen& g)
	{
		const bool yesNoExp = g.a && g.a->concepts.Has(C_EXP) && !g.a->concepts.Has(C_WHAT);
		std::string out;
		if (yesNoExp)
		{
			if (Fighting(g))
				out = g.rng.Chance(50) ? Txt(g, "Tak. ", "Yep. ") : Txt(g, "No, expie. ", "Yeah, hunting. ");
			else
				out = Txt(g, "Teraz nie. ", "Not right now. ");
		}
		// It named another map a moment ago: the move is part of the answer.
		if (const long old = RecentOtherMap(g))
			out += WithOldMap(g, Txt(g, "Juz nie jestem $WASAT. ", "Not $WASAT anymore. "), old);
		out += ActivityClause(g, !g.saidMap);
		if (!g.saidMap && IsKnownMap(g.s.mapIndex) && out.find(MapWordsFor(g, g.s.mapIndex).atShort) != std::string::npos)
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
			static const char* const kWhyEn[V_COUNT] = {
				"Because the exp here is good for my level.", "Just ended up here, and it's quiet.",
				"Because these mobs drop stuff that sells well.", "Because I like a fight, and there's plenty here.",
				"Because there's always someone around here." };
			g.reason = Txt(g, kWhy[g.voice], kWhyEn[g.voice]);
		}
		else if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
			g.reason = Txt(g, "Bo mialem juz malo HP.", "Because my HP was low.");
		else if (g.s.action == A_TRAVEL)
			g.reason = Txt(g, "Bo tam mam lepszy spot na moj poziom.", "Because there's a better spot for my level there.");
		else if (g.s.action == A_SHOP || g.s.action == A_MARKET)
			g.reason = Txt(g, "Bo potrzebuje zapasow.", "Because I need supplies.");
		// A question back, now and then.
		if (!g.Merged() && !g.Bad() && g.askBack.empty() && g.tier >= TIER_KNOWN && g.rng.Chance(g.voice == V_SOCIAL ? 45 : 20))
		{
			static const char* const k[] = { "A ty co robisz?", "A ty co porabiasz?" };
			static const char* const kEn[] = { "What about you, what are you doing?", "And you, what are you up to?" };
			g.askBack = PBC_SAY2(g, k, kEn);
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
			static const char* const kEn[] = { "Yeah, I'm $MAPIN.", "Yep, $MAPIN." };
			std::string out = PBC_SAY2(g, k, kEn);
			CapitalizeFirst(out);
			return out;
		}
		if (!IsKnownMap(s.mapIndex))
			return Txt(g, "Nie, jestem gdzie indziej.", "Nope, I'm somewhere else.");
		// "jestes w joan?" right after it said it was, and a teleport since.
		if (mentioned == RecentOtherMap(g) && mentioned != 0)
			return WithOldMap(g, Txt(g, "Juz nie - bylem $WASAT, a teraz jestem $MAPIN.",
					"Not anymore - I was $WASAT, now I'm $MAPIN."), mentioned);
		static const char* const k[] = { "Nie, jestem $MAPIN.", "Nie, teraz $MAPIN." };
		static const char* const kEn[] = { "Nope, I'm $MAPIN.", "No, I'm $MAPIN now." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenLocation(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (const long mentioned = MentionedMap(g))
			return MapYesNo(g, mentioned);
		if (!IsKnownMap(s.mapIndex))
		{
			static const char* const k[] = { "Gdzies na uboczu, nawet nie wiem, jak to miejsce sie nazywa.", "W jakims dziwnym miejscu, nie znam nazwy." };
			static const char* const kEn[] = { "Somewhere off the beaten path, don't even know what this place is called.",
				"Some weird place, no idea what it's called." };
			return PBC_SAY2(g, k, kEn);
		}
		g.saidMap = true;
		if (const long old = RecentOtherMap(g))
		{
			static const char* const k[] = { "Bylem $WASAT, ale juz jestem $MAPIN.", "Juz nie $WASAT - teraz jestem $MAPIN." };
			static const char* const kEn[] = { "I was $WASAT, but now I'm $MAPIN.", "Not $WASAT anymore - I'm $MAPIN now." };
			return WithOldMap(g, PBC_PICK2(g, k, kEn), old);
		}
		if (s.askerNear)
		{
			static const char* const k[] = { "Tuz obok ciebie :)", "Przeciez stoje niedaleko ciebie, $MAPIN." };
			static const char* const kEn[] = { "Right next to you :)", "I'm standing right near you, $MAPIN." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
		{
			static const char* const k[] = { "Jestem $MAPIN, ale ide $DEST.", "Teraz $MAPIN, zmierzam $DEST." };
			static const char* const kEn[] = { "I'm $MAPIN, but heading $DEST.", "$MAPIN for now, on my way $DEST." };
			return PBC_SAY2(g, k, kEn);
		}
		if (g.saidActivity)
		{
			static const char* const k[] = { "Jestem $MAPIN.", "A jestem $MAPIN." };
			static const char* const kEn[] = { "I'm $MAPIN.", "Oh, and I'm $MAPIN." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Jestem teraz $MAPIN.", "Siedze $MAPIN.", "Aktualnie $MAPIN.", "Jestem $MAPINSHORT." };
		static const char* const kEn[] = { "I'm $MAPIN right now.", "Hanging out $MAPIN.", "Currently $MAPIN.", "I'm $MAPINSHORT." };
		return PBC_SAY2(g, k, kEn);
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
			return WithOldMap(g, Fighting(g) ? Txt(g, "Juz nie $WASAT, teraz expie $MAPIN.", "Not $WASAT anymore, hunting $MAPIN now.") :
					Txt(g, "Juz nie $WASAT, teraz jestem $MAPIN.", "Not $WASAT anymore, I'm $MAPIN now."), old);
		}
		g.saidMap = true;
		// The last whisper already named the map: say so, do not recite it.
		if (!g.Merged() && g.m.lastReply.find(MapWordsFor(g, s.mapIndex).atShort) != std::string::npos &&
				g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
		{
			static const char* const k[] = { "No mowie, $MAPIN :)", "Tutaj, $MAPIN.", "$MAPIN, tak jak pisalem." };
			static const char* const kEn[] = { "Like I said, $MAPIN :)", "Right here, $MAPIN.", "$MAPIN, like I wrote." };
			return PBC_SAY2(g, k, kEn);
		}
		if (Fighting(g))
		{
			if (g.saidActivity)
			{
				static const char* const k[] = { "$MAPIN.", "Tutaj, $MAPIN." };
				static const char* const kEn[] = { "$MAPIN.", "Right here, $MAPIN." };
				return PBC_SAY2(g, k, kEn);
			}
			if (HasFamily(g))
			{
				static const char* const k[] = { "$MAPIN, bije $FAMILY.", "Expie $MAPIN.", "$MAPIN, tu jest niezly spot na $FAMILY." };
				static const char* const kEn[] = { "$MAPIN, killing $FAMILY.", "Hunting $MAPIN.", "$MAPIN, nice spot for $FAMILY here." };
				return PBC_SAY2(g, k, kEn);
			}
			static const char* const k[] = { "Expie $MAPIN.", "$MAPIN, tu jest dobry spot.", "Teraz $MAPIN." };
			static const char* const kEn[] = { "Hunting $MAPIN.", "$MAPIN, good spot here.", "$MAPIN right now." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap))
		{
			static const char* const k[] = { "Teraz nigdzie, dopiero ide $DEST.", "Ide wlasnie $DEST, tam bede expil." };
			static const char* const kEn[] = { "Nowhere yet, still on my way $DEST.", "Heading $DEST, gonna hunt there." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.inTown)
		{
			static const char* const k[] = { "Teraz nigdzie, jestem $MAPIN. Potem pewnie wroce na exp.", "Na razie nie expie, stoje $MAPIN." };
			static const char* const kEn[] = { "Nowhere right now, I'm $MAPIN. Probably back to hunting later.",
				"Not hunting at the moment, just standing $MAPIN." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Teraz nie expie, ale jestem $MAPIN.", "Na razie przerwa, jestem $MAPIN." };
		static const char* const kEn[] = { "Not hunting right now, but I'm $MAPIN.", "Taking a break for now, I'm $MAPIN." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenMobCount(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.inTown && !Fighting(g))
		{
			static const char* const k[] = { "W miescie? Tu nie ma mobow :)", "Tu sa sami gracze i handlarze, zadnych mobow." };
			static const char* const kEn[] = { "In town? There are no mobs here :)", "Just players and merchants here, no mobs." };
			return PBC_SAY2(g, k, kEn);
		}
		const int n = s.mobsNear;
		const bool askedFew = g.a && g.a->polarityNegative;
		std::string out;
		if (n < 0)
		{
			static const char* const k[] = { "Troche jest.", "Da sie expic." };
			static const char* const kEn[] = { "There are some.", "Enough to hunt." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (n == 0)
		{
			static const char* const k[] = { "Pusto teraz, wszystko wybite.", "Nic nie ma, czekam na respawn." };
			static const char* const kEn[] = { "Empty right now, all cleared out.", "Nothing here, waiting for the respawn." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (n <= 3)
		{
			static const char* const k[] = { "Malo. Ktos tu chyba przede mna czyscil.", "Kilka sztuk, nic wielkiego." };
			static const char* const kEn[] = { "Not many. I think someone cleared it before me.", "A few, nothing big." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (n <= 9)
		{
			static const char* const k[] = { "Troche jest, ale spot jest spokojny.", "Troche ich jest, w sam raz.", "Jest co bic, ale bez szalu." };
			static const char* const kEn[] = { "Some, but it's a chill spot.", "A fair few, just right.", "Enough to hit, nothing crazy." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (n <= 19)
		{
			static const char* const kFew[] = { "Wcale nie malo, sporo ich.", "Nie, jest ich calkiem sporo." };
			static const char* const k[] = { "Sporo, jest co bic.", "Sporo ich, exp leci." };
			static const char* const kFewEn[] = { "Not few at all, there's a lot.", "Nah, there's quite a lot of them." };
			static const char* const kEn[] = { "Quite a lot, plenty to hit.", "Lots of them, the exp is flowing." };
			out = askedFew ? PBC_SAY2(g, kFew, kFewEn) : PBC_SAY2(g, k, kEn);
		}
		else
		{
			static const char* const k[] = { "Duzo! Ledwo nadazam.", "Mnostwo, az sie roi." };
			static const char* const kEn[] = { "Loads! Can barely keep up.", "Tons of them, it's swarming." };
			out = PBC_SAY2(g, k, kEn);
		}
		if (s.playersNear >= 3 && g.rng.Chance(60))
			Append(out, Txt(g, "Tylko tloczno troche, sporo ludzi.", "Bit crowded though, lots of people."));
		g.reason = n > 9 ? Txt(g, "Bo szybko sie respia.", "Because they respawn fast.") :
				Txt(g, "Bo ktos tu pewnie wczesniej byl.", "Because someone was probably here before me.");
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
				static const char* const kEn[] = { "Nothing in particular, looking for something to hit.",
					"Nothing right now, looking around." };
				return PBC_SAY2(g, k, kEn);
			}
			static const char* const k[] = { "Teraz nic nie bije.", "Nic, mam przerwe od walki." };
			static const char* const kEn[] = { "Not hitting anything right now.", "Nothing, taking a break from fighting." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.targetStone)
		{
			static const char* const k[] = { "Metina! Zaraz go rozwale.", "Kamien Metina, zaraz padnie." };
			static const char* const kEn[] = { "A Metin! Gonna smash it.", "A Metin stone, it's almost down." };
			return PBC_SAY2(g, k, kEn);
		}
		if (HasFamily(g))
		{
			static const char* const k[] = { "Bije $FAMILY. Teraz akurat $TARGET.", "$FAMILY, glownie.", "Teraz $TARGET." };
			static const char* const kEn[] = { "Killing $FAMILY. Right now it's $TARGET.", "$FAMILY, mostly.", "$TARGET right now." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Teraz na celowniku: $TARGET.", "Aktualnie $TARGET.", "Bije sie z tym tu, $TARGET." };
		static const char* const kEn[] = { "In my sights: $TARGET.", "Currently $TARGET.", "Fighting this one here, $TARGET." };
		return PBC_SAY2(g, k, kEn);
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
		g.reason = Txt(g, "Bo tyle wyexpilem.", "That's how far I've leveled.");
		if (again)
		{
			static const char* const k[] = { "Dalej $LVL :)", "$LVL, nic sie nie zmienilo.", "Wciaz $LVL." };
			static const char* const kEn[] = { "Still $LVL :)", "$LVL, nothing's changed.", "Still level $LVL." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.expPct >= 85)
		{
			static const char* const k[] = { "Mam $LVL, zaraz wbijam $NEXTLVL.", "$LVL, ale juz prawie $NEXTLVL." };
			static const char* const kEn[] = { "Level $LVL, about to hit $NEXTLVL.", "$LVL, but almost $NEXTLVL." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (s.expPct >= 60 && g.voice == V_GRINDER)
			out = Fill(g, Txt(g, "Mam $LVL. Jeszcze troche i powinienem wbic kolejny poziom.",
					"Level $LVL. A bit more and I should level up."));
		else
		{
			static const char* const k[] = { "Mam $LVL poziom.", "$LVL lvl.", "Mam $LVL. Powoli do przodu.", "$LVL, na razie." };
			static const char* const kEn[] = { "I'm level $LVL.", "$LVL lvl.", "Level $LVL. Slowly getting there.", "$LVL, for now." };
			out = PBC_SAY2(g, k, kEn);
		}
		if (s.askerLevel > 0 && g.rng.Chance(35))
		{
			if (s.askerLevel >= s.level + 10)
				Append(out, Txt(g, "Ty mnie juz troche przegoniles.", "You're a bit ahead of me already."));
			else if (s.level >= s.askerLevel + 10)
				Append(out, Txt(g, "Troche wyzej od ciebie.", "A bit higher than you."));
			else if (s.askerLevel >= s.level - 3 && s.askerLevel <= s.level + 3)
				Append(out, Txt(g, "Mamy podobnie.", "We're about the same."));
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

	// The same in English: the players' word, and the English client's own
	// group name (BuildGameNameEn) in brackets where the two differ.
	inline const char* BuildPhraseEn(int build)
	{
		switch (build)
		{
			case B_BODY: return "a body warrior";
			case B_MENTAL: return "a mental warrior";
			case B_DAGGER: return "a dagger ninja (Blade)";
			case B_ARCHER: return "an archer ninja (Arc)";
			case B_WEAPON: return "a WP sura (Weapon)";
			case B_BLACK_MAGIC: return "a BM sura (Magic)";
			case B_DRAGON: return "a dragon shaman";
			case B_HEAL: return "a heal shaman (Healing)";
			default: return "";
		}
	}

	inline const char* BuildPhraseIn(bool en, int build)
	{
		return en ? BuildPhraseEn(build) : BuildPhrase(build);
	}

	// The path's skills, best first - the build's own first skill ahead of an
	// equal one - as "Aura Miecza M3, Wir Miecza 17". Empty before any is
	// learnt.
	inline std::string SkillsList(const TBotSnapshot& s, size_t maxItems, bool en = false)
	{
		int order[6];
		int n = 0;
		for (int i = 0; i < 6; ++i)
		{
			if (s.skillVnums[i] == 0 || s.skillLevels[i] <= 0 || !*SkillNameIn(en, s.skillVnums[i]))
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
			out += SkillNameIn(en, s.skillVnums[order[k]]);
			out += ' ';
			out += SkillGradeText(s.skillLevels[order[k]]);
		}
		return out;
	}

	inline std::string GenClass(TGen& g)
	{
		static const char* const k[] = { "Gram $CLASSI.", "Jestem $CLASSI.", "$CLASSI, od poczatku." };
		static const char* const kEn[] = { "I play $CLASSI.", "I'm $CLASSI.", "$CLASSI, from day one." };
		std::string out = PBC_PICK2(g, k, kEn);
		const int build = g.s.Build();
		ReplaceAll(out, "$CLASSI", build != B_NONE ? BuildPhraseIn(g.en, build) :
				(g.en ? ClassNameInstrEn(g.s.job) : ClassNameInstr(g.s.job)));
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
			g.reason = Txt(g, "Bo sciezke wybiera sie u trenera od piatego poziomu.",
					"Because you pick your path at the trainer from level five.");
			if (s.level < 5)
				return Fill(g, Txt(g, "Jeszcze nie mam sciezki, mam dopiero $LVL poziom. Wybiera sie ja od piatego.",
						"No path yet, I'm only level $LVL. You pick one from level five."));
			return Txt(g, "Jeszcze nie wybralem sciezki, musze isc do trenera.", "Haven't picked my path yet, need to visit the trainer.");
		}
		const unsigned int named = g.a ? NamedBuildsInLine(g.a->tokens, g.a->concepts) : 0;
		const unsigned int mine = 1u << build;
		std::string out;
		if (named != 0 && (named & mine) == 0)
			out = std::string(Txt(g, "Nie, gram ", "Nope, I'm ")) + BuildPhraseIn(g.en, build) + ".";
		else if (named == mine)
		{
			static const char* const k[] = { "Tak, gram $P.", "Zgadza sie, jestem $P.", "Tak, jestem $P." };
			static const char* const kEn[] = { "Yep, I play $P.", "That's right, I'm $P.", "Yeah, I'm $P." };
			out = PBC_PICK2(g, k, kEn);
			ReplaceAll(out, "$P", BuildPhraseIn(g.en, build));
		}
		else
		{
			static const char* const k[] = { "Gram $P.", "Jestem $P.", "Gram $P, tak wybralem u trenera." };
			static const char* const kEn[] = { "I play $P.", "I'm $P.", "I play $P, picked it at the trainer." };
			out = PBC_PICK2(g, k, kEn);
			ReplaceAll(out, "$P", BuildPhraseIn(g.en, build));
		}
		CapitalizeFirst(out);
		std::string tail = Fill(g, Txt(g, "Mam $LVL poziom", "Level $LVL"));
		const std::string best = SkillsList(s, 1, g.en);
		if (!best.empty())
			tail += Txt(g, ", najwyzej ", ", my best is ") + best;
		Append(out, tail + ".");
		g.reason = Txt(g, "Tak wybralem u trenera i tak juz zostalo.", "That's what I picked at the trainer, and it stuck.");
		return out;
	}

	inline std::string GenEmpire(TGen& g)
	{
		if (!*EmpireName(g.s.empire))
			return Txt(g, "Sam juz nie wiem, heh.", "Honestly not sure anymore, heh.");
		static const char* const k[] = { "Jestem z $EMPIRE.", "$EMPIRE.", "Z $EMPIRE, od zawsze." };
		static const char* const kEn[] = { "I'm from $EMPIRE.", "$EMPIRE.", "$EMPIRE, always have been." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenName(TGen& g)
	{
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Przeciez mnie znasz, $NAME :)", "No $NAME, a kto?" };
			static const char* const kEn[] = { "You know me, it's $NAME :)", "$NAME, who else?" };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Jestem $NAME.", "$NAME. Milo mi.", "$NAME, a ty to $PLAYER, prawda?" };
		static const char* const kEn[] = { "I'm $NAME.", "$NAME. Nice to meet you.", "$NAME, and you're $PLAYER, right?" };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenGoal(TGen& g)
	{
		static const char* const k[] = { "Ogolnie chce $G.", "Moj plan to $G.", "Na dluzsza mete chce $G.", "Glownie chce $G." };
		static const char* const kEn[] = { "Overall I want to $G.", "My plan is to $G.", "Long term I want to $G.", "Mostly I want to $G." };
		std::string out = PBC_PICK2(g, k, kEn);
		ReplaceAll(out, "$G", GoalPhrase(g));
		if (g.s.goal == G_HUNTING && !g.s.huntMob.empty())
			Append(out, Fill(g, Txt(g, "Zostalo mi $HUNTN sztuk: $HUNT.", "$HUNTN left to kill: $HUNT.")));
		static const char* const kWhy[V_COUNT] = {
			"Bo chce byc mocniejszy.", "Bo chce zobaczyc, co jest dalej.", "Bo to sie oplaci.",
			"Bo lubie wyzwania.", "Bo wtedy moge wiecej pomoc ekipie." };
		static const char* const kWhyEn[V_COUNT] = {
			"Because I want to get stronger.", "Because I want to see what's further ahead.", "Because it'll pay off.",
			"Because I like a challenge.", "Because then I can help the team more." };
		g.reason = Txt(g, kWhy[g.voice], kWhyEn[g.voice]);
		return out;
	}

	inline std::string GenNextPlan(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.dead)
			return Txt(g, "Najpierw musze wstac :)", "Gotta get back up first :)");
		if (g.LowHp())
		{
			g.reason = Txt(g, "Bo mam malo HP.", "Because my HP is low.");
			return Txt(g, "Najpierw odpoczne, bo mam malo HP.", "Resting first, my HP is low.");
		}
		if (s.bagCells > 0 && s.freeCells <= 2 && !s.inTown)
		{
			g.reason = Txt(g, "Bo mam pelne EQ.", "Because my bag is full.");
			return Txt(g, "Zaraz wracam do miasta, EQ mam pelne. Potem pewnie znowu na exp.",
					"Heading back to town soon, bag's full. Then probably back to hunting.");
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
		{
			static const char* const k[] = { "Najpierw dojde $DEST, potem sie zobaczy.", "Jak dojde $DEST, to tam zostane na troche." };
			static const char* const kEn[] = { "First getting $DEST, then we'll see.", "Once I get $DEST, I'll stay there a while." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.askerInParty && Fighting(g))
		{
			static const char* const k[] = { "Jak skonczymy tutaj, pewnie wracam do miasta.", "Jeszcze troche tu pobijemy, a potem sie zobaczy." };
			static const char* const kEn[] = { "When we're done here, probably back to town.", "We'll hit a bit more here, then we'll see." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.shopStanding)
			return Txt(g, "Postoje jeszcze troche ze straganem, a potem pewnie na exp.",
					"Gonna run my shop a bit longer, then probably go hunting.");
		if (s.fishing)
			return Txt(g, "Jeszcze troche polowie, potem zobaczymy.", "Gonna fish a bit more, then we'll see.");
		if (Fighting(g) && s.goal == G_LEVEL)
		{
			static const char* const k[] = { "Dalej expic, moze zmienie spot, jak wbije poziom.", "Jeszcze troche tu, potem pewnie do miasta sprzedac drop." };
			static const char* const kEn[] = { "Keep hunting, maybe switch spots after I level up.",
				"A bit more here, then probably to town to sell the drops." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Potem pewnie $G.", "Pozniej chce $G.", "Dalej? Chyba $G." };
		static const char* const kEn[] = { "After that I'll probably $G.", "Later I want to $G.", "Next? I guess I'll $G." };
		std::string out = PBC_PICK2(g, k, kEn);
		ReplaceAll(out, "$G", GoalPhrase(g));
		return out;
	}

	inline std::string GenHp(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.dead)
			return Txt(g, "Nie zyje wlasnie, czekam az wstane.", "Dead right now, waiting to get up.");
		if (s.hpPct < 15)
			out = Txt(g, "Bardzo malo, ledwo stoje.", "Very low, barely standing.");
		else if (s.hpPct < 30)
			out = Txt(g, "Malo, zaraz musze sie podleczyc.", "Low, gotta heal up soon.");
		else if (s.hpPct < 60)
			out = Fill(g, Txt(g, "Moze byc, okolo $HP%.", "It's okay, around $HP%."));
		else if (s.hpPct < 95)
		{
			static const char* const k[] = { "Dobrze, okolo $HP%.", "W porzadku, $HP%." };
			static const char* const kEn[] = { "Good, around $HP%.", "Fine, $HP%." };
			out = PBC_SAY2(g, k, kEn);
		}
		else
		{
			static const char* const k[] = { "Pelne, jestem w formie.", "Full HP." };
			static const char* const kEn[] = { "Full, I'm in good shape.", "Full HP." };
			out = PBC_SAY2(g, k, kEn);
		}
		if (s.spPct < 20)
			Append(out, Txt(g, "Gorzej z SP, prawie pusto.", "SP is worse though, almost empty."));
		return out;
	}

	inline std::string GenGold(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.tier <= TIER_STRANGER && s.gold >= 1000000)
		{
			static const char* const k[] = { "Wystarczy mi na potrzeby :)", "Troche jest, nie narzekam." };
			static const char* const kEn[] = { "Enough for what I need :)", "Got some, can't complain." };
			return PBC_SAY2(g, k, kEn);
		}
		std::string out;
		if (s.gold < 10000)
			out = Txt(g, "Prawie nic, bieda :(", "Almost nothing, I'm broke :(");
		else if (s.gold < 1000000)
			out = Fill(g, Txt(g, "Troche drobnych, $GOLD.", "Just some change, $GOLD."));
		else if (s.gold < 50000000)
		{
			static const char* const k[] = { "Mam $GOLD, nie narzekam.", "Okolo $GOLD." };
			static const char* const kEn[] = { "Got $GOLD, can't complain.", "Around $GOLD." };
			out = PBC_SAY2(g, k, kEn);
		}
		else
			out = Fill(g, Txt(g, "Sporo, $GOLD. Ale nie mow nikomu :)", "Quite a bit, $GOLD. Don't tell anyone :)"));
		if (g.voice == V_MERCHANT && g.rng.Chance(40))
			Append(out, Txt(g, "I caly czas obracam tym na targu.", "And I keep it moving on the market."));
		g.reason = g.voice == V_MERCHANT ? Txt(g, "Bo handluje.", "Because I trade.") :
				Txt(g, "Bo sprzedaje drop.", "Because I sell my drops.");
		return out;
	}

	inline std::string GenHorse(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.horseLevel <= 0)
			return Txt(g, "Nie mam jeszcze konia.", "No horse yet.");
		if (s.riding)
		{
			static const char* const k[] = { "Jade teraz na nim, ma $HORSELVL poziom.", "Siedze na nim wlasnie. $HORSELVL poziom." };
			static const char* const kEn[] = { "Riding it right now, it's level $HORSELVL.", "Sitting on it right now. Level $HORSELVL." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.goal == G_HORSE)
			return Fill(g, Txt(g, "Mam, $HORSELVL poziom. Wlasnie go rozwijam.", "Yep, level $HORSELVL. Working on it right now."));
		static const char* const k[] = { "Mam, $HORSELVL poziom.", "Jest, poziom $HORSELVL." };
		static const char* const kEn[] = { "Yep, level $HORSELVL.", "Got one, level $HORSELVL." };
		return PBC_SAY2(g, k, kEn);
	}

	// The gear line. The names stand in the nominative, after "w rece:" or
	// "bron to" - "Mam Pajecza Wlocznia" is a case nobody speaks - and a
	// question asked again a moment later gets "dalej to samo", not the
	// line recited a second and a third time.
	inline std::string GenEquipment(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.weaponName.empty())
			return Txt(g, "Na razie bez porzadnej broni.", "No decent weapon for now.");
		const bool again = g.m.gearSaidAt != 0 && g.now - g.m.gearSaidAt < CONV_FACT_TTL_MS;
		g.m.gearSaidAt = g.now != 0 ? g.now : 1;
		if (again)
		{
			static const char* const k[] = { "Dalej to samo: $WEAPON.", "Nic sie nie zmienilo, dalej $WEAPON.", "Wciaz $WEAPON w rece." };
			static const char* const kEn[] = { "Same as before: $WEAPON.", "Nothing's changed, still $WEAPON.", "Still $WEAPON in my hand." };
			return PBC_SAY2(g, k, kEn);
		}
		if (g.a && g.a->concepts.Has(C_BONUS))
		{
			std::string out = Txt(g, "Bonusow nie licze co do punktu.", "I don't keep track of my bonuses to the last point.");
			Append(out, Fill(g, s.armorName.empty() ? Txt(g, "W rece: $WEAPON.", "In my hand: $WEAPON.") :
					Txt(g, "W rece: $WEAPON, na sobie: $ARMOR.", "In my hand: $WEAPON, wearing: $ARMOR.")));
			return out;
		}
		std::string out;
		if (!s.armorName.empty())
		{
			static const char* const k[] = { "W rece $WEAPON, na sobie $ARMOR.", "Bron to $WEAPON, a zbroja $ARMOR.",
				"W rece: $WEAPON. Na sobie: $ARMOR." };
			static const char* const kEn[] = { "Holding $WEAPON, wearing $ARMOR.", "Weapon's $WEAPON, armor's $ARMOR.",
				"In my hand: $WEAPON. Wearing: $ARMOR." };
			out = PBC_SAY2(g, k, kEn);
		}
		else
			out = Fill(g, Txt(g, "W rece: $WEAPON.", "In my hand: $WEAPON."));
		if (s.weaponPlus >= 7)
			Append(out, Txt(g, "Nie narzekam.", "Can't complain."));
		else if (s.weaponPlus <= 2 && g.rng.Chance(50))
			Append(out, Txt(g, "Trzeba by to ulepszyc.", "Should upgrade it at some point."));
		return out;
	}

	inline std::string GenInventory(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (g.en)
			out = s.bagSummary.empty() ? std::string("Nothing interesting in my bag.") :
					"In my bag, among other things: " + s.bagSummary + ".";
		else
			out = s.bagSummary.empty() ? std::string("Nic ciekawego w EQ.") : "W EQ m.in.: " + s.bagSummary + ".";
		if (s.bagCells > 0)
			Append(out, Fill(g, Txt(g, "Wolnych miejsc $FREE.", "$FREE free slots.")));
		return out;
	}

	inline std::string GenInventorySpace(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.bagCells <= 0)
			return Txt(g, "Nie wiem, jeszcze nie ogarnalem plecaka.", "Not sure, haven't sorted my bag yet.");
		if (s.freeCells == 0)
		{
			g.reason = Txt(g, "Bo za duzo dropu nazbieralem.", "Because I picked up too much loot.");
			return Txt(g, "Zero, EQ pelne. Musze do miasta.", "Zero, bag's full. Need to go to town.");
		}
		if (s.freeCells <= 5)
			return Fill(g, Txt(g, "Malo, $FREE wolnych miejsc.", "Not much, $FREE free slots."));
		static const char* const k[] = { "Mam $FREE wolnych miejsc.", "Jeszcze $FREE miejsc wolnych, spokojnie." };
		static const char* const kEn[] = { "I've got $FREE free slots.", "Still $FREE free slots, no worries." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string ShopWhere(const TGen& g);
	inline std::string SayMoney(long long v);

	inline std::string GenItemOwn(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return Txt(g, "Co konkretnie?", "What exactly?");
		// Not items: "masz czas?" / "got a sec?", "masz racje" / "you have a
		// point", "masz pomysl?" / "got an idea?".
		if (obj == "czas" || obj == "chwile" || obj == "moment" || obj == "chwilke" || obj == "time" || obj == "sec" ||
				obj == "second" || obj == "minute")
			return g.Bad() ? Txt(g, "Troche mam, o co chodzi?", "I've got a bit, what's up?") :
					Txt(g, "Mam chwile, o co chodzi?", "Got a moment, what's up?");
		if (obj == "racje" || obj == "racja" || obj == "point")
			return Txt(g, "No wiem :)", "I know :)");
		if (obj == "ochote" || obj == "ochota")
			return Txt(g, "Na co?", "For what?");
		if (obj == "pomysl" || obj == "pomysly" || obj == "idea" || obj == "ideas")
			return Txt(g, "Moze pobijemy cos razem?", "Maybe we hunt something together?");
		std::string name;
		unsigned int count = 0;
		if (g.world && g.world->FindItem(obj, name, count))
		{
			std::string out = Txt(g, "Tak, mam: ", "Yep, I have: ") + name;
			if (count > 1)
				out += " x" + ToString(count);
			out += ".";
			return out;
		}
		long long price = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return g.en ? "Not in my bag, but my shop " + ShopWhere(g) + " has " + name + " for " + SayMoney(price) + "." :
					"W EQ nie, ale na straganie " + ShopWhere(g) + " stoi " + name + " za " + SayMoney(price) + ".";
		static const char* const k[] = { "Nie, nie mam tego.", "Nie mam czegos takiego w EQ.", "Niestety nie." };
		static const char* const kEn[] = { "Nope, don't have that.", "Don't have anything like that in my bag.", "Sadly no." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenParty(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		g.saidParty = true;
		if (!s.inParty)
		{
			if (g.voice == V_SOCIAL && g.tier >= TIER_STRANGER && g.askBack.empty())
			{
				g.askBack = Txt(g, "Zagramy razem?", "Wanna play together?");
				g.askBackKind = ASK_JOIN;
			}
			static const char* const k[] = { "Teraz jestem sam.", "Sam, nikt mnie nie zaprosil :)", "Solo na razie." };
			static const char* const kEn[] = { "Alone right now.", "Solo, nobody invited me :)", "Solo for now." };
			g.reason = g.voice == V_GRINDER ? Txt(g, "Bo sam szybciej expie.", "Because I level faster alone.") :
					Txt(g, "Bo nikt mnie nie zaprosil :)", "Because nobody invited me :)");
			return PBC_SAY2(g, k, kEn);
		}
		if (s.askerInParty && !(g.a && g.a->concepts.Has(C_WHO)))
			return Txt(g, "No z toba przeciez :)", "With you, obviously :)");
		if (g.a && (g.a->follow == F_WHO || g.a->concepts.Has(C_WHO)))
			return Fill(g, Txt(g, "Liderem jest $LEADER.", "$LEADER is the leader."));
		if (s.leaderIsMe)
		{
			static const char* const k[] = { "Prowadze PT, jest nas $PARTYN.", "Mam swoje PT, $PARTYN osob." };
			static const char* const kEn[] = { "I'm leading a party, $PARTYN of us.", "Got my own party, $PARTYN people." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Jestem w PT z $LEADER, razem $PARTYN osob.", "W grupie, $PARTYN osob. Lider to $LEADER." };
		static const char* const kEn[] = { "In a party with $LEADER, $PARTYN of us.", "In a group of $PARTYN. $LEADER leads." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenGuild(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (!s.inGuild)
		{
			static const char* const kSoc[] = { "Nie mam gildii. Szukam jakiejs fajnej.", "Jeszcze nie, ale chetnie bym do jakiejs wstapil." };
			static const char* const k[] = { "Nie mam gildii.", "Bez gildii na razie.", "Nie, jestem bez gildii." };
			static const char* const kSocEn[] = { "No guild. Looking for a nice one.", "Not yet, but I'd happily join one." };
			static const char* const kEn[] = { "No guild.", "Guildless for now.", "Nope, not in a guild." };
			g.reason = Txt(g, "Jakos nie trafilem na odpowiednia.", "Haven't found the right one yet.");
			return g.voice == V_SOCIAL ? PBC_SAY2(g, kSoc, kSocEn) : PBC_SAY2(g, k, kEn);
		}
		std::string out;
		if (g.a && (g.a->follow == F_COUNT || g.a->concepts.Has(C_HOWMUCH) || g.a->concepts.Has(C_MANY)))
			out = Fill(g, Txt(g, "W $GUILD jest nas $GUILDN.", "There are $GUILDN of us in $GUILD."));
		else
		{
			static const char* const k[] = { "Jestem w $GUILD, jest nas $GUILDN.", "$GUILD. Fajna ekipa.", "Tak, $GUILD." };
			static const char* const kEn[] = { "I'm in $GUILD, $GUILDN of us.", "$GUILD. Great bunch.", "Yep, $GUILD." };
			out = PBC_SAY2(g, k, kEn);
		}
		if (s.guildWar)
			Append(out, Txt(g, "Akurat mamy wojne!", "We're at war right now!"));
		return out;
	}

	inline std::string GenFishing(TGen& g)
	{
		if (g.s.fishing)
		{
			static const char* const k[] = { "Tak, lowie teraz ryby.", "No, siedze z wedka." };
			static const char* const kEn[] = { "Yeah, fishing right now.", "Yep, sitting with my rod." };
			return PBC_SAY2(g, k, kEn);
		}
		if (g.s.style == S_FISHER)
			return Txt(g, "Teraz nie, ale lubie polowic.", "Not right now, but I like fishing.");
		return Txt(g, "Nie, teraz nie lowie.", "Nope, not fishing right now.");
	}

	inline std::string GenMining(TGen& g)
	{
		if (g.s.mining)
		{
			static const char* const k[] = { "Tak, kopie rude.", "No, kopie. Ciezka robota." };
			static const char* const kEn[] = { "Yeah, mining ore.", "Yep, mining. Hard work." };
			return PBC_SAY2(g, k, kEn);
		}
		if (g.s.style == S_MINER)
			return Txt(g, "Teraz nie, ale kopanie to moja dzialka.", "Not now, but mining's my thing.");
		return Txt(g, "Nie, teraz nie kopie.", "Nope, not mining right now.");
	}

	inline std::string GenHerbalism(TGen& g)
	{
		return g.s.herbUnlocked ? Txt(g, "Tak, znam sie troche na ziolach.", "Yeah, I know a bit about herbs.") :
				Txt(g, "Nie, zielarstwa jeszcze nie ogarniam.", "Nope, haven't picked up herbalism yet.");
	}

	inline std::string GenBiologist(TGen& g)
	{
		if (!g.s.bioWanted.empty())
			return Fill(g, g.s.action == A_BIOLOGIST ? Txt(g, "Wlasnie zbieram dla Biologa: $BIO.",
					"Collecting for the Biologist right now: $BIO.") :
					Txt(g, "Dla Biologa szukam teraz: $BIO.", "Looking for this for the Biologist: $BIO."));
		if (g.s.action == A_BIOLOGIST)
			return Txt(g, "Wlasnie robie jego zadanie.", "Doing his quest right now.");
		return Txt(g, "Teraz nic dla Biologa nie zbieram.", "Not collecting anything for the Biologist right now.");
	}

	inline std::string GenMetin(TGen& g)
	{
		if (Fighting(g) && g.s.targetStone)
			return Txt(g, "Wlasnie jednego bije!", "Hitting one right now!");
		if (g.s.metinHunter || g.s.goal == G_METIN)
			return Txt(g, "Poluje na nie, jak tylko sie jakis pojawi.", "I hunt them whenever one shows up.");
		if (g.voice == V_FIGHTER)
			return Txt(g, "Uwielbiam je bic, ale teraz akurat zadnego nie widze.", "Love smashing them, but I don't see any right now.");
		return Txt(g, "Teraz nie, ale jak jakis sie trafi, to bije.", "Not right now, but if one shows up, I'll hit it.");
	}

	inline std::string GenDemonTower(TGen& g)
	{
		if (g.s.demonTower)
			return g.s.mapIndex == 66 ? Txt(g, "Tak, jestem w Wiezy Demonow.", "Yeah, I'm in the Demon Tower.") :
					Txt(g, "Tak, mam sprawy w Wiezy Demonow.", "Yeah, got business in the Demon Tower.");
		return g.s.level >= 40 ? Txt(g, "Teraz nie, moze kiedys z ekipa.", "Not now, maybe some day with a team.") :
				Txt(g, "Jeszcze za slaby jestem na Wieze.", "Still too weak for the Tower.");
	}

	inline std::string GenGuildWar(TGen& g)
	{
		if (g.s.guildWar)
			return Txt(g, "Tak, mamy wojne gildii!", "Yeah, we're in a guild war!");
		if (g.s.inGuild)
			return Txt(g, "Teraz spokoj, zadnej wojny.", "All quiet now, no war.");
		return Txt(g, "Nie mam gildii, wiec i wojen nie ma.", "No guild, so no wars either.");
	}

	inline std::string GenMercenary(TGen& g)
	{
		if (g.s.mercContract)
			return Txt(g, "Tak, mam teraz kontrakt.", "Yeah, I've got a contract right now.");
		if (g.s.style == S_MERC)
			return Txt(g, "Teraz nie mam kontraktu. Ale jakby co, jestem do wynajecia.",
					"No contract right now. But I'm for hire if you need one.");
		return Txt(g, "Nie, nie mam teraz zadnego kontraktu.", "Nope, no contract right now.");
	}

	inline std::string GenPartyRequest(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.tier == TIER_HOSTILE)
			return Txt(g, "Nie, dzieki.", "No, thanks.");
		if (s.askerInParty)
			return Txt(g, "Przeciez juz jestesmy razem :)", "We're already together :)");
		if (g.a && g.a->follow == F_HOW)
			return Txt(g, "Po prostu zapros mnie do grupy.", "Just invite me to the group.");
		if (s.inParty)
			return Txt(g, "Jestem teraz w innym PT, moze pozniej.", "I'm in another party right now, maybe later.");
		if (s.shopStanding)
			return Txt(g, "Teraz stoje ze straganem, moze pozniej.", "Running my shop right now, maybe later.");
		if (s.dead || g.LowHp())
			return Txt(g, "Chwila, najpierw sie podlecze.", "Hold on, gotta heal up first.");
		// An invitation reaches nobody on the other channel.
		if (AskerOnOtherChannel(s))
			return Fill(g, Txt(g, "Chetnie, ale jestem $MAPIN. Przejdz na moj kanal, to mnie zaprosisz.",
					"Sure, but I'm $MAPIN. Switch to my channel and invite me."));
		std::string out;
		if (s.askerLevel > 0 && (s.askerLevel > s.level + 15 || s.level > s.askerLevel + 15))
			out = Txt(g, "Chetnie, ale mamy duza roznice poziomow. Zapros, zobaczymy.",
					"Sure, but there's a big level gap between us. Invite me and we'll see.");
		else if (g.tier == TIER_STRANGER)
		{
			static const char* const k[] = { "Nie znamy sie jeszcze, ale czemu nie. Zapros mnie do PT.", "Mozemy sprobowac. Zapros mnie, zobaczymy." };
			static const char* const kEn[] = { "We don't know each other yet, but why not. Invite me to the party.",
				"We can try. Invite me and we'll see." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (g.tier == TIER_KNOWN)
		{
			static const char* const k[] = { "Jasne, mozemy sprobowac. Zapros mnie.", "Jasne, zapros mnie do PT." };
			static const char* const kEn[] = { "Sure, we can try. Invite me.", "Sure, invite me to the party." };
			out = PBC_SAY2(g, k, kEn);
		}
		else
		{
			static const char* const k[] = { "Z toba zawsze mozna isc. Dawaj zaproszenie.", "Jasne! Zapros mnie, juz ide." };
			static const char* const kEn[] = { "Always up for it with you. Send the invite.", "Sure! Invite me, I'm coming." };
			out = PBC_SAY2(g, k, kEn);
		}
		return out;
	}

	// ------------------------------------------------------------- trade

	inline std::string SayMoney(long long v)
	{
		return FormatYang(v);
	}

	inline std::string ShopWhere(const TGen& g)
	{
		std::string out = Fill(g, "$SHOPAT");
		if (g.s.shopOtherChannel)
			out += Txt(g, " (inny kanal)", " (other channel)");
		return out;
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
				static const char* const kEn[] = {
					"It's even cheaper, $PRICE. Just buy it from my shop $WHERE.",
					"No need to overpay, it's $PRICE in my shop $WHERE." };
				out = PBC_PICK2(g, k, kEn);
			}
			else if (offer >= price)
			{
				static const char* const k[] = {
					"Za $OFFER moze byc. $ITEM stoi na moim straganie $WHERE, kup normalnie.",
					"$OFFER? Pasuje. Masz to na straganie $WHERE za $PRICE." };
				static const char* const kEn[] = {
					"$OFFER works. $ITEM is in my shop $WHERE, just buy it there.",
					"$OFFER? Deal. It's in my shop $WHERE for $PRICE." };
				out = PBC_PICK2(g, k, kEn);
			}
			else if (offer * 100 >= price * 80)
			{
				static const char* const k[] = {
					"Troche malo. Stoi za $PRICE, taniej raczej nie zejde.",
					"Blisko, ale stoi za $PRICE. Taniej nie oddam." };
				static const char* const kEn[] = {
					"A bit low. It's $PRICE, not going lower.",
					"Close, but it's $PRICE. Won't go cheaper." };
				out = PBC_PICK2(g, k, kEn);
			}
			else
			{
				static const char* const k[] = { "Za $OFFER? Nie, stoi za $PRICE.", "Za malo. Chce $PRICE." };
				static const char* const kEn[] = { "For $OFFER? No, it's $PRICE.", "Too little. I want $PRICE." };
				out = PBC_PICK2(g, k, kEn);
			}
		}
		else if (count > 1)
			out = Txt(g, "Tak, na straganie $WHERE stoi $ITEM x$COUNT, $PRICE za calosc.",
					"Yep, my shop $WHERE has $ITEM x$COUNT, $PRICE for all of it.");
		else
		{
			static const char* const k[] = { "Tak, na straganie $WHERE stoi $ITEM za $PRICE.", "Mam. $ITEM, $PRICE, stragan $WHERE." };
			static const char* const kEn[] = { "Yep, my shop $WHERE has $ITEM for $PRICE.", "Got it. $ITEM, $PRICE, shop $WHERE." };
			out = PBC_PICK2(g, k, kEn);
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
			std::string out = Txt(g, "Na straganie tego nie mam.", "Don't have that in my shop.");
			if (!g.s.shopSummary.empty())
				Append(out, Txt(g, "Mam za to: ", "I do have: ") + g.s.shopSummary + ".");
			return out;
		}
		unsigned int bagCount = 0;
		if (g.world && g.world->FindItem(obj, name, bagCount))
			return g.en ? "No shop right now, but I've got " + name + " in my bag." :
					"Straganu teraz nie mam, ale w EQ lezy " + name + ".";
		return Txt(g, "Nie mam tego, a straganu teraz tez nie.", "Don't have it, and no shop right now either.");
	}

	inline std::string GenShop(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.a && !g.a->object.empty())
			return ShopItemAnswer(g, g.a->object);
		if (s.shopOpen && !s.shopSummary.empty())
		{
			std::string out = g.en ? "Got a shop " + ShopWhere(g) + ". Among other things: " + s.shopSummary + "." :
					"Mam stragan " + ShopWhere(g) + ". Na nim m.in.: " + s.shopSummary + ".";
			if (s.shopItems > 3)
				Append(out, Txt(g, "I jeszcze troche innych rzeczy.", "And a few other things."));
			return out;
		}
		if (s.shopOpen)
			return Txt(g, "Mam stragan, ale juz prawie wszystko zeszlo.", "Got a shop, but almost everything's sold.");
		if (g.voice == V_MERCHANT)
			return Txt(g, "Teraz nie mam straganu, ale niedlugo cos wystawie.", "No shop right now, but I'll put something up soon.");
		return Txt(g, "Nie mam teraz straganu.", "No shop right now.");
	}

	inline std::string GenPrice(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
		{
			if (g.s.shopOpen && !g.s.shopSummary.empty())
				return Txt(g, "U mnie: ", "At my shop: ") + g.s.shopSummary + ".";
			return Txt(g, "Ale czego cena?", "The price of what?");
		}
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
		{
			if (g.a->offerYang > 0)
				return ShopLineAnswer(g, name, price, count);
			std::string out = g.en ? "In my shop " + name + " goes for " + SayMoney(price) + "." :
					"U mnie na straganie " + name + " stoi za " + SayMoney(price) + ".";
			if (count > 1)
				out = g.en ? "At my shop " + name + " x" + ToString(count) + " for " + SayMoney(price) + " total." :
						"U mnie " + name + " x" + ToString(count) + " za " + SayMoney(price) + " calosc.";
			return out;
		}
		unsigned int sellers = 0;
		if (g.world && g.world->FindMarketPrice(obj, name, price, sellers))
		{
			static const char* const k[] = {
				"Na targu widzialem $ITEM po $PRICE.", "$ITEM chodzi teraz po jakies $PRICE.",
				"Najtaniej widzialem $ITEM za $PRICE." };
			static const char* const kEn[] = {
				"Saw $ITEM on the market for $PRICE.", "$ITEM goes for about $PRICE right now.",
				"Cheapest I've seen $ITEM for is $PRICE." };
			std::string out = PBC_PICK2(g, k, kEn);
			ReplaceAll(out, "$ITEM", name);
			ReplaceAll(out, "$PRICE", SayMoney(price));
			CapitalizeFirst(out);
			if (g.voice == V_MERCHANT && g.rng.Chance(50))
				Append(out, Txt(g, "Ceny sie jednak zmieniaja.", "Prices change though."));
			return out;
		}
		static const char* const k[] = {
			"Nie wiem, dawno nie widzialem tego na targu.", "Ciezko powiedziec, nikt tego ostatnio nie wystawial." };
		static const char* const kEn[] = {
			"No idea, haven't seen it on the market in a while.", "Hard to say, nobody's listed it lately." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenMarket(TGen& g)
	{
		if (g.s.marketTrip || g.s.action == A_MARKET)
			return Txt(g, "Wlasnie ide na targ, zobacze co jest.", "Heading to the market now, gonna see what's there.");
		if (g.voice == V_MERCHANT)
		{
			static const char* const k[] = { "Ceny ostatnio skacza, trzeba uwazac.", "Handel to moja dzialka. Kupuje tanio, sprzedaje drozej." };
			static const char* const kEn[] = { "Prices jump around lately, gotta be careful.", "Trading's my thing. Buy low, sell high." };
			return PBC_SAY2(g, k, kEn);
		}
		return Txt(g, "Handluje troche, jak mam cos na zbyciu.", "I trade a bit when I have something to sell.");
	}

	inline std::string GenBuy(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
		{
			if (g.en)
				return g.s.shopOpen && !g.s.shopSummary.empty()
						? "In my shop I've got: " + g.s.shopSummary + ". What are you interested in?"
						: std::string("What exactly do you want to buy?");
			return g.s.shopOpen && !g.s.shopSummary.empty()
					? "Na straganie mam: " + g.s.shopSummary + ". Co cie interesuje?"
					: std::string("Co konkretnie chcesz kupic?");
		}
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return ShopLineAnswer(g, name, price, count);
		if (g.world && g.world->FindItem(obj, name, count))
			return g.en ? "I've got " + name + " in my bag, but it's not up for sale." :
					"W EQ lezy " + name + ", ale nie wystawilem tego na sprzedaz.";
		if (g.s.shopOpen)
			return Txt(g, "Tego nie mam na straganie.", "Don't have that in my shop.");
		return Txt(g, "Nie mam tego teraz na sprzedaz.", "Don't have that for sale right now.");
	}

	inline std::string GenSell(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return Txt(g, "Co chcesz mi sprzedac?", "What do you want to sell me?");
		std::string r = g.world ? g.world->AnswerSell(obj) : std::string();
		return r.empty() ? std::string(Txt(g, "Nie potrzebuje teraz tego.", "Don't need that right now.")) : r;
	}

	inline std::string GenSkills(TGen& g)
	{
		std::string out;
		if (g.s.action == A_READ_BOOK)
			out = Txt(g, "Wlasnie czytam ksiegi, podbijam skille.", "Reading books right now, leveling my skills.");
		else if (g.s.goal == G_SKILL)
			out = Txt(g, "Teraz glownie podbijam umiejetnosci.", "Mostly leveling my skills right now.");
		else
		{
			static const char* const k[] = { "Rozwijam, jak mam ksiegi. Tanio nie jest.", "Powoli do przodu z umiejetnosciami." };
			static const char* const kEn[] = { "I level them when I have books. Not cheap.", "Slowly getting there with my skills." };
			out = PBC_SAY2(g, k, kEn);
		}
		// What they are at, from the character sheet.
		const std::string list = SkillsList(g.s, 3, g.en);
		if (!list.empty())
			Append(out, Txt(g, "Najwyzej mam ", "My best: ") + list + ".");
		return out;
	}

	inline std::string GenPvp(TGen& g)
	{
		if (g.LowHp() || g.s.dead)
			return Txt(g, "Nie teraz, mam malo HP.", "Not now, my HP is low.");
		if (g.voice == V_FIGHTER)
			return Txt(g, "Chetnie! Wyzwij mnie normalnie przez PvP.", "Gladly! Just challenge me to a duel.");
		return Txt(g, "Mozemy, ale wyzwij mnie normalnie przez PvP.", "Sure, just challenge me to a duel the normal way.");
	}

	inline std::string GenTravel(TGen& g)
	{
		if (const long mentioned = MentionedMap(g))
		{
			if (mentioned == g.s.mapIndex && IsKnownMap(g.s.mapIndex))
			{
				g.saidMap = true;
				return Fill(g, Txt(g, "Juz jestem $MAPIN.", "I'm already $MAPIN."));
			}
			if (g.s.action == A_TRAVEL && mentioned == g.s.travelMap)
				return Fill(g, Txt(g, "Tak, ide $DEST.", "Yeah, heading $DEST."));
			std::string out = Txt(g, "Nie. ", "No. ");
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
			return Fill(g, Txt(g, "Nigdzie, zostaje $MAPIN.", "Nowhere, staying $MAPIN."));
		}
		return Txt(g, "Nigdzie, zostaje tutaj.", "Nowhere, staying here.");
	}

	inline std::string GenRest(TGen& g)
	{
		if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
			return Txt(g, "Tak, odpoczywam chwile.", "Yeah, resting a bit.");
		if (g.LowHp())
			return Txt(g, "Zaraz bede musial, HP mi siada.", "Gonna have to soon, my HP's dropping.");
		return Txt(g, "Nie, jeszcze mam sile.", "Nope, still got energy.");
	}

	inline std::string GenRefine(TGen& g)
	{
		if (g.s.action == A_REFINE)
			return Txt(g, "Wlasnie ulepszam, trzymaj kciuki.", "Upgrading right now, fingers crossed.");
		if (g.s.euphoria)
			return Txt(g, "Ostatnio weszlo mi niezle ulepszenie!", "Just landed a nice upgrade!");
		if (g.s.goal == G_REFINE)
			return Txt(g, "Planuje ulepszyc bron, jak tylko zbiore materialy.", "Planning to upgrade my weapon once I have the materials.");
		if (!g.s.weaponName.empty())
			return Fill(g, Txt(g, "Ulepszam, jak mam materialy. Teraz w rece: $WEAPON.",
					"I upgrade when I have the materials. In my hand now: $WEAPON."));
		return Txt(g, "Jak bedzie z czego, to ulepsze.", "When I have the stuff, I'll upgrade.");
	}

	inline std::string GenMissions(TGen& g)
	{
		if (!g.s.huntMob.empty())
			return Fill(g, Txt(g, "Mam polowanie: $HUNT, zostalo $HUNTN.", "Got a hunt going: $HUNT, $HUNTN left."));
		if (!g.s.bioWanted.empty())
			return Fill(g, Txt(g, "Zbieram dla Biologa: $BIO.", "Collecting for the Biologist: $BIO."));
		return Txt(g, "Teraz zadnej misji nie mam.", "No missions right now.");
	}

	inline std::string GenDeath(TGen& g)
	{
		if (g.s.dead)
			return Txt(g, "No wlasnie leze, ktos mnie ubil.", "Yep, lying here right now, something killed me.");
		if (g.s.recentDeaths > 0 && g.s.minutesSinceDeath < 60)
			return g.s.recentDeaths > 1 ? Txt(g, "Kilka razy juz dzis padlem. Bywa.", "Died a few times today already. It happens.") :
					Txt(g, "Tak, niedawno zginalem. Bywa.", "Yeah, died not long ago. It happens.");
		return Txt(g, "Nie, dzis jeszcze nie :)", "Nope, not today yet :)");
	}

	inline std::string GenRelationship(TGen& g)
	{
		switch (g.tier)
		{
			case TIER_HOSTILE: return Txt(g, "Szczerze? Nie bardzo po tym, co pisales.", "Honestly? Not really, after what you wrote.");
			case TIER_STRANGER: return Txt(g, "Dopiero sie poznajemy, ale wydajesz sie spoko.",
					"We're just getting to know each other, but you seem cool.");
			case TIER_KNOWN: return Txt(g, "Jasne, spoko jestes.", "Sure, you're cool.");
			case TIER_FRIEND: return Txt(g, "Pewnie! Dobrze sie z toba gada.", "Of course! I like talking to you.");
			default: return Txt(g, "No jasne, jestes jednym z moich ulubionych ludzi tutaj :)",
					"Of course, you're one of my favorite people here :)");
		}
	}

	inline std::string GenTimeHere(TGen& g)
	{
		const u32 m = g.s.actionMinutes > 0 ? g.s.actionMinutes : g.s.goalMinutes;
		if (m >= 60)
			return Fill(g, Txt(g, "Dluzsza chwile, ponad godzine.", "A good while, over an hour."));
		if (m >= 10)
			return Txt(g, "Z kilkanascie minut, moze wiecej.", "Fifteen minutes or so, maybe more.");
		if (m > 0)
			return Txt(g, "Dopiero przyszedlem.", "Just got here.");
		return Txt(g, "Nie liczylem, chwile.", "Didn't count, a while.");
	}

	inline std::string GenMapOpinion(TGen& g)
	{
		const long mentioned = MentionedMap(g);
		if (mentioned && mentioned != g.s.mapIndex)
		{
			// The opinion is keyed on the map's Polish name, so the same bot
			// holds the same view of a place in either language.
			const TMapWords& w = GetMapWords(mentioned);
			const int roll = OpinionRoll(g, *w.name ? w.name : "gdzies", 23);
			const TMapWords& shown = MapWordsFor(g, mentioned);
			std::string place = *shown.name ? shown.name : Txt(g, "Tam", "There");
			if (roll < 45)
				return place + Txt(g, "? Lubie, dobre miejsce.", "? I like it, good spot.");
			if (roll < 80)
				return place + Txt(g, "? Moze byc, zalezy na co.", "? It's alright, depends what for.");
			return place + Txt(g, "? Srednio, wole inne miejsca.", "? Meh, I prefer other places.");
		}
		if (g.s.inTown)
			return Txt(g, "Miasto jak miasto. Lubie tu wrocic po expie.", "A town's a town. I like coming back here after hunting.");
		if (!IsKnownMap(g.s.mapIndex))
			return Txt(g, "Dziwne miejsce, ale moze byc.", "Weird place, but it's okay.");
		static const char* const k[V_COUNT][2] = {
			{ "$MAPNAME jest ok, exp leci.", "Lubie, jesli exp dobry." },
			{ "Lubie, ladnie tu.", "$MAPNAME ma swoj klimat." },
			{ "Moze byc, drop sie dobrze sprzedaje.", "Jest ok, jak cos wypada." },
			{ "Jest ok, byle bylo z czym walczyc.", "Lubie, tu sa mocne moby." },
			{ "Lubie, czesto kogos tu spotykam.", "Fajnie, zwlaszcza z ekipa." },
		};
		static const char* const kEn[V_COUNT][2] = {
			{ "$MAPNAME is fine, the exp flows.", "I like it when the exp is good." },
			{ "I like it, it's pretty here.", "$MAPNAME has its own vibe." },
			{ "It's alright, the drops sell well.", "It's fine when stuff drops." },
			{ "It's fine as long as there's something to fight.", "I like it, the mobs here are tough." },
			{ "I like it, I often run into people here.", "It's nice, especially with a team." },
		};
		return Say(g, g.en ? kEn[g.voice] : k[g.voice], 2);
	}

	inline std::string GenDropLuck(TGen& g)
	{
		if (g.s.unlucky)
			return Txt(g, "Kiepsko, dawno nic dobrego nie wypadlo.", "Bad, nothing good has dropped in ages.");
		if (g.s.euphoria)
			return Txt(g, "Swietnie! Ostatnio mialem farta.", "Great! Got lucky recently.");
		if (g.Good())
			return Txt(g, "Calkiem niezle, nie narzekam.", "Pretty good, can't complain.");
		static const char* const k[] = { "Normalnie, nic specjalnego.", "Jak zwykle, raz lepiej, raz gorzej." };
		static const char* const kEn[] = { "Normal, nothing special.", "The usual, sometimes better, sometimes worse." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenProgressToday(TGen& g)
	{
		if (g.s.onlineMinutes >= 120)
			return Txt(g, "Troche sie zrobilo, gram juz dobrych kilka godzin.", "Got a fair bit done, been playing a good few hours.");
		if (g.s.onlineMinutes >= 30)
			return Txt(g, "Troche expa wpadlo, gram od jakiegos czasu.", "Got some exp in, been playing a while.");
		return Txt(g, "Dopiero zaczalem, jeszcze nic wielkiego.", "Just started, nothing big yet.");
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
		static const char* const kEn[V_COUNT][2] = {
			{ "I keep it simple. Exp, level, next.", "I don't talk much, I grind more." },
			{ "I like wandering around, exploring and chatting.", "Curious about the world, I'd say." },
			{ "More of a trader than a fighter.", "I like a good deal." },
			{ "I like a good fight, the tougher the better.", "I don't really run from risk." },
			{ "I like playing with people. Solo is boring.", "I'm pretty social, you can probably tell." },
		};
		return Say(g, g.en ? kEn[g.voice] : k[g.voice], 2);
	}

	inline std::string GenMood(TGen& g)
	{
		if (g.s.dead)
			return Txt(g, "Kiepsko, wlasnie zginalem.", "Bad, just died.");
		if (g.Good())
			return g.LowHp() ? Txt(g, "Humor dobry, tylko HP mniej :)", "Good mood, just less HP :)") :
					Txt(g, "Swietny! Wszystko idzie jak trzeba.", "Great! Everything's going right.");
		if (g.Bad())
		{
			g.reason = g.s.unlucky ? Txt(g, "Bo dawno nic dobrego nie wypadlo.", "Because nothing good has dropped in ages.") :
					Txt(g, "Jakos nic nie idzie.", "Nothing's working out somehow.");
			return g.s.unlucky ? Txt(g, "Kiepski. Pech mnie przesladuje.", "Bad. My luck's been awful.") :
					Txt(g, "Kiepski, szczerze mowiac.", "Bad, honestly.");
		}
		return Txt(g, "Normalnie, spokojnie.", "Normal, calm.");
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
			static const char* const kEn[] = { "Hey again :)", "Hey hey.", "We already said hi :)" };
			return PBC_SAY2(g, k, kEn);
		}
		if (shortForm)
		{
			static const char* const k[] = { "Hej!", "Siema!", "Czesc!" };
			static const char* const kEn[] = { "Hey!", "Hi!", "Yo!" };
			return PBC_SAY2(g, k, kEn);
		}
		const bool longGap = g.a && g.a->gapBefore > 6u * 3600u * 1000u;
		if (g.tier >= TIER_FRIEND)
		{
			if (longGap)
				return Fill(g, Txt(g, "Siema $PLAYER! Dawno cie nie bylo.", "Hey $PLAYER! Long time no see."));
			static const char* const k[] = { "O, siema $PLAYER!", "Hej $PLAYER! Co tam?", "Siemano $PLAYER." };
			static const char* const kEn[] = { "Oh, hey $PLAYER!", "Hey $PLAYER! What's up?", "Yo $PLAYER." };
			return PBC_SAY2(g, k, kEn);
		}
		if (g.tier == TIER_KNOWN)
		{
			static const char* const k[] = { "O, siema $PLAYER.", "Hej $PLAYER.", "Czesc, co tam?" };
			static const char* const kEn[] = { "Oh, hi $PLAYER.", "Hey $PLAYER.", "Hi, what's up?" };
			return PBC_SAY2(g, k, kEn);
		}
		if (g.tier == TIER_HOSTILE)
			return Txt(g, "No.", "Mhm.");
		static const char* const kNight[] = { "Hej. Pozno juz, a ty dalej grasz?", "Siema. Nocna zmiana?" };
		static const char* const kNightEn[] = { "Hey. It's late, you're still playing?", "Hi. Night shift?" };
		if (g.s.hour >= 0 && g.s.hour < 5 && g.rng.Chance(40))
			return PBC_SAY2(g, kNight, kNightEn);
		static const char* const k[] = { "Hej.", "Siema!", "Czesc.", "Hej, co tam?" };
		static const char* const kEn[] = { "Hey.", "Hi!", "Hello.", "Hey, what's up?" };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenFarewell(TGen& g)
	{
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Nara $PLAYER, do nastepnego!", "Trzymaj sie $PLAYER!" };
			static const char* const kEn[] = { "Later $PLAYER, see you next time!", "Take care $PLAYER!" };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Na razie!", "Trzymaj sie.", "Do zobaczenia.", "Powodzenia na expie." };
		static const char* const kEn[] = { "See ya!", "Take care.", "See you around.", "Good luck with the grind." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenHowAreYou(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.dead)
			out = Txt(g, "Kiepsko, wlasnie zginalem :(", "Not great, just died :(");
		else if (g.LowHp())
			out = Txt(g, "Moglo byc lepiej, mam malo HP.", "Could be better, my HP is low.");
		else if (g.Bad())
		{
			static const char* const k[] = { "Tak sobie. Dzis jakos nie idzie.", "Srednio, szczerze mowiac." };
			static const char* const kEn[] = { "So-so. Not my day today.", "Meh, honestly." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (g.Good() || s.euphoria)
		{
			static const char* const k[] = { "Swietnie! Wszystko idzie jak trzeba.", "Super, dzis mi wszystko wychodzi." };
			static const char* const kEn[] = { "Great! Everything's going right.", "Awesome, everything's working out today." };
			out = PBC_SAY2(g, k, kEn);
		}
		else
		{
			static const char* const k[] = { "Spoko, robie swoje.", "W porzadku.", "Dobrze, nie narzekam." };
			static const char* const kEn[] = { "Good, just doing my thing.", "All good.", "Fine, can't complain." };
			out = PBC_SAY2(g, k, kEn);
			if (!g.saidActivity && !g.groupHasActivity && g.rng.Chance(60))
			{
				const std::string act = ActivityClause(g, !g.saidMap);
				if (IsKnownMap(s.mapIndex) && act.find(MapWordsFor(g, s.mapIndex).atShort) != std::string::npos)
					g.saidMap = true;
				Append(out, act);
				g.saidActivity = true;
			}
		}
		if (g.askBack.empty() && !g.Bad() && g.tier != TIER_HOSTILE && g.rng.Chance(g.voice == V_SOCIAL ? 70 : 45))
		{
			static const char* const k[] = { "A u ciebie?", "A ty jak?", "A co u ciebie?" };
			static const char* const kEn[] = { "How about you?", "And you?", "What about you?" };
			g.askBack = PBC_SAY2(g, k, kEn);
			g.askBackKind = ASK_HOW_ARE_YOU;
		}
		return out;
	}

	inline std::string GenHelp(TGen& g)
	{
		return Txt(g, "Pytaj normalnie, jak czlowieka :) Moge powiedziec co robie, gdzie jestem, ile jest mobow, jaki mam lvl, EQ, gildie, PT i plany. A jak chcesz, pogadamy o czymkolwiek.",
				"Just ask like you'd ask anyone :) I can tell you what I'm doing, where I am, how many mobs there are, my level, gear, guild, party and plans. Or we can just chat about whatever.");
	}

	inline std::string GenIsBot(TGen& g)
	{
		static const char* const k[] = { "A co, tak slabo gram? xD", "Bot to ty jestes :P", "Hehe, gram po prostu duzo." };
		static const char* const kEn[] = { "Why, do I play that badly? xD", "You're the bot :P", "Hehe, I just play a lot." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenInsult(TGen& g)
	{
		if (g.m.negative >= 4)
			return std::string(); // stops answering an abusive line
		if (g.m.negative >= 3)
			return Txt(g, "Nie mam ochoty tak rozmawiac.", "I don't feel like talking like this.");
		static const char* const k[] = { "Spokojnie, bez nerwow.", "Nie musisz tak od razu.", "Ok, jak uwazasz." };
		static const char* const kEn[] = { "Easy, no need to get mad.", "No need for that.", "Ok, whatever you say." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenPraise(TGen& g)
	{
		static const char* const k[] = { "Dzieki! Ty tez spoko.", "Hehe, dzieki.", "Milo slyszec." };
		static const char* const kEn[] = { "Thanks! You're cool too.", "Hehe, thanks.", "Nice to hear." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenAge(TGen& g)
	{
		static const char* const k[] = { "Wystarczajaco, zeby grac do rana :)", "O wieku sie nie rozmawia, hehe.", "A co, wygladam staro?" };
		static const char* const kEn[] = { "Old enough to play till morning :)", "We don't talk about age, hehe.", "Why, do I look old?" };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenOrigin(TGen& g)
	{
		if (!*EmpireName(g.s.empire))
			return Txt(g, "Stad i stamtad.", "From here and there.");
		static const char* const k[] = { "Z $EMPIRE.", "Jestem z $EMPIRE. A ty?", "$EMPIRE, od urodzenia." };
		static const char* const kEn[] = { "From $EMPIRE.", "I'm from $EMPIRE. You?", "$EMPIRE, born and raised." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenItemShop(TGen& g)
	{
		if (!g.s.dragonKnown)
			return Txt(g, "Nie sprawdzalem ostatnio, ile mam SM.", "Haven't checked my Dragon Coins lately.");
		if (g.s.dragonCoins <= 0)
			return g.voice == V_MERCHANT ? Txt(g, "Zero SM. Wole yang, IS to nie moja bajka.",
					"Zero Dragon Coins. I prefer yang, the item shop isn't my thing.") :
					Txt(g, "Zero SM, wszystko wydalem.", "Zero Dragon Coins, spent them all.");
		static const char* const k[] = { "Mam $SM SM.", "Jakies $SM SM, nie wiecej." };
		static const char* const kEn[] = { "I've got $SM Dragon Coins.", "About $SM Dragon Coins, no more." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenKs(TGen& g)
	{
		if (g.tier == TIER_HOSTILE)
			return Txt(g, "Nie widzialem tam twojego imienia.", "Didn't see your name on it.");
		static const char* const k[] = {
			"Sorki, nie zauwazylem, ze go bijesz.", "Oj, wybacz, nie widzialem ciebie.", "Sorry, nie chcialem ci ksowac." };
		static const char* const kEn[] = {
			"Sorry, didn't notice you were hitting it.", "Oops, my bad, didn't see you.", "Sorry, didn't mean to KS you." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenReady(TGen& g)
	{
		if (g.s.dead)
			return Txt(g, "Chwila, jeszcze leze.", "Hold on, still lying here.");
		if (g.LowHp())
			return Txt(g, "Chwila, najpierw sie podlecze.", "Hold on, gotta heal up first.");
		static const char* const k[] = { "Gotowy!", "Jasne, rdy.", "Moge isc." };
		static const char* const kEn[] = { "Ready!", "Sure, rdy.", "Good to go." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenGoodLuck(TGen& g)
	{
		static const char* const k[] = { "Dzieki, tobie tez!", "Nawzajem!", "Dzieki, przyda sie." };
		static const char* const kEn[] = { "Thanks, you too!", "Same to you!", "Thanks, I'll need it." };
		return PBC_SAY2(g, k, kEn);
	}

	inline std::string GenBrb(TGen& g)
	{
		static const char* const k[] = { "Jasne, czekam.", "Ok, bede tu.", "Spoko." };
		static const char* const kEn[] = { "Sure, I'll wait.", "Ok, I'll be here.", "No worries." };
		return PBC_SAY2(g, k, kEn);
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
	inline std::string BuffEffectText(const TBuffLine& l, bool en = false)
	{
		char buf[160];
		buf[0] = 0;
		switch (l.skill)
		{
			case CONV_SKILL_BLESSING:
				if (en)
					snprintf(buf, sizeof(buf), "%d%% less damage taken", l.amount);
				else
					snprintf(buf, sizeof(buf), "o %d%% mniej obrazen", l.amount);
				break;
			case CONV_SKILL_REFLECT:
				if (en)
					snprintf(buf, sizeof(buf), "reflects %d%% of melee damage", l.amount);
				else
					snprintf(buf, sizeof(buf), "odbija %d%% obrazen wrecz", l.amount);
				break;
			case CONV_SKILL_DRAGON_AID:
				if (en)
					snprintf(buf, sizeof(buf), "+%d%% critical hit chance", l.amount);
				else
					snprintf(buf, sizeof(buf), "+%d%% szansy na cios krytyczny", l.amount);
				break;
			case CONV_SKILL_CURE:
				if (l.amountMax > l.amount)
				{
					if (en)
						snprintf(buf, sizeof(buf), "heals %d-%d HP", l.amount, l.amountMax);
					else
						snprintf(buf, sizeof(buf), "leczy %d-%d HP", l.amount, l.amountMax);
				}
				else if (en)
					snprintf(buf, sizeof(buf), "heals %d HP", l.amount);
				else
					snprintf(buf, sizeof(buf), "leczy %d HP", l.amount);
				break;
			case CONV_SKILL_SWIFTNESS:
				if (l.amount2 > 0)
				{
					if (en)
						snprintf(buf, sizeof(buf), "+%d movement speed and +%d%% casting speed", l.amount, l.amount2);
					else
						snprintf(buf, sizeof(buf), "+%d do szybkosci ruchu i +%d%% do szybkosci czarowania",
								l.amount, l.amount2);
				}
				else if (en)
					snprintf(buf, sizeof(buf), "+%d movement speed", l.amount);
				else
					snprintf(buf, sizeof(buf), "+%d do szybkosci ruchu", l.amount);
				break;
			case CONV_SKILL_ATTACK_UP:
				if (en)
					snprintf(buf, sizeof(buf), "+%d attack value", l.amount);
				else
					snprintf(buf, sizeof(buf), "+%d do wartosci ataku", l.amount);
				break;
			default:
				break;
		}
		std::string out = buf;
		if (l.seconds > 0)
			out += (en ? " for " : " przez ") + DurationText(l.seconds);
		if (l.skill == CONV_SKILL_CURE && l.amount3 > 0)
		{
			out += en ? ", plus a shield against " + ToString(l.amount3) + " damage from monsters" :
					", do tego oslona na " + ToString(l.amount3) + " obrazen od potworow";
			if (l.seconds3 > 0)
				out += (en ? " for " : " przez ") + DurationText(l.seconds3);
		}
		return out;
	}

	// "zbuffuj mnie", "dasz buffa?" - asked for, not asked about. And in
	// English "buff me", "can you buff me", "need a buff".
	inline bool IsBuffRequest(const TAnalysis& a)
	{
		for (size_t i = 0; i < a.tokens.words.size(); ++i)
		{
			const std::string& w = a.tokens.words[i];
			if (StartsWith(w, "zbuf") || StartsWith(w, "buffuj") || StartsWith(w, "bufuj") ||
					StartsWith(w, "buffnij") || StartsWith(w, "bufnij"))
				return true;
			if (a.tokens.english && StartsWith(w, "buff") && i + 1 < a.tokens.words.size() &&
					(a.tokens.words[i + 1] == "me" || a.tokens.words[i + 1] == "us"))
				return true;
		}
		if (a.tokens.english && a.concepts.Has(C_BUFF) && (a.tokens.Has("need") || a.tokens.Has("give") ||
				a.tokens.Has("prosze") || (a.concepts.Has(C_CAN) && a.concepts.Has(C_ME))))
			return true;
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
		g.reason = Txt(g, "Tyle wychodzi z moich skilli i inteligencji.", "That's what my skills and intelligence give.");
		if (s.job != 3)
		{
			std::string out = g.en ? std::string("I don't have buffs, I play ") + ClassNameInstrEn(s.job) + "." :
					std::string("Nie mam buffow, gram ") + ClassNameInstr(s.job) + ".";
			Append(out, Txt(g, "Buffy daje szaman.", "Buffs are a shaman thing."));
			return out;
		}
		const int build = s.Build();
		if (build == B_NONE)
			return Txt(g, "Nie wybralem jeszcze sciezki, wiec buffow jeszcze nie mam.",
					"Haven't picked my path yet, so no buffs yet.");
		if (asked && BuffBuildOf(asked) != build)
		{
			if (g.en)
				return std::string("Don't have that one, I'm ") + BuildPhraseEn(build) + ". " + SkillNameOfEn(asked) +
						" is a " + BuildShortNameEn(BuffBuildOf(asked)) + " shaman buff.";
			return std::string("Tego nie mam, jestem ") + BuildPhrase(build) + ". " + SkillNameOf(asked) +
					" ma szaman " + BuildShortName(BuffBuildOf(asked)) + ".";
		}
		TBuffReport report;
		if (!g.world || !g.world->DescribeBuffs(report) || report.count <= 0)
			return Txt(g, "Nie umiem ci teraz tego policzyc.", "Can't work that out for you right now.");
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
					return std::string(Txt(g, "Tego jeszcze sie nie nauczylem (", "Haven't learned that one yet (")) +
							SkillNameIn(g.en, l.skill) + ").";
				++unlearnt;
				continue;
			}
			if (!l.known)
				continue;
			Append(list, std::string(SkillNameIn(g.en, l.skill)) + " (" + SkillGradeText(l.level) + ") - " +
					BuffEffectText(l, g.en) + ".");
			++shown;
		}
		if (shown == 0)
			return unlearnt ? Txt(g, "Zadnego buffa jeszcze sie nie nauczylem.", "Haven't learned any buffs yet.") :
					Txt(g, "Nie umiem ci teraz tego policzyc.", "Can't work that out for you right now.");
		std::string out = std::string(report.onAsker ? Txt(g, "Na tobie: ", "On you: ") : Txt(g, "Moje buffy: ", "My buffs: ")) + list;
		if (unlearnt)
			Append(out, Txt(g, "Reszty jeszcze nie umiem.", "Haven't learned the rest yet."));
		if (g.a && IsBuffRequest(*g.a))
			Append(out, s.askerInParty ? Txt(g, "Jestesmy w PT, to pilnuje twoich buffow.", "We're in a party, so I keep your buffs up.") :
					Txt(g, "Zapros mnie do PT, to bede cie buffowac.", "Invite me to your party and I'll buff you."));
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
		// So is "come" in English.
		if (a.concepts.Has(C_JOIN))
		{
			const int w = a.concepts.firstWord[C_JOIN];
			const std::string word = w >= 0 && w < (int)a.tokens.words.size() ? a.tokens.words[w] : std::string();
			if (word != "chodz" && word != "choc" && word != "chodzze" && word != "come")
				return true;
		}
		static const char* const kWords[] = {
			"dam", "dac", "dostaniesz", "prezent", "pokaze", "pokazac", "pomoc", "pomoz", "pomozesz",
			"potrzebuje", "sprawa", "sprawe", "pogadac", "porozmawiac", "zobaczysz", "zobacz" };
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (a.tokens.Has(kWords[i]))
				return true;
		// "come here, i wanna show you something", "i need help", "got a gift".
		if (a.tokens.english)
		{
			static const char* const kWordsEn[] = {
				"show", "give", "gift", "help", "need", "talk", "chat", "trade", "see", "look", "something", "quick",
				"question", "pay" };
			for (size_t i = 0; i < sizeof(kWordsEn) / sizeof(kWordsEn[0]); ++i)
				if (a.tokens.Has(kWordsEn[i]))
					return true;
		}
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
				g.reason = Txt(g, "Bo jestem na innej mapie.", "Because I'm on another map.");
				if (IsKnownMap(g.s.mapIndex))
					return Fill(g, Txt(g, "Jestem daleko, $MAPIN. Stad nie dam rady przyjsc.",
							"I'm far away, $MAPIN. Can't get to you from here."));
				return Txt(g, "Jestem daleko, na innej mapie. Stad nie dam rady przyjsc.",
						"I'm far away, on another map. Can't get to you from here.");
			case SB_OTHER_CHANNEL:
				// $MAPIN names the channel to a person on the other one.
				g.reason = Txt(g, "Bo jestem na innym kanale.", "Because I'm on another channel.");
				return Fill(g, Txt(g, "Jestem $MAPIN, a ty na innym kanale. Stad nie dam rady przyjsc.",
						"I'm $MAPIN and you're on another channel. Can't get to you from here."));
			case SB_STALL:
				g.reason = Txt(g, "Bo pilnuje straganu.", "Because I'm minding my shop.");
				return Txt(g, "Stoje teraz ze straganem, nie moge odejsc.", "Running my shop right now, can't leave it.");
			case SB_FISHING:
				g.reason = Txt(g, "Bo lowie.", "Because I'm fishing.");
				return Txt(g, "Wlasnie lowie, nie zostawie wedki.", "Fishing right now, not leaving my rod.");
			case SB_MINING:
				g.reason = Txt(g, "Bo kopie rude.", "Because I'm mining.");
				return Txt(g, "Kopie teraz rude, nie moge odejsc.", "Mining ore right now, can't leave.");
			case SB_DUEL:
				g.reason = Txt(g, "Bo mam pojedynek.", "Because I'm in a duel.");
				return Txt(g, "Mam teraz pojedynek, pozniej.", "In a duel right now, later.");
			case SB_GUILD_WAR:
				g.reason = Txt(g, "Bo moja gildia ma wojne.", "Because my guild is at war.");
				return Txt(g, "Moja gildia ma teraz wojne, nie moge.", "My guild is at war right now, can't.");
			case SB_TOWER:
				g.reason = Txt(g, "Bo jestem w Wiezy Demonow.", "Because I'm in the Demon Tower.");
				return Txt(g, "Jestem z gildia w Wiezy Demonow, teraz nie wyjde.", "I'm in the Demon Tower with my guild, can't leave now.");
			case SB_DUNGEON:
				g.reason = Txt(g, "Bo jestem w lochu.", "Because I'm in a dungeon.");
				return Txt(g, "Jestem w lochu, teraz stad nie wyjde.", "I'm in a dungeon, can't get out right now.");
			case SB_MERC:
				g.reason = Txt(g, "Bo mam kontrakt.", "Because I've got a contract.");
				return Txt(g, "Mam kontrakt, najpierw musze go skonczyc.", "I've got a contract, gotta finish it first.");
			case SB_OTHER_PARTY:
				g.reason = Txt(g, "Bo jestem z kims innym.", "Because I'm with someone else.");
				return Txt(g, "Jestem teraz z kims w druzynie, nie moge odejsc.", "I'm in a party with someone right now, can't leave.");
			case SB_OTHER_SUMMON:
				g.reason = Txt(g, "Bo juz ide do kogos innego.", "Because I'm already going to someone else.");
				return Txt(g, "Juz ide do kogos innego, sorki.", "Already heading to someone else, sorry.");
			case SB_DEAD:
				return Txt(g, "Chwila, najpierw wstane.", "Hold on, gotta get up first.");
			default:
				return Txt(g, "Teraz nie moge, sorki.", "Can't right now, sorry.");
		}
	}

	// The walk begins - or the engine, asked once more, says no.
	inline std::string SummonGo(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.summonBlock != SB_NONE)
			return SummonRefusal(g, s.summonBlock);
		const int code = g.world ? g.world->StartSummon() : (int)SUMMON_START_FAILED;
		g.reason = Txt(g, "Bo mnie zawolales.", "Because you called me.");
		switch (code)
		{
			case SUMMON_START_OK:
			case SUMMON_START_RENEWED:
				if (s.askerOnMap && s.askerDistance >= 0 && s.askerDistance <= CONV_SUMMON_NEAR_DISTANCE)
					return Txt(g, "Jestem obok, poczekam chwile.", "I'm right here, I'll wait a bit.");
				if (g.tier >= TIER_FRIEND)
				{
					static const char* const k[] = { "Jasne, juz lece!", "Dla ciebie zawsze, juz ide!" };
					static const char* const kEn[] = { "Sure, on my way!", "For you, always. Coming!" };
					return PBC_SAY2(g, k, kEn);
				}
				{
					static const char* const k[] = { "Juz ide!", "Dobra, zaraz bede.", "Ok, ide do ciebie." };
					static const char* const kEn[] = { "On my way!", "Okay, be there in a sec.", "Ok, coming to you." };
					return PBC_SAY2(g, k, kEn);
				}
			case SUMMON_START_BLOCKED:
				return Txt(g, "Teraz nie moge, sorki.", "Can't right now, sorry.");
			default:
				return Txt(g, "Nie widze cie, gdzie jestes?", "I can't see you, where are you?");
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
			g.reason = Txt(g, "Bo mnie zawolales.", "Because you called me.");
			if (code == SUMMON_START_OK || code == SUMMON_START_RENEWED)
				return s.summonArrived ? Txt(g, "Przeciez jestem obok :) Zostane jeszcze chwile.", "I'm right here :) I'll stay a bit longer.") :
						Txt(g, "Juz ide, juz!", "Coming, coming!");
			return Txt(g, "Teraz nie moge, sorki.", "Can't right now, sorry.");
		}
		if (g.tier == TIER_HOSTILE)
		{
			g.reason = Txt(g, "Bo mnie obrazasz.", "Because you keep insulting me.");
			return Txt(g, "Po tym, jak mnie traktujesz? Nie.", "After how you treat me? No.");
		}
		if (s.summonBlock != SB_NONE)
			return SummonRefusal(g, s.summonBlock);
		if (g.tier == TIER_STRANGER && !(g.a && SummonHasReason(*g.a)))
		{
			const bool askedAlready = g.m.botAsk == ASK_SUMMON && g.now - g.m.botAskAt < CONV_BOT_ASK_TTL_MS;
			if (askedAlready || SummonStrangerRoll(g) < SummonRefuseShare(g))
			{
				g.reason = Txt(g, "Bo sie nie znamy.", "Because we don't know each other.");
				static const char* const k[] = {
					"Nie znamy sie, a ja mam swoje sprawy.", "Sorki, nie chodze do obcych bez powodu." };
				static const char* const kEn[] = {
					"We don't know each other, and I've got my own stuff to do.", "Sorry, I don't go to strangers for no reason." };
				return PBC_SAY2(g, k, kEn);
			}
			// The question belongs to this reply and outranks an "a ty?" another
			// line of the same batch may have put there: without it the answer
			// is not read as one, and a stranger with no reason must not be
			// walked to by default.
			g.askBack = Txt(g, "Po co mam przyjsc?", "What for?");
			g.askBackKind = ASK_SUMMON;
			g.askBackTopic = T_NONE;
			return Txt(g, "Hm, nie znamy sie.", "Hm, we don't know each other.");
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
			static const char* const kEn[] = {
				"Alright, back to my own stuff then.", "Ok, I'm off. See ya!", "Sure. Message me if you need anything." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Przeciez nigdzie za toba nie chodze :)", "Dobra, i tak mam swoje sprawy." };
		static const char* const kEn[] = { "I'm not following you anywhere though :)", "Alright, I've got my own stuff anyway." };
		return PBC_SAY2(g, k, kEn);
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
				static const char* const kEn[] = { "Just short on cash for now, like I said.", "Money, like I said. Saving up.",
					"Just gotta save up first.", "Saving up for now, like I said." };
				return PBC_SAY2(g, k, kEn);
			}
			static const char* const k[] = { "Ale na razie zostaje przy swojej.", "Jak trafie cos w normalnej cenie, to zmienie.",
				"Na razie ta mi wystarcza." };
			static const char* const kEn[] = { "But I'm sticking with mine for now.", "If I find something at a fair price, I'll switch.",
				"This one's enough for now." };
			return PBC_SAY2(g, k, kEn);
		}
		if (s.weaponName.empty())
			return Txt(g, "Na razie w ogole nie mam porzadnej broni, zbieram na cos.",
					"Don't really have a decent weapon yet, saving up for one.");
		if (s.goal == G_EQUIPMENT || s.marketTrip || s.action == A_MARKET)
			return Txt(g, "Wlasnie sie za czyms lepszym rozgladam.", "Looking around for something better right now.");
		if (s.weaponIsGoal)
			return Txt(g, "Na moj poziom lepszej za bardzo nie ma, sprawdzalem.", "There's nothing much better for my level, I checked.");
		if (saving)
			return g.tier >= TIER_KNOWN ? Fill(g, Txt(g, "Odkladam na nowa bron ($GOAL), ale kosztuje z $GOALPRICE, a mam $GOLD.",
					"Saving for a new weapon ($GOAL), but it costs like $GOALPRICE and I have $GOLD.")) :
					Fill(g, Txt(g, "Odkladam na nowa bron ($GOAL), ale jeszcze mnie nie stac.",
					"Saving for a new weapon ($GOAL), but can't afford it yet."));
		if (s.weaponOutclassed && !s.weaponGoal.empty())
			return Fill(g, Txt(g, "Celuje w nowa bron ($GOAL), tylko nikt jej nie wystawia w normalnej cenie.",
					"I'm aiming for a new weapon ($GOAL), but nobody sells it at a fair price."));
		if (s.weaponPlus >= 7)
			return Fill(g, Txt(g, "Moja ma +$WPLUS i jeszcze daje rade, szkoda mi jej.",
					"Mine's +$WPLUS and still does the job, don't wanna give it up."));
		if (s.gold < 1000000)
			return Txt(g, "Na lepsza mnie jeszcze nie stac.", "Can't afford a better one yet.");
		return Txt(g, "Jeszcze nie trafilem na nic lepszego w normalnej cenie.", "Haven't found anything better at a fair price yet.");
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
			static const char* const kEn[] = { "$ITEM? That's what I've got :)", "I already have $ITEM :)" };
			return SayWithItem(g, g.en ? kEn : k, 2, shown);
		}
		if (s.weaponOutclassed)
		{
			if (GearReasonSaidLately(g, CONV_FACT_TTL_MS))
			{
				static const char* const k[] = { "No mowie, na $ITEM zbieram :)", "Przeciez pisze, ze na $ITEM odkladam :)" };
				static const char* const kEn[] = { "That's what I'm saying, saving for $ITEM :)", "Told you, I'm saving up for $ITEM :)" };
				return SayWithItem(g, g.en ? kEn : k, 2, shown);
			}
			static const char* const k[] = { "$ITEM? No wlasnie na to zbieram!", "$ITEM? Na to wlasnie odkladam!" };
			static const char* const kEn[] = { "$ITEM? That's exactly what I'm saving for!", "$ITEM? That's what I'm saving up for!" };
			return SayWithItem(g, g.en ? kEn : k, 2, shown);
		}
		static const char* const k[] = { "$ITEM? Kiedys na pewno.", "$ITEM to by bylo cos, kiedys." };
		static const char* const kEn[] = { "$ITEM? Some day for sure.", "$ITEM would be something, some day." };
		return SayWithItem(g, g.en ? kEn : k, 2, shown);
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
			out = s.weaponPlus >= 7 ? Fill(g, Txt(g, "Moja ma juz +$WPLUS, dalej to juz loteria u kowala.",
					"Mine's already +$WPLUS, past that it's a lottery at the Blacksmith.")) :
					std::string(Txt(g, "Ulepszam, jak mam materialy i kase na kowala.",
					"I upgrade when I have the materials and the cash for the Blacksmith."));
			g.reason = out;
			return out;
		}
		// "but it has average damage": the point granted.
		const bool but = a && (a->tokens.Has("przeciez") || a->tokens.Has("ale") ||
				(a->tokens.english && (a->tokens.Has("but") || a->tokens.Has("though") || a->tokens.Has("still"))));
		if (a && a->levelNamed > 0 && s.weaponLevel > 0 &&
				(a->levelNamed + 5 < s.weaponLevel || a->levelNamed > s.weaponLevel + 5))
			out = Fill(g, Txt(g, "To bron na $WLVL poziom, nie na ", "It's a weapon for level $WLVL, not ")) +
					ToString((long long)a->levelNamed) + " :)";
		else if (but && a->concepts.Has(C_BONUS))
		{
			static const char* const k[] = { "No ma, nie przecze.", "Racja, srednie robia robote.", "Wiem, wiem." };
			static const char* const kEn[] = { "It does, not denying that.", "True, average damage does the job.", "I know, I know." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (s.weaponLevel > 0 && s.weaponLevel + 15 <= s.level)
		{
			static const char* const k[] = { "Wiem, stara jest.", "No wiem, juz troche odstaje.", "Tak, dawno jej nie zmienialem." };
			static const char* const kEn[] = { "I know, it's old.", "Yeah I know, it's falling behind a bit.",
				"Yeah, haven't changed it in a while." };
			out = PBC_SAY2(g, k, kEn);
		}
		if (!shown.empty())
		{
			if (LineNamesGoal(g))
				Append(out, GoalNamedReaction(g, shown));
			else
			{
				static const char* const k[] = { "$ITEM? Dobry pomysl.", "$ITEM? Tez o tym myslalem.", "$ITEM to niezly wybor." };
				static const char* const kEn[] = { "$ITEM? Good idea.", "$ITEM? I was thinking about that too.", "$ITEM is a solid pick." };
				Append(out, SayWithItem(g, g.en ? kEn : k, 3, shown));
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
			static const char* const kEn[] = { "I know, damage matters.", "Yeah, won't get far without damage.",
				"You're right, it'd go faster with a better weapon." };
			out = PBC_SAY2(g, k, kEn);
		}
		else if (!shown.empty())
		{
			if (LineNamesGoal(g))
				out = GoalNamedReaction(g, shown);
			else
			{
				static const char* const k[] = { "$ITEM? Moze to dobry pomysl.", "$ITEM? Pomysle o tym." };
				static const char* const kEn[] = { "$ITEM? Might be a good idea.", "$ITEM? I'll think about it." };
				out = SayWithItem(g, g.en ? kEn : k, 2, shown);
			}
		}
		else
		{
			static const char* const k[] = { "Moze masz racje.", "Pomysle o tym.", "Wiem, przydaloby sie cos lepszego." };
			static const char* const kEn[] = { "Maybe you're right.", "I'll think about it.", "I know, something better would help." };
			out = PBC_SAY2(g, k, kEn);
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
				static const char* const kEn[] = { "Depends on your class and level, but one with average damage always pays off.",
					"For your level? Get something with average damage, it always pays off." };
				return PBC_SAY2(g, k, kEn);
			}
			if (s.weaponIsGoal && !s.weaponName.empty())
				return Fill(g, Txt(g, "Na moj poziom chyba ta, ktora mam: $WEAPON.", "For my level probably the one I have: $WEAPON."));
			if (!s.weaponGoal.empty())
				return Fill(g, Txt(g, "Na moj poziom chyba $GOAL. Na nia odkladam.", "For my level probably $GOAL. Saving up for it."));
			static const char* const k[] = { "Zalezy od poziomu i klasy. Ja bym bral cos ze srednimi.",
				"Kazda ma swoje plusy. Byle ze srednimi.", "Zalezy, do czego. Na expa te ze srednimi." };
			static const char* const kEn[] = { "Depends on level and class. I'd go for something with average damage.",
				"Each has its pros. As long as it has average damage.", "Depends what for. For grinding, the ones with average damage." };
			return PBC_SAY2(g, k, kEn);
		}
		std::string out;
		if (a && a->concepts.Has(C_BONUS))
		{
			static const char* const k[] = { "Srednie to podstawa na expie, wszystko szybciej schodzi.",
				"Bron ze srednimi to swietna sprawa, tylko dobre sa drogie.", "Bez srednich ani rusz, to wiem." };
			static const char* const kEn[] = { "Average damage is key for grinding, everything dies faster.",
				"A weapon with average damage is great, the good ones are just pricey.", "Can't do without average damage, I know that." };
			out = PBC_SAY2(g, k, kEn);
			if (!shown.empty())
			{
				if (!LineNamesGoal(g))
					Append(out, shown + Txt(g, " to solidna sprawa.", " is solid stuff."));
				else if (s.weaponIsGoal)
					Append(out, Txt(g, "Sam taka mam.", "I've got one myself."));
				else if (s.weaponOutclassed)
					Append(out, g.en ? "I'm saving for " + shown + " myself." : "Sam na " + shown + " odkladam.");
				else
					Append(out, shown + Txt(g, " to by bylo cos.", " would be something."));
			}
		}
		else if (!shown.empty())
		{
			static const char* const k[] = { "$ITEM? Z dobrymi srednimi to marzenie :)", "$ITEM to solidna sprawa." };
			static const char* const kEn[] = { "$ITEM? With good average damage it's a dream :)", "$ITEM is solid stuff." };
			out = SayWithItem(g, g.en ? kEn : k, 2, shown);
		}
		else
		{
			static const char* const k[] = { "Dobra bron to podstawa, reszta to dodatki.", "Liczy sie bron i bonusy, jak dla mnie." };
			static const char* const kEn[] = { "A good weapon is the base, the rest is extras.", "Weapon and bonuses are what count, for me." };
			out = PBC_SAY2(g, k, kEn);
		}
		return out;
	}

	// "a jakbym ci dal riba +9 ze srednimi, wymienilbys?" - of course.
	inline std::string GenGiftOffer(TGen& g)
	{
		if (g.tier == TIER_HOSTILE)
			return Txt(g, "Od ciebie? Watpie :P", "From you? Doubt it :P");
		const std::string shown = ShownItem(g);
		const bool swap = g.a && g.a->concepts.Has(C_SWAP);
		if (!shown.empty())
		{
			if (swap)
			{
				static const char* const k[] = { "Jasne, ze bym wymienil! $ITEM to by bylo cos :D", "Od razu bym wymienil, $ITEM to by bylo cos." };
				static const char* const kEn[] = { "Of course I'd swap! $ITEM would be something :D", "I'd swap right away, $ITEM would be something." };
				return SayWithItem(g, g.en ? kEn : k, 2, shown);
			}
			static const char* const k[] = { "$ITEM? Bralbym w ciemno :D", "$ITEM? Jasne, ze tak! Od razu bym zalozyl.",
				"Pewnie! $ITEM to by bylo cos." };
			static const char* const kEn[] = { "$ITEM? I'd take it in a heartbeat :D", "$ITEM? Of course! I'd put it on right away.",
				"Sure! $ITEM would be something." };
			return SayWithItem(g, g.en ? kEn : k, 3, shown);
		}
		if (swap)
		{
			static const char* const k[] = { "Jasne, ze bym wymienil!", "Od razu bym wymienil :D" };
			static const char* const kEn[] = { "Of course I'd swap!", "I'd swap right away :D" };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Pewnie, ze tak!", "Bralbym bez zastanowienia :D", "No ba! Kto by nie wzial." };
		static const char* const kEn[] = { "Sure thing!", "I'd take it without thinking :D", "Obviously! Who wouldn't." };
		return PBC_SAY2(g, k, kEn);
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
			out = Txt(g, "Szkoda, ze mnie nie bylo!", "Too bad I wasn't there!");
		// Shown again a moment later: seen it, and said so, not the same
		// admiration twice.
		if (g.m.lastAnswered == I_SHOW_ITEM && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
		{
			static const char* const k[] = { "No widze, widze :) Piekna sztuka.", "Juz widzialem, szacun :D", "No, robi wrazenie." };
			static const char* const kEn[] = { "I see it, I see it :) Beautiful piece.", "Already saw it, respect :D", "Yeah, that's impressive." };
			Append(out, PBC_SAY2(g, k, kEn));
			return out;
		}
		if (shown.empty())
		{
			static const char* const k[] = { "O, ladne. Gratki!", "Fajne, gratki!", "No no, niezle." };
			static const char* const kEn[] = { "Oh, nice. Grats!", "Cool, grats!", "Wow, not bad." };
			Append(out, PBC_SAY2(g, k, kEn));
			return out;
		}
		if (plus >= 8)
		{
			static const char* const k[] = { "Ale sztuka! $ITEM, szacun.", "O kurcze, $ITEM! Zazdroszcze.", "No no, $ITEM. Ile w to wlozyles?" };
			static const char* const kEn[] = { "What a piece! $ITEM, respect.", "Oh wow, $ITEM! Jealous.", "Wow, $ITEM. How much did you put into that?" };
			Append(out, SayWithItem(g, g.en ? kEn : k, 3, shown));
		}
		else if (plus >= 5)
		{
			static const char* const k[] = { "Niezle! $ITEM to juz cos.", "O, $ITEM. Ladnie." };
			static const char* const kEn[] = { "Nice! $ITEM is already something.", "Oh, $ITEM. Nice." };
			Append(out, SayWithItem(g, g.en ? kEn : k, 2, shown));
		}
		else
		{
			static const char* const k[] = { "O, $ITEM. Ladne.", "Fajne, gratki!" };
			static const char* const kEn[] = { "Oh, $ITEM. Nice.", "Cool, grats!" };
			Append(out, SayWithItem(g, g.en ? kEn : k, 2, shown));
		}
		if (LineNamesGoal(g) && !g.s.weaponIsGoal && g.s.weaponOutclassed)
			Append(out, Txt(g, "Sam na taka odkladam.", "I'm saving for one myself."));
		return out;
	}

	// ------------------------------------------- a person talking at the bot

	// "przestan do mnie pisac": said once, and the bot keeps to it
	// (TConvMemory::quietUntil keeps it from starting anything).
	inline std::string GenStopTalking(TGen& g)
	{
		const bool released = ReleaseSummonFor(g);
		static const char* const k[] = { "Dobra, juz nie pisze.", "Ok, nie przeszkadzam.", "Spoko, juz daje spokoj." };
		static const char* const kEn[] = { "Alright, I'll stop writing.", "Ok, won't bother you.", "Sure, I'll leave you be." };
		std::string out = PBC_SAY2(g, k, kEn);
		if (released)
			Append(out, Txt(g, "Wracam do swoich spraw.", "Back to my own stuff."));
		return out;
	}

	// "bana ci daje": a question back about what for - and after a sum it had
	// just got right, that sum.
	inline std::string GenThreat(TGen& g)
	{
		if (g.m.lastAnswered == I_MATH && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
			return Txt(g, "Za co? Przeciez dobrze policzylem :P", "For what? I got the math right :P");
		if (g.m.negative >= 3)
			return Txt(g, "Rob, co chcesz.", "Do whatever you want.");
		static const char* const k[] = { "Za co? Przeciez nic ci nie zrobilem.", "Hej, spokojnie, za co od razu ban?",
			"Ban? A za co, za pisanie? :(" };
		static const char* const kEn[] = { "For what? I didn't do anything to you.", "Hey, easy, why a ban right away?",
			"A ban? For what, for talking? :(" };
		return PBC_SAY2(g, k, kEn);
	}

	// Banter answered as banter: "bieda", "zawijaj stad", "tyle jestes
	// warta", "daleko w zyciu zajdziesz". After "przestan do mnie pisac" or
	// from somebody hostile, only a short "jak uwazasz".
	inline std::string GenMock(TGen& g)
	{
		const TAnalysis* a = g.a;
		if (a && a->answeredAsk == ASK_JOIN)
			return Txt(g, "Haha, dobra, to sam sobie pobije :P", "Haha, fine, I'll just solo it then :P");
		if (IsQuiet(g.m, g.now) || g.tier == TIER_HOSTILE)
		{
			static const char* const k[] = { "Jak uwazasz.", "Niech ci bedzie.", "Ok." };
			static const char* const kEn[] = { "Whatever you say.", "Have it your way.", "Ok." };
			return PBC_SAY2(g, k, kEn);
		}
		bool leave = false, worth = false, future = false, poor = false, weak = false;
		if (a)
		{
			const TTokens& t = a->tokens;
			for (size_t i = 0; i < t.words.size(); ++i)
			{
				const std::string& w = t.words[i];
				if (w == "zawijaj" || w == "zawijajcie" || w == "wypad" || w == "spadaj")
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
				// The same banter in English, read only in an English line so a
				// Polish one is answered as it always was.
				if (!t.english)
					continue;
				const std::string prev = i > 0 ? t.words[i - 1] : std::string();
				if (w == "scram" || w == "gtfo" || w == "shoo" || w == "begone" || (w == "lost" && prev == "get") ||
						(w == "away" && prev == "go") || (w == "off" && (prev == "buzz" || prev == "piss" || prev == "back")))
					leave = true;
				if (w == "worth" || w == "worthless")
					worth = true;
				if (w == "far" && t.Has("life"))
					future = true;
				if (w == "poor" || w == "broke" || w == "brokie" || w == "beggar" || w == "poverty")
					poor = true;
				if (w == "weak" || w == "weakling" || w == "pathetic" || w == "cringe" || w == "lame" || w == "trash")
					weak = true;
			}
		}
		if (leave)
		{
			static const char* const k[] = { "Haha, dobra, juz sie zwijam :P", "Dobra, dobra, juz mnie nie ma :D" };
			static const char* const kEn[] = { "Haha, ok, I'm out :P", "Alright, alright, I'm gone :D" };
			return PBC_SAY2(g, k, kEn);
		}
		if (worth)
		{
			static const char* const k[] = { "Moze i niewiele, ale uczciwie zarobione :P", "Auc. Ale sie nie poddaje :P" };
			static const char* const kEn[] = { "Maybe not much, but it's honestly earned :P", "Ouch. But I'm not giving up :P" };
			return PBC_SAY2(g, k, kEn);
		}
		if (future)
		{
			static const char* const k[] = { "Hehe, na razie zajde do nastepnego poziomu.", "Krok po kroku, jak na expie :P" };
			static const char* const kEn[] = { "Hehe, for now I'll go as far as the next level.", "Step by step, like grinding :P" };
			return PBC_SAY2(g, k, kEn);
		}
		if (poor)
		{
			static const char* const k[] = { "Kazdy kiedys zaczynal :P", "Bieda, ale uczciwa.", "Spokojnie, jeszcze sie odkuje." };
			static const char* const kEn[] = { "Everyone starts somewhere :P", "Broke, but honest.", "Relax, I'll bounce back." };
			return PBC_SAY2(g, k, kEn);
		}
		if (weak)
		{
			static const char* const k[] = { "Kazdy jest slaby na poczatku :P", "Jeszcze zobaczysz :P" };
			static const char* const kEn[] = { "Everyone's weak at the start :P", "You'll see :P" };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Haha, dobra, dobra.", "Hehe, jak tam chcesz :P" };
		static const char* const kEn[] = { "Haha, ok, ok.", "Hehe, whatever you say :P" };
		return PBC_SAY2(g, k, kEn);
	}

	// "ile to 2+2" - the number, the way a person writes it: "2,5" to a
	// Polish reader, "2.5" to an English one.
	inline std::string GenMath(TGen& g)
	{
		const TAnalysis* a = g.a;
		if (!a)
			return "Hm?";
		if (a->mathDivZero)
			return Txt(g, "Przez zero sie nie dzieli :P", "You can't divide by zero :P");
		if (a->mathTooBig || a->mathText.empty())
			return Txt(g, "Za duze liczby jak na moja glowe :D", "Numbers too big for my head :D");
		const std::string r = g.en && !a->mathTextEn.empty() ? a->mathTextEn : a->mathText;
		if (r == "4" && a->tokens.norm.find("2 +2") != std::string::npos)
			return Txt(g, "4. To akurat wiem :D", "4. That one I know :D");
		if (a->mathMixed)
			return r + Txt(g, ". Najpierw mnozenie :P", ". Multiplication first :P");
		static const char* const k[] = { "$R.", "Wychodzi $R.", "$R :)", "Hmm... $R." };
		static const char* const kEn[] = { "$R.", "It's $R.", "$R :)", "Hmm... $R." };
		std::string out = Pick(g, g.en ? kEn : k, 4);
		ReplaceAll(out, "$R", r);
		return out;
	}

	// "to powiedziales mi, ze w Joan": it did say so - and moved since.
	inline std::string GenContradiction(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		const long mentioned = MentionedMap(g);
		if (!IsKnownMap(s.mapIndex))
			return Txt(g, "Nie klamie :) Po prostu sie przenioslem.", "I'm not lying :) I just moved.");
		g.saidMap = true;
		if (mentioned > 0 && mentioned != s.mapIndex && IsKnownMap(mentioned))
		{
			if (SaidMapLately(g, mentioned))
			{
				static const char* const k[] = { "Bo wtedy bylem $WASAT :) Teraz jestem juz $MAPIN.",
					"Bylem $WASAT, ale juz sie przenioslem - teraz jestem $MAPIN." };
				static const char* const kEn[] = { "Because back then I was $WASAT :) Now I'm $MAPIN.",
					"I was $WASAT, but I moved - now I'm $MAPIN." };
				return WithOldMap(g, Pick(g, g.en ? kEn : k, 2), mentioned);
			}
			return Fill(g, Txt(g, "Nie, jestem $MAPIN. Moze cos ci sie pomylilo?", "No, I'm $MAPIN. Maybe you mixed something up?"));
		}
		if (mentioned == s.mapIndex)
			return Fill(g, Txt(g, "No tak, dalej jestem $MAPIN.", "Yeah, still $MAPIN."));
		long old = RecentOtherMap(g);
		if (!old && g.m.prevSaidMap != 0 && g.m.prevSaidMap != s.mapIndex && IsKnownMap(g.m.prevSaidMap) &&
				SaidMapLately(g, g.m.prevSaidMap))
			old = g.m.prevSaidMap;
		if (old)
			return WithOldMap(g, Txt(g, "Wczesniej bylem $WASAT, teraz jestem juz $MAPIN.", "I was $WASAT before, now I'm $MAPIN."), old);
		static const char* const k[] = { "Nie klamie :) Moze sie zle wyrazilem.", "Hm, chyba sie nie zrozumielismy." };
		static const char* const kEn[] = { "I'm not lying :) Maybe I put it badly.", "Hm, I think we misunderstood each other." };
		return PBC_SAY2(g, k, kEn);
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
				return Fill(g, Txt(g, "Bo $MAPNAME lepiej pasuje na moj poziom.", "Because $MAPNAME suits my level better."));
			return WithOldMap(g, Txt(g, "Wlasnie tak zrobilem - bylem $WASAT, a teraz jestem juz $MAPIN.",
					"That's what I did - I was $WASAT, now I'm $MAPIN."), old);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
			return Fill(g, why ? Txt(g, "Bo tam jest lepszy exp. Wlasnie ide $DEST.", "Better exp there. Heading $DEST right now.") :
					Txt(g, "Wlasnie ide $DEST.", "Heading $DEST right now."));
		if (s.inTown)
			return why ? Txt(g, "Bo mam sprawy w miescie.", "Because I've got stuff to do in town.") :
					Txt(g, "Pewnie tak. Jak zalatwie sprawy w miescie, to sie przeniose.", "Probably. Once I'm done in town, I'll move.");
		if (why)
			return Txt(g, "Bo tu jest dobry exp na moj poziom.", "Because the exp here is good for my level.");
		static const char* const k[] = { "Tu mi pasuje, exp leci na moj poziom.", "Moze i tak. Zobacze, jak wbije pare poziomow." };
		static const char* const kEn[] = { "It works for me here, good exp for my level.", "Maybe. I'll see once I get a few levels." };
		return PBC_SAY2(g, k, kEn);
	}

	// ------------------------------------------------------------- follow-ups

	inline std::string GenerateOne(TGen& g, const TAnalysis& a);

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
					static const char* const kWhyEn[V_COUNT] = {
						"Good exp here for my level.", "Just ended up here, and it's quiet.",
						"These mobs drop stuff that sells.", "I like fighting, and there's plenty to fight here.",
						"There's always someone around here." };
					return g.en ? kWhyEn[g.voice] : kWhy[g.voice];
				}
				if (g.s.inTown && g.s.bagCells > 0 && g.s.freeCells <= 5)
					return Txt(g, "Bo mialem pelne EQ i trzeba bylo sprzedac.", "My bag was full, had to sell.");
				if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
					return Txt(g, "Bo mialem malo HP.", "My HP was low.");
				return Txt(g, "Tak wyszlo. Nie wszystko trzeba planowac.", "Just happened. Not everything needs a plan.");
			case I_PARTY:
				return g.s.inParty ? Txt(g, "Bo razem sie lepiej expi.", "Grinding's better together.") :
						(g.voice == V_GRINDER ? Txt(g, "Bo sam szybciej expie.", "I level faster solo.") :
						Txt(g, "Bo nikt mnie nie zaprosil :)", "Nobody invited me :)"));
			case I_GUILD:
				return g.s.inGuild ? Txt(g, "Bo sa tam fajni ludzie.", "The people there are cool.") :
						Txt(g, "Jakos nie trafilem na odpowiednia.", "Haven't found the right one yet.");
			case I_REST: case I_HP:
				return Txt(g, "Bo mnie moby porzadnie obily.", "The mobs beat me up pretty good.");
			case I_GOAL: case I_NEXT_PLAN:
			{
				static const char* const kWhy[V_COUNT] = {
					"Bo chce byc mocniejszy.", "Bo chce zobaczyc, co jest dalej.", "Bo to sie oplaci.",
					"Bo lubie wyzwania.", "Bo wtedy moge wiecej pomoc ekipie." };
				static const char* const kWhyEn[V_COUNT] = {
					"I want to get stronger.", "I wanna see what's further on.", "It pays off.",
					"I like a challenge.", "Then I can help the team more." };
				return g.en ? kWhyEn[g.voice] : kWhy[g.voice];
			}
			case I_GENERAL:
			{
				const TTopicPack* pack = TopicPackFor(g, g.m.lastTopic);
				if (pack)
					return PickField(g, pack->why, 2);
				break;
			}
			default:
				break;
		}
		static const char* const k[] = { "Tak wyszlo.", "Po prostu tak.", "Dobre pytanie. Tak jakos." };
		static const char* const kEn[] = { "Just happened.", "Just because.", "Good question. It just worked out that way." };
		return PBC_SAY2(g, k, kEn);
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
				static const char* const kEn[] = { "Seriously.", "I'm telling you.", "Really.", "Yeah, for real." };
				std::string out = PBC_SAY2(g, k, kEn);
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
					case I_PARTY_REQUEST: return Txt(g, "Po prostu zapros mnie do grupy.", "Just invite me to your party.");
					case I_ACTIVITY: case I_ACTIVITY_LOCATION:
						return Txt(g, "Normalnie, bije i zbieram drop.", "The usual, I kill stuff and grab the drops.");
					case I_GOLD:
						return g.voice == V_MERCHANT ? Txt(g, "Handel. Kupic tanio, sprzedac drozej.", "Trading. Buy low, sell high.") :
								Txt(g, "Drop i troche handlu.", "Drops and a bit of trading.");
					case I_LEVEL: return Txt(g, "Expem, jak kazdy.", "Grinding, like everyone.");
					case I_HOW_ARE_YOU: return GenHowAreYou(g);
					default: return Txt(g, "Normalnie, po swojemu.", "Just my own way.");
				}
			case F_WHEN:
				if (a.subject == I_NEXT_PLAN || a.subject == I_TRAVEL || a.subject == I_REST || a.subject == I_GOAL)
				{
					static const char* const k[] = { "Pewnie za chwile.", "Niedlugo, zobaczymy." };
					static const char* const kEn[] = { "Probably in a bit.", "Soon, we'll see." };
					return PBC_SAY2(g, k, kEn);
				}
				return Txt(g, "Nie wiem dokladnie, zobaczymy.", "Not sure exactly, we'll see.");
			case F_NEXT:
			{
				const TTopicPack* pack = TopicPackFor(g, g.m.lastTopic);
				if (pack && g.askBack.empty())
				{
					const std::string q = PickField(g, pack->ask, 2);
					if (!q.empty())
					{
						g.askBackKind = ASK_TOPIC;
						g.askBackTopic = (ETopic)pack->topic;
						return Txt(g, "No i tyle :) ", "That's about it :) ") + q;
					}
				}
				return Txt(g, "No i tyle :)", "That's about it :)");
			}
			case F_COUNT:
				return Txt(g, "Ale czego?", "Of what?");
			case F_THIS:
				return Txt(g, "Ale ktory?", "Which one?");
			case F_WHO:
				return Txt(g, "Kto? O kim mowisz?", "Who? Who are you talking about?");
			case F_WHAT:
			default:
			{
				static const char* const k[] = { "Co masz na mysli?", "Ale co dokladnie?", "Hm?" };
				static const char* const kEn[] = { "What do you mean?", "What exactly?", "Hm?" };
				return PBC_SAY2(g, k, kEn);
			}
		}
	}

	inline std::string GenAnswerToBot(TGen& g, const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
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
					static const char* const kEn[] = { "Aw, sorry to hear. It'll get better.", "That sucks... Hang in there." };
					return PBC_SAY2(g, k, kEn);
				}
				if (yes || c.Has(C_POSITIVE))
				{
					static const char* const k[] = { "To dobrze!", "Super.", "No i git." };
					static const char* const kEn[] = { "Good to hear!", "Great.", "Nice." };
					return PBC_SAY2(g, k, kEn);
				}
				return Txt(g, "No, to jak u mnie.", "Same as me, then.");
			case ASK_ACTIVITY:
				if (c.Has(C_EXP) || c.Has(C_HIT) || c.Has(C_MOB))
					return Txt(g, "O, to powodzenia na expie!", "Oh, good luck with the grind!");
				if (c.Has(C_FISH))
					return Txt(g, "Lowienie to relaks. Powodzenia.", "Fishing's relaxing. Good luck.");
				if (c.Has(C_TRADE) || c.Has(C_SHOP))
					return Txt(g, "Handel to dobra rzecz. Obys dobrze sprzedal.", "Trading's a good thing. Hope you sell well.");
				if (a.tokens.Has("nic") || (a.tokens.english && (a.tokens.Has("nothing") || a.tokens.Has("nothin"))))
					return Txt(g, "Tez czasem tak mam.", "I have days like that too.");
				{
					static const char* const k[] = { "Aha, rozumiem.", "No to spoko.", "Tez fajnie." };
					static const char* const kEn[] = { "Ah, I see.", "Cool then.", "Nice too." };
					return PBC_SAY2(g, k, kEn);
				}
			case ASK_JOIN:
			{
				// "teraz to najwyzej mozesz mi zbic konia" is a no with a joke in
				// it; asking "to jak, zapraszasz?" after it was not listening.
				const TTokens& t = a.tokens;
				const bool softNo = t.Has("najwyzej") || t.Has("raczej") || t.Has("potem") || t.Has("innym") ||
						t.Has("zajety") || t.Has("zajeta") || t.Has("sorry") || t.Has("sory") || t.Has("sorki") ||
						(t.english && (t.Has("later") || t.Has("busy") || t.Has("sry") || t.Has("another") ||
						t.Has("rather") || t.Has("maybe")));
				if (yes && !no && !softNo)
					return Txt(g, "To zapros mnie do PT!", "Then send me a party invite!");
				if (no || softNo)
				{
					static const char* const k[] = { "No trudno, moze innym razem.", "Dobra, innym razem :)" };
					static const char* const kEn[] = { "Oh well, maybe another time.", "Ok, another time :)" };
					return PBC_SAY2(g, k, kEn);
				}
				static const char* const k[] = { "Haha, no dobra, to innym razem :)", "Dobra, jakbys zmienil zdanie, to pisz." };
				static const char* const kEn[] = { "Haha, alright, another time then :)", "Ok, message me if you change your mind." };
				return PBC_SAY2(g, k, kEn);
			}
			case ASK_FOUND:
				if (yes || c.Has(C_POSITIVE))
					return Txt(g, "O, gratki!", "Oh, grats!");
				return Txt(g, "Nastepnym razem sie uda.", "Better luck next time.");
			case ASK_SUMMON:
				// "Po co mam przyjsc?" - a reason is what was asked for.
				if (SummonHasReason(a))
					return SummonGo(g);
				if (no)
					return Txt(g, "No to zostaje przy swoim.", "Then I'll stick to my own stuff.");
				return Txt(g, "Hm, to jednak zostane przy swoim.", "Hm, I'll stick to my own stuff after all.");
			case ASK_TOPIC:
			default:
			{
				const TTopicPack* pack = TopicPackFor(g, g.m.botAskTopic);
				if (yes && !no)
				{
					static const char* const k[] = { "No to mamy cos wspolnego.", "O, fajnie.", "Tez tak mam." };
					static const char* const kEn[] = { "So we have something in common.", "Oh, cool.", "Same here." };
					return PBC_SAY2(g, k, kEn);
				}
				if (no && !yes)
				{
					static const char* const k[] = { "A widzisz, kazdy ma inaczej.", "Aha, rozumiem." };
					static const char* const kEn[] = { "See, everyone's different.", "Ah, I see." };
					return PBC_SAY2(g, k, kEn);
				}
				if (pack)
					return PickField(g, pack->react, 4);
				static const char* const k[] = { "Aha, rozumiem.", "No, jasne.", "Mhm, rozumiem." };
				static const char* const kEn[] = { "Ah, I see.", "Yeah, sure.", "Mhm, got it." };
				return PBC_SAY2(g, k, kEn);
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
				if (g.rng.Chance(g.Bad() ? 70 : 40))
					return std::string();
				static const char* const k[] = { "Hehe", "xD", "Haha", ":D" };
				static const char* const kEn[] = { "Hehe", "xD", "Haha", "lol" };
				return PBC_SAY2(g, k, kEn);
			}
			case I_ACK:
			{
				if (g.rng.Chance(g.Bad() ? 75 : 55))
					return std::string();
				static const char* const k[] = { "No.", "Mhm.", "No wlasnie.", "Dokladnie." };
				static const char* const kEn[] = { "Yeah.", "Mhm.", "Right.", "Exactly." };
				return PBC_SAY2(g, k, kEn);
			}
			case I_YES:
			{
				if (g.rng.Chance(50))
					return std::string();
				static const char* const k[] = { "No dobra.", "Ok.", "Tez tak mysle." };
				static const char* const kEn[] = { "Alright.", "Ok.", "I think so too." };
				return PBC_SAY2(g, k, kEn);
			}
			default:
			{
				if (g.rng.Chance(50))
					return std::string();
				static const char* const k[] = { "Aha.", "No dobra.", "Ok, rozumiem." };
				static const char* const kEn[] = { "Ah.", "Alright.", "Ok, got it." };
				return PBC_SAY2(g, k, kEn);
			}
		}
	}

	// A question nothing understood. Twice in a row it stops pretending and
	// says what it can talk about.
	inline std::string GenUnknownQuestion(TGen& g, const TAnalysis& a)
	{
		if (g.m.fallbackStreak >= 1)
		{
			static const char* const k[] = { "Chyba sie nie rozumiemy :) Zapytaj mnie o exp, sprzet albo mape.",
				"Nie lapie, o co chodzi. Zapytaj jakos inaczej?" };
			static const char* const kEn[] = { "I don't think we're getting each other :) Ask me about exp, gear or the map.",
				"Not sure what you mean. Can you ask some other way?" };
			return PBC_SAY2(g, k, kEn);
		}
		if (a.concepts.Has(C_YOU))
		{
			static const char* const k[] = { "Hm, nie bardzo rozumiem, o co pytasz. Mozesz inaczej?", "A czemu pytasz? :)" };
			static const char* const kEn[] = { "Hm, not sure what you're asking. Say it another way?", "Why do you ask? :)" };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const kSteer[] = {
			"Dobre pytanie. Sam nie wiem.", "Nie wiem, nigdy sie nad tym nie zastanawialem.", "Nie mam pojecia, szczerze." };
		static const char* const kSteerEn[] = {
			"Good question. No idea myself.", "Dunno, never really thought about it.", "No idea, honestly." };
		std::string out = PBC_SAY2(g, kSteer, kSteerEn);
		if (g.rng.Chance(25) && g.askBack.empty())
		{
			g.askBack = Txt(g, "A czemu pytasz?", "Why do you ask?");
			g.askBackKind = ASK_NONE;
		}
		return out;
	}

	inline std::string GenUnknownStatement(TGen& g, const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		if (c.Has(C_POSITIVE))
		{
			static const char* const k[] = { "O, fajnie!", "Gratki!", "No to super." };
			static const char* const kEn[] = { "Oh, nice!", "Grats!", "That's great." };
			return PBC_SAY2(g, k, kEn);
		}
		if (c.Has(C_NEGATIVE))
		{
			static const char* const k[] = { "Oj, szkoda.", "Bywa... Nastepnym razem bedzie lepiej.", "Kiepsko." };
			static const char* const kEn[] = { "Aw, too bad.", "It happens... Next time will be better.", "That sucks." };
			return PBC_SAY2(g, k, kEn);
		}
		if ((a.tokens.Has("tez") && a.tokens.Has("ja")) ||
				(a.tokens.english && ((a.tokens.Has("me") && a.tokens.Has("too")) || a.tokens.Has("same"))))
			return Txt(g, "No to tak jak ja.", "Same as me then.");
		if (c.Has(C_EXP) && c.Has(C_ME))
			return Txt(g, "O, to powodzenia na expie.", "Oh, good luck with the grind.");
		// Two lines in a row nothing understood: say what the bot is doing,
		// something the person can pick up, instead of another "aha".
		if (g.m.fallbackStreak >= 1)
		{
			std::string out = Txt(g, "Aha.", "Ah.");
			Append(out, ActivityClause(g, !g.saidMap));
			g.saidActivity = true;
			return out;
		}
		// About the bot itself: a shrug with a smile, not "ciekawe".
		if (c.Has(C_YOU))
		{
			static const char* const k[] = { "Moze troche :P", "Tak myslisz? :)", "Hehe, moze." };
			static const char* const kEn[] = { "Maybe a little :P", "You think so? :)", "Hehe, maybe." };
			return PBC_SAY2(g, k, kEn);
		}
		static const char* const k[] = { "Aha, rozumiem.", "Mhm, jasne.", "No, rozumiem.", "Jasne." };
		static const char* const kEn[] = { "Ah, I see.", "Mhm, sure.", "Yeah, I get it.", "Sure." };
		std::string out = PBC_SAY2(g, k, kEn);
		if (!g.Bad() && g.askBack.empty() && g.rng.Chance(g.voice == V_SOCIAL ? 40 : 20))
		{
			// The first of each pair asks how the person is, the second what
			// they are doing.
			static const char* const kq[] = { "A co u ciebie?", "A ty co teraz robisz?" };
			static const char* const kqEn[] = { "How about you?", "What are you up to right now?" };
			g.askBack = PBC_SAY2(g, kq, kqEn);
			g.askBackKind = g.askBack == kq[0] || g.askBack == kqEn[0] ? ASK_HOW_ARE_YOU : ASK_ACTIVITY;
		}
		return out;
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
					Append(out, Txt(g, "Wracam do swoich spraw.", "Back to my own stuff."));
				break;
			case I_THANKS:
			{
				if (ReleaseSummonFor(g))
				{
					static const char* const k[] = {
						"Nie ma sprawy! To wracam do swoich spraw.", "Spoko, to ja lece do swoich spraw." };
					static const char* const kEn[] = {
						"No problem! I'll get back to my own stuff then.", "Sure, I'm off to do my own thing then." };
					out = PBC_SAY2(g, k, kEn);
					break;
				}
				static const char* const k[] = { "Nie ma sprawy.", "Spoko.", "Nie ma za co.", "Luz." };
				static const char* const kEn[] = { "No problem.", "No worries.", "You're welcome.", "Np." };
				out = PBC_SAY2(g, k, kEn);
				break;
			}
			case I_APOLOGY:
				out = g.m.negative > 0 ? Txt(g, "No dobra, zapomnijmy.", "Alright, let's forget it.") :
						Txt(g, "Spoko, nic sie nie stalo.", "No worries, it's fine.");
				break;
			case I_HOW_ARE_YOU: out = GenHowAreYou(g); break;
			case I_HELP: out = GenHelp(g); break;
			case I_IS_BOT: out = GenIsBot(g); break;
			case I_INSULT:
				out = GenInsult(g);
				if (ReleaseSummonFor(g))
					Append(out, Txt(g, "Radz sobie sam.", "You're on your own."));
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
