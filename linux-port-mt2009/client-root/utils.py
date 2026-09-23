import constInfo

class Sandbox(object):
	# DO NOT REMOVE ANY ENTRY OF WHITE LIST
	# THIS ENTIES ARE USED FOR Sandbox.
	# 	(If sys.modules['__main__'] is changed, you can see the hell.)
	WHITE_LIST = ['__builtin__', 'types', __name__, '__main__', 'sys']
	def __init__(self, prevent_imported_modules = False, allowed_modules = [], prevented_modules = [], allowed_paths = []):
		self.prevent_imported_modules = prevent_imported_modules
		self.allowed_modules = allowed_modules
		self.prevented_modules = prevented_modules
		self.allowed_paths = allowed_paths

	def add_allowed_modules(self, allowed_modules):
		self.allowed_modules = self.allowed_modules + allowed_modules

	def add_prevented_modules(self, prevented_modules):
		self.prevented_modules = self.prevented_modules + prevented_modules

	def execfile(self, filename, dic):
		import sys
		import copy

		for allowed_module_name in self.allowed_modules:
			try:
				exec 'import {0}'.format(allowed_module_name)
			except:
				# Just now, pass the exception.
				# (filename could not use this module)
				sys.modules['dbg'].TraceError("UISCRIPT_LOAD_ERROR: Could not import %s [filename %s]" % (allowed_module_name, filename)) # @fixme004 (sys.stderr.write to dbg.TraceError)
				pass
		# shallow copy of sys.modules
		old_modules = copy.copy(sys.modules)

		old_path = sys.path
		sys.path = self.allowed_paths

		# set current imported modules None
		if self.prevent_imported_modules:
			import types
			for k, v in old_modules.items():
				if type(v) == types.ModuleType:
					if not (k in self.WHITE_LIST or k in self.allowed_modules):
						sys.modules[k] = None

		# set prevented modules None.
		for prevented_module_name in self.prevented_modules:
			sys.modules[prevented_module_name] = None

		try:
			f = open(filename, 'rb')
			data = f.read()
			code = compile(data, filename, 'exec')
			exec code in dic
		except Exception as e:
			old_modules['dbg'].TraceError("UISCRIPT_LOAD_ERROR: %s [filename %s]" % (str(e), filename)) # @fixme004 (sys.stderr.write to dbg.TraceError)
		finally:
			# Restore original settings.
			#sys.modules = old_modules# <- This is not effective. I don't know why, but I guess some where got old sys.modules reference and access old one.
			for k, v in sys.modules.items():
				if not k in old_modules:
					del sys.modules[k]
			for k, v in old_modules.items():
				sys.modules[k] = v

			sys.path = old_path

#######################################################
#	@usage:
#		GetElementDictByName(window,"BoardExample")
#
#	@desc:
#		it could search in a dict (script file for ScriptWindow) an element (whether it is child of window or another child in the tree)
#
#	@notes:
#		adviced to check the return before to access (it could be None)
def GetElementDictByName(dct, name):
	if 'children' in dct:
		for child in dct['children']:
			if 'name' in child:
				if child['name'] == name:
					return child

			if 'children' in child:
				search = GetElementDictByName(child, name)
				if search != None:
					return search
	return None

### Find the element <name> inside <parent> and return the pos
def FindElementPos(parent, name):
	for i in range(len(parent)):
		if parent[i]["name"] == name:
			return i
	return -1

### Find the element <name> inside <parent> and return the ref
def FindElementRef(parent, name):
	for i in range(len(parent)):
		if parent[i]["name"] == name:
			return parent[i]
	return None

### Find the element <name> inside <parent> and add it before that (only for lists)
def ElementAddBefore(parent, name, obj):
	i = FindElementPos(parent, name)
	if i != -1:
		parent.insert(i, obj)

### Find the element <name> inside <parent> and add it after that (only for lists)
def ElementAddAfter(parent, name, obj):
	i = FindElementPos(parent, name)
	if i != -1:
		parent.insert(i+1, obj)

