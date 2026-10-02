# MT2009_PLUS_SEONHAE_V1: Seon-Hae's 6th/7th bonus window - Owsap v6.2.6's
# uiscript/attr67adddialog.py (312 x 224, the same places), uiseonhae.py
# fills it. The GF graphics (d:/ymir work/ui/game/attr6th7th/*.sub on
# d:/ymir work/ui/properties_01.dds) are used when the packs have them; a
# client without them draws the plain slot frames and small "+"/"-" buttons
# in their places instead, so the window never fails to load.
import app
import uiScriptLocale

PATTERN_PATH = "d:/ymir work/ui/pattern/"
ROOT = "d:/ymir work/ui/game/attr6th7th/"
SLOT_BASE = "d:/ymir work/ui/public/slot_base.sub"


def _Exists(path):
	try:
		return bool(app.IsExistFile(path))
	except Exception:
		return False

GF_ART = _Exists(ROOT + "regist_slot.sub") and _Exists(ROOT + "arrow_up_default.sub") and _Exists("d:/ymir work/ui/properties_01.dds")

WINDOW_WIDTH = 312
WINDOW_HEIGHT = 224

PANEL_WIDTH = 144
PANEL_HEIGHT = 156
PANEL_PATTERN_X_COUNT = ((PANEL_WIDTH - 32) / 16) - 1
PANEL_PATTERN_Y_COUNT = (PANEL_HEIGHT - 32) / 16

TITLE_COLOR = 0xFFD8CAC2

TITLE_REGIST = "Przedmiot"
TITLE_MATERIAL = "Od\xb3amki"
TITLE_SUPPORT = "Suplementy"
TITLE_WINDOW = "Seon-Hae: 6. i 7. bonus"
BUTTON_ADD = "Dodaj bonus"


def _Panel(name, x, y, children):
	base = [
		{"name": name + "LeftTop", "type": "image", "style": ("ltr",), "x": 0, "y": 0, "image": PATTERN_PATH + "border_A_left_top.tga"},
		{"name": name + "RightTop", "type": "image", "style": ("ltr",), "x": PANEL_WIDTH - 16, "y": 0, "image": PATTERN_PATH + "border_A_right_top.tga"},
		{"name": name + "LeftBottom", "type": "image", "style": ("ltr",), "x": 0, "y": PANEL_HEIGHT - 16, "image": PATTERN_PATH + "border_A_left_bottom.tga"},
		{"name": name + "RightBottom", "type": "image", "style": ("ltr",), "x": PANEL_WIDTH - 16, "y": PANEL_HEIGHT - 16, "image": PATTERN_PATH + "border_A_right_bottom.tga"},
		{"name": name + "TopCenterImg", "type": "expanded_image", "style": ("ltr",), "x": 16, "y": 0, "image": PATTERN_PATH + "border_A_top.tga", "rect": (0.0, 0.0, PANEL_PATTERN_X_COUNT, 0)},
		{"name": name + "LeftCenterImg", "type": "expanded_image", "style": ("ltr",), "x": 0, "y": 16, "image": PATTERN_PATH + "border_A_left.tga", "rect": (0.0, 0.0, 0, PANEL_PATTERN_Y_COUNT)},
		{"name": name + "RightCenterImg", "type": "expanded_image", "style": ("ltr",), "x": PANEL_WIDTH - 16, "y": 16, "image": PATTERN_PATH + "border_A_right.tga", "rect": (0.0, 0.0, 0, PANEL_PATTERN_Y_COUNT)},
		{"name": name + "BottomCenterImg", "type": "expanded_image", "style": ("ltr",), "x": 16, "y": PANEL_HEIGHT - 16, "image": PATTERN_PATH + "border_A_bottom.tga", "rect": (0.0, 0.0, PANEL_PATTERN_X_COUNT, 0)},
		{"name": name + "CenterImg", "type": "expanded_image", "style": ("ltr",), "x": 16, "y": 16, "image": PATTERN_PATH + "border_A_center.tga", "rect": (0.0, 0.0, PANEL_PATTERN_X_COUNT, PANEL_PATTERN_Y_COUNT)},
	]
	return {
		"name": name, "type": "window", "style": ("attach", "ltr",),
		"x": x, "y": y, "width": PANEL_WIDTH, "height": PANEL_HEIGHT,
		"children": tuple(base + children),
	}


