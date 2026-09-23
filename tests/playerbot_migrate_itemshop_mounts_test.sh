#!/bin/sh
# The migrator's ItemShop mounts step, with db() replaced by a stub.
#
# The columns of common.itemshop_items are read from information_schema, so
# what the step writes depends on what the table turns out to be. Pinned here:
# the switch, a world without the table, a table whose columns it does not
# know, a shop with no hairstyle to copy, and the INSERT it builds for two
# shapes of table - the index, the item, the count and the price put in, a
# promotion or auction number cleared, an auto-increment left to the server,
# everything else copied from the hairstyle row. The SQL itself was run on a
# MariaDB against both shapes; this only keeps the shell's half honest.
#
#   sh tests/playerbot_migrate_itemshop_mounts_test.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
APPLY="${1:-$HERE/../linux-port-mt2009/docker/mariadb/playerbot/apply.sh}"
[ -f "$APPLY" ] || { echo "no apply.sh at $APPLY" >&2; exit 2; }

BLOCK=$(sed -n '/^ishop_on=/,/^esac$/p' "$APPLY")
[ -n "$BLOCK" ] || { echo "the ItemShop mounts block is not in $APPLY" >&2; exit 2; }

# The two shapes, as information_schema answers for them (name|extra|type).
SHAPE_A='index||int
vnum||int
count||int
price||int
currency||tinyint
min_level||tinyint
socket0||int
promotion_price||int
auction_start||datetime'
SHAPE_B='id|auto_increment|int
item_index||int
item_vnum||int
item_count||int
item_price||int
currency||int'

fails=0
run() { # run DESCRIPTION ON PRICE COLS TEMPLATE EXPECTED_SUBSTRING...
    _desc="$1"; _on="$2"; _price="$3"; _cols="$4"; _tmpl="$5"; shift 5
    _log=$(mktemp)
    _out=$(M2_ITEMSHOP_MOUNTS="$_on" M2_ITEMSHOP_MOUNT_PRICE="$_price" COLS="$_cols" TMPL="$_tmpl" SQLLOG="$_log" \
        sh -c '
db() {
    case "$*" in
        *information_schema.columns*) [ -n "$COLS" ] && printf "%s\n" "$COLS"; return 0 ;;
        *"SELECT MIN("*)              printf "%s\n" "$TMPL"; return 0 ;;
        *"INSERT INTO"*)              printf "SQL %s\n" "$(printf "%s" "$*" | tr -s " \n" " ")" >> "$SQLLOG"; printf "2\n"; return 0 ;;
        *"SELECT COUNT(*)"*)          printf "3\n"; return 0 ;;
        *)                            return 0 ;;
    esac
}
'"$BLOCK" 2>&1)
    _out="$_out
$(cat "$_log")"
    rm -f "$_log"
    for _want in "$@"; do
        if ! printf '%s' "$_out" | grep -qF -- "$_want"; then
            echo "FAIL - $_desc"
            echo "       expected to find: $_want"
            printf '       got: %s\n' "$_out"
            fails=$((fails + 1))
            return
        fi
    done
    echo "ok   - $_desc"
}
bq='`'

run 'the switch leaves the shop alone' 0 250 "$SHAPE_A" 301 \
    'left to the operator'
run 'a world without the table says so' 1 250 '' 301 \
    'no common.itemshop_items'
run 'unknown columns list nothing' 1 250 'a||int
b||int' 301 \
    'columns this step does not know (a b)'
run 'no hairstyle to copy lists nothing' 1 250 "$SHAPE_A" NULL \
    'no hairstyle row to copy'
run 'shape A: the row is built from the hairstyle' 1 250 "$SHAPE_A" 301 \
    "INSERT INTO common.itemshop_items (${bq}index${bq}, ${bq}vnum${bq}, ${bq}count${bq}, ${bq}price${bq}, ${bq}currency${bq}, ${bq}min_level${bq}, ${bq}socket0${bq}, ${bq}promotion_price${bq}, ${bq}auction_start${bq})" \
    "SELECT n.idx, n.vnum, 1, 250, t.${bq}currency${bq}, t.${bq}min_level${bq}, t.${bq}socket0${bq}, 0, t.${bq}auction_start${bq}" \
    "ON t.${bq}index${bq} = 301" \
    'ItemShop mounts: 3 in the world, 2 added'
run 'shape B: the surrogate key is left to the server' 1 250 "$SHAPE_B" 330 \
    "SELECT NULL, n.idx, n.vnum, 1, 250, t.${bq}currency${bq}" \
    "WHERE ${bq}item_index${bq} BETWEEN 701 AND 799"
run 'a price that is not a number reads as 250' 1 'abc' "$SHAPE_B" 330 \
    'n.vnum, 1, 250,'
run 'the price in .env is used' 1 99 "$SHAPE_B" 330 \
    'n.vnum, 1, 99,' '(99 Dragon Coins each)'

if [ "$fails" -ne 0 ]; then
    echo "$fails ItemShop mounts test(s) failed"
    exit 1
fi
echo "all ItemShop mounts tests passed"
