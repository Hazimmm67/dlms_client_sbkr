"""CLI validation failures must occur before opening any connections."""
import copy,json,pathlib,subprocess,sys,tempfile
exe,config=sys.argv[1:]
base=json.loads(pathlib.Path(config).read_text())
cases=[]
def change(key,value):
    x=copy.deepcopy(base);x['io_points'][0][key]=value;return json.dumps(x)
for key,val in [('sbkr_id',-1),('sbkr_id',1.5),('obis_code','1-1:999.25'),('obis_code','1.1.1.25.0.255junk'),('high',-1e20),('fallback_scaler',13),('data_type','DI')]:
    cases.append(change(key,val))
x=copy.deepcopy(base);x['io_points'][1]['sbkr_id']=x['io_points'][0]['sbkr_id'];cases.append(json.dumps(x))
x=copy.deepcopy(base);x['io_points'][1]['obis_code']=x['io_points'][0]['obis_code'];cases.append(json.dumps(x))
x=copy.deepcopy(base);x['connectivity']['polling_frequency']='0min';cases.append(json.dumps(x))
x=copy.deepcopy(base);x['connectivity']['serial_settings']['parity']='Even';cases.append(json.dumps(x))
x=copy.deepcopy(base);x['io_points']=[];cases.append(json.dumps(x))
x=copy.deepcopy(base);x['connectivity']['connection_type']='TCP';cases.append(json.dumps(x))
cases += ['{"io_points":[],"io_points":[]}',json.dumps(base)+' trailing junk','{bad json}']
with tempfile.TemporaryDirectory() as d:
    f=pathlib.Path(d)/'dtm.json'
    for i,case in enumerate(cases):
        f.write_text(case)
        r=subprocess.run([exe,str(f),'--check'],capture_output=True,text=True)
        assert r.returncode==2,(i,r.stdout,r.stderr)
    x=copy.deepcopy(base);x['io_points'][0]['obis_code']='1.1.1.25.0.255';f.write_text(json.dumps(x))
    assert subprocess.run([exe,str(f),'--check'],capture_output=True).returncode==0
print(f'{len(cases)} invalid configurations rejected; full OBIS accepted')
