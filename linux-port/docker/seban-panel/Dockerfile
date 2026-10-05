FROM python:3.13-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
# VERSION with a star, and that star is the whole of this fix: the file is
# in .gitignore and is written by prepare-context.sh and the Windows
# launcher, so it is there on every machine that has ever run one of them
# and on no other. A fresh install that simply builds - a VPS from the
# guide, a clone, anything - had no VERSION and the whole compose bake
# stopped at "failed to compute cache key ... /VERSION: not found", taking
# the game, the panel and the itemshop down with it (superjerry925,
# 20 September, on 2.0.88 where 2.0.8 had worked). A COPY whose other
# patterns match tolerates one that matches nothing, so the file is used
# where it exists and missed where it does not - and app.py has always
# answered "dev" when it cannot read it.
# event_autogen.py (MT2009_PLUS_EVENTS_AUTOGEN_V1): the events planner's week generator.
# special_item_group.snapshot.txt (MT2009_PLUS_CHEST_EDITOR_V1): the chests page's
# copy of the game's file, shown until the game publishes its own; starred
# for the same reason as VERSION.
COPY app.py collector.py item_grants.py decisions.py decision_tables.py event_autogen.py gm_commands.txt CHANGELOG.md VERSION* special_item_group.snapshot*.txt ./
COPY dbeditor ./dbeditor
# MT2009_PLUS_DB_EDITOR_V1: the client data builder (pure Python), with the
# released client's base files in m2clientpack/base/<client version>/.
COPY m2clientpack ./m2clientpack
COPY templates ./templates
COPY static ./static
ENV PYTHONUNBUFFERED=1
EXPOSE 7789
CMD ["gunicorn", "--bind", "0.0.0.0:7789", "--workers", "2", "--threads", "4", "app:app"]
