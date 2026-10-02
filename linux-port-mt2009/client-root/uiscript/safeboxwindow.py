import uiScriptLocale
import flamewindPath

window = {
	"name" : "SafeboxWindow",

	"x" : 100,
	"y" : 20,

	"style" : ("movable", "float",),

	"width" : 176,
	"height" : 250,

	"children" :
	(
		{
			"name" : "board",
			"type" : "board",

			"x" : 0,
			"y" : 0,

			"width" : 176,
			"height" : 250,

			"children" :
			(
				## Title
				{
					"name" : "TitleBar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 8,
					"y" : 7,

					"width" : 161,
					"color" : "yellow",

					"children" :
					(
						{ "name":"TitleName", "type":"text", "x":77, "y":3, "text":uiScriptLocale.SAFE_TITLE, "text_horizontal_align":"center" },

						# MT2009_PLUS_SAFEBOX_ARRANGE_V1 (uisafebox.py): the
						# inventory's two ways to tidy (inventoryarrange.py),
						# "Uloz i scal" left of the title and "Tylko scal
						# stosy" with the inventory's own icon right of it.
						{
							"name" : "SortButton",
							"type" : "button",
							"x" : 6,
							"y" : -1,
							"vertical_align": "center",
							"default_image" : flamewindPath.GetPublic("refresh_button_01"),
							"over_image" : flamewindPath.GetPublic("refresh_button_02"),
							"down_image" : flamewindPath.GetPublic("refresh_button_03"),
							"tooltip_text" : "U\xb3\xf3\xbf i scal",
							"tooltip_y": -19,
						},
						{
							"name" : "StackButton",
							"type" : "button",
							"x" : 42,
							"y" : -1,
							"horizontal_align": "right",
							"vertical_align": "center",
							"default_image" : flamewindPath.GetInventory("autostack_01"),
							"over_image" : flamewindPath.GetInventory("autostack_02"),
							"down_image" : flamewindPath.GetInventory("autostack_03"),
							"tooltip_text" : "Tylko scal stosy",
							"tooltip_y": -19,
							"tooltip_x": -30,
						},
					),
				},

				## Button
				{
					"name" : "ChangePasswordButton",
					"type" : "button",

					"x" : 0,
					"y" : 58,

					"text" : uiScriptLocale.SAFE_CHANGE_PASSWORD,
					"horizontal_align" : "center",
					"vertical_align" : "bottom",

					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
				},
				{
					"name" : "ExitButton",
					"type" : "button",

					"x" : 0,
					"y" : 37,

					"text" : uiScriptLocale.CLOSE,
					"horizontal_align" : "center",
					"vertical_align" : "bottom",

					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
				},

			),
		},
	),
}
