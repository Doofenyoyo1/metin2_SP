#ifndef PLAYERBOT_PRICE_RULES_H
#define PLAYERBOT_PRICE_RULES_H
// Iwakura's Community Patch 5 on prices, as pure arithmetic: the inflation
// compounded (point 10), what several maximal lines on one piece add (point
// 11), and which of his two kinds of bonus row prices a line (point 13) - and
// his answer of 28 September on the counters: the markdown of a line nobody
// buys and the markup of a kind the market keeps running out of. No engine
// types; unit-tested in tests/playerbot_price_rules_test.cpp. The engine side
// is ScalePlayerBotIwakuraPrice and GetPlayerBotListingPrice
// (playerbot_town.h), GetPlayerBotBonusPricePercent (playerbot_bonus.h) and
// UpdatePlayerBotShortageMarkups (playerbot_world_memory.h).
#include <cmath>
#include <cstdint>

namespace playerbot_price_rules {

// Point 10: "zamiast dodawac plaskie +5% do mnoznika za kazde wygenerowane
// 2,5 mld Yang, system powinien mnozyc obecne wartosci przez 1.05 (x1.05) za
// kazde kolejne 2,5 mld Yang". The multiplier is kept in ten-thousandths, so
// one step is exactly Community Patch 2's +5% and the caller's integer
// arithmetic keeps its shape: 10 000 is no inflation, 10 500 one step, 11 025
// two, 11 576 three, where adding five points a step made 11 500.
const long long INFLATION_FACTOR_ONE = 10000;

// The multiplier after `steps` whole steps of `stepPercent` each, compounded,
// and never over `maxFactor`: a power of a step count outgrows any integer
// long before it outgrows a world's purses, so the ceiling is arithmetic and
// nothing else.
inline long long InflationFactor(long long steps, int stepPercent, long long maxFactor)
{
	if (steps <= 0 || stepPercent <= 0)
		return INFLATION_FACTOR_ONE;
	const double factor = (double)INFLATION_FACTOR_ONE *
			std::pow(1.0 + (double)stepPercent / 100.0, (double)steps);
	if (!(factor < (double)maxFactor))
		return maxFactor;
	return (long long)(factor + 0.5);
}

// MT2009_PLUS_WORLD_YANG_PRICES_V1 (the owner, 2 October): the bots' price
// level from the yang the world's characters hold, smooth instead of in
// steps. A new world sells at `startPercent` of the sheet, rising in a
// straight line to the sheet itself at `zeroYang`; over it every `stepYang`
// lifts prices by `hotPercent`, compounded, and over `coolYang` by
// `coolPercent` a step, compounded on what the hot part reached. Ten-
// thousandths, as InflationFactor.
inline long long WorldYangFactor(long long yang, int startPercent, long long zeroYang, long long stepYang,
		int hotPercent, long long coolYang, int coolPercent, long long maxFactor)
{
	const double one = (double)INFLATION_FACTOR_ONE;
	if (yang < 0)
		yang = 0;
	double factor;
	if (zeroYang <= 0 || stepYang <= 0)
		factor = one;
	else if (yang <= zeroYang)
		factor = one * ((double)startPercent + (100.0 - (double)startPercent) * (double)yang / (double)zeroYang) / 100.0;
	else
	{
		const long long hotEnd = std::max(zeroYang, coolYang);
		const double hotSteps = (double)(std::min(yang, hotEnd) - zeroYang) / (double)stepYang;
		factor = one * std::pow(1.0 + (double)hotPercent / 100.0, hotSteps);
		if (yang > hotEnd)
			factor *= std::pow(1.0 + (double)coolPercent / 100.0, (double)(yang - hotEnd) / (double)stepYang);
	}
	if (!(factor < (double)maxFactor))
		return maxFactor;
	return std::max(1LL, (long long)(factor + 0.5));
}

// Point 11: "Jezeli przedmiot posiada wiecej niz jeden maksymalny bonus, jego
// cena koncowa jest dodatkowo mnozona" - two lines at their top x1.7, three
// x2.5, four x4.0, over what the lines already asked on their own. A fifth,
// which only the blessing marble adds, asks what four do: his list stops at
// four. Hundredths, by the count.
const int MAX_LINES_COUNTED = 4;
const int MAX_LINES_PERCENT[MAX_LINES_COUNTED + 1] = { 100, 100, 170, 250, 400 };

inline int MaxLinesPercent(int maxLines)
{
	if (maxLines < 0)
		maxLines = 0;
	if (maxLines > MAX_LINES_COUNTED)
		maxLines = MAX_LINES_COUNTED;
	return MAX_LINES_PERCENT[maxLines];
}

// "Srednie obrazenia na poziomie 40% lub wyzszym (40+) sa zawsze traktowane i
// liczone jako maksymalny bonus": a weapon's average damage counts as a line
// at its top from forty, though the tiers of his sheet run to sixty.
const long AVERAGE_DAMAGE_MAX_FROM = 40;

// One of his bonus rows for a line: what it multiplies by at the line's top
// and at any other value (hundredths), and the top it names itself - 0 for
// the rows of his first sheet, whose top is whatever this world's table rolls.
struct BonusRow
{
	int maxPct;
	int otherPct;
	long top;
};

// Whether a line is at its top: what this world's item_attr rolls for it on
// the piece (tableMax, 0 when the table has no such line), or the top his
// point-13 row names for a line only upstream's 2.2.31 table rolled (writtenTop, 0
// when no row names one). A line over the table's top - regeneration at 20 or
// 30, rolled under 2.2.31, where this table stops at 12 - is at it.
inline bool IsMaxLine(long value, long tableMax, long writtenTop)
{
	if (value <= 0)
		return false;
	return (tableMax > 0 && value >= tableMax) || (writtenTop > 0 && value >= writtenTop);
}

// What one line asks, in hundredths, when his sheet may price it twice: the
// slot's row, judged against this world's table, and a point-13 row with a top
// of its own. Only regeneration has both - 1.3 | 1.1 on a helmet and a
// necklace at this table's 12, 1.5 | 1.2 at upstream's 2.2.31 table's 30. The new
// row's maximum is for a line at its own top; under it the world's row
// decides, so a 20, over this table's top, still asks that row's maximum and
// never less than the 12 under it. A line no slot row prices - the elemental
// resistances, the two chances, a mana regeneration his first sheet put at
// x1.0 - takes the new row's "any other value". 100 when no row applies.
inline int LinePercent(long value, long tableMax, const BonusRow* slotRow, const BonusRow* topRow)
{
	if (value <= 0)
		return 100;
	if (topRow && topRow->top > 0 && value >= topRow->top)
		return topRow->maxPct;
	if (slotRow)
		return (tableMax > 0 && value >= tableMax) ? slotRow->maxPct : slotRow->otherPct;
	if (topRow)
		return topRow->otherPct;
	return 100;
}

// The lines' product, one more line in: his multipliers compound, and the
// product stops at capPct (hundredths) - that ceiling is ours, not his, so a
// weapon of sixty average and thirty skill is not 2800 times its base.
inline long long CompoundLinePercent(long long product, int linePct, long long capPct)
{
	product = product * linePct / 100;
	return product > capPct ? capPct : product;
}

// What a piece asks over its base, in percent: the lines' product, then the
// maximal lines' multiplier over the whole of it, as he wrote it - on the
// final price, so over our ceiling too.
inline long long PiecePremiumPercent(long long product, int maxLines)
{
	return product * MaxLinesPercent(maxLines) / 100 - 100;
}

// His answer of 28 September to the operator's four questions on the bots'
// prices, points 4 and 5, which are one rule seen from its two sides. A line
// nobody buys comes down ten percent for every three hours it has stood, to
// forty - "To zbyt agresywne, zrobilbym 10% co 3h do max -40%", where it was
// ten every two hours to fifty. And a kind that keeps selling and keeps being
// missing from the counters goes up ten percent for every three hours of
// that, to forty: "jesli jakies przedmioty sprzedaja sie caly czas, np.
// medale konne, i ciagle ich brakuje na rynku, to cena rosnie o 10% do max
// 40% - tylko nie wiem jak dokladnie to technicznie wyliczyc". How it is
// worked out is ours, below.

// `steps` whole steps of `stepPercent` each, never over `maxPercent`.
inline int SteppedPercent(long long steps, int stepPercent, int maxPercent)
{
	if (steps <= 0 || stepPercent <= 0 || maxPercent <= 0)
		return 0;
	// A step is a percent at least, so this many steps are the ceiling
	// whatever the step, and the product below cannot overflow.
	if (steps >= maxPercent)
		return maxPercent;
	const long long percent = steps * (long long)stepPercent;
	return percent >= maxPercent ? maxPercent : (int)percent;
}

// Point 4: the markdown of a line that has stood `standingMs` unsold - a step
// for every whole `stepMs`, none before the first.
inline int UnsoldMarkdownPercent(uint32_t standingMs, uint32_t stepMs, int stepPercent, int maxPercent)
{
	return stepMs == 0 ? 0 : SteppedPercent((long long)(standingMs / stepMs), stepPercent, maxPercent);
}

// What a line asks, in hundredths of what the market asks for it: under it
// by its own markdown, or over it by its kind's markup - never both. The two
// are opposite states of one kind: a line that has stood unsold is the market
// saying its kind is not short, whatever the kind's markup still says.
inline int ListingPercent(int markdownPercent, int markupPercent)
{
	if (markdownPercent > 0)
		return markdownPercent >= 100 ? 0 : 100 - markdownPercent;
	return markupPercent > 0 ? 100 + markupPercent : 100;
}

// A price moved by ListingPercent, in 64 bits - the stall's own arithmetic
// was 32 and wrapped for a line over forty-odd million - and never under one
// yang for a price that was one.
inline long long ApplyListingPercent(long long price, int percent)
{
	if (price <= 0 || percent == 100)
		return price;
	const long long moved = price * (long long)(percent < 0 ? 0 : percent) / 100;
	return moved < 1 ? 1 : moved;
}

// Point 5, as a window per kind. The evidence of one step is gathered for
// windowMs (three hours, the markdown's step) and then judged whole, so a
// line that flickers on and off the counters between two looks is neither a
// shortage nor its end:
//
//   - missing: the ledger found none of the kind on any counter at
//     missingPercent of its looks in the window or more ("ciagle ich
//     brakuje");
//   - selling: at least minLinesSold lines of it were sold in the window
//     ("sprzedaja sie caly czas").
//
// Missing and selling is a step up, to maxPercent. Stock that stood on the
// counters for more of the window than that is a step down, to nothing:
// there is stock and it is not selling out. Missing without the sales is no
// evidence either way - nothing to buy is nothing sold - and the kind keeps
// its markup; whatever is put up next either sells out or stands, and that
// window decides. A kind at no markup whose window did not raise it is let go,
// and its next sale opens a new window.
struct ShortageRules
{
	uint32_t windowMs;
	int stepPercent;
	int maxPercent;
	uint32_t minLinesSold;
	int missingPercent;
};

struct ShortageState
{
	bool watching;
	uint32_t windowStart;
	uint32_t looks;
	uint32_t missingLooks;
	uint32_t linesSold;
	int markupPercent;
	ShortageState()
		: watching(false), windowStart(0), looks(0), missingLooks(0), linesSold(0),
		  markupPercent(0)
	{
	}
};

enum ShortageVerdict
{
	SHORTAGE_OPEN,  // the window is still gathering
	SHORTAGE_HOLD,  // missing, and too little sold to say: the markup stays
	SHORTAGE_UP,    // missing and selling: a step up
	SHORTAGE_DOWN,  // stock stood on the counters: a step down
};

// What a closed window was, for the line that says why a markup moved.
struct ShortageJudgement
{
	ShortageVerdict verdict;
	uint32_t looks;
	uint32_t missingLooks;
	uint32_t linesSold;
	int markupBefore;
	int markupAfter;
};

inline ShortageVerdict JudgeShortageWindow(uint32_t looks, uint32_t missingLooks, uint32_t linesSold,
		const ShortageRules& rules)
{
	if (looks == 0)
		return SHORTAGE_HOLD;
	const uint64_t missingPercent = (uint64_t)(rules.missingPercent < 0 ? 0 : rules.missingPercent);
	if ((uint64_t)missingLooks * 100u < (uint64_t)looks * missingPercent)
		return SHORTAGE_DOWN;
	return linesSold >= rules.minLinesSold ? SHORTAGE_UP : SHORTAGE_HOLD;
}

// Lines of the kind sold since the last look. A kind nobody was watching is
// watched from now: its window opens with the sale.
inline void NoteShortageSale(ShortageState& state, uint32_t now, uint32_t lines)
{
	if (!state.watching)
	{
		state.watching = true;
		state.windowStart = now;
		state.looks = state.missingLooks = state.linesSold = 0;
	}
	state.linesSold = lines > 0xFFFFFFFFu - state.linesSold ? 0xFFFFFFFFu : state.linesSold + lines;
}

// One look of the ledger at a watched kind: counted, and once the window has
// run its windowMs, judged and begun again. The clock is the core's
// millisecond one, so the difference is taken unsigned and a wrap costs
// nothing.
inline ShortageJudgement NoteShortageLook(ShortageState& state, uint32_t now, bool missing,
		const ShortageRules& rules)
{
	ShortageJudgement judged = { SHORTAGE_OPEN, 0, 0, 0, state.markupPercent, state.markupPercent };
	if (!state.watching)
		return judged;
	++state.looks;
	if (missing)
		++state.missingLooks;
	if ((uint32_t)(now - state.windowStart) < rules.windowMs)
		return judged;
	judged.verdict = JudgeShortageWindow(state.looks, state.missingLooks, state.linesSold, rules);
	judged.looks = state.looks;
	judged.missingLooks = state.missingLooks;
	judged.linesSold = state.linesSold;
	if (judged.verdict == SHORTAGE_UP)
		state.markupPercent = state.markupPercent + rules.stepPercent > rules.maxPercent
				? rules.maxPercent : state.markupPercent + rules.stepPercent;
	else if (judged.verdict == SHORTAGE_DOWN)
		state.markupPercent = state.markupPercent - rules.stepPercent < 0
				? 0 : state.markupPercent - rules.stepPercent;
	judged.markupAfter = state.markupPercent;
	state.windowStart = now;
	state.looks = state.missingLooks = state.linesSold = 0;
	if (state.markupPercent <= 0)
	{
		state.markupPercent = 0;
		state.watching = false;
	}
	return judged;
}

// MT2009_PLUS_MARKET_V3: the living market of 2 October, as arithmetic. The
// engine side is the market index and the census in playerbot_town.h
// (UpdatePlayerBotMarketIndex, GetPlayerBotPricedPlus), the listing price and
// its floor beside them, and the reprice in playerbot_offline_shop.h.

// Point 1: the price of a kind the counters of the whole world (the bots' and
// the people's) lack goes up, and of one they are full of goes down. What the
// counters hold now (supply) is read against what they usually hold (usual):
// a slow average of the same count, so a kind the market always carries a
// thousand of is not "flooded" at a thousand, and the owner's base prices are
// what the usual market asks. The bots short of it (demand) are buyers on top
// of the usual. (usual + demand + q0) / (supply + q0), to `exponent`, between
// minPercent and maxPercent - hundredths.
inline int MarketIndexTargetPercent(double usual, double supply, double demand, double q0,
		double exponent, int minPercent, int maxPercent)
{
	if (usual < 0.0) usual = 0.0;
	if (supply < 0.0) supply = 0.0;
	if (demand < 0.0) demand = 0.0;
	if (q0 <= 0.0) q0 = 1.0;
	const double target = 100.0 * std::pow((usual + demand + q0) / (supply + q0), exponent);
	if (!(target > (double)minPercent))
		return minPercent;
	if (!(target < (double)maxPercent))
		return maxPercent;
	return (int)(target + 0.5);
}

// A step of an average with a time constant: `elapsedMs` of `tauMs` takes the
// value 1 - e^(-t/tau) of the way to `target`. Both the usual supply (a day)
// and the index itself (two hours, "plynnie i z opoznieniem") move this way,
// so a counter emptied for ten minutes moves no price, and one empty all
// afternoon does.
inline double SmoothTowards(double value, double target, uint32_t elapsedMs, uint32_t tauMs)
{
	if (tauMs == 0)
		return target;
	const double alpha = 1.0 - std::exp(-(double)elapsedMs / (double)tauMs);
	return value + (target - value) * alpha;
}

// Point 2: the markdown of a line by the supply of its kind. A line whose
// kind the other counters do not carry - fewer units than the line itself
// holds, so a lone sword and a lone stack of fifty alike - is rare goods and is
// not marked down at all; at `plentyLines` lines' worth of other stock it
// takes its whole markdown, and between the two a share of it.
inline int SupplyMarkdownPercent(int markdownPercent, long long supplyUnits, long long lineUnits,
		int plentyLines)
{
	if (markdownPercent <= 0)
		return 0;
	if (lineUnits < 1)
		lineUnits = 1;
	long long others = supplyUnits - lineUnits;
	if (others < lineUnits)
		return 0;
	if (plentyLines <= 1)
		return markdownPercent;
	const long long plenty = lineUnits * (long long)plentyLines;
	if (others >= plenty)
		return markdownPercent;
	return (int)((long long)markdownPercent * (others - lineUnits + 1) / (plenty - lineUnits + 1));
}

// Point 3: a price a person writes. Three digits for a price that starts with
// a one and two and a half for the rest - 1 487 312 is 1 490 000, 2 463 000
// is 2 450 000, 7 312 is 7 300 - to the nearest such number, or up to it for
// a floor, which nothing may ask under. Under `minPrice` nothing is touched.
inline long long HumanPrice(long long price, bool up, long long minPrice)
{
	if (price < minPrice || price < 100)
		return price;
	long long magnitude = 1;
	while (magnitude <= price / 10)
		magnitude *= 10;
	const long long step = price / magnitude < 2 ? magnitude / 100 : magnitude / 20;
	if (step < 2)
		return price;
	const long long rounded = up ? (price + step - 1) / step * step : (price + step / 2) / step * step;
	return rounded < step ? step : rounded;
}

// Point 5: a piece's lines as a plus. A line counts by how far up its top
// (what this world's table rolls for it) it is: at its top three points, at
// three quarters two, at half one. Two points are a clean +7 - "bransoleta +0
// z 1500 PZ kosztuje tyle, co czysta +7", and 1 500 of the bracelet's 2 000 is
// three quarters - four a +8, one a +5. Zero for no plus at all.
inline int BonusLinePoints(long value, long top)
{
	if (value <= 0 || top <= 0)
		return 0;
	const long long pct = (long long)value * 100 / top;
	if (pct >= 100)
		return 3;
	if (pct >= 75)
		return 2;
	return pct >= 50 ? 1 : 0;
}

inline int BonusPlusLevel(int points)
{
	if (points >= 4)
		return 8;
	if (points >= 2)
		return 7;
	return points == 1 ? 5 : 0;
}

// Point 4: a weapon's average damage as a plus - from `sevenFrom` a +7, from
// `sixFrom` a +6, at any plus the piece itself has.
inline int AverageDamagePlusLevel(long average, long sixFrom, long sevenFrom)
{
	if (average >= sevenFrom)
		return 7;
	return average >= sixFrom ? 6 : 0;
}

// And the best copies of a weapon on the server: one whose average is the
// best seen of its family asks `maxPremium` percent more, one at `fromPercent`
// of the best nothing, straight between; under `minAverage` nothing.
inline int TopCopyPercent(long average, long best, long minAverage, int fromPercent, int maxPremium)
{
	if (average < minAverage || best <= 0 || maxPremium <= 0)
		return 100;
	if (average >= best)
		return 100 + maxPremium;
	const long long share = (long long)average * 100 / best;
	if (share <= fromPercent || fromPercent >= 100)
		return 100;
	return 100 + (int)((long long)maxPremium * (share - fromPercent) / (100 - fromPercent));
}

// MT2009_PLUS_BONUS_COUNT_PRICE_V1: the owner's rule of 2 October - a piece with
// bonuses, weak ones included, is worth a great deal more while the world has
// no Moonlight chests to bonus a set out of: a bracelet that dropped with two
// lines, or one a bot gave two or three. On while no Moonlight chest event has
// run for MOONLIGHT_QUIET_SECONDS (fourteen days), off while one runs or ran
// within them. The engine half: the clock in playerbot_events.h
// (UpdatePlayerBotBonusCountPricing), the add-on in playerbot_bonus.h
// (GetPlayerBotBonusCountAddon) and the price in playerbot_town.h
// (LiftPlayerBotGearPrice).
//
// How it meets MARKET_V3's "priced like a +N" (BonusPlusLevel, point 5): both
// are a price of the same lines, so they are never added together. The piece
// asks the larger of the two - the sheet's price of the plus its lines make it,
// or its own price with this add-on on top - and the best copy's and
// Intelligence's premiums go on whichever won, as they did.
enum EBonusCountClass
{
	BONUS_COUNT_NONE = 0,
	// Bracelet, necklace, earrings, boots and shields.
	BONUS_COUNT_ACCESSORY = 1,
	// Body armour, helmet and every weapon but the two below.
	BONUS_COUNT_GEAR = 2,
	// A weapon of level 30 or 75: its first two lines add nothing, only the
	// third, the fourth and the fifth.
	BONUS_COUNT_WEAPON_30_75 = 3,
};

const long long BONUS_COUNT_LEVEL_SPLIT = 42;	// "do 42 lvl" and "powyzej 42 lvl"
const long long BONUS_COUNT_ACCESSORY_LOW = 1500000;
const long long BONUS_COUNT_ACCESSORY_HIGH = 2500000;
const long long BONUS_COUNT_GEAR_LOW = 500000;
const long long BONUS_COUNT_GEAR_HIGH = 1200000;
const int BONUS_COUNT_MAX_LINES = 5;
const int BONUS_COUNT_WEAPON_FREE_LINES = 2;
const long MOONLIGHT_QUIET_SECONDS = 14L * 24L * 60L * 60L;

// The yang one line adds, on the owner's numbers (before the world's yang
// curve, which the caller puts it through like every sheet price).
inline long long BonusCountLineValue(int cls, int limitLevel)
{
	const bool high = limitLevel > BONUS_COUNT_LEVEL_SPLIT;
	switch (cls)
	{
		case BONUS_COUNT_ACCESSORY:
			return high ? BONUS_COUNT_ACCESSORY_HIGH : BONUS_COUNT_ACCESSORY_LOW;
		case BONUS_COUNT_GEAR:
		case BONUS_COUNT_WEAPON_30_75:
			return high ? BONUS_COUNT_GEAR_HIGH : BONUS_COUNT_GEAR_LOW;
	}
	return 0;
}

// The add-on of a piece with `lines` bonus lines (the five ordinary ones; a
// count past five is five).
inline long long BonusCountAddon(int cls, int limitLevel, int lines)
{
	if (lines <= 0)
		return 0;
	if (lines > BONUS_COUNT_MAX_LINES)
		lines = BONUS_COUNT_MAX_LINES;
	int counted = lines;
	if (cls == BONUS_COUNT_WEAPON_30_75)
		counted = lines > BONUS_COUNT_WEAPON_FREE_LINES ? lines - BONUS_COUNT_WEAPON_FREE_LINES : 0;
	return (long long)counted * BonusCountLineValue(cls, limitLevel);
}

// Whether the rule is on: no chest event now, and the last second one was
// seen (0: never, as far as this world remembers) at least `quietSeconds`
// before `now`. A last second in the future - a clock put back - is now.
inline bool BonusCountPricingOn(bool chestEventNow, long lastMoonlight, long now, long quietSeconds)
{
	if (chestEventNow)
		return false;
	if (lastMoonlight <= 0)
		return true;
	if (lastMoonlight >= now)
		return false;
	return now - lastMoonlight >= quietSeconds;
}

}  // namespace playerbot_price_rules

#endif
