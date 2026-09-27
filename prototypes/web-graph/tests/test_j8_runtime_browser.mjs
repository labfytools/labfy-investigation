import assert from "node:assert/strict";
import {access, mkdtemp, readFile, rm} from "node:fs/promises";
import {tmpdir} from "node:os";
import {join} from "node:path";
import {spawn, spawnSync} from "node:child_process";
import puppeteer from "puppeteer-core";
import { openAutomaticSession } from "./browser_automatic_session.mjs";

const REPOSITORY=new URL("../../..",import.meta.url),PROTOTYPE=new URL("..",import.meta.url);
const FIREFOX_PATH=process.env.FIREFOX_PATH??"/usr/bin/firefox";
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function ready(server){return new Promise((resolve,reject)=>{let output="";const timer=setTimeout(()=>reject(new Error("démarrage runtime J8 expiré")),8000);server.stdout.on("data",chunk=>{output+=chunk;const url=output.match(/http:\/\/127\.0\.0\.1:(\d+)\//);if(url){clearTimeout(timer);resolve({url:url[0]});}});});}
async function waitFile(path){for(let i=0;i<200;i++){try{await access(path);return;}catch{}await delay(10);}throw new Error(`barrière absente: ${path}`);}
async function post(page,path,body){return page.evaluate(async({path,body})=>{const session=await fetch("/api/v1/session").then(r=>r.json());const response=await fetch(path,{method:"POST",headers:{"Content-Type":"application/json","X-Labfy-CSRF":session.csrf},body:JSON.stringify(body)});return {status:response.status,body:await response.json()};},{path,body});}
async function scenario(kind){const workspace=await mkdtemp(join(tmpdir(),`labfy-j8-${kind}-`)),profile=await mkdtemp(join(tmpdir(),`labfy-j8-${kind}-firefox-`));let browser,server;try{
  assert.equal(spawnSync("./tools/local-jobs",["init-j7-specimen","--workspace",workspace],{cwd:REPOSITORY,encoding:"utf8"}).status,0);spawnSync("./tools/local-jobs",["export","--workspace",workspace],{cwd:REPOSITORY});
  const maximum=kind==="limit"?"100":"5000";server=spawn("python3",["workspace_server.py","--workspace",workspace,"--bridge","../../tools/local-jobs-test","--port","0"],{cwd:PROTOTYPE,env:{...process.env,PYTHONUNBUFFERED:"1",LABFY_TEST_PLAN_ACTIVE_MS:maximum,LABFY_TEST_TOOL_DELAY_MS:"2000"},stdio:["ignore","pipe","pipe"]});const session=await ready(server);
  browser=await puppeteer.launch({browser:"firefox",executablePath:FIREFOX_PATH,headless:true,userDataDir:profile,args:["--no-remote"]});const page=await browser.newPage();await openAutomaticSession(page,session.url);const planner=JSON.parse(await readFile(join(workspace,"planner-snapshot.json"))),ids=planner.recommendations.filter(x=>x.kind==="ANALYSIS"&&x.available).slice(0,kind==="limit"?2:1).map(x=>x.id);const response=await post(page,"/api/v1/plans",{recommendation_ids:ids,input_revision:planner.input_revision,profile_id:"SPECIMEN_SMALL",idempotency_key:kind==="limit"?"89100000-0000-4000-8000-000000000001":"89200000-0000-4000-8000-000000000001"});assert.equal(response.status,202);const marker=join(workspace,".labfy/runtime/test-tool-started");await waitFile(marker);
  if(kind==="cancel"){const jobId=spawnSync("sqlite3",[join(workspace,".labfy/runtime/jobs.sqlite"),"SELECT job_id FROM jobs WHERE state='RUNNING';"],{encoding:"utf8"}).stdout.trim();assert.ok(jobId);const pid=Number((await readFile(marker,"utf8")).trim());assert.equal((await post(page,`/api/v1/jobs/${jobId}/cancel`,{})).status,200);await page.waitForFunction(()=>[...document.querySelectorAll("#jobs-list li")].some(x=>x.dataset.jobState==="CANCELLED"),{timeout:10000});assert.throws(()=>process.kill(pid,0));}
  else {await page.waitForFunction(()=>document.querySelector("#plans-list")?.textContent.includes("LIMIT_REACHED"),{timeout:10000});const snapshot=JSON.parse(await readFile(join(workspace,"jobs-snapshot.json")));assert.equal(snapshot.jobs.filter(x=>x.state==="BLOCKED").length,1);assert.equal(snapshot.jobs.filter(x=>x.state==="QUEUED").length,1);assert.equal(snapshot.plans[0].remaining_active_ms,0);}
}finally{if(browser)await browser.close();if(server){server.kill("SIGINT");await new Promise(resolve=>server.once("exit",resolve));}await rm(profile,{recursive:true,force:true});await rm(workspace,{recursive:true,force:true});}}
await scenario("limit");await scenario("cancel");console.log("PASS navigateur Firefox : J8 B limite en exécution et E annulation RUNNING après barrière");
