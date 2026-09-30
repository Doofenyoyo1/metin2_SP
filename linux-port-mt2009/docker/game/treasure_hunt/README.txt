MT2009_PLUS_GOBLIN_V1 - Treasure Island (map 419) for the Treasure Hunt event
(playerbot_goblin.h). The Dockerfile copies:
  map/metin2_map_treasure_hunt -> share/locale/poland/map/ (+ "419" in map/index)
  monster/*                    -> share/data/monster/
From the "Official Treasure Hunt System" archive (03. Server/share/locale/xxx/map,
02. Client/npc2/ymir work/npc2). The goblin's server motlist names walk.msa as
its RUN as well: the engine times an NPC's walk by its RUN motion, and the
goblin has only a walk - so the server's walk takes as long as the client's.
