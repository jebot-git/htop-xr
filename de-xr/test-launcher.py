#!/usr/bin/env python3
"""Check launch dispatch with a pseudo-terminal and the exact upstream binary."""
import hashlib, os, pty, select, subprocess, tempfile, time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='htop-launcher-') as temporary:
    prefix=Path(temporary);lib=prefix/'libexec/htop';lib.mkdir(parents=True)
    (lib/'htop-tui').symlink_to(root/'build/tui/htop')
    (lib/'htop-fltk').write_text('#!/bin/sh\nprintf "GUI selected\\n"\n')
    (lib/'htop-fltk').chmod(0o755)
    env={**os.environ,'HTOP_DE_XR_PREFIX':temporary,'WAYLAND_DISPLAY':'test-graphical','TERM':'xterm-256color','HTOPRC':str(prefix/'htoprc')}
    launcher=str(root/'de-xr/htop')
    for args in (['--help'],['--version'],['--tui','--help']):
        expected=subprocess.check_output([str(root/'build/tui/htop'),*([args[-1]] if args[0]=='--tui' else args)],env=env)
        actual=subprocess.check_output([launcher,*args],env=env)
        assert actual==expected,args
    assert subprocess.check_output([launcher],env=env)==b'GUI selected\n'
    assert subprocess.check_output([launcher,'--gui'],env=env)==b'GUI selected\n'
    master,slave=pty.openpty()
    p=subprocess.Popen([launcher],env=env,stdin=slave,stdout=slave,stderr=slave);os.close(slave)
    output=b'';end=time.monotonic()+5
    while time.monotonic()<end and b'F10' not in output:
        if select.select([master],[],[],.1)[0]:output+=os.read(master,65536)
        if p.poll() is not None:break
    os.write(master,b'q');p.wait(timeout=5);os.close(master)
    assert p.returncode==0 and b'GUI selected' not in output and b'CPU' in output
    rc=prefix/'htoprc';rc.write_text('# GUI must leave this untouched\n')
    before=hashlib.sha256(rc.read_bytes()).digest()
    subprocess.run([str(root/'build/gui/htop-fltk'),'--dump-json'],env=env,stdout=subprocess.DEVNULL,check=True)
    assert hashlib.sha256(rc.read_bytes()).digest()==before
print('PASS exact upstream CLI output, console PTY launch, GUI dispatch and htoprc isolation')
