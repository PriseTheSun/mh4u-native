// Local public MCP client. Keeps one REA import alive and retains complete replies.
import {createRequire} from 'node:module';
import {readFile,writeFile,mkdir,appendFile} from 'node:fs/promises';
import {createInterface} from 'node:readline';
import {fileURLToPath} from 'node:url';
import path from 'node:path';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const configPath=process.argv[2]??path.join(root,'research/private/analysis-tools.json');
const cfg=JSON.parse(await readFile(configPath,'utf8'));
const require=createRequire(cfg.rea_launcher);
const {Client}=require('@modelcontextprotocol/client');
const {StdioClientTransport}=require('@modelcontextprotocol/client/stdio');
const out=path.join(root,'research/private/rea-session-'+Date.now());
await mkdir(out);
const client=new Client({name:'mh4u-native-investigation',version:'0.1.0'});
const env={...process.env,GHIDRA_INSTALL_DIR:cfg.ghidra_install_dir,JAVA_HOME:cfg.java_home,
  GHIDRA_HEADLESS_MAXMEM:cfg.heap,REA_GHIDRA_STARTUP_TIMEOUT_MS:String(cfg.startup_timeout_ms),
  REA_GHIDRA_LANGUAGE_ID:cfg.language_id,REA_LOG_LEVEL:'info'};
if(cfg.disabled_analyzers?.length)env.REA_GHIDRA_DISABLED_ANALYZERS=cfg.disabled_analyzers.join(',');
const transport=new StdioClientTransport({command:cfg.node,args:[cfg.rea_launcher,'--mcp'],env,stderr:'pipe'});
const options={timeout:cfg.startup_timeout_ms+240000};
let opened=false;
let index=0;
let logWrite=Promise.resolve();
let logError;
await client.connect(transport,options);
transport.stderr?.on('data',data=>{
  logWrite=logWrite.then(()=>appendFile(path.join(out,'server.stderr'),data)).catch(error=>{logError=error;});
});
const catalog=await client.listTools();
await writeFile(path.join(out,'tools.json'),JSON.stringify(catalog,null,2)+'\n');
await writeFile(path.join(root,'research/private/current-rea-session.txt'),out+'\n');
console.log(JSON.stringify({ready:true,output:out,tools:catalog.tools.length}));
const input=createInterface({input:process.stdin,crlfDelay:Infinity});
let interrupted=false;
let activeAbort;
const interrupt=()=>{interrupted=true;activeAbort?.abort();input.close();};
process.once('SIGINT',interrupt);
try {
  for await (const line of input) {
    if(interrupted)break;
    if(!line.trim())continue;
    if(line.trim()==='exit')break;
    try {
      const request=JSON.parse(line);
      if(typeof request.name!=='string'||!catalog.tools.some(t=>t.name===request.name))throw new Error('Unadvertised tool');
      const start=performance.now();
      activeAbort=new AbortController();
      const result=await client.callTool(request,{...options,signal:activeAbort.signal});
      activeAbort=undefined;
      const filename=`${String(++index).padStart(3,'0')}-${request.name}.json`;
      await writeFile(path.join(out,filename),JSON.stringify({request,elapsed_ms:performance.now()-start,response:result},null,2)+'\n');
      if(request.name==='open_binary'&&!result.isError)opened=true;
      if(request.name==='close_binary'&&!result.isError)opened=false;
      let body=result.structuredContent;
      if(!body){const t=result.content?.find(x=>x.type==='text');try{body=JSON.parse(t?.text??'null');}catch{body=null;}}
      console.log(JSON.stringify({file:filename,isError:result.isError??false,elapsed_ms:Math.round(performance.now()-start),
        keys:body&&typeof body==='object'?Object.keys(body):[],id:body?.id??body?.evidence_id??null}));
    }catch(error){console.log(JSON.stringify({error:String(error)}));}
  }
} finally {
  process.removeListener('SIGINT',interrupt);
  input.close();
  if(opened){
    try{
      const result=await client.callTool({name:'close_binary',arguments:{}},options);
      await writeFile(path.join(out,'final-close.json'),JSON.stringify(result,null,2)+'\n');
      console.log(JSON.stringify({closed:!result.isError}));
    }catch(error){console.error('Session cleanup failed: '+String(error));process.exitCode=1;}
  }
  await client.close();
  await logWrite;
  if(logError){console.error('Evidence log write failed: '+String(logError));process.exitCode=1;}
}
