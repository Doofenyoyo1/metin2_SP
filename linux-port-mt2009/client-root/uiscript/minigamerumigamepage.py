# MT2009_PLUS_RUMI_V1: Owsap's uiscript/minigamerumigamepage.py (v6.2.6). The three
# card "slot" windows are plain windows here - uiminigamerumi.py puts an image box
# per card on them (our exe has no slot drawn from a grpImage pointer) - and the
# four "ani_image" effects are made in code (FrameAnimation: key frames, reset,
# scale). The texts come from uiminigamerumi.py (Polish, CP1250).
import uiScriptLocale

ROOT = "d:/ymir work/ui/minigame/rumi/"

WINDOW_WIDTH	= 352
WINDOW_HEIGHT	= 384

CARD_IMG_WIDTH	= 38#40
CARD_IMG_HEIGHT	= 52#54

DECK_IMG_WIDTH	= 42
DECK_IMG_HEIGHT = 56

HAND_CARD_GAP	= 21
FIELD_CARD_GAP	= 5

window = {
	"name" : "RumiGamePage",
	"style" : ("movable", "float",),
	
	"x" : SCREEN_WIDTH / 2 - WINDOW_WIDTH / 2,
	"y" : SCREEN_HEIGHT / 2 - WINDOW_HEIGHT / 2,
	
	"width" : WINDOW_WIDTH,
	"height" : WINDOW_HEIGHT,
	
	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),
			
			"x" : 0,
			"y" : 0,
			
			"width" : WINDOW_WIDTH,
			"height" : WINDOW_HEIGHT,
			
			"children" :
			(				
				{
					"name" : "titlebar",
					"type" : "titlebar",
					"style" : ("attach",),

					"x" : 0,
					"y" : 0,

					"width" : WINDOW_WIDTH,
					"color" : "yellow",

					"children" :
					(
						{ "name":"TitleName", "type":"text", "x":0, "y":0, "text": uiScriptLocale.MINI_GAME_RUMI_TITLE, "all_align":"center" },
					),
				},
	
				{
					"name" : "game_exit_button",
					"type" : "button",
					
					"x" : 133,
					"y" : 349,
					
					"text" : uiScriptLocale.MINI_GAME_RUMI_EXIT,
					
					"default_image" : "d:/ymir work/ui/public/large_button_01.sub",
					"over_image" : "d:/ymir work/ui/public/large_button_02.sub",
					"down_image" : "d:/ymir work/ui/public/large_button_03.sub",
				},

				{
					"name" : "BG",
					"type" : "expanded_image",
					"style" : ("attach",),
					
					"x" : 13,
					"y" : 35,

					"image" : ROOT + "rumi_bg.tga",
				},
				
				{
					"name" : "HandCardSlot",
					"type" : "window",
					
					"x" : 39,
					"y" : 60,
					
					"width" : CARD_IMG_WIDTH * 5 + HAND_CARD_GAP * 4,
					"height" : CARD_IMG_HEIGHT,
					
				},
				
				{
					"name" : "FieldCardSlot",
					"type" : "window",
					
					"x" : 114,
					"y" : 149,
					
					"width" : CARD_IMG_WIDTH * 3 + FIELD_CARD_GAP * 2,
					"height" : CARD_IMG_HEIGHT,
					
				},
				
				{
					"name" : "DeckCardSlot",
					"type" : "window",
					
					"x" : 46,
					"y" : 234,
					
					"width" : DECK_IMG_WIDTH,
					"height" : DECK_IMG_HEIGHT,
					
					
				},
				
				{
					"name" : "score_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 243,
					"y" : 168,
					
					"width" : 29,
					"height" : 18,
					
					"children" :
					(
						{
							"name" : "score_number_text",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							"all_align":"center"
						},
					),
				},
				
				{
					"name" : "card_cnt_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 112,
					"y" : 273,
					
					"width" : 24,
					"height" : 18,
					
					"children" :
					(
						{
							"name" : "card_cnt_text",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							"all_align":"center"
						},
					),
				},
				
				{
					"name" : "total_score_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 218,
					"y" : 273,
					
					"width" : 90,
					"height" : 18,
					
					"children" :
					(										
						{
							"name" : "total_score",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							
							"text" : "0",
							"all_align":"center"
						},
					),
				},
				
				{
					"name" : "cross_text_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 92,
					"y" : 273,
					
					"width" : 20,
					"height" : 18,
					
					"children" :
					(										
						{
							"name" : "cross_text",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							
							"text" : "X",
							"all_align":"center"
						},
					),
				},
				
				{
					"name" : "total_score_text_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 138,
					"y" : 273,
					
					"width" : 60,
					"height" : 18,
					
					"children" :
					(										
						{
							"name" : "total_score_text",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							
							"text" : uiScriptLocale.MINI_GAME_RUMI_SCORE,
							"all_align":"center",
						},
					),
				},
				
				{
					"name" : "mouse_lbutton_img",
					"type" : "expanded_image",
					"style" : ("attach",),
					
					"x" : 18,
					"y" : 314,

					"image" : ROOT + "mouse_lbutton.sub",
				},
				
				{
					"name" : "mouse_lbutton_desc_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 42,
					"y" : 330,
					
					"width" : 167,
					"height" : 10,
					
					"children" :
					(										
						{
							"name" : "mouse_lbutton_desc_text",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							
							"text" : uiScriptLocale.MINI_GAME_RUMI_LBUTTON_DESC,
							"text_horizontal_align" : "left",
							"text_vertical_align" : "center",
							
							"r":145/255.0,
							"g":134/255.0,
							"b":122/255.0,
						},
					),
				},
				
				{
					"name" : "mouse_rbutton_img",
					"type" : "expanded_image",
					"style" : ("attach",),
					
					"x" : 188,
					"y" : 314,

					"image" : ROOT + "mouse_rbutton.sub",
				},
				
				{
					"name" : "mouse_rbutton_desc_window",
					"type" : "window",
					"style" : ("attach",),
					
					"x" : 214,
					"y" : 330,
					
					"width" : 167,
					"height" : 10,
					
					"children" :
					(										
						{
							"name" : "mouse_rbutton_desc_text",
							"type" : "text",
							
							"x" : 0,
							"y" : 0,
							
							"text" : uiScriptLocale.MINI_GAME_RUMI_RBUTTON_DESC,
							"text_horizontal_align" : "left",
							"text_vertical_align" : "center",
							
							"r":145/255.0,
							"g":134/255.0,
							"b":122/255.0,
						},
					),
				},
			),
		},
	),	
}
