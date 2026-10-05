# MT2009 PLUS New Pet System - the pet window (the operator, 28 September:
# "nowy pet system, ktory bedzie dzialal obok obecnego systemu petow").
# dracaryS' New Pet System rebuilt without a new client binary: the pet lives
# in the server's database (playerbot_newpet.h) and this window speaks to it
# with chat commands. Every server line is one command, "NewPet <what> ...":
#   NewPet Begin <pets> <open> <max pets>
#   NewPet Pet <id> <egg> <young mob> <hero mob> <level> <exp> <need> <evolution>
#              <level cap> <life left s> <life max s> <active> <out>
#              <bonus 0> <bonus 1> <bonus 2> <skills "type.level,..." x15>
#              <name hex> <species hex>
#   NewPet End
#   NewPet Evo <stage> <level> <yang> <vnum1> <count1> ... <vnum5> <count5> (3 to 5 pairs)
#   NewPet Hatch <cell> <egg vnum> <yang>  - an egg used: ask the name
#   NewPet Rename <id> <yang>              - Zwoj Imienia Peta used
#   NewPet Info <id> <name hex>            - a pet in a transporter (tooltip)
# and the window sends "/newpet open|sync|refresh|toggle|evolve <id>|select <id>|
# release <id>|skilldel <slot>|hatch <cell> <name>|rename <name>".
# U opens it, as does "/newpet" typed in the chat. A pet in its transporter
# (55007: socket 0 its id, 1 level + 1000 * evolution, 2 its egg) is shown by
# uitooltip.py through TransporterLines.
#
# The windows are dracaryS' own (uiscript/mt2009newpet.py and
# mt2009newpetname.py, the sprites cut out of the mod's dds files into
# mt2009_ui/newpet/): the pet window, its evolution window beside it and the
# hatching / renaming window.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import app
import dbg
import item
import net
import ui
import uiCommon
import uiToolTip
import wndMgr

REFRESH_SECONDS = 3.0
SKILL_LOCKED = 99
BOOK_BASE = 55009

EVOLUTION_NAMES = ('M\xb3ody', 'Dziki', 'Odwa\xbfny', 'Heroiczny')

# The three main bonuses: the name, the value at level 20, the unit.
BONUSES = (
	('Maks. P\xaf', 4000, ''),
	('Silny przeciwko potworom', 20, '%'),
	('Szansa na krytyczne uderzenie', 10, '%'),
)

# The skills (the book's value): what they give at level 20.
SKILLS = {
	1: ('Silny przeciwko Metinom', 10, '%'),
	2: ('Silny przeciwko bossom', 10, '%'),
	3: ('Silny przeciwko nieumar\xb3ym i diab\xb3om', 15, '%'),
	4: ('Silny przeciwko p\xf3\xb3ludziom', 5, '%'),
	5: ('Szybko\x9c\xe6 czarowania', 15, '%'),
	6: ('Silny przeciwko potworom', 10, '%'),
	7: ('Si\xb3a', 10, ''),
	8: ('Inteligencja', 10, ''),
	9: ('Zr\xeaczno\x9c\xe6', 10, ''),
	10: ('Obra\xbfenia umiej\xeatno\x9cci', 5, '%'),
	11: ('\x8cr. obra\xbfenia', 10, '%'),
	12: ('Odporno\x9c\xe6 na krytyczne', 10, '%'),
	13: ('Odporno\x9c\xe6 na przeszywaj\xb9ce', 10, '%'),
	14: ('Odporno\x9c\xe6 na p\xf3\xb3ludzi', 5, '%'),
	15: ('Blok', 10, '%'),
	16: ('Silny przeciwko bossom', 10, '%'),
	17: ('Odporno\x9c\xe6 na l\xf3d, ziemi\xea, ciemno\x9c\xe6', 10, '%'),
	18: ('Obrona przed \x9cr. obra\xbfeniami', 10, '%'),
	19: ('Obrona przed umiej\xeatno\x9cciami', 5, '%'),
	20: ('Podw\xf3jne do\x9cw./przedmioty (yang x2)', 10, '%'),
	21: ('Odporno\x9c\xe6 na trucizn\xea', 10, '%'),
	22: ('Witalno\x9c\xe6', 10, ''),
}

GOLD = 0xFFFEE3AE
WHITE = 0xFFFFFFFF
GREEN = 0xFF8EC292
RED = 0xFFE57875
GRAY = 0xFFA0A0A0

# The species by egg (the server's SPECIES).
SPECIES = {
	55401: 'Ma\xb3pka', 55402: 'Paj\xb9czek', 55403: 'Mini Razador', 55404: 'Mini Nemere',
	55405: 'Smoczek', 55406: 'Czerwony Smoczek', 55409: 'Ma\xb3y Baashido', 55410: 'Nessie',
	55411: 'Piskl\xea Exedyara',
}

