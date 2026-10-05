import app
import ime
import grp
import snd
import wndMgr
import item
import skill
import localeInfo
import dbg
# MARK_BUG_FIX
import guild
# END_OF_MARK_BUG_FIX
import constInfo
import sys
from constInfo import ENABLE_RECURSIVE_UI_DESTROY
from _weakref import proxy
import gamefont
import mathUtils
import colorInfo
import math

from ui_event import MakeEvent, Event, MakeCallback

EmptyFunction = lambda *args, **kwargs : None

if ENABLE_RECURSIVE_UI_DESTROY:
	WOC_WHITELIST = {'hWnd', 'parentWindow', 'Children', 'ElementDictionary', 'windowName', 'WocIsDestroyed', 'WocIsCleaned'}
	WOC_ENABLE_RECURSIVE_CLEANER = False #unnecessary
	WOC_ENABLE_PRINT_DEBUG = False
	WOC_ENABLE_PRINT_DEL_DEBUG = False
	WOC_ENABLE_FORCE_HIDE = True
	WOC_ENABLE_PRINT_REGISTERS = False
	WOC_ENABLE_PRINT_STACK = False

	def IsIterable(obj):
		return isinstance(obj, (list,tuple,set,dict))

	def WindowDestroy(func):
		def _decorator(self, *args, **kwargs):
			if WOC_ENABLE_PRINT_DEL_DEBUG: dbg.TraceError("WindowDestroy decorator called for {}".format(self.__class__.__name__))
			if WOC_ENABLE_FORCE_HIDE: self.Hide()
			WindowObjectCleaner(self)
			func(self, *args, **kwargs)
		return _decorator

	def WindowObjectCleaner(obj):
		######################################
		def WindowDestroyer(obj):
			if WOC_ENABLE_RECURSIVE_CLEANER:
				WindowObjectCleaner(obj)

			fDestroy = getattr(obj, 'Destroy', None)
			if fDestroy and not getattr(obj, "WocIsDestroyed", False):
				setattr(obj, "WocIsDestroyed", True)
				if WOC_ENABLE_FORCE_HIDE: obj.Hide()
				fDestroy()
		######################################
		def ObjectItering(obj):
			if isinstance(obj, (list,tuple,set)):
				for elem in obj:
					ObjectItering(elem)

			elif isinstance(obj, dict):
				for k in obj:
					ObjectItering(obj[k])

			elif isinstance(obj, (Window,NoWindow)):
				WindowDestroyer(obj)
		######################################
		if WOC_ENABLE_RECURSIVE_CLEANER:
			if getattr(obj, "WocIsCleaned", False):
				if WOC_ENABLE_PRINT_DEBUG: dbg.TraceError("WocIsCleaned already True")
				return
			setattr(obj, "WocIsCleaned", True)

		if WOC_ENABLE_PRINT_DEBUG: dbg.TraceError("WindowObjectCleaner vars: {}".format(repr(vars(obj).keys())))
		for elemName in vars(obj).keys():
			elemObj = getattr(obj, elemName)
			elemType = type(elemObj)

			if elemName in WOC_WHITELIST:
				continue

			if WOC_ENABLE_PRINT_DEBUG: dbg.TraceError("WindowObjectCleaner elemName {}".format(elemName))
			if isinstance(elemObj, (Window,NoWindow)):
				if WOC_ENABLE_PRINT_DEBUG: dbg.TraceError("WindowDestroyer elemName {}".format(elemName))
				WindowDestroyer(elemObj)

			elif IsIterable(elemObj):
				if WOC_ENABLE_PRINT_DEBUG: dbg.TraceError("{} is iterable: {}".format(elemName, repr(elemObj)))
				ObjectItering(elemObj)
				elemObj = elemType()
				continue

			setattr(obj, elemName, None)

else:
	WOC_ENABLE_PRINT_DEL_DEBUG = False
	WOC_ENABLE_PRINT_REGISTERS = False
	def WindowDestroy(func):
		def _decorator(self, *args, **kwargs):
			func(self, *args, **kwargs)
		return _decorator


BACKGROUND_COLOR = grp.GenerateColor(0.0, 0.0, 0.0, 1.0)
DARK_COLOR = grp.GenerateColor(0.2, 0.2, 0.2, 1.0)
BRIGHT_COLOR = grp.GenerateColor(0.7, 0.7, 0.7, 1.0)

SELECT_COLOR = grp.GenerateColor(0.0, 0.0, 0.5, 0.3)

WHITE_COLOR = grp.GenerateColor(1.0, 1.0, 1.0, 0.5)
HALF_WHITE_COLOR = grp.GenerateColor(1.0, 1.0, 1.0, 0.2)

createToolTipWindowDict = {}
def RegisterCandidateWindowClass(codePage, candidateWindowClass):
	EditLine.candidateWindowClassDict[codePage]=candidateWindowClass
def RegisterToolTipWindow(type, createToolTipWindow):
	createToolTipWindowDict[type]=createToolTipWindow

app.SetDefaultFontName(gamefont.GetCurrentFont(), gamefont.GetCurrentFontHuge())

## Window Manager Event List##
##############################
## "OnMouseLeftButtonDown"
## "OnMouseLeftButtonUp"
## "OnMouseLeftButtonDoubleClick"
## "OnMouseRightButtonDown"
## "OnMouseRightButtonUp"
## "OnMouseRightButtonDoubleClick"
## "OnMouseDrag"
## "OnSetFocus"
## "OnKillFocus"
## "OnMouseOverIn"
## "OnMouseOverOut"
## "OnRender"
## "OnUpdate"
## "OnKeyDown"
## "OnKeyUp"
## "OnTop"
## "OnIMEUpdate" ## IME Only
## "OnIMETab"	## IME Only
## "OnIMEReturn" ## IME Only
##############################
## Window Manager Event List##


class __mem_func__:
	class __noarg_call__:
		def __init__(self, cls, obj, func):
			self.cls=cls
			self.obj=proxy(obj)
			self.func=proxy(func)

		def __call__(self, *arg):
			return self.func(self.obj)

	class __arg_call__:
		def __init__(self, cls, obj, func):
			self.cls=cls
			self.obj=proxy(obj)
			self.func=proxy(func)

		def __call__(self, *arg):
			return self.func(self.obj, *arg)

	def __init__(self, mfunc):
		if mfunc.im_func.func_code.co_argcount>1:
			self.call=__mem_func__.__arg_call__(mfunc.im_class, mfunc.im_self, mfunc.im_func)
		else:
			self.call=__mem_func__.__noarg_call__(mfunc.im_class, mfunc.im_self, mfunc.im_func)

	def __call__(self, *arg):
		return self.call(*arg)

if WOC_ENABLE_PRINT_REGISTERS:
	WocWindowRegister = {}
	WocWindowNames = {}
	if WOC_ENABLE_PRINT_STACK:
		WocWindowStack = {}

	def WocGetStackList():
		stacks = []
		try:
			for i in range(10):
				stack = sys._getframe(i)
				trace = stack.f_code.co_filename
				line = stack.f_lineno
				module = stack.f_code.co_name
				stacks.append((trace, line, module))
		except ValueError:
			pass

		return stacks

	def WocDumpRegisters():
		# if called at the end of prototype.py RunApp, you'll get no leaks, but you'll miss all the ingame-leaks
		# if called inside networkmodule.py MainStream.Destroy, mouseModule.mouseController is still allocating NumberLine 1x
		with old_open("windowRegisters.txt", "w") as f1:
			for k,v in WocWindowRegister.items():
				f1.write("window {} count {} name {}\n".format(k,v,WocWindowNames.get(k, "NONAME")))
				if WOC_ENABLE_PRINT_STACK:
					if v > 0:
						for stack in WocWindowStack[k]:
							f1.write("\tfile {} line {} name {}\n".format(*stack))

class NoWindow:
	@WindowDestroy
	def Destroy(self):
		pass
	def Hide(self):
		pass

class Window(object):
	def NoneMethod(cls):
		pass

	NoneMethod = classmethod(NoneMethod)

	def __init__(self, layer = "UI"):
		self.hWnd = None
		self.parentWindow = 0
		self.onMouseLeftButtonUpEvent = None
		self.clickEvent = None
		self.eventDict = {}
		self.eventArgs = {}

		self.RegisterWindow(layer)
		self.baseWindowSizeChangeEvent = None

		# self.Hide()
		if WOC_ENABLE_PRINT_REGISTERS:
			global WocWindowRegister
			WocKey = "{}[{}]".format(self.__class__.__name__, id(self))
			try:
				WocWindowRegister[WocKey] += 1
			except:
				WocWindowRegister[WocKey] = 1
				if WOC_ENABLE_PRINT_STACK:
					global WocWindowStack
					WocWindowStack[WocKey] = WocGetStackList()

	def __del__(self):
		wndMgr.Destroy(self.hWnd)
		if WOC_ENABLE_PRINT_DEL_DEBUG: import dbg; dbg.TraceError("{} __del__ called".format(self.__class__.__name__))
		if WOC_ENABLE_PRINT_REGISTERS:
			global WocWindowRegister,WocWindowStack
			WocKey = "{}[{}]".format(self.__class__.__name__, id(self))
			try:
				WocWindowRegister[WocKey] -= 1
			except:
				WocWindowRegister[WocKey] = -1

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.Register(self, layer)

	@WindowDestroy
	def Destroy(self):
		pass

	def GetWindowHandle(self):
		return self.hWnd

	def AddFlag(self, style):
		wndMgr.AddFlag(self.hWnd, style)

	def IsRTL(self):
		return wndMgr.IsRTL(self.hWnd)

	# MT2009_PLUS_MONSTER_CARDS_V1: the official client's ui.py mirrors a window for its
	# right-to-left locales; ours has none (the card window calls it under IsRTL only).
	def LeftRightReverse(self):
		pass

	def SetWindowName(self, Name):
		wndMgr.SetName(self.hWnd, Name)
		if WOC_ENABLE_PRINT_REGISTERS:
			global WocWindowNames
			WocKey = "{}[{}]".format(self.__class__.__name__, id(self))
			WocWindowNames[WocKey] = self.GetWindowName()

	def GetWindowName(self):
		return wndMgr.GetName(self.hWnd)

	def SetParent(self, parent):
		wndMgr.SetParent(self.hWnd, parent.hWnd)

	def SetParentProxy(self, parent):
		self.parentWindow=proxy(parent)
		wndMgr.SetParent(self.hWnd, parent.hWnd)

	def GetParentProxy(self):
		return self.parentWindow

	def SetPickAlways(self):
		wndMgr.SetPickAlways(self.hWnd)

	def SetWindowHorizontalAlignLeft(self):
		wndMgr.SetWindowHorizontalAlign(self.hWnd, wndMgr.HORIZONTAL_ALIGN_LEFT)

	def SetWindowHorizontalAlignCenter(self):
		wndMgr.SetWindowHorizontalAlign(self.hWnd, wndMgr.HORIZONTAL_ALIGN_CENTER)

	def SetWindowHorizontalAlignRight(self):
		wndMgr.SetWindowHorizontalAlign(self.hWnd, wndMgr.HORIZONTAL_ALIGN_RIGHT)

	def SetWindowVerticalAlignTop(self):
		wndMgr.SetWindowVerticalAlign(self.hWnd, wndMgr.VERTICAL_ALIGN_TOP)

	def SetWindowVerticalAlignCenter(self):
		wndMgr.SetWindowVerticalAlign(self.hWnd, wndMgr.VERTICAL_ALIGN_CENTER)

	def SetWindowVerticalAlignBottom(self):
		wndMgr.SetWindowVerticalAlign(self.hWnd, wndMgr.VERTICAL_ALIGN_BOTTOM)

	def SetTop(self):
		wndMgr.SetTop(self.hWnd)

	def Show(self):
		wndMgr.Show(self.hWnd)

	def Hide(self):
		wndMgr.Hide(self.hWnd)

	def Lock(self):
		wndMgr.Lock(self.hWnd)

	def Unlock(self):
		wndMgr.Unlock(self.hWnd)

	def IsShow(self):
		return wndMgr.IsShow(self.hWnd)

	def SetVisible(self, is_show):
		if is_show == True:
			self.Show()
		else:
			self.Hide()

	def UpdateRect(self):
		wndMgr.UpdateRect(self.hWnd)

	def SetSize(self, width, height):
		wndMgr.SetWindowSize(self.hWnd, width, height)
		if self.baseWindowSizeChangeEvent:
			self.baseWindowSizeChangeEvent()

	def GetWidth(self):
		return wndMgr.GetWindowWidth(self.hWnd)

	def GetHeight(self):
		return wndMgr.GetWindowHeight(self.hWnd)
	def GetSize(self):
		return (self.GetWidth(), self.GetHeight(), )

	def GetLocalPosition(self):
		return wndMgr.GetWindowLocalPosition(self.hWnd)

	def GetGlobalPosition(self):
		return wndMgr.GetWindowGlobalPosition(self.hWnd)

	def GetMouseLocalPosition(self):
		return wndMgr.GetMouseLocalPosition(self.hWnd)

	def GetRect(self):
		return wndMgr.GetWindowRect(self.hWnd)

	def SetPosition(self, x, y):
		wndMgr.SetWindowPosition(self.hWnd, x, y)

	def SetCenterPosition(self, x = 0, y = 0):
		self.SetPosition((wndMgr.GetScreenWidth() - self.GetWidth()) / 2 + x, (wndMgr.GetScreenHeight() - self.GetHeight()) / 2 + y)

	def IsFocus(self):
		return wndMgr.IsFocus(self.hWnd)

	def SetFocus(self):
		wndMgr.SetFocus(self.hWnd)

	def KillFocus(self):
		wndMgr.KillFocus(self.hWnd)

	def GetChildCount(self):
		return wndMgr.GetChildCount(self.hWnd)

	def IsInPosition(self):
		xMouse, yMouse = wndMgr.GetMousePosition()
		x, y = self.GetGlobalPosition()
		return xMouse >= x and xMouse < x + self.GetWidth() and yMouse >= y and yMouse < y + self.GetHeight()

	def IsIn(self):
		return wndMgr.IsIn(self.hWnd)

	def OnMouseOverIn(self):
		if "MOUSE_OVER_IN" in self.eventDict:
			apply(self.eventDict["MOUSE_OVER_IN"], self.eventArgs.get("MOUSE_OVER_IN", ()))

	def OnMouseOverOut(self):
		if "MOUSE_OVER_OUT" in self.eventDict:
			apply(self.eventDict["MOUSE_OVER_OUT"], self.eventArgs.get("MOUSE_OVER_OUT", ()))

	def SetEvent(self, name, event, *args):
		self.SetStringEvent(name, event, *args)

	def SAFE_SetEvent(self, name, event, *args):
		self.SAFE_SetStringEvent(name, event, *args)

	def SAFE_SetStringEvent(self, name, event, *args):
		self.SetStringEvent(name, __mem_func__(event), *args)

	def SetMouseWheelEvent(self, event):
		self.SAFE_SetStringEvent("MOUSE_WHEEL_UP", event, 1)
		self.SAFE_SetStringEvent("MOUSE_WHEEL_DOWN", event, -1)

	# def SetOverInEvent(self, event, *args):
	# 	self.SAFE_SetStringEvent("MOUSE_OVER_IN", event, *args)

	# def SetOverOutEvent(self, event, *args):
	# 	self.SAFE_SetStringEvent("MOUSE_OVER_OUT", event, *args)
	
	# def SetMouseLeftButtonDownEvent(self, event, *args):
	# 	self.SAFE_SetStringEvent("MOUSE_LEFT_BUTTON", event, *args)

	def ClearEvents(self):
		self.eventDict = {}
		self.eventArgs = {}

	def SetStringEvent(self, name, event, *args):
		self.eventDict[name] = event
		self.eventArgs[name] = args

	def SetClickEvent(self, event):
		self.clickEvent = __mem_func__(event)

	def OnMouseRightButtonUp(self):
		if "MOUSE_RIGHT_BUTTON_UP" in self.eventDict:
			self.eventDict["MOUSE_RIGHT_BUTTON_UP"](*self.eventArgs.get("MOUSE_RIGHT_BUTTON_UP", ()))

	def OnMouseRightButtonDown(self):
		if "MOUSE_RIGHT_BUTTON" in self.eventDict:
			self.eventDict["MOUSE_RIGHT_BUTTON"](*self.eventArgs.get("MOUSE_RIGHT_BUTTON", ()))

	def SetOnMouseLeftButtonUpEvent(self, event):
		self.SAFE_SetEvent("MOUSE_LEFT_BUTTON_UP", event)

	def OnMouseLeftButtonUp(self):
		if "MOUSE_LEFT_BUTTON_UP" in self.eventDict:
			self.eventDict["MOUSE_LEFT_BUTTON_UP"](*self.eventArgs.get("MOUSE_LEFT_BUTTON_UP", ()))

	def OnMouseLeftButtonDown(self):
		if self.clickEvent:
			self.clickEvent()
		if "MOUSE_LEFT_BUTTON" in self.eventDict:
			self.eventDict["MOUSE_LEFT_BUTTON"](*self.eventArgs.get("MOUSE_LEFT_BUTTON", ()))

	def OnMouseWheel(self, nLen):
		if nLen > 0:
			if "MOUSE_WHEEL_UP" in self.eventDict:
				apply(self.eventDict["MOUSE_WHEEL_UP"], self.eventArgs.get("MOUSE_WHEEL_UP", ()))
				return True
			else:
				return False
		elif nLen < 0:
			if "MOUSE_WHEEL_DOWN" in self.eventDict:
				apply(self.eventDict["MOUSE_WHEEL_DOWN"], self.eventArgs.get("MOUSE_WHEEL_DOWN", ()))
				return True
			else:
				return False
		return False

	def RefreshPosition(self):
		(x, y) = self.GetLocalPosition()
		self.SetPosition(x, y)

	def MovePosition(self, moveX, moveY):
		(x, y) = self.GetLocalPosition()
		self.SetPosition(x + moveX, y + moveY)

	def SetWidth(self, width):
		self.SetSize(width, self.GetHeight())

	def SetHeight(self, height):
		self.SetSize(self.GetWidth(), height)

	if app.__BL_CLIP_MASK__:
		def SetClippingMaskRect(self, left, top, right, bottom):
			wndMgr.SetClippingMaskRect(self.hWnd, left, top, right, bottom)

		def SetClippingMaskWindow(self, clipping_mask_window):
			wndMgr.SetClippingMaskWindow(self.hWnd, clipping_mask_window.hWnd)

	def SetSizeChangeEvent(self, event):
		self.baseWindowSizeChangeEvent = MakeEvent(event)

class BaseEvents(object):

	def __init__(self):
		super(BaseEvents, self).__init__()
		self.eventOnMouseOverIn = None
		self.eventOnMouseOverOut = None
		self.eventOnMouseClick = None

	def SetOverInEvent(self, event):
		self.eventOnMouseOverIn = MakeEvent(event)

	def SetOverOutEvent(self, event):
		self.eventOnMouseOverOut = MakeEvent(event)

	def SetMouseLeftButtonDownEvent(self, event):
		self.eventOnMouseClick = MakeEvent(event)

	def OnMouseOverIn(self):
		debug("OnMouseOverIn")
		if self.eventOnMouseOverIn:
			return self.eventOnMouseOverIn()

		return False

	def OnMouseOverOut(self):
		if self.eventOnMouseOverOut:
			return self.eventOnMouseOverOut()

		return False

	def OnMouseLeftButtonDown(self):
		if self.eventOnMouseClick:
			return self.eventOnMouseClick()

		return False


class WindowWithBaseEvents(Window, BaseEvents):

	def __init__(self):
		Window.__init__(self)
		BaseEvents.__init__(self)

class ListBoxEx(Window):

	class Item(Window):
		def __init__(self):
			Window.__init__(self)

		def __del__(self):
			Window.__del__(self)

		def SetParent(self, parent):
			Window.SetParent(self, parent)
			self.parent = proxy(parent)

		def OnMouseLeftButtonDown(self):
			self.parent.SelectItem(self)

		def OnRender(self):
			if self.parent.GetSelectedItem() == self:
				self.OnSelectedRender()

		def OnSelectedRender(self):
			x, y = self.GetGlobalPosition()
			grp.SetColor(grp.GenerateColor(0.0, 0.0, 0.7, 0.7))
			grp.RenderBar(x, y, self.GetWidth(), self.GetHeight())

	def __init__(self):
		Window.__init__(self)

		self.viewItemCount = 10
		self.basePos = 0
		self.itemHeight = 16
		self.itemStep = 20
		self.selItem = 0
		self.itemList = []
		self.onSelectItemEvent = lambda *arg: None

		if localeInfo.IsARABIC():
			self.itemWidth = 130
		else:
			self.itemWidth = 100

		self.scrollBar = None
		self.__UpdateSize()

	def __del__(self):
		Window.__del__(self)

	def __UpdateSize(self):
		height = self.itemStep * self.__GetViewItemCount()

		self.SetSize(self.itemWidth, height)

	def IsEmpty(self):
		if len(self.itemList) == 0:
			return 1
		return 0

	def SetItemStep(self, itemStep):
		self.itemStep = itemStep
		self.__UpdateSize()

	def SetItemSize(self, itemWidth, itemHeight):
		self.itemWidth = itemWidth
		self.itemHeight = itemHeight
		self.__UpdateSize()

	def SetViewItemCount(self, viewItemCount):
		self.viewItemCount = viewItemCount

	def SetSelectEvent(self, event):
		self.onSelectItemEvent = event

	def SetBasePos(self, basePos):
		for oldItem in self.itemList[self.basePos:self.basePos+self.viewItemCount]:
			oldItem.Hide()

		self.basePos=basePos

		pos=basePos
		for newItem in self.itemList[self.basePos:self.basePos+self.viewItemCount]:
			(x, y)=self.GetItemViewCoord(pos, newItem.GetWidth())
			newItem.SetPosition(x, y)
			newItem.Show()
			pos+=1

	def GetItemIndex(self, argItem):
		return self.itemList.index(argItem)

	def GetSelectedItem(self):
		return self.selItem

	def SelectIndex(self, index):

		if index >= len(self.itemList) or index < 0:
			self.selItem = None
			return

		try:
			self.selItem = self.itemList[index]
		except:
			pass

	def SelectItem(self, selItem):
		self.selItem = selItem
		self.onSelectItemEvent(selItem)

	def RemoveAllItems(self):
		for item in self.itemList:
			item.Hide()

		self.selItem = None
		self.itemList = []

		if self.scrollBar:
			self.scrollBar.SetPos(0)

	def RemoveItem(self, delItem):
		if delItem == self.selItem:
			self.selItem = None

		self.itemList.remove(delItem)

	def AppendItem(self, newItem):
		newItem.SetParent(self)
		newItem.SetSize(self.itemWidth, self.itemHeight)

		pos=len(self.itemList)
		if self.__IsInViewRange(pos):
			(x, y) = self.GetItemViewCoord(pos, newItem.GetWidth())
			newItem.SetPosition(x, y)
			newItem.Show()
		else:
			newItem.Hide()

		self.itemList.append(newItem)

	def SetScrollBar(self, scrollBar):
		scrollBar.SetScrollEvent(__mem_func__(self.__OnScroll))
		self.scrollBar = scrollBar

	def __OnScroll(self):
		self.SetBasePos(int(self.scrollBar.GetPos()*self.__GetScrollLen()))

	def __GetScrollLen(self):
		scrollLen = self.__GetItemCount() - self.__GetViewItemCount()
		if scrollLen < 0:
			return 0

		return scrollLen

	def __GetViewItemCount(self):
		return self.viewItemCount

	def __GetItemCount(self):
		return len(self.itemList)

	GetItemCount = __GetItemCount

	def GetItemViewCoord(self, pos, itemWidth):
		if localeInfo.IsARABIC():
			return (self.GetWidth() - itemWidth - 10, (pos - self.basePos) * self.itemStep)
		else:
			return (0, (pos - self.basePos) * self.itemStep)

	def __IsInViewRange(self, pos):
		if pos < self.basePos:
			return 0
		if pos >= self.basePos + self.viewItemCount:
			return 0
		return 1

class CandidateListBox(ListBoxEx):

	HORIZONTAL_MODE = 0
	VERTICAL_MODE = 1

	class Item(ListBoxEx.Item):
		def __init__(self, text):
			ListBoxEx.Item.__init__(self)

			self.textBox=TextLine()
			self.textBox.SetParent(self)
			self.textBox.SetText(text)
			self.textBox.Show()

		def __del__(self):
			ListBoxEx.Item.__del__(self)

	def __init__(self, mode = HORIZONTAL_MODE):
		ListBoxEx.__init__(self)
		self.itemWidth=32
		self.itemHeight=32
		self.mode = mode

	def __del__(self):
		ListBoxEx.__del__(self)

	def SetMode(self, mode):
		self.mode = mode

	def AppendItem(self, newItem):
		ListBoxEx.AppendItem(self, newItem)

	def GetItemViewCoord(self, pos):
		if self.mode == self.HORIZONTAL_MODE:
			return ((pos-self.basePos)*self.itemStep, 0)
		elif self.mode == self.VERTICAL_MODE:
			return (0, (pos-self.basePos)*self.itemStep)


