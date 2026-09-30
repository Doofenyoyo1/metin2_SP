#
# Title: Dungeon Information System
# Author: Owsap
# Description: List of all available dungeons.
# Date: 2021.01.09
# Last Update: 2021.06.03
# Version 2.0.0.2
#
# Skype: owsap.
# Discord: Owsap#0905
#
# 0x0
#
# Web: https://owsap-productions.com/
# GitHub: https://github.com/Owsap
#

# MT2009_PLUS_DUNGEON_PANEL_V1: as in the Arezzo client ("tooltip_text_new" is our "tooltip_text").
import uiScriptLocale

BOARD_WIDTH = 490
BOARD_HEIGHT = 463

INNER_RIGHT_BOARD_WIDTH = 190
INNER_RIGHR_BOARD_HEIGH = 26

INNER_RIGHT_BOARD_X = INNER_RIGHT_BOARD_WIDTH + 4
INNER_RIGHR_BOARD_Y = INNER_RIGHR_BOARD_HEIGH

ROOT = "d:/ymir work/ui/game/dungeon_info/"

window = {
	"name" : "DungeonInfoWindow",
	"style" : ("movable", "float",),

	"x" : 0,
	"y" : 0,

	"width" : BOARD_WIDTH,
	"height" : BOARD_HEIGHT,

	"children" :
	(
		## Main Window Board
		{
			"name" : "Board",
			"type" : "board",
			"style" : ("attach", "ltr"),

			"x" : 0,
			"y" : 0,

			"width" : BOARD_WIDTH,
			"height" : BOARD_HEIGHT,

			"children" :
			(
				## Main Window Title Bar
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 6,
					"y" : 7,

					"width" : BOARD_WIDTH - 13,

					"children" :
					(
						{
							"name" : "TitleText",
							"type" : "text",

							"x" : 0,
							"y" : -3,

							"text" : uiScriptLocale.DUNGEON_INFO_TITLE,
                            "outline":"1",
							"all_align" : "center"
						},
					),
				},

				{
					## Board Container
					"name" : "BoardContainer",
					"type" : "image",
					"style" : ("attach",),

					"x" : 0,
					"y" : 32,
					"image" : ROOT + "dungeon_allbg.png",

					"horizontal_align" : "center",

					"children" :
					(
						## Dungeon Title Name ( Background Image )
						{
							"name" : "TitleBackgroundImage",
							"type" : "image",
							"style" : ("attach",),

							"x" : 0,
							"y" : 3,
							"horizontal_align" : "center",

							"image" : ROOT + "header_dungeon.png",

							"children" : (
								## Dungeon Title Name ( Text )
								{
									"name" : "TitleNameText",
									"type" : "text",

									"x" : 10,
									"y" : 3,
									"text_horizontal_align" : "left",

									"text" : "",
									"fontname" : "Tahoma:14b",
									"color" : 0xFFC1C1C1,
									"outline" : 1,
								},


							),
						},

						## Dungeon Button List ( ThinBoard )
						{
							"name" : "ButtonListThinBoard",
							"type" : "thinboard_circle",

							"x" : 3,
							"y" : 28,

							"width" : 240,
							"height" : 388,
							"children" : (
								## ...
							),
						},

						## Dungeon Information Window
						{
							"name" : "InformationWindow",
							"type" : "window",

							"x" : INNER_RIGHT_BOARD_X,
							"y" : INNER_RIGHR_BOARD_HEIGH,
							"horizontal_align" : "right",

							"width" : INNER_RIGHT_BOARD_WIDTH,
							"height" : INNER_RIGHR_BOARD_HEIGH + 15,

							"children" : (
								{
									"name" : "InformationThinBoard",
									"type" : "thinboard_circle",

									"x" : INNER_RIGHT_BOARD_WIDTH+11,
									"y" : 2,

									"width":202,
									"height":388,
									"horizontal_align" : "right",
									"children": (
                                        {
											"name" : "InformationThinBoard2",
											"type" : "image",

											"x" : 3,
											"y" : 3,

											"image" : ROOT + "dungeon_bg.png",

											"children" : (
												## Dungeon Type
												{
													"name" : "TypeText", "type" : "text", "x" : 5, "y" : 1,							
                                                    "outline":1,
													"text" : "",
												},
												## Dungeon Level Limit
												{
													"name" : "LevelLimitText", "type" : "text", "x" : 5, "y" : 17,
                                                    "outline":1,
													"text" : "",
												},
												## Dungeon Party Limit ( Members )
												{
													"name" : "MemberLimitText", "type" : "text", "x" : 5, "y" : 33,
                                                    "outline":1,
													"text" : "",
												},
												# ## Dungeon Duration
												# {
												# 	"name" : "DurationText", "type" : "text", "x" : 5, "y" : 50,
                                                #     "outline":1,
												# 	"text" : "",
												# },
												## Dungeon Cooldown
												# {
												# 	"name" : "CooldownText", "type" : "text", "x" : 5, "y" : 66,
                                                #     "outline":1,
												# 	"text" : "",
												# },
												{
													"name" : "LocationText", "type" : "text", "x" : 5, "y" : 82,
                                                    "outline":1,
													"text" : "",
												},
												{
													"name" : "EntraceText", "type" : "text", "x" : 5, "y" : 98,
                                                    "outline":1,
													"text" : "",
												},
												# Personal Stats
												# {
                                                #     "name":"dungeoninfopersonalstatsimage",
                                                #     "type":"image",
                                                #     "x":-1,
                                                #     "y":188,
                                                #     "image":ROOT+"header.png",
                                                #     "children":
                                                #     (
												# 		{
												# 			"name" : "DungeonInfoPersonalStats",
												# 			"type" : "text",

												# 			"x" : 0,
												# 			"y" : 0,

												# 			"text" : "Twoje statystyki",
												# 			#"color" : 0xFFFEE3AE,
												# 			"fontname":"Tahoma:14b",
												# 			"all_align" : "center",
												# 			"outline":"1",
												# 		},
												# 	),
												# },
												{
													"name" : "TotalFinishedText", "type" : "text", "x" : 5, "y" : 210,"outline":1,
													"text" : "",
												},
												## Dungeon Fastest Time
												{
													"name" : "FastestTimeText", "type" : "text", "x" : 5, "y" : 227,"outline":1,
													"text" : "",
												},
												## Dungeon Highest Damage
												{
													"name" : "HighestDamageText", "type" : "text", "x" : 5, "y" : 243,"outline":1,
													"text" : "",
												},
											),
										},
									)
								},
							),
						},
						## Dungeon Rank Score Button
						{
							"name" : "RankScoreButton",
							"type" : "button",

							"x" : 197,
							"y" : 310,

							"tooltip_text" : uiScriptLocale.DUNGEON_RANKING_TYPE_TOOL_TIP_01,
							"tooltip_text_color" : 0xfff1e6c0,
							"tooltip_x" : 0,
							"tooltip_y" : -35,

                            "horizontal_align" : "right",

							"default_image" : ROOT + "button/rank_button00.sub",
							"over_image" : ROOT + "button/rank_button01.sub",
							"down_image" : ROOT + "button/rank_button02.sub",
						},
						## Dungeon Rank Time Button
						{
							"name" : "RankTimeButton",
							"type" : "button",

							"x" : 133,
							"y" : 310,

							"tooltip_text" : uiScriptLocale.DUNGEON_RANKING_TYPE_TOOL_TIP_02,
							"tooltip_text_color" : 0xfff1e6c0,
							"tooltip_x" : 0,
							"tooltip_y" : -35,

                            "horizontal_align" : "right",

							"default_image" : ROOT + "button/rank_button10.sub",
							"over_image" : ROOT + "button/rank_button11.sub",
							"down_image" : ROOT + "button/rank_button12.sub",
						},
						## Dungeon Rank Damage Button
						{
							"name" : "RankDamageButton",
							"type" : "button",

							"x" : 69,
							"y" : 310,

							"tooltip_text" : uiScriptLocale.DUNGEON_RANKING_TYPE_TOOL_TIP_03,
							"tooltip_text_color" : 0xfff1e6c0,
							"tooltip_x" : 0,
							"tooltip_y" : -35,

                            "horizontal_align" : "right",

							"default_image" : ROOT + "button/rank_button20.sub",
							"over_image" : ROOT + "button/rank_button21.sub",
							"down_image" : ROOT + "button/rank_button22.sub",
						},
						## Dungeon Box Drop ( Button )
						{
							"name" : "BoxButton",
							"type" : "toggle_button",

							"x" : 118,
							"y" : 375,
							"horizontal_align" : "right",

							"default_image" : ROOT+"dropbutton0.png",
							"over_image" : ROOT+"dropbutton1.png",
							"down_image" : ROOT+"dropbutton2.png",
						},
						{
							"name" : "ElementalImage",
							"type" : "image",
							"style" : ("attach",),

							"x" : 27, #0,
							"y" : 3,
							"horizontal_align" : "right",

							"image" : ROOT+"bonus.png",
						},
						{
							"name" : "DungeonInfoItem",
							"type" : "expanded_image",
							"style" : ("attach",),

							"x" : 270,
							"y" : 345,

							"image" : "d:/ymir work/ui/game/dungeon_info/req_slot_frame.tga",
							
							"children" :
							(
								{
									"name" : "RequiredItemSlot",
									"type" : "slot",

									"x" : 7,
									"y" : 7,

									"width" : 130,
									"height" : 32,

									"image" : "d:/ymir work/ui/public/Slot_Base.sub",

									"slot" : (
										{ "index" : 0, "x" : 0, "y" : 0, "width" : 32, "height" : 32 },
									),
								},
							),
						},
						{
							"name" : "WarpButton",
							"type" : "button",

							"x" : 113,
							"y" : 345,
							"horizontal_align" : "right",

							"tooltip_text" : uiScriptLocale.DUNGEON_INFO_GO_TO_ENTRANCE_TOOL_TIP,
							"tooltip_text_color" : 0xfff1e6c0,
							"tooltip_x" : 0,
							"tooltip_y" : -35,


							"default_image" : ROOT+"teleportbutton0.png",
							"over_image" : ROOT+"teleportbutton1.png",
							"down_image" : ROOT+"teleportbutton2.png",
						},
						{
							"name" : "BoxWindow",
							"type" : "window",

							"x" : 130,
							"y" : 28,
							"horizontal_align" : "center",

							"width" : 201,
							"height" : 329,

							"children" : (
								{
									"name" : "BoxBackgroundImg",
									"type" : "expanded_image",

									"x" : 0,
									"y" : 0,

									"width" : 201,
									"height" : 329,

									"image" : ROOT+"slot_grid.png",

									"children" :
									(
										{
											"name" : "BoxItemSlot",
											"type" : "grid_table",

											"x" : 7,
											"y" : 6,

											"start_index" : 0,
											"x_count" : 6,
											"y_count" : 10,
											"x_step" : 32,
											"y_step" : 32,

											"image" : "d:/ymir work/ui/public/Slot_Base.sub",
										},
									),
								},
							)
						},
					),
				},
			)
		},
	)
}