def _Title(name, x, y, text):
	children = []
	if GF_ART:
		children.append({"name": name + "_bg", "type": "image", "x": 0, "y": 0, "image": ROOT + "memu_text.sub"})
	children.append({"name": name, "type": "text", "x": 0, "y": 0, "text": text, "all_align": "center", "color": TITLE_COLOR})
	return {"name": name + "_window", "type": "window", "x": x, "y": y, "width": 138, "height": 21, "style": ("attach",), "children": tuple(children)}


def _Count(name, x, y):
	children = []
	if GF_ART:
		children.append({"name": name + "_bg", "type": "image", "x": 0, "y": 0, "image": ROOT + "material_count_text.sub"})
	children.append({"name": name, "type": "text", "x": 0, "y": 0, "text": "0", "all_align": "center"})
	return {"name": name + "_window", "type": "window", "x": x, "y": y, "width": 26, "height": 18, "style": ("attach",), "children": tuple(children)}


def _Arrows(prefix, x, y):
	if GF_ART:
		return [
			{"name": prefix + "_arrow_up_button", "type": "button", "x": x, "y": y,
			 "default_image": ROOT + "arrow_up_default.sub", "over_image": ROOT + "arrow_up_over.sub", "down_image": ROOT + "arrow_up_down.sub"},
			{"name": prefix + "_arrow_down_button", "type": "button", "x": x, "y": y + 17,
			 "default_image": ROOT + "arrow_down_default.sub", "over_image": ROOT + "arrow_down_over.sub", "down_image": ROOT + "arrow_down_down.sub"},
		]
	# xsmall_button is 38 x 19: "+" over "-", right of the count
	return [
		{"name": prefix + "_arrow_up_button", "type": "button", "x": x - 4, "y": y - 3, "text": "+",
		 "default_image": "d:/ymir work/ui/public/xsmall_button_01.sub", "over_image": "d:/ymir work/ui/public/xsmall_button_02.sub", "down_image": "d:/ymir work/ui/public/xsmall_button_03.sub"},
		{"name": prefix + "_arrow_down_button", "type": "button", "x": x - 4, "y": y + 17, "text": "-",
		 "default_image": "d:/ymir work/ui/public/xsmall_button_01.sub", "over_image": "d:/ymir work/ui/public/xsmall_button_02.sub", "down_image": "d:/ymir work/ui/public/xsmall_button_03.sub"},
	]


def _SlotFrame(name, x, y, rows):
	# the plain 32 x 32 frames under a slot, when the GF art (which has its
	# own) is not in the packs
	if GF_ART:
		return []
	return [{"name": "%s_base%d" % (name, i), "type": "image", "x": x, "y": y + 32 * i, "image": SLOT_BASE} for i in xrange(rows)]


## the item (left panel)
regist_children = [_Title("regist_slot_text", 3, 3, TITLE_REGIST)]
if GF_ART:
	regist_children.append({"name": "regist_slot_img", "type": "image", "x": 50, "y": 36, "image": ROOT + "regist_slot.sub"})
regist_children += _SlotFrame("regist_slot", 56, 42, 3)
regist_children.append({
	"name": "regist_slot", "type": "slot", "x": 56, "y": 42, "width": 32, "height": 96,
	"slot": ({"index": 0, "x": 0, "y": 0, "width": 32, "height": 96},),
})

