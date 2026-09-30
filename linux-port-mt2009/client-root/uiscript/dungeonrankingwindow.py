# MT2009_PLUS_DUNGEON_PANEL_V1: "border_a" is our board with the "a" skin.
import uiScriptLocale
# import app

BOARD_WIDTH = 400
BOARD_HEIGHT = 344
BACK_IMG_PATH = "d:/ymir work/ui/public/public_board_back/"
ROOT_PATH = "d:/ymir work/ui/game/guild/dragonlairranking/"

window = {
	"name" : "DungeonRankingWindow",
	"style" : ("movable", "float",),

	"x" : 0,
	"y" : 0,

	"width" : BOARD_WIDTH,
	"height" : BOARD_HEIGHT,

	"children" :
	(
		{
			"name" : "Board",
			"type" : "board",
			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width" : BOARD_WIDTH,
			"height" : BOARD_HEIGHT,

			"children" :
			(
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 6,
					"y" : 6,

					"width" : BOARD_WIDTH - 13,

					"children" :
					(
						{
							"name" : "TitleBarName",
							"type" : "text",

							"x" : 0,
							"y" : -3,

							"text" : uiScriptLocale.DUNGEON_RANKING,
                            "outline":"1",
							"all_align" : "center"
						},
					),
				},
				{
					"name" : "border_a",
					"type" : "board",
					"skin" : "a",
					"width": 375,
					"height" : 268,
					"x":0,"y":30,
					"horizontal_align":"center",
					"children":
					(
						{
							"name" : "RankingTiTleImg",
							"type" : "image",
							"x" : 1,
							"y" : 3,
							"horizontal_align":"center",
							"image" : "d:/ymir work/ui/game/dungeon_info/ranking_header.png",
						},
					)
				},
				{
					"name" : "border_a1",
					"type" : "board",
					"skin" : "a",
					"width": 375,
					"height" : 20,
					"x":0,"y":300, 
					"horizontal_align":"center",
				},
				{
					"name" : "WaitAniImg",
					"type" : "window",

					"x" : 0,
					"y" : 40,
					"width" : 32,
					"height" : 128,
					"horizontal_align" : "center",
					"vertical_align" : "center",

					

					"children" :
					(
						{
							"name" : "WaitTextLine",
							"type" : "text",

							"x" : 2,
							"y" : -15,

							"text" : "...",
							"all_align" : "center"
						},
					)
				},
			),
		},
	),
}
