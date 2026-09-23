# =============================================================================
#  ItemShop -- standalone PHP web shop, buys write into player.item_award so
#  the stock delivery pipeline (ItemAwardManager.cpp) hands items to the
#  player next login. Independent login form against account.account, not the
#  retail sas/pid auto-login scheme -- real game credentials work.
# =============================================================================
# Pinned to bookworm and built without apt: the trixie tag's apt verifies
# InRelease with sequoia and failed on a player's Docker Desktop ("Splitting
# of clearsigned file failed as it doesn't contain all expected parts"), which
# cancelled the whole compose build and left him on the old server. Nothing
# here needs a Debian package - the healthcheck asks PHP itself.
FROM php:8.2-apache-bookworm

RUN docker-php-ext-install mysqli

# The official image ships no php.ini at all, which means display_errors
# on: a fatal error came back as HTTP 200 with the stack trace as the page,
# the healthcheck below called that healthy, and the client's embedded
# IE cached the error page so that even a repaired shop kept showing it
# (archded, 10 September, with the Apache log). The production ini turns
# display_errors off and log_errors on, so a fatal is a 500 with nothing
# cacheable in it and the trace lands in the container log instead.
RUN cp "$PHP_INI_DIR/php.ini-production" "$PHP_INI_DIR/php.ini"

COPY app/ /var/www/html/

# COPY keeps the source's modes, and a ZIP unpacked under a strict umask on
# Linux gives 770 on directories and 660 on files - owned by root, unreadable
# by www-data. Apache then answers 403 and syserr carries AH00529 for the
# .htaccess it cannot read; the shop looked broken and a hand-made chmod inside
# the running container fixed it until the container was recreated. Normalised
# here instead, in the image, where it survives every recreate.
#
# 755 on directories and 644 on files is what a document root needs and no
# more: nothing here is executed as a program and the application writes
# nothing back into its own tree - purchases go into the database.
RUN find /var/www/html -type d -exec chmod 0755 {} +  && find /var/www/html -type f -exec chmod 0644 {} +  && chown -R root:root /var/www/html

# ACMD(do_in_game_mall) hardcodes "http://host/ishop?pid=...&sas=..." (no
# trailing slash). mod_dir's default DirectorySlash would 302 that to
# "/ishop/?pid=...&sas=..." before it ever reaches index.php - fine in a
# normal browser, but doubles the round trips through the old in-game
# embedded browser and made the shop intermittently fail to open
# ("Nie mozna odnalezc strony") instead of just being a bit slower. Serving
# /ishop directly avoids the extra hop entirely.
RUN a2enmod rewrite \
 && sed -i '/<VirtualHost \*:80>/a\    RewriteEngine On\n    RewriteRule ^/ishop$ /ishop/index.php [L]' \
    /etc/apache2/sites-enabled/000-default.conf

EXPOSE 80/tcp

# file_get_contents is false on any 4xx/5xx, and with display_errors off a
# PHP fatal is a 500 - so this fails on a broken application, not only on
# a dead web server. The body test is the belt to those braces: a warning
# printed into an otherwise successful page is still a broken shop.
HEALTHCHECK --interval=20s --timeout=5s --start-period=20s --retries=3 \
    CMD php -r '$b=@file_get_contents("http://127.0.0.1/itemshop/"); exit($b===false || strpos($b,"Fatal error")!==false || strpos($b,"Warning:")!==false ? 1 : 0);'
