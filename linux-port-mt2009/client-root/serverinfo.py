import app
import localeInfo
import net
from constInfo import TextColor
app.ServerName = None

# Local configuration.
# Auth: 11000; CH1: 13000; CH2: 13010.
# The remaining map cores are reached through the server's own map hand-over.
# This file is stored in pack/root.data and pack/root.index.

def GetServerID():
	serverID = 0
	for k, server_data in SERVER_LIST.items():
		if server_data["main"]["name"] == net.GetServerInfo().split(",")[0]:
			serverID = k
			break
	return serverID

SERVER_LIST = {}
def __AddServerToServerList(server_data):
	server_index = len(SERVER_LIST)

	mark_name = server_data["mark_name"]
	SERVER_LIST[server_index] = {  # serverIndex
		"main": server_data,
		"channel": {},
		"auth": {},
		"mark": {"mark": "%s.tga" % mark_name, "symbol_path": mark_name},
	}

STATE_NONE = TextColor(localeInfo.CHANNEL_STATUS_OFFLINE, "FF0000") #RED
STATE_DICT = {
	0: TextColor(localeInfo.CHANNEL_STATUS_OFFLINE, "FF0000"), 		#RED
	1: TextColor(localeInfo.CHANNEL_STATUS_RECOMMENDED, "00ff00"), 	#GREEN
	2: TextColor(localeInfo.CHANNEL_STATUS_BUSY, "ffff00"), 		#YELLOW
	3: TextColor(localeInfo.CHANNEL_STATUS_FULL, "ff8a08") 			#ORANGE
}

SERVER_LOCALHOST = {
	"name":TextColor("mt2009 localhost", "ffd500"), #GOLD
	"host":"127.0.0.1",
	"auth_base_port": 11000,
	"auth_port_increment": 0,
	"auth_port_channel_increment": 0,
	"auth_count": 1,
	"channel_base_port": 13000,
	"channel_port_increment": 10,
	"channel_count": 2,
	"mark":13000,
	"mark_name": "10",
	"premium_channels": (),
}


def __IsValidCoopHost(host):
	if not host or len(host) > 253:
		return False

	allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-"
	for character in host:
		if character not in allowed:
			return False

	# A value made only from digits and dots is an IPv4 address, so validate
	# all four octets instead of accepting malformed forms such as 999.1.1.1.
	if host.replace(".", "").isdigit():
		parts = host.split(".")
		if len(parts) != 4:
			return False
		for part in parts:
			if not part or len(part) > 3 or int(part) > 255:
				return False
		return True

	# DNS names may contain letters, digits, dots and hyphens. Each label must
	# start and end with a letter or digit.
	labels = host.split(".")
	for label in labels:
		if not label or len(label) > 63:
			return False
		if not label[0].isalnum() or not label[-1].isalnum():
			return False
	return True


# coop.cfg and coop2.cfg: up to two worlds besides localhost, each file one
# world in the same format (Ustaw_serwery.bat writes both).
def __LoadCoopServer(path="coop.cfg", mark_name="10"):
	try:
		coop_file = open(path, "r")
		try:
			contents = coop_file.read(8193)
		finally:
			coop_file.close()

		if len(contents) > 8192:
			return None

		settings = {}
		for raw_line in contents.splitlines():
			line = raw_line.strip()
			if not line or line.startswith("#"):
				continue
			if "=" not in line:
				return None
			key, value = line.split("=", 1)
			key = key.strip().lower()
			value = value.strip()
			if not key or key in settings:
				return None
			settings[key] = value

		for required_key in ("name", "host", "auth", "channel", "channels"):
			if required_key not in settings or not settings[required_key]:
				return None

		name = settings["name"]
		host = settings["host"]
		auth_port = int(settings["auth"])
		channel_port = int(settings["channel"])
		channel_count = int(settings["channels"])

		if "\x00" in name or not __IsValidCoopHost(host):
			return None
		if auth_port < 1 or auth_port > 65535:
			return None
		if channel_port < 1 or channel_port > 65535:
			return None
		if channel_count < 1 or channel_count > 2:
			return None
		if channel_port + (channel_count - 1) * 10 > 65535:
			return None

		return {
			"name": TextColor("Online: " + name, "57c7ff"), #LIGHT BLUE
			"host": host,
			"auth_base_port": auth_port,
			"auth_port_increment": 0,
			"auth_port_channel_increment": 0,
			"auth_count": 1,
			"channel_base_port": channel_port,
			"channel_port_increment": 10,
			"channel_count": channel_count,
			"mark": channel_port,
			"mark_name": mark_name,
			"premium_channels": (),
		}
	# coop.cfg is optional and must never prevent the client from starting.
	except:
		return None


__AddServerToServerList(SERVER_LOCALHOST)

# Each world keeps its own guild-mark cache: with the local world's "10" a
# friend's world drew the host's marks (the fix Tieru's client carried).
SERVER_COOP = __LoadCoopServer("coop.cfg", "20")
if SERVER_COOP:
	__AddServerToServerList(SERVER_COOP)

SERVER_COOP2 = __LoadCoopServer("coop2.cfg", "30")
if SERVER_COOP2:
	__AddServerToServerList(SERVER_COOP2)


## channel data
for server_id, server_data in SERVER_LIST.items():
	for i in range(server_data["main"]["channel_count"]):
		channelIndex = i+1
		channelPort = server_data["main"]["channel_base_port"] + server_data["main"]["channel_port_increment"] * i
		isPremium = channelIndex in server_data["main"]["premium_channels"]
		server_data["channel"][i] = {
			"name": TextColor("CH%d%s" % (channelIndex, " |Eother/premium|e" if isPremium else ""), "FFffFF"),
			"ip": server_data["main"]["host"],
			"tcp_port": channelPort,
			"udp_port": channelPort,
			"state": STATE_NONE,
		}

## auth data
for server_id, server_data in SERVER_LIST.items():
	for i in range(server_data["main"]["channel_count"]):
		server_data["auth"][i] = {
			"ip": server_data["main"]["host"],
			"port": [],
		}

		for j in range(server_data["main"]["auth_count"]):
			authNumber = j + 1
			port = server_data["main"]["auth_base_port"] + server_data["main"]["auth_port_channel_increment"] * i + server_data["main"]["auth_port_increment"] * authNumber
			server_data["auth"][i]["port"].append(port)
