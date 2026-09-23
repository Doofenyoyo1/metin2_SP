#!/usr/bin/env python3
import hashlib,json,os,subprocess,sys,tempfile,urllib.request,zipfile
ROOT=os.environ.get("SEBAN_M2_ROOT", "/opt/metin2-mt2009/mt2009-r41023-base"); COMPOSE=os.path.join(ROOT,"linux-port","docker")
OVERRIDE_DIR=os.environ.get("SEBAN_OVERRIDE_DIR", os.path.dirname(os.path.abspath(__file__)))
HOOK=os.path.join(OVERRIDE_DIR,"apply-seban-overrides.sh")
PROJECT=os.environ.get("SEBAN_COMPOSE_PROJECT", "metin2")
UPDATE_PANEL=os.environ.get("SEBAN_UPDATE_PANEL", "0") == "1"
MANIFEST="https://raw.githubusercontent.com/TieruYT/metin2-playerbots/main/update-manifest-mt2009.json"
SPOOL=os.environ.get("SEBAN_UPDATE_SPOOL", "/var/lib/docker/volumes/metin2_update-spool/_data")
def progress(step, message):
    tmp=os.path.join(SPOOL, "update.status.new")
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(f"state=running\ntime={int(__import__('time').time())}\nstep={step}\nsteps=5\nmessage={message}\nversion={version()}\n")
        os.replace(tmp, os.path.join(SPOOL, "update.status"))
    except OSError:
        pass
def fetch(url): return urllib.request.urlopen(urllib.request.Request(url,headers={"User-Agent":"seban-mt2009-updater/1"}),timeout=180).read()
def version(): return open(os.path.join(ROOT,"VERSION"),encoding="utf-8").read().strip()
def version_key(value):
    try: return tuple(int(part) for part in value.strip().split("."))
    except (AttributeError, ValueError): return ()
def running_panel_version():
    found=subprocess.run(["docker","compose","-p",PROJECT,"ps","-q","seban-panel"],cwd=COMPOSE,text=True,capture_output=True,check=False).stdout.strip()
    if not found: return ""
    return subprocess.run(["docker","exec",found.splitlines()[0],"cat","/app/VERSION"],text=True,capture_output=True,check=False).stdout.strip()
m=json.loads(fetch(MANIFEST))["server"]; current=version()
print(f"Installed: {current}; available: {m['version']}")
if "--check" in sys.argv: sys.exit(0)
if current==m["version"]: print("Already current."); sys.exit(0)
with tempfile.TemporaryDirectory(prefix="seban-mt2009-") as d:
 progress(2, "Downloading and validating the update package.")
 pkg=os.path.join(d,"update.zip"); open(pkg,"wb").write(fetch(m["url"]))
 if hashlib.sha256(open(pkg,"rb").read()).hexdigest().lower()!=m["sha256"].lower(): raise SystemExit("ERROR: SHA-256 mismatch.")
 progress(3, "Unpacking the MT2009 update.")
 with zipfile.ZipFile(pkg) as z:
  for info in z.infolist():
   n=info.filename.replace("\\","/")
   if n.endswith("/") or n.startswith("/") or ".." in n.split("/"): continue
   p=os.path.join(ROOT,*n.split("/")); os.makedirs(os.path.dirname(p),exist_ok=True)
   with z.open(info) as src,open(p,"wb") as dst: dst.write(src.read())
   if n.endswith(".sh"): os.chmod(p,0o755)
progress(4, "Applying Seban overrides and rebuilding services.")
subprocess.run([HOOK,ROOT],check=True)
print("Seban overrides applied from the saved panel settings.")
services=["mariadb","playerbot-migrate","game","panel","itemshop"]
panel_updated=False
if UPDATE_PANEL:
 panel_source=os.path.join(COMPOSE,"seban-panel")
 if not os.path.isdir(panel_source):
  raise SystemExit(f"ERROR: Seban Panel update requested, but the Tieru package has no {panel_source}.")
 bundled_version=open(os.path.join(panel_source,"VERSION"),encoding="utf-8").read().strip()
 installed_panel_version=running_panel_version()
 if version_key(installed_panel_version) and version_key(bundled_version) <= version_key(installed_panel_version):
  print(f"Seban Panel update skipped: bundled {bundled_version}, installed {installed_panel_version}. Downgrades and same-version rebuilds are blocked.")
 else:
  services.extend(["seban-panel","seban-collector","seban-item-grants"])
  panel_updated=True
  print(f"Seban Panel update enabled: {installed_panel_version or 'unknown'} -> {bundled_version} (version bundled by Tieru).")
else:
 print("Seban Panel update disabled: keeping the currently installed version.")
subprocess.run(["docker","compose","-p",PROJECT,"up","-d","--build",*services],cwd=COMPOSE,check=True)
print("Done. Playerbots updated with Seban Panel " + ("updated." if panel_updated else "kept unchanged."))
