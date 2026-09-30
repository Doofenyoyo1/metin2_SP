import uiScriptLocale

LOCALE_PATH = uiScriptLocale.LOGIN_PATH
SERVER_BOARD_HEIGHT = 220 + 30
SERVER_LIST_HEIGHT = 171 + 30
window = {
	"name" : "LoginWindow",
	"sytle" : ("movable",),

	"x" : 0,
	"y" : 0,

	"width" : SCREEN_WIDTH,
	"height" : SCREEN_HEIGHT,

	"children" :
	(

		## Black under the picture, the whole screen: the client never clears
		## the screen, so a part of it no window draws over keeps the last
		## picture drawn there - a board after it was hidden. The picture
		## covers the screen (intrologin.py); this shows where one did not
		## load. clientrootify.py.
		{
			"name" : "BackgroundFill", "type" : "bar", "x" : 0, "y" : 0,
			"width" : SCREEN_WIDTH, "height" : SCREEN_HEIGHT,
			"color" : 0xff000000,
		},

		{
			"name" : "background", "type" : "expanded_image", "x" : 0, "y" : 0,
			"x_scale" : float(SCREEN_WIDTH) / 1280.0, "y_scale" : float(SCREEN_HEIGHT) / 768.0,
			# "horizontal_align" : "center",
			# "vertical_align" : "center",
			"image" : "locale/pl/ui/login_medium.jpg",
		},


		## ConnectBoard
		{
			"name" : "ConnectBoard",
			"type" : "thinboard",

			"x" : 208 + 30,
			"y" : 170 + 35,
			"width" : 208,
			"height" : 30,

			"horizontal_align" : "right",
			"vertical_align" : "bottom",

			"children" :
			(
				{
					"name" : "ConnectName",
					"type" : "text",

					"x" : 15,
					"y" : 0,
					"vertical_align" : "center",
					"text_vertical_align" : "center",

					"text" : uiScriptLocale.LOGIN_DEFAULT_SERVERADDR,
				},
				{
					"name" : "SelectConnectButton",
					"type" : "button",

					"x" : 150,
					"y" : 0,
					"vertical_align" : "center",

					"default_image" : "d:/ymir work/ui/public/small_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/small_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/small_button_03.sub",

					"text" : uiScriptLocale.LOGIN_SELECT_BUTTON,
				},
			),
		},

		{
			"name" : "Credentials",
			"type" : "window",

			"x" : 238,
			"y" : 355,

			"width" : 208,
			"height" : 105,
			"horizontal_align" : "right",
			"vertical_align" : "bottom",
		},

		## LoginBoard
		{
			"name" : "LoginBoard",
			"type" : "image",

			"x" : 208 + 30,
			"y" : 170,

			"horizontal_align" : "right",
			"vertical_align" : "bottom",

			"image" : LOCALE_PATH + "loginwindow.sub",

			"children" :
			(
				{
					"name" : "ID_EditLine",
					"type" : "editline",

					"x" : 77,
					"y" : 16,

					"width" : 120,
					"height" : 18,

					"input_limit" : 16,
					"enable_codepage" : 0,

					"r" : 1.0,
					"g" : 1.0,
					"b" : 1.0,
					"a" : 1.0,
				},
				{
					"name" : "Password_EditLine",
					"type" : "editline",

					"x" : 77,
					"y" : 43,

					"width" : 120,
					"height" : 18,

					"input_limit" : 16,
					"secret_flag" : 1,
					"enable_codepage" : 0,

					"r" : 1.0,
					"g" : 1.0,
					"b" : 1.0,
					"a" : 1.0,
				},
				{
					"name" : "LoginButton",
					"type" : "button",

					"x" : 15,
					"y" : 65,

					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

					"text" : uiScriptLocale.LOGIN_CONNECT,
				},
				{
					"name" : "LoginExitButton",
					"type" : "button",

					"x" : 105,
					"y" : 65,

					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

					"text" : uiScriptLocale.LOGIN_EXIT,
				},
			),
		},

		## ServerBoard
		{
			"name" : "ServerBoard",
			"type" : "thinboard",

			"x" : 0,
			"y" : SCREEN_HEIGHT - SERVER_BOARD_HEIGHT - 72,
			"width" : 375,
			"height" : SERVER_BOARD_HEIGHT,
			"horizontal_align" : "center",

			"children" :
			(

				## Title
				{
					"name" : "Title",
					"type" : "text",

					"x" : 0,
					"y" : 12,
					"horizontal_align" : "center",
					"text_horizontal_align" : "center",
					"text" : uiScriptLocale.LOGIN_SELECT_TITLE,
				},

				## Horizontal
				{
					"name" : "HorizontalLine1",
					"type" : "line",

					"x" : 10,
					"y" : 34,
					"width" : 354,
					"height" : 0,
					"color" : 0xff777777,
				},
				{
					"name" : "HorizontalLine2",
					"type" : "line",

					"x" : 10,
					"y" : 35,
					"width" : 355,
					"height" : 0,
					"color" : 0xff111111,
				},

				## Vertical
				{
					"name" : "VerticalLine1",
					"type" : "line",

					"x" : 246,
					"y" : 38,
					"width" : 0,
					"height" : SERVER_LIST_HEIGHT + 4,
					"color" : 0xff777777,
				},
				{
					"name" : "VerticalLine2",
					"type" : "line",

					"x" : 247,
					"y" : 38,
					"width" : 0,
					"height" : SERVER_LIST_HEIGHT + 4,
					"color" : 0xff111111,
				},

				## ListBox
				{
					"name" : "ServerList",
					"type" : "listbox2",

					"x" : 10,
					"y" : 40,
					"width" : 232,
					"height" : SERVER_LIST_HEIGHT,
					"row_count" : 15,
					"item_align" : 0,
				},
				{
					"name" : "ChannelList",
					"type" : "listbox",

					"x" : 255,
					"y" : 40,
					"width" : 109,
					"height" : SERVER_LIST_HEIGHT,

					"item_align" : 0,
				},

				## Buttons
				{
					"name" : "ServerSelectButton",
					"type" : "button",

					"x" : 267,
					"y" : SERVER_LIST_HEIGHT,

					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

					"text" : uiScriptLocale.OK,
				},
				{
					"name" : "ServerExitButton",
					"type" : "button",

					"x" : 267,
					"y" : SERVER_LIST_HEIGHT + 22,

					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

					"text" : uiScriptLocale.LOGIN_SELECT_EXIT,
				},

			),

		},

		{
			"name" : "HomePageButton",
			"type" : "button",

			"x" : 5,
			"y" : 150,

			"vertical_align" : "bottom",

			"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

			"bold" : True,
			"text" : uiScriptLocale.BUTTON_HOMEPAGE,
		},

		{
			"name" : "FacebookButton",
			"type" : "button",

			"x" : 5,
			"y" : 125,

			"vertical_align" : "bottom",

			"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

			"bold" : True,
			"text_color" : 0xff91a8e3,
			"text" : "Facebook",
		},

		{
			"name" : "DiscordButton",
			"type" : "button",

			"x" : 5,
			"y" : 100,

			"vertical_align" : "bottom",

			"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

			"bold" : True,
			"text_color" : 0xffb1a2de,
			"text" : "Discord",
		},

		{
			"name" : "SettingsButton",
			"type" : "button",

			"x" : 5,
			"y" : 65,

			"vertical_align" : "bottom",

			"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/large_button_03.sub",

			"bold" : True,

			"text" : "Ustawienia",
		},

		{
			"name" : "RulesButton",
			"type" : "button",

			"x" : 5,
			"y" : 40,

			"vertical_align" : "bottom",

			"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
			"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
			"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
			"bold" : True,
			"text" : "Zasady",
		},

		{
			"name" : "Version",
			"type" : "text",

			"x" : 5,
			"y" : 5,


			# "horizontal_align" : "right",
			# "text_horizontal_align" : "right",
			"vertical_align" : "bottom",
			"text_vertical_align" : "bottom",

			"outline" : True,
			"bold": True,
			"text" : "Wersja: xxx",
		},

	),
}
