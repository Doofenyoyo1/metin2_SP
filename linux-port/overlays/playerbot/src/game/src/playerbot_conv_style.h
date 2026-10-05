#ifndef __INC_METIN2_PLAYERBOT_CONV_STYLE_H__
#define __INC_METIN2_PLAYERBOT_CONV_STYLE_H__

// MT2009_PLUS_BOT_CHAT_V2 - how a bot types (pure, no engine types).
//
// The generators write tidy sentences ("Jestem w Joan. Expie wilki."). A
// person on the whisper does not: lowercase, no full stop at the end, a comma
// where a sentence ended, "nwm" for "nie wiem", now and then a letter swapped
// and fixed in the next line with a star ("*wilki"). Each bot has its own hand
// - seeded from its name, so the same bot types the same way every time -
// and the hand is put on a reply after it is composed (CConvEngine::ServePair),
// never on what the memory keeps.
//
// What is never touched: a word with a capital inside or a digit (a name, a
// map, an item: "Joan", "Chunjo", "Miecz Pelni Ksiezyca+5", "BezIsaGram"), the
// asker's and the bot's own names, a word that is part of a run of capitalised
// words (an item's name, which the engine turns into a link afterwards by
// matching it), and anything that a swap would turn into a swear word - a
// typo once made "Chunjo" into something else, which a person read as the bot
// swearing.
//
// How long the reply takes to type is here too (TypingDelayMs): a person
// reads the line, then types for as long as the answer is long.

#include "playerbot_conv_state.h"

namespace playerbot_conv
{
	// The hand of a bot, from its name: stable for that bot.
	struct TTypingHand
	{
		bool lowerStart;     // "jestem w Joan" rather than "Jestem w Joan"
		bool dropDot;        // no full stop at the end
		bool commaJoin;      // "jestem w Joan, expie" rather than two sentences
		int typoPercent;     // replies with one letter wrong
		int fixPercent;      // of those, followed by "*word"
		int slangPercent;    // "nie wiem" -> "nwm", "w ogole" -> "wgl"
		u32 perCharMs;       // typing speed
	};

	inline TTypingHand HandOf(const std::string& botName)
	{
		const u32 h = HashStr(botName.c_str(), 0xC45A1u);
		TTypingHand hand;
		hand.lowerStart = h % 100 < 70;
		hand.dropDot = (h >> 7) % 100 < 80;
		hand.commaJoin = hand.lowerStart && (h >> 14) % 100 < 55;
		hand.typoPercent = 2 + (int)((h >> 8) % 6);      // 2..7
		hand.fixPercent = 25 + (int)((h >> 12) % 40);    // 25..64
		hand.slangPercent = 15 + (int)((h >> 16) % 40);  // 15..54
		hand.perCharMs = 36 + (h >> 20) % 42;            // 36..77 ms a letter
		return hand;
	}

	// What a typo may never produce.
	inline bool IsRudeTypo(const std::string& w)
	{
		static const char* const kBad[] = {
			"chuj", "huj", "kurw", "pizd", "jeb", "cip", "dup", "cwel", "pierd", "sra", "kutas", "fiut", "szmat",
			"suk", "pedal", "ciot", "gown", "rucha"
		};
		for (size_t i = 0; i < sizeof(kBad) / sizeof(kBad[0]); ++i)
			if (w.find(kBad[i]) != std::string::npos)
				return true;
		return false;
	}

	inline bool IsPlainLowerWord(const std::string& w)
	{
		if (w.empty())
			return false;
		for (size_t i = 0; i < w.size(); ++i)
			if (w[i] < 'a' || w[i] > 'z')
				return false;
		return true;
	}

	// Words a person does not misspell in a chat (too short to bother) or
	// that carry the meaning alone.
	inline bool IsTypoSafeWord(const std::string& w)
	{
		if (w.size() < 5 || !IsPlainLowerWord(w))
			return false;
		static const char* const kKeep[] = { "nie", "tak", "jestem", "spoko", "dzieki" };
		for (size_t i = 0; i < sizeof(kKeep) / sizeof(kKeep[0]); ++i)
			if (w == kKeep[i])
				return false;
		return true;
	}

