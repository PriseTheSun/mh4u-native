#!/usr/bin/env python3
"""Research-only, guarded execution of the sampler fallback and initializer tail.

Uses pinned ELF loading from the root-motion harness. No game execution, host
calls, implicit BSS initialization, or dependency of the delivered engine.
"""
import argparse
import hashlib
import inspect
import json
from pathlib import Path
import struct
from root_motion_probe import Harness, bits, ELF_SHA

RAM=0x02000000
STACK=0x03000000
RETURN=0x04000000


def probe(harness,kind,seed,frame=0,codec=None,reference=None,negative=None):
    u,a=harness.u,harness.a
    mu=u.Uc(u.UC_ARCH_ARM,u.UC_MODE_ARM|u.UC_MODE_LITTLE_ENDIAN)
    mu.ctl_set_cpu_model(a.UC_CPU_ARM_11MPCORE)
    pages=set();initialized=set();source=[];reads={};writes={};executed=[];failures=[]
    def mapping(address,size):
        for page in range(address&~4095,(address+size+4095)&~4095,4096):
            if page not in pages:mu.mem_map(page,4096);pages.add(page)
    requested=[(0xaea0e4,0xaea27c)] if kind=='sampler' else [
        (0xd1d4b8,0xd1d9d4),(0x1001f4,0x100216),(0xbdf310,0xbdf320)]
    for lo,hi in requested:
        matches=[(address,data) for address,data,size,flags in harness.segments
                 if address<=lo and hi<=address+len(data)]
        if len(matches)!=1:raise ValueError('Requested source range is not uniquely initialized')
        address,data=matches[0]
        mapping(lo,hi-lo);mu.mem_write(lo,data[lo-address:hi-address]);source.append((lo,hi))
    writable=[(RAM,RAM+4096),(STACK,STACK+65536)]
    if kind=='initializer-tail':writable += [(0x10e6250,0x10e6580),(0x1056064,0x10560b8)]
    for lo,hi in writable:
        mapping(lo,hi-lo);mu.mem_write(lo,bytes([seed])*(hi-lo))
    def init(address,data):
        mu.mem_write(address,data);initialized.update(range(address,address+len(data)))
    for i in range(13):mu.reg_write(getattr(a,'UC_ARM_REG_R'+str(i)),seed*0x01010101)
    for i in range(32):mu.reg_write(getattr(a,'UC_ARM_REG_S'+str(i)),bits(seed+i+.5))
    mu.reg_write(a.UC_ARM_REG_CPSR,0x10)
    mu.reg_write(a.UC_ARM_REG_C1_C0_2,0xf00000)
    mu.reg_write(a.UC_ARM_REG_FPEXC,1<<30)
    mu.reg_write(a.UC_ARM_REG_FPSCR,0)
    mu.reg_write(a.UC_ARM_REG_SP,STACK+0xff00)
    mu.reg_write(a.UC_ARM_REG_LR,RETURN)
    if kind=='sampler':
        start,end=0xaea0e4,RETURN
        allowed=[(0xaea0e4,0xaea26c)]
        mu.reg_write(a.UC_ARM_REG_R0,RAM)
        mu.reg_write(a.UC_ARM_REG_S0,bits(frame))
        if codec is None:
            init(RAM,struct.pack('<I',0))
        else:
            init(RAM,struct.pack('<I',RAM+0x100))
            init(RAM+0x100,struct.pack('<I',codec))
            init(RAM+0x110,struct.pack('<4f',*reference))
        if negative=='uninitialized-pointer':initialized.difference_update(range(RAM,RAM+4))
        if negative=='unknown-codec':init(RAM+0x100,struct.pack('<I',4))
    else:
        # A self-contained tail: first instructions load their pointers and
        # floating constants. Stop before the original function's epilogue.
        start,end=0xd1d4b8,0xd1d9a8
        allowed=[(0xd1d4b8,0xd1d840),(0xd1d8ac,0xd1d9a8),
                 (0x1001f4,0x100216),(0xbdf310,0xbdf31c)]
        if negative=='missing-helper':allowed=allowed[:-1]
    def fail(reason,**data):
        failures.append({'reason':reason,'pc':hex(mu.reg_read(a.UC_ARM_REG_PC)),**data});mu.emu_stop()
    def code(engine,pc,size,user):
        if pc==end:engine.emu_stop();return
        if not any(lo<=pc and pc+size<=hi for lo,hi in allowed):
            fail('Execution escaped declared instruction ranges',address=hex(pc));return
        executed.append(pc)
    def read(engine,access,address,size,value,user):
        valid=all(x in initialized for x in range(address,address+size))
        if not valid and not any(lo<=address and address+size<=hi for lo,hi in source):
            fail('Uninitialized read',address=hex(address),size=size);return
        reads[address,size]=reads.get((address,size),0)+1
    def write(engine,access,address,size,value,user):
        if not any(lo<=address and address+size<=hi for lo,hi in writable):
            fail('Out-of-scope write',address=hex(address),size=size);return
        initialized.update(range(address,address+size));writes[address,size]=writes.get((address,size),0)+1
    def invalid(engine,access,address,size,value,user):
        fail('Unmapped/protected access',address=hex(address),size=size,access=access);return False
    hooks=[mu.hook_add(u.UC_HOOK_CODE,code),mu.hook_add(u.UC_HOOK_MEM_READ,read),
           mu.hook_add(u.UC_HOOK_MEM_WRITE,write),mu.hook_add(u.UC_HOOK_MEM_INVALID,invalid)]
    try:mu.emu_start(start,end,timeout=2_000_000,count=20000)
    except u.UcError as error:failures.append({'reason':'Unicorn error','detail':str(error)})
    if not failures and mu.reg_read(a.UC_ARM_REG_PC)!=end:failures.append({'reason':'Instruction/time budget exhausted'})
    if kind=='sampler':
        words=[mu.reg_read(getattr(a,'UC_ARM_REG_S'+str(i))) for i in range(4)]
        observed={'register_words':[hex(x) for x in words],
                  'values':list(struct.unpack('<4f',struct.pack('<4I',*words)))}
        expected=[0,0,0,0x3f800000] if codec is None else [bits(x) for x in reference]
        matches=words==expected
    else:
        observed={}
        for address,count in [(0x10e62c0,3),(0x10e6370,3),(0x10e6560,4),(0x10e6570,4)]:
            complete=all(x in initialized for x in range(address,address+4*count))
            observed[hex(address)]=list(struct.unpack('<'+str(count)+'f',mu.mem_read(address,4*count))) if complete else None
        matches=observed=={'0x10e62c0':[0,0,0],'0x10e6370':[0,0,0],
                           '0x10e6560':[0,0,0,1],'0x10e6570':[0,0,0,0]}
    result={'kind':kind,'register_and_ram_poison_seed':seed,'frame':frame,'codec':codec,
            'reference':reference,'negative_control':negative,'start':hex(start),'end':hex(end),
            'status':'failed' if failures else 'passed','matches_expected':matches,
            'observed':observed,'failures':failures,'instruction_count':len(executed),
            'executed_addresses':[hex(x) for x in sorted(set(executed))],
            'reads':[{'address':hex(x),'size':s,'count':n} for (x,s),n in sorted(reads.items())],
            'writes':[{'address':hex(x),'size':s,'count':n} for (x,s),n in sorted(writes.items())]}
    for hook in hooks:mu.hook_del(hook)
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--elf',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    if args.output.exists():ap.error('Choose a new report path')
    harness=Harness(args.elf)
    cases=[]
    for seed in [0x25,0xa5,0xde]:
        cases.append(probe(harness,'initializer-tail',seed))
        for frame in [-7,0,4.5,100000]:cases.append(probe(harness,'sampler',seed,frame))
        for codec,reference in [(1,[1.25,-2,4.5,7]),(2,[.5,-.25,.75,2])]:
            cases.append(probe(harness,'sampler',seed,4.5,codec,reference))
    guards=[probe(harness,'sampler',0xa5,negative='uninitialized-pointer'),
            probe(harness,'sampler',0xa5,codec=1,reference=[1,2,3,4],negative='unknown-codec'),
            probe(harness,'initializer-tail',0xa5,negative='missing-helper')]
    passed=all(x['status']=='passed' and x['matches_expected'] for x in cases)
    guarded=all(x['status']=='failed' and x['failures'] for x in guards)
    report={'schema':'mh4u-root-motion-defaults-probe-v1','elf_sha256':ELF_SHA,
            'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'unicorn_version':harness.u.__version__,'cpu_model':'ARM11MPCore','fpscr':0,
            'elf_loader_source':inspect.getsource(Harness.__init__),
            'cases':cases,'negative_controls':guards,'passed':passed and guarded,
            'original_game_runtime_executed':False,'helper_calls_intercepted':False,
            'scope':'Self-contained initializer tail excluding preceding trig tables and original epilogue; sampler absent/reference branches. Synthetic registers, stack and poisoned globals; no whole startup or later mutation claim.'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x') as stream:json.dump(report,stream,indent=2);stream.write('\n')
    print(json.dumps({'passed':report['passed'],'positive_cases':len(cases),'negative_controls':len(guards),
                      'failures':[x['failures'] for x in cases if x['failures']]}))
    return int(not report['passed'])


if __name__=='__main__':raise SystemExit(main())