class TextLine(Window):
	DEFAULT_COLOR = grp.GenerateColor(0.8007, 0.8007, 0.8007, 1.0)
	def __init__(self, layer="UI"):
		Window.__init__(self, layer)
		self.max = 0
		self.color = [0.761, 0.761, 0.761, 1.0]
		self.fontName = None
		self.SetDefaultFontName()

	def __del__(self):
		Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterTextLine(self, layer)

	def SetMax(self, max):
		wndMgr.SetMax(self.hWnd, max)

	def SetLimitWidth(self, width):
		wndMgr.SetLimitWidth(self.hWnd, width)

	def SetMultiLine(self):
		wndMgr.SetMultiLine(self.hWnd, True)

	def SetHorizontalAlignArabic(self):
		wndMgr.SetHorizontalAlign(self.hWnd, wndMgr.TEXT_HORIZONTAL_ALIGN_ARABIC)

	def SetHorizontalAlignLeft(self):
		wndMgr.SetHorizontalAlign(self.hWnd, wndMgr.TEXT_HORIZONTAL_ALIGN_LEFT)

	def SetHorizontalAlignRight(self):
		wndMgr.SetHorizontalAlign(self.hWnd, wndMgr.TEXT_HORIZONTAL_ALIGN_RIGHT)

	def SetHorizontalAlignCenter(self):
		wndMgr.SetHorizontalAlign(self.hWnd, wndMgr.TEXT_HORIZONTAL_ALIGN_CENTER)

	def SetVerticalAlignTop(self):
		wndMgr.SetVerticalAlign(self.hWnd, wndMgr.TEXT_VERTICAL_ALIGN_TOP)

	def SetVerticalAlignBottom(self):
		wndMgr.SetVerticalAlign(self.hWnd, wndMgr.TEXT_VERTICAL_ALIGN_BOTTOM)

	def SetVerticalAlignCenter(self):
		wndMgr.SetVerticalAlign(self.hWnd, wndMgr.TEXT_VERTICAL_ALIGN_CENTER)

	def SetAllAlignCenter(self):
		self.SetHorizontalAlignCenter()
		self.SetVerticalAlignCenter()
		self.SetWindowHorizontalAlignCenter()
		self.SetWindowVerticalAlignCenter()

	def SetSecret(self, Value=True):
		wndMgr.SetSecret(self.hWnd, Value)

	def SetOutline(self, Value=True):
		wndMgr.SetOutline(self.hWnd, Value)

	def SetFeather(self, value=True):
		wndMgr.SetFeather(self.hWnd, value)

	def SetFontAttribute(self, isSet, attributeString):
		self.SetFontName(self.fontName + attributeString if isSet else "")

	def SetBold(self, value=True):
		self.SetFontAttribute(value, "b")

	def SetItalic(self, value=True):
		self.SetFontAttribute(value, "i")

	def SetUnderline(self, value=True):
		self.SetFontAttribute(value, "u")

	def SetStrikeOut(self, value=True):
		self.SetFontAttribute(value, "s")

	def SetFontName(self, fontName):
		self.fontName = fontName
		wndMgr.SetFontName(self.hWnd, fontName)

	def SetDefaultFontName(self):
		self.SetFontName(gamefont.GetCurrentFont())

	def SetLargeFont(self):
		self.SetFontName(gamefont.GetCurrentFontLarge())

	def SetSmallFont(self):
		self.SetFontName(gamefont.GetCurrentFontSmall())

	def SetOutlineColor(self, red, green, blue, alpha=1.0):
		self.color = [red, green, blue, alpha]
		wndMgr.SetOutlineColor(self.hWnd, red, green, blue)

	def SetPackedOutlineColor(self, color):
		wndMgr.SetOutlineColor(self.hWnd, color)

	def SetFontColor(self, red, green, blue, alpha=1.0):
		self.color = [red, green, blue, alpha]
		wndMgr.SetFontColor(self.hWnd, red, green, blue)

	def SetPackedFontColor(self, color):
		wndMgr.SetFontColor(self.hWnd, color)

	def SetFontAlpha(self, alpha):
		clr = grp.GenerateColor(self.color[0], self.color[1], self.color[2], alpha)
		wndMgr.SetFontColor(self.hWnd, clr)

	def SetFontColorTuple(self, color_tuple):
		self.SetPackedFontColor(colorInfo.GetColorFromColorTuple(color_tuple))

	def SetText(self, text):
		wndMgr.SetText(self.hWnd, text)

	def GetText(self):
		return wndMgr.GetText(self.hWnd)

	def GetTextSize(self):
		return wndMgr.GetTextSize(self.hWnd)
	if app.ENABLE_IKASHOP_RENEWAL:
		def FitText(self):
			self.SetSize(*self.GetTextSize())



class EmptyCandidateWindow(Window):
	def __init__(self):
		Window.__init__(self)

	def __del__(self):
		Window.__del__(self)

	def Load(self):
		pass

	def SetCandidatePosition(self, x, y, textCount):
		pass

	def Clear(self):
		pass

	def Append(self, text):
		pass

	def Refresh(self):
		pass

	def Select(self):
		pass

class EditLine(TextLine):
	candidateWindowClassDict = {}

	PLACEHOLDER_COLOR = grp.GenerateColor(0.5, 0.5, 0.5, 1.0)

	def __init__(self):
		TextLine.__init__(self)

		self.eventReturn = Window.NoneMethod
		self.eventEscape = Window.NoneMethod
		self.eventKeyDown = None
		self.eventTab = None
		self.numberMode = False
		self.useIME = True

		self.bCodePage = False
		if app.ENABLE_IKASHOP_RENEWAL:
			self.updateEvent = None

		self.candidateWindowClass = None
		self.candidateWindow = None
		self.SetCodePage(app.GetDefaultCodePage())

		self.readingWnd = ReadingWnd()
		self.readingWnd.Hide()

		self.placeholderTextLine = None

	def __del__(self):
		TextLine.__del__(self)

		self.eventReturn = Window.NoneMethod
		self.eventEscape = Window.NoneMethod
		self.eventKeyDown = None
		self.eventTab = None

	def SetCodePage(self, codePage):
		candidateWindowClass=EditLine.candidateWindowClassDict.get(codePage, EmptyCandidateWindow)
		self.__SetCandidateClass(candidateWindowClass)

	def __SetCandidateClass(self, candidateWindowClass):
		if self.candidateWindowClass==candidateWindowClass:
			return

		self.candidateWindowClass = candidateWindowClass
		self.candidateWindow = self.candidateWindowClass()
		self.candidateWindow.Load()
		self.candidateWindow.Hide()

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterTextLine(self, layer)

	def SAFE_SetKeyDownEvent(self, event):
		self.eventKeyDown = __mem_func__(event)

	def SAFE_SetReturnEvent(self, event):
		self.eventReturn = __mem_func__(event)

	def SetReturnEvent(self, event):
		self.eventReturn = event

	def SetEscapeEvent(self, event):
		self.eventEscape = event

	def SetTabEvent(self, event):
		self.eventTab = event

	if app.ENABLE_IKASHOP_RENEWAL:
		def SetUpdateEvent(self, event):
			self.updateEvent = event

	def SetMax(self, max):
		self.max = max
		wndMgr.SetMax(self.hWnd, self.max)
		ime.SetMax(self.max)
		self.SetUserMax(self.max)

	def SetUserMax(self, max):
		self.userMax = max
		ime.SetUserMax(self.userMax)

	def SetNumberMode(self, state=True):
		self.numberMode = state

	def SetPlaceholder(self, text, fontName=None):
		self.placeholderTextLine = TextLine()
		self.placeholderTextLine.SetParent(self)
		self.placeholderTextLine.SetPosition(1, 1)
		if fontName:
			self.placeholderTextLine.SetFontName(fontName)
		self.placeholderTextLine.SetPackedFontColor(self.PLACEHOLDER_COLOR)
		self.placeholderTextLine.SetText(text)
		self.placeholderTextLine.Show()

	def CanPlaceholder(self):
		return self.placeholderTextLine and len(self.GetText()) < 1

	#def AddExceptKey(self, key):
	#	ime.AddExceptKey(key)

	#def ClearExceptKey(self):
	#	ime.ClearExceptKey()

	def SetIMEFlag(self, flag):
		self.useIME = flag

	def SetText(self, text):
		wndMgr.SetText(self.hWnd, text)

		if self.IsFocus():
			ime.SetText(text)

	def Clear(self):
		self.SetText("")
		self.KillFocus()

	def Enable(self):
		wndMgr.ShowCursor(self.hWnd)

	def Disable(self):
		wndMgr.HideCursor(self.hWnd)

	def SetEndPosition(self):
		ime.MoveEnd()

	def OnSetFocus(self):
		Text = self.GetText()
		ime.SetText(Text)
		ime.SetMax(self.max)
		ime.SetUserMax(self.userMax)
		ime.SetCursorPosition(-1)

		if self.CanPlaceholder():
			self.placeholderTextLine.Hide()

		if self.numberMode:
			ime.SetNumberMode()
		else:
			ime.SetStringMode()
		ime.EnableCaptureInput()
		if self.useIME:
			ime.EnableIME()
		else:
			ime.DisableIME()
		wndMgr.ShowCursor(self.hWnd, True)
		if app.ENABLE_IKASHOP_RENEWAL:
			if self.updateEvent:
				self.updateEvent(True)

	def OnKillFocus(self):
		self.SetText(ime.GetText(self.bCodePage))
		self.OnIMECloseCandidateList()
		self.OnIMECloseReadingWnd()
		ime.DisableIME()
		ime.DisableCaptureInput()
		wndMgr.HideCursor(self.hWnd)
		if app.ENABLE_IKASHOP_RENEWAL:
			if self.updateEvent:
				self.updateEvent(False)

		if self.CanPlaceholder():
			self.placeholderTextLine.Show()


	def OnIMEChangeCodePage(self):
		self.SetCodePage(ime.GetCodePage())

	def OnIMEOpenCandidateList(self):
		self.candidateWindow.Show()
		self.candidateWindow.Clear()
		self.candidateWindow.Refresh()

		gx, gy = self.GetGlobalPosition()
		self.candidateWindow.SetCandidatePosition(gx, gy, len(self.GetText()))

		return True

	def OnIMECloseCandidateList(self):
		self.candidateWindow.Hide()
		return True

	def OnIMEOpenReadingWnd(self):
		gx, gy = self.GetGlobalPosition()
		textlen = len(self.GetText())-2
		reading = ime.GetReading()
		readinglen = len(reading)
		self.readingWnd.SetReadingPosition( gx + textlen*6-24-readinglen*6, gy )
		self.readingWnd.SetText(reading)
		if ime.GetReadingError() == 0:
			self.readingWnd.SetTextColor(0xffffffff)
		else:
			self.readingWnd.SetTextColor(0xffff0000)
		self.readingWnd.SetSize(readinglen * 6 + 4, 19)
		self.readingWnd.Show()
		return True

	def OnIMECloseReadingWnd(self):
		self.readingWnd.Hide()
		return True

	def IsNumber(self, text) :
		try :
			int(text)
			return True
		except ValueError :
			return False

	def IsIncludeCodePage(self, text) :
		textLen = len(text)
		idx = 0

		while(idx < textLen) :
			n = text.find("@", idx)

			if n == -1 :
				break
			elif n+4 < textLen and self.IsNumber(text[n+1:n+4]) == True:
				return True
			else:
				idx = n + 1

		return False

	def OnIMEUpdate(self):
		snd.PlaySound("sound/ui/type.wav")
		TextLine.SetText(self, ime.GetText(self.bCodePage))
		if app.ENABLE_IKASHOP_RENEWAL:
			if self.updateEvent:
				self.updateEvent(self.IsFocus())



	def OnIMETab(self):
		if self.eventTab:
			self.eventTab()
			return True

		return False

	def OnIMEReturn(self):
		snd.PlaySound("sound/ui/click.wav")
		self.eventReturn()

		return True

	def OnPressEscapeKey(self):
		if self.IsFocus(): #@fixme018 (IsFocus check)
			self.eventEscape()
			return True
		return False

	def OnKeyDown(self, key):
		if app.DIK_F1 == key:
			return False
		if app.DIK_F2 == key:
			return False
		if app.DIK_F3 == key:
			return False
		if app.DIK_F4 == key:
			return False
		if app.DIK_LALT == key:
			return False
		if app.DIK_SYSRQ == key:
			return False
		if app.DIK_LCONTROL == key:
			return False
		if app.DIK_V == key:
			if app.IsPressed(app.DIK_LCONTROL):
				ime.PasteTextFromClipBoard()

		return True

	def OnKeyUp(self, key):
		if app.DIK_F1 == key:
			return False
		if app.DIK_F2 == key:
			return False
		if app.DIK_F3 == key:
			return False
		if app.DIK_F4 == key:
			return False
		if app.DIK_LALT == key:
			return False
		if app.DIK_SYSRQ == key:
			return False
		if app.DIK_LCONTROL == key:
			return False

		return True

	def OnIMEKeyDown(self, key):
		if self.eventKeyDown:
			return self.eventKeyDown(key)

		return self.BaseOnIMEKeyDown(key)

	def BaseOnIMEKeyDown(self, key):
		# Left
		if app.VK_LEFT == key:
			ime.MoveLeft()
			return True
		# Right
		if app.VK_RIGHT == key:
			ime.MoveRight()
			return True

		# Home
		if app.VK_HOME == key:
			ime.MoveHome()
			return True
		# End
		if app.VK_END == key:
			ime.MoveEnd()
			return True

		# Delete
		if app.VK_DELETE == key:
			ime.Delete()
			TextLine.SetText(self, ime.GetText(self.bCodePage))
			return True

		return True

	#def OnMouseLeftButtonDown(self):
	#	self.SetFocus()
	def OnMouseLeftButtonDown(self):
		if False == self.IsIn():
			return False

		self.SetFocus()
		PixelPosition = wndMgr.GetCursorPosition(self.hWnd)
		ime.SetCursorPosition(PixelPosition)