def FindElement(name, data):
	if 'children' in data:
		for child in data['children']:
			if child.get('name', '') == name:
				return child
		for child in data['children']:
			res = FindElement(name, child)
			if res:
				return res
	return None

def ReplaceElement(name, nvalue, data):
	value = FindElement(name, data)
	for n, v in nvalue.items():
		value[n] = v
	for n in value:
		if n not in nvalue:
			del value[n]

def AppendChildren(name, children, data):
	value = FindElement(name, data)
	value['children'] = value.get('children', tuple()) + children

import player
def CountItemCountInInventory(item_vnum):
	total_count = 0

	# Every bag page, and the horse page while the horse is out.
	slotCount = player.INVENTORY_DEFAULT_MAX_NUM
	if constInfo.IS_HORSE_SUMMONED:
		slotCount = player.INVENTORY_MAX_NUM

	for i in xrange(slotCount):
		if player.GetItemIndex(i) == item_vnum:
			total_count += player.GetItemCount(i)

	return total_count

import difflib
def check_string_similarity(input, check_word, threshold=0.6):
	similarity = difflib.SequenceMatcher(None, input, check_word).ratio()
	if len(input) <= len(check_word) and check_word.startswith(input) and similarity >= threshold:
		return True
	return False

import os
def open_url(url):
	os.popen("start %s" % (url))

import item
import nonplayer
def process_string(input_string, token):
	start_index = input_string.find('{%s' % token)
	end_index = input_string.find('}')

	value = None
	transformed_value = None

	if start_index != -1 and end_index != -1:
		value_start = start_index + 2
		value = int(input_string[value_start:end_index])

		if token == "i":
			item.SelectItem(value)
			transformed_value = item.GetItemName()
		elif token == "m":
			transformed_value = nonplayer.GetMonsterName(value)

		input_string = input_string[:start_index] + transformed_value + input_string[end_index + 1:]

	transformed_string = input_string.strip()

	return transformed_string, value

