#!/usr/bin/env python3
"""Bounded research-only execution of pinned MH4U ARM root-motion routines.

Requires optional Unicorn 2.1.4; never imported by the delivered renderer. All
model fields, globals and intercepted sampler results are synthetic parameters.
Uninitialized RAM reads, undeclared calls and out-of-scope writes stop execution.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ELF_SHA='d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435'
MODEL=0x02000000;STACK=0x03000000;RETURN=0x04000000
APPLY=0x95cab0;ADVANCE=0x9573b8;PRE_UPDATE=0x95b790;SAMPLER=0xaea0e4
IDENTITY=[0.,0.,0.,1.]
GLOBAL_DEFAULTS={0x10e62c0:[0.,0.,0.],0x10e6370:[0.,0.,0.],
                 0x10e6560:IDENTITY,0x10e6570:IDENTITY}


def bits(x):return struct.unpack('<I',struct.pack('<f',x))[0]
def fp(word):return struct.unpack('<f',struct.pack('<I',word))[0]
def digest(data):return hashlib.sha256(data).hexdigest()
def default_case():
    return {'name':'relative-identity-scale','entry':'apply','model':{'position':[10,20,30],
        'rotation':IDENTITY,'scale':[2,3,4],'null_scale':[.5,2,.25]},
        'layer':{'attr':0,'internal_flags':4,'previous_flags':1,'blend':1,'sample_frame':4,
            'next_frame':5,'frame_max':10,'previous_translation':[1,2,3],'previous_rotation':IDENTITY,
            'translation_weight':1,'rotation_weight':1},
        'samples':[{'frame':4,'translation':[5,7,11],'rotation':IDENTITY}],
        'globals':{hex(k):v for k,v in GLOBAL_DEFAULTS.items()},'fpscr':0}


class Harness:
    def __init__(self,elf):
        import unicorn as u
        from unicorn import arm_const as a
        if u.__version__!='2.1.4':raise ValueError('Pin Unicorn 2.1.4 for this research profile')
        self.u,self.a=u,a;self.raw=Path(elf).read_bytes()
        if digest(self.raw)!=ELF_SHA:raise ValueError('ELF identity differs from pinned MH4U analysis image')
        self.segments=[];ph=struct.unpack_from('<I',self.raw,28)[0];size,count=struct.unpack_from('<HH',self.raw,42)
        if self.raw[:6]!=b'\x7fELF\x01\x01':raise ValueError('Expected ELF32 little endian')
        for i in range(count):
            typ,offset,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',self.raw,ph+size*i)
            if typ==1:
                if offset+filesz>len(self.raw) or filesz>memsz:raise ValueError('ELF segment bounds')
                self.segments.append((va,self.raw[offset:offset+filesz],memsz,flags))

    def run(self,case):
        u,a=self.u,self.a
        mu=u.Uc(u.UC_ARCH_ARM,u.UC_MODE_ARM|u.UC_MODE_LITTLE_ENDIAN)
        mu.ctl_set_cpu_model(a.UC_CPU_ARM_11MPCORE)
        pages=set();file_ranges=[];ram_valid=set();written=set();executed=[];calls=[];failures=[];reads={};event_plans=[]
        def map_range(addr,size,perm):
            begin=addr&~4095;end=(addr+size+4095)&~4095
            missing=[page for page in range(begin,end,4096) if page not in pages]
            if not missing:return
            run=previous=missing[0]
            for page in missing[1:]+[missing[-1]+8192]:
                if page!=previous+4096:
                    mu.mem_map(run,previous+4096-run,perm);pages.update(range(run,previous+4096,4096));run=page
                previous=page
        for va,data,memsz,flags in self.segments:
            map_range(va,len(data),u.UC_PROT_ALL);mu.mem_write(va,data);file_ranges.append((va,va+len(data)))
        for va,data,memsz,flags in self.segments:
            mu.mem_protect(va&~4095,((len(data)+4095)//4096)*4096,u.UC_PROT_READ|(u.UC_PROT_EXEC if flags&1 else 0))
        map_range(MODEL,0x2000,u.UC_PROT_READ|u.UC_PROT_WRITE)
        map_range(STACK,0x10000,u.UC_PROT_READ|u.UC_PROT_WRITE)
        map_range(RETURN,0x1000,u.UC_PROT_READ|u.UC_PROT_EXEC)
        # Poison synthetic memory; it becomes readable only after explicit
        # initialization or a tracked ARM write. No assumption about zero BSS.
        mu.mem_write(MODEL,b'\xa5'*0x2000);mu.mem_write(STACK,b'\xa5'*0x10000)
        initial=[]
        def write(addr,fmt,values,label):
            data=struct.pack(fmt,*values);map_range(addr,len(data),u.UC_PROT_READ|u.UC_PROT_WRITE)
            mu.mem_write(addr,data);ram_valid.update(range(addr,addr+len(data)))
            initial.append({'address':hex(addr),'label':label,'hex':data.hex(),'values':list(values)})
        def m(offset,fmt,values,label):write(MODEL+offset,fmt,values,label)
        md=case['model'];layer=case['layer'];entry=case['entry']
        m(0x234,'<I',[1],'layer_count')
        for off,key in [(0x40,'position'),(0x50,'rotation'),(0x60,'scale'),(0xbd0,'null_scale')]:
            values=md[key];m(off,'<'+str(len(values))+'f',values,key)
        for offset,fmt,key in [(4,'H','slot'),(6,'H','internal_flags'),(8,'H','previous_flags'),(12,'I','attr')]:
            m(0x240+offset,'<'+fmt,[layer.get(key,0)],key)
        for offset,key,default in [(0x48,'blend',1),(0x54,'frame_max',10),(0x64,'sample_frame',4),
            (0x68,'next_frame',5),(0xe4,'translation_weight',1),(0xf4,'rotation_weight',1)]:
            m(0x240+offset,'<f',[layer.get(key,default)],key)
        for offset,key in [(0x78,'previous_rotation'),(0xa8,'previous_translation')]:
            values=layer[key];m(0x240+offset,'<'+str(len(values))+'f',values,key)
        for address,values in case['globals'].items():
            address=int(address,0)
            if address not in GLOBAL_DEFAULTS or len(values)!=len(GLOBAL_DEFAULTS[address]):raise ValueError('Undeclared global template')
            write(address,'<'+str(len(values))+'f',values,'synthetic global template')
        for offset,fmt,values in case.get('extra_model_fields',[]):m(int(offset,0),fmt,values,'explicit additional synthetic field')
        # All CPU registers are explicit fixture state. VFP access is enabled;
        # FPSCR is a parameter, not an inferred state of the original game.
        for i in range(13):mu.reg_write(getattr(a,'UC_ARM_REG_R'+str(i)),0)
        for i in range(32):mu.reg_write(getattr(a,'UC_ARM_REG_S'+str(i)),0)
        mu.reg_write(a.UC_ARM_REG_CPSR,0x10)
        mu.reg_write(a.UC_ARM_REG_C1_C0_2,0xf00000)
        mu.reg_write(a.UC_ARM_REG_FPEXC,1<<30)
        mu.reg_write(a.UC_ARM_REG_FPSCR,case.get('fpscr',0))
        mu.reg_write(a.UC_ARM_REG_R0,MODEL);mu.reg_write(a.UC_ARM_REG_SP,STACK+0xff00);mu.reg_write(a.UC_ARM_REG_LR,RETURN)
        if entry not in ('apply','advance','pre-update'):raise ValueError('Unknown routine entry')
        execute_pre_update=entry=='pre-update' or entry=='advance' and case.get('pre_advance_update')=='execute-pinned'
        allowed=[(APPLY,0x95de18)] if entry=='apply' else [(ADVANCE,0x9574dc),(0x957f80,0x958730)] if entry=='advance' else []
        if execute_pre_update:allowed.extend([(PRE_UPDATE,0x95b930),(APPLY,0x95de18)])
        def reject(reason,**kw):failures.append({'reason':reason,'pc':hex(mu.reg_read(a.UC_ARM_REG_PC)),**kw});mu.emu_stop()
        def on_code(engine,pc,size,user):
            if pc==RETURN:engine.emu_stop();return
            if pc==SAMPLER:
                address=engine.reg_read(a.UC_ARM_REG_R0);frame=fp(engine.reg_read(a.UC_ARM_REG_S0))
                kind={MODEL+0x318:'translation',MODEL+0x328:'rotation'}.get(address)
                samples=[x for x in case['samples'] if bits(x['frame'])==bits(frame)]
                if kind is None or len(samples)!=1:reject('Unexpected sampler input',address=hex(address),frame=frame);return
                values=samples[0][kind];calls.append({'pc':hex(pc),'lr':hex(engine.reg_read(a.UC_ARM_REG_LR)),
                    'kind':kind,'frame':frame,'result':values})
                if case.get('poison_sampler_scratch',False):
                    for reg in (a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3,a.UC_ARM_REG_R12):engine.reg_write(reg,0x5a5a5a5a)
                    for i in range(4,16):engine.reg_write(getattr(a,'UC_ARM_REG_S'+str(i)),bits(100+i))
                for i,value in enumerate(values):engine.reg_write(getattr(a,'UC_ARM_REG_S'+str(i)),bits(value))
                engine.reg_write(a.UC_ARM_REG_PC,engine.reg_read(a.UC_ARM_REG_LR));return
            if execute_pre_update and pc in (PRE_UPDATE,APPLY):
                calls.append({'pc':hex(pc),'execution':'original pinned instructions','lr':hex(engine.reg_read(a.UC_ARM_REG_LR))})
            if entry=='advance' and pc==PRE_UPDATE and not execute_pre_update:
                if case.get('pre_advance_update')!='explicit-no-op':
                    reject('Pre-advance model update requires explicit interception declaration');return
                calls.append({'pc':hex(pc),'intercept':'pre-advance model update is intentionally no-op'})
                if case.get('poison_pre_update_scratch',False):
                    for reg in (a.UC_ARM_REG_R0,a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3,a.UC_ARM_REG_R12):engine.reg_write(reg,0x5a5a5a5a)
                    for i in range(16):engine.reg_write(getattr(a,'UC_ARM_REG_S'+str(i)),bits(100+i))
                engine.reg_write(a.UC_ARM_REG_PC,engine.reg_read(a.UC_ARM_REG_LR));return
            if not any(start<=pc<end for start,end in allowed):reject('Execution escaped declared routine',address=hex(pc));return
            if entry=='advance' and pc==0x9585cc:
                # Observe only initialized locals after the original endpoint
                # adjustments; no register/memory replacement or clock model.
                sp=engine.reg_read(a.UC_ARM_REG_SP)
                if not all(x in ram_valid for x in range(sp,sp+24)):
                    reject('Event-plan locals not fully initialized');return
                begin,end,second,begin2,end2,first=struct.unpack('<6i',engine.mem_read(sp,24))
                event_plans.append({'first':bool(first),'second':bool(second),
                    'ranges':[[begin,end],[begin2,end2]]})
            executed.append(pc)
        def on_read(engine,access,address,size,value,user):
            if not all(x in ram_valid for x in range(address,address+size)) and not any(lo<=address and address+size<=hi for lo,hi in file_ranges):
                reject('Read of uninitialized synthetic/BSS memory',address=hex(address),size=size);return
            reads[(address,size)]=reads.get((address,size),0)+1
        def on_write(engine,access,address,size,value,user):
            if not (MODEL<=address and address+size<=MODEL+0x2000 or STACK<=address and address+size<=STACK+0x10000):
                reject('Write outside declared model/stack',address=hex(address),size=size);return
            ram_valid.update(range(address,address+size));written.update(range(address,address+size))
        def on_invalid(engine,access,address,size,value,user):
            reject('Unmapped/protected memory access',access=access,address=hex(address),size=size);return False
        hooks=[mu.hook_add(u.UC_HOOK_CODE,on_code),mu.hook_add(u.UC_HOOK_MEM_READ,on_read),
               mu.hook_add(u.UC_HOOK_MEM_WRITE,on_write),mu.hook_add(u.UC_HOOK_MEM_INVALID,on_invalid)]
        try:mu.emu_start({'apply':APPLY,'advance':ADVANCE,'pre-update':PRE_UPDATE}[entry],RETURN,timeout=2_000_000,count=200000)
        except u.UcError as error:failures.append({'reason':'Unicorn error','error':str(error),'pc':hex(mu.reg_read(a.UC_ARM_REG_PC))})
        pc=mu.reg_read(a.UC_ARM_REG_PC)
        if pc!=RETURN and not failures:failures.append({'reason':'Instruction/time limit or missing return','pc':hex(pc)})
        def output(offset,count):
            addr=MODEL+offset
            if not all(x in ram_valid for x in range(addr,addr+4*count)):return None
            return list(struct.unpack('<'+str(count)+'f',mu.mem_read(addr,4*count)))
        outputs={name:output(off,num) for name,off,num in [('position',0x40,3),('rotation',0x50,4),
            ('delta_position',0xbe0,3),('delta_rotation',0xbf0,4),('previous_translation',0x2e8,3),
            ('previous_rotation',0x2b8,4),('sampled_translation',0x308,3),('sampled_rotation',0x2d8,4),
            ('layer_next_delta_translation',0x270,3),('layer_next_delta_rotation',0x260,4),
            ('frame_accumulator',0x28c,1),('previous_frame',0x290,1)]}
        if entry in ('advance','pre-update'):
            outputs.update({name:output(off,num) for name,off,num in [('progress',0x254,1),('previous_step',0x258,1)]})
            for name,off,fmt in [('internal_flags',0x246,'H'),('previous_flags',0x248,'H'),('combined_attributes',0x238,'I'),
                                 ('event_masks',0x348,'4I'),('previous_event_masks',0x358,'4I')]:
                size=struct.calcsize('<'+fmt);addr=MODEL+off
                outputs[name]=list(struct.unpack('<'+fmt,mu.mem_read(addr,size))) if all(x in ram_valid for x in range(addr,addr+size)) else None
        if execute_pre_update:
            outputs.update({name:output(off,num) for name,off,num in [('sample_frame',0x2a4,1),('next_frame',0x2a8,1)]})
        result = {'name':case['name'],'status':'passed' if not failures else 'failed','fixture':case,'initializations':initial,
            'outputs':outputs,'calls':calls,'event_plans':event_plans,'failures':failures,'instruction_count':len(executed),
            'executed_addresses':[hex(x) for x in sorted(set(executed))],
            'reads':[{'address':hex(addr),'size':sz,'count':n} for (addr,sz),n in sorted(reads.items())],
            'model_written_offsets':[hex(x-MODEL) for x in sorted(written) if MODEL<=x<MODEL+0x2000],
            'fpscr_after':hex(mu.reg_read(a.UC_ARM_REG_FPSCR))}
        for hook in hooks:mu.hook_del(hook)
        return result


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--elf',type=Path,required=True)
    ap.add_argument('--cases',type=Path);ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();cases=json.loads(args.cases.read_text()) if args.cases else [default_case()]
    if len(cases)>1000:raise ValueError('Local policy: maximum 1000 fixtures')
    harness=Harness(args.elf);results=[harness.run(case) for case in cases]
    report={'producer':'Isolated Unicorn execution, not REA Evidence or original 3DS runtime',
        'elf_sha256':ELF_SHA,'harness_sha256':digest(Path(__file__).read_bytes()),'unicorn_version':harness.u.__version__,
        'cpu_model':'UC_CPU_ARM_11MPCORE','hardware_equivalence_claimed':False,'sampler_intercepted':True,
        'globals_status':'Explicit synthetic parameters; no BSS initialization claim','cases':results}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('x') as f:json.dump(report,f,indent=2);f.write('\n')
    print(json.dumps({'passed':sum(x['status']=='passed' for x in results),'failed':sum(x['status']=='failed' for x in results),
        'cases':[{'name':x['name'],'outputs':x['outputs'],'failures':x['failures']} for x in results]}))
    return int(any(x['status']=='failed' for x in results))
if __name__=='__main__':sys.exit(main())
