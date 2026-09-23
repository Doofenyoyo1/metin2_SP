"""Isolated Linux fixtures: run python3 integration/test_settings.py PATH_TO_NATIVE_M2_RATES."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil

HERE = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='m2-settings-test-') as tmp:
    root = Path(tmp)
    share, var, spool, bindir = [root / p for p in ('share', 'var', 'spool', 'bin')]
    for p in (share,var,spool,bindir): p.mkdir()
    for name in ('m2-map-regens','m2-server-settings'):
        shutil.copyfile(HERE/name,bindir/name); (bindir/name).chmod(0o755)
    shutil.copyfile(sys.argv[1],bindir/'m2-rates'); (bindir/'m2-rates').chmod(0o755)
    pack=root/'pack.sh'
    pack.write_text('pack_apply_rates() { [ "$RATE_EXP" != 999 ]; }\n')
    env=dict(os.environ, M2_SHARE_DIR=str(share),M2_VAR_DIR=str(var),M2_RATES_SPOOL=str(spool),
             M2_RATES_PACK=str(pack),M2_RATES_LOG=str(root/'rates.log'),PATH=str(bindir)+':'+os.environ['PATH'])
    folders={1:'metin2_map_a1',3:'metin2_map_a3',21:'metin2_map_b1',23:'metin2_map_b3',25:'metin2_map_monkey_dungeon_12',41:'metin2_map_c1',43:'metin2_map_c3',61:'map_n_snowm_01',63:'metin2_map_n_desert_01',64:'map_n_threeway',104:'metin2_map_spiderdungeon',108:'metin2_map_monkey_dungeon2',109:'metin2_map_monkey_dungeon3'}
    base=share/'locale/english/map'
    originals={}
    no_stones={25,104,108,109}
    for i,folder in folders.items():
        d=base/folder;d.mkdir(parents=True)
        files=[('regen.txt','5s')] + ([] if i in no_stones else [('stone.txt','1800s')])
        for name,delay in files:
            p=d/name
            content=f'# retained comment\ng\t1 2 3 4 0 0  {delay}\t100 1 101\nga 1 2 3 4 0 0 10m 100 1 102\ne 1 2 3 4 0 0 0s 100 1 0\n'
            p.write_text(content);originals[p]=content
    (base/'index').write_text(''.join(f'{i} {f}\n' for i,f in folders.items()))
    def run(name, *args, rc=0):
        p=subprocess.run([str(bindir/name),*args],env=env,text=True,capture_output=True)
        assert p.returncode==rc,(name,args,p.returncode,p.stdout,p.stderr)
    def request(n, extra='', exp=120, action='apply'):
        (spool/'server-settings.request').write_text(f'id=test-{n}\naction={action}\nsource=panel\nexp={exp}\ndrop=130\nyang=140\n'+extra)
    # Existing legacy requests must not replay after a combined operation.
    for name in ('rates','map-regens'):
        (var/name).mkdir();(var/name/'last-id').write_text('old\n')
    (spool/'request').write_text('id=old\nexp=100\ndrop=100\nyang=100\n')
    (spool/'map-regens.request').write_text('id=old\nmap_1=1\n')
    request(1,'map_21=1\nmap_stone_21=300\nmap_64=7\n')
    run('m2-server-settings','poll',rc=10)
    for helper in ('m2-rates','m2-map-regens'):run(helper,'poll')
    b=base/folders[21]
    assert '  1s\t' in (b/'regen.txt').read_text()
    assert '  300s\t' in (b/'stone.txt').read_text()
    assert (base/folders[1]/'regen.txt').read_text()==originals[base/folders[1]/'regen.txt']
    assert '  7s\t' in (base/folders[64]/'regen.txt').read_text()
    assert 'ga 1 2 3 4 0 0 1s ' in (b/'regen.txt').read_text()
    run('m2-server-settings','complete')
    assert 'last_restart_source=panel' in (spool/'server-settings.status').read_text()
    run('m2-server-settings','poll')
    before=(var/'map-regens/wanted').read_text()
    request(2,'map_21=6\nmap_stone_21=4000\n')
    run('m2-server-settings','poll',rc=1)
    assert (var/'map-regens/wanted').read_text()==before
    # Failed rate application rolls back changed map files and intents.
    request(3,'map_21=17\n',exp=999)
    run('m2-server-settings','poll',rc=1)
    assert (var/'map-regens/wanted').read_text()==before
    assert '  1s\t' in (b/'regen.txt').read_text()
    request(4,'map_stone_21=reset\n')
    run('m2-server-settings','poll',rc=10);run('m2-server-settings','complete')
    assert (b/'stone.txt').read_text()==originals[b/'stone.txt']
    assert '  1s\t' in (b/'regen.txt').read_text()
    request(5,'map_21=9999\n',exp=999,action='restart')
    run('m2-server-settings','poll',rc=10);run('m2-server-settings','complete')
    assert '  1s\t' in (b/'regen.txt').read_text()
    run('m2-server-settings','record','automatic','channel1-core1')
    status=(spool/'server-settings.status').read_text()
    assert 'last_restart_source=panel' in status and 'last_core_name=channel1-core1' in status
    # Execute the actual supervisor batch function with fake core lifecycle.
    text=(HERE/'m2-supervise').read_text()
    function=text[text.index('server_settings_change() {'):text.index('\nlang_change() {')]
    request(6,'map_21=2\n')
    run('m2-server-settings','poll',rc=10)
    harness='stop_cores(){ echo stop >>"$M2_VAR_DIR/cycles"; }; boot_cores(){ echo boot >>"$M2_VAR_DIR/cycles"; };\n'+function+'\nserver_settings_change\n'
    subprocess.run(['bash','-c',harness],env=env,check=True,capture_output=True)
    assert (var/'cycles').read_text()=='stop\nboot\n'
    for helper in ('m2-server-settings','m2-rates','m2-map-regens'): run(helper,'poll')
    print('PASS: map IDs, independent mobs/Metins, reset, invalid batch, rollback, restart-only, history, single restart and legacy cursor isolation')