import ui
class ToggleButton(ui.Window):
	def __init__(self):
		super(ToggleButton, self).__init__()
		
		self.__Initialize()
		self.__BuildPage()
	
	def __del__(self):
		self.__Destroy()
		super(ToggleButton, self).__del__()
	
		#Private Methods
	def __Initialize(self):
		self.loadedWindow = False
		
		self.base_board = None
		self.button_scale = 1.0

		self.invStagesWindow = None
		self.txtLine = None
		
		self.clickEvent = None
		self.buttonState = 0
		self.button_images = []
	
	def __Destroy(self):
		if self.base_board:
			self.base_board.Hide()
			self.base_board.Destroy()
		
		del self.base_board
		self.base_board = None

		if self.invStagesWindow:
			self.invStagesWindow.Hide()
			self.invStagesWindow.Destroy()
		
		del self.invStagesWindow
		self.invStagesWindow = None

		if self.txtLine:
			self.txtLine.Hide()
			self.txtLine.Destroy()
		
		del self.txtLine
		self.txtLine = None

	def __BuildPage(self):
		if self.loadedWindow is True:
			return
		
		self.base_board = ui.ExpandedImageBox()
		self.base_board.SetParent(self)
		self.base_board.SetPosition(0, 0)
		self.base_board.Show()
		
		self.invStagesWindow = ui.Window()
		self.invStagesWindow.SetParent(self)
		self.invStagesWindow.SetPosition(0, 0)
		self.invStagesWindow.SAFE_SetStringEvent("MOUSE_OVER_IN", self.OnHover)
		self.invStagesWindow.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.OnUp)
		self.invStagesWindow.SAFE_SetStringEvent("MOUSE_LEFT_BUTTON", self.OnDown)
		self.invStagesWindow.Show()
		
		self.txtLine = ui.TextLine()
		self.txtLine.SetParent(self.base_board)
		self.txtLine.Hide()
		
		self.loadedWindow = True
	
	def __ReloadButtonSize(self):
		self.SetSize(self.base_board.GetWidth(), self.base_board.GetHeight())
		self.invStagesWindow.SetSize(self.base_board.GetWidth(), self.base_board.GetHeight())
	
		#Public Methods
	def SetText(self, txt, centerAlign = True):
		if not self.txtLine:
			return
		
		self.txtLine.SetText(txt)
		if centerAlign:
			self.txtLine.SetPosition(((self.GetWidth() / 2) - (self.txtLine.GetTextSize()[0] / 2)),\
					((self.GetHeight() / 2) - (self.txtLine.GetTextSize()[1] / 2)))
		self.txtLine.Show()
	
	def SetTextPosition(self, x, y):
		if not self.txtLine:
			return
		
		self.txtLine.SetPosition(x, y)
	
	def SetButtonVisual(self, images, scale = 1.0):
		if not isinstance(images, list):
			return
		
		if len(images) != 3:
			return
		
		self.button_images = images
		self.button_scale = max(scale, 0.0)

		self.base_board.LoadImage(self.button_images[self.buttonState])
		self.base_board.SetScale(self.button_scale, self.button_scale)

		self.__ReloadButtonSize()
	
	def SAFE_SetClickEvent(self, event, *args):
		self.clickEvent = (ui.__mem_func__(event), args)
	
	def OnUp(self, forceEvent = False):
		if forceEvent is False:
			if not self.base_board or\
				self.buttonState == 2:
				return
		
		self.buttonState = 0

		self.base_board.LoadImage(self.button_images[self.buttonState])
		self.base_board.SetScale(self.button_scale, self.button_scale)
	
	def OnHover(self):
		if not self.base_board or\
			self.buttonState == 2:
			return
		
		self.buttonState = 1

		self.base_board.LoadImage(self.button_images[self.buttonState])
		self.base_board.SetScale(self.button_scale, self.button_scale)
	
	def OnDown(self, runEvent = True):
		if not self.base_board:
			return
		
		if self.buttonState == 2:
			self.buttonState = 0
		else:
			self.buttonState = 2
		
		self.base_board.LoadImage(self.button_images[self.buttonState])
		self.base_board.SetScale(self.button_scale, self.button_scale)
			
		if runEvent is True:
			if self.clickEvent:
				apply(*self.clickEvent)
	
	def Show(self):
		super(ToggleButton, self).Show()
	
	def Hide(self):
		super(ToggleButton, self).Hide()