	// One letter wrong: two neighbours swapped, one doubled or one dropped.
	inline std::string MakeTypo(const std::string& w, TRng& rng)
	{
		if (w.size() < 5)
			return w;
		std::string out = w;
		const size_t at = 1 + rng.Range((u32)(w.size() - 2));
		switch (rng.Range(3))
		{
			case 0:
				if (at + 1 < out.size() && out[at] != out[at + 1])
					std::swap(out[at], out[at + 1]);
				break;
			case 1:
				out.insert(at, 1, out[at]);
				break;
			default:
				out.erase(at, 1);
				break;
		}
		return out;
	}

	struct TWordSpan
	{
		size_t start;
		size_t len;
	};

	inline void FindWordSpans(const std::string& s, std::vector<TWordSpan>& out)
	{
		out.clear();
		size_t i = 0;
		while (i < s.size())
		{
			const unsigned char c = (unsigned char)s[i];
			const bool wordChar = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
					c >= 0x80 || c == '+' || c == '$' || c == '|' || c == '[' || c == ']' || c == '_';
			if (!wordChar)
			{
				++i;
				continue;
			}
			size_t j = i;
			while (j < s.size())
			{
				const unsigned char d = (unsigned char)s[j];
				const bool wc = (d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') ||
						d >= 0x80 || d == '+' || d == '$' || d == '|' || d == '[' || d == ']' || d == '_';
				if (!wc)
					break;
				++j;
			}
			TWordSpan span;
			span.start = i;
			span.len = j - i;
			out.push_back(span);
			i = j;
		}
	}

	inline bool IsCapitalised(const std::string& w)
	{
		return !w.empty() && w[0] >= 'A' && w[0] <= 'Z';
	}

	// "Jestem" - a capital first and lowercase letters after it.
	inline bool IsTitleWord(const std::string& w)
	{
		// The one-letter words a sentence opens with: "A ty?", "W realu", "Z Joan".
		if (w.size() == 1)
			return w == "A" || w == "W" || w == "Z" || w == "O" || w == "I" || w == "U";
		if (w.size() < 2 || !IsCapitalised(w))
			return false;
		for (size_t i = 1; i < w.size(); ++i)
			if (w[i] < 'a' || w[i] > 'z')
				return false;
		return true;
	}

	// The words a reply opens with that are never a name, even before one:
	// "Mam Miecz Pelni Ksiezyca" opens with "mam".
	inline bool IsPlainStarter(const std::string& w)
	{
		static const char* const k[] = {
			"Mam", "Mamy", "Jestem", "Jestes", "Sprzedam", "Kupie", "Bije", "Expie", "Ide", "Szukam", "Wlasnie",
			"Teraz", "Tak", "Nie", "No", "Na", "W", "Z", "A", "O", "Moj", "Moja", "Moje", "Mieszkam", "Bylem",
			"Siedze", "Stoje", "Gram", "Lece", "Jade", "Ja", "Ty", "Tu", "Tam", "Jak", "Co", "Juz", "Dalej", "U"
		};
		for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i)
			if (w == k[i])
				return true;
		return false;
	}

	inline bool SameFolded(const std::string& a, const std::string& b)
	{
		if (a.size() != b.size() || a.empty())
			return false;
		for (size_t i = 0; i < a.size(); ++i)
		{
			char x = a[i], y = b[i];
			if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
			if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
			if (x != y)
				return false;
		}
		return true;
	}

