#!/usr/bin/env python3
import hashlib,json,os,subprocess,sys,tempfile,urllib.request,zipfile
ROOT=os.environ.get("SEBAN_M2_ROOT", "/opt/metin2-mt2009/mt2009-r41023-base"); COMPOSE=os.path.join(ROOT,"linux-port","docker")
OVERRIDE_DIR=os.environ.get("SEBAN_OVERRIDE_DIR", os.path.dirname(os.path.abspath(__file__)))
HOOK=os.path.join(OVERRIDE_DIR,"apply-seban-overrides.sh")
PROJECT=os.environ.get("SEBAN_COMPOSE_PROJECT", "metin2")
UPDATE_PANEL=os.environ.get("SEBAN_UPDATE_PANEL", "0") == "1"
# This project's own channel, never an upstream's: their packages would
# overwrite this project's changes.
MANIFEST=os.environ.get("SEBAN_UPDATE_MANIFEST", "https://raw.githubusercontent.com/Doofenyoyo1/metin2_SP/main/update-manifest-mt2009.json")
if "TieruYT/metin2-playerbots" in MANIFEST or "zaxerrrr-dot/mt2009-sp-plus" in MANIFEST: raise SystemExit("ERROR: SEBAN_UPDATE_MANIFEST wskazuje repozytorium projektu zrodlowego; ta paczka aktualizuje sie tylko z repozytorium Metin2 SinglePlayer.")
SPOOL=os.environ.get("SEBAN_UPDATE_SPOOL", "/var/lib/docker/volumes/metin2_update-spool/_data")
def progress(step, message):
    tmp=os.path.join(SPOOL, "update.status.new")
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(f"state=running\ntime={int(__import__('time').time())}\nstep={step}\nsteps=5\nmessage={message}\nversion={version()}\n")
        os.replace(tmp, os.path.join(SPOOL, "update.status"))
    except OSError:
        pass
def fetch(url, timeout=180): return urllib.request.urlopen(urllib.request.Request(url,headers={"User-Agent":"seban-mt2009-updater/1"}),timeout=timeout).read()
# MT2009_PLUS_UPDATE_MIRROR_V1: the fallback update source. When GitHub does not
# answer (network error, non-200, not JSON) the manifest and the zip are read
# from here under the same file names; the zip passes the same SHA-256 check.
# Space-separated bases, empty turns it off; a manifest's "mirrors" go first.
# No default: MT2009 PLUS's mirror carries its packages, not this project's.
MIRRORS=os.environ.get("SEBAN_UPDATE_MIRROR", "").split()
MIRROR_NOTICE="GitHub niedostępny - pobieram z serwera zapasowego"
def mirror_urls(url, extra=()):
    name=url.split("?",1)[0].rstrip("/").rsplit("/",1)[-1]
    if not name or not all(c.isalnum() or c in "._-" for c in name): return []
    out=[]
    for base in list(extra)+MIRRORS:
        if not isinstance(base,str) or not base.startswith(("http://","https://")): continue
        u=(base if base.endswith("/") else base+"/")+name
        if u!=url and u not in out: out.append(u)
    return out
def fetch_manifest():
    try:
        doc=json.loads(fetch(MANIFEST, timeout=15).decode("utf-8-sig"))
        if isinstance(doc,dict): return doc
    except Exception as e:
        print(f"GitHub: {e}")
    for u in mirror_urls(MANIFEST):
        try:
            doc=json.loads(fetch(u, timeout=30).decode("utf-8-sig"))
            if isinstance(doc,dict): print(f"{MIRROR_NOTICE} ({u})"); return doc
        except Exception as e:
            print(f"{u}: {e}")
    raise SystemExit("ERROR: the manifest could not be read from GitHub or the fallback server.")
def fetch_package(url, sha, extra):
    for i,u in enumerate([url]+mirror_urls(url, extra)):
        if i==1: print(MIRROR_NOTICE)
        try:
            data=fetch(u)
        except Exception as e:
            print(f"{u}: {e}"); continue
        if hashlib.sha256(data).hexdigest().lower()==sha.lower(): return data
        print(f"{u}: SHA-256 mismatch.")
    raise SystemExit("ERROR: the package could not be downloaded with the right SHA-256 (GitHub and the fallback server).")
def version(): return open(os.path.join(ROOT,"VERSION"),encoding="utf-8").read().strip()
def version_key(value):
    try: return tuple(int(part) for part in value.strip().split("."))
    except (AttributeError, ValueError): return ()
def running_panel_version():
    found=subprocess.run(["docker","compose","-p",PROJECT,"ps","-q","seban-panel"],cwd=COMPOSE,text=True,capture_output=True,check=False).stdout.strip()
    if not found: return ""
    return subprocess.run(["docker","exec",found.splitlines()[0],"cat","/app/VERSION"],text=True,capture_output=True,check=False).stdout.strip()
manifest=fetch_manifest(); m=manifest["server"]; current=version()
print(f"Installed: {current}; available: {m['version']}")
if "--check" in sys.argv: sys.exit(0)
if current==m["version"]: print("Already current."); sys.exit(0)
with tempfile.TemporaryDirectory(prefix="seban-mt2009-") as d:
 progress(2, "Downloading and validating the update package.")
 pkg=os.path.join(d,"update.zip"); open(pkg,"wb").write(fetch_package(m["url"], m["sha256"], manifest.get("mirrors") or []))
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
  raise SystemExit(f"ERROR: Seban Panel update requested, but the Playerbots package has no {panel_source}.")
 bundled_version=open(os.path.join(panel_source,"VERSION"),encoding="utf-8").read().strip()
 installed_panel_version=running_panel_version()
 if version_key(installed_panel_version) and version_key(bundled_version) <= version_key(installed_panel_version):
  print(f"Seban Panel update skipped: bundled {bundled_version}, installed {installed_panel_version}. Downgrades and same-version rebuilds are blocked.")
 else:
  services.extend(["seban-panel","seban-collector","seban-item-grants"])
  panel_updated=True
  print(f"Seban Panel update enabled: {installed_panel_version or 'unknown'} -> {bundled_version} (version bundled with Playerbots).")
else:
 print("Seban Panel update disabled: keeping the currently installed version.")
subprocess.run(["docker","compose","-p",PROJECT,"up","-d","--build",*services],cwd=COMPOSE,check=True)
print("Done. Playerbots updated with Seban Panel " + ("updated." if panel_updated else "kept unchanged."))
