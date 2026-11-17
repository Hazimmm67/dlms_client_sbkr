"""Checks the demo library, persistence, error handling, and full DLMS -> store path."""
import json,pathlib,selectors,socket,subprocess,sys,tempfile
client,meter_exe,gateway_exe,library,api,config=sys.argv[1:]
def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1',0));return s.getsockname()[1]
def ready(process):
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout,selectors.EVENT_READ)
        assert selector.select(5),'Process readiness timed out'
        line=process.stdout.readline()
        assert line,(process.poll(),process.stderr.read().decode())
def finish(process):
    process.terminate()
    try:out,err=process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill();out,err=process.communicate();raise AssertionError('Child did not stop')
    assert process.returncode in (0,-15),(process.returncode,err.decode())
def start(*args):
    p=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
    try:ready(p)
    except BaseException:p.kill();p.wait();raise
    return p
def run_api(endpoint,mode):
    r=subprocess.run([api,endpoint,mode],capture_output=True,text=True,timeout=8)
    assert r.returncode==0,(mode,r.stdout,r.stderr)
with tempfile.TemporaryDirectory() as directory:
    d=pathlib.Path(directory);store=d/'api-store.json';endpoint=f'tcp://127.0.0.1:{port()}'
    gateway=start(gateway_exe,endpoint,str(store))
    try:
        run_api(endpoint,'--suite')
        saved=json.loads(store.read_text());assert saved['demo'] is True
        assert len(saved['points'])==2 and saved['logs'][0]['message']=='Literal 100% good'
        assert saved['points'][0]['sequence']==str(2**64-1)
        duplicate=subprocess.run([gateway_exe,f'tcp://127.0.0.1:{port()}',str(store)],capture_output=True,timeout=5)
        assert duplicate.returncode==2 and b'locked' in duplicate.stderr
    finally:finish(gateway)
    gateway=start(gateway_exe,endpoint,str(store))
    try:run_api(endpoint,'--verify')
    finally:finish(gateway)
    run_api(endpoint,'--timeout')
    recovery=subprocess.Popen([api,endpoint,'--recover'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
    try:
        ready(recovery)
        gateway=start(gateway_exe,endpoint,str(store))
        try:
            out,err=recovery.communicate(input=b'\n',timeout=5)
            assert recovery.returncode==0,(out,err)
        finally:finish(gateway)
    finally:
        if recovery.poll() is None:recovery.kill();recovery.wait()
    broken=d/'broken.json';broken.write_text('{broken json')
    rejected=subprocess.run([gateway_exe,endpoint,str(broken)],capture_output=True,timeout=5)
    assert rejected.returncode==2 and broken.read_text()=='{broken json'
    failure=d/'write-fail.json';gateway=start(gateway_exe,endpoint,str(failure));failure.mkdir()
    try:run_api(endpoint,'--write-fail')
    finally:finish(gateway)
    x=json.loads(pathlib.Path(config).read_text());x['dlms']['port']=port();x['dlms']['timeout_ms']=1000
    x['sbkr']={'gateway':endpoint,'library':library,'logger_endpoint':endpoint}
    path=d/'dtm.json';path.write_text(json.dumps(x));end_store=d/'meter-store.json'
    meter=start(meter_exe,str(path));gateway=start(gateway_exe,endpoint,str(end_store))
    try:
        result=subprocess.run([client,str(path),'--once'],capture_output=True,text=True,timeout=10)
        assert result.returncode==0,(result.stdout,result.stderr)
        assert '25 good, 0 failed, SBKR status 0' in result.stderr
        saved=json.loads(end_store.read_text());assert saved['write_batches']==1
        actual={p['id']:p for p in saved['points']};assert len(actual)==25
        for expected in x['io_points']:
            point=actual[expected['sbkr_id']]
            assert point['value']==expected['simulator_value'] and point['quality']==2
        finish(gateway);gateway=None
        result=subprocess.run([client,str(path),'--once'],capture_output=True,text=True,timeout=10)
        assert result.returncode==1 and 'SBKR status 31' in result.stderr
        gateway=start(gateway_exe,endpoint,str(end_store))
        result=subprocess.run([client,str(path),'--once'],capture_output=True,text=True,timeout=10)
        assert result.returncode==0 and json.loads(end_store.read_text())['write_batches']==2
    finally:
        finish(meter)
        if gateway:finish(gateway)
print('Demo API, ACK, readback, persistence, invalid requests, write failure, timeout and 25-point DLMS publication passed')