	// The whole hand put on a reply. `fix` gets "*word" when a typo is made
	// and the bot corrects it in a line of its own.
	inline std::string Casualize(const std::string& text, const TBotSnapshot& s, TRng& rng, std::string& fix,
			bool cold, bool tidy = false)
	{
		fix.clear();
		if (text.empty())
			return text;
		const TTypingHand hand = HandOf(s.name);
		std::string out = text;

		// Slang first, on the plain words only.
		if (!cold && (int)rng.Range(100) < hand.slangPercent)
		{
			static const char* const kSlang[][2] = {
				{ "nie wiem", "nwm" }, { "Nie wiem", "nwm" }, { "w ogole", "wgl" }, { "pozdrawiam", "pozdro" },
				{ "dziekuje", "dzieki" }, { "naprawde", "serio" }, { "chwileczke", "chwilka" }, { "troche", "troche" },
			};
			const size_t pick = rng.Range((u32)(sizeof(kSlang) / sizeof(kSlang[0])));
			const size_t pos = out.find(kSlang[pick][0]);
			if (pos != std::string::npos)
				out.replace(pos, strlen(kSlang[pick][0]), kSlang[pick][1]);
		}

		std::vector<TWordSpan> spans;
		FindWordSpans(out, spans);
		std::vector<std::string> words(spans.size());
		for (size_t i = 0; i < spans.size(); ++i)
			words[i] = out.substr(spans[i].start, spans[i].len);

		// Sentence starts: the first word, and a word after ". ", "! ", "? ".
		if (hand.lowerStart)
		{
			for (size_t i = 0; i < spans.size(); ++i)
			{
				const size_t st = spans[i].start;
				bool sentenceStart = i == 0 && st == 0;
				if (!sentenceStart && st >= 2 && out[st - 1] == ' ' &&
						(out[st - 2] == '.' || out[st - 2] == '!' || out[st - 2] == '?'))
					sentenceStart = true;
				// After a smiley: ":P A ty?" is two sentences too.
				if (!sentenceStart && st >= 3 && out[st - 1] == ' ' &&
						((out[st - 3] == ':' || out[st - 3] == ';') || (out[st - 3] == 'x' && (out[st - 2] == 'd' || out[st - 2] == 'D'))))
					sentenceStart = true;
				if (!sentenceStart || !IsTitleWord(words[i]))
					continue;
				// A name: two capitalised words in a row, or the people's own.
				if (i + 1 < spans.size() && IsCapitalised(words[i + 1]) &&
						spans[i + 1].start == spans[i].start + spans[i].len + 1 && !IsPlainStarter(words[i]))
					continue;
				if (SameFolded(words[i], s.name) || SameFolded(words[i], s.askerName))
					continue;
				out[st] = (char)(out[st] - 'A' + 'a');
				words[i][0] = out[st];
			}
		}

		// ". jestem" -> ", jestem": a breath, not a full stop - half the time.
		if (hand.commaJoin && hand.lowerStart && !tidy)
		{
			for (size_t i = 1; i + 2 < out.size(); ++i)
			{
				if (out[i] == '.' && out[i + 1] == ' ' && out[i + 2] >= 'a' && out[i + 2] <= 'z' &&
						out[i - 1] != '.' && rng.Chance(50))
					out[i] = ',';
			}
		}

		// The full stop at the end.
		if (hand.dropDot && out.size() >= 2 && out[out.size() - 1] == '.' && out[out.size() - 2] != '.')
			out.erase(out.size() - 1);

		// One letter wrong, now and then - never in a cold reply, never in a
		// name, never into a swear word.
		if (!cold && !tidy && out.size() >= 24 && (int)rng.Range(100) < hand.typoPercent)
		{
			std::vector<TWordSpan> again;
			FindWordSpans(out, again);
			std::vector<size_t> candidates;
			for (size_t i = 0; i < again.size(); ++i)
			{
				const std::string w = out.substr(again[i].start, again[i].len);
				if (IsTypoSafeWord(w))
					candidates.push_back(i);
			}
			if (!candidates.empty())
			{
				const size_t k = candidates[rng.Range((u32)candidates.size())];
				const std::string w = out.substr(again[k].start, again[k].len);
				const std::string t = MakeTypo(w, rng);
				if (t != w && !IsRudeTypo(t))
				{
					out.replace(again[k].start, again[k].len, t);
					if ((int)rng.Range(100) < hand.fixPercent)
						fix = "*" + w;
				}
			}
		}
		return out;
	}

	// How long a person takes to type it: a moment to read, then the letters.
	inline u32 TypingDelayMs(const std::string& text, const TBotSnapshot& s, TRng& rng)
	{
		const TTypingHand hand = HandOf(s.name);
		u32 ms = 200 + (u32)text.size() * hand.perCharMs;
		ms = ms * (85 + rng.Range(31)) / 100;
		// Busy with a fight: the answer waits for a free hand.
		if (s.action == A_FIGHT || s.action == A_LURE)
			ms += 300 + rng.Range(1400);
		if (ms > 7000)
			ms = 6200 + rng.Range(1200);
		return ms;
	}
}

#endif
