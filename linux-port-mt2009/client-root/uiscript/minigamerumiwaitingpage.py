# MT2009_PLUS_RUMI_V1: Owsap's uiscript/minigamerumiwaitingpage.py (v6.2.6), its
# __OKEY_EVENT_FLAG_RENEWAL__ page only (the card and card set counters). The texts
# are set on uiScriptLocale by uiminigamerumi.py (Polish, CP1250).
import uiScriptLocale


ROOT = "d:/ymir work/ui/game/"
PUBLIC_PATH = "d:/ymir work/ui/public/"

WINDOW_WIDTH	= 352
WINDOW_HEIGHT	= 384


PATTERN_PATH				= "d:/ymir work/ui/pattern/"
ROOT_PATH					= "d:/ymir work/ui/event/"

BACKGROUND_WINDOW_WIDTH		= 330
BACKGROUND_WINDOW_HEIGHT	= 300

SLOT_IMAGE_WIDTH			= 48
SLOT_IMAGE_HEIGHT			= 48

SLOT_WIDTH					= 32
SLOT_HEIGHT					= 32

ITEM_COUNT_TEXT_POS_X		= 1
ITEM_COUNT_TEXT_POS_Y		= 3

window = {
	"name"		: "RumiWaitingPage",
	"style"		: ("movable", "float", ),

	"x"			: SCREEN_WIDTH / 2 - WINDOW_WIDTH / 2,
	"y"			: SCREEN_HEIGHT / 2 - WINDOW_HEIGHT / 2,

	"width"		: WINDOW_WIDTH,
	"height"	: WINDOW_HEIGHT,

	"children" :
	(
		{
			"name"		: "board",
			"type"		: "board_with_titlebar",
			#"style"		: ("attach",),
		
			"x"			: 0,
			"y"			: 0,
		
			"width"		: WINDOW_WIDTH,
			"height"	: WINDOW_HEIGHT,
		
			"title"		: uiScriptLocale.MINI_GAME_RUMI_TITLE,
		
			"children" :
			(
				## titlebar
				#{
				#	"name"	: "titlebar",
				#	"type"	: "titlebar",
				#	"style" : ("attach",),
				#
				#	"x"		: 0,
				#	"y"		: 0,
				#
				#	"width" : WINDOW_WIDTH,
				#	"color" : "yellow",
				#
				#	"children" :
				#	(
				#		{ "name":"TitleName", "type":"text", "x":0, "y":0, "text": uiScriptLocale.MINI_GAME_RUMI_TITLE, "all_align":"center" },
				#	),
				#},

				## background
				{
					"name"		: "desc_board",
					"type"		: "window",
					"style"		: ("ltr", "attach", ),
				
					"x"			: 10,
					"y"			: 32,

					"width"		: BACKGROUND_WINDOW_WIDTH,
					"height"	: BACKGROUND_WINDOW_HEIGHT,

					"children" :
					(
						{
							"name"	: "line_window_left_top",
							"type"	: "image",
							"style" : ("ltr",),
						
							"x"		: 0,
							"y"		: 0,

							"image" : PATTERN_PATH + "border_A_left_top.tga",
						},

						{
							"name"	: "line_window_right_top",
							"type"	: "image",
							"style" : ("ltr",),
						
							"x"		: BACKGROUND_WINDOW_WIDTH - 16,
							"y"		: 0,

							"image" : PATTERN_PATH + "border_A_right_top.tga",
						},

						{
							"name"	: "line_window_left_bottom",
							"type"	: "image",
							"style" : ("ltr",),
						
							"x"		: 0,
							"y"		: BACKGROUND_WINDOW_HEIGHT - 16,

							"image" : PATTERN_PATH + "border_A_left_bottom.tga",
						},

						{
							"name"	: "line_window_right_bottom",
							"type"	: "image",
							"style" : ("ltr",),
						
							"x"		: BACKGROUND_WINDOW_WIDTH - 16,
							"y"		: BACKGROUND_WINDOW_HEIGHT - 16,

							"image" : PATTERN_PATH + "border_A_right_bottom.tga",
						},

						{
							"name"	: "line_window_top_center",
							"type"	: "expanded_image",
							"style" : ("ltr",),
						
							"x"		: 16,
							"y"		: 0,

							"image" : PATTERN_PATH + "border_A_top.tga",
							"rect"	: (0.0, 0.0, 18, 0),
						},

						{
							"name"	: "line_window_left_center",
							"type"	: "expanded_image",
							"style" : ("ltr",),
						
							"x"		: 0,
							"y"		: 16,

							"image" : PATTERN_PATH + "border_A_left.tga",
							"rect"	: (0.0, 0.0, 0, 16),
						},

						{
							"name"	: "line_window_right_center",
							"type"	: "expanded_image",
							"style" : ("ltr",),
						
							"x"		: BACKGROUND_WINDOW_WIDTH - 16,
							"y"		: 16,

							"image" : PATTERN_PATH + "border_A_right.tga",
							"rect"	: (0.0, 0.0, 0, 16),
						},

						{
							"name"	: "line_window_bottom_center",
							"type"	: "expanded_image",
							"style" : ("ltr",),
						
							"x"		: 16,
							"y"		: BACKGROUND_WINDOW_HEIGHT - 16,
							"image" : PATTERN_PATH + "border_A_bottom.tga",
							"rect"	: (0.0, 0.0, 18, 0),
						},

						{
							"name"	: "line_window_center",
							"type"	: "expanded_image",
							"style" : ("ltr",),
						
							"x"		: 16,
							"y"		: 16,
							"image" : PATTERN_PATH + "border_A_center.tga",
							"rect"	: (0.0, 0.0, 18, 16),
						},

						{
							"name"	: "line_window_horizontal_boundary_left_line",
							"type"	: "image",
							"style" : ("ltr",),
						
							"x"		: 0,
							"y"		: 110,

							"image" : ROOT_PATH + "horizontal_line_left.sub",
						},

						{
							"name"	: "line_window_horizontal_boundary_right_line",
							"type"	: "image",
							"style" : ("ltr",),
						
							"x"		: 322,
							"y"		: 110,

							"image" : ROOT_PATH + "horizontal_line_right.sub",
						},

						{
							"name"	: "line_window_middle_boundary_center_line",
							"type"	: "expanded_image",
							"style" : ("ltr",),
						
							"x"		: 8,
							"y"		: 110,

							"image" : PATTERN_PATH + "border_A_top.tga",
							"rect"	: (0.0, 0.0, 19, 0),
						},
					),
				},					
				
				## slot
				{
					"name"		: "slot_window",
					"type"		: "window",
					"style"		: ("ltr", "attach", ),
				
					"x"			: 10,
					"y"			: 32,

					"width"		: BACKGROUND_WINDOW_WIDTH,
					"height"	: BACKGROUND_WINDOW_HEIGHT,

					"children" :
					(
						## rumi_card_piece_slot
						{
							"name"				: "rumi_card_piece_slot_bg",
							"type"				: "image",
						
							"x"					: -32,
							"y"					: 8,
						
							"width"				: SLOT_IMAGE_WIDTH,
							"height"			: SLOT_IMAGE_HEIGHT,

							"horizontal_align"	: "center",
						
							"image"				: ROOT_PATH + "slot.sub",

							"children" :
							(
								{
									"name"		: "rumi_card_piece_slot",
									"type"		: "slot",
								
									"x"			: 0,
									"y"			: 0,
								
									"width"		: SLOT_IMAGE_WIDTH,
									"height"	: SLOT_IMAGE_HEIGHT,

									"slot" : 
									(
										{"index":0, "x":8, "y":8, "width":SLOT_WIDTH, "height":SLOT_HEIGHT},
									),
								},
								## rumi_card_piece_count_text
								{
									"name"		: "rumi_card_piece_count_text_bg",
									"type"		: "image",

									"x"			: 50,
									"y"			: SLOT_HEIGHT / 2,
						
									"image"		: ROOT_PATH + "count_bg2.sub",

									"children" :
									(
										{
											"name"					: "rumi_card_piece_count_text",
											"type"					: "text",

											"x"						: ITEM_COUNT_TEXT_POS_X,
											"y"						: ITEM_COUNT_TEXT_POS_Y,

											"horizontal_align"		: "center",
											"text_horizontal_align"	: "center",

											"text"					: "",
										},
									),
								},
							),
						},

						## rumi_card_slot
						{
							"name"				: "rumi_card_slot_bg",
							"type"				: "image",
						
							"x"					: -32,
							"y"					: 55,
						
							"width"				: SLOT_IMAGE_WIDTH,
							"height"			: SLOT_IMAGE_HEIGHT,
							
							"horizontal_align"	: "center",
						
							"image"				: ROOT_PATH + "slot.sub",

							"children" :
							(
								{
									"name"		: "rumi_card_slot",
									"type"		: "slot",
								
									"x"			: 0,
									"y"			: 0,
								
									"width"		: SLOT_IMAGE_WIDTH,
									"height"	: SLOT_IMAGE_HEIGHT,

									"slot" : 
									(
										{"index":0, "x":8, "y":8, "width":SLOT_WIDTH, "height":SLOT_HEIGHT},
									),
								},

								## rumi_card_count_text
								{
									"name"	: "rumi_card_count_text_bg",
									"type"	: "image",
							
									"x"		: 50,
									"y"		: SLOT_HEIGHT / 2,
						
									"image"	: ROOT_PATH + "count_bg2.sub",

									"children" :
									(
										{
											"name"					: "rumi_card_count_text",
											"type"					: "text",

											"x"						: ITEM_COUNT_TEXT_POS_X,
											"y"						: ITEM_COUNT_TEXT_POS_Y,

											"horizontal_align"		: "center",
											"text_horizontal_align"	: "center",

											"text"					: "",
										},
									),
								},
							),
						},
					),
				},
				{
					"name"				: "prev_button",
					"type"				: "button",

					"x"					: WINDOW_WIDTH -30 -20 -20 -10,
					"y"					: 75,
				
					"vertical_align"	: "bottom",
				
					"default_image"		: PUBLIC_PATH + "public_intro_btn/prev_btn_01.sub",
					"over_image"		: PUBLIC_PATH + "public_intro_btn/prev_btn_02.sub",
					"down_image"		: PUBLIC_PATH + "public_intro_btn/prev_btn_01.sub",
				},
			
				{
					"name"				: "next_button",
					"type"				: "button",

					"x"					: WINDOW_WIDTH - 30 -20,
					"y"					: 75,
				
					"vertical_align"	: "bottom",
				
					"default_image"		: PUBLIC_PATH + "public_intro_btn/next_btn_01.sub",
					"over_image"		: PUBLIC_PATH + "public_intro_btn/next_btn_02.sub",
					"down_image"		: PUBLIC_PATH + "public_intro_btn/next_btn_01.sub",
				},
			
				{
					"name"				: "game_start_button",
					"type"				: "button",
				
					"x"					: 40,
					"y"					: 40,
				
					"text"				: uiScriptLocale.MINI_GAME_RUMI_START_TEXT,
				
					"vertical_align"	: "bottom",
					"horizontal_align"	: "left",
				
				
					"default_image"		: "d:/ymir work/ui/public/large_button_01.sub",
					"over_image"		: "d:/ymir work/ui/public/large_button_02.sub",
					"down_image"		: "d:/ymir work/ui/public/large_button_03.sub",
				},
			
				{
					"name"		: "confirm_check_button_text_window",
					"type"		: "window",
					"style"		: ("attach",),
				
					"x"			: 275,
					"y"			: 348,
				
					"width"		: 29,
					"height"	: 18,
				
					"children" :
					(
						{
							"name"					: "confirm_check_button_text",
							"type"					: "text",
						
							"x"						: 0,
							"y"						: 0,
						
							"text_horizontal_align" : "right",
						
							"text"					: uiScriptLocale.MINI_GAME_RUMI_DISCARD_TEXT,
						},
					),
				},
			
				{
					"name"	: "confirm_check_button",
					"type"	: "expanded_image",
				
					"x"		: 285,
					"y"		: 344,
				
					"image" : "d:/ymir work/ui/public/Parameter_Slot_07.sub",
				},
				{
					"name"	: "check_image",
					"type"	: "expanded_image",
					"style" : ("not_pick",),
				
					"x"		: 290,
					"y"		: 344,
				
					"image" : "d:/ymir work/ui/public/check_image.sub",
				},
			
			),
		},
	),	
}