class MarkBox(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

	def __del__(self):
		Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterMarkBox(self, layer)

	def Load(self):
		wndMgr.MarkBox_Load(self.hWnd)

	def SetScale(self, scale):
		wndMgr.MarkBox_SetScale(self.hWnd, scale)

	def SetIndex(self, guildID):
		MarkID = guild.GuildIDToMarkID(guildID)
		wndMgr.MarkBox_SetImageFilename(self.hWnd, guild.GetMarkImageFilenameByMarkID(MarkID))
		wndMgr.MarkBox_SetIndex(self.hWnd, guild.GetMarkIndexByMarkID(MarkID))

	def SetAlpha(self, alpha):
		wndMgr.MarkBox_SetDiffuseColor(self.hWnd, 1.0, 1.0, 1.0, alpha)

class ImageBox(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

		self.eventDict={}
		self.argDict={}
		self.eventFunc = {"mouse_click" : None, "mouse_over_in" : None, "mouse_over_out" : None}
		self.eventArgs = {"mouse_click" : None, "mouse_over_in" : None, "mouse_over_out" : None}

	def __del__(self):
		Window.__del__(self)
		self.eventFunc = None
		self.eventArgs = None

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterImageBox(self, layer)

	def LoadImage(self, imageName):
		self.name=imageName
		wndMgr.LoadImage(self.hWnd, imageName)

		if len(self.eventDict)!=0:
			print("LOAD IMAGE", self, self.eventDict)

	def SetAlpha(self, alpha):
		wndMgr.SetDiffuseColor(self.hWnd, 1.0, 1.0, 1.0, alpha)

	def SetColor(self, r, g, b, a):
		wndMgr.SetDiffuseColor(self.hWnd, r, g, b, a)

	def GetWidth(self):
		return wndMgr.GetWidth(self.hWnd)

	def GetHeight(self):
		return wndMgr.GetHeight(self.hWnd)

	def SAFE_SetStringEvent(self, event, func, *args):
		self.eventDict[event]=__mem_func__(func)
		self.argDict[event]=args

	def SetStringEvent(self, event, func, *args):
		self.eventDict[event]=func
		self.argDict[event]=args

	def DelStringEvent(self, event):
		self.eventDict.pop(event, None)
		self.argDict.pop(event, None)

	def SetEvent(self, func, *args) :
		result = self.eventFunc.has_key(args[0])
		if result :
			self.eventFunc[args[0]] = func
			self.eventArgs[args[0]] = args
		else :
			print("[ERROR] ui.py SetEvent, Can`t Find has_key : %s" % args[0])

	def OnMouseLeftButtonUp(self) :
		if app.ENABLE_IKASHOP_RENEWAL:
			super(ImageBox, self).OnMouseLeftButtonUp()
		if self.eventFunc["mouse_click"] :
			apply(self.eventFunc["mouse_click"], self.eventArgs["mouse_click"])

	def OnMouseOverIn(self) :
		if self.eventFunc["mouse_over_in"] :
			apply(self.eventFunc["mouse_over_in"], self.eventArgs["mouse_over_in"])
		else:
			try:
				apply(self.eventDict["MOUSE_OVER_IN"], self.argDict["MOUSE_OVER_IN"])
			except KeyError:
				pass

	def OnMouseOverOut(self) :
		if self.eventFunc["mouse_over_out"] :
			apply(self.eventFunc["mouse_over_out"], self.eventArgs["mouse_over_out"])
		else :
			try:
				apply(self.eventDict["MOUSE_OVER_OUT"], self.argDict["MOUSE_OVER_OUT"])
			except KeyError:
				pass


class ExpandedImageBox(ImageBox):
	def __init__(self, layer = "UI"):
		ImageBox.__init__(self, layer)

	def __del__(self):
		ImageBox.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterExpandedImageBox(self, layer)

	def SetScale(self, xScale, yScale):
		wndMgr.SetScale(self.hWnd, xScale, yScale)

	def SetOrigin(self, x, y):
		wndMgr.SetOrigin(self.hWnd, x, y)

	def SetRotation(self, rotation):
		wndMgr.SetRotation(self.hWnd, rotation)

	def SetRenderingMode(self, mode):
		wndMgr.SetRenderingMode(self.hWnd, mode)

	def SetRenderingRect(self, left, top, right, bottom):
		wndMgr.SetRenderingRect(self.hWnd, left, top, right, bottom)

	def SetPercentage(self, curValue, maxValue):
		if maxValue:
			self.SetRenderingRect(0.0, 0.0, -1.0 + float(curValue) / float(maxValue), 0.0)
		else:
			self.SetRenderingRect(0.0, 0.0, 0.0, 0.0)

	def SetVPercentage(self, curValue, maxValue):
		if maxValue:
			self.SetRenderingRect(0.0, -1.0 + float(curValue) / float(maxValue), 0.0, 0.0)
		else:
			self.SetRenderingRect(0.0, 0.0, 0.0, 0.0)

	def GetWidth(self):
		return wndMgr.GetWindowWidth(self.hWnd)

	def GetHeight(self):
		return wndMgr.GetWindowHeight(self.hWnd)

class AniImageBox(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

	def __del__(self):
		Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterAniImageBox(self, layer)

	def SetDelay(self, delay):
		wndMgr.SetDelay(self.hWnd, delay)

	def AppendImage(self, filename):
		wndMgr.AppendImage(self.hWnd, filename)

	def SetPercentage(self, curValue, maxValue):
		wndMgr.SetRenderingRect(self.hWnd, 0.0, 0.0, -1.0 + float(curValue) / float(maxValue), 0.0)

	def SetPercentage2(self, progress):
		wndMgr.SetRenderingRect(self.hWnd, 0.0, 0.0, -1.0 + progress, 0.0)

	# MT2009_PLUS_MONSTER_CARDS_V1: back to the first frame - in an exe that can
	# (wndMgr.ResetFrame, the minigames' exe), a no-op in an older one.
	def ResetFrame(self):
		if hasattr(wndMgr, "ResetFrame"):
			wndMgr.ResetFrame(self.hWnd)

	def OnEndFrame(self):
		pass

class Button(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

		self.ButtonText = None
		self.ToolTipText = None
		self.tooltip = None
		self.betterToolTipText = None
		self.showToolTipEvent = None
		self.hideToolTipEvent = None

	def __del__(self):
		Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterButton(self, layer)

	def SetUpVisual(self, filename):
		wndMgr.SetUpVisual(self.hWnd, filename)

	def SetOverVisual(self, filename):
		wndMgr.SetOverVisual(self.hWnd, filename)

	def SetDownVisual(self, filename):
		wndMgr.SetDownVisual(self.hWnd, filename)

	def SetDisableVisual(self, filename):
		wndMgr.SetDisableVisual(self.hWnd, filename)

	def GetUpVisualFileName(self):
		return wndMgr.GetUpVisualFileName(self.hWnd)

	def GetOverVisualFileName(self):
		return wndMgr.GetOverVisualFileName(self.hWnd)

	def GetDownVisualFileName(self):
		return wndMgr.GetDownVisualFileName(self.hWnd)

	def Flash(self):
		wndMgr.Flash(self.hWnd)

	# MT2009_PLUS_MONSTER_CARDS_V1: the official client's Button calls the Monster Card
	# window (uimonstercard.py) leans on. A blinking button and a tooltip shown also
	# while the button is disabled come from the exe where it has them (the minigames'
	# wndMgr); an older exe leaves the button as it is.
	def EnableFlash(self):
		if hasattr(wndMgr, "EnableFlash"):
			wndMgr.EnableFlash(self.hWnd)

	def DisableFlash(self):
		if hasattr(wndMgr, "DisableFlash"):
			wndMgr.DisableFlash(self.hWnd)

	def SetAlwaysToolTip(self, isAlways):
		if hasattr(wndMgr, "SetAlwaysToolTip"):
			wndMgr.SetAlwaysToolTip(self.hWnd, isAlways)

	# A function (and its arguments) run when the mouse comes over the button / leaves
	# it, after the button's own tooltip (ShowToolTip / HideToolTip below).
	def SetShowToolTipEvent(self, func, *args):
		self.showToolTipEvent = (func, args)

	def SetHideToolTipEvent(self, func, *args):
		self.hideToolTipEvent = (func, args)

	def Enable(self):
		wndMgr.Enable(self.hWnd)

	def Disable(self):
		wndMgr.Disable(self.hWnd)

	def Down(self):
		wndMgr.Down(self.hWnd)

	def SetUp(self):
		wndMgr.SetUp(self.hWnd)

	def SAFE_SetEvent(self, func, *args):
		Window.SAFE_SetEvent(self, "MOUSE_LEFT_BUTTON_UP", func, *args)

	def SetEvent(self, func, *args):
		Window.SetEvent(self, "MOUSE_LEFT_BUTTON_UP", func, *args)

	def ClearEvent(self):
		Window.SetEvent(self, "MOUSE_LEFT_BUTTON_UP", None, None)

	def SetTextColor(self, color):
		if not self.ButtonText:
			return
		self.ButtonText.SetPackedFontColor(color)

	def SetText(self, text, height=4, isBold=False, font="default"):

		if not self.ButtonText:
			textLine = TextLine()
			textLine.SetParent(self)
			textLine.SetPosition(self.GetWidth() / 2, self.GetHeight() / 2)
			if font != "default":
				textLine.SetFontName(font)
			textLine.SetVerticalAlignCenter()
			textLine.SetHorizontalAlignCenter()
			if isBold:
				textLine.SetBold()
			textLine.Show()
			self.ButtonText = textLine

		self.ButtonText.SetText(text)

	def SetFormToolTipText(self, type, text, x, y):
		if not self.ToolTipText:
			toolTip = createToolTipWindowDict[type]()
			toolTip.SetParent(self)
			toolTip.SetSize(0, 0)
			toolTip.SetHorizontalAlignCenter()
			toolTip.SetOutline()
			toolTip.Hide()
			toolTip.SetPosition(x + self.GetWidth() / 2, y)
			self.ToolTipText = toolTip

		self.ToolTipText.SetText(text)

	def SAFE_SetBetterToolTip(self, tooltipText, tooltip):
		self.SetBetterToolTip(tooltipText, proxy(tooltip))

	def SetBetterToolTip(self, tooltipText, tooltip):
		self.tooltip = tooltip
		self.betterToolTipText = tooltipText

	def SetToolTipWindow(self, toolTip):
		self.ToolTipText = toolTip
		self.ToolTipText.SetParentProxy(self)

	def SetToolTipText(self, text, x=0, y=-19):
		self.SetFormToolTipText("TEXT", text, x, y)

	if constInfo.EXTRA_UI_FEATURE:
		def GetText(self):
			if not self.ButtonText:
				return ""
			return self.ButtonText.GetText()

	def AppendTextLineAllClear(self) :
		self.TextChild = []

	def SetAppendTextChangeText(self, idx, text):
		if not len(self.TextChild) :
			return

		self.TextChild[idx].SetText(text)

	def SetAppendTextColor(self, idx, color) :
		if not len(self.TextChild) :
			return

		self.TextChild[idx].SetPackedFontColor(color)

	def AppendTextLine(self, text
		, font_size = gamefont.GetCurrentFont()
		, font_color = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
		, text_sort = "center"
		, pos_x = None
		, pos_y = None) :
		textLine = TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(font_size)
		textLine.SetPackedFontColor(font_color)
		textLine.SetText(text)
		textLine.Show()

		if not pos_x and not pos_y :
			textLine.SetPosition(self.GetWidth()/2, self.GetHeight()/2)
		else :
			textLine.SetPosition(pos_x, pos_y)

		textLine.SetVerticalAlignCenter()
		if "center" == text_sort :
			textLine.SetHorizontalAlignCenter()
		elif "right" == text_sort :
			textLine.SetHorizontalAlignRight()
		elif "left" == 	text_sort :
			textLine.SetHorizontalAlignLeft()

		self.TextChild.append(textLine)

	def CallEvent(self):
		if not getattr(self, "disableClickSound", False):
			snd.PlaySound("sound/ui/click.wav")
		Window.OnMouseLeftButtonUp(self)

	def ShowToolTip(self):
		if self.ToolTipText:
			self.ToolTipText.Show()
		elif self.tooltip:
			self.tooltip.ClearToolTip()
			self.tooltip.AppendTextLine(self.betterToolTipText)
			self.tooltip.ShowToolTip()

		Window.OnMouseOverIn(self)

		showEvent = getattr(self, "showToolTipEvent", None)	# MT2009_PLUS_MONSTER_CARDS_V1
		if showEvent:
			showEvent[0](*showEvent[1])

	def HideToolTip(self):
		if self.ToolTipText:
			self.ToolTipText.Hide()
		elif self.tooltip:
			self.tooltip.HideToolTip()

		Window.OnMouseOverOut(self)

		hideEvent = getattr(self, "hideToolTipEvent", None)	# MT2009_PLUS_MONSTER_CARDS_V1
		if hideEvent:
			hideEvent[0](*hideEvent[1])

	def IsDown(self):
		return wndMgr.IsDown(self.hWnd)


# MT2009_PLUS_MONSTER_CARDS_V1 (Autor: Digi Rasta): the official client's MoveImageBox, in
# Python - an image that flies to a point of the screen (SetMovePosition takes global
# coordinates) at SetMoveSpeed pixels a frame and calls SetEndMoveEvent's function at the
# end. A plain window with the image inside, because the exe's image box never calls a
# Python OnUpdate. The Monster Card window deals its cards with it (uimonstercard.py).
class MoveImageBox(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)
		self.AddFlag("not_pick")
		self.image = ImageBox()
		self.image.SetParent(self)
		self.image.AddFlag("not_pick")
		self.image.SetPosition(0, 0)
		self.image.Show()
		self.moving = False
		self.target = (0.0, 0.0)
		self.current = (0.0, 0.0)
		self.speed = 1.0
		self.endMoveEvent = None

	def __del__(self):
		Window.__del__(self)

	def LoadImage(self, imageName):
		self.image.LoadImage(imageName)
		self.SetSize(self.image.GetWidth(), self.image.GetHeight())

	def SetEndMoveEvent(self, event):
		self.endMoveEvent = event

	def SetMoveSpeed(self, speed):
		self.speed = float(speed)

	def SetMovePosition(self, x, y):
		self.target = (float(x), float(y))

	def GetMove(self):
		return self.moving

	def MoveStart(self):
		(x, y) = self.GetGlobalPosition()
		self.current = (float(x), float(y))
		self.moving = True

	def MoveStop(self):
		self.moving = False

	def __PlaceGlobal(self, x, y):
		(gx, gy) = self.GetGlobalPosition()
		(lx, ly) = self.GetLocalPosition()
		self.SetPosition(int(x) - (gx - lx), int(y) - (gy - ly))

	def OnUpdate(self):
		if not self.moving:
			return
		(x, y) = self.current
		dx = self.target[0] - x
		dy = self.target[1] - y
		length = (dx * dx + dy * dy) ** 0.5
		if self.speed <= 0.0 or length <= self.speed:
			self.moving = False
			self.__PlaceGlobal(self.target[0], self.target[1])
			if self.endMoveEvent:
				self.endMoveEvent()
			return
		self.current = (x + dx / length * self.speed, y + dy / length * self.speed)
		self.__PlaceGlobal(self.current[0], self.current[1])

class RadioButton(Button):
	def __init__(self):
		Button.__init__(self)

	def __del__(self):
		Button.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterRadioButton(self, layer)

class RadioButtonGroup(NoWindow):
	def __init__(self):
		self.buttonGroup = []
		self.selectedBtnIdx = -1

	def __del__(self):
		for button, ue, de in self.buttonGroup:
			button.__del__()

	def Show(self):
		for (button, selectEvent, unselectEvent) in self.buttonGroup:
			button.Show()

	def Hide(self):
		for (button, selectEvent, unselectEvent) in self.buttonGroup:
			button.Hide()

	def SetText(self, idx, text):
		if idx >= len(self.buttonGroup):
			return
		(button, selectEvent, unselectEvent) = self.buttonGroup[idx]
		button.SetText(text)

	def OnClick(self, btnIdx):
		if btnIdx == self.selectedBtnIdx:
			return
		(button, selectEvent, unselectEvent) = self.buttonGroup[self.selectedBtnIdx]
		if unselectEvent:
			unselectEvent()
		button.SetUp()

		self.selectedBtnIdx = btnIdx
		(button, selectEvent, unselectEvent) = self.buttonGroup[btnIdx]
		if selectEvent:
			selectEvent()

		button.Down()

	def AddButton(self, button, selectEvent, unselectEvent):
		i = len(self.buttonGroup)
		button.SetEvent(lambda : self.OnClick(i))
		self.buttonGroup.append([button, selectEvent, unselectEvent])
		button.SetUp()

	def Create(rawButtonGroup):
		radioGroup = RadioButtonGroup()
		for (button, selectEvent, unselectEvent) in rawButtonGroup:
			radioGroup.AddButton(button, selectEvent, unselectEvent)

		radioGroup.OnClick(0)

		return radioGroup

	Create=staticmethod(Create)

class ToggleButton(Button):
	def __init__(self):
		Button.__init__(self)

		self.eventUp = None
		self.eventDown = None

		self.eventUpArgs = None
		self.eventDownArgs = None

	def __del__(self):
		Button.__del__(self)

		self.eventUp = None
		self.eventDown = None

		self.eventUpArgs = None
		self.eventDownArgs = None

	def SetToggleUpEvent(self, event, *args):
		self.eventUp = event
		self.eventUpArgs = args

	def SetToggleDownEvent(self, event, *args):
		self.eventDown = event
		self.eventDownArgs = args

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterToggleButton(self, layer)

	def OnToggleUp(self):
		if self.eventUp:
			if self.eventUpArgs:
				apply(self.eventUp, self.eventUpArgs)
			else:
				self.eventUp()

	def OnToggleDown(self):
		if self.eventDown:
			if self.eventDownArgs:
				apply(self.eventDown, self.eventDownArgs)
			else:
				self.eventDown()

class DragButton(Button):
	def __init__(self):
		Button.__init__(self)
		self.AddFlag("movable")

		self.callbackEnable = True
		self.eventMove = lambda: None

	def __del__(self):
		Button.__del__(self)

		self.eventMove = lambda: None

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterDragButton(self, layer)

	def SetMoveEvent(self, event):
		self.eventMove = event

	def SetRestrictMovementArea(self, x, y, width, height):
		wndMgr.SetRestrictMovementArea(self.hWnd, x, y, width, height)

	def TurnOnCallBack(self):
		self.callbackEnable = True

	def TurnOffCallBack(self):
		self.callbackEnable = False

	def OnMove(self):
		if self.callbackEnable:
			self.eventMove()

class NumberLine(Window):

	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

	def __del__(self):
		Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterNumberLine(self, layer)

	def SetHorizontalAlignCenter(self):
		wndMgr.SetNumberHorizontalAlignCenter(self.hWnd)

	def SetHorizontalAlignRight(self):
		wndMgr.SetNumberHorizontalAlignRight(self.hWnd)

	def SetPath(self, path):
		wndMgr.SetPath(self.hWnd, path)

	def SetNumber(self, number):
		wndMgr.SetNumber(self.hWnd, number)

###################################################################################################
## PythonScript Element
###################################################################################################

class Box(Window):

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterBox(self, layer)

	def SetColor(self, color):
		wndMgr.SetColor(self.hWnd, color)

class Bar(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

	def __del__(self):
		Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterBar(self, layer)

	def SetColor(self, color):
		wndMgr.SetColor(self.hWnd, color)

class Line(Window):

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterLine(self, layer)

	def SetColor(self, color):
		wndMgr.SetColor(self.hWnd, color)

class SlotBar(Window):

	def __init__(self):
		Window.__init__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterBar3D(self, layer)

## Same with SlotBar
class Bar3D(Window):

	def __init__(self):
		Window.__init__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterBar3D(self, layer)

	def SetColor(self, left, right, center):
		wndMgr.SetColor(self.hWnd, left, right, center)

class TextValueBar(Bar):
	def __init__(self, name, value_text, size_x, size_y):
		Bar.__init__(self)

		self.SetSize(size_x, size_y)

		title = TextLine()
		title.SetParent(self)
		title.SetWindowVerticalAlignCenter()
		title.SetVerticalAlignCenter()
		title.SetPosition(3, 0)
		title.SetText(name)
		title.Show()
		self.title = title

		value = TextLine()
		value.SetParent(self)
		value.SetHorizontalAlignRight()
		value.SetWindowHorizontalAlignRight()
		value.SetWindowVerticalAlignCenter()
		value.SetVerticalAlignCenter()
		value.SetPosition(3, 0)
		value.SetText(value_text)
		value.Show()
		self.value = value

		self.Show()

	def __del__(self):
		Bar.__del__(self)

	def SetBarBackground(self, isEven):
		self.SetColor(0x40000000 if isEven else 0x10ffffff)

	def SetTitleLabel(self, text):
		self.title.SetText(text)

	def SetValueLabel(self, text):
		self.value.SetText(text)

class SlotWindow(Window):

	SLOT_COLOR_RED = (1.0, 0, 0, 0.3)
	SLOT_COLOR_GRAY = (1.0, 1.0, 1.0, 0.3)
	SLOT_COLOR_NORMAL = (0, 0, 0, 0)

	def __init__(self):
		Window.__init__(self)

		self.StartIndex = 0

		self.eventSelectEmptySlot = None
		self.eventSelectItemSlot = None
		self.eventUnselectEmptySlot = None
		self.eventUnselectItemSlot = None
		self.eventUseSlot = None
		self.eventOverInItem = None
		self.eventOverOutItem = None
		self.eventPressedSlotButton = None

	def __del__(self):
		Window.__del__(self)

		self.eventSelectEmptySlot = None
		self.eventSelectItemSlot = None
		self.eventUnselectEmptySlot = None
		self.eventUnselectItemSlot = None
		self.eventUseSlot = None
		self.eventOverInItem = None
		self.eventOverOutItem = None
		self.eventPressedSlotButton = None

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterSlotWindow(self, layer)

	def SetSlotStyle(self, style):
		wndMgr.SetSlotStyle(self.hWnd, style)

	def HasSlot(self, slotIndex):
		return wndMgr.HasSlot(self.hWnd, slotIndex)

	def SetSlotBaseImage(self, imageFileName, r = 1.0, g = 1.0, b = 1.0, a = 1.0):
		wndMgr.SetSlotBaseImage(self.hWnd, imageFileName, r, g, b, a)

	def SetCoverButton(self,\
						slotIndex,\
						upName="d:/ymir work/ui/public/slot_cover_button_01.sub",\
						overName="d:/ymir work/ui/public/slot_cover_button_02.sub",\
						downName="d:/ymir work/ui/public/slot_cover_button_03.sub",\
						disableName="d:/ymir work/ui/public/slot_cover_button_04.sub",\
						LeftButtonEnable = False,\
						RightButtonEnable = True):
		wndMgr.SetCoverButton(self.hWnd, slotIndex, upName, overName, downName, disableName, LeftButtonEnable, RightButtonEnable)

	def EnableCoverButton(self, slotIndex):
		wndMgr.EnableCoverButton(self.hWnd, slotIndex)

	def DisableCoverButton(self, slotIndex):
		wndMgr.DisableCoverButton(self.hWnd, slotIndex)

	def SetAlwaysRenderCoverButton(self, slotIndex, bAlwaysRender = True):
		wndMgr.SetAlwaysRenderCoverButton(self.hWnd, slotIndex, bAlwaysRender)

	def AppendSlotButton(self, upName, overName, downName):
		wndMgr.AppendSlotButton(self.hWnd, upName, overName, downName)

	def ShowSlotButton(self, slotNumber):
		wndMgr.ShowSlotButton(self.hWnd, slotNumber)

	def HideAllSlotButton(self):
		wndMgr.HideAllSlotButton(self.hWnd)

	def AppendRequirementSignImage(self, filename):
		wndMgr.AppendRequirementSignImage(self.hWnd, filename)

	def ShowRequirementSign(self, slotNumber):
		wndMgr.ShowRequirementSign(self.hWnd, slotNumber)

	def HideRequirementSign(self, slotNumber):
		wndMgr.HideRequirementSign(self.hWnd, slotNumber)

	def ActivateSlot(self, slotNumber, r = 1.0, g = 1.0, b = 1.0, a = 1.0):
		wndMgr.ActivateSlot(self.hWnd, slotNumber, r, g, b, a)

	def DeactivateSlot(self, slotNumber):
		wndMgr.DeactivateSlot(self.hWnd, slotNumber)

	if app.ENABLE_HIGHLIGHT_NEW_ITEM:
		def SetSlotDiffuseColor(self, slotindex, colortype):
			wndMgr.SetSlotDiffuseColor(self.hWnd, slotindex, colortype)

	def ShowSlotBaseImage(self, slotNumber):
		wndMgr.ShowSlotBaseImage(self.hWnd, slotNumber)

	def HideSlotBaseImage(self, slotNumber):
		wndMgr.HideSlotBaseImage(self.hWnd, slotNumber)

	def SAFE_SetButtonEvent(self, button, state, event):
		if "LEFT"==button:
			if "EMPTY"==state:
				self.eventSelectEmptySlot=__mem_func__(event)
			elif "EXIST"==state:
				self.eventSelectItemSlot=__mem_func__(event)
			elif "ALWAYS"==state:
				self.eventSelectEmptySlot=__mem_func__(event)
				self.eventSelectItemSlot=__mem_func__(event)
		elif "RIGHT"==button:
			if "EMPTY"==state:
				self.eventUnselectEmptySlot=__mem_func__(event)
			elif "EXIST"==state:
				self.eventUnselectItemSlot=__mem_func__(event)
			elif "ALWAYS"==state:
				self.eventUnselectEmptySlot=__mem_func__(event)
				self.eventUnselectItemSlot=__mem_func__(event)

	def SetSelectEmptySlotEvent(self, empty):
		self.eventSelectEmptySlot = empty

	def SetSelectItemSlotEvent(self, item):
		self.eventSelectItemSlot = item

	def SetUnselectEmptySlotEvent(self, empty):
		self.eventUnselectEmptySlot = empty

	def SetUnselectItemSlotEvent(self, item):
		self.eventUnselectItemSlot = item

	def SetUseSlotEvent(self, use):
		self.eventUseSlot = use

	def SetOverInItemEvent(self, event):
		self.eventOverInItem = event

	def SetOverOutItemEvent(self, event):
		self.eventOverOutItem = event

	def SetPressedSlotButtonEvent(self, event):
		self.eventPressedSlotButton = event

	def GetSlotCount(self):
		return wndMgr.GetSlotCount(self.hWnd)

	def SetUseMode(self, flag):
		wndMgr.SetUseMode(self.hWnd, flag)

	def SetUsableItem(self, flag):
		wndMgr.SetUsableItem(self.hWnd, flag)

	## Slot
	if app.ENABLE_SLOT_WINDOW_EX:
		def IsActivatedSlot(self, slotNumber):
			return wndMgr.IsActivatedSlot(self.hWnd, slotNumber)

		def GetSlotCoolTime(self, slotIndex):
			return wndMgr.GetSlotCoolTime(self.hWnd, slotIndex)

	def SetSlotCoolTime(self, slotIndex, coolTime, elapsedTime = 0.0):
		wndMgr.SetSlotCoolTime(self.hWnd, slotIndex, coolTime, elapsedTime)
		self.EnableSlotTimeInfoText(slotIndex)

	def DisableSlot(self, slotIndex):
		wndMgr.DisableSlot(self.hWnd, slotIndex)

	def EnableSlot(self, slotIndex):
		wndMgr.EnableSlot(self.hWnd, slotIndex)

	def LockSlot(self, slotIndex):
		wndMgr.LockSlot(self.hWnd, slotIndex)

	def UnlockSlot(self, slotIndex):
		wndMgr.UnlockSlot(self.hWnd, slotIndex)

	def RefreshSlot(self):
		wndMgr.RefreshSlot(self.hWnd)

	def ClearSlot(self, slotNumber):
		wndMgr.ClearSlot(self.hWnd, slotNumber)

	def ClearAllSlot(self):
		wndMgr.ClearAllSlot(self.hWnd)

	def AppendSlot(self, index, x, y, width, height):
		wndMgr.AppendSlot(self.hWnd, index, x, y, width, height)

	def SetSlot(self, slotIndex, itemIndex, width, height, icon, diffuseColor = (1.0, 1.0, 1.0, 1.0)):
		wndMgr.SetSlot(self.hWnd, slotIndex, itemIndex, width, height, icon, diffuseColor)

	def SetSlotCount(self, slotNumber, count):
		wndMgr.SetSlotCount(self.hWnd, slotNumber, count)

	def SetSlotCountNew(self, slotNumber, grade, count):
		wndMgr.SetSlotCountNew(self.hWnd, slotNumber, grade, count)

	def SetItemSlot(self, renderingSlotNumber, ItemIndex, ItemCount = 0, diffuseColor = (1.0, 1.0, 1.0, 1.0), socket=None):
		if 0 == ItemIndex or None == ItemIndex:
			wndMgr.ClearSlot(self.hWnd, renderingSlotNumber)
			return

		item.SelectItem(ItemIndex)

		if socket is not None:
			if ItemIndex == 50300:
				skillVnum = socket[0]
				if skillVnum > 0 and skillVnum <= 6:
					item.SelectItem(50290)
				elif skillVnum <= 21:
					item.SelectItem(50291)
				elif skillVnum <= 36:
					item.SelectItem(50292)
				elif skillVnum <= 52:
					item.SelectItem(50293)
				elif skillVnum <= 66:
					item.SelectItem(50294)
				elif skillVnum <= 81:
					item.SelectItem(50295)
				elif skillVnum <= 96:
					item.SelectItem(50296)
				elif skillVnum <= 111:
					item.SelectItem(50297)

		itemIcon = item.GetIconImage()

		item.SelectItem(ItemIndex)
		(width, height) = item.GetItemSize()

		wndMgr.SetSlot(self.hWnd, renderingSlotNumber, ItemIndex, width, height, itemIcon, diffuseColor)
		wndMgr.SetSlotCount(self.hWnd, renderingSlotNumber, ItemCount)

	def SetSkillSlot(self, renderingSlotNumber, skillIndex, skillLevel):

		skillIcon = skill.GetIconImage(skillIndex)

		if 0 == skillIcon:
			wndMgr.ClearSlot(self.hWnd, renderingSlotNumber)
			return

		wndMgr.SetSlot(self.hWnd, renderingSlotNumber, skillIndex, 1, 1, skillIcon)
		wndMgr.SetSlotCount(self.hWnd, renderingSlotNumber, skillLevel)

	def SetSkillSlotNew(self, renderingSlotNumber, skillIndex, skillGrade, skillLevel):

		skillIcon = skill.GetIconImageNew(skillIndex, skillGrade)

		if 0 == skillIcon:
			wndMgr.ClearSlot(self.hWnd, renderingSlotNumber)
			return

		wndMgr.SetSlot(self.hWnd, renderingSlotNumber, skillIndex, 1, 1, skillIcon)

	def SetEmotionSlot(self, renderingSlotNumber, emotionIndex):
		import player
		icon = player.GetEmotionIconImage(emotionIndex)

		if 0 == icon:
			wndMgr.ClearSlot(self.hWnd, renderingSlotNumber)
			return

		wndMgr.SetSlot(self.hWnd, renderingSlotNumber, emotionIndex, 1, 1, icon)

	def SetSlotMaskColor(self, slotIndex, colorTuple):
		self.SetSlotMaskColorRaw(slotIndex, *colorTuple)

	def SetSlotMaskColorRaw(self, slotIndex, r, g, b, a=0.3):
		wndMgr.SetSlotMaskColor(self.hWnd, slotIndex, r, g, b, a)

	def SetSlotMaskNormalColor(self, slotIndex):
		self.SetSlotMaskColor(slotIndex, self.SLOT_COLOR_NORMAL)

	def SetCanMouseEventSlot(self, slotIndex):
		wndMgr.SetCanMouseEventSlot(self.hWnd, slotIndex)
		self.SetSlotMaskColor(slotIndex, self.SLOT_COLOR_NORMAL)

	def SetCantMouseEventSlot(self, slotIndex):
		wndMgr.SetCantMouseEventSlot(self.hWnd, slotIndex)
		self.SetSlotMaskColor(slotIndex, self.SLOT_COLOR_RED)

	def SetUsableSlot(self, slotIndex):
		wndMgr.SetUsableSlotOnTopWnd(self.hWnd, slotIndex)
		wndMgr.SetCanMouseEventSlot(self.hWnd, slotIndex)
		self.SetSlotMaskColor(slotIndex, self.SLOT_COLOR_NORMAL)

	def SetUnusableSlot(self, slotIndex):
		wndMgr.SetUnusableSlotOnTopWnd(self.hWnd, slotIndex)
		wndMgr.SetCantMouseEventSlot(self.hWnd, slotIndex)
		self.SetSlotMaskColor(slotIndex, self.SLOT_COLOR_GRAY)

	def SetSlotCoolTimeInverse(self, slotIndex, coolTime, elapsedTime=0.0):
		wndMgr.SetSlotCoolTimeInverse(self.hWnd, slotIndex, coolTime, elapsedTime)

	def SetSlotCoolTimeColor(self, slotIndex, r, g, b, a):
		wndMgr.SetSlotCoolTimeColor(self.hWnd, slotIndex, r, g, b, a)

	def EnableSlotTimeInfoText(self, slotindex):
		wndMgr.EnableSlotTimeInfoText(self.hWnd, slotindex)

	def DeactivateSlotTimeInfoText(self, slotindex):
		wndMgr.DeactivateSlotTimeInfoText(self.hWnd, slotindex)

	## Event
	def OnSelectEmptySlot(self, slotNumber):
		if self.eventSelectEmptySlot:
			self.eventSelectEmptySlot(slotNumber)

	def OnSelectItemSlot(self, slotNumber):
		if self.eventSelectItemSlot:
			self.eventSelectItemSlot(slotNumber)

	def OnUnselectEmptySlot(self, slotNumber):
		if self.eventUnselectEmptySlot:
			self.eventUnselectEmptySlot(slotNumber)

	def OnUnselectItemSlot(self, slotNumber):
		if self.eventUnselectItemSlot:
			self.eventUnselectItemSlot(slotNumber)

	def OnUseSlot(self, slotNumber):
		if self.eventUseSlot:
			self.eventUseSlot(slotNumber)

	def OnOverInItem(self, slotNumber):
		if self.eventOverInItem:
			self.eventOverInItem(slotNumber)

	def OnOverOutItem(self):
		if self.eventOverOutItem:
			self.eventOverOutItem()

	def OnPressedSlotButton(self, slotNumber):
		if self.eventPressedSlotButton:
			self.eventPressedSlotButton(slotNumber)

	def GetSlotBindedItemIndex(self, slotNumber):
		return wndMgr.GetSlotBindedItemIndex(self.hWnd, slotNumber)

	def GetStartIndex(self):
		return 0

class GridSlotWindow(SlotWindow):

	def __init__(self):
		SlotWindow.__init__(self)

		self.startIndex = 0
		self.x_width = 0
		self.y_height = 0
		self.checkSlotEvent = None

	def __del__(self):
		SlotWindow.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterGridSlotWindow(self, layer)

	def ArrangeSlot(self, StartIndex, xCount, yCount, xSize, ySize, xBlank, yBlank):

		self.startIndex = StartIndex

		wndMgr.ArrangeSlot(self.hWnd, StartIndex, xCount, yCount, xSize, ySize, xBlank, yBlank)
		self.startIndex = StartIndex

		self.x_width = xCount
		self.y_height = yCount

	def GetStartIndex(self):
		return self.startIndex

	def FindEmptySlot(self, neededItemSize=1, startIndex=0):
		busySlots = []
		local_max_slot_num = self.x_width * self.y_height
		for y in range(self.y_height):
			for x in range(self.x_width):
				localSlotNum = y * self.x_width + x
				slotNum = startIndex + localSlotNum
				vnum = self.GetSlotBindedItemIndex(localSlotNum)
				itemSize = 0
				if vnum > 0:
					item.SelectItem(vnum)
					itemSize = item.GetItemSize()[1]
				if itemSize > 1:
					for i in range(1, itemSize):
						busySlotNum = slotNum + i * self.x_width
						busySlots.append(busySlotNum)
				if vnum <= 0 and not slotNum in busySlots:
					if neededItemSize > 1:
						searchForAnotherSlot = False
						for i in range(1, neededItemSize):
							localPotentialSlot = localSlotNum + i * self.x_width
							if self.GetSlotBindedItemIndex(localPotentialSlot) > 0:
								searchForAnotherSlot = True
								break
							if localPotentialSlot >= local_max_slot_num:
								return -1

						if searchForAnotherSlot:
							continue

					if self.checkSlotEvent is not None and not self.checkSlotEvent(slotNum, neededItemSize):
						continue
					return slotNum
		return -1

class TitleBar(Window):

	BLOCK_WIDTH = 32
	BLOCK_HEIGHT = 23

	def __init__(self):
		Window.__init__(self)
		self.AddFlag("attach")

	def __del__(self):
		Window.__del__(self)

	def MakeTitleBar(self, width, color):


		width = max(64, width)

		imgLeft = ImageBox()
		imgCenter = ExpandedImageBox()
		imgRight = ImageBox()
		imgLeft.AddFlag("not_pick")
		imgCenter.AddFlag("not_pick")
		imgRight.AddFlag("not_pick")
		imgLeft.SetParent(self)
		imgCenter.SetParent(self)
		imgRight.SetParent(self)

		if localeInfo.IsARABIC():
			imgLeft.LoadImage("locale/ae/ui/pattern/titlebar_left.tga")
			imgCenter.LoadImage("locale/ae/ui/pattern/titlebar_center.tga")
			imgRight.LoadImage("locale/ae/ui/pattern/titlebar_right.tga")
		else:
			imgLeft.LoadImage("d:/ymir work/ui/pattern/titlebar_left.tga")
			imgCenter.LoadImage("d:/ymir work/ui/pattern/titlebar_center.tga")
			imgRight.LoadImage("d:/ymir work/ui/pattern/titlebar_right.tga")

		imgLeft.Show()
		imgCenter.Show()
		imgRight.Show()

		btnClose = Button()
		btnClose.SetParent(self)
		btnClose.SetUpVisual("d:/ymir work/ui/public/close_button_01.sub")
		btnClose.SetOverVisual("d:/ymir work/ui/public/close_button_02.sub")
		btnClose.SetDownVisual("d:/ymir work/ui/public/close_button_03.sub")
		btnClose.SetToolTipText(localeInfo.UI_CLOSE, 0, -23)
		btnClose.Show()

		self.imgLeft = imgLeft
		self.imgCenter = imgCenter
		self.imgRight = imgRight
		self.btnClose = btnClose

		self.SetWidth(width)

	def SetWidth(self, width):
		self.imgCenter.SetRenderingRect(0.0, 0.0, float((width - self.BLOCK_WIDTH*2) - self.BLOCK_WIDTH) / self.BLOCK_WIDTH, 0.0)
		self.imgCenter.SetPosition(self.BLOCK_WIDTH, 0)
		self.imgRight.SetPosition(width - self.BLOCK_WIDTH, 0)

		if localeInfo.IsARABIC():
			self.btnClose.SetPosition(3, 3)
		else:
			self.btnClose.SetPosition(width - self.btnClose.GetWidth() - 3, 3)

		self.SetSize(width, self.BLOCK_HEIGHT)

	def SetCloseEvent(self, event):
		self.btnClose.SetEvent(event)

	def CloseButtonHide(self) :
		if localeInfo.IsARABIC():
			self.imgLeft.LoadImage("d:/ymir work/ui/pattern/titlebar_left.tga")
			self.btnClose.Hide()
		else:
			self.imgRight.LoadImage("d:/ymir work/ui/pattern/titlebar_right_02.tga")
			self.btnClose.Hide()

class HorizontalBar(Window):

	HORIZONTAL_BAR_FILE_DATA = {
		"default": {
			"width": 32,
			"height": 17,
			"files": (
				"d:/ymir work/ui/pattern/horizontalbar_left.tga",
				"d:/ymir work/ui/pattern/horizontalbar_center.tga",
				"d:/ymir work/ui/pattern/horizontalbar_right.tga",
			),
		},

		"c": {
			"width": 21,
			"height": 21,
			"files": (
				"d:/ymir work/ui/pattern/border_c_left.tga",
				"d:/ymir work/ui/pattern/border_c_middle.tga",
				"d:/ymir work/ui/pattern/border_c_right.tga",
			),
		},

		"light": {
			"width": 32,
			"height": 17,
			"files": (
				"d:/ymir work/ui/pattern/horizontalbar_light_left.tga",
				"d:/ymir work/ui/pattern/horizontalbar_light_center.tga",
				"d:/ymir work/ui/pattern/horizontalbar_light_right.tga",
			),
		},
	}

	def __init__(self):
		Window.__init__(self)
		self.AddFlag("attach")

	def __del__(self):
		Window.__del__(self)

	def GetSkinData(self, skin):
		return self.HORIZONTAL_BAR_FILE_DATA[skin]

	def Create(self, width, skin="default"):
		skinData = self.GetSkinData(skin)

		imgLeft = ImageBox()
		imgLeft.SetParent(self)
		imgLeft.AddFlag("not_pick")
		imgLeft.LoadImage(skinData["files"][0])
		imgLeft.Show()

		imgCenter = ExpandedImageBox()
		imgCenter.SetParent(self)
		imgCenter.AddFlag("not_pick")
		imgCenter.LoadImage(skinData["files"][1])
		imgCenter.Show()

		imgRight = ImageBox()
		imgRight.SetParent(self)
		imgRight.AddFlag("not_pick")
		imgRight.LoadImage(skinData["files"][2])
		imgRight.Show()

		self.imgLeft = imgLeft
		self.imgCenter = imgCenter
		self.imgRight = imgRight

		self.SetWidth(width, skinData["width"], skinData["height"])

	def SetWidth(self, width, block_width, block_height):
		self.imgCenter.SetRenderingRect(0.0, 0.0,
										float((width - block_width * 2) - block_width) / block_width,
										0.0)
		self.imgCenter.SetPosition(block_width, 0)
		self.imgRight.SetPosition(width - block_width, 0)
		self.SetSize(width, block_height)


class HorizontalBarTitle(HorizontalBar):
	def __init__(self, width, title_text):
		HorizontalBar.__init__(self)
		self.Create(width)

		title = TextLine()
		title.SetParent(self)
		title.SetBold()
		title.SetPackedFontColor(0xffe3cca1)
		title.SetVerticalAlignCenter()
		title.SetWindowVerticalAlignCenter()
		title.SetPosition(4, 0)
		title.SetText(title_text)
		title.Show()
		self.title = title

		self.Show()

	def __del__(self):
		HorizontalBar.__del__(self)


class Gauge(Window):

	SLOT_WIDTH = 16
	SLOT_HEIGHT = 7

	GAUGE_TEMPORARY_PLACE = 12
	GAUGE_WIDTH = 16

	def __init__(self):
		Window.__init__(self)
		self.width = 0
	def __del__(self):
		Window.__del__(self)

	def MakeGauge(self, width, color):

		self.width = max(48, width)

		imgSlotLeft = ImageBox()
		imgSlotLeft.SetParent(self)
		imgSlotLeft.LoadImage("d:/ymir work/ui/pattern/gauge_slot_left.tga")
		imgSlotLeft.Show()

		imgSlotRight = ImageBox()
		imgSlotRight.SetParent(self)
		imgSlotRight.LoadImage("d:/ymir work/ui/pattern/gauge_slot_right.tga")
		imgSlotRight.Show()
		imgSlotRight.SetPosition(width - self.SLOT_WIDTH, 0)

		imgSlotCenter = ExpandedImageBox()
		imgSlotCenter.SetParent(self)
		imgSlotCenter.LoadImage("d:/ymir work/ui/pattern/gauge_slot_center.tga")
		imgSlotCenter.Show()
		imgSlotCenter.SetRenderingRect(0.0, 0.0, float((width - self.SLOT_WIDTH*2) - self.SLOT_WIDTH) / self.SLOT_WIDTH, 0.0)
		imgSlotCenter.SetPosition(self.SLOT_WIDTH, 0)

		imgGauge = ExpandedImageBox()
		imgGauge.SetParent(self)
		imgGauge.LoadImage("d:/ymir work/ui/pattern/gauge_" + color + ".tga")
		imgGauge.Show()
		imgGauge.SetRenderingRect(0.0, 0.0, 0.0, 0.0)
		imgGauge.SetPosition(self.GAUGE_TEMPORARY_PLACE, 0)

		imgSlotLeft.AddFlag("attach")
		imgSlotCenter.AddFlag("attach")
		imgSlotRight.AddFlag("attach")

		self.imgLeft = imgSlotLeft
		self.imgCenter = imgSlotCenter
		self.imgRight = imgSlotRight
		self.imgGauge = imgGauge

		self.SetSize(width, self.SLOT_HEIGHT)

	def SetPercentage(self, curValue, maxValue):
		if math.isnan(curValue) or math.isnan(maxValue):
			return 0.0

		# PERCENTAGE_MAX_VALUE_ZERO_DIVISION_ERROR
		if maxValue > 0.0:
			percentage = min(1.0, float(curValue)/float(maxValue))
		else:
			percentage = 0.0
		# END_OF_PERCENTAGE_MAX_VALUE_ZERO_DIVISION_ERROR

		gaugeSize = -1.0 + float(self.width - self.GAUGE_TEMPORARY_PLACE*2) * percentage / self.GAUGE_WIDTH
		self.imgGauge.SetRenderingRect(0.0, 0.0, gaugeSize, 0.0)

class Board(Window):

	LT = 0
	LB = 1
	RT = 2
	RB = 3
	L = 0
	R = 1
	T = 2
	B = 3

	BOARD_FILE_DATA = {
		"default": {
			"corner_x": 32,
			"corner_y": 32,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/Board_Corner_LeftTop.tga",
				"d:/ymir work/ui/pattern/Board_Corner_LeftBottom.tga",
				"d:/ymir work/ui/pattern/Board_Corner_RightTop.tga",
				"d:/ymir work/ui/pattern/Board_Corner_RightBottom.tga",
			),

			"line_x": 128,
			"line_y": 128,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/Board_Line_Left.tga",
				"d:/ymir work/ui/pattern/Board_Line_Right.tga",
				"d:/ymir work/ui/pattern/Board_Line_Top.tga",
				"d:/ymir work/ui/pattern/Board_Line_Bottom.tga",
			),

			"board_base": "d:/ymir work/ui/pattern/board_base.tga",
		},

		"a": {
			"corner_x": 16,
			"corner_y": 16,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/border_a_left_top.tga",
				"d:/ymir work/ui/pattern/border_a_left_bottom.tga",
				"d:/ymir work/ui/pattern/border_a_right_top.tga",
				"d:/ymir work/ui/pattern/border_a_right_bottom.tga",
			),

			"line_x": 16,
			"line_y": 16,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/border_a_left.tga",
				"d:/ymir work/ui/pattern/border_a_right.tga",
				"d:/ymir work/ui/pattern/border_a_top.tga",
				"d:/ymir work/ui/pattern/border_a_bottom.tga",
			),

			"board_base": "d:/ymir work/ui/pattern/border_a_center.tga",
		},

		"b": {
			"corner_x": 16,
			"corner_y": 16,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/border_b_left_top.tga",
				"d:/ymir work/ui/pattern/border_b_left_bottom.tga",
				"d:/ymir work/ui/pattern/border_b_right_top.tga",
				"d:/ymir work/ui/pattern/border_b_right_bottom.tga",
			),

			"line_x": 16,
			"line_y": 16,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/border_b_left.tga",
				"d:/ymir work/ui/pattern/border_b_right.tga",
				"d:/ymir work/ui/pattern/border_b_top.tga",
				"d:/ymir work/ui/pattern/border_b_bottom.tga",
			),

			"board_base": "d:/ymir work/ui/pattern/border_b_center.tga",
		},

		"thin_gold": {
			"corner_x": 16,
			"corner_y": 16,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_corner_lefttop_gold.tga",
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_corner_leftbottom_gold.tga",
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_corner_righttop_gold.tga",
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_corner_rightbottom_gold.tga",
			),

			"line_x": 16,
			"line_y": 16,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_line_left_gold.tga",
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_line_right_gold.tga",
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_line_top_gold.tga",
				"d:/ymir work/ui/pattern/thinboardgold/thinboard_line_bottom_gold.tga",
			),

			"board_base": "d:/ymir work/ui/pattern/thinboardgold/thinboard_bg_gold.tga",
		},

		"green_bar": {
			"corner_x": 5,
			"corner_y": 5,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/greenbar/green_bar_corner_lefttop.tga",
				"d:/ymir work/ui/pattern/greenbar/green_bar_corner_leftbottom.tga",
				"d:/ymir work/ui/pattern/greenbar/green_bar_corner_righttop.tga",
				"d:/ymir work/ui/pattern/greenbar/green_bar_corner_rightbottom.tga",
			),

			"line_x": 5,
			"line_y": 5,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/greenbar/green_bar_line_left.tga",
				"d:/ymir work/ui/pattern/greenbar/green_bar_line_right.tga",
				"d:/ymir work/ui/pattern/greenbar/green_bar_line_top.tga",
				"d:/ymir work/ui/pattern/greenbar/green_bar_line_bottom.tga",
			),

			"board_base": "d:/ymir work/ui/pattern/greenbar/green_bar_base.tga",
		},

		"ime": {
			"corner_x": 4,
			"corner_y": 4,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/corner_lefttop.tga",
				"d:/ymir work/ui/pattern/corner_leftbottom.tga",
				"d:/ymir work/ui/pattern/corner_righttop.tga",
				"d:/ymir work/ui/pattern/corner_rightbottom.tga",
			),

			"line_x": 16,
			"line_y": 4,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/bar_left.tga",
				"d:/ymir work/ui/pattern/bar_right.tga",
				"d:/ymir work/ui/pattern/bar_top.tga",
				"d:/ymir work/ui/pattern/bar_bottom.tga",
			),

			"board_base": "d:/ymir work/ui/pattern/board_base.tga",
		}
	}

	def __init__(self, createBoard=True):
		Window.__init__(self)
		self.skin = "default"
		if createBoard:
			self.MakeBoard()
			self.MakeBase()

	def SetAlpha(self, alpha):
		for corner in self.Corners:
			corner.SetAlpha(alpha)

		for line in self.Lines:
			line.SetAlpha(alpha)

		self.Base.SetAlpha(alpha)

	def MakeBoard(self, skin="default"):
		self.skin = skin
		boardImageData = self.GetBoardImageData()

		self.Corners = []
		for fileName in boardImageData["CornerFileNames"]:
			Corner = ExpandedImageBox()
			Corner.AddFlag("not_pick")
			Corner.LoadImage(fileName)
			Corner.SetParent(self)
			Corner.SetPosition(1, 1)
			Corner.Show()
			self.Corners.append(Corner)

		self.Lines = []
		for fileName in boardImageData["LineFileNames"]:
			Line = ExpandedImageBox()
			Line.AddFlag("not_pick")
			Line.LoadImage(fileName)
			Line.SetParent(self)
			Line.SetPosition(1, 0)
			Line.Show()
			self.Lines.append(Line)

		self.Lines[self.L].SetPosition(1, boardImageData["corner_y"])
		self.Lines[self.T].SetPosition(boardImageData["corner_x"], 1)

	def GetBoardImageData(self):
		return self.BOARD_FILE_DATA[self.skin]

	def MakeBase(self):
		boardImageData = self.GetBoardImageData()

		self.Base = ExpandedImageBox()
		self.Base.AddFlag("not_pick")
		self.Base.LoadImage(boardImageData["board_base"])
		self.Base.SetParent(self)
		self.Base.SetPosition(boardImageData["corner_x"], boardImageData["corner_y"])
		self.Base.Show()

	def __del__(self):
		Window.__del__(self)

	def SetSize(self, width, height):
		boardImageData = self.GetBoardImageData()

		CORNER_WIDTH = boardImageData["corner_x"]
		CORNER_HEIGHT = boardImageData["corner_y"]
		LINE_HEIGHT = boardImageData["line_x"]
		LINE_WIDTH = boardImageData["line_y"]

		width = max(CORNER_WIDTH * 2, width)
		height = max(CORNER_HEIGHT * 2, height)
		Window.SetSize(self, width, height)

		self.Corners[self.LB].SetPosition(1, height - CORNER_HEIGHT-1)
		self.Corners[self.RT].SetPosition(width - CORNER_WIDTH-1, 1)
		self.Corners[self.RB].SetPosition(width - CORNER_WIDTH-1, height - CORNER_HEIGHT-1)
		self.Lines[self.R].SetPosition(width - CORNER_WIDTH-1, CORNER_HEIGHT)
		self.Lines[self.B].SetPosition(CORNER_HEIGHT, height - CORNER_HEIGHT-1)

		verticalShowingPercentage = float((height - CORNER_HEIGHT * 2) - LINE_HEIGHT) / LINE_HEIGHT
		horizontalShowingPercentage = float((width - CORNER_WIDTH * 2) - LINE_WIDTH) / LINE_WIDTH
		self.Lines[self.L].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.R].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.T].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		self.Lines[self.B].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)

		if self.Base:
			self.Base.SetRenderingRect(0, 0, horizontalShowingPercentage, verticalShowingPercentage)

