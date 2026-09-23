#!/bin/sh
# =============================================================================
#  sync-upstream.sh -- bring the upstream project's new commits into this repo.
#
#      sh tools/sync-upstream.sh            # from the repository root
#
#  tools/upstream-sync.json names the upstream repository and the last commit
#  already brought in. This script
#
#    1. fetches upstream's branch into the remote "upstream",
#    2. lists the commits since synced_commit,
#    3. applies their combined change on top of the working tree (git apply
#       -3: a conflict is left with markers, as a merge would leave it),
#    4. points upstream's repository links at this repository again,
#    5. records the new synced_commit, and takes synced_version and
#       engine_package from upstream's own update-manifest-mt2009.json at that
#       commit - the release workflow builds the engine files from that package,
#    6. reports what still needs a person: conflicted files, and added lines
#       that name people or places this repository does not use.
#
#  It commits nothing. Resolve the conflicts, read the report, set
#  linux-port-mt2009/VERSION and CLIENT_VERSION above both this repo's and
#  upstream's numbers, write the CHANGELOG section, and commit.
# =============================================================================
set -eu

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"
SYNC=tools/upstream-sync.json
OURS=Doofenyoyo1/metin2_SP
[ -f "$SYNC" ] || { echo "no $SYNC"; exit 1; }

field() { python3 -c "import json,sys; d=json.load(open('$SYNC')); print(d$1)"; }
REPO_URL=$(field "['repository']")
BRANCH=$(field "['branch']")
LAST=$(field "['synced_commit']")

git remote get-url upstream >/dev/null 2>&1 || git remote add upstream "$REPO_URL"
echo "fetching $REPO_URL $BRANCH"
GIT_LFS_SKIP_SMUDGE=1 git fetch --depth=500 upstream "$BRANCH"
NEW=$(git rev-parse "upstream/$BRANCH")
git cat-file -e "$LAST^{commit}" 2>/dev/null || { echo "synced_commit $LAST is not in the fetched history - fetch deeper"; exit 1; }

if [ "$LAST" = "$NEW" ]; then
    echo "already synced to $NEW - nothing new upstream"
    exit 0
fi
echo "new upstream commits:"
git log --format='  %h %ad %s' --date=short "$LAST..$NEW"

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "the working tree has changes - commit or stash them first"; exit 1
fi

PATCH=$(mktemp)
git diff --binary "$LAST" "$NEW" > "$PATCH"
set +e
git apply -3 "$PATCH"
set -e
rm -f "$PATCH"

# Upstream's own links, in every file the sync touched.
CHANGED=$(git diff --name-only "$LAST" "$NEW" | while read -r f; do [ -f "$f" ] && printf '%s\n' "$f"; done)
for f in $CHANGED; do
    case "$f" in tools/upstream-sync.json) continue ;; esac
    grep -q "TieruYT/metin2-playerbots" "$f" 2>/dev/null && \
        sed -i "s#TieruYT/metin2-playerbots#$OURS#g" "$f" && echo "  links -> $OURS: $f"
done

git show "$NEW:update-manifest-mt2009.json" > /tmp/upstream-manifest.$$
python3 - "$SYNC" "$NEW" /tmp/upstream-manifest.$$ <<'EOF'
import json, sys
path, commit, manifest = sys.argv[1:4]
d = json.load(open(path, encoding='utf-8'))
m = json.load(open(manifest, encoding='utf-8-sig'))
d['synced_commit'] = commit
d['synced_version'] = m['server']['version']
d['engine_package'] = {'url': m['server']['url'], 'sha256': m['server']['sha256']}
open(path, 'w', encoding='utf-8').write(json.dumps(d, indent=2) + '\n')
print('synced to %s (upstream %s); engine files from %s' % (commit[:7], d['synced_version'], d['engine_package']['url']))
EOF
rm -f /tmp/upstream-manifest.$$

echo
echo "== conflicted files (resolve, then git add):"
{ git diff --name-only --diff-filter=U; grep -l '^<<<<<<< ' $CHANGED 2>/dev/null; } | sort -u || true
echo
echo "== upstream's added lines naming its people or places (plain repo links are already replaced in the files; review the rest):"
git diff "$LAST" "$NEW" | grep '^+' | grep -v '^+++' | \
    grep -nP 'TieruYT|\bTieru\b|@tieru|discord\.gg/|buycoffee|zrzutka|patreon|metin2singleplayer\.com|dawio' | cut -c1-200 || echo "  none"
echo
echo "next: resolve, set VERSION/CLIENT_VERSION above both numbering lines, add the CHANGELOG section, commit, push"
