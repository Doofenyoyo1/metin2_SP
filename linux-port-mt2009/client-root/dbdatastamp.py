# MT2009_PLUS_DBDATA_STAMP_V1 - "your item/skill files are not this server's".
#
# The Seban panel's database editor changes items and skills on the server;
# the client shows their names, bonuses and descriptions from its own
# pack/dbdata, which the player replaces with the zip from the panel
# ("Pobierz aktualne pliki klienta (zip)"). The zip carries dbdata_stamp.txt
# in the client folder: "stamp <value>" (the client base's version alone, or
# "<version>-<12 hex>" with edits) and the sizes of the pack files it came
# with ("size pack/dbdata.data <bytes>").
#
# At every entry into the game the core sends "DbDataStamp <stamp>"
# (playerbot_dbdata_stamp.h). When the server has edits (a "-" in its stamp)
# and this client's stamp is another one - or there is no dbdata_stamp.txt,
# or the pack files are no longer the zip's (a client update put the
# release's pack back) - the player gets a popup and a chat line, once a
# session (a warp or a channel change is the same session). Nothing when
# everything matches; a server without the panel's stamp sends nothing.
#
# game.py registers NOTICE.OnCommand (stringCommander keeps a weak reference
# to the bound method's object, so the object lives here) and calls Destroy()
# when the game window closes.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.
import os

STAMP_FILE = "dbdata_stamp.txt"
STAMP_MAX = 64

POPUP_LINES = (
	"Ten serwer ma zmienione przedmioty/umiej\xeatno\x9cci (Edytor bazy danych),",
	"a Tw\xf3j klient ma inne pliki. Opisy w grze mog\xb9 si\xea nie zgadza\xe6.",
	"",
	"Pobierz aktualne pliki klienta w panelu:",
	"Edytor bazy danych -> Zastosuj -> Pobierz aktualne pliki klienta (zip)",
	"i rozpakuj je do folderu gry (zast\xb9p pliki), potem uruchom gr\xea ponownie.",
)
CHAT_LINES = (
	"[Pliki klienta] Ten serwer ma zmienione przedmioty/umiej\xeatno\x9cci (Edytor bazy danych), a Tw\xf3j klient ma inne pliki. Opisy w grze mog\xb9 si\xea nie zgadza\xe6.",
	"[Pliki klienta] Pobierz aktualne pliki klienta w panelu: Edytor bazy danych -> Zastosuj -> Pobierz aktualne pliki klienta (zip) i rozpakuj je do folderu gry.",
)


def _Valid(stamp):
	if not stamp or len(stamp) > STAMP_MAX:
		return False
	for ch in stamp:
		if not (ch.isalnum() or ch in ".-_"):
			return False
	return True


def ReadLocalStamp(root=""):
	"""The stamp of this client's dbdata files: the one in dbdata_stamp.txt
	while the pack files still have the sizes it names; "" without the file
	(the release's own files) or when the pack is no longer the zip's."""
	path = os.path.join(root, STAMP_FILE) if root else STAMP_FILE
	stamp, sizes = "", []
	try:
		handle = open(path, "r")
		try:
			for line in handle.readlines():
				parts = line.split()
				if not parts or parts[0].startswith("#"):
					continue
				if parts[0] == "stamp" and len(parts) >= 2:
					stamp = parts[1]
				elif parts[0] == "size" and len(parts) >= 3:
					sizes.append((parts[1], parts[2]))
		finally:
			handle.close()
	except (IOError, OSError):
		return ""
	if not _Valid(stamp):
		return ""
	for name, size in sizes:
		target = os.path.join(root, *name.split("/")) if root else os.path.join(*name.split("/"))
		try:
			if os.path.getsize(target) != int(size):
				return ""
		except (OSError, ValueError):
			return ""
	return stamp


def NeedsNotice(serverStamp, localStamp):
	"""The server has client-visible edits and this client is not on them."""
	if not _Valid(serverStamp) or "-" not in serverStamp:
		return False
	return serverStamp != localStamp


class DbDataStampNotice:
	def __init__(self):
		self.shownFor = None
		self.popup = None

	def OnCommand(self, serverStamp="", *rest):
		try:
			if serverStamp == self.shownFor:
				return
			if not NeedsNotice(serverStamp, ReadLocalStamp()):
				return
			self.shownFor = serverStamp
			self.__Show()
		except Exception:
			# never let a notice break the game window's command handling
			try:
				import dbg
				dbg.TraceError("dbdatastamp: notice failed")
			except Exception:
				pass

	def __Show(self):
		try:
			import chat
			for line in CHAT_LINES:
				chat.AppendChat(chat.CHAT_TYPE_INFO, line)
		except Exception:
			pass
		import uiCommon
		self.Destroy()
		popup = uiCommon.PopupDialog()
		popup.SetText("[ENTER]".join(POPUP_LINES))
		popup.Open()
		self.popup = popup

	def Destroy(self):
		"""The game window closes (a warp, a channel change, the logout)."""
		popup, self.popup = self.popup, None
		if popup:
			try:
				popup.Destroy()
			except Exception:
				pass


NOTICE = DbDataStampNotice()


def Destroy():
	NOTICE.Destroy()
