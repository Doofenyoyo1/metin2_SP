# linux-port-mt2009

The suite on the mt2009 / Martysama r41023 server files (`__REVISION__` 41023,
full C++ source, FreeBSD/clang Makefiles, Lua 5.0 in-tree, cryptopp 7.0, protos
in the database). What is shared with `linux-port/` is shared by reference —
the launcher, the panels, the ItemShop, the updater and the playerbot overlay
under `linux-port/overlays/playerbot/` — and what differs is rendered from the
r40250 originals by the scripts in `port/`, never edited by hand.

## What is where

| Path | What |
|---|---|
| `docker/game/src/` | **Not in the repository** (`.gitignore`): `server/` (the engine, staged from the package's `Server Source/Server` minus `.o`/`.vcxproj`/`.sln`), `extern/` (`include/` + `cryptopp/`), `extern-tarballs/` (DevIL 1.8.0), `serverfiles/` (the runtime tree from `new_ftp.tar.gz`, binaries removed; `mark-default/` from `chs/ch11/mark`). |
| `docker/game/Dockerfile`, `build-deps-mt2009.sh` | The four-stage build: deps (apt, `APT_MIRROR`), libs (Lua, libsql/libgame/libpoly/libthecore, the db core), builder (game core), quests (`qc` compiles `quest/*.quest`), runtime. |
| `docker/game/bin/` | `m2-render-config` (conf.txt with six SQL handles, `PROTO_FROM_DB`, the map split), `entrypoint.sh`, `m2-supervise`, `m2-healthcheck`, `m2ctl` — copies of r40250's where nothing differs. `m2-rates` is this engine's own: the rates are event flags in `player.quest` (`mob_exp`/`mob_item`/`mob_gold` + `_buyer`), written by the panels and set live by the `web_admin` quest's `RATES` command, so the container-side script only restarts the cores when the panel falls back to that. No `m2-gm` (no admin socket here) and no `m2-lang`. |
| `docker/mariadb/initdb.d/` | `10-import-dumps.sh` (five per-database dumps, the `player.item_proto`/`mob_proto` views over `world`, `social_id` widened to 18, mileage/jackpot, tester accounts), `20-log-schema.sql`, `dumps/*.sql` (**not in the repository**, from the package's `sql/`). |
| `docker/mariadb/playerbot/` | `apply.sh`, `playerbots_seed.sql`, `playerbot_names.sql`, `itemshop_schema.sql`, `log_schema.sql` — all rendered (see below). |
| `docker/docker-compose.yml` | The stack; the panel/ItemShop/updater block is rendered from r40250's compose with contexts pointing back at `../../linux-port/docker/<name>`. `docker-compose.deploy.yml` is the same file with `./<name>` contexts, published as the player's `docker-compose.yml`. |
| `docker/ENGINE` | The word `mt2009`. The launcher reads `linux-port\docker\ENGINE` to know which engine it is looking at. |
| `docker/.env.example` | r40250's plus `M2_APT_MIRROR`. |
| `docker/game/{mob_drop_item.m3.append.txt,special_item_group.moonlight.txt,special_item_group.starter.txt}` | What the image appends to the package's `locale/poland` share. |
| `client-root/serverinfo.py` | The client's server list pointing at 127.0.0.1 (auth 11000, channel 13000). Packed into `pack/root.{index,data}` with `tools/eterpack.py --profile mt2009 repack <orig>/root <out>/root linux-port-mt2009/client-root`. The profile is PackMakerLite's layout, and it is not optional: this client has `ENABLE_CRC32_CHECK`, so `data_crc` must be the CRC32 of the object's on-disk bytes (r40250 left garbage there and the r40250 layout put the real size in it) — a root packed the r40250 way opens not one script and the client dies at `RunMain Error` with nothing else in `syserr.txt`. Verified byte-for-byte against the stock pack: same slots (rounded to 256), same ciphered lengths, same CRCs for every untouched file. |
| `client-root/playerbot_status_tail.py`, `client-root/game.py` | A bot's status over its head as a text tail and never a chat line: the server sends `PlayerBotStatus <vid> <hex>` (`SendPlayerBotOverheadChat`), game.py (rendered by `port/clientrootify.py`) hands it to the module, and the module calls `textTail.RegisterChatTail`. The module is new to the root, so the first repack that carries it needs `--allow-new`; `tests/playerbot_status_tail_test.py` checks the decoder on Python 2.7 and 3. |
| `client-root/uiautohunt.py`, `client-root/game.py` | Auto Lowy, the player's free auto-hunt (K): six skills, two potions and three items on a clock, Metin stones, revive and walk-back, and a pick-up by kind (weapons, armour, jewellery, potions, books, stones, the rest). The server names the target (`/autohunt_target` -> `AutoHuntTarget`) and the item (`/autohunt_loot` -> `AutoHuntLoot`) from `apply_auto_hunt` in playerbotify.py; game.py's two handlers and the K key are clientrootify.py's. |
| `client-root/offlineshopmanage.py` | A left click on an empty slot of the offline shop's edit grid removes nothing; `RemoveItem` read `myshop_data["items"][slot]` unchecked and raised KeyError. |
| `client-root/autostackpump.py`, `client-root/uiinventory.py` | The inventory's auto-stack button sends its moves six every tenth of a second instead of all in one frame: 300 packets in a second is the server's flood limit (`CInputMain::Analyze`, `FLOOD_HEADER_13` in `log.hack_log`) and it closed the connection. |
| `tools/game-compile.sh` | Syntax-checks the overlay inside the `m2mt2009-libs:dev` image with the game directory bind-mounted. It sees the image's copy of `common/`, so a change to `common/tables.h` only shows in a full `docker compose build game`. |

## The port scripts, in the order they run

Every one is idempotent and re-runnable after editing the r40250 original it
reads. They never touch anything by hand-maintained list where a measurement
will do.

| Script | Reads | Writes |
|---|---|---|
| `port/linuxify.py <server>` | the staged engine | Linux fixes in place: epoll `fdwatch` from `linux-port`, the `signal.h` shadow guard, `optreset`, `<md5.h>` from libmd, the `bind_ip`/`listen_ip`/`public_ip` split, `-Wl,--start-group` links, CRLF stripped from Makefiles and `__REVISION__`. |
| `port/playerbotify.py <server>` | `linux-port/overlays/playerbot/` | copies `playerbot_*` into `game/src`, `CFLAGS += -DPLAYERBOT_ENGINE_MT2009`, and ports patches 0001–0008, 0010–0015 as exact-string edits (0004 is already in this engine, 0009 — the F9 GM panel — is `apply_gm_panel`: the 31 `gmpanel_*` commands from `port/gm_panel_commands.cpp.txt` appended to `cmd_gm.cpp`, their declarations and `cmd_info[]` rows in `cmd.cpp`, and the deferred `SetGMFlag` in `char.cpp`. His `botadmin_*` are left out — they call manager methods of his fork). The bot load packet carries the account id here (`TBotPlayerLoadPacket.account_id`): the db core loads special flags with `pid=%d or aid=%d`, and aid 0 matched every bot's flags at once. `CSpecialItemGroup` gets a `GetGroupType()` getter, so the chest pass can reserve room for every line of a Pct group and for the largest line of the others (audit D01). |
| `port/seedify.py` | r40250's `playerbots_seed.sql` | the seed without `is_testor`/`empire`/`name_checked`/`bank_value`, the `account.empire` update dropped. |
| `port/migratorify.py` | r40250's `apply.sh` | the migrator probing `log.hack_log`, the hosted-map list of this stack, the `social_id` widening, mileage/jackpot, `log_schema.sql`. |
| `port/logschemify.py` | r40250's `log.sql` dump | the 23 log tables the engine writes and the package lacks, plus `log.log.ip`. |
| `port/composify.py` | r40250's `docker-compose.yml` | the shared services block, `docker-compose.deploy.yml`. |
| `port/shareify.py` | `linux-port/docker/game/*.txt`, the world dump | the three share additions and the Dockerfile step; refuses an item the package does not have (one unknown vnum fails the whole `special_item_group.txt` at boot). |
| `port/envify.py` | r40250's `.env.example` | this stack's. |
| `port/rulesify.py` | `client-locale-src/rules.pl.txt` (UTF-8, editable) | `client-locale/locale/pl/rules.txt` — the client's terms-of-use window, CP1250/CRLF, pairs of lines with `[ENTER]` between points: ours (what the project is, buycoffee, Discord) instead of the public Mt2009 server's. Repacked into the `locale` pack. |
| `port/iconify.py --icon-pack <extracted icon pack> --item-list <949item_list.txt from the item pack> --vnums <vnum list from player.item_proto> [--fallback <r40250 panel static>]` | the client's `icon/item/NNNNN.tga`, its `item_list.txt` (`.sub` names), the world's item_proto | `files/static/icons/*.png` and `files/static/item_icons.json` for the classic panel: a per-item TGA where the client has one (a refine level shares its +0's), the older set under `r40250-` names for what the client keeps only in atlases, a grey `_unknown.png` for the rest. Needs Pillow — run it in the `m2-eterpack` image. |
| `port/clientrootify.py --root <extracted stock root>` | the stock root scripts | `client-root/gamerules.py` (`RULES_VERSION` bumped so the new terms show once) and `client-root/intrologin.py` (login-window buttons: GitHub, buycoffee, our Discord), `uiitemshop.py` + `itemshop_subscriptionwindow.py` (the coin and subscription buttons open buycoffee), `uisystem.py` (support opens the Discord), `uitooltip.py` (the GM branch guarded - it killed every item tooltip for a GM - and the speed potion's tooltip compares with `getattr(item, "APPLY_ATT_SPEED", 17)` and `19`, names this client's item module does not export), `offlineshopmanage.py` (an empty slot of the shop's edit grid removes nothing). |

`playerbotify.py` also flips the db core's `m_bMaintenance(TRUE)` to `FALSE`:
the package boots every world closed until a GM types `/maintenance 0`, and an
ordinary login — `admin` included — was answered `MAINTENA` ("Obecnie trwa
przerwa techniczna") at the last step. The GM command still closes the world.

The package also ships an empty `common.gmlist` and no character on the tester
account, where r40250 had `[SA]Admin` with an IMPLEMENTOR row. GM rights are
the pair (account, character name) — `gm.cpp` checks the account and, under
`GERMAN_GM_NOT_CHECK_HOST`, never the host — so
`docker/mariadb/playerbot/gm_characters.sql` (run by `10-import-dumps.sh` on a
fresh world and by `apply.sh` on every start, the directory is mounted into
both containers) creates four: `Admin` (warrior), `AdminNinja`, `AdminSura`,
`AdminSzaman`, PIDs 9001-9004, level ninety on Joan, each with the best +9 set
**the client can draw** for its class — the level-66 armour (shape 12) and the
level-75 weapon: the client's `pc2/*.msm` know armour shapes 0-12, 14-22 and 24
only and its `item` pack has weapon models up to the level-75 set, so 2.0.4's
level-90 armour (shape 13) made the character invisible and immobile (bonus
lines are POINT ids here — `item.attrtype`
holds POINT numbers on this engine, and the two damage lines sit in slots 5
and 6 as `item_addon.cpp` writes them), a second weapon, potions and scrolls,
a level-21 horse (`c_aHorseStat[21]`: 35 health, 120 stamina) and its summon
book 50053 — and gmlist rows for all four. Only when the admin account has no
character at all; otherwise `apply.sh` grants IMPLEMENTOR to the account's
oldest character on a world whose list is empty (the 2.0.0/2.0.1 installs).
Once only, and never on a list that has anything in it.

A player's new character gets its starter chest from `starter_chest.quest`
(`files/`, copied into `docker/game/quest/` and named in the Dockerfile's
quest list): one Skrzynia Ucznia I by class at the first login at level five
or under, the chest the seed gives every bot. The db core of this package
has no starting-item step, so without it a player started with nothing.

An edit whose idempotency marker is its own inserted text stops being
idempotent the moment a later edit changes that text: the bot-load struct and
its db case were inserted twice that way (two `SBotPlayerLoadPacket`s, two
`case` labels). Such an edit names a marker that survives — a line the later
edit leaves alone — and `playerbotify.py` run twice over a pristine tree must
leave every file identical; that is checked, not assumed.
| `port/listify.py [--pristine <Server Source\Server>]` | the staged engine against the pristine package | `launcher/server-update-files.mt2009.txt`: every engine file the port changed or added (76 at the time of writing) plus the tree's own files and the shared ones. |

## Why the player's tree is still called `linux-port`

`Metin2-Launcher.ps1`, the GUI, `start-server.ps1`, the diagnostics module and
the packager name `linux-port\docker` in some ninety places. Rather than
parameterise all of them, the mt2009 tree is *deployed* under that name: the
packager is run with

```
-FileList launcher\server-update-files.mt2009.txt
-PathMap @{ 'linux-port-mt2009/docker/docker-compose.deploy.yml' = 'linux-port/docker/docker-compose.yml';
            'linux-port-mt2009/VERSION' = 'VERSION';
            'linux-port-mt2009/PACZKA_INFO.txt' = 'PACZKA_INFO.txt';
            'linux-port-mt2009/' = 'linux-port/' }
```

The two single-file rows are not decoration and this block was missing them
until 2.0.36. The map is applied longest key first, so without its own row
`linux-port-mt2009/VERSION` is published as `linux-port/VERSION` - a path
nothing on a player's machine reads. `tools/update.sh` reads `<root>/VERSION`
both to report what is installed and to decide whether an update is needed at
all, so the number never moved: the updater said "the server is now running
version 2.0.34" after installing 2.0.35, and downloaded and unpacked the same
release again on the next run (Mkls, 13 September). `New-M2DeployTree.ps1` has
always carried the full map; this text is what a release was typed from, so
the two are kept identical, and the packager now refuses a server package with
no `VERSION` at its root.

and the launcher tells the two apart by `linux-port\docker\ENGINE`
(`Get-M2ServerEngine`). Engine-specific in the launcher: the dumps a world is
made from (`world.sql` instead of `hotbackup.sql`), the r40250 engine patches
(never applied here — the tree ships patched), the overlay seed (never copied
over the rendered one) and what a complete build context holds
(`Get-M2RequiredGameContext`; `start-server.ps1` carries the same lists because
it imports no module). `tools/check-update-covers-build.py --docker
linux-port-mt2009/docker --list launcher/server-update-files.mt2009.txt` is the
release-time check.

## How it reaches a player

This line is versioned on its own (`VERSION`, 2.x) and has its own update
channel, `update-manifest-mt2009.json` at the repository root: the launcher
picks it by the `ENGINE` marker (`Get-M2DefaultLauncherConfig`), so an r40250
install (1.33.x, `update-manifest.json`) never sees these releases and this
one never sees those. `CHANGELOG.md` is shared; `PACZKA_INFO.txt` is this
line's own.

There is no installer that fetches sources. What a player downloads from the
hosting is **one full zip** — `Metin2 Singleplayer\{CZYTAJ.txt, Klient\, Serwer\}`
— built by two tools in `tools/`:

- `New-M2DeployTree.ps1 -Deploy <dir>` renders `Serwer\`: the update package
  (packager with the `-PathMap` above) unpacked, plus what no update carries —
  the staged engine, externals, runtime share, SQL dumps, docs, installer.
- `New-M2FullPackage.ps1 -Deploy <dir> -Client <Klient-127> -OutputDirectory <dir>`
  zips that beside the client (7-Zip), leaving out `.env`, the installation
  identity, logs, backups, and the client's credentials, settings and
  screenshots. The player's launcher creates a fresh `.env` and identity on
  first start; `..\Klient\metin2client.exe` is found without a dialog
  (`Get-M2SiblingClientExecutable`), and the client-update button is the plain
  client update, not the GM panel.

The zip never has to be re-uploaded for a server update: the update package
on GitHub carries every engine file the port changed (76 at the time of
writing, `port/listify.py` measures them) and overwrites the copies the zip
brought.

**Adoption is per engine.** `start-server.ps1` adopts the one existing Docker
installation it finds when the tree has no identity yet — right for a second
unpacked copy of the same line, wrong across lines: a player coming from
r40250 has exactly one existing stack, and adopting it would start mt2009
against a database volume with no `world` schema and an initdb that never
runs. `Select-SameEngineInstallations` reads the candidate's `ENGINE` file and
keeps only stacks of the same engine; another line's stack is neither adopted
nor counted as ambiguity.

## What the overlay does differently under `PLAYERBOT_ENGINE_MT2009`

`linux-port/overlays/playerbot/src/game/src/playerbot_engine_compat.h`, included
before `playerbot_types.h`:

- `APPLY_*` are `POINT_*` here (the engine has no `EApplyTypes`; `item_proto`
  applytype holds POINT numbers, `item_attr.apply` is an enum of POINT names,
  the two damage lines are 121/122 rather than 71/72 — the panels switch on
  `M2PANEL_ENGINE` / `PLAYERBOTS_ENGINE`); `AFF_*` names differ
  (`AFF_SKILL_BERSERK` and friends).
- `PlayerBotChangeGold` — `PointChange(POINT_GOLD, ..)` is refused outright
  ("unknown point change type 11"); `ChangeGold(YANG)` is the way.
- `PlayerBotCanEquipNow` / `PlayerBotEquipItem` — `CanEquipNow` is rate-limited
  to six answers per half second per pid (`PulseManager`, `ePulse::ItemEquip`);
  the gear pass asks once per bag piece, so the clock is cleared before every
  ask. 63 000 refused upgrades an hour before, none after.
- Fishing is a reaction test plus a bar minigame (`m_pkPreFishingEvent` bites
  after 17 s, take within 1700–3500 ms, then `fishing::Take` climbs the bar);
  `fishing()` wants level 50, maps 1/21/41 and the pass 27620. The overlay's
  pass drives it; the gate in `WantsPlayerBotFishingTrip` means bots do not
  fish here yet — a world decision, not a bug.
- `OpenMyShop` takes a time index, `SetSkillNextReadTime` a success flag,
  there is no `HEADER_GD_FLUSH_CACHE`, `SAFEBOX_PAGE_SIZE` is derived.
- A private shop is a right this engine grants at **level 15 and 800 kills**
  (`CHARACTER::CanOpenShop` reads the `stat_monster` special flag); the stall
  pass asks `CanOpenShop()` before the walk to the pitch and comes back after
  `PLAYERBOT_MT2009_SHOP_NOT_YET_RETRY`. A young world logged 39 engine
  refusals in a row before that gate existed. `OpenMyShop` also refuses a sign
  outside printable ASCII + CP1250 Polish (`has_proper_characters`), a sign the
  banword list catches, and a pitch within 60 units of another shop.
- The stall signs are the community's own (`playerbot_shop_signs.h`, Iwakura's
  list of 11 September 2026), chosen by what the counter mostly holds — fish,
  books, materials, gear, medals, scrolls, spirit stones — with the two item
  headlines (level-30 weapon, big refine) kept. Shared with r40250.

Known, not yet done: the `levelup` quest the hunting missions drive does not
exist in this package (its `hunting` quest is another shape), the desert boss
and other Phase-6 world measurements, `updater`/`client-builder`/`wsbridge`.

## Running it

Test stack on the development machine: `docker compose` in `docker/` with the
`.env` there (project `m2mt`, containers `m2mt-*`). The deploy directory the
launcher runs is `C:\Users\dawio\Downloads\Metin2 Singleplayer\Serwer`
(project `m2dep`). Both bind the same ports; stop one before starting the
other. `M2_APT_MIRROR` is set in both `.env` files because archive.ubuntu.com
is unreachable from this network.