class BoardWithTitleBar(Board):
	def __init__(self):
		Board.__init__(self)

		titleBar = TitleBar()
		titleBar.SetParent(self)
		titleBar.MakeTitleBar(0, "red")
		titleBar.SetPosition(8, 7)
		titleBar.Show()

		titleName = TextLine()
		titleName.SetParent(titleBar)
		titleName.SetPosition(0, 4)
		titleName.SetWindowHorizontalAlignCenter()
		titleName.SetHorizontalAlignCenter()
		titleName.Show()

		self.titleBar = titleBar
		self.titleName = titleName

		self.SetCloseEvent(self.Hide)

	def __del__(self):
		Board.__del__(self)
		self.titleBar = None
		self.titleName = None

	def SetSize(self, width, height):
		self.titleBar.SetWidth(width - 15)
		#self.pickRestrictWindow.SetSize(width, height - 30)
		Board.SetSize(self, width, height)
		self.titleName.UpdateRect()

	def SetTitleColor(self, color):
		self.titleName.SetPackedFontColor(color)

	def SetTitleName(self, name):
		self.titleName.SetText(name)

	def SetCloseEvent(self, event):
		self.titleBar.SetCloseEvent(event)

class ThinBoard(Window):
	LT = 0
	LB = 1
	RT = 2
	RB = 3
	L = 0
	R = 1
	T = 2
	B = 3

	THINBOARD_FILE_DATA = {
		"default": {
			"corner_x": 16,
			"corner_y": 16,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/thinboard_corner_lefttop.tga",
				"d:/ymir work/ui/pattern/thinboard_corner_leftbottom.tga",
				"d:/ymir work/ui/pattern/thinboard_corner_righttop.tga",
				"d:/ymir work/ui/pattern/thinboard_corner_rightbottom.tga",
			),

			"line_x": 16,
			"line_y": 16,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/thinboard_line_left.tga",
				"d:/ymir work/ui/pattern/thinboard_line_right.tga",
				"d:/ymir work/ui/pattern/thinboard_line_top.tga",
				"d:/ymir work/ui/pattern/thinboard_line_bottom.tga",
			),

			"board_color": grp.GenerateColor(0.0, 0.0, 0.0, 0.51),
		},

		"goldtint": {
			"corner_x": 16,
			"corner_y": 16,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_corner_lefttop.png",
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_corner_leftbottom.png",
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_corner_righttop.png",
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_corner_rightbottom.png",
			),

			"line_x": 16,
			"line_y": 16,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_line_left.png",
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_line_right.png",
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_line_top.png",
				"d:/ymir work/ui/pattern/thinboard_goldtint/thinboard_line_bottom.png",
			),

			"board_color": grp.GenerateColor(0.0, 0.0, 0.0, 0.51),
		},

		"circle": {
			"corner_x": 4,
			"corner_y": 4,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_corner_lefttop_circle.tga",
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_corner_leftbottom_circle.tga",
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_corner_righttop_circle.tga",
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_corner_rightbottom_circle.tga",
			),

			"line_x": 4,
			"line_y": 4,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_line_left_circle.tga",
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_line_right_circle.tga",
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_line_top_circle.tga",
				"d:/ymir work/ui/pattern/thinboardcircle/thinboard_line_bottom_circle.tga",
			),

			"board_color": grp.GenerateColor(0.0314, 0.0314, 0.0314, 1.0),
		},

		"b": {
			"corner_x": 16,
			"corner_y": 16,
			"CornerFileNames": (
				"d:/ymir work/ui/pattern/thinboardb_corner_lefttop.tga",
				"d:/ymir work/ui/pattern/thinboardb_corner_leftbottom.tga",
				"d:/ymir work/ui/pattern/thinboardb_corner_righttop.tga",
				"d:/ymir work/ui/pattern/thinboardb_corner_rightbottom.tga",
			),

			"line_x": 16,
			"line_y": 16,
			"LineFileNames": (
				"d:/ymir work/ui/pattern/thinboardb_line_left.tga",
				"d:/ymir work/ui/pattern/thinboardb_line_right.tga",
				"d:/ymir work/ui/pattern/thinboardb_line_top.tga",
				"d:/ymir work/ui/pattern/thinboardb_line_bottom.tga",
			),

			"board_color": grp.GenerateColor(0.0, 0.0, 0.0, 0.71),
		},
	}

	def __init__(self, layer="UI", createBoard=True):
		Window.__init__(self, layer)
		self.skin = "default"
		if createBoard:
			self.MakeThinboard()

	def __del__(self):
		Window.__del__(self)

	def MakeThinboard(self, skin="default"):
		self.skin = skin
		thinBoardImageData = self.GetThinBoardImageData()

		self.Corners = []
		for fileName in thinBoardImageData["CornerFileNames"]:
			Corner = ExpandedImageBox()
			Corner.AddFlag("attach")
			Corner.AddFlag("not_pick")
			Corner.LoadImage(fileName)
			Corner.SetParent(self)
			Corner.SetPosition(0, 0)
			Corner.Show()
			self.Corners.append(Corner)

		self.Lines = []
		for fileName in thinBoardImageData["LineFileNames"]:
			Line = ExpandedImageBox()
			Line.AddFlag("attach")
			Line.AddFlag("not_pick")
			Line.LoadImage(fileName)
			Line.SetParent(self)
			Line.SetPosition(0, 0)
			Line.Show()
			self.Lines.append(Line)

		Base = Bar()
		Base.SetParent(self)
		Base.AddFlag("attach")
		Base.AddFlag("not_pick")
		Base.SetPosition(thinBoardImageData["corner_x"], thinBoardImageData["corner_y"])
		Base.SetColor(thinBoardImageData["board_color"])
		Base.Show()
		self.Base = Base

		self.Lines[self.L].SetPosition(0, thinBoardImageData["corner_y"])
		self.Lines[self.T].SetPosition(thinBoardImageData["corner_x"], 0)

	def SetSkin(self, skinName):
		self.skin = skinName
		thinBoardImageData = self.GetThinBoardImageData()

		for i in xrange(len(self.Corners)):
			corner = self.Corners[i]
			fileName = thinBoardImageData["CornerFileNames"][i]
			corner.LoadImage(fileName)

		for i in xrange(len(self.Lines)):
			line = self.Lines[i]
			fileName = thinBoardImageData["LineFileNames"][i]
			line.LoadImage(fileName)

		self.Base.SetPosition(thinBoardImageData["corner_x"], thinBoardImageData["corner_y"])
		self.Base.SetColor(thinBoardImageData["board_color"])
		self.SetSize(self.GetWidth(), self.GetHeight())

	def GetThinBoardImageData(self):
		return self.THINBOARD_FILE_DATA[self.skin]

	def SetSize(self, width, height):
		thinBoardImageData = self.GetThinBoardImageData()
		CORNER_WIDTH = thinBoardImageData["corner_x"]
		CORNER_HEIGHT = thinBoardImageData["corner_y"]
		LINE_WIDTH = thinBoardImageData["line_x"]
		LINE_HEIGHT = thinBoardImageData["line_y"]

		width = max(CORNER_WIDTH * 2, width)
		height = max(CORNER_HEIGHT * 2, height)
		Window.SetSize(self, width, height)

		self.Corners[self.LB].SetPosition(0, height - CORNER_HEIGHT)
		self.Corners[self.RT].SetPosition(width - CORNER_WIDTH, 0)
		self.Corners[self.RB].SetPosition(width - CORNER_WIDTH, height - CORNER_HEIGHT)
		self.Lines[self.R].SetPosition(width - CORNER_WIDTH, CORNER_HEIGHT)
		self.Lines[self.B].SetPosition(CORNER_HEIGHT, height - CORNER_HEIGHT)

		verticalShowingPercentage = float((height - CORNER_HEIGHT * 2) - LINE_HEIGHT) / LINE_HEIGHT
		horizontalShowingPercentage = float((width - CORNER_WIDTH * 2) - LINE_WIDTH) / LINE_WIDTH
		self.Lines[self.L].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.R].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.T].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		self.Lines[self.B].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		self.Base.SetSize(width - CORNER_WIDTH * 2, height - CORNER_HEIGHT * 2)

	def ShowInternal(self):
		self.Base.Show()
		for wnd in self.Lines:
			wnd.Show()
		for wnd in self.Corners:
			wnd.Show()

	def HideInternal(self):
		self.Base.Hide()
		for wnd in self.Lines:
			wnd.Hide()
		for wnd in self.Corners:
			wnd.Hide()

class ThinBoardCircle(Window):
	CORNER_WIDTH = 4
	CORNER_HEIGHT = 4
	LINE_WIDTH = 4
	LINE_HEIGHT = 4
	BOARD_COLOR = grp.GenerateColor(0.0, 0.0, 0.0, 1.0)

	LT = 0
	LB = 1
	RT = 2
	RB = 3
	L = 0
	R = 1
	T = 2
	B = 3

	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)

		CornerFileNames = [ "d:/ymir work/ui/pattern/thinboardcircle/ThinBoard_Corner_"+dir+"_Circle.tga" for dir in ["LeftTop","LeftBottom","RightTop","RightBottom"] ]
		LineFileNames = [ "d:/ymir work/ui/pattern/thinboardcircle/ThinBoard_Line_"+dir+"_Circle.tga" for dir in ["Left","Right","Top","Bottom"] ]

		self.Corners = []
		for fileName in CornerFileNames:
			Corner = ExpandedImageBox()
			Corner.AddFlag("attach")
			Corner.AddFlag("not_pick")
			Corner.LoadImage(fileName)
			Corner.SetParent(self)
			Corner.SetPosition(0, 0)
			Corner.Show()
			self.Corners.append(Corner)

		self.Lines = []
		for fileName in LineFileNames:
			Line = ExpandedImageBox()
			Line.AddFlag("attach")
			Line.AddFlag("not_pick")
			Line.LoadImage(fileName)
			Line.SetParent(self)
			Line.SetPosition(0, 0)
			Line.Show()
			self.Lines.append(Line)

		Base = Bar()
		Base.SetParent(self)
		Base.AddFlag("attach")
		Base.AddFlag("not_pick")
		Base.SetPosition(self.CORNER_WIDTH, self.CORNER_HEIGHT)
		Base.SetColor(self.BOARD_COLOR)
		Base.Show()
		self.Base = Base

		self.ButtonText = None
		self.BonusId = 0

		self.Lines[self.L].SetPosition(0, self.CORNER_HEIGHT)
		self.Lines[self.T].SetPosition(self.CORNER_WIDTH, 0)

	def __del__(self):
		Window.__del__(self)

	def SetSize(self, width, height):

		width = max(self.CORNER_WIDTH*2, width)
		height = max(self.CORNER_HEIGHT*2, height)
		Window.SetSize(self, width, height)

		self.Corners[self.LB].SetPosition(0, height - self.CORNER_HEIGHT)
		self.Corners[self.RT].SetPosition(width - self.CORNER_WIDTH, 0)
		self.Corners[self.RB].SetPosition(width - self.CORNER_WIDTH, height - self.CORNER_HEIGHT)
		self.Lines[self.R].SetPosition(width - self.CORNER_WIDTH, self.CORNER_HEIGHT)
		self.Lines[self.B].SetPosition(self.CORNER_HEIGHT, height - self.CORNER_HEIGHT)

		verticalShowingPercentage = float((height - self.CORNER_HEIGHT*2) - self.LINE_HEIGHT) / self.LINE_HEIGHT
		horizontalShowingPercentage = float((width - self.CORNER_WIDTH*2) - self.LINE_WIDTH) / self.LINE_WIDTH
		self.Lines[self.L].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.R].SetRenderingRect(0, 0, 0, verticalShowingPercentage)
		self.Lines[self.T].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		self.Lines[self.B].SetRenderingRect(0, 0, horizontalShowingPercentage, 0)
		self.Base.SetSize(width - self.CORNER_WIDTH*2, height - self.CORNER_HEIGHT*2)

	def SetText(self, text):
		if not self.ButtonText:
			textLine = TextLine()
			textLine.SetParent(self)
			textLine.SetPosition(self.GetWidth()/2, self.GetHeight()/2)
			textLine.SetVerticalAlignCenter()
			textLine.SetHorizontalAlignCenter()
			textLine.Show()
			self.ButtonText = textLine

		self.ButtonText.SetText(text)

	def GetText(self):
		if not self.ButtonText:
			return ""
		return self.ButtonText.GetText()

	def SetBonusId(self, bnsId):
		self.BonusId = bnsId

	def GetBonusId(self):
		if self.BonusId != 0:
			return self.BonusId

	def ShowInternal(self):
		self.Base.Show()
		for wnd in self.Lines:
			wnd.Show()
		for wnd in self.Corners:
			wnd.Show()

	def HideInternal(self):
		self.Base.Hide()
		for wnd in self.Lines:
			wnd.Hide()
		for wnd in self.Corners:
			wnd.Hide()