IMG = 'mt2009_ui/newpet/'
# MT2009_PLUS_PET_SKILL_TOOLTIP_V2: an empty slot under every skill slot. The
# window's background (pet_ui_bg.tga, cut out of the mod) has fifteen skill
# icons with "20" painted into the slots: where no button of ours lay on it
# (an opened slot without a skill, a window without a pet) the owner saw a
# skill there, but there was nothing to hover.
SLOT_BASE = 'd:/ymir work/ui/public/Slot_Base.sub'
SKILL_EMPTY = 0

_data = {'pets': [], 'pending': None, 'max': 3, 'evo': {}, 'window': None, 'shown': 0, 'dialog': None,
		'names': {}, 'asked': {}, 'nameWaiting': False}


def Money(n):
	text = str(n)
	parts = []
	while len(text) > 3:
		parts.insert(0, text[-3:])
		text = text[:-3]
	parts.insert(0, text)
	return '.'.join(parts)


def ItemName(vnum):
	try:
		item.SelectItem(vnum)
		return item.GetItemName()
	except Exception:
		return str(vnum)


def BonusValue(level, maxValue):
	if level <= 0:
		return 0
	if level >= 20:
		return maxValue
	return level * maxValue / 20


# The client's embedded Python has no encodings.hex_codec (its encodings
# module is a single cythonized file), so text.decode('hex') raises
# LookupError there: the hex is decoded by hand.
def Unhex(text):
	if not text or text == '-' or len(text) % 2:
		return ''
	try:
		return ''.join([chr(int(text[i:i + 2], 16)) for i in xrange(0, len(text), 2)])
	except ValueError:
		return ''


def Duration(seconds):
	if seconds <= 0:
		return '0'
	days = seconds / 86400
	hours = (seconds % 86400) / 3600
	if days > 0:
		return '%d d %d h' % (days, hours)
	return '%d h %d min' % (hours, (seconds % 3600) / 60)


def Send(command):
	net.SendChatPacket('/newpet ' + command)


# After every loading screen (game.py): the pet back where the owner is.
def Start():
	Send('sync')


def OnCommand(what='', *args):
	if what == 'Begin':
		try:
			_data['pending'] = []
			_data['pendingOpen'] = len(args) > 1 and args[1] == '1'
			if len(args) > 2:
				_data['max'] = int(args[2])
		except ValueError:
			_data['pending'] = None
	elif what == 'Pet':
		__OnPet(args)
	elif what == 'End':
		__OnEnd()
	elif what == 'Evo':
		try:
			values = [int(a) for a in args[:13]]
			pairs = [(values[i], values[i + 1]) for i in xrange(3, len(values) - 1, 2)]
			_data['evo'][values[0]] = {'level': values[1], 'gold': values[2], 'items': pairs[:9]}
		except (ValueError, IndexError):
			pass
	elif what == 'Hatch':
		__AskName('hatch', args)
	elif what == 'Rename':
		__AskName('rename', args)
	elif what == 'Info':
		try:
			_data['names'][int(args[0])] = Unhex(args[1]) if len(args) > 1 else ''
		except (ValueError, IndexError):
			pass


# The tooltip lines of a Transporter z Petem: [(text, color)].
def TransporterLines(sockets):
	try:
		petId, packed, egg = int(sockets[0]), int(sockets[1]), int(sockets[2])
	except (TypeError, ValueError, IndexError):
		return []
	if petId <= 0:
		return [('Transporter jest pusty.', GRAY)]
	level, evolution = packed % 1000, max(0, min(3, packed / 1000))
	lines = []
	name = _data['names'].get(petId)
	if name is None:
		now = app.GetTime()
		if now - _data['asked'].get(petId, -10.0) > 5.0:
			_data['asked'][petId] = now
			Send('info %d' % petId)
	elif name:
		lines.append(('Imi\xea: %s' % name, GOLD))
	lines.append(('Gatunek: %s' % SPECIES.get(egg, ItemName(egg)), WHITE))
	lines.append(('Poziom: %d' % level, WHITE))
	lines.append(('Ewolucja: %s' % EVOLUTION_NAMES[evolution], WHITE))
	return lines


def __OnPet(args):
	if _data['pending'] is None or len(args) < 19:
		return
	try:
		v = [int(a) for a in args[:16]]
	except ValueError:
		return
	skills = []
	for part in args[16].split(','):
		try:
			t, l = part.split('.')
			skills.append((int(t), int(l)))
		except ValueError:
			skills.append((SKILL_LOCKED, 0))
	while len(skills) < 15:
		skills.append((SKILL_LOCKED, 0))
	_data['pending'].append({'id': v[0], 'egg': v[1], 'young': v[2], 'hero': v[3], 'level': v[4],
			'exp': v[5], 'need': v[6], 'evolution': v[7], 'cap': v[8], 'life': v[9], 'lifeMax': v[10],
			'active': v[11] != 0, 'out': v[12] != 0, 'bonus': (v[13], v[14], v[15]), 'skills': skills[:15],
			'name': Unhex(args[17]), 'species': Unhex(args[18]) or SPECIES.get(v[1], ItemName(v[1]))})


