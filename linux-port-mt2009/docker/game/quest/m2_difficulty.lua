-- metin2-suite: the world's difficulty, as the quests read it.
--
-- The operator chooses easy, medium, hard or custom in .env (M2_DIFFICULTY,
-- M2_BIOLOGIST_WAIT_HOURS, M2_HORSE_WAIT_HOURS); the migrator (apply.sh)
-- turns that into event flags in seconds at every start, and this file -
-- dofile'd by _otherModuleLoader.lua after collect_data.lua - is the one
-- place the quests ask. An event flag is the package's own idiom for a
-- world-wide switch (beta_server in these same quests), it is loaded by the
-- db core at boot and pushed to every game core, and the panel can read it.
--
-- Nothing here touches the bots: their Biologist hand-in and their stable
-- keeper are the AI's own code and never kept a wait.
m2_difficulty = {}

-- Seconds an event flag asks for; a missing or negative flag is no wait.
m2_difficulty.seconds = function(name)
	local v = game.get_event_flag(name)
	if v == nil or v < 0 then
		return 0
	end
	return v
end

-- The stable keeper's waits: "buy" (the pony), "upgrade" (each Horse Book),
-- "train" (levels 1-10, the medals), "train2" (levels 11-19). A plain global
-- rather than a dotted name, because qc only admits dotted calls it knows.
function m2_horse_wait(kind)
	if kind == "buy" then
		return m2_difficulty.seconds("m2_horse_buy_wait")
	elseif kind == "upgrade" then
		return m2_difficulty.seconds("m2_horse_upgrade_wait")
	elseif kind == "train2" then
		return m2_difficulty.seconds("m2_horse_train2_wait")
	end
	return m2_difficulty.seconds("m2_horse_train_wait")
end

-- The Biologist: every collect_quest_lv* asks collect_data.is_wait, which
-- asks these two. The package waited until the reset hour of the next day
-- (time_until_hour); the world's number replaces that, and zero means the
-- next specimen is taken at once - what 2.0.55 did for everybody.
collect_data.is_research_in_progress = function()
	if m2_difficulty.seconds("m2_biologist_wait") <= 0 then
		return false
	end
	return get_time() < pc.getf("collect_quest", "wait")
end

collect_data.set_wait_time = function()
	local t = m2_difficulty.seconds("m2_biologist_wait")
	if game.get_event_flag("beta_server") > 0 then
		t = 60
	end
	if t <= 0 then
		collect_data.reset_time()
		return
	end
	pc.setf("collect_quest", "wait", get_time() + t)
	collect_data.send_delay()
end

-- MT2009_PLUS_EXCHANGE_CHANCE_V1: the NPC exchanges' chances by the world's
-- difficulty (libs/crafting/item_exchange.lua; the Dockerfile routes both of
-- its reads of a chance through here - the roll of each piece and the
-- window's "chance" field, so the window shows what is rolled). kind is the
-- package's own number: 1 soul stones to Magiczny Pyl (the Alchemist,
-- 20001), 2 skill books to Pergamin (20023), 3 upgrade items to Materialy
-- Rzemieslnicze (the Dozorca, 9005). Easy is the package's 100 / 100 / 55,
-- medium 90 / 45 / 55, hard 55 / 40 / 55: they follow the level flag, which
-- the panel's difficulty card sets live. Custom takes the migrator's
-- m2_exchange_{dust,parchment,material}_chance (.env M2_EXCHANGE_*_CHANCE,
-- written at every start), zero being the package's number. The bots roll
-- the same (GetPlayerBotExchangeChance, playerbot_config.h) - keep the two
-- tables alike, and the panel's and the launchers' texts with them.
m2_difficulty.EXCHANGE_PRESETS = {
	[1] = { 90, 45, 55 },
	[2] = { 55, 40, 55 },
}
m2_difficulty.EXCHANGE_CUSTOM_FLAGS = {
	"m2_exchange_dust_chance",
	"m2_exchange_parchment_chance",
	"m2_exchange_material_chance",
}

m2_difficulty.exchange_chance = function(kind, package_chance)
	local level = game.get_event_flag("m2_difficulty")
	local preset = m2_difficulty.EXCHANGE_PRESETS[level]
	if preset ~= nil and preset[kind] ~= nil then
		return preset[kind]
	end
	if level == 3 and m2_difficulty.EXCHANGE_CUSTOM_FLAGS[kind] ~= nil then
		local v = game.get_event_flag(m2_difficulty.EXCHANGE_CUSTOM_FLAGS[kind])
		if v ~= nil and v > 0 then
			if v > 100 then
				return 100
			end
			return v
		end
	end
	return package_chance
end