class ScrollBar(Window):

	SCROLLBAR_WIDTH = 17
	SCROLLBAR_MIDDLE_HEIGHT = 9
	SCROLLBAR_BUTTON_WIDTH = 17
	SCROLLBAR_BUTTON_HEIGHT = 17
	MIDDLE_BAR_POS = 5
	MIDDLE_BAR_UPPER_PLACE = 3
	MIDDLE_BAR_DOWNER_PLACE = 4
	TEMP_SPACE = MIDDLE_BAR_UPPER_PLACE + MIDDLE_BAR_DOWNER_PLACE

	class MiddleBar(DragButton):
		top = None
		bottom = None
		middle = None

		def __init__(self):
			DragButton.__init__(self)
			self.AddFlag("movable")
			#self.AddFlag("restrict_x")

		def MakeImage(self):
			top = ImageBox()
			top.SetParent(self)
			top.LoadImage("d:/ymir work/ui/pattern/ScrollBar_Top.tga")
			top.SetPosition(0, 0)
			top.AddFlag("not_pick")
			top.Show()
			bottom = ImageBox()
			bottom.SetParent(self)
			bottom.LoadImage("d:/ymir work/ui/pattern/ScrollBar_Bottom.tga")
			bottom.AddFlag("not_pick")
			bottom.Show()

			middle = ExpandedImageBox()
			middle.SetParent(self)
			middle.LoadImage("d:/ymir work/ui/pattern/ScrollBar_Middle.tga")
			middle.SetPosition(0, 4)
			middle.AddFlag("not_pick")
			middle.Show()

			self.top = top
			self.bottom = bottom
			self.middle = middle

		def SetSize(self, height):
			height = max(12, height)
			DragButton.SetSize(self, 10, height)
			if self.bottom:
				self.bottom.SetPosition(0, height-4)

			height -= 4*3
			if self.middle:
				self.middle.SetRenderingRect(0, 0, 0, float(height)/4.0)

	def __init__(self):
		Window.__init__(self)

		self.pageSize = 1
		self.curPos = 0.0
		self.eventScroll = lambda *arg: None
		self.lockFlag = False
		self.scrollStep = 0.20


		self.CreateScrollBar()

	def __del__(self):
		Window.__del__(self)

	def CreateScrollBar(self):
		barSlot = Bar3D()
		barSlot.SetParent(self)
		barSlot.AddFlag("not_pick")
		barSlot.Show()

		middleBar = self.MiddleBar()
		middleBar.SetParent(self)
		middleBar.SetMoveEvent(__mem_func__(self.OnMove))
		middleBar.Show()
		middleBar.MakeImage()
		middleBar.SetSize(12)

		upButton = Button()
		upButton.SetParent(self)
		upButton.SetEvent(__mem_func__(self.OnUp))
		upButton.SetUpVisual("d:/ymir work/ui/public/scrollbar_up_button_01.sub")
		upButton.SetOverVisual("d:/ymir work/ui/public/scrollbar_up_button_02.sub")
		upButton.SetDownVisual("d:/ymir work/ui/public/scrollbar_up_button_03.sub")
		upButton.Show()

		downButton = Button()
		downButton.SetParent(self)
		downButton.SetEvent(__mem_func__(self.OnDown))
		downButton.SetUpVisual("d:/ymir work/ui/public/scrollbar_down_button_01.sub")
		downButton.SetOverVisual("d:/ymir work/ui/public/scrollbar_down_button_02.sub")
		downButton.SetDownVisual("d:/ymir work/ui/public/scrollbar_down_button_03.sub")
		downButton.Show()

		self.upButton = upButton
		self.downButton = downButton
		self.middleBar = middleBar
		self.barSlot = barSlot

		self.SCROLLBAR_WIDTH = self.upButton.GetWidth()
		self.SCROLLBAR_MIDDLE_HEIGHT = self.middleBar.GetHeight()
		self.SCROLLBAR_BUTTON_WIDTH = self.upButton.GetWidth()
		self.SCROLLBAR_BUTTON_HEIGHT = self.upButton.GetHeight()

		# if app.ENABLE_MOUSEWHEEL_EVENT:
		# 	self.upButton.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.downButton.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.middleBar.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.barSlot.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)

	if app.ENABLE_MOUSEWHEEL_EVENT:
		def OnMouseWheel(self, delta):
			if delta > 0:
				self.SetPos(self.curPos - (self.scrollStep/2))
				return True
			elif delta < 0:
				self.SetPos(self.curPos + (self.scrollStep/2))
				return True
			return False

	@WindowDestroy
	def Destroy(self):
		self.middleBar = None
		self.upButton = None
		self.downButton = None
		self.view_window = None
		self.content_window = None
		self.eventScroll = lambda *arg: None

	def SetScrollEvent(self, event):
		self.eventScroll = event

	def SAFE_SetOnWheelEvent(self, window):
		window.SAFE_SetStringEvent("MOUSE_WHEEL_UP", self.OnMouseWheel, 1)
		window.SAFE_SetStringEvent("MOUSE_WHEEL_DOWN", self.OnMouseWheel, -1)

	def SetMiddleBarSize(self, pageScale):
		realHeight = self.GetHeight() - self.SCROLLBAR_BUTTON_HEIGHT*2
		self.SCROLLBAR_MIDDLE_HEIGHT = int(pageScale * float(realHeight))
		self.middleBar.SetSize(self.SCROLLBAR_MIDDLE_HEIGHT)
		self.pageSize = (self.GetHeight() - self.SCROLLBAR_BUTTON_HEIGHT*2) - self.SCROLLBAR_MIDDLE_HEIGHT - (self.TEMP_SPACE)

	def SetScrollBarSize(self, height):
		self.pageSize = (height - self.SCROLLBAR_BUTTON_HEIGHT*2) - self.SCROLLBAR_MIDDLE_HEIGHT - (self.TEMP_SPACE)
		self.SetSize(self.SCROLLBAR_WIDTH, height)
		self.upButton.SetPosition(0, 0)
		self.downButton.SetPosition(0, height - self.SCROLLBAR_BUTTON_HEIGHT)
		self.middleBar.SetRestrictMovementArea(self.MIDDLE_BAR_POS, self.SCROLLBAR_BUTTON_HEIGHT + self.MIDDLE_BAR_UPPER_PLACE, self.MIDDLE_BAR_POS+2, height - self.SCROLLBAR_BUTTON_HEIGHT*2 - self.TEMP_SPACE)
		self.middleBar.SetPosition(self.MIDDLE_BAR_POS, 0)

		self.UpdateBarSlot()

	def UpdateBarSlot(self):
		self.barSlot.SetPosition(0, self.SCROLLBAR_BUTTON_HEIGHT)
		self.barSlot.SetSize(self.GetWidth() - 2, self.GetHeight() - self.SCROLLBAR_BUTTON_HEIGHT*2 - 2)
	if app.ENABLE_IKASHOP_RENEWAL:
		def UpdateScrollbarLenght(self, scrollableSpace):
			minheight = self.GetHeight() - self.SCROLLBAR_BUTTON_HEIGHT*2
			scrollable = max(minheight, scrollableSpace)
			scale = float(scrollable) / max(1, minheight)
			scale = 1.0 / max(1.0, scale)
			scale = max(0.08, scale)
			self.SetScrollStep(scale/3.0)
			self.SetMiddleBarSize(scale)
			self.SetScrollStep((scale/3.0) if minheight > 80 else 0.5)


	def GetPos(self):
		return self.curPos

	def SetPos(self, pos):
		pos = max(0.0, pos)
		pos = min(1.0, pos)

		newPos = float(self.pageSize) * pos
		self.middleBar.SetPosition(self.MIDDLE_BAR_POS, int(newPos) + self.SCROLLBAR_BUTTON_HEIGHT + self.MIDDLE_BAR_UPPER_PLACE)
		self.OnMove()

	def SetScrollStep(self, step):
		self.scrollStep = step

	def GetScrollStep(self):
		return self.scrollStep

	def OnUp(self):
		self.SetPos(self.curPos-self.scrollStep)

	def OnDown(self):
		self.SetPos(self.curPos+self.scrollStep)

	def OnMove(self):

		if self.lockFlag:
			return

		if 0 == self.pageSize:
			return

		(xLocal, yLocal) = self.middleBar.GetLocalPosition()
		self.curPos = float(yLocal - self.SCROLLBAR_BUTTON_HEIGHT - self.MIDDLE_BAR_UPPER_PLACE) / float(self.pageSize)

		self.eventScroll()

	def OnMouseLeftButtonDown(self):
		(xMouseLocalPosition, yMouseLocalPosition) = self.GetMouseLocalPosition()
		pickedPos = yMouseLocalPosition - self.SCROLLBAR_BUTTON_HEIGHT - self.SCROLLBAR_MIDDLE_HEIGHT/2
		newPos = float(pickedPos) / float(self.pageSize)
		self.SetPos(newPos)

	def LockScroll(self):
		self.lockFlag = True

	def UnlockScroll(self):
		self.lockFlag = False

	def SetScrollContent(self, view_window, content_window):
		self.view_window = view_window
		self.content_window = content_window
		self.SetScrollEvent(__mem_func__(self.OnScroll))

	def SetStepScrollBasedOnAverageHeight(self, item_list):
		if self.view_window and self.content_window:
			avg_height = 0
			for i in item_list:
				avg_height += i.GetHeight()

			avg_height /= max(len(item_list), 1)

			view_height = self.view_window.GetHeight()
			content_height = self.content_window.GetHeight()
			content_height -= view_height
			self.scrollStep = float(avg_height) / content_height

	def ResizeScrollBar(self):
		if self.view_window and self.content_window:
			progress = 1
			total_height = self.content_window.GetHeight()
			if total_height > 0:
				progress = min(float(self.view_window.GetHeight()) / total_height, 1)

			if progress >= 1:
				self.Hide()
			else:
				self.Show()

			self.SetMiddleBarSize(progress)  # 0 - 1
			self.SetPos(0)

	def OnScroll(self):
		if self.view_window and self.content_window:
			startPos = 0

			maskWindowHeight = self.view_window.GetHeight()
			scrollContentHeight = self.content_window.GetHeight()
			endPos = 0
			if maskWindowHeight < scrollContentHeight:
				endPos = maskWindowHeight - scrollContentHeight
			v = mathUtils.GetLerp(startPos, endPos, self.GetPos())

			self.content_window.SetPosition(0, v)

class ThinScrollBar(ScrollBar):

	def CreateScrollBar(self):
		middleBar = self.MiddleBar()
		middleBar.SetParent(self)
		middleBar.SetMoveEvent(__mem_func__(self.OnMove))
		middleBar.Show()
		middleBar.SetUpVisual("d:/ymir work/ui/public/scrollbar_thin_middle_button_01.sub")
		middleBar.SetOverVisual("d:/ymir work/ui/public/scrollbar_thin_middle_button_02.sub")
		middleBar.SetDownVisual("d:/ymir work/ui/public/scrollbar_thin_middle_button_03.sub")

		upButton = Button()
		upButton.SetParent(self)
		upButton.SetUpVisual("d:/ymir work/ui/public/scrollbar_thin_up_button_01.sub")
		upButton.SetOverVisual("d:/ymir work/ui/public/scrollbar_thin_up_button_02.sub")
		upButton.SetDownVisual("d:/ymir work/ui/public/scrollbar_thin_up_button_03.sub")
		upButton.SetEvent(__mem_func__(self.OnUp))
		upButton.Show()

		downButton = Button()
		downButton.SetParent(self)
		downButton.SetUpVisual("d:/ymir work/ui/public/scrollbar_thin_down_button_01.sub")
		downButton.SetOverVisual("d:/ymir work/ui/public/scrollbar_thin_down_button_02.sub")
		downButton.SetDownVisual("d:/ymir work/ui/public/scrollbar_thin_down_button_03.sub")
		downButton.SetEvent(__mem_func__(self.OnDown))
		downButton.Show()

		self.middleBar = middleBar
		self.upButton = upButton
		self.downButton = downButton

		self.SCROLLBAR_WIDTH = self.upButton.GetWidth()
		self.SCROLLBAR_MIDDLE_HEIGHT = self.middleBar.GetHeight()
		self.SCROLLBAR_BUTTON_WIDTH = self.upButton.GetWidth()
		self.SCROLLBAR_BUTTON_HEIGHT = self.upButton.GetHeight()
		self.MIDDLE_BAR_POS = 0
		self.MIDDLE_BAR_UPPER_PLACE = 0
		self.MIDDLE_BAR_DOWNER_PLACE = 0
		self.TEMP_SPACE = 0

	def UpdateBarSlot(self):
		pass

class SmallThinScrollBar(ScrollBar):

	def CreateScrollBar(self):
		middleBar = self.MiddleBar()
		middleBar.SetParent(self)
		middleBar.SetMoveEvent(__mem_func__(self.OnMove))
		middleBar.Show()
		middleBar.SetUpVisual("d:/ymir work/ui/public/scrollbar_small_thin_middle_button_01.sub")
		middleBar.SetOverVisual("d:/ymir work/ui/public/scrollbar_small_thin_middle_button_01.sub")
		middleBar.SetDownVisual("d:/ymir work/ui/public/scrollbar_small_thin_middle_button_01.sub")

		upButton = Button()
		upButton.SetParent(self)
		upButton.SetUpVisual("d:/ymir work/ui/public/scrollbar_small_thin_up_button_01.sub")
		upButton.SetOverVisual("d:/ymir work/ui/public/scrollbar_small_thin_up_button_02.sub")
		upButton.SetDownVisual("d:/ymir work/ui/public/scrollbar_small_thin_up_button_03.sub")
		upButton.SetEvent(__mem_func__(self.OnUp))
		upButton.Show()

		downButton = Button()
		downButton.SetParent(self)
		downButton.SetUpVisual("d:/ymir work/ui/public/scrollbar_small_thin_down_button_01.sub")
		downButton.SetOverVisual("d:/ymir work/ui/public/scrollbar_small_thin_down_button_02.sub")
		downButton.SetDownVisual("d:/ymir work/ui/public/scrollbar_small_thin_down_button_03.sub")
		downButton.SetEvent(__mem_func__(self.OnDown))
		downButton.Show()

		self.middleBar = middleBar
		self.upButton = upButton
		self.downButton = downButton

		self.SCROLLBAR_WIDTH = self.upButton.GetWidth()
		self.SCROLLBAR_MIDDLE_HEIGHT = self.middleBar.GetHeight()
		self.SCROLLBAR_BUTTON_WIDTH = self.upButton.GetWidth()
		self.SCROLLBAR_BUTTON_HEIGHT = self.upButton.GetHeight()
		self.MIDDLE_BAR_POS = 0
		self.MIDDLE_BAR_UPPER_PLACE = 0
		self.MIDDLE_BAR_DOWNER_PLACE = 0
		self.TEMP_SPACE = 0

		# if app.ENABLE_MOUSEWHEEL_EVENT:
		# 	self.middleBar.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.upButton.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.downButton.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)
		# 	self.SetMouseWheelEvent(self.OnMouseWheel_ScrollBar)

	def UpdateBarSlot(self):
		pass

class SliderBar(Window):

	def __init__(self):
		Window.__init__(self)

		self.curPos = 1.0
		self.pageSize = 1.0
		self.eventChange = None

		self.__CreateBackGroundImage()
		self.__CreateCursor()

	def __del__(self):
		Window.__del__(self)

	def __CreateBackGroundImage(self):
		img = ImageBox()
		img.SetParent(self)
		img.LoadImage("d:/ymir work/ui/game/windows/sliderbar.sub")
		img.Show()
		self.backGroundImage = img

		##
		self.SetSize(self.backGroundImage.GetWidth(), self.backGroundImage.GetHeight())

	def __CreateCursor(self):
		cursor = DragButton()
		cursor.AddFlag("movable")
		cursor.AddFlag("restrict_y")
		cursor.SetParent(self)
		cursor.SetMoveEvent(__mem_func__(self.__OnMove))
		cursor.SetUpVisual("d:/ymir work/ui/game/windows/sliderbar_cursor.sub")
		cursor.SetOverVisual("d:/ymir work/ui/game/windows/sliderbar_cursor.sub")
		cursor.SetDownVisual("d:/ymir work/ui/game/windows/sliderbar_cursor.sub")
		cursor.Show()
		self.cursor = cursor

		##
		self.cursor.SetRestrictMovementArea(0, 0, self.backGroundImage.GetWidth(), 0)
		self.pageSize = self.backGroundImage.GetWidth() - self.cursor.GetWidth()

	def __OnMove(self):
		(xLocal, yLocal) = self.cursor.GetLocalPosition()
		self.curPos = float(xLocal) / float(self.pageSize)

		if self.eventChange:
			self.eventChange()

	def SetSliderPos(self, pos):
		self.curPos = pos
		self.cursor.SetPosition(int(self.pageSize * pos), 0)

	def GetSliderPos(self):
		return self.curPos

	def SetEvent(self, event):
		self.eventChange = event

	def Enable(self):
		self.cursor.Show()

	def Disable(self):
		self.cursor.Hide()

class ListBox(Window):
	TEMPORARY_PLACE = 3

	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)
		self.overLine = -1
		self.selectedLine = -1
		self.width = 0
		self.height = 0
		self.stepSize = 17
		self.basePos = 0
		self.showLineCount = 0
		self.itemCenterAlign = True
		self.itemList = []
		self.keyDict = {}
		self.textDict = {}
		self.event = lambda *arg: None

	def __del__(self):
		Window.__del__(self)

	def SetWidth(self, width):
		self.SetSize(width, self.height)

	def SetSize(self, width, height):
		Window.SetSize(self, width, height)
		self.width = width
		self.height = height

	def SetTextCenterAlign(self, flag):
		self.itemCenterAlign = flag

	def SetBasePos(self, pos):
		self.basePos = pos
		self._LocateItem()

	def ClearItem(self):
		self.keyDict = {}
		self.textDict = {}
		self.itemList = []
		self.overLine = -1
		self.selectedLine = -1

	def InsertItem(self, number, text, textColor=None, textFont=None):
		self.keyDict[len(self.itemList)] = number
		self.textDict[len(self.itemList)] = text

		textLine = TextLine()
		textLine.SetParent(self)

		if textFont is not None:
			textLine.SetFontName(textFont)

		textLine.SetText(text)
		if textColor is not None:
			textLine.SetPackedFontColor(textColor)

		textLine.Show()

		if self.itemCenterAlign:
			textLine.SetWindowHorizontalAlignCenter()
			textLine.SetHorizontalAlignCenter()

		self.itemList.append(textLine)

		self._LocateItem()

	def ChangeItem(self, number, text):
		for key, value in self.keyDict.items():
			if value == number:
				self.textDict[key] = text

				if number < len(self.itemList):
					self.itemList[key].SetText(text)

				return

	def LocateItem(self):
		self._LocateItem()

	def _LocateItem(self):

		skipCount = self.basePos
		yPos = 0
		self.showLineCount = 0

		for textLine in self.itemList:
			textLine.Hide()

			if skipCount > 0:
				skipCount -= 1
				continue

			if localeInfo.IsARABIC():
				w, h = textLine.GetTextSize()
				textLine.SetPosition(w+10, yPos + 3)
			else:
				textLine.SetPosition(0, yPos + 3)

			yPos += self.stepSize

			if yPos <= self.GetHeight():
				self.showLineCount += 1
				textLine.Show()

	def ArrangeItem(self):
		self.SetSize(self.width, len(self.itemList) * self.stepSize)
		self._LocateItem()

	def GetViewItemCount(self):
		return int(self.GetHeight() / self.stepSize)

	def GetItemCount(self):
		return len(self.itemList)

	def SetEvent(self, event):
		self.event = event

	def SelectItem(self, line):

		if not self.keyDict.has_key(line):
			return

		if line == self.selectedLine:
			return

		self.selectedLine = line
		self.event(self.keyDict.get(line, 0), self.textDict.get(line, "None"))

	def SelectIndex(self, index):
		self.selectedLine = index
		self.event(self.GetSelectedItem(), self.textDict.get(index, "None"))

	def ClearSelection(self):
		self.selectedLine = -1

	def GetSelectedItem(self):
		return self.keyDict.get(self.selectedLine, 0)

	def OnMouseLeftButtonDown(self):
		if self.overLine < 0:
			return

	def OnMouseLeftButtonUp(self):
		if self.overLine >= 0:
			self.SelectItem(self.overLine+self.basePos)

	def OnUpdate(self):

		self.overLine = -1

		if self.IsIn():
			x, y = self.GetGlobalPosition()
			height = self.GetHeight()
			xMouse, yMouse = wndMgr.GetMousePosition()

			if yMouse - y < height - 1:
				self.overLine = (yMouse - y) / self.stepSize

				if self.overLine < 0:
					self.overLine = -1
				if self.overLine >= len(self.itemList):
					self.overLine = -1

	def OnRender(self):
		xRender, yRender = self.GetGlobalPosition()
		yRender -= self.TEMPORARY_PLACE
		widthRender = self.width
		heightRender = self.height + self.TEMPORARY_PLACE*2

		if localeInfo.IsCIBN10:
			if -1 != self.overLine and self.keyDict[self.overLine] != -1:
				grp.SetColor(HALF_WHITE_COLOR)
				grp.RenderBar(xRender + 2, yRender + self.overLine*self.stepSize + 4, self.width - 3, self.stepSize)

			if -1 != self.selectedLine and self.keyDict[self.selectedLine] != -1:
				if self.selectedLine >= self.basePos:
					if self.selectedLine - self.basePos < self.showLineCount:
						grp.SetColor(SELECT_COLOR)
						grp.RenderBar(xRender + 2, yRender + (self.selectedLine-self.basePos)*self.stepSize + 4, self.width - 3, self.stepSize)

		else:
			if -1 != self.overLine:
				grp.SetColor(HALF_WHITE_COLOR)
				grp.RenderBar(xRender + 2, yRender + self.overLine*self.stepSize + 4, self.width - 3, self.stepSize)

			if -1 != self.selectedLine:
				if self.selectedLine >= self.basePos:
					if self.selectedLine - self.basePos < self.showLineCount:
						grp.SetColor(SELECT_COLOR)
						grp.RenderBar(xRender + 2, yRender + (self.selectedLine-self.basePos)*self.stepSize + 4, self.width - 3, self.stepSize)



class ListBox2(ListBox):
	def __init__(self, *args, **kwargs):
		ListBox.__init__(self, *args, **kwargs)
		self.rowCount = 10
		self.barWidth = 0
		self.colCount = 0

	def SetRowCount(self, rowCount):
		self.rowCount = rowCount

	def SetSize(self, width, height):
		ListBox.SetSize(self, width, height)
		self._RefreshForm()

	def ClearItem(self):
		ListBox.ClearItem(self)
		self._RefreshForm()

	def InsertItem(self, *args, **kwargs):
		ListBox.InsertItem(self, *args, **kwargs)
		self._RefreshForm()

	def OnUpdate(self):
		mpos = wndMgr.GetMousePosition()
		self.overLine = self._CalcPointIndex(mpos)

	def OnRender(self):
		x, y = self.GetGlobalPosition()
		pos = (x + 2, y)

		if -1 != self.overLine:
			grp.SetColor(HALF_WHITE_COLOR)
			self._RenderBar(pos, self.overLine)

		if -1 != self.selectedLine:
			if self.selectedLine >= self.basePos:
				if self.selectedLine - self.basePos < self.showLineCount:
					grp.SetColor(SELECT_COLOR)
					self._RenderBar(pos, self.selectedLine-self.basePos)



	def _CalcPointIndex(self, mpos):
		if self.IsIn():
			px, py = mpos
			gx, gy = self.GetGlobalPosition()
			lx, ly = px - gx, py - gy

			col = lx / self.barWidth
			row = ly / self.stepSize
			idx = col * self.rowCount + row
			if col >= 0 and col < self.colCount:
				if row >= 0 and row < self.rowCount:
					if idx >= 0 and idx < len(self.itemList):
						return idx

		return -1

	def _CalcRenderPos(self, pos, idx):
		x, y = pos
		row = idx % self.rowCount
		col = idx / self.rowCount
		return (x + col * self.barWidth, y + row * self.stepSize)

	def _RenderBar(self, basePos, idx):
		x, y = self._CalcRenderPos(basePos, idx)
		grp.RenderBar(x, y, self.barWidth - 3, self.stepSize)

	def _LocateItem(self):
		pos = (0, self.TEMPORARY_PLACE)

		self.showLineCount = 0
		for textLine in self.itemList:
			x, y = self._CalcRenderPos(pos, self.showLineCount)
			textLine.SetPosition(x, y)
			textLine.Show()

			self.showLineCount += 1

	def _RefreshForm(self):
		if len(self.itemList) % self.rowCount:
			self.colCount = len(self.itemList) / self.rowCount + 1
		else:
			self.colCount = len(self.itemList) / self.rowCount

		if self.colCount:
			self.barWidth = self.width / self.colCount
		else:
			self.barWidth = self.width


class ComboBox(Window):

	class ListBoxWithBoard(ListBox):

		def __init__(self, layer):
			ListBox.__init__(self, layer)

		def OnRender(self):
			xRender, yRender = self.GetGlobalPosition()
			yRender -= self.TEMPORARY_PLACE
			widthRender = self.width
			heightRender = self.height + self.TEMPORARY_PLACE*2
			grp.SetColor(BACKGROUND_COLOR)
			grp.RenderBar(xRender, yRender, widthRender, heightRender)
			grp.SetColor(DARK_COLOR)
			grp.RenderLine(xRender, yRender, widthRender, 0)
			grp.RenderLine(xRender, yRender, 0, heightRender)
			grp.SetColor(BRIGHT_COLOR)
			grp.RenderLine(xRender, yRender+heightRender, widthRender, 0)
			grp.RenderLine(xRender+widthRender, yRender, 0, heightRender)

			ListBox.OnRender(self)

	def __init__(self):
		Window.__init__(self)
		self.x = 0
		self.y = 0
		self.width = 0
		self.height = 0
		self.isSelected = False
		self.isOver = False
		self.isListOpened = False
		self.event = lambda *arg: None
		self.enable = True
		self.index = -1

		self.textLine = MakeTextLine(self)
		self.textLine.SetText(localeInfo.UI_ITEM)

		self.listBox = self.ListBoxWithBoard("TOP_MOST")
		self.listBox.SetPickAlways()
		self.listBox.SetParent(self)
		self.listBox.SetEvent(__mem_func__(self.OnSelectItem))
		self.listBox.Hide()

	def __del__(self):
		Window.__del__(self)

	@WindowDestroy
	def Destroy(self):
		self.textLine = None
		self.listBox = None

	def SetPosition(self, x, y):
		Window.SetPosition(self, x, y)
		self.x = x
		self.y = y
		self.__ArrangeListBox()

	def SetSize(self, width, height):
		Window.SetSize(self, width, height)
		self.width = width
		self.height = height
		self.textLine.UpdateRect()
		self.__ArrangeListBox()

	def __ArrangeListBox(self):
		self.listBox.SetPosition(0, self.height + 5)
		self.listBox.SetWidth(self.width)

	def Enable(self):
		self.enable = True

	def Disable(self):
		self.enable = False
		self.textLine.SetText("")
		self.CloseListBox()

	def SetEvent(self, event):
		self.event = event

	def SAFE_SetEvent(self, func):
		self.SetEvent(__mem_func__(func))

	def ClearItem(self):
		self.CloseListBox()
		self.listBox.ClearItem()

	def InsertItem(self, index, name, color=None, font=None):
		self.listBox.InsertItem(index, name, color, font)
		self.listBox.ArrangeItem()

	def SetCurrentItem(self, text, color=None):
		self.textLine.SetText(text)
		if color:
			self.textLine.SetPackedFontColor(color)
		else:
			self.textLine.SetPackedFontColor(self.textLine.DEFAULT_COLOR)

	def SetIndex(self, index):
		self.index = index

	def SelectItem(self, key):
		self.listBox.SelectItem(key)

	def OnSelectItem(self, index, name):

		self.CloseListBox()
		self.event(index)

	def CloseListBox(self):
		self.isListOpened = False
		self.listBox.Hide()

	def OnMouseLeftButtonDown(self):

		if not self.enable:
			return

		self.isSelected = True

	def OnMouseLeftButtonUp(self):

		if not self.enable:
			return

		self.isSelected = False

		if self.isListOpened:
			self.CloseListBox()
		else:
			if self.listBox.GetItemCount() > 0:
				self.isListOpened = True
				self.listBox.Show()
				self.__ArrangeListBox()

	def OnUpdate(self):

		if not self.enable:
			return

		if self.IsIn():
			self.isOver = True
		else:
			self.isOver = False

	def OnRender(self):
		self.x, self.y = self.GetGlobalPosition()
		xRender = self.x
		yRender = self.y
		widthRender = self.width
		heightRender = self.height
		grp.SetColor(BACKGROUND_COLOR)
		grp.RenderBar(xRender, yRender, widthRender, heightRender)
		grp.SetColor(DARK_COLOR)
		grp.RenderLine(xRender, yRender, widthRender, 0)
		grp.RenderLine(xRender, yRender, 0, heightRender)
		grp.SetColor(BRIGHT_COLOR)
		grp.RenderLine(xRender, yRender+heightRender, widthRender, 0)
		grp.RenderLine(xRender+widthRender, yRender, 0, heightRender)

		if self.isOver:
			grp.SetColor(HALF_WHITE_COLOR)
			grp.RenderBar(xRender + 2, yRender + 3, self.width - 3, heightRender - 5)

			if self.isSelected:
				grp.SetColor(WHITE_COLOR)
				grp.RenderBar(xRender + 2, yRender + 3, self.width - 3, heightRender - 5)

