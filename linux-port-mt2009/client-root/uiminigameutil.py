# MT2009_PLUS_MINIGAME_UTIL_V1 - what the mini game windows (Catch the King,
# Rumi, Yutnori) need and our ui.py / exe do not give:
#
# UpdateTicker: our exe calls a python OnUpdate only on plain windows (CWindow,
# Bar, Line): CImageBox / CExpandedImageBox / CButton / CTextLine::OnUpdate do
# not pass it on. An animation or a moving card written as an ImageBox with an
# OnUpdate never moved - Catch the King's "Moja karta" effect stood on its first
# frame, its end event never came and the board stayed locked. The ticker is a
# plain child window: the exe updates it while its owner is shown, and it
# calls the owner's TickUpdate().
#
# LoadError / SafeCreate (MT2009_PLUS_WINDOW_NO_ABORT_V1): a window that does
# not load (a uiscript error, a missing child) is logged to syserr and not
# opened - Owsap's exception.Abort closed the whole client.
#
# DescriptionText (MT2009_PLUS_MINIGAME_DESC_V1): the rules text of the
# waiting pages, drawn as plain TextLines.
#
# Owsap drew the rules with the quest text renderer (event.RegisterEventSet,
# UpdateEventSet with a y that moves by the page, RenderEventSet). Our exe
# lays out the visible lines of an event set itself (USE_NEW_EVENT_TEXT_AUTO_Y)
# so Owsap's page shift moved the text out of the box (above the window on the
# second page). Here every line is a TextLine child of the description box:
# it moves with the window and stays inside the box on every page.
#
# The file is the same locale text as before (CP1250, "[ENTER]" ends a line,
# other [..] tags are dropped); a line wider than the box is wrapped by words,
# measured with the font the TextLines use.
#
# Python 2.7 as the client has it.

import app
import pack
import ui

from _weakref import proxy


class WindowLoadError(Exception):
	pass


def LoadError(where):
	"""In place of exception.Abort(where) in a window's load: the error (and
	the exception being handled, if any) goes to syserr, the load stops."""
	import sys
	import dbg
	exc = sys.exc_info()
	if exc[0] is not None and exc[0] is not WindowLoadError:
		import traceback
		dbg.TraceError("WINDOW_LOAD_ERROR %s:\n%s" % (where, "".join(traceback.format_exception(*exc))))
	else:
		dbg.TraceError("WINDOW_LOAD_ERROR %s" % where)
	raise WindowLoadError(where)


NOT_LOADED_TEXT = "Okno nie wczyta\xb3o si\xea (b\xb3\xb9d w syserr) - zg\xb3o\xb6 to administracji."


def SafeCreate(factory, name):
	"""factory() or None when the window does not load: logged, a chat line,
	the client keeps running."""
	try:
		return factory()
	except Exception:
		import sys
		import dbg
		exc = sys.exc_info()
		if exc[0] is not WindowLoadError:
			import traceback
			dbg.TraceError("WINDOW_LOAD_ERROR %s:\n%s" % (name, "".join(traceback.format_exception(*exc))))
		try:
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, "%s: %s" % (name, NOT_LOADED_TEXT))
		except Exception:
			pass
		return None


class UpdateTicker(ui.Window):
	"""A plain child window that calls owner.TickUpdate() every frame the
	owner is shown (see above)."""

	def __init__(self, owner):
		ui.Window.__init__(self)
		self.owner = proxy(owner)
		self.SetParent(owner)
		self.SetPosition(0, 0)
		self.SetSize(0, 0)
		self.AddFlag("not_pick")
		self.Show()

	def __del__(self):
		ui.Window.__del__(self)

	def OnUpdate(self):
		try:
			self.owner.TickUpdate()
		except ReferenceError:
			pass

_SKIP_TAGS = ('[DELAY', '[WAIT', '[NEXT', '[DONE', '[TEXT_')


def _ReadFile(path):
	try:
		if pack.Exist(path):
			return pack.Get(path) or ''
	except Exception:
		pass
	try:
		f = old_open(path, 'rb')
		try:
			return f.read()
		finally:
			f.close()
	except Exception:
		return ''


def _SplitLines(data):
	data = data.replace('\r\n', '\n').replace('\r', '\n')
	out = []
	for raw in data.split('\n'):
		for part in raw.split('[ENTER]'):
			out.append(part)
		if raw.endswith('[ENTER]'):
			out.pop()	# "text[ENTER]" ends a line, not one more empty line
	lines = []
	for line in out:
		text = line
		# drop the tags the quest renderer would have read ([DELAY value;0] ...)
		while '[' in text and ']' in text[text.find('['):]:
			start = text.find('[')
			end = text.find(']', start)
			text = text[:start] + text[end + 1:]
		if not text.strip() and line.strip().startswith(_SKIP_TAGS):
			continue	# a tag-only line ([DELAY value;0]) is no empty line
		lines.append(text.rstrip())
	while lines and not lines[-1]:
		lines.pop()
	return lines


class DescriptionText(ui.Window):
	"""The rules: SHOW lines per page, the page turned with Prev/Next."""

	def __init__(self, parent, x, y, width, height, line_count, line_step = 16, color = (0.7843, 0.7843, 0.7843)):
		ui.Window.__init__(self)
		self.SetParent(parent)
		self.SetPosition(x, y)
		self.SetSize(width, height)
		self.AddFlag("not_pick")
		self.line_count = max(1, line_count)
		self.line_step = line_step
		self.color = color
		self.lines = []
		self.page_start = 0
		self.text_lines = []
		for i in xrange(self.line_count):
			t = ui.TextLine()
			t.SetParent(self)
			t.SetPosition(0, i * line_step)
			t.SetFontColor(color[0], color[1], color[2])
			t.Hide()
			self.text_lines.append(t)
		self.measure = ui.TextLine()
		self.measure.Hide()

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.text_lines = []
		self.measure = None
		self.lines = []

	def __Width(self, text):
		try:
			self.measure.SetText(text)
			return self.measure.GetTextSize()[0]
		except Exception:
			return len(text) * 6

	def __Wrap(self, line):
		width = self.GetWidth()
		if not line or self.__Width(line) <= width:
			return [line]
		out = []
		cur = ''
		for word in line.split(' '):
			test = word if not cur else cur + ' ' + word
			if cur and self.__Width(test) > width:
				out.append(cur)
				cur = word
			else:
				cur = test
		if cur:
			out.append(cur)
		return out

	def LoadFile(self, path):
		self.lines = []
		for line in _SplitLines(_ReadFile(path)):
			self.lines.extend(self.__Wrap(line))
		self.page_start = 0
		self.__Refresh()

	def LoadLocaleFile(self, name):
		self.LoadFile("%s/%s" % (app.GetLocalePath(), name))

	def HasMorePages(self):
		return len(self.lines) > self.line_count

	def __Refresh(self):
		for i, t in enumerate(self.text_lines):
			idx = self.page_start + i
			if idx < len(self.lines) and self.lines[idx]:
				t.SetText(self.lines[idx])
				t.Show()
			else:
				t.SetText('')
				t.Hide()

	def PrevPage(self):
		if self.page_start <= 0:
			return
		self.page_start = max(0, self.page_start - self.line_count)
		self.__Refresh()

	def NextPage(self):
		if self.page_start + self.line_count >= len(self.lines):
			return
		self.page_start += self.line_count
		self.__Refresh()

	def FirstPage(self):
		self.page_start = 0
		self.__Refresh()