## the shards and the additives (right panel)
process_children = [_Title("material_slot_text", 3, 3, TITLE_MATERIAL)]
if GF_ART:
	process_children.append({"name": "material_slot_bg", "type": "image", "x": 20, "y": 27, "image": ROOT + "material_slot.sub"})
process_children += _SlotFrame("material_slot", 28, 35, 1)
process_children.append({
	"name": "material_slot", "type": "slot", "x": 28, "y": 35, "width": 32, "height": 32,
	"slot": ({"index": 0, "x": 0, "y": 0, "width": 32, "height": 32},),
})
process_children.append(_Count("material_slot_count_text", 76, 42))
process_children += _Arrows("material_slot", 102, 34)

process_children.append(_Title("support_slot_text", 3, 79, TITLE_SUPPORT))
if GF_ART:
	process_children.append({"name": "support_slot_bg", "type": "image", "x": 20, "y": 103, "image": ROOT + "material_slot.sub"})
process_children += _SlotFrame("support_slot", 28, 111, 1)
process_children.append({
	"name": "support_slot", "type": "slot", "x": 28, "y": 111, "width": 32, "height": 32,
	"slot": ({"index": 0, "x": 0, "y": 0, "width": 32, "height": 32},),
})
process_children.append(_Count("support_slot_count_text", 76, 118))
process_children += _Arrows("support_slot", 102, 110)

window = {
	"name": "SeonHaeWindow",
	"style": ("movable", "float",),

	"x": SCREEN_WIDTH / 2 - WINDOW_WIDTH / 2,
	"y": SCREEN_HEIGHT / 2 - WINDOW_HEIGHT / 2,

	"width": WINDOW_WIDTH,
	"height": WINDOW_HEIGHT,

	"children":
	(
		{
			"name": "board",
			"type": "board_with_titlebar",
			"x": 0,
			"y": 0,
			"width": WINDOW_WIDTH,
			"height": WINDOW_HEIGHT,
			"title": TITLE_WINDOW,
		},
		_Panel("regist_window", 10, 32, regist_children),
		_Panel("process_window", 157, 32, process_children),

		## the chance, or the time Seon-Hae still needs
		{
			"name": "total_success_percent_text_window",
			"type": "window",
			"style": ("attach", "ltr",),
			"x": 14,
			"y": 192,
			"width": 184,
			"height": 18,
			"children":
			(
				{"name": "TotalSuccessWindowLeftImg", "type": "image", "style": ("ltr",), "x": 0, "y": 0, "image": PATTERN_PATH + "border_c_left.tga"},
				{"name": "TotalSuccessWindowCenterImg", "type": "expanded_image", "style": ("ltr",), "x": 21, "y": 0, "image": PATTERN_PATH + "border_c_middle.tga", "rect": (0.0, 0.0, 6, 0)},
				{"name": "TotalSuccessWindowRightImg", "type": "image", "style": ("ltr",), "x": 184 - 21, "y": 0, "image": PATTERN_PATH + "border_c_right.tga"},
				{"name": "TotalSuccessText", "type": "text", "x": 0, "y": 2, "all_align": "center", "text": ""},
			),
		},

		## "Dodaj bonus" / "Odbierz"
		{
			"name": "attr_add_button",
			"type": "button",
			"x": 209,
			"y": 193,
			"default_image": "d:/ymir work/ui/public/large_button_01.sub",
			"over_image": "d:/ymir work/ui/public/large_button_02.sub",
			"down_image": "d:/ymir work/ui/public/large_button_03.sub",
			"text": BUTTON_ADD,
		},

		## the "?" with the rules
		{
			"name": "question_button",
			"type": "button",
			"x": WINDOW_WIDTH - 30 - 16,
			"y": 9,
			"default_image": "d:/ymir work/ui/pattern/q_mark_01.tga",
			"over_image": "d:/ymir work/ui/pattern/q_mark_02.tga",
			"down_image": "d:/ymir work/ui/pattern/q_mark_01.tga",
		},
	),
}
