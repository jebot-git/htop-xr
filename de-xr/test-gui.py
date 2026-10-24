#!/usr/bin/env python3
"""Native Wayland and floor protocol checks on an isolated DomeD/Xvfb session."""
import json, os, signal, socket, struct, subprocess, sys, tempfile, time, urllib.request
from pathlib import Path
from PIL import Image
signal.pthread_sigmask(signal.SIG_SETMASK, [])
if os.environ.get('HTOP_TEST_BUS') != '1':
    os.execvpe('dbus-run-session', ['dbus-run-session', '--', sys.executable, *sys.argv], {**os.environ, 'HTOP_TEST_BUS':'1'})
root=Path(__file__).resolve().parents[1]
out=root/'build/gui-artifacts';out.mkdir(parents=True,exist_ok=True)
children=[];streams=[];checks=[]
def wait(fn, timeout=15):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            result=fn()
            if result:return result
        except (OSError, ValueError, KeyError, StopIteration):pass
        time.sleep(.05)
    raise AssertionError('Timed out; inspect '+str(out))
def start(args,env,name):
    stream=(out/(name+'.log')).open('w');streams.append(stream)
    p=subprocess.Popen(args,env=env,stdout=stream,stderr=stream);children.append(p);return p
def check(condition,name):
    assert condition,name
    checks.append(name);print('PASS '+name,flush=True)
try:
    with tempfile.TemporaryDirectory(prefix='htop-wayland-') as temporary:
        runtime=Path(temporary);runtime.chmod(0o700)
        rd,wr=os.pipe()
        xvfb=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1600x1000x24','-nolisten','tcp'],pass_fds=(wr,),stderr=subprocess.DEVNULL);children.append(xvfb);os.close(wr)
        with os.fdopen(rd) as stream:display=':'+stream.readline().strip()
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        env={**os.environ,'DISPLAY':display,'XDG_RUNTIME_DIR':temporary,'XDG_CONFIG_HOME':temporary+'/config','XDG_STATE_HOME':temporary+'/state','XDG_CACHE_HOME':temporary+'/cache','XDG_DATA_HOME':temporary+'/data','XDG_CURRENT_DESKTOP':'Dome','DOME_PREVIEW_PORT':str(port),'DOME_LOGIN_SESSION':'0','DOME_START_XR':'0','DOME_APPEARANCE_SERVICE':'0','DOME_RENDERER':'gles','LIBGL_ALWAYS_SOFTWARE':'1','FLTK_BACKEND':'wayland'}
        for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET'):env.pop(key,None)
        for service in ('TRAY','KDECONNECT','KWALLET','POLKIT','IME'):env['DOME_START_'+service]='0'
        start([os.environ.get('DOME_TEST_COMPOSITOR','/opt/fltk-vr/bin/dome-compositor'),'--backend','nested'],env,'compositor')
        http=urllib.request.build_opener(urllib.request.ProxyHandler({}))
        def read(path):
            with http.open(f'http://127.0.0.1:{port}'+path,timeout=2) as response:return json.load(response)
        wait(lambda:read('/api/state'))
        records=wait(lambda:list(runtime.glob('*.dome-session.json')))
        realms={json.loads(p.read_text())['realm']:p.name.removesuffix('.dome-session.json') for p in records}
        wait(lambda:len(realms)==2)
        def gui(realm,name,capture=False):
            return start([str(root/'build/gui/htop-fltk'),*(['--screenshot',str(out/(name+'.ppm'))] if capture else [])],{**env,'WAYLAND_DISPLAY':realms[realm],'WAYLAND_DEBUG':'1','DE_XR_GL_TRACE':'1'},name)
        def published():
            state=json.loads((runtime/'htop-floor.json').read_text());raw=(runtime/'htop-floor.rgba').read_bytes()
            return (state,raw) if len(raw)==1024*256*4+12 and struct.unpack('<Q',raw[4:12])[0]==state['serial'] else None
        first=gui('desktop','desktop-live')
        state,raw=wait(published)
        check(raw[:4]==b'HTPF' and state['enabled'] and [m['label'] for m in state['meters']]==['CPU','RAM','SWAP'],'Matched floor sample uses real CPU, RAM and swap meters')
        check((runtime/'htop-floor.json').stat().st_mode&0o777==0o600,'Floor publication is private')
        image=Image.frombytes('RGBA',(1024,256),raw[12:]);image.save(out/'floor.png')
        check(len(image.getcolors(1024*256) or [])>=5,'Floor canvas contains text and coloured bars')
        check(image.getpixel((0,0))==(0,0,0,0) and image.getpixel((30,60))[3]==0,
              'XR floor has no panel background and meter grout is transparent')
        for i,meter in enumerate(state['meters']):
            if meter['value']<1:
                check(image.getpixel((1007,60+i*80))[3]==0,'Unused '+meter['label']+' track is transparent')
        second=gui('desktop','desktop-second',True);check(second.wait(timeout=10)==0,'Second GUI coexists without taking floor ownership')
        check(json.loads((runtime/'htop-floor.json').read_text())['pid']==first.pid,'Closing secondary GUI preserves primary publication')
        first.terminate();first.wait(timeout=5)
        # SIGTERM has no GUI cleanup; the compositor rejects its stale timestamp.
        for p in ('htop-floor.json','htop-floor.rgba'):(runtime/p).unlink(missing_ok=True)
        xr=gui('xr','xr',True);wait(published);check(xr.wait(timeout=10)==0,'XR GUI renders and exits cleanly')
        check(not (runtime/'htop-floor.json').exists(),'Clean exit withdraws the floor publication')
        for name in ('desktop-second','xr'):
            im=Image.open(out/(name+'.ppm'));im.save(out/(name+'.png'))
            check(im.getbbox() is not None and len(im.getcolors(im.width*im.height) or [])>8,name+' contains a rendered process table')
        text=(out/'xr.log').read_text()
        check('wl_compositor' in text and 'org.domed.Htop' in text,'Frontend uses native Wayland with its application identity')
        check('org_kde_kwin_appmenu' in text,'Universal application menu is exported through Wayland')
        (out/'report.json').write_text(json.dumps({'passed':True,'checks':checks},indent=2)+'\n')
finally:
    for p in reversed(children):
        if p.poll() is None:p.terminate()
    for p in reversed(children):
        try:p.wait(timeout=4)
        except subprocess.TimeoutExpired:p.kill();p.wait()
    for stream in streams:stream.close()
