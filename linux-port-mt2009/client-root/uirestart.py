import dbg
import app
import net

import ui

# MT2009_PLUS_DIGI_SERVER_QOL_V1 (Autor: Digi Rasta, nowy-system v0.23.0: his
# "Renewal-Dead-Packet"): the death window counts down. The core sends
# "DeadTime <here> <town>" at death (playerbot_digi_qol.h), the seconds before
# /restart_here and /restart_town go through (do_restart: 10 and 7, the portal
# limit after a fight); until then a button shows them and cannot be clicked.
# A core without it sends nothing and the buttons are as before.
DEAD_TIMES = [0.0, 0.0]

def SetDeadTimes(here, town):
	now = app.GetTime()
	DEAD_TIMES[0] = now + max(0, here)
	DEAD_TIMES[1] = now + max(0, town)

###################################################################################################
## Restart
class RestartDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.deadLabels = [None, None]  # MT2009_PLUS_DIGI_SERVER_QOL_V1

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadDialog(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/restartdialog.py")
		except Exception, msg:
			import sys
			(type, msg, tb)=sys.exc_info()
			dbg.TraceError("RestartDialog.LoadDialog - %s:%s" % (type, msg))
			app.Abort()
			return 0

		try:
			self.restartHereButton=self.GetChild("restart_here_button")
			self.restartTownButton=self.GetChild("restart_town_button")
		except:
			import sys
			(type, msg, tb)=sys.exc_info()
			dbg.TraceError("RestartDialog.LoadDialog - %s:%s" % (type, msg))
			app.Abort()
			return 0

		self.restartHereButton.SetEvent(ui.__mem_func__(self.RestartHere))
		self.restartTownButton.SetEvent(ui.__mem_func__(self.RestartTown))

		return 1

	@ui.WindowDestroy
	def Destroy(self):
		self.restartHereButton=0
		self.restartTownButton=0
		self.ClearDictionary()

	def OpenDialog(self):
		self.Show()

	def OnUpdate(self):
		# MT2009_PLUS_DIGI_SERVER_QOL_V1: the seconds to the restart on the buttons.
		import uiScriptLocale
		now = app.GetTime()
		buttons = ((self.restartHereButton, DEAD_TIMES[0], getattr(uiScriptLocale, "RESTART_HERE", "")),
			(self.restartTownButton, DEAD_TIMES[1], getattr(uiScriptLocale, "RESTART_TOWN", "")))
		for i in xrange(2):
			button, end, text = buttons[i]
			if not button:
				continue
			left = int(end - now + 0.999)
			label = left > 0 and ("%s (%d)" % (text, left)) or text
			if self.deadLabels[i] == label:
				continue
			self.deadLabels[i] = label
			button.SetText(label)
			if left > 0:
				button.Disable()
			else:
				button.Enable()

	def Close(self):
		self.Hide()
		return True

	def RestartHere(self):
		net.SendChatPacket("/restart_here")

	def RestartTown(self):
		net.SendChatPacket("/restart_town")

	def OnPressExitKey(self):
		return True

	def OnPressEscapeKey(self):
		return True