def __OnEnd():
	if _data['pending'] is None:
		return
	oldIds = [p['id'] for p in _data['pets']]
	_data['pets'] = _data['pending']
	_data['pending'] = None
	shownId = oldIds[_data['shown']] if 0 <= _data['shown'] < len(oldIds) else 0
	_data['shown'] = 0
	for i, pet in enumerate(_data['pets']):
		if (shownId and pet['id'] == shownId) or (not shownId and pet['active']):
			_data['shown'] = i
	if len(_data['pets']) > len(oldIds) and oldIds:
		_data['shown'] = len(_data['pets']) - 1
	# A hatching or a renaming the server took (a pet has the name now): its
	# window goes. A refusal comes as a chat line and leaves it open.
	if _data['nameWaiting']:
		for pet in _data['pets']:
			if pet['name'] == _data.get('nameSent'):
				_data['nameWaiting'] = False
				__CloseDialog()
				break
	wnd = _data['window']
	if _data.get('pendingOpen'):
		wnd = GetWindow()
		if not wnd.IsShow():
			wnd.Show()
			wnd.SetTop()
			wnd.nextRequest = app.GetTime() + REFRESH_SECONDS
	if wnd:
		wnd.Refresh()


def ValidPetName(name):
	return 2 <= len(name) <= 12 and name.isalnum()


def __AskName(kind, args):
	try:
		first = int(args[0])
		price = int(args[2] if kind == 'hatch' else args[1])
		egg = int(args[1]) if kind == 'hatch' else 0
	except (ValueError, IndexError):
		return
	if kind == 'rename':
		for pet in _data['pets']:
			if pet['id'] == first:
				egg = pet['egg']
	dialog = _data['dialog']
	if not dialog:
		dialog = NameInputWindow()
		_data['dialog'] = dialog
	dialog.Open(kind, first, egg, price)


def __CloseDialog():
	dialog = _data['dialog']
	if dialog:
		dialog.Close()
	return True


def SealImage(egg):
	# The mod's seals 55701-55711 belong to the eggs 55401-55411.
	return IMG + 'seal/%d.tga' % (egg + 300)


# dracaryS' nameinputwindow: the name of a pet to hatch, or its new name.
class NameInputWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.kind = ''
		self.first = 0
		ui.PythonScriptLoader().LoadScriptFile(self, 'UIScript/mt2009newpetname.py')
		self.titleName = self.GetChild('TitleName')
		self.itemSlot = self.GetChild('ItemSlot')
		self.inputString = self.GetChild('InputString')
		self.moneyText = self.GetChild('HatchingMoney')
		self.namingTitle = self.GetChild('PetNamingTitle')
		self.GetChild('Titlebar').SetCloseEvent(ui.__mem_func__(self.Close))
		self.GetChild('acceptbtn').SetEvent(ui.__mem_func__(self.AcceptInput))
		self.inputString.OnIMEReturn = ui.__mem_func__(self.AcceptInput)
		self.inputString.OnPressEscapeKey = ui.__mem_func__(self.Close)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Open(self, kind, first, egg, price):
		self.kind = kind
		self.first = first
		if kind == 'hatch':
			self.titleName.SetText(ItemName(egg))
			self.namingTitle.SetText('Wyklucie peta - imi\xea')
		else:
			self.titleName.SetText('Zmiana imienia peta')
			self.namingTitle.SetText('Nowe imi\xea peta')
		self.moneyText.SetText('Koszt: %s yang' % Money(price))
		try:
			self.itemSlot.SetItemSlot(0, egg, 0)
		except Exception:
			self.itemSlot.ClearSlot(0)
		self.itemSlot.RefreshSlot()
		self.inputString.SetText('')
		_data['nameWaiting'] = False
		self.Show()
		self.SetTop()
		self.inputString.SetFocus()

	def AcceptInput(self):
		name = self.inputString.GetText().strip()
		if not ValidPetName(name):
			self.namingTitle.SetText('2-12 liter lub cyfr!')
			return
		if self.kind == 'hatch':
			Send('hatch %d %s' % (self.first, name))
		else:
			Send('rename %s' % name)
		# Closed when the server answers with the pets (__OnEnd); a refusal
		# comes as a chat line and leaves it open for another name.
		_data['nameWaiting'] = True
		_data['nameSent'] = name

	def Close(self):
		_data['nameWaiting'] = False
		self.inputString.KillFocus()
		self.Hide()
		return True

	def OnPressEscapeKey(self):
		self.Close()
		return True