class ScrollWindow(Window):
	THIN, MIDDLE, THICK = range(3)

	def __init__(self, layer="UI"):
		super(ScrollWindow, self).__init__(layer)
		self.layer = layer
		self.scrollBar = None
		self.contentWnd = None
		self.scrollSpeed = 4
		self.__loadScrollBar()

	def SetScrollSpeed(self, speed):
		self.scrollSpeed = speed

	def __loadScrollBar(self):
		self.scrollBar = SmallThinScrollBar()
		self.scrollBar.SetScrollBarSize(self.GetHeight())
		self.scrollBar.SetParent(self)
		self.scrollBar.SetPosition(self.GetWidth() - self.scrollBar.SCROLLBAR_WIDTH, 0)
		self.scrollBar.SetPos(0)
		self.scrollBar.SetTop()

	def Show(self):
		super(ScrollWindow, self).Show()
		if self.scrollBar:
			self.scrollBar.Show()

		if self.contentWnd:
			self.contentWnd.Show()
	
	def SetSize(self, width, height):
		super(ScrollWindow, self).SetSize(width, height)
		self.scrollBar.SetScrollBarSize(self.GetHeight())

	def Hide(self):
		super(ScrollWindow, self).Hide()
		if self.scrollBar:
			self.scrollBar.Hide()

		if self.contentWnd:
			self.contentWnd.Hide()

	def NeedScrollbar(self):
		return self.contentWnd.GetHeight() > self.GetHeight()

	def SetContentWindow(self, contentWnd):
		x, y = contentWnd.GetLocalPosition()
		from _weakref import proxy
		self.contentWnd = proxy(contentWnd)
		self.contentWnd.SetParent(self)
		self.contentWnd.SetSizeChangeEvent(self.OnContentWindowSizeChange)
		self.scrollBar.SetScrollContent(self, self.contentWnd)
		self.scrollBar.SAFE_SetOnWheelEvent(self.contentWnd)
		self.scrollBar.ResizeScrollBar()
		self.startContentOffset = (x, y)
		self.SetContentOffset(x, y)

	def InitializeClipping(self):
		if self.contentWnd:
			self.contentWnd.SetClippingMaskWindow(self)

	def OnContentWindowSizeChange(self):
		if self.scrollBar and self.contentWnd:
			if not self.NeedScrollbar():
				self.scrollBar.Hide()
				self.ScrollToStart()
			else:
				self.scrollBar.ResizeScrollBar()
				self.scrollBar.Show()

	def FitWidth(self, contentWidth):
		fittedWidth = contentWidth + self.scrollBar.SCROLLBAR_WIDTH
		self.SetSize(fittedWidth, self.GetHeight())
		self.scrollBar.SetScrollBarSize(self.GetHeight())
		self.scrollBar.SetPosition(contentWidth - 2, 0)
		self.scrollBar.SetTop()

	def AutoFitWidth(self):
		fittedWidth = self.contentWnd.GetWidth() + self.scrollBar.SCROLLBAR_WIDTH
		self.SetSize(fittedWidth, self.GetHeight())
		self.scrollBar.SetScrollBarSize(self.GetHeight())
		self.scrollBar.SetPosition(self.contentWnd.GetWidth(), 0)
		self.scrollBar.SetTop()

	def QuestWindowFitWidth(self):
		fittedWidth = self.contentWnd.GetWidth() + self.scrollBar.SCROLLBAR_WIDTH
		self.SetSize(fittedWidth, self.GetHeight())
		self.scrollBar.SetScrollBarSize(self.GetHeight())
		self.scrollBar.SetPosition(self.contentWnd.GetWidth() - self.scrollBar.SCROLLBAR_WIDTH - 10, 0)
		self.scrollBar.SetTop()

	def QuestSlideWindowFitWidth(self):
		fittedWidth = self.contentWnd.GetWidth() + self.scrollBar.SCROLLBAR_WIDTH
		self.SetSize(fittedWidth, self.GetHeight())
		self.scrollBar.SetScrollBarSize(self.GetHeight() - 15)
		self.scrollBar.SetPosition(self.contentWnd.GetWidth() - self.scrollBar.SCROLLBAR_WIDTH - 10, 15)
		self.scrollBar.SetTop()

	def OnUpdate(self):
		if not self.NeedScrollbar():
			self.scrollBar.Hide()

	def ShowScrollBar(self):
		self.scrollBar.Show()

	def HideScrollBar(self):
		self.scrollBar.Hide()

	def MiddleButtonDragEnd(self):
		pass
		# self.scrollBar.SetPos(
		#    int(round(self.scrollBar.GetPos() / self.scrollBar.GetScrollStep())) * self.scrollBar.GetScrollStep())

	def SetScrollStep(self, step):
		self.scrollBar.SetScrollStep(step)

	def GetContentOffset(self):
		return self.contentWnd.GetLocalPosition()

	def SetContentOffset(self, x, y):
		self.contentWnd.SetPosition(int(x), int(round(y)))

	def SetScrollPos(self, pos):
		self.scrollBar.SetPos(pos)

	def ScrollToStart(self):
		self.SetContentOffset(*self.startContentOffset)
		self.SetScrollPos(0.0)
		
class FillImage(WindowWithBaseEvents):

	MODE_HORIZONTAL = 0
	MODE_VERTICAL = 1

	def __init__(self, layer = "UI"):
		WindowWithBaseEvents.__init__(self)

		baseImg = ImageBox()
		baseImg.SetParent(self)
		baseImg.Show()
		self.baseImg = baseImg

		fillImg = ExpandedImageBox()
		fillImg.SetParent(baseImg)
		fillImg.Show()
		self.fillImg = fillImg

		self.perc = 0.0
		self.mode = self.MODE_HORIZONTAL

		self.SetWindowName("NONAME_FillImage")

	def SetHorizontalMode(self):
		self.mode = self.MODE_HORIZONTAL

	def SetVerticalMode(self):
		self.mode = self.MODE_VERTICAL

	def LoadBaseImage(self, image):
		self.baseImg.LoadImage(image)
		
		WindowWithBaseEvents.SetSize(self, self.baseImg.GetWidth(), self.baseImg.GetHeight())
		self.SetSize(self.baseImg.GetWidth(), self.baseImg.GetHeight())

	def LoadFillImage(self, image, x = 0, y = 0):
		self.fillImg.SetPosition(x, y)
		self.fillImg.LoadImage(image)
		self.__RefreshPercentage()

	def SetPercentage(self, perc):
		self.perc = float(perc)
		if self.perc < 0.0:
			self.perc = 0.0
		if self.perc > 100.0:
			self.perc = 100.0
		self.__RefreshPercentage()

	def __RefreshPercentage(self):
		if self.mode == self.MODE_HORIZONTAL:
			self.fillImg.SetPercentage(self.perc, 100.0)
		elif self.mode == self.MODE_VERTICAL:
			self.fillImg.SetVPercentage(self.perc, 100.0)

	def IsIn(self):
		return Window.IsIn(self) or self.baseImg.IsIn() or self.fillImg.IsIn()

class ColumnBlock(Window):
	def __init__(self, length):
		Window.__init__(self)
		self.length = length
		self.columns = []
		for i in range(length):
			column_window = Window()
			column_window.SetParent(self)
			column_window.Show()
			self.columns.append({"window": column_window, "children": []})

	def SetColumnWidth(self, width):
		total_width = 0
		idx = 0
		for column_data in self.columns:
			column_wnd = column_data["window"]
			column_wnd.SetWidth(width)
			column_wnd.SetPosition(width * idx, 0)
			total_width += width
			idx += 1

		self.SetWidth(total_width)

	def SetColumnHeight(self, height):
		for column_data in self.columns:
			column_data["window"].SetHeight(height)

		self.SetHeight(height)

	def GetColumn(self, index):
		return self.columns[index]

	def GetColumnWindow(self, index):
		return self.GetColumn(index)["window"]

	def GetColumnChildren(self, index):
		return self.GetColumn(index)["children"]

	def RefreshColumnSize(self):
		minWidth = 0
		minHeight = 0

		for column_data in self.columns:
			column_height = 0
			for child in column_data["children"]:
				child_width = child.GetWidth()
				child_height = child.GetHeight()
				minWidth = max(minWidth, child_width)
				column_height += child_height

			minHeight = max(column_height, minHeight)

		self.SetColumnWidth(minWidth)
		self.SetColumnHeight(minHeight)

	def AddItem(self, column_index, item):
		item.SetParent(self.GetColumnWindow(column_index))
		childrenList = self.GetColumnChildren(column_index)
		set_pos = 0
		for child in childrenList:
			set_pos += child.GetHeight()

		item.SetWindowHorizontalAlignCenter()
		item.SetPosition(0, set_pos)
		childrenList.append(item)
		self.RefreshColumnSize()

class ColourBox(Box):

	BASE_OPACITY = 40 ## %

	def	__init__(self):
		Box.__init__(self)

		self.boxContent = Bar()
		self.boxContent.SetParent(self)
		self.boxContent.SetPosition(1, 1)
		self.boxContent.Show()

	def	__del__(self):
		Box.__del__(self)

		self.boxContent = None

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterBox(self, layer)

	def	SetSize(self, iWidth, iHeight):
		Box.SetSize(self, iWidth, iHeight)

		self.boxContent.SetSize(iWidth-2, iHeight-2)

	def SetColor(self, color, cntColor = None):
		Box.SetColor(self, color)

		## We need to set transparency on boxContent (if user did not provide any opacity)
		## Popping up value from const
		try:
			colorText = hex(color)
			color = int(hex(int(255*self.BASE_OPACITY/100)) + colorText[4:len(colorText)-1], 16)
		except Exception as e:
			print "Invalid color value was provided", e

		self.boxContent.SetColor(color if not cntColor else cntColor)

class TabBoard(Window):
	BUTTON_NORMAL	=	0
	BUTTON_HOVER	=	1
	BUTTON_ACTIVE	=	2
	def __init__(self):
		Window.__init__(self)

		self.__imageResources = {
			self.BUTTON_NORMAL : None,
			self.BUTTON_HOVER : None,
			self.BUTTON_ACTIVE : None,
		}

		self.main_board = None
		self.tab_buttons = {}
		self.tab_width = 85
		self.selected_tab = None

	def SetSize(self, width, height):
		Window.SetSize(self, width, height)

	def SetTabWidth(self, width):
		self.tab_width = width
	
	def SetVisual(self, default, hover = None, active = None):
		self.__imageResources[self.BUTTON_NORMAL] = default
		if hover:
			self.__imageResources[self.BUTTON_HOVER] = hover
		if active:
			self.__imageResources[self.BUTTON_ACTIVE] = active

	def AppendTab(self, name):
		self.tab_buttons[name] = RadioButton()
		self.tab_buttons[name].SetParent(self)
		self.tab_buttons[name].SetUpVisual(self.__imageResources[self.BUTTON_NORMAL])
		self.tab_buttons[name].SetOverVisual(self.__imageResources[self.BUTTON_HOVER])
		self.tab_buttons[name].SetDownVisual(self.__imageResources[self.BUTTON_ACTIVE])
		
		self.SetTabWidth(self.tab_buttons[name].GetWidth())
		self.tab_buttons[name].SetPosition((len(self.tab_buttons) - 1) * self.tab_width, 0)
		self.tab_buttons[name].Show()

		self.tab_buttons[name].description = TextLine()
		self.tab_buttons[name].description.SetParent(self.tab_buttons[name])
		self.tab_buttons[name].description.SetHorizontalAlignCenter()
		self.tab_buttons[name].description.SetVerticalAlignCenter()
		self.tab_buttons[name].description.SetWindowHorizontalAlignCenter()
		self.tab_buttons[name].description.SetWindowVerticalAlignCenter()
		self.tab_buttons[name].description.SetText(name)
		self.tab_buttons[name].description.Show()

		self.tab_buttons[name].SAFE_SetEvent(self.OnSelectTab, name)

	def SetTabEvent(self, name, event, *args):
		self.tab_buttons[name].event_func = event
		self.tab_buttons[name].event_args = args

	def SelectTab(self, name):
		for tab in self.tab_buttons.values():
			tab.SetUp()

		self.tab_buttons[name].Down()

		self.selected_tab = name

	def OnSelectTab(self, name):
		self.SelectTab(name)

		try:
			if self.tab_buttons[name].event_func:
				apply(self.tab_buttons[name].event_func, self.tab_buttons[name].event_args)
		except:
			pass
	def GetSelectedTab(self):
		return self.selected_tab

	def GetSelectedTabIndex(self):
		if self.selected_tab is None:
			return -1

		return list(self.tab_buttons.keys()).index(self.selected_tab)

import time

class DropDownBox(Window):
	class Item(ListBoxEx.Item):
		def __init__(self, parent, text, value = 0):
			ListBoxEx.Item.__init__(self)
			self.dropdown = parent
			self.parent = parent.GetParentProxy()

			self.textline = TextLine()
			self.textline.SetParent(self)
			self.textline.SetText(text)
			self.textline.Show()

			self.value = value

		def __del__(self):
			ListBoxEx.Item.__del__(self)

		def GetValue(self):
			return self.value

		def GetText(self):
			return self.textline.GetText()

		def OnRender(self):
			if self == self.parent.GetSelectedItem():
				self.OnSelectedRender()
			elif self.IsIn():
				self.OnUnselectedRender()

		def OnUnselectedRender(self):
			width = self.GetWidth() + 30
			if self.dropdown.wndScrollBar and self.dropdown.wndScrollBar.IsShow():
				width -= 15

			x, y = self.GetGlobalPosition()
			grp.SetColor(HALF_WHITE_COLOR)
			grp.RenderBar(x, y+2, width, self.GetHeight())

		def OnSelectedRender(self):
			width = self.GetWidth() + 30
			if self.dropdown.wndScrollBar and self.dropdown.wndScrollBar.IsShow():
				width -= 15

			x, y = self.GetGlobalPosition()
			grp.SetColor(grp.GenerateColor(0.0, 0.0, 0.5, 0.3))
			grp.RenderBar(x, y+2, width, self.GetHeight())

	def __init__(self, parent):
		Window.__init__(self, "TOP_MOST")
		self.wndContainer = None
		self.wndBG = None
		self.wndList = None
		self.wndListBox = None
		self.wndScrollBar = None
		self.description = None

		self.max_size = 100
		self.speed = 1
		self.is_open = False
		self.select_event = None
		self.drag_event = {"UP" : None, "DOWN" : None}

		self.animation = {
			"active": False,
			"start_time": 0,
			"duration": 0.22,
			"from_height": 0,
			"to_height": 0,
			"direction": "open",
		}

		self.SetParentProxy(parent)
		self.MakeWindow()
		self.BindEvent()

	def MakeWindow(self):
		self.wndContainer = Box("TOP_MOST")
		self.wndContainer.SetParent(self)
		self.wndContainer.SetColor(HALF_WHITE_COLOR)
		self.wndContainer.Show()

		self.wndBG = Bar("TOP_MOST")
		self.wndBG.SetParent(self)
		self.wndBG.SetColor(BACKGROUND_COLOR)
		self.wndBG.SetPosition(2, 2)
		self.wndBG.Show()

		self.description = TextLine()
		self.description.SetParent(self.wndBG)
		self.description.SetHorizontalAlignCenter()
		self.description.SetVerticalAlignCenter()
		self.description.SetWindowHorizontalAlignCenter()
		self.description.SetWindowVerticalAlignCenter()
		self.description.Show()

		self.wndList = Bar("TOP_MOST")
		self.wndList.SetParent(self.GetParentProxy())
		self.wndList.SetColor(BACKGROUND_COLOR)
		self.wndList.SetPosition(0, 21)
		self.wndList.Show()

		self.wndScrollBar = ThinScrollBar()
		self.wndScrollBar.SetParent(self.wndList)
		self.wndScrollBar.SetScrollBarSize(0)
		self.wndScrollBar.SetCenterPosition()
		self.wndScrollBar.Hide()

		self.wndListBox = ListBoxEx()
		self.wndListBox.SetParent(self.wndList)
		self.wndListBox.SetScrollBar(self.wndScrollBar)
		self.wndListBox.Hide()

	def BindEvent(self):
		self.wndBG.OnMouseOverIn = self.OnMouseOverIn
		self.wndBG.OnMouseOverOut = self.OnMouseOverOut
		self.wndBG.OnMouseLeftButtonDown = self.ToggleListWindow

		self.wndListBox.SetSelectEvent(self.SelectListBoxItem)

	def ToggleListWindow(self):
		if self.wndListBox.IsEmpty():
			return

		if self.IsOpen():
			self.Close()
		else:
			self.Open()

	def Open(self):
		if self.is_open: return
		self.is_open = True

		self.animation["active"] = True
		self.animation["start_time"] = time.time()
		self.animation["from_height"] = self.wndList.GetHeight()
		self.animation["to_height"] = self.GetMaxHeight()
		self.animation["direction"] = "open"
		self.wndListBox.Show()
		if self.drag_event["DOWN"]:
			self.drag_event["DOWN"]()

	def Close(self):
		if not self.is_open: return
		self.is_open = False

		self.animation["active"] = True
		self.animation["start_time"] = time.time()
		self.animation["from_height"] = self.wndList.GetHeight()
		self.animation["to_height"] = 0
		self.animation["direction"] = "close"
		if self.drag_event["UP"]:
			self.drag_event["UP"]()

	def IsOpen(self):
		return self.is_open

	def SetSize(self, w, h):
		Window.SetSize(self, w, h)
		self.wndContainer.SetSize(w, h)
		self.wndBG.SetSize(w - 2, h - 2)
		self.wndList.SetSize(w, self.wndList.GetHeight())
		self.wndListBox.SetSize(w - 18, self.wndListBox.GetHeight())
		self.wndListBox.SetItemSize(self.wndList.GetWidth() - 33, self.wndListBox.itemHeight)
		self.wndScrollBar.SetPosition(w - 18, 0)

	def SetPosition(self, x, y):
		Window.SetPosition(self, x, y)
		self.wndList.SetPosition(x + 2, y + self.wndBG.GetHeight() + 4)

	def SetMaxHeight(self, value):
		self.max_size = value

	def GetMaxHeight(self):
		return min(self.wndListBox.itemStep * self.wndListBox.GetItemCount(), self.max_size)

	def SetSpeed(self, value):
		self.speed = value

	def SetItemStep(self, value):
		self.wndListBox.itemStep = value

	def SetItemHeight(self, value):
		self.wndListBox.itemHeight = value

	def SetDescription(self, text):
		self.description.SetText(text)

	def AppendItem(self, text, value = 0):
		self.wndListBox.AppendItem(self.Item(self, text, value))

		if self.GetMaxHeight() >= self.max_size:
			self.wndScrollBar.SetScrollBarSize(self.max_size)

	def RemoveAllItems(self):
		self.wndListBox.RemoveAllItems()

	def SetDragUpEvent(self, event):
		self.drag_event["UP"] = event

	def SetDragDownEvent(self, event):
		self.drag_event["DOWN"] = event

	def SetSelectEvent(self, event):
		self.select_event = event

	def SelectItem(self, index):
		self.wndListBox.SelectIndex(index)
		self.SetDescription(self.wndListBox.GetSelectedItem().GetText())
		#self.SelectListBoxItem(self.wndListBox.GetSelectedItem())

	def SelectItemValue(self, value):
		for index, item in enumerate(self.wndListBox.itemList):
			if item.GetValue() == value:
				self.SelectItem(index)
				break

	def GetSelectedItem(self):
		item = self.wndListBox.GetSelectedItem()
		return (item.GetValue(), item.GetText())

	def SelectListBoxItem(self, item = None):
		if item:
			self.SetDescription(item.GetText())

		if self.select_event:
			self.select_event()

		self.Close()

	def OnMouseOverIn(self):
		self.wndBG.SetColor(0xc00a0a0a)

	def OnMouseOverOut(self):
		self.wndBG.SetColor(BACKGROUND_COLOR)

	def OnUpdate(self):
		width, height = (self.wndList.GetWidth(), self.wndList.GetHeight())

		if self.IsOpen() and height < self.GetMaxHeight():
			self.wndList.SetSize(width, height + self.speed)
			self.wndListBox.Show()

			if self.wndList.GetHeight() >= self.max_size:
				self.wndScrollBar.Show()
		elif not self.IsOpen() and height > 0:
			self.wndList.SetSize(width, height - self.speed)
			self.wndListBox.Hide()
			self.wndScrollBar.Hide()

class BoxedBoard(Window):
	BORDER_TOP = 0
	BORDER_RIGHT = 1
	BORDER_BOTTOM = 2
	BORDER_LEFT = 3

	DEFAULT_BORDER_COLOR = grp.GenerateColor(0.3, 0.3, 0.3, 1.0)
	DEFAULT_BASE_COLOR = grp.GenerateColor(0, 0, 0, 0.5)

	def __init__(self):
		Window.__init__(self)
		
		self.borderSize = 1
		
		# Create Borders
		self.borders = [
			Bar(),
			Bar(),
			Bar(),
			Bar()
		]
		
		for border in self.borders:
			border.SetParent(self)
			border.AddFlag("not_pick")
			border.Show()
		
		# Create Base
		self.base = Bar()
		self.base.SetParent(self)
		self.base.AddFlag("not_pick")
		self.base.Show()
		
		# Set Default Colors
		self.SetBorderColor(self.DEFAULT_BORDER_COLOR)
		self.SetBaseColor(self.DEFAULT_BASE_COLOR)

	def __del__(self):
		self.Destroy()
		Window.__del__(self)

	def Destroy(self):
		del self.borders[:]
		self.base = None
		
		Window.Destroy(self)

	def SetBorderColor(self, color):
		for border in self.borders:
			border.SetColor(color)

	def SetBorderSize(self, borderSize):
		self.borderSize = borderSize
		self.SetSize(self.GetWidth(), self.GetHeight())

	def SetBaseColor(self, color):
		self.base.SetColor(color)

	def SetSize(self, width, height):
		width = max(width, (2 * self.borderSize) + 1)
		height = max(height, (2 * self.borderSize) + 1)
		
		Window.SetSize(self, width, height)
		self.UpdateBoard()

	def UpdateBoard(self):
		width = self.GetWidth()
		height = self.GetHeight()
		
		top, right, bottom, left = self.borders
		
		# Top Border
		top.SetSize(width - self.borderSize, self.borderSize)
		
		# Right Border
		right.SetSize(self.borderSize, height - self.borderSize)
		right.SetPosition(width - self.borderSize, 0)
		
		# Bottom Border
		bottom.SetSize(width - self.borderSize, self.borderSize)
		bottom.SetPosition(self.borderSize, height - self.borderSize)
		
		# Left Border
		left.SetSize(self.borderSize, height - self.borderSize)
		left.SetPosition(0, self.borderSize)
		
		# Base
		self.base.SetSize(width - (2 * self.borderSize), height - (2 * self.borderSize))
		self.base.SetPosition(self.borderSize, self.borderSize)

class LineBox(Window):

	LINE_TOP = 0
	LINE_BOTTOM = 1
	LINE_LEFT = 2
	LINE_RIGHT = 3

	def __init__(self):
		Window.__init__(self)

		self.lines = {}
		self.MakeWindow()

	def MakeWindow(self):
		self.lines[self.LINE_TOP] = Line()
		self.lines[self.LINE_TOP].SetParent(self)
		self.lines[self.LINE_TOP].SetSize(20, 0)
		self.lines[self.LINE_TOP].SetPosition(0, 0)
		self.lines[self.LINE_TOP].Show()

		self.lines[self.LINE_BOTTOM] = Line()
		self.lines[self.LINE_BOTTOM].SetParent(self)
		self.lines[self.LINE_BOTTOM].SetSize(20, 0)
		self.lines[self.LINE_BOTTOM].SetPosition(0, 20)
		self.lines[self.LINE_BOTTOM].Show()

		self.lines[self.LINE_LEFT] = Line()
		self.lines[self.LINE_LEFT].SetParent(self)
		self.lines[self.LINE_LEFT].SetSize(0, 20)
		self.lines[self.LINE_LEFT].SetPosition(0, 0)
		self.lines[self.LINE_LEFT].Show()

		self.lines[self.LINE_RIGHT] = Line()
		self.lines[self.LINE_RIGHT].SetParent(self)
		self.lines[self.LINE_RIGHT].SetSize(0, 20)
		self.lines[self.LINE_RIGHT].SetPosition(20, 0)
		self.lines[self.LINE_RIGHT].Show()

	def SetSize(self, width, height):
		Window.SetSize(self, width, height)

		self.lines[self.LINE_TOP].SetSize(width, 0)
		self.lines[self.LINE_BOTTOM].SetSize(width, 0)
		self.lines[self.LINE_LEFT].SetSize(0, height)
		self.lines[self.LINE_RIGHT].SetSize(0, height)

		self.AdjustPosition()

	def AdjustPosition(self):
		self.lines[self.LINE_TOP].SetPosition(1, 0)
		self.lines[self.LINE_BOTTOM].SetPosition(1, self.GetHeight())
		self.lines[self.LINE_LEFT].SetPosition(0, 0)
		self.lines[self.LINE_RIGHT].SetPosition(self.GetWidth(), 0)

	def SetColor(self, color):
		for line in self.lines.values():
			line.SetColor(color)

	def HideLine(self, line):
		self.lines[line].Hide()

	def ShowLine(self, line):
		self.lines[line].Show()

