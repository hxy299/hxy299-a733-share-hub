"""PTY provisioning test; synthetic credential only, never a live API call."""
import json, os, pty, select, subprocess, sys, time
binary, directory = sys.argv[1:]
def run(provider, model, key):
    master, slave = pty.openpty()
    proc = subprocess.Popen([binary], stdin=slave, stdout=slave, stderr=slave)
    os.close(slave)
    transcript = bytearray()
    for prompt, value in [(b'Provider [',provider), (b'Model name [',model),
                           (b'API key (hidden):',key)]:
        limit=time.monotonic()+10
        while prompt not in transcript:
            assert time.monotonic()<limit, bytes(transcript)
            if select.select([master],[],[],0.1)[0]:
                transcript.extend(os.read(master,4096))
        os.write(master,value+b'\n')
    assert proc.wait(timeout=10)==0
    try:
        while select.select([master],[],[],0.1)[0]:
            transcript.extend(os.read(master,4096))
    except OSError:
        pass
    os.close(master)
    assert key not in transcript, 'Secret echoed!'
    with open(directory+'/config/config.json') as f:
        cfg=json.load(f)
    assert cfg['api_key'].startswith('a7g1:')
    assert key.decode() not in json.dumps(cfg)
    return cfg
cfg=run(b'1',b'',b'host-test-secret-123')
assert cfg['model']=='deepseek-flash' and cfg['llm_host']=='api.deepseek.com'
cfg=run(b'2',b'custom-mimo-next',b'host-test-secret-456')
assert cfg['model']=='custom-mimo-next' and cfg['llm_host']=='api.xiaomimimo.com'
print('PTY setup: both providers, default/custom model, hidden key and encrypted JSON passed')