class MoreiraButton(ui.Window):
	DEFAULT_BUTTON_STATE,\
	HOVER_BUTTON_STATE,\
	DOWN_BUTTON_STATE,\
	DISABLE_BUTTON_STATE\
	= range(4)
	
	def __init__(self, parent, isRadioButton = False):
		super(MoreiraButton, self).__init__()
		
		if not parent:
			dbg.TraceError('[MoreiraButton] Received a none parent in constructor.')
			return
		
		self.__Initialize(parent, isRadioButton)
		self.__BuildBaseWindow()
		self.__BuildButton()
	
	def __del__(self):
		super(MoreiraButton, self).__del__()
	
		#@ Private Methods
	def __Initialize(self, parent, isRadioButton):
		from _weakref import proxy
		
		self.parent = proxy(parent)
		self.isRadioButton = isRadioButton
		
		self.objButtonBaseNorm = None
		self.objButtonBaseDown = None
		self.objButtonIdentifier = None
		self.objButtonLightEffect = None
		
		self.lastButtonState = self.DEFAULT_BUTTON_STATE
		self.wndAnimationController = None
	
	def __BuildBaseWindow(self):
		self.SetWindowName('MoreiraButton')
		self.SetParent(self.parent)
		self.SetPosition(0, 0)
		self.Hide()
	
	def __BuildButton(self):
		self.objButtonBaseNorm = ui.ExpandedImageBox()
		self.objButtonBaseNorm.SetParent(self)
		self.objButtonBaseNorm.SetPosition(0, 0)
		self.objButtonBaseNorm.Show()
		
		self.objButtonBaseDown = ui.ExpandedImageBox()
		self.objButtonBaseDown.SetParent(self)
		self.objButtonBaseDown.SetPosition(0, 0)
		self.objButtonBaseDown.Show()
		
		self.objButtonIdentifier = ui.ExpandedImageBox()
		self.objButtonIdentifier.SetParent(self)
		self.objButtonIdentifier.SetPosition(0, 0)
		self.objButtonIdentifier.Show()
		
		self.objButtonLightEffect = ui.ExpandedImageBox()
		self.objButtonLightEffect.SetParent(self)
		self.objButtonLightEffect.SetPosition(0, 0)
		self.objButtonLightEffect.Show()
		
		self.wndAnimationController = ui.Window()
		self.wndAnimationController.SetParent(self)
		self.wndAnimationController.SetPosition(0, 0)
		if self.isRadioButton:
			# self.wndAnimationController.SetOverInEvent(ui.__mem_func__(self.__OnRadioHover))
			# self.wndAnimationController.SetOverOutEvent(ui.__mem_func__(self.__OnRadioUp))
			self.wndAnimationController.SetMouseLeftButtonDownEvent(ui.__mem_func__(self.__OnRadioDown))
		else:
			self.wndAnimationController.SAFE_SetStringEvent("MOUSE_OVER_IN", self.__OnHover)
			self.wndAnimationController.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__OnUp)
			self.wndAnimationController.SAFE_SetStringEvent("MOUSE_LEFT_BUTTON", self.__OnDown)
			self.wndAnimationController.SAFE_SetStringEvent("MOUSE_LEFT_BUTTON_UP", self.__OnUpAfterDown)
		self.wndAnimationController.Show()
		
		self.__ReloadButtonSize([0, 0])
	
	def __ReloadButtonSize(self, size):
		self.SetSize(*size)
		if self.wndAnimationController:
			self.wndAnimationController.SetSize(*size)
	
	def __SwitchButtonBaseState(self, newState):
		if (not self.objButtonBaseNorm) or (not self.objButtonBaseDown) or\
			(not self.objButtonIdentifier) or (not self.objButtonLightEffect):
			return
		
		if newState == self.DEFAULT_BUTTON_STATE:
			self.objButtonBaseNorm.Show()
			self.objButtonBaseDown.Hide()
			self.objButtonIdentifier.Show()
			self.objButtonLightEffect.Hide()
		elif newState == self.HOVER_BUTTON_STATE:
			self.objButtonBaseNorm.Show()
			self.objButtonBaseDown.Hide()
			self.objButtonIdentifier.Show()
			self.objButtonLightEffect.Show()
		elif (newState == self.DOWN_BUTTON_STATE or newState == self.DISABLE_BUTTON_STATE):
			self.objButtonBaseNorm.Hide()
			self.objButtonBaseDown.Show()
			self.objButtonIdentifier.Show()
			self.objButtonLightEffect.Hide()
	
	def __OnRadioUp(self, forceEvent = False):
		if not self.isRadioButton:
			return
		
		if forceEvent is False:
			if self.lastButtonState == self.DOWN_BUTTON_STATE:
				return
		
		self.lastButtonState = self.DEFAULT_BUTTON_STATE
		self.__SwitchButtonBaseState(self.lastButtonState)
	
	def __OnRadioHover(self):
		if not self.isRadioButton:
			return
		
		if self.lastButtonState == self.DOWN_BUTTON_STATE:
			return
		
		self.lastButtonState = self.HOVER_BUTTON_STATE
		self.__SwitchButtonBaseState(self.lastButtonState)
	
	def __OnRadioDown(self, runEvent = True):
		if not self.isRadioButton:
			return
		
		if self.lastButtonState == self.DOWN_BUTTON_STATE:
			return
		
		self.lastButtonState = self.DOWN_BUTTON_STATE
		self.__SwitchButtonBaseState(self.lastButtonState)
		
		if runEvent is True:
			if self.clickEvent:
				apply(*self.clickEvent)
	
	def __OnUp(self, checkState = True):
		if self.isRadioButton:
			return
		
		if checkState and self.lastButtonState == self.DISABLE_BUTTON_STATE:
			return
		
		self.lastButtonState = self.DEFAULT_BUTTON_STATE
		self.__SwitchButtonBaseState(self.lastButtonState)
	
	def __OnHover(self):
		if self.isRadioButton:
			return
		
		if self.lastButtonState == self.DISABLE_BUTTON_STATE:
			return
		
		self.lastButtonState = self.HOVER_BUTTON_STATE
		self.__SwitchButtonBaseState(self.lastButtonState)
	
	def __OnDown(self):
		if self.isRadioButton:
			return
		
		if self.lastButtonState == self.DISABLE_BUTTON_STATE:
			return
		
		self.lastButtonState = self.DOWN_BUTTON_STATE
		self.__SwitchButtonBaseState(self.lastButtonState)
	
	def __OnUpAfterDown(self):
		if self.isRadioButton:
			return
		
		if self.lastButtonState == self.DISABLE_BUTTON_STATE:
			return
		
		if self.wndAnimationController and self.wndAnimationController.IsIn():
			self.__OnHover()
			if self.clickEvent:
				apply(*self.clickEvent)
	
		#@ Public Methods
	def SetUp(self):
		self.__OnRadioUp(True)
	
	def Down(self):
		self.__OnRadioDown(False)
	
	def SAFE_SetClickEvent(self, event, *args):
		self.clickEvent = (ui.__mem_func__(event), args)
	
	def EnableButton(self):
		if not self.isRadioButton:
			self.__OnUp(False)
	
	def DisableButton(self):
		if not self.isRadioButton:
			self.lastButtonState = self.DISABLE_BUTTON_STATE
			self.__SwitchButtonBaseState(self.lastButtonState)
	
	def OnUpdate(self):
		if (self.isRadioButton or self.lastButtonState == self.DISABLE_BUTTON_STATE):
			return
		
		if (self.wndAnimationController and not self.wndAnimationController.IsIn()) and self.lastButtonState != self.DEFAULT_BUTTON_STATE:
			self.__OnUp()
	
	def BindButtonObjectImages(self, strBaseImageNorm, strBaseImageDown, strButtonIndetifier, strButtonLighEffect):
		if self.objButtonBaseNorm:
			self.objButtonBaseNorm.LoadImage(strBaseImageNorm)
		
		if self.objButtonBaseDown:
			self.objButtonBaseDown.LoadImage(strBaseImageDown)
		
		if self.objButtonIdentifier:
			self.objButtonIdentifier.LoadImage(strButtonIndetifier)
		
		if self.objButtonLightEffect:
			self.objButtonLightEffect.LoadImage(strButtonLighEffect)
		
		self.lastButtonState = self.DEFAULT_BUTTON_STATE
		
		self.__ReloadButtonSize([self.objButtonBaseNorm.GetWidth(), self.objButtonBaseNorm.GetHeight()])
		self.__SwitchButtonBaseState(self.lastButtonState)
	
	def SetAlphaSensitiveImages(self, state):
		if state:
			self.AddFlag('alpha_sensitive')
			
			if self.objButtonBaseNorm:
				self.objButtonBaseNorm.AddFlag('alpha_sensitive')
			
			if self.objButtonBaseDown:
				self.objButtonBaseDown.AddFlag('alpha_sensitive')
			
			if self.objButtonIdentifier:
				self.objButtonIdentifier.AddFlag('alpha_sensitive')
			
			if self.objButtonLightEffect:
				self.objButtonLightEffect.AddFlag('alpha_sensitive')
	
	def Show(self):
		super(MoreiraButton, self).Show()
	
	def Hide(self):
		super(MoreiraButton, self).Hide()