class TextToolTip(ui.Window):
	def __init__(self):
		ui.Window.__init__(self, 'TOP_MOST')
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetHorizontalAlignCenter()
		textLine.SetOutline()
		textLine.Show()
		self.textLine = textLine

	def __del__(self):
		ui.Window.__del__(self)

	def SetText(self, text):
		self.textLine.SetText(text)

	def OnRender(self):
		(mouseX, mouseY) = wndMgr.GetMousePosition()
		self.textLine.SetPosition(mouseX, mouseY - 15)


# The skill slots of the mod's window (its PetSkillSlot) and their positions.
SKILL_SLOTS = ((153, 362), (217, 362), (281, 362),
		(27, 402), (79, 402), (131, 402), (183, 402), (236, 403), (287, 402),
		(27, 446), (79, 446), (131, 446), (183, 446), (235, 446), (287, 446))


class NewPetWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.question = None
		self.nextRequest = 0.0
		self.feedWindow = None
		self.skillButtons = []
		self.skillLevels = []
		self.skillBases = []
		self.skillShown = [None] * 15
		self.skillData = [None] * 15
		# MT2009_PLUS_PET_SKILL_TOOLTIP_V2: one tooltip for the skills, written
		# when the mouse comes over a slot (as the Seon-Hae window and the
		# other working windows do) and written again when the pet changes
		# under it - never cleared while it is shown without being written and
		# shown again.
		self.skillTip = uiToolTip.ToolTip()
		self.skillTip.HideToolTip()
		self.skillHovered = -1
		self.skillTipKey = None
		self.skillOver = -1
		self.errorsLogged = 0
		self.expTip = TextToolTip()
		self.expTip.Hide()
		self.iconTip = uiToolTip.ToolTip()
		self.iconTip.HideToolTip()
		self.itemTip = uiToolTip.ItemToolTip()
		self.itemTip.HideToolTip()
		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		ui.PythonScriptLoader().LoadScriptFile(self, 'UIScript/mt2009newpet.py')
		self.GetChild('CloseButton').SetEvent(ui.__mem_func__(self.Close))
		self.GetChild('FeedEvolButton').SetEvent(ui.__mem_func__(self.ClickEvolveGUI))
		self.GetChild('SummonButton').SetEvent(ui.__mem_func__(self.__Summon))
		self.GetChild('ReleaseButton').SetEvent(ui.__mem_func__(self.__AskRelease))
		gauge = self.GetChild('UpBringing_Pet_EXP_Gauge_Board')
		gauge.SAFE_SetStringEvent('MOUSE_OVER_IN', self.__OverInExp)
		gauge.SAFE_SetStringEvent('MOUSE_OVER_OUT', self.__OverOutExp)
		self.gauges = [self.GetChild('UpBringing_Pet_EXPGauge_0%d' % i) for i in range(1, 5)]

		self.petIcon = ui.Button()
		self.petIcon.SetParent(self)
		self.petIcon.SetPosition(22, 52)
		self.petIcon.SetEvent(ui.__mem_func__(self.__NextPet))
		# The tooltips are shown on the mouse-over events, not through
		# SetToolTipWindow: that one parents the tooltip to the button (it
		# lost its TOP_MOST layer and was drawn under the window's boards).
		self.petIcon.SAFE_SetStringEvent('MOUSE_OVER_IN', self.__OverInIcon)
		self.petIcon.SAFE_SetStringEvent('MOUSE_OVER_OUT', self.__OverOutIcon)

		for i, (x, y) in enumerate(SKILL_SLOTS):
			# MT2009_PLUS_PET_SKILL_TOOLTIP_V2: the empty slot covers the
			# skill painted into the background.
			base = ui.ImageBox()
			base.SetParent(self)
			base.AddFlag('not_pick')
			base.SetPosition(x, y)
			base.LoadImage(SLOT_BASE)
			base.Show()
			button = ui.Button()
			button.SetParent(self)
			button.SetPosition(x, y)
			button.SetEvent(ui.__mem_func__(self.__ClickSkill), i)
			# The tooltip comes from the mouse's position in OnUpdate
			# (MT2009_PLUS_PET_SKILL_TOOLTIP_V1) and from the button's
			# mouse-over too, whichever of them sees the mouse first.
			button.SAFE_SetStringEvent('MOUSE_OVER_IN', self.__OverInSkill, i)
			button.SAFE_SetStringEvent('MOUSE_OVER_OUT', self.__OverOutSkill, i)
			level = ui.TextLine()
			level.SetParent(self)
			level.SetPosition(x + 31, y + 20)
			level.SetHorizontalAlignRight()
			level.SetOutline()
			level.AddFlag('not_pick')
			self.skillBases.append(base)
			self.skillButtons.append(button)
			self.skillLevels.append(level)
		self.SetCenterPosition()

	# ---- data ----

	def __Pet(self):
		pets = _data['pets']
		if not pets:
			return None
		_data['shown'] = max(0, min(_data['shown'], len(pets) - 1))
		return pets[_data['shown']]

	def ClearData(self):
		for name in ('PetName', 'PetMobName', 'LevelValue', 'AgeValue', 'LifeTextValue'):
			self.GetChild(name).SetText('')
		self.GetChild('LifeGauge').Hide()
		for j in xrange(3):
			self.GetChild('bonus_value_%d' % j).SetText('')
			self.GetChild('bonus_title_%d' % j).SetText('')
		for g in self.gauges:
			g.Hide()
		self.petIcon.Hide()
		self.iconTip.HideToolTip()
		for i in xrange(15):
			self.__SetSkill(i, None, 0)
		self.__HideSkillTip()
		self.expTip.SetText('')

	def Refresh(self):
		pet = self.__Pet()
		if not pet:
			self.ClearData()
			self.GetChild('PetName').SetText('Nie masz jeszcze peta')
			self.GetChild('PetMobName').SetText('Kliknij jajko prawym przyciskiem')
			self.GetChild('SummonButton').Hide()
			self.GetChild('ReleaseButton').Hide()
			self.GetChild('FeedEvolButton').Hide()
			if self.feedWindow:
				self.feedWindow.Hide()
			return
		self.GetChild('SummonButton').Show()
		self.GetChild('ReleaseButton').Show()
		self.GetChild('FeedEvolButton').Show()
		pets = _data['pets']

		image = SealImage(pet['egg'])
		if getattr(self, 'iconImage', None) != image:
			self.iconImage = image
			try:
				self.petIcon.SetUpVisual(image)
				self.petIcon.SetOverVisual(image)
				self.petIcon.SetDownVisual(image)
			except Exception:
				pass
		self.petIcon.Show()
		self.iconTip.ClearToolTip()
		self.iconTip.AppendTextLine(pet['name'] or pet['species'], GOLD)
		if len(pets) > 1:
			self.iconTip.AppendTextLine('Kliknij: nast\xeapny pet (%d/%d)' % (_data['shown'] + 1, len(pets)), GRAY)

		self.GetChild('PetName').SetText(pet['name'] or pet['species'])
		state = 'przywo\xb3any' if pet['out'] else ('wybrany' if pet['active'] else 'w stajni')
		mobName = '%s - %s' % (pet['species'], state)
		if len(pets) > 1:
			mobName += '  (%d/%d)' % (_data['shown'] + 1, len(pets))
		self.GetChild('PetMobName').SetText(mobName)
		self.GetChild('LevelValue').SetText('%d / %d' % (pet['level'], pet['cap']))
		self.GetChild('AgeValue').SetText(EVOLUTION_NAMES[max(0, min(3, pet['evolution']))])
		self.__UpdateExp(pet)
		self.__UpdateTime(pet)
		self.__UpdateBonus(pet)
		for i, (skill, level) in enumerate(pet['skills']):
			self.__SetSkill(i, skill, level)
		# The tooltip under the mouse shows the new data at once.
		if self.skillHovered >= 0:
			self.skillTipKey = None
			self.__ShowSkillTip(self.skillHovered)

		summon = self.GetChild('SummonButton')
		if not pet['active']:
			summon.SetText('Wybierz')
		elif pet['out']:
			summon.SetText('Odwo\xb3aj')
		else:
			summon.SetText('Przywo\xb3aj')
		self.CheckFeedWindow()

	def __UpdateExp(self, pet):
		curPoint = max(0, pet['exp'])
		maxPoint = max(1, pet['need'])
		curPoint = min(curPoint, maxPoint)
		quarterPoint = maxPoint / 4
		fullCount = 0
		if quarterPoint:
			fullCount = min(4, curPoint / quarterPoint)
		for g in self.gauges:
			g.Hide()
		for i in xrange(fullCount):
			self.gauges[i].SetRenderingRect(0.0, 0.0, 0.0, 0.0)
			self.gauges[i].Show()
		if quarterPoint and fullCount < 4:
			percentage = float(curPoint % quarterPoint) / quarterPoint - 1.0
			self.gauges[fullCount].SetRenderingRect(0.0, percentage, 0.0, 0.0)
			self.gauges[fullCount].Show()
		self.expTip.SetText('Do\x9cwiadczenie: %s / %s (%.2f%%)' % (Money(curPoint), Money(maxPoint),
				float(curPoint) / maxPoint * 100))

	def __UpdateTime(self, pet):
		life = max(0, pet['life'])
		gauge = self.GetChild('LifeGauge')
		if life > 0:
			self.GetChild('LifeTextValue').SetText(Duration(life))
			gauge.SetPercentage(life, max(1, pet['lifeMax']))
			gauge.Show()
		else:
			self.GetChild('LifeTextValue').SetText('0 - nakarm peta')
			gauge.Hide()

	def __UpdateBonus(self, pet):
		for j, (name, maxValue, unit) in enumerate(BONUSES):
			level = pet['bonus'][j]
			self.GetChild('bonus_title_%d' % j).SetText(name)
			self.GetChild('bonus_value_%d' % j).SetText('Lv %d  -  +%d%s' % (level, BonusValue(level, maxValue), unit))

	def __SetSkill(self, i, skill, level):
		button = self.skillButtons[i]
		label = self.skillLevels[i]
		if skill is None:
			button.Hide()
			label.Hide()
			self.skillShown[i] = None
			self.skillData[i] = None
			return
		# An opened slot without a skill (or a skill this client does not
		# know) is an empty slot - with its own tooltip.
		if skill != SKILL_LOCKED and not SKILLS.get(skill):
			skill, level = SKILL_EMPTY, 0
		self.skillData[i] = (skill, level)
		if skill == SKILL_LOCKED:
			image = IMG + 'skill_locked.tga'
		elif skill == SKILL_EMPTY:
			image = SLOT_BASE
		else:
			image = IMG + 'skill/%d.tga' % skill
		if self.skillShown[i] != image:
			self.skillShown[i] = image
			button.SetUpVisual(image)
			button.SetOverVisual(image)
			button.SetDownVisual(image)
		button.Show()
		if skill == SKILL_LOCKED or skill == SKILL_EMPTY:
			label.Hide()
			return
		label.SetText(str(level))
		label.Show()

	# The tooltip's lines for a slot: [(text, color)].
	def __SkillLines(self, i):
		data = self.skillData[i] if 0 <= i < len(self.skillData) else None
		if not data:
			return []
		skill, level = data
		if skill == SKILL_LOCKED:
			return [('Zablokowane miejsce', GRAY), ('Otwiera je ewolucja peta', GRAY)]
		if skill == SKILL_EMPTY:
			return [('Wolne miejsce na umiej\xeatno\x9c\xe6', WHITE),
					('U\xbfyj Ksi\xeagi Umiej\xeatno\x9cci Peta z ekwipunku', GRAY)]
		name, maxValue, unit = SKILLS[skill]
		lines = [(ItemName(BOOK_BASE + skill), GOLD),
				('Poziom %d/20' % level, WHITE),
				('%s +%d%s' % (name, BonusValue(level, maxValue), unit), GREEN)]
		if level < 20:
			lines.append(('Na 20 poziomie: +%d%s' % (maxValue, unit), GRAY))
		lines.append(('Kliknij, aby zapomnie\xe6 (Pet Revertus)', GRAY))
		return lines

	# Writes the skill tooltip for slot i and shows it - written again only
	# when the slot or its skill changed, shown again when something hid it.
	def __ShowSkillTip(self, i):
		tip = self.skillTip
		if not tip:
			return
		data = self.skillData[i] if 0 <= i < len(self.skillData) else None
		if not data:
			self.__HideSkillTip()
			return
		key = (i, data)
		if key == self.skillTipKey:
			if not tip.IsShow():
				tip.ShowToolTip()
			return
		try:
			lines = self.__SkillLines(i)
		except Exception:
			self.__LogError('skill lines')
			lines = [('Umiej\xeatno\x9c\xe6 peta', GOLD)]
		try:
			tip.ClearToolTip()
			for text, color in lines:
				tip.AppendTextLine(text, color)
			tip.ResizeToolTip()
		except Exception:
			# Never an empty frame: a tooltip that could not be written is
			# not shown.
			self.__LogError('skill tooltip')
			tip.HideToolTip()
			self.skillHovered = -1
			self.skillTipKey = None
			return
		self.skillHovered = i
		self.skillTipKey = key
		tip.ShowToolTip()

	def __HideSkillTip(self):
		if self.skillTip:
			self.skillTip.HideToolTip()
		self.skillHovered = -1
		self.skillTipKey = None

	def __LogError(self, where):
		if self.errorsLogged >= 5:
			return
		self.errorsLogged += 1
		try:
			import sys
			dbg.TraceError('uinewpet %s: %s %s' % (where, sys.exc_info()[0], sys.exc_info()[1]))
		except Exception:
			pass

	# ---- the evolution window (the mod's feed window) ----

	def CreateFeedWindow(self):
		board = ui.BoardWithTitleBar()
		board.AddFlag('float')
		board.SetSize(130, 196)
		board.SetTitleName('Ewolucja')
		board.SetCloseEvent(ui.__mem_func__(self.__HideFeed))
		grid = ui.GridSlotWindow()
		grid.SetParent(board)
		grid.SetPosition(17, 30)
		grid.ArrangeSlot(0, 3, 3, 32, 32, 0, 0)
		grid.SetSlotBaseImage('d:/ymir work/ui/public/Slot_Base.sub', 1.0, 1.0, 1.0, 1.0)
		grid.SetOverInItemEvent(ui.__mem_func__(self.__OverInFeed))
		grid.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutFeed))
		grid.Show()
		self.feedLevel = ui.TextLine()
		self.feedLevel.SetParent(board)
		self.feedLevel.SetPosition(65, 130)
		self.feedLevel.SetHorizontalAlignCenter()
		self.feedLevel.Show()
		self.feedGold = ui.TextLine()
		self.feedGold.SetParent(board)
		self.feedGold.SetPosition(65, 145)
		self.feedGold.SetHorizontalAlignCenter()
		self.feedGold.Show()
		# The confirm button with its text (the round accept sprite had no
		# label and was not seen as the evolution's button - item 32).
		button = ui.Button()
		button.SetParent(board)
		button.SetPosition(34, 164)
		button.SetUpVisual('d:/ymir work/ui/public/middle_button_01.sub')
		button.SetOverVisual('d:/ymir work/ui/public/middle_button_02.sub')
		button.SetDownVisual('d:/ymir work/ui/public/middle_button_03.sub')
		button.SetText('Ewoluuj')
		button.SetEvent(ui.__mem_func__(self.ClickEvolve))
		button.Show()
		self.feedWindow = board
		self.feedGrid = grid
		self.feedButton = button
		self.feedItems = []

	def __PlaceFeed(self):
		if self.feedWindow and self.feedWindow.IsShow():
			(x, y) = self.GetGlobalPosition()
			self.feedWindow.SetPosition(x + self.GetWidth() - 5, y + 150)

	def CheckFeedWindow(self):
		if not self.feedWindow:
			return
		pet = self.__Pet()
		for j in xrange(9):
			self.feedGrid.ClearSlot(j)
		self.feedItems = []
		if not pet or pet['evolution'] >= 3:
			self.feedLevel.SetText('Najwy\xbfsza ewolucja')
			self.feedGold.SetText('')
			self.feedButton.Hide()
			self.feedGrid.RefreshSlot()
			return
		self.feedButton.Show()
		evo = _data['evo'].get(pet['evolution'])
		if evo:
			for vnum, count in evo['items']:
				if vnum and count:
					self.feedGrid.SetItemSlot(len(self.feedItems), vnum, count)
					self.feedItems.append(vnum)
			self.feedLevel.SetText('Poziom peta: %d' % evo['level'])
			self.feedLevel.SetPackedFontColor(GREEN if pet['level'] >= evo['level'] else RED)
			self.feedGold.SetText('%s yang' % Money(evo['gold']))
		self.feedGrid.RefreshSlot()

	def ClickEvolveGUI(self):
		if not self.feedWindow:
			self.CreateFeedWindow()
		if self.feedWindow.IsShow():
			self.feedWindow.Hide()
			return
		self.feedWindow.Show()
		self.feedWindow.SetTop()
		self.CheckFeedWindow()
		self.__PlaceFeed()

	def __HideFeed(self):
		if self.feedWindow:
			self.feedWindow.Hide()
		self.itemTip.HideToolTip()

	def ClickEvolve(self):
		pet = self.__Pet()
		if not pet:
			return
		# One command with the shown pet's id: the server chooses it first
		# (a separate "select" right before was dropped by its 300 ms limit).
		Send('evolve %d' % pet['id'])

	def __OverInFeed(self, slotIndex):
		if 0 <= slotIndex < len(self.feedItems):
			self.itemTip.SetItemToolTip(self.feedItems[slotIndex])

	def __OverOutFeed(self):
		self.itemTip.HideToolTip()

	# ---- events ----

	def __OverInIcon(self):
		if self.iconTip and self.petIcon.IsShow():
			self.iconTip.ShowToolTip()

	def __OverOutIcon(self):
		if self.iconTip:
			self.iconTip.HideToolTip()

	def __OverInSkill(self, i):
		self.skillOver = i
		if 0 <= i < len(self.skillData) and self.skillData[i]:
			self.__ShowSkillTip(i)

	def __OverOutSkill(self, i):
		if self.skillOver == i:
			self.skillOver = -1
		if self.skillHovered == i:
			self.__HideSkillTip()

	def __OverInExp(self):
		self.expTip.Show()

	def __OverOutExp(self):
		self.expTip.Hide()

	def __NextPet(self):
		if len(_data['pets']) > 1:
			_data['shown'] = (_data['shown'] + 1) % len(_data['pets'])
			self.Refresh()

	def __Summon(self):
		pet = self.__Pet()
		if not pet:
			return
		if not pet['active']:
			Send('select %d' % pet['id'])
		else:
			Send('toggle')

	def __AskRelease(self):
		pet = self.__Pet()
		if not pet:
			return
		self.__CloseQuestion()
		question = uiCommon.QuestionDialog()
		question.SetText('Wypu\x9c\xe6 %s na wolno\x9c\xe6? Tego nie da si\xea cofn\xb9\xe6.' % (pet['name'] or pet['species']))
		question.SetAcceptEvent(lambda i=pet['id']: self.__Release(i))
		question.SetCancelEvent(ui.__mem_func__(self.__CloseQuestion))
		question.Open()
		self.question = question

	def __Release(self, petId):
		self.__CloseQuestion()
		Send('release %d' % petId)

	def __ClickSkill(self, slotIndex):
		pet = self.__Pet()
		if not pet or slotIndex >= len(pet['skills']):
			return
		skill, level = pet['skills'][slotIndex]
		if skill <= 0 or skill == SKILL_LOCKED:
			return
		if not pet['active']:
			Send('select %d' % pet['id'])
		self.__CloseQuestion()
		question = uiCommon.QuestionDialog()
		question.SetText('Zapomnie\xe6 %s? (zu\xbfywa Pet Revertus)' % ItemName(BOOK_BASE + skill))
		question.SetAcceptEvent(lambda s=slotIndex: self.__Forget(s))
		question.SetCancelEvent(ui.__mem_func__(self.__CloseQuestion))
		question.Open()
		self.question = question

	def __Forget(self, slotIndex):
		self.__CloseQuestion()
		Send('skilldel %d' % slotIndex)

	def __CloseQuestion(self):
		if self.question:
			self.question.Close()
		self.question = None

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()
		Send('open')
		self.nextRequest = app.GetTime() + REFRESH_SECONDS

	# The slot under the mouse, from the window's position (the slots are
	# 32x32 at SKILL_SLOTS), whether a button of ours is there or not.
	def __HoveredSkill(self):
		if not self.IsShow():
			return -1
		(mx, my) = wndMgr.GetMousePosition()
		(wx, wy) = self.GetGlobalPosition()
		for i, (x, y) in enumerate(SKILL_SLOTS):
			if self.skillData[i] and wx + x <= mx < wx + x + 32 and wy + y <= my < wy + y + 32:
				return i
		return -1

	def __UpdateSkillTooltip(self):
		hovered = self.__HoveredSkill()
		if hovered < 0 and 0 <= self.skillOver < len(self.skillButtons) and self.skillData[self.skillOver]:
			# The button says the mouse is on it (its mouse-over event).
			if self.skillButtons[self.skillOver].IsIn():
				hovered = self.skillOver
			else:
				self.skillOver = -1
		if hovered < 0:
			if self.skillHovered >= 0:
				self.__HideSkillTip()
			return
		self.__ShowSkillTip(hovered)

	def OnUpdate(self):
		# MT2009_PLUS_PET_SKILL_TOOLTIP_V2: the tooltip first, and nothing
		# after it can stop it (an exception here is one syserr line).
		try:
			self.__UpdateSkillTooltip()
		except Exception:
			self.__LogError('skill hover')
		try:
			self.__PlaceFeed()
		except Exception:
			self.__LogError('feed window')
		if app.GetTime() >= self.nextRequest:
			self.nextRequest = app.GetTime() + REFRESH_SECONDS
			Send('refresh')

	def Close(self):
		self.expTip.Hide()
		self.iconTip.HideToolTip()
		self.itemTip.HideToolTip()
		self.__HideSkillTip()
		self.skillOver = -1
		self.__CloseQuestion()
		self.__HideFeed()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Teardown(self):
		self.Close()
		self.feedWindow = None
		self.skillButtons = []
		self.skillBases = []
		self.skillLevels = []
		self.skillData = [None] * 15
		self.skillTip = None
		self.expTip = None
		self.iconTip = None
		self.itemTip = None
		self.ClearDictionary()


def GetWindow():
	if not _data['window']:
		_data['window'] = NewPetWindow()
	return _data['window']


def ToggleWindow():
	wnd = GetWindow()
	if wnd.IsShow():
		wnd.Close()
	else:
		wnd.Open()


def DestroyWindow():
	dialog = _data['dialog']
	_data['dialog'] = None
	if dialog:
		dialog.Close()
		dialog.ClearDictionary()
	wnd = _data['window']
	if wnd:
		wnd.Teardown()
	_data['window'] = None
	_data['pets'] = []
	_data['pending'] = None
	_data['nameWaiting'] = False
