import app
import net
import player
import ime

FIXED_TIMESTEP_UPDATE = 167
ONE_SECOND = 10000


class UpdatableAbstract:
	def __init__(self):
		pass

	def OnUpdate(self):
		pass

	def Destroy(self):
		pass

	def CanUpdate(self):
		return False


class PickUpOnDownKey(UpdatableAbstract):
	def __init__(self):
		UpdatableAbstract.__init__(self)
		self.timer = 0
		self.delay = 1500  # 0.15s

	def OnUpdate(self):
		self.timer += FIXED_TIMESTEP_UPDATE
		# With the pick-up filter on (uipickupfilter.py) the server picks up,
		# as it may once in half a second; PickCloseItem would take whatever
		# lies nearest, filtered or not.
		import uipickupfilter
		if uipickupfilter.IsActive():
			if self.timer >= 5000:
				net.SendChatPacket("/pickup_nearby")
				self.timer = 0
			return
		if self.timer >= self.delay:
			player.PickCloseItem()
			self.timer = 0

	def CanUpdate(self):
		# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion): the pick-up's own keys
		# (keybind.py: Z and ~ unless rebound; Ctrl+Z is the filter's window,
		# no pick-up).
		if ime.IsCaptureInput():
			return False
		import keybind
		return keybind.IsHeld("pickup") or keybind.IsHeld("pickup_near")