MAIN_PATH = "Assets/ui/auto-hunt-astra/toggle_switch/special/"
class ToggleSwitch(Window):
	def __init__(self):
		Window.__init__(self)
		self._Initialize()
		self.CreateElements()
		
	def __del__(self):
		Window.__del__(self)
		self._Initialize()
	
	def _Initialize(self):
		self.backgroundImage = None
		self.checkImage = None
		
		self.eventFunc = { "ON_CHECK" : None, "ON_UNCHECK" : None, }
		self.eventArgs = { "ON_CHECK" : None, "ON_UNCHECK" : None, }
		
		self.isChecked = False
		self.animationProgress = 0
		self.animationDuration = 10
		self.isAnimating = False
		
		self.uncheckedXPosition = 0
		self.checkedXPosition = 13
		self.animationDistance = self.checkedXPosition - self.uncheckedXPosition
	
	def CreateElements(self):
		self.backgroundImage = ImageBox()
		self.backgroundImage.SetParent(self)
		self.backgroundImage.AddFlag("not_pick")
		self.backgroundImage.SetPosition(0, 0)
		self.backgroundImage.LoadImage(MAIN_PATH + "bg.png")
		self.backgroundImage.Show()
		
		self.checkImage = ImageBox()
		self.checkImage.SetParent(self.backgroundImage)
		self.checkImage.AddFlag("not_pick")
		self.checkImage.SetPosition(self.uncheckedXPosition, 0)
		self.checkImage.LoadImage(MAIN_PATH + "Unchecked.png")
		self.checkImage.Show()
		
		self.textInfo = TextLine()
		self.textInfo.SetParent(self)
		self.textInfo.SetPosition(0, 0)
		self.textInfo.SetWindowHorizontalAlignRight()
		self.textInfo.Show()
		
		self.SetSize(self.backgroundImage.GetWidth(), self.backgroundImage.GetHeight())
		
		self.backgroundImage.SetWindowHorizontalAlignCenter()
		
	def SetTextInfo(self, info, x = 50, y = 0):
		if self.textInfo:
			self.textInfo.SetText(info)
			self.textInfo.SetPosition(x, y)
		
	def SetCheckStatus(self, flag):
		if flag != self.isChecked:
			self.isChecked = flag
			self.StartAnimation()
	
	def GetCheckStatus(self):
		if self.checkImage:
			return self.isChecked
			
		return False
		
	def SetEvent(self, func, *args) :
		result = self.eventFunc.has_key(args[0])		
		if result :
			self.eventFunc[args[0]] = func
			self.eventArgs[args[0]] = args
		else :
			print "[ERROR] ui.py SetEvent, Can`t Find has_key : %s" % args[0]
		
	def OnMouseLeftButtonUp(self):
		if not self.isAnimating:
			self.isChecked = not self.isChecked
			self.StartAnimation()

			if self.isChecked and self.eventFunc["ON_CHECK"]:
				apply(self.eventFunc["ON_CHECK"], self.eventArgs["ON_CHECK"])
			elif not self.isChecked and self.eventFunc["ON_UNCHECK"]:
				apply(self.eventFunc["ON_UNCHECK"], self.eventArgs["ON_UNCHECK"])

	def StartAnimation(self):
		self.isAnimating = True
		self.animationProgress = 0
		self.checkImage.Show()

	def OnRender(self):
		if self.isAnimating:
			self.animationProgress += 1
			progress = float(self.animationProgress) / self.animationDuration

			if progress <= 1:
				x_offset = int(math.sin(progress * math.pi / 2) * self.animationDistance)
				
				if self.isChecked:
					self.checkImage.SetPosition(self.uncheckedXPosition + x_offset, 0)
					self.checkImage.LoadImage(MAIN_PATH + "Checked.png")
				else:
					self.checkImage.SetPosition(self.checkedXPosition - x_offset, 0)
					self.checkImage.LoadImage(MAIN_PATH + "Unchecked.png")
			else:
				self.isAnimating = False
				self.animationProgress = 0
				
				if self.isChecked:
					self.checkImage.SetPosition(self.checkedXPosition, 0)
					self.checkImage.LoadImage(MAIN_PATH + "Checked.png")
				else:
					self.checkImage.SetPosition(self.uncheckedXPosition, 0)
					self.checkImage.LoadImage(MAIN_PATH + "Unchecked.png")

class ExpandedCheckBox(Window):
	IMAGE_NORMAL  = "Assets/ui/check-box/checkbox_00.tga"
	IMAGE_CHECKED = "Assets/ui/check-box/checkbox_02.tga"
	
	STATE_UNSELECTED, STATE_SELECTED = range(2)
	
	def __init__(self):
		super(ExpandedCheckBox, self).__init__()
		self.state = self.STATE_UNSELECTED
		self.event = None
		self.args = None
		self.bindStateFuncGet = None
		self.bindStateFuncSet = None
		self.boundStateFunc = False
		self.boundStateChecked = False
	
		image = ImageBox()
		image.SetParent(self)
		image.LoadImage(self.IMAGE_NORMAL)
		image.OnMouseLeftButtonDown = MakeCallback(self.OnMouseLeftButtonDown)
		image.Show()
		self.image = image
	
		textLine = TextLine()
		textLine.SetParent(self)
		textLine.SetPosition(image.GetRight() + 5, -3)
		textLine.SetVerticalAlignCenter()
		textLine.Show()
		self.textLine = textLine
		self.SetSize(image.GetWidth(), image.GetHeight())
	
	def SetText(self, text):
		self.textLine.SetText(text)
		self.SetSize(self.textLine.GetLeft() + self.textLine.GetWidth(), self.GetHeight())
	
	def IsChecked(self):
		return self.state == self.STATE_SELECTED
	
	def SetChecked(self, val):
		self.state = val
		self.__SetState(val)
	
	def __SetState(self, state):
		self.SetState(state)
		if self.event:
			self.event(state)
	
	def SetState(self, state):
		self.state = state
		self.RefreshImage()
	
	def BindStateFunc(self, funcGet, funcSet, checked=False):
		self.bindStateFuncGet = funcGet
		self.bindStateFuncSet = funcSet
		self.boundStateFunc = True
		self.boundStateChecked = checked
	
		if self.boundStateChecked:
			self.SetState(not self.bindStateFuncGet())
		else:
			self.SetState(self.bindStateFuncGet())
	
	def RefreshImage(self):
		if self.IsChecked():
			self.image.LoadImage(self.IMAGE_CHECKED)
		else:
			self.image.LoadImage(self.IMAGE_NORMAL)
	
	def OnMouseLeftButtonDown(self):
		if self.boundStateFunc:
			if self.boundStateChecked:
				self.bindStateFuncSet(not self.bindStateFuncGet())
			else:
				self.bindStateFuncSet(not self.bindStateFuncGet())
	
			if self.boundStateChecked:
				self.SetState(not self.bindStateFuncGet())
			else:
				self.SetState(self.bindStateFuncGet())
	
			return True
	
		if self.state == self.STATE_UNSELECTED:
			state = self.STATE_SELECTED
		else:
			state = self.STATE_UNSELECTED
		self.__SetState(state)
		return True
	
	def SetEvent(self, event):
		self.event = MakeEvent(event)

class SpecialButton(ImageBox):

	BUTTON_NORMAL	=	0
	BUTTON_HOVER	=	1
	BUTTON_ACTIVE	=	2

	MOVE_NONE = 0

	MOVE_HORIZONTAL = 1
	MOVE_VERTICAL = 2

	MOVE_POSITIVE = 1
	MOVE_NEGATIVE = 2

	def __init__(self):
		ImageBox.__init__(self)

		self.__imageResources = {
			self.BUTTON_NORMAL : None,
			self.BUTTON_HOVER : None,
			self.BUTTON_ACTIVE : None,
		}

		self.__buttonState = self.BUTTON_NORMAL

		self.__moveData = (self.MOVE_NONE, self.MOVE_NONE)
		self.__movePos = 0
		self.__moveSpeed = 5

		self.__enableButton = True

		self.__eventDict = {}
		self.__argsDict = {}

		self.OnMouseOverIn = lambda arg = True: self.__ToggleMouseOverButton(arg)
		self.OnMouseOverOut = lambda arg = False: self.__ToggleMouseOverButton(arg)
		self.OnMouseLeftButtonDown = lambda: self.__OnClick()
		self.OnMouseLeftButtonUp = lambda: self.__OnEndClick()

	def __del__(self):
		ImageBox.__del__(self)

	def __SetState(self, state):
		self.__buttonState = state

		if self.__imageResources[state]:
			self.LoadImage(self.__imageResources[state])

	def SetVisual(self, default, hover = None, active = None):
		self.__imageResources[self.BUTTON_NORMAL] = default

		if hover:
			self.__imageResources[self.BUTTON_HOVER] = hover

		if active:
			self.__imageResources[self.BUTTON_ACTIVE] = active

		self.__SetState(self.BUTTON_NORMAL)

	def SetEvent(self, event, *args):
		self.__eventDict["CLICK"] = event
		self.__argsDict["CLICK"] = args

	def Down(self):
		self.__SetState(self.BUTTON_ACTIVE)

	def SetUp(self):
		self.__SetState(self.BUTTON_NORMAL)

	def SetHover(self):
		self.__SetState(self.BUTTON_HOVER)

	def Enable(self):
		self.__enableButton = True

	def Disable(self):
		self.__enableButton = False

	def SetMouseOverEvent(self, key, event, *args):
		self.__eventDict[key] = event
		self.__argsDict[key] = args

	def __ToggleMouseOverButton(self, arg):
		if not self.__enableButton:
			return

		arg = int(arg)
		state = (self.BUTTON_NORMAL, self.BUTTON_HOVER)[arg]
		key = ("MOUSE_OVER_OUT", "MOUSE_OVER_IN")[arg]

		self.__SetState(state)

		try:
			if self.__eventDict[key]:
				apply(self.__eventDict[key], self.__argsDict[key])
		except:
			pass

	def SetMoveSpeed(self, value):
		self.__moveSpeed = value

	def SetPositiveVerticalMove(self, toPos):
		self.__movePos = toPos
		self.__moveData = (self.MOVE_VERTICAL, self.MOVE_POSITIVE)

	def SetNegativeVerticalMove(self, toPos):
		self.__movePos = toPos
		self.__moveData = (self.MOVE_VERTICAL, self.MOVE_NEGATIVE)

	def SetPositiveHorizontalMove(self, toPos):
		self.__movePos = toPos
		self.__moveData = (self.MOVE_HORIZONTAL, self.MOVE_POSITIVE)

	def SetNegativeHorizontalMove(self, toPos):
		self.__movePos = toPos
		self.__moveData = (self.MOVE_HORIZONTAL, self.MOVE_NEGATIVE)

	def OnUpdate(self):
		ImageBox.OnUpdate(self)

		### Needs further testing
		moveMode = self.__moveData[0]
		moveType = self.__moveData[1]

		if moveMode == self.MOVE_NONE and moveType == self.MOVE_NONE:
			return

		(x, y) = self.GetLocalPosition()
		moveSpeed = self.__moveSpeed

		comparisonValue = x
		if moveMode == self.MOVE_VERTICAL:
			comparisonValue = y

		if (moveType == self.MOVE_POSITIVE and comparisonValue >= self.__movePos) or\
		(moveType == self.MOVE_NEGATIVE and comparisonValue <= self.__movePos):
			self.__moveData = (self.MOVE_NONE, self.MOVE_NONE)
			return

		if moveType == self.MOVE_NEGATIVE:
			moveSpeed = -moveSpeed

		if moveMode == self.MOVE_VERTICAL:
			self.SetPosition(x, y + moveSpeed)
		else:
			self.SetPosition(x + moveSpeed, y)

	def __OnClick(self):
		if not self.__enableButton:
			return

		self.Down()

	def __OnEndClick(self):
		if self.__buttonState == self.BUTTON_ACTIVE and self.__enableButton:
			self.SetUp()
		try:
			if self.__eventDict["CLICK"]:
				apply(self.__eventDict["CLICK"], self.__argsDict["CLICK"])
		except:
			pass

class TransparentButton(Window):
	def	__init__(self):
		Window.__init__(self)
		self.bBuilt = False

		self.bRunningEffect = False

		self.textLine = None
		self.buttonImages = {}
		self.bindEvents = {}

	def	__del__(self):
		Window.__del__(self)

		self.bRunningEffect = False

		self.textLine = None
		self.buttonImages = {}
		self.bindEvents = {}

	def	__BuildObject(self):
		if not self.bBuilt:
			if not "NORMAL" in self.buttonImages:
				print "Cannot build button! Normal image is incorrent!"
				return

			## Loading image
			self.buttonImage = ImageBox()
			self.buttonImage.SetParent(self)
			self.buttonImage.LoadImage(self.buttonImages["NORMAL"])
			self.buttonImage.SetPosition(0, 0)

			## Size
			self.SetSize(self.buttonImage.GetWidth(), self.buttonImage.GetHeight())
			print self.buttonImage.GetWidth(), self.buttonImage.GetHeight()

			self.buttonImage.SAFE_SetStringEvent("MOUSE_OVER_IN", self.__ShowHover) 
			self.buttonImage.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__ShowNormal) 
			self.buttonImage.SetOnMouseLeftButtonUpEvent(self.__ClickButton)
			self.buttonImage.SetClickEvent(self.__ShowDark)

			self.buttonImage.Show()

			self.textLine = TextLine()
			self.textLine.SetParent(self.buttonImage)
			self.textLine.SetHorizontalAlignCenter()
			self.textLine.SetVerticalAlignCenter()
			self.textLine.SetWindowHorizontalAlignCenter()
			self.textLine.SetWindowVerticalAlignCenter()
			self.textLine.SetPosition(0, 0)
			self.textLine.Show()

			self.bBuilt = True

	def	SetUpVisual(self, sImage):
		self.buttonImages["NORMAL"] = sImage

	def	SetOverVisual(self, sImage):
		self.buttonImages["HOVER"] = sImage

	def	SetDownVisual(self, sImage):
		self.buttonImages["DOWN"] = sImage

	def	BindEvent(self, key, event, *args):
		self.bindEvents[key] = (__mem_func__(event), args)

	def	RunningEffect(self, bState):
		self.bRunningEffect = bState

	def	ReloadImage(self):
		if self.bBuilt:
			if "NORMAL" in self.buttonImages:
				self.buttonImage.LoadImage(self.buttonImages["NORMAL"])

	## Deprecated
	def	SetDisableVisual(self, sImage):
		pass

	def	SetText(self, sText):
		self.textLine.SetText(sText)

	def	SetTextColor(self, rColour):
		pass

	def	SetToolTipText(self, sText):
		pass

	## Late binds
	def	__ShowHover(self):
		if not self.bRunningEffect:
			if "HOVER" in self.buttonImages:
				self.buttonImage.LoadImage(self.buttonImages["HOVER"])

			if "HOVER" in self.bindEvents:
				self.bindEvents["HOVER"][0](*self.bindEvents["HOVER"][1])

	def	__ShowNormal(self):
		if not self.bRunningEffect:
			if "NORMAL" in self.buttonImages:
				self.buttonImage.LoadImage(self.buttonImages["NORMAL"])

			if "NORMAL" in self.bindEvents:
				self.bindEvents["NORMAL"][0](*self.bindEvents["NORMAL"][1])

	def	__ShowDark(self):
		if not self.bRunningEffect:
			if "DARK" in self.buttonImages:
				self.buttonImage.LoadImage(self.buttonImages["DARK"])

			if "DARK" in self.bindEvents:
				self.bindEvents["DARK"][0](*self.bindEvents["DARK"][1])

	def	__ClickButton(self):
		if not self.bRunningEffect:
			self.buttonImage.LoadImage(self.buttonImages["NORMAL"])

			if "CLICK" in self.bindEvents:
				self.bindEvents["CLICK"][0](*self.bindEvents["CLICK"][1])

	def	Show(self):
		if not self.bBuilt:
			self.__BuildObject()

		Window.Show(self)
###################################################################################################
## Python Script Loader
###################################################################################################

class ScriptWindow(Window):
	def __init__(self, layer = "UI"):
		Window.__init__(self, layer)
		self.Children = []
		self.ElementDictionary = {}

	def __del__(self):
		Window.__del__(self)

	def ClearDictionary(self):
		if ENABLE_RECURSIVE_UI_DESTROY:
			if hasattr(self, "Children"):
				for child in self.Children:
					if hasattr(child, "Destroy"):
						child.Destroy()
				self.Children = []
		self.Children = []
		self.ElementDictionary = {}

	def InsertChild(self, name, child):
		self.ElementDictionary[name] = child

	def IsChild(self, name):
		return self.ElementDictionary.has_key(name)

	def GetChild(self, name):
		return self.ElementDictionary[name]

	def GetChild2(self, name):
		return self.ElementDictionary.get(name, None)
