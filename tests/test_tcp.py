"""Real Gurux HDLC/association/read/reconnect using shared JSON over loopback."""
import json,pathlib,selectors,socket,subprocess,sys,tempfile,time
client,server,config=sys.argv[1:]
def line(process,timeout=8):
    # Each simulator/client emits one small line per operation. Use unbuffered
    # binary pipes to avoid text buffering hiding selector readiness.
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout,selectors.EVENT_READ)
        if not selector.select(timeout):raise AssertionError('Timed out waiting for process output')
        data=process.stdout.readline()
        assert data, 'Process exited before output'
        return data.decode()
def stop(process):
    if process is None:return
    process.terminate()
    try:process.wait(timeout=4)
    except subprocess.TimeoutExpired:process.kill();process.wait()
    assert process.returncode in (0,-15), (process.returncode,process.stderr.read().decode())
with tempfile.TemporaryDirectory() as d:
    x=json.loads(pathlib.Path(config).read_text())
    with socket.socket() as s:s.bind(('127.0.0.1',0));x['dlms']['port']=s.getsockname()[1]
    path=pathlib.Path(d)/'dtm.json';path.write_text(json.dumps(x))
    meter=poll=None
    def start_meter():
        proc=subprocess.Popen([server,str(path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
        assert 'SIMULATED meter' in line(proc)
        return proc
    try:
        meter=start_meter()
        poll=subprocess.Popen([client,str(path)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,bufsize=0)
        def sample():
            for _ in range(30):
                text=line(poll)
                if text.startswith('{'):return json.loads(text)['points']
            raise AssertionError('No JSON samples')
        points=sample()
        assert len(points)==25
        for actual,expected in zip(points,x['io_points']):
            assert actual['quality']==2,actual
            assert actual['value']==expected['simulator_value'],(actual,expected)
            assert actual['sbkr_id']==expected['sbkr_id']
        stop(meter);meter=None
        for _ in range(30):
            failed=sample()
            if any(p['quality']!=2 for p in failed):break
        else:raise AssertionError('Loss of server did not mark data bad')
        meter=start_meter()
        for _ in range(30):
            restored=sample()
            if all(p['quality']==2 for p in restored):break
        else:raise AssertionError('Client did not reconnect')
        assert restored[0]['value']==x['io_points'][0]['simulator_value']
    finally:
        stop(poll);stop(meter)
print('25 shared JSON values read via DLMS; failure quality and reconnect passed')
