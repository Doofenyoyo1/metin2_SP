import ui
import app
import gameSettings
import localeInfo
import uiScriptLocale
import file_utils

RULES_VERSION = 4

class GameRulesDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.LoadDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.content_list = []
		self.last_item_pos = 0

	def Destroy(self):
		self.ClearDictionary()
		self.__Initialize()

	def LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/rulesdialog.py")

			self.titleBar = self.GetChild("TitleBar")
			self.scrollBar = self.GetChild("ScrollBar")
			self.contentMask = self.GetChild("ContentMask")
			self.content = self.GetChild("Content")
			self.acceptButton = self.GetChild("AcceptButton")

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
			self.acceptButton.SAFE_SetEvent(self.OnAccept)

			self.scrollBar.SetScrollContent(self.contentMask, self.content)
			self.scrollBar.SAFE_SetOnWheelEvent(self.content)
			self.scrollBar.SetScrollEvent(ui.__mem_func__(self.__OnScroll))

			self.SetRules()

			if not self.IsAcceptRules():
				self.acceptButton.Down()
				self.acceptButton.Disable()

		except:
			import exception
			exception.Abort("GameRulesDialog.LoadDialog.BindObject")

	def __OnScroll(self):
		self.scrollBar.OnScroll()
		if self.scrollBar.GetPos() >= 1:
			self.acceptButton.Enable()
			self.acceptButton.SetUp()

	def __CreateTitle(self, number, title):
		text = ui.TextLine()
		text.SetParent(self.content)
		text.SetClippingMaskWindow(self.contentMask)
		text.SetLargeFont()
		text.SetBold()
		text.SetWindowHorizontalAlignCenter()
		text.SetHorizontalAlignCenter()
		text.SetText(localeInfo.PARAGRAPH + "%d. %s" % (number,title))
		text.SetPosition(0, self.last_rule_pos)
		text.Show()
		self.last_rule_pos += 16

		for i in xrange(2):
			horizontalLine = ui.Line()
			horizontalLine.SetParent(self.content)
			horizontalLine.SetClippingMaskWindow(self.contentMask)
			horizontalLine.SetPosition(0, self.last_rule_pos + 3 + i)
			horizontalLine.SetSize(179*2, 0)
			horizontalLine.Show()

			if 0 == i:
				horizontalLine.SetColor(0xff555555)
			else:
				horizontalLine.SetColor(0xff000000)

			self.content_list.append(horizontalLine)

		self.last_rule_pos += 8

		self.content_list.append(text)
		return text

	def __CreateRule(self, rule_text):
		text = ui.TextLine()
		text.SetParent(self.content)
		text.SetClippingMaskWindow(self.contentMask)
		text.SetText(rule_text)
		text.SetPosition(3, self.last_rule_pos)
		text.Show()
		self.content_list.append(text)
		self.last_rule_pos += 12
		return text

	def SetRules(self):
		rules_content = file_utils.GetFileContent(uiScriptLocale.RULES)
		lines = rules_content.splitlines()
		self.last_rule_pos = 3
		for i in range(0, len(lines), 2):
			title_str = lines[i].strip()
			content_str = lines[i+1].strip()

			title = self.__CreateTitle((i + 2)/2, title_str)

			rules_list = content_str.split("[ENTER]")
			for rule_text in rules_list:
				word_list = rule_text.split(" ")
				line = ""
				for word in word_list:
					dest_line = line + " " + word
					if len(dest_line) > 60:
						self.__CreateRule(dest_line)
						line = ""
					else:
						line = dest_line

				if len(line) > 0:
					self.__CreateRule(line)

			self.last_rule_pos += 8

		self.content.SetHeight(self.last_rule_pos)
		self.scrollBar.ResizeScrollBar()

	def IsAcceptRules(self):
		return gameSettings.GetNumberKey("accept_rules") == RULES_VERSION

	def OnAccept(self):
		gameSettings.SetKey("accept_rules", RULES_VERSION)
		self.Close()

	def Close(self):
		if not self.IsAcceptRules():
			app.Exit()
			return
		self.Hide()

	def Open(self):
		self.Show()
		self.scrollBar.SetPos(0)