class PythonScriptLoader(object):

	BODY_KEY_LIST = ( "x", "y", "width", "height" )

	#####

	DEFAULT_KEY_LIST = ( "type", "x", "y", )
	WINDOW_KEY_LIST = ( "width", "height", )
	IMAGE_KEY_LIST = ( "image", )
	EXPANDED_IMAGE_KEY_LIST = ( "image", )
	ANI_IMAGE_KEY_LIST = ( "images", )
	SLOT_KEY_LIST = ( "width", "height", "slot", )
	CANDIDATE_LIST_KEY_LIST = ( "item_step", "item_xsize", "item_ysize", )
	GRID_TABLE_KEY_LIST = ( "start_index", "x_count", "y_count", "x_step", "y_step", )
	EDIT_LINE_KEY_LIST = ( "width", "height", "input_limit", )
	COMBO_BOX_KEY_LIST = ( "width", "height", )
	TITLE_BAR_KEY_LIST = ( "width", )
	HORIZONTAL_BAR_KEY_LIST = ( "width", )
	BOARD_KEY_LIST = ( "width", "height", )
	BOARD_WITH_TITLEBAR_KEY_LIST = ( "width", "height", "title", )
	BOX_KEY_LIST = ( "width", "height", )
	BAR_KEY_LIST = ( "width", "height", )
	LINE_KEY_LIST = ( "width", "height", )
	SLOTBAR_KEY_LIST = ( "width", "height", )
	GAUGE_KEY_LIST = ( "width", "color", )
	SCROLLBAR_KEY_LIST = ( "size", )
	LIST_BOX_KEY_LIST = ( "width", "height", )

	def __init__(self):
		self.Clear()

	def Clear(self):
		self.ScriptDictionary = { "SCREEN_WIDTH" : wndMgr.GetScreenWidth(), "SCREEN_HEIGHT" : wndMgr.GetScreenHeight() }
		self.InsertFunction = 0

	def LoadScriptFile(self, window, FileName):
		import exception
		import exceptions
		import os
		import errno
		self.Clear()

		print("===== Load Script File : %s" % (FileName))

		import sys
		from utils import Sandbox
		sandbox = Sandbox(True, ["uiScriptLocale", "localeInfo", "sys", "item", "app", "player","utils", "flamewindPath", "shop","grp", "quest", "colorInfo"])

		import chr
		import player
		import app
		self.ScriptDictionary["PLAYER_NAME_MAX_LEN"] = chr.PLAYER_NAME_MAX_LEN
		self.ScriptDictionary["DRAGON_SOUL_EQUIPMENT_SLOT_START"] = player.DRAGON_SOUL_EQUIPMENT_SLOT_START
		self.ScriptDictionary["LOCALE_PATH"] = app.GetLocalePath()

		if __USE_EXTRA_CYTHON__:
			# sub functions
			from os.path import splitext as op_splitext, basename as op_basename, dirname as op_dirname
			def GetModName(filename):
				return op_splitext(op_basename(filename))[0]
			def IsInUiPath(filename):
				def ICmp(s1, s2):
					return s1.lower() == s2.lower()
				return ICmp(op_dirname(filename), "uiscript")
			# module name to import
			modname = GetModName(FileName)
			# lazy loading of uiscriptlib
			import uiscriptlib
			# copy scriptdictionary stuff to builtin scope (otherwise, import will fail)
			tpl2Main = (
				"SCREEN_WIDTH","SCREEN_HEIGHT",
				"PLAYER_NAME_MAX_LEN", "DRAGON_SOUL_EQUIPMENT_SLOT_START","LOCALE_PATH"
			)
			import __builtin__ as bt
			for idx in tpl2Main:
				tmpVal = self.ScriptDictionary[idx]
				exec "bt.%s = tmpVal"%idx in globals(), locals()
			# debug stuff
			import dbg
			dbg.TraceError("Loading %s (%s %s)"%(FileName, GetModName(FileName), IsInUiPath(FileName)))
		try:
			if __USE_EXTRA_CYTHON__ and IsInUiPath(FileName) and uiscriptlib.isExist(modname):
				m1 = uiscriptlib.moduleImport(modname)
				self.ScriptDictionary["window"] = m1.window.copy()
				del m1
			else:
				sandbox.execfile(FileName, self.ScriptDictionary)
		except IOError as err:
			import sys
			import dbg
			dbg.TraceError("Failed to load script file : %s" % (FileName))
			dbg.TraceError("error: %s" % (err))
			exception.Abort("LoadScriptFile1")
		except RuntimeError as err:
			import sys
			import dbg
			dbg.TraceError("Failed to load script file : %s" % (FileName))
			dbg.TraceError("error: %s" % (err))
			exception.Abort("LoadScriptFile2")
		except:
			import sys
			import dbg
			dbg.TraceError("Failed to load script file : %s" % (FileName))
			exception.Abort("LoadScriptFile!!!!!!!!!!!!!!")

		#####

		Body = self.ScriptDictionary["window"]
		self.CheckKeyList("window", Body, self.BODY_KEY_LIST)

		window.ClearDictionary()
		self.InsertFunction = window.InsertChild

		window.SetPosition(int(Body["x"]), int(Body["y"]))

		if localeInfo.IsARABIC():
			w = wndMgr.GetScreenWidth()
			h = wndMgr.GetScreenHeight()
			if Body.has_key("width"):
				w = int(Body["width"])
			if Body.has_key("height"):
				h = int(Body["height"])

			window.SetSize(w, h)
		else:
			window.SetSize(int(Body["width"]), int(Body["height"]))
			if True == Body.has_key("style"):
				for StyleList in Body["style"]:
					window.AddFlag(StyleList)


		self.LoadChildren(window, Body)

	def LoadChildren(self, parent, dicChildren):

		if localeInfo.IsARABIC():
			parent.AddFlag( "rtl" )

		if True == dicChildren.has_key("style"):
			for style in dicChildren["style"]:
				parent.AddFlag(style)

		if False == dicChildren.has_key("children"):
			return False

		Index = 0

		ChildrenList = dicChildren["children"]
		parent.Children = range(len(ChildrenList))
		for ElementValue in ChildrenList:
			try:
				Name = ElementValue["name"]
			except KeyError:
				Name = ElementValue["name"] = "NONAME"

			try:
				Type = ElementValue["type"]
			except KeyError:
				Type = ElementValue["type"] = "window"

			if False == self.CheckKeyList(Name, ElementValue, self.DEFAULT_KEY_LIST):
				del parent.Children[Index]
				continue

			if Type == "window":
				parent.Children[Index] = ScriptWindow()
				parent.Children[Index].SetParent(parent)
				self.LoadElementWindow(parent.Children[Index], ElementValue, parent)

			elif Type == "button":
				parent.Children[Index] = Button()
				parent.Children[Index].SetParent(parent)
				self.LoadElementButton(parent.Children[Index], ElementValue, parent)

			elif Type == "radio_button":
				parent.Children[Index] = RadioButton()
				parent.Children[Index].SetParent(parent)
				self.LoadElementButton(parent.Children[Index], ElementValue, parent)

			elif Type == "toggle_button":
				parent.Children[Index] = ToggleButton()
				parent.Children[Index].SetParent(parent)
				self.LoadElementButton(parent.Children[Index], ElementValue, parent)

			elif Type == "mark":
				parent.Children[Index] = MarkBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementMark(parent.Children[Index], ElementValue, parent)

			elif Type == "image":
				parent.Children[Index] = ImageBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementImage(parent.Children[Index], ElementValue, parent)

			elif Type == "expanded_image":
				parent.Children[Index] = ExpandedImageBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementExpandedImage(parent.Children[Index], ElementValue, parent)

			elif Type == "ani_image":
				parent.Children[Index] = AniImageBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementAniImage(parent.Children[Index], ElementValue, parent)

			elif Type == "slot":
				parent.Children[Index] = SlotWindow()
				parent.Children[Index].SetParent(parent)
				self.LoadElementSlot(parent.Children[Index], ElementValue, parent)

			elif Type == "candidate_list":
				parent.Children[Index] = CandidateListBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementCandidateList(parent.Children[Index], ElementValue, parent)

			elif Type == "grid_table":
				parent.Children[Index] = GridSlotWindow()
				parent.Children[Index].SetParent(parent)
				self.LoadElementGridTable(parent.Children[Index], ElementValue, parent)

			elif Type == "text":
				parent.Children[Index] = TextLine()
				parent.Children[Index].SetParent(parent)
				self.LoadElementText(parent.Children[Index], ElementValue, parent)

			elif Type == "editline":
				parent.Children[Index] = EditLine()
				parent.Children[Index].SetParent(parent)
				self.LoadElementEditLine(parent.Children[Index], ElementValue, parent)

			elif Type == "titlebar":
				parent.Children[Index] = TitleBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementTitleBar(parent.Children[Index], ElementValue, parent)

			elif Type == "horizontalbar":
				parent.Children[Index] = HorizontalBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementHorizontalBar(parent.Children[Index], ElementValue, parent)

			elif Type == "board":
				parent.Children[Index] = Board()
				parent.Children[Index].SetParent(parent)
				self.LoadElementBoard(parent.Children[Index], ElementValue, parent)

			elif Type == "board_with_titlebar":
				parent.Children[Index] = BoardWithTitleBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementBoardWithTitleBar(parent.Children[Index], ElementValue, parent)

			elif Type == "thinboard":
				parent.Children[Index] = ThinBoard()
				parent.Children[Index].SetParent(parent)
				self.LoadElementThinBoard(parent.Children[Index], ElementValue, parent)

			elif Type == "thinboard_circle":
				parent.Children[Index] = ThinBoardCircle()
				parent.Children[Index].SetParent(parent)
				self.LoadElementThinBoard(parent.Children[Index], ElementValue, parent)

			elif Type == "box":
				parent.Children[Index] = Box()
				parent.Children[Index].SetParent(parent)
				self.LoadElementBox(parent.Children[Index], ElementValue, parent)

			elif Type == "bar":
				parent.Children[Index] = Bar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementBar(parent.Children[Index], ElementValue, parent)

			elif Type == "line":
				parent.Children[Index] = Line()
				parent.Children[Index].SetParent(parent)
				self.LoadElementLine(parent.Children[Index], ElementValue, parent)

			elif Type == "slotbar":
				parent.Children[Index] = SlotBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementSlotBar(parent.Children[Index], ElementValue, parent)

			elif Type == "gauge":
				parent.Children[Index] = Gauge()
				parent.Children[Index].SetParent(parent)
				self.LoadElementGauge(parent.Children[Index], ElementValue, parent)

			elif Type == "scrollbar":
				parent.Children[Index] = ScrollBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementScrollBar(parent.Children[Index], ElementValue, parent)

			elif Type == "thin_scrollbar":
				parent.Children[Index] = ThinScrollBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementScrollBar(parent.Children[Index], ElementValue, parent)

			elif Type == "small_thin_scrollbar":
				parent.Children[Index] = SmallThinScrollBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementScrollBar(parent.Children[Index], ElementValue, parent)

			elif Type == "sliderbar":
				parent.Children[Index] = SliderBar()
				parent.Children[Index].SetParent(parent)
				self.LoadElementSliderBar(parent.Children[Index], ElementValue, parent)

			elif Type == "listbox":
				parent.Children[Index] = ListBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementListBox(parent.Children[Index], ElementValue, parent)

			elif Type == "listbox2":
				parent.Children[Index] = ListBox2()
				parent.Children[Index].SetParent(parent)
				self.LoadElementListBox2(parent.Children[Index], ElementValue, parent)

			elif Type == "listboxex":
				parent.Children[Index] = ListBoxEx()
				parent.Children[Index].SetParent(parent)
				self.LoadElementListBoxEx(parent.Children[Index], ElementValue, parent)

			elif Type == "editline_centered":
				parent.Children[Index] = EditLineCentered()
				parent.Children[Index].SetParent(parent)
				self.LoadElementEditLine(parent.Children[Index], ElementValue, parent)

			elif Type == "combobox":
				parent.Children[Index] = ComboBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementComboBox(parent.Children[Index], ElementValue, parent)

			elif Type == "scrollwindow":
				parent.Children[Index] = ScrollWindow()
				parent.Children[Index].SetParent(parent)
				self.LoadElementWindow(parent.Children[Index], ElementValue, parent)

			elif Type == "boxed-board":
				parent.Children[Index] = BoxedBoard()
				parent.Children[Index].SetParent(parent)
				self.LoadElementBoxedBoard(parent.Children[Index], ElementValue, parent)

			elif Type == "line-box":
				parent.Children[Index] = LineBox()
				parent.Children[Index].SetParent(parent)
				self.LoadElementLine(parent.Children[Index], ElementValue, parent)

			elif Type == "toggle-switch":
				parent.Children[Index] = ToggleSwitch()
				parent.Children[Index].SetParent(parent)
				self.LoadElementToggleSwitch(parent.Children[Index], ElementValue, parent)

			elif Type == "tab-board":
				parent.Children[Index] = TabBoard()
				parent.Children[Index].SetParent(parent)
				self.LoadElementTabBoard(parent.Children[Index], ElementValue, parent)

			elif Type == "drop-down":
				parent.Children[Index] = DropDownBox(parent)
				self.LoadElementDropDownBox(parent.Children[Index], ElementValue, parent)

			else:
				Index += 1
				continue

			parent.Children[Index].SetWindowName(Name)
			if 0 != self.InsertFunction:
				self.InsertFunction(Name, parent.Children[Index])

			self.LoadChildren(parent.Children[Index], ElementValue)
			Index += 1

	def CheckKeyList(self, name, value, key_list):
		for DataKey in key_list:
			if False == value.has_key(DataKey):
				print("Failed to find data key", "[" + name + "/" + DataKey + "]")
				return False

		return True

	def LoadDefaultData(self, window, value, parentWindow):
		loc_x = int(value["x"])
		loc_y = int(value["y"])
		if value.has_key("vertical_align"):
			if "center" == value["vertical_align"]:
				window.SetWindowVerticalAlignCenter()
			elif "bottom" == value["vertical_align"]:
				window.SetWindowVerticalAlignBottom()

		if parentWindow.IsRTL():
			loc_x = int(value["x"]) + window.GetWidth()
			if value.has_key("horizontal_align"):
				if "center" == value["horizontal_align"]:
					window.SetWindowHorizontalAlignCenter()
					loc_x = - int(value["x"])
				elif "right" == value["horizontal_align"]:
					window.SetWindowHorizontalAlignLeft()
					loc_x = int(value["x"]) - window.GetWidth()
					## loc_x = parentWindow.GetWidth() - int(value["x"]) + window.GetWidth()
			else:
				window.SetWindowHorizontalAlignRight()

			if value.has_key("all_align"):
				window.SetWindowVerticalAlignCenter()
				window.SetWindowHorizontalAlignCenter()
				loc_x = - int(value["x"])
		else:
			if value.has_key("horizontal_align"):
				if "center" == value["horizontal_align"]:
					window.SetWindowHorizontalAlignCenter()
				elif "right" == value["horizontal_align"]:
					window.SetWindowHorizontalAlignRight()

		if "content_window" in value:
			# TODO: Maybe we should check the window type (ScrollWindow)
			parentWindow.SetContentWindow(window)
			parentWindow.AutoFitWidth()
			parentWindow.OnContentWindowSizeChange()
			
		window.SetPosition(loc_x, loc_y)
		window.Show()

	## Window
	def LoadElementWindow(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.WINDOW_KEY_LIST):
			return False

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Button
	def LoadElementButton(self, window, value, parentWindow):

		if value.has_key("width") and value.has_key("height"):
			window.SetSize(int(value["width"]), int(value["height"]))

		if True == value.has_key("default_image"):
			window.SetUpVisual(value["default_image"])
		if True == value.has_key("over_image"):
			window.SetOverVisual(value["over_image"])
		if True == value.has_key("down_image"):
			window.SetDownVisual(value["down_image"])
		if True == value.has_key("disable_image"):
			window.SetDisableVisual(value["disable_image"])

		if True == value.has_key("text"):
			is_bold = value.has_key("bold") and value["bold"]
			font = value["font"] if value.has_key("font") else "default"
			if True == value.has_key("text_height"):
				window.SetText(value["text"], value["text_height"], is_bold, font)
			else:
				window.SetText(value["text"], isBold=is_bold, font=font)

			if value.has_key("text_color"):
				window.SetTextColor(value["text_color"])

		if True == value.has_key("tooltip_text"):
			if True == value.has_key("tooltip_x") and True == value.has_key("tooltip_y"):
				window.SetToolTipText(value["tooltip_text"], int(value["tooltip_x"]), int(value["tooltip_y"]))
			else:
				window.SetToolTipText(value["tooltip_text"])

		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Mark
	def LoadElementMark(self, window, value, parentWindow):

		#if False == self.CheckKeyList(value["name"], value, self.MARK_KEY_LIST):
		#	return False

		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Image
	def LoadElementImage(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.IMAGE_KEY_LIST):
			return False

		window.LoadImage(value["image"])
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## AniImage
	def LoadElementAniImage(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.ANI_IMAGE_KEY_LIST):
			return False

		if True == value.has_key("delay"):
			window.SetDelay(value["delay"])

		for image in value["images"]:
			window.AppendImage(image)

		if value.has_key("width") and value.has_key("height"):
			window.SetSize(value["width"], value["height"])

		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Expanded Image
	def LoadElementExpandedImage(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.EXPANDED_IMAGE_KEY_LIST):
			return False

		window.LoadImage(value["image"])

		if True == value.has_key("x_origin") and True == value.has_key("y_origin"):
			window.SetOrigin(float(value["x_origin"]), float(value["y_origin"]))

		if True == value.has_key("x_scale") and True == value.has_key("y_scale"):
			window.SetScale(float(value["x_scale"]), float(value["y_scale"]))

		if True == value.has_key("rect"):
			RenderingRect = value["rect"]
			window.SetRenderingRect(RenderingRect[0], RenderingRect[1], RenderingRect[2], RenderingRect[3])

		if True == value.has_key("mode"):
			mode = value["mode"]
			if "MODULATE" == mode:
				window.SetRenderingMode(wndMgr.RENDERING_MODE_MODULATE)

		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Slot
	def LoadElementSlot(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.SLOT_KEY_LIST):
			return False

		global_x = int(value["x"])
		global_y = int(value["y"])
		global_width = int(value["width"])
		global_height = int(value["height"])

		window.SetPosition(global_x, global_y)
		window.SetSize(global_width, global_height)
		window.Show()

		r = 1.0
		g = 1.0
		b = 1.0
		a = 1.0

		if True == value.has_key("image_r") and \
			True == value.has_key("image_g") and \
			True == value.has_key("image_b") and \
			True == value.has_key("image_a"):
			r = float(value["image_r"])
			g = float(value["image_g"])
			b = float(value["image_b"])
			a = float(value["image_a"])

		SLOT_ONE_KEY_LIST = ("index", "x", "y", "width", "height")

		for slot in value["slot"]:
			if True == self.CheckKeyList(value["name"] + " - one", slot, SLOT_ONE_KEY_LIST):
				wndMgr.AppendSlot(window.hWnd,
									int(slot["index"]),
									int(slot["x"]),
									int(slot["y"]),
									int(slot["width"]),
									int(slot["height"]))

		if True == value.has_key("image"):
			wndMgr.SetSlotBaseImage(window.hWnd,
									value["image"],
									r, g, b, a)

		self.LoadDefaultData(window, value, parentWindow)
		return True

	def LoadElementCandidateList(self, window, value, parentWindow):
		if False == self.CheckKeyList(value["name"], value, self.CANDIDATE_LIST_KEY_LIST):
			return False

		window.SetPosition(int(value["x"]), int(value["y"]))
		window.SetItemSize(int(value["item_xsize"]), int(value["item_ysize"]))
		window.SetItemStep(int(value["item_step"]))
		window.Show()

		return True

	## Table
	def LoadElementGridTable(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.GRID_TABLE_KEY_LIST):
			return False

		xBlank = 0
		yBlank = 0
		if True == value.has_key("x_blank"):
			xBlank = int(value["x_blank"])
		if True == value.has_key("y_blank"):
			yBlank = int(value["y_blank"])

		if localeInfo.IsARABIC():
			pass
		else:
			window.SetPosition(int(value["x"]), int(value["y"]))

		window.ArrangeSlot(	int(value["start_index"]),
							int(value["x_count"]),
							int(value["y_count"]),
							int(value["x_step"]),
							int(value["y_step"]),
							xBlank,
							yBlank)
		if True == value.has_key("image"):
			r = 1.0
			g = 1.0
			b = 1.0
			a = 1.0
			if True == value.has_key("image_r") and \
				True == value.has_key("image_g") and \
				True == value.has_key("image_b") and \
				True == value.has_key("image_a"):
				r = float(value["image_r"])
				g = float(value["image_g"])
				b = float(value["image_b"])
				a = float(value["image_a"])
			wndMgr.SetSlotBaseImage(window.hWnd, value["image"], r, g, b, a)

		if True == value.has_key("style"):
			if "select" == value["style"]:
				wndMgr.SetSlotStyle(window.hWnd, wndMgr.SLOT_STYLE_SELECT)
		if localeInfo.IsARABIC():
			self.LoadDefaultData(window, value, parentWindow)
		else:
			window.Show()

		return True

	## Text
	def LoadElementText(self, window, value, parentWindow):

		if value.has_key("fontsize"):
			fontSize = value["fontsize"]

			if "LARGE" == fontSize:
				window.SetLargeFont()

			if "SMALL" == fontSize:
				window.SetSmallFont()

		elif value.has_key("fontname"):
			fontName = value["fontname"]
			window.SetFontName(fontName)

		if value.has_key("text_horizontal_align"):
			if "left" == value["text_horizontal_align"]:
				window.SetHorizontalAlignLeft()
			elif "center" == value["text_horizontal_align"]:
				window.SetHorizontalAlignCenter()
			elif "right" == value["text_horizontal_align"]:
				window.SetHorizontalAlignRight()

		if value.has_key("text_vertical_align"):
			if "top" == value["text_vertical_align"]:
				window.SetVerticalAlignTop()
			elif "center" == value["text_vertical_align"]:
				window.SetVerticalAlignCenter()
			elif "bottom" == value["text_vertical_align"]:
				window.SetVerticalAlignBottom()

		if value.has_key("all_align"):
			window.SetHorizontalAlignCenter()
			window.SetVerticalAlignCenter()
			window.SetWindowHorizontalAlignCenter()
			window.SetWindowVerticalAlignCenter()

		if value.has_key("r") and value.has_key("g") and value.has_key("b"):
			window.SetFontColor(float(value["r"]), float(value["g"]), float(value["b"]))
		elif value.has_key("color"):
			window.SetPackedFontColor(value["color"])
		else:
			window.SetFontColor(0.8549, 0.8549, 0.8549)

		isBold = value["bold"] if value.has_key("bold") else False
		isItalic = value["italic"] if value.has_key("italic") else False
		isUnderline = value["underline"] if value.has_key("underline") else False
		isStrikeOut = value["strike_out"] if value.has_key("strike_out") else False

		if isBold:
			window.SetBold()

		if isItalic:
			window.SetItalic()

		if isUnderline:
			window.SetUnderline()

		if isStrikeOut:
			window.SetStrikeOut()

		if value.has_key("outline"):
			if value["outline"]:
				window.SetOutline()
		if True == value.has_key("text"):
			window.SetText(value["text"])

		self.LoadDefaultData(window, value, parentWindow)

		return True

	## EditLine
	def LoadElementEditLine(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.EDIT_LINE_KEY_LIST):
			return False


		if value.has_key("secret_flag"):
			window.SetSecret(value["secret_flag"])
		if value.has_key("placeholder"):
			window.SetPlaceholder(value["placeholder"])
		if value.has_key("with_codepage"):
			if value["with_codepage"]:
				window.bCodePage = True
		if value.has_key("only_number"):
			if value["only_number"]:
				window.SetNumberMode()
		if value.has_key("enable_codepage"):
			window.SetIMEFlag(value["enable_codepage"])
		if value.has_key("enable_ime"):
			window.SetIMEFlag(value["enable_ime"])
		if value.has_key("limit_width"):
			window.SetLimitWidth(value["limit_width"])
		if value.has_key("multi_line"):
			if value["multi_line"]:
				window.SetMultiLine()

		window.SetMax(int(value["input_limit"]))
		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadElementText(window, value, parentWindow)

		return True

	## TitleBar
	def LoadElementTitleBar(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.TITLE_BAR_KEY_LIST):
			return False

		window.MakeTitleBar(int(value["width"]), value.get("color", "red"))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## HorizontalBar
	def LoadElementHorizontalBar(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.HORIZONTAL_BAR_KEY_LIST):
			return False

		skin = "default"
		if value.has_key("skin"):
			skin = value["skin"]

		window.Create(int(value["width"]), skin)
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Board
	def LoadElementBoard(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.BOARD_KEY_LIST):
			return False

		if value.has_key("skin"):
			window.MakeBoard(value["skin"])
			window.MakeBase()
		else:
			window.MakeBoard()
			window.MakeBase()

		if value.has_key("alpha"):
			window.SetAlpha(value["alpha"])

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Board With TitleBar
	def LoadElementBoardWithTitleBar(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.BOARD_WITH_TITLEBAR_KEY_LIST):
			return False

		window.SetSize(int(value["width"]), int(value["height"]))
		window.SetTitleName(value["title"])
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## ThinBoard
	def LoadElementThinBoard(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.BOARD_KEY_LIST):
			return False

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Box
	def LoadElementBox(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.BOX_KEY_LIST):
			return False

		if True == value.has_key("color"):
			window.SetColor(value["color"])

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Bar
	def LoadElementBar(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.BAR_KEY_LIST):
			return False

		if True == value.has_key("color"):
			window.SetColor(value["color"])

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Line
	def LoadElementLine(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.LINE_KEY_LIST):
			return False

		if True == value.has_key("color"):
			window.SetColor(value["color"])

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Slot
	def LoadElementSlotBar(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.SLOTBAR_KEY_LIST):
			return False

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## Gauge
	def LoadElementGauge(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.GAUGE_KEY_LIST):
			return False

		window.MakeGauge(value["width"], value["color"])
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## ScrollBar
	def LoadElementScrollBar(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.SCROLLBAR_KEY_LIST):
			return False

		window.SetScrollBarSize(value["size"])
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## SliderBar
	def LoadElementSliderBar(self, window, value, parentWindow):

		self.LoadDefaultData(window, value, parentWindow)

		return True

	## ListBox
	def LoadElementListBox(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.LIST_BOX_KEY_LIST):
			return False

		if value.has_key("item_align"):
			window.SetTextCenterAlign(value["item_align"])

		window.SetSize(value["width"], value["height"])
		self.LoadDefaultData(window, value, parentWindow)

		return True

	## ListBox2
	def LoadElementListBox2(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.LIST_BOX_KEY_LIST):
			return False

		window.SetRowCount(value.get("row_count", 10))
		window.SetSize(value["width"], value["height"])
		self.LoadDefaultData(window, value, parentWindow)

		if value.has_key("item_align"):
			window.SetTextCenterAlign(value["item_align"])

		return True

	def LoadElementListBoxEx(self, window, value, parentWindow):
		if False == self.CheckKeyList(value["name"], value, self.LIST_BOX_KEY_LIST):
			return False

		window.SetSize(value["width"], value["height"])
		self.LoadDefaultData(window, value, parentWindow)

		if value.has_key("itemsize_x") and value.has_key("itemsize_y"):
			window.SetItemSize(int(value["itemsize_x"]), int(value["itemsize_y"]))

		if value.has_key("itemstep"):
			window.SetItemStep(int(value["itemstep"]))

		if value.has_key("viewcount"):
			window.SetViewItemCount(int(value["viewcount"]))

		return True

	def LoadElementComboBox(self, window, value, parentWindow):
		if False == self.CheckKeyList(value["name"], value, self.COMBO_BOX_KEY_LIST):
			return False

		window.SetSize(value["width"], value["height"])
		self.LoadDefaultData(window, value, parentWindow)

		if value.has_key("default_text"):
			window.SetCurrentItem(value["default_text"])

		if value.has_key("item"):
			for i in range(len(value["item"])):
				item = value["item"][i]
				window.InsertItem(i, item["value"], item["color"] if item.has_key("color") else None)
		return True

	def LoadElementBoxedBoard(self, window, value, parentWindow):
		if not self.CheckKeyList(value["name"], value, self.WINDOW_KEY_LIST):
			return False

		window.SetSize(value["width"], value["height"])

		if value.has_key("border_color"):
			window.SetBorderColor(value["border_color"])

		if value.has_key("border_size"):
			window.SetBorderSize(value["border_size"])

		if value.has_key("base_color"):
			window.SetBaseColor(value["base_color"])

		self.LoadDefaultData(window, value, parentWindow)
		return True

	def LoadElementToggleSwitch(self, window, value, parentWindow):
		if not self.CheckKeyList(value["name"], value, self.WINDOW_KEY_LIST):
			return False

		window.SetSize(value["width"], value["height"])

		# if value.has_key("on_image"):
		# 	window.SetOnImage(value["on_image"])

		# if value.has_key("off_image"):
		# 	window.SetOffImage(value["off_image"])

		self.LoadDefaultData(window, value, parentWindow)
		return True

	def LoadElementTabBoard(self, window, value, parentWindow):

		if False == self.CheckKeyList(value["name"], value, self.BOARD_KEY_LIST):
			return False

		if value.has_key("tabs_width"):
			window.SetTabWidth(value["tabs_width"])

		if value.has_key("tabs"):
			for tab in value["tabs"]:
				window.AppendTab(tab)

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		return True
		
	def LoadElementDropDownBox(self, window, value, parentWindow):
		if not self.CheckKeyList(value["name"], value, self.WINDOW_KEY_LIST):
			return False

		window.SetSize(int(value["width"]), int(value["height"]))
		self.LoadDefaultData(window, value, parentWindow)

		if value.has_key("item_step"):
			window.SetItemStep(int(value["item_step"]))

		if value.has_key("item_height"):
			window.SetItemHeight(int(value["item_height"]))

		if value.has_key("max_height"):
			window.SetMaxHeight(int(value["max_height"]))

		if value.has_key("drop_speed"):
			window.SetSpeed(int(value["drop_speed"]))

		if value.has_key("items"):
			for item in value["items"]:
				window.AppendItem(*item)

			if value.has_key("default_item"):
				window.SelectItem(int(value["default_item"]))

		return True

class ReadingWnd(Bar):

	def __init__(self):
		Bar.__init__(self,"TOP_MOST")

		self.__BuildText()
		self.SetSize(80, 19)
		self.Show()

	def __del__(self):
		Bar.__del__(self)

	def __BuildText(self):
		self.text = TextLine()
		self.text.SetParent(self)
		self.text.SetPosition(4, 3)
		self.text.Show()

	def SetText(self, text):
		self.text.SetText(text)

	def SetReadingPosition(self, x, y):
		xPos = x + 2
		yPos = y - self.GetHeight() - 2
		self.SetPosition(xPos, yPos)

	def SetTextColor(self, color):
		self.text.SetPackedFontColor(color)

class EditLineCentered(EditLine):
	def __init__(self):
		EditLine.__init__(self)
		self.basePos = (0, 0)

	def __del__(self):
		EditLine.__del__(self)
		del self.basePos

	def SetFocus(self):
		EditLine.SetFocus(self)
		self.AdjustTextPosition()

	def SetPosition(self, x, y):
		EditLine.SetPosition(self, x, y)
		self.basePos = (x, y)
		self.AdjustTextPosition()

	def OnIMEUpdate(self):
		EditLine.OnIMEUpdate(self)
		self.AdjustTextPosition()

	def SetText(self, text):
		EditLine.SetText(self, text)
		self.AdjustTextPosition()

	def OnMouseLeftButtonDown(self):
		if self.IsIn():
			EditLine.SetFocus(self)
			ime.SetCursorPosition(wndMgr.GetCursorPosition(self.hWnd))

	def AdjustTextPosition(self):
		(textX, textY) = EditLine.GetTextSize(self)
		(locX, locY) = self.basePos
		TextLine.SetPosition(self, locX + (self.GetWidth() / 2) - textX / 2, locY)

def MakeSlotBar(parent, x, y, width, height):
	slotBar = SlotBar()
	slotBar.SetParent(parent)
	slotBar.SetSize(width, height)
	slotBar.SetPosition(x, y)
	slotBar.Show()
	return slotBar

def MakeImageBox(parent, name, x, y):
	image = ImageBox()
	image.SetParent(parent)
	image.LoadImage(name)
	image.SetPosition(x, y)
	image.Show()
	return image

def MakeTextLine(parent):
	textLine = TextLine()
	textLine.SetParent(parent)
	textLine.SetWindowHorizontalAlignCenter()
	textLine.SetWindowVerticalAlignCenter()
	textLine.SetHorizontalAlignCenter()
	textLine.SetVerticalAlignCenter()
	textLine.Show()
	return textLine

def MakeButton(parent, x, y, tooltipText, path, up, over, down):
	button = Button()
	button.SetParent(parent)
	button.SetPosition(x, y)
	button.SetUpVisual(path + up)
	button.SetOverVisual(path + over)
	button.SetDownVisual(path + down)
	button.SetToolTipText(tooltipText)
	button.Show()
	return button

def MakeText(parent, textlineText, x, y, color):
	textline = TextLine()
	if parent:
		textline.SetParent(parent)
	textline.SetPosition(x, y)
	if color:
		textline.SetFontColor(color[0], color[1], color[2])
	textline.SetText(textlineText)
	textline.Show()
	return textline

def MakeThinBoardCircle(parent, x, y, width, heigh, text, bnsId = 0):
	thin = RadioButton()
	thin.SetParent(parent)
	thin.SetSize(width, heigh)
	thin.SetPosition(x, y)
	thin.SetText(text)
	thin.SetBonusId(bnsId)
	thin.Show()
	return thin

def MakeRadioButton(parent, x, y, path, up, over, down):
	button = RadioButton()
	button.SetParent(parent)
	button.SetPosition(x, y)
	button.SetUpVisual(path + up)
	button.SetOverVisual(path + over)
	button.SetDownVisual(path + down)
	button.Show()
	return button

def RenderRoundBox(x, y, width, height, color):
	grp.SetColor(color)
	grp.RenderLine(x+2, y, width-3, 0)
	grp.RenderLine(x+2, y+height, width-3, 0)
	grp.RenderLine(x, y+2, 0, height-4)
	grp.RenderLine(x+width, y+1, 0, height-3)
	grp.RenderLine(x, y+2, 2, -2)
	grp.RenderLine(x, y+height-2, 2, 2)
	grp.RenderLine(x+width-2, y, 2, 2)
	grp.RenderLine(x+width-2, y+height, 2, -2)

def GenerateColor(r, g, b):
	r = float(r) / 255.0
	g = float(g) / 255.0
	b = float(b) / 255.0
	return grp.GenerateColor(r, g, b, 1.0)

def EnablePaste(flag):
	ime.EnablePaste(flag)

def GetHyperlink():
	return wndMgr.GetHyperlink()

def MakeGridSlot(parent, xCount, yCount):
	defStep = 32
	defImage = "d:/ymir work/ui/public/Slot_Base.sub"
	gridSlot = GridSlotWindow()
	
	if parent != None:
		gridSlot.SetParent(parent)

	gridSlot.ArrangeSlot(0, xCount, yCount, defStep, defStep, 0, 0)
	gridSlot.SetSlotBaseImage(defImage, 1.0, 1.0, 1.0, 1.0)
	gridSlot.Show()
	return gridSlot

class SimplyWindow(Window):
	def __init__(self, layer, flags, width, height, initializeMethod = None, destroyMethod = None):
		Window.__init__(self, layer)
		self.children		= {}
		self.windowConfig	= {
			"size"	:	{
				"width"		:	width,
				"height"	:	height
			},
			"flag"	:	flags,
		}

		if destroyMethod:
			self.destroyMethod = __mem_func__(destroyMethod)

		if initializeMethod:
			self.Initialize(__mem_func__(initializeMethod))

	def __del__(self):
		Window.__del__(self)

	def AppendObject(self, key, child, multiply = False):
		parent = self.children.get(key, None)

		if isinstance(parent, (list, tuple)) and parent != None:
			self.children[key].append(child)
		else:
			if multiply == True:
				self.children[key] = [child]
			else:
				self.children[key] = child

	def DeleteObject(self, key):
		if key in self.children:
			del self.children[key]

	def GetObject(self, key, index = -1):
		child = self.children.get(key, None)

		if isinstance(child, (tuple, list)):
			if index != -1:
				return child[index]
			return child
		elif index != -1:
			return proxy(child[index])
		else:
			return proxy(child)

	def GetCountObject(self, key):
		try:
			return len(self.children.get(key, None))
		except (KeyError, TypeError):
			return 0

	def Initialize(self, initializeMethod):
		self.SetSize(self.windowConfig["size"]["width"], self.windowConfig["size"]["height"])
		map(self.AddFlag, self.windowConfig["flag"])
		self.SetCenterPosition()

		try:
			initializeMethod()
			return True
		except (Exception, ) as error:
			return self.PrintException(error)

	def PrintException(self, error):
		import exception
		exception.Abort(" ")
		
		self.Destroy()
		self.Close()
		return False

	def GetSize(self):
		return (self.GetWidth(), self.GetHeight(), )
	
	def Destroy(self):
		if hasattr(self, "destroyMethod"):
			self.destroyMethod()

		self.children = {}
		self.destroyMethod = None

	def Open(self):
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def OpenWindow(self):
		self.Close() if self.IsShow() == True else self.Open()

	def OnPressExitKey(self):
		self.Close()
		return True

	OnPressEscapeKey = OnPressExitKey

RegisterToolTipWindow("TEXT", TextLine)
