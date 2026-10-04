// MT2009_PLUS_BOT_FRIENDS_V1: a person may add a bot to the friends list (the
// owner, 3 October: "Mozliwosc zaproszenia bota do znajomych/przyjaciol").
//
// The engine asks the invited character with the command "messenger_auth",
// a question in its client - and a bot has no client: its descriptor drops
// every packet, so the request sat in MessengerManager for good and the
// person heard nothing. server-patches/botfriends makes
// MessengerManager::RequestToAdd ask this instead and answer through the
// same AuthToAdd a person's "Tak" or "Nie" goes through. Online and offline
// on the list are the engine's own: a bot comes into the world through
// CInputLogin::Entergame (MessengerManager::Login) and leaves through
// CHARACTER::Disconnect (Logout). Whispers to a bot friend are answered as
// any whisper to a bot is, and removing one is the engine's ordinary
// removal, both ways.
//
// A bot says yes as a rule. It says no when it would not talk to the person
// anyway: a shouter (it answers no whisper), and a bot of another kingdom of
// the share that fights the other kingdoms (the KINGDOMPVP slider,
// IsPlayerBotHostileToOtherKingdoms) - it would attack the new friend at the
// next stone. Its own list's limit is the engine's, as a person's is.
//
// Defined outside the anonymous namespace: messenger_manager.cpp declares it.
// Include once, after playerbot_shouters.h and playerbot_sidekick.h.

bool Mt2009BotFriendRequest(LPCHARACTER from, LPCHARACTER bot)
{
	if (!from || !bot || !bot->IsPC() || !from->IsPC())
		return false;
	const DWORD botPid = bot->GetPlayerID();
	// The person's own companion is its owner's friend whenever asked.
	const TPlayerBotSidekick* companion = FindPlayerBotSidekickOf(botPid);
	const bool ownCompanion = companion && companion->dwOwnerPID == from->GetPlayerID();
	const char* refusal = NULL;
	if (!ownCompanion)
	{
		if (IsPlayerBotShouterPID(botPid))
			refusal = "shouter";
		else if (bot->GetEmpire() != from->GetEmpire() && IsPlayerBotHostileToOtherKingdoms(bot))
			refusal = "hostile_kingdom";
		else if (MessengerManager::instance().GetFriendCount(bot->GetName()) >= MESSENGER_MAX_FRIEND_LIMIT)
			refusal = "full";
	}
	const bool accepted = refusal == NULL;
	if (accepted)
	{
		static const char* const yes[] = {
			"Jasne, dodaje cie do znajomych!",
			"Pewnie, mozemy byc znajomymi. Pisz smialo.",
			"Przyjete! Daj znac, jak bedziesz czegos potrzebowal.",
			"Ok, jestes na mojej liscie znajomych.",
		};
		SendPlayerBotWhisper(bot, from, yes[PlayerBotNavHash(botPid ^ from->GetPlayerID()) % (sizeof(yes) / sizeof(yes[0]))]);
	}
	else if (!strcmp(refusal, "hostile_kingdom"))
		SendPlayerBotWhisper(bot, from, "Z kims z wrogiego krolestwa sie nie kumpluje. Bez urazy.");
	else if (!strcmp(refusal, "full"))
		SendPlayerBotWhisper(bot, from, "Mam juz pelna liste znajomych, sorki.");
	if (!accepted)
		from->ChatPacket(CHAT_TYPE_INFO, "%s odrzuca zaproszenie do znajomych.", bot->GetName());
	sys_log(0, "PLAYERBOT_FRIENDS: invitation from=%u name=%s bot=%u bot_name=%s accepted=%d why=%s",
			from->GetPlayerID(), from->GetName(), botPid, bot->GetName(), accepted ? 1 : 0,
			refusal ? refusal : (ownCompanion ? "own_companion" : "ok"));
	return accepted;
}
