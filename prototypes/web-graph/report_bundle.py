#!/usr/bin/env python3
"""Publication J9 locale, Unicode, bornée et vérifiable sans écriture."""
from __future__ import annotations
import fcntl, hashlib, html, json, os, shutil, stat, tempfile
from pathlib import Path, PurePosixPath
import cairo, gi
gi.require_version("Pango", "1.0"); gi.require_version("PangoCairo", "1.0")
from gi.repository import Pango, PangoCairo

CONTRACT="labfy.investigation_report.v1"; MANIFEST="labfy.report_manifest.v1"; INTENT="labfy.report_intent.v1"
FILES={"report.json","report.html","report.pdf","NOTICE.txt","manifest.json"}; PAYLOAD=FILES-{"manifest.json"}
MEDIA={"report.json":"application/json","report.html":"text/html; charset=utf-8","report.pdf":"application/pdf","NOTICE.txt":"text/plain; charset=utf-8"}
MAX_FILE=6*1024*1024; MAX_TOTAL=8*1024*1024; MAX_MANIFEST=128*1024; MAX_PAGES=40

class ReportError(ValueError): pass
def canonical(value): return json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(",",":")).encode()+b"\n"
def _safe(value): return html.escape("" if value is None else str(value),quote=True)
def _atomic_json(path,value):
    temporary=path.with_name(path.name+f".{os.getpid()}.tmp");temporary.write_bytes(canonical(value));os.chmod(temporary,0o600);os.replace(temporary,path)
def _object_fields(item):
    keys=(("référence","id"),("nature","type"),("libellé","label"),("état","state"),("revue","review_state"),("rôle","selection_role"),("brut","value_raw"),("normalisé","value_normalized"),("corrigé","value_corrected"),("provenance","provenance_kind"),("outil","tool_id"),("version","tool_version"),("lacune","missing_provenance_reason"))
    return [(label,item.get(key) if item.get(key) is not None else "non disponible") for label,key in keys]

def render_html(document):
    sections=document.get("sections",{});parts=["<!doctype html><html lang='fr'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'><title>Rapport Labfy</title><style>body{font:14px system-ui;color:#1e1e2e;background:#fff;max-width:1100px;margin:auto;padding:2rem}h1,h2{color:#5c4fb5}dt{font-weight:bold}dd{overflow-wrap:anywhere}table{border-collapse:collapse;width:100%}th,td{border:1px solid #bbb;padding:.4rem;overflow-wrap:anywhere;text-align:left}tr{page-break-inside:avoid}</style></head><body>",f"<h1>{_safe(document['title'])}</h1><p><strong>Rapport local SPECIMEN.</strong> Généré : {_safe(document['generated_at'])}</p><p>Révision : <code>{_safe(document['revision'])}</code></p><h2>Commentaire de rédaction humaine</h2><p>{_safe(document.get('human_comment'))}</p>"]
    if sections.get("evidence"):
        parts.append("<h2>Sélection et justificatifs</h2>")
        for item in document["objects"]:
            parts.append(f"<section><h3>{_safe(item['id'])}</h3><dl>")
            parts.extend(f"<dt>{_safe(k)}</dt><dd>{_safe(v)}</dd>" for k,v in _object_fields(item));parts.append("</dl></section>")
    if sections.get("infrastructure"):
        parts.append("<h2>Réseau sélectionné</h2><p>Légende : → lien orienté ; — lien non orienté ; rapprochements exploratoires.</p><ul>")
        for edge in document["network"]:
            arrow="→" if edge.get("directed") else "—";parts.append(f"<li><code>{_safe(edge['source'])}</code> {arrow} {_safe(edge['semantic'])} {arrow} <code>{_safe(edge['target'])}</code> · {_safe(edge.get('review_state'))}</li>")
        parts.append("</ul>")
    if sections.get("timeline"):
        parts.append("<h2>Chronologie</h2><table><tr><th>Valeur</th><th>Catégorie</th><th>Objet</th><th>Fuseau</th><th>Précision</th></tr>")
        parts.extend(f"<tr><td>{_safe(e['raw_value'])}</td><td>{_safe(e['category'])}</td><td>{_safe(e['object_id'])}</td><td>{_safe(e['timezone'])}</td><td>{_safe(e['precision'])}</td></tr>" for e in document["timeline"]);parts.append("</table>")
    parts.append("<h2>Limites et prudence</h2><ul>"+"".join(f"<li>{_safe(x)}</li>" for x in document["limitations"])+"</ul><p>Ce dossier n’atteste ni l’authenticité ni la recevabilité judiciaire des sources.</p></body></html>")
    return "".join(parts).encode()

def _lines(document):
    sections=document.get("sections",{});lines=[document["title"],"Rapport local SPECIMEN",f"Révision : {document['revision']}",f"Généré : {document['generated_at']}","Commentaire de rédaction humaine : "+str(document.get("human_comment") or "")]
    if sections.get("evidence"):
        lines += ["","SÉLECTION ET JUSTIFICATIFS"]
        for item in document["objects"]: lines += [""]+[f"{k} : {v}" for k,v in _object_fields(item)]
    if sections.get("timeline"):
        lines += ["","CHRONOLOGIE"]+[f"{e['raw_value']} · {e['category']} · {e['object_id']} · {e['timezone']} · {e['precision']}" for e in document["timeline"]]
    if sections.get("infrastructure"):
        lines += ["","RÉSEAU SÉLECTIONNÉ","Légende : → lien orienté ; — lien non orienté ; rapprochement exploratoire."]
        for edge in document["network"]:
            arrow="→" if edge.get("directed") else "—";lines.append(f"{edge['source']} {arrow} {edge['semantic']} {arrow} {edge['target']} · {edge.get('review_state') or 'non disponible'}")
    return lines+["","LIMITES"]+["• "+x for x in document["limitations"]]

def render_pdf(document,target):
    surface=cairo.PDFSurface(str(target),595,842);context=cairo.Context(surface);layout=PangoCairo.create_layout(context);layout.set_font_description(Pango.FontDescription("DejaVu Sans 9.5"));layout.set_width(int(511*Pango.SCALE));layout.set_wrap(Pango.WrapMode.WORD_CHAR);y=42;pages=1
    for line in _lines(document):
        layout.set_text(line,-1);_,logical=layout.get_pixel_extents();height=max(14,logical.height+4)
        if height>758: surface.finish();raise ReportError("Une valeur ne peut pas être paginée sans perte")
        if y+height>800:
            context.show_page();pages+=1;y=42
            if pages>MAX_PAGES: surface.finish();raise ReportError("Le rapport dépasse la limite de pages")
        context.move_to(42,y);PangoCairo.show_layout(context,layout);y+=height
    surface.finish();return pages

def _load(path,limit):
    if path.stat(follow_symlinks=False).st_size>limit: raise ReportError("json_too_large")
    def pairs(values):
        result={}
        for key,value in values:
            if key in result: raise ReportError("duplicate_json_key")
            result[key]=value
        return result
    try:return json.loads(path.read_bytes().decode("utf-8"),object_pairs_hook=pairs)
    except (UnicodeDecodeError,json.JSONDecodeError) as error:raise ReportError("json_invalid") from error
def _sha(value):return isinstance(value,str) and len(value)==64 and all(c in "0123456789abcdef" for c in value)

def verify(directory:Path,expected_report_id=None):
    errors=[]
    try:
        if directory.is_symlink():raise ReportError("bundle_symlink")
        if not stat.S_ISDIR(directory.stat(follow_symlinks=False).st_mode):raise ReportError("bundle_absent")
        entries=list(os.scandir(directory));names=set();total=0
        if len(entries)>len(FILES):errors.append("bundle_file_count")
        for entry in entries:
            info=entry.stat(follow_symlinks=False);names.add(entry.name);total+=min(info.st_size,MAX_TOTAL+1)
            if stat.S_ISLNK(info.st_mode):errors.append("symlink:"+entry.name)
            elif not stat.S_ISREG(info.st_mode):errors.append("not_regular:"+entry.name)
            elif info.st_size>MAX_FILE:errors.append("file_limit:"+entry.name)
        if total>MAX_TOTAL:errors.append("bundle_size")
        if names-FILES:errors.append("unexpected:"+",".join(sorted(names-FILES)))
        if FILES-names:errors.append("missing:"+",".join(sorted(FILES-names)))
        if errors:return {"contract":"labfy.report_verification.v1","valid":False,"errors":errors}
        manifest=_load(directory/"manifest.json",MAX_MANIFEST);document=_load(directory/"report.json",MAX_FILE)
        if not isinstance(manifest,dict):raise ReportError("manifest_root_type")
        if not isinstance(document,dict):raise ReportError("document_root_type")
        if manifest.get("contract")!=MANIFEST:errors.append("unknown_manifest_contract")
        if document.get("contract")!=CONTRACT:errors.append("unknown_report_contract")
        files=manifest.get("files")
        if not isinstance(files,list):raise ReportError("manifest_files_type")
        listed=[]
        for entry in files:
            if not isinstance(entry,dict):raise ReportError("manifest_entry_type")
            if set(entry)!={"path","media_type","size","sha256"}:raise ReportError("manifest_entry_shape")
            path=PurePosixPath(entry["path"]) if isinstance(entry.get("path"),str) else None
            if path is None or path.is_absolute() or ".." in path.parts or len(path.parts)!=1:raise ReportError("unsafe_path")
            name=path.as_posix();listed.append(name)
            if name not in PAYLOAD or entry["media_type"]!=MEDIA.get(name):errors.append("file_metadata:"+name)
            if type(entry.get("size")) is not int or not 0<=entry["size"]<=MAX_FILE:errors.append("size_type:"+name)
            if not _sha(entry.get("sha256")):errors.append("hash_type:"+name)
            if name in PAYLOAD:
                data=(directory/name).read_bytes()
                if len(data)!=entry.get("size"):errors.append("size:"+name)
                if hashlib.sha256(data).hexdigest()!=entry.get("sha256"):errors.append("hash:"+name)
        if len(listed)!=len(set(listed)):errors.append("duplicate_manifest_path")
        if set(listed)!=PAYLOAD:errors.append("file_list")
        selected=[x.get("id") for x in document.get("objects",[]) if isinstance(x,dict) and x.get("selection_role")=="selected"]
        for dk,mk in (("investigation_id","investigation_id"),("revision","report_revision"),("profile","profile"),("rule_version","rule_version")):
            if document.get(dk)!=manifest.get(mk):errors.append("coherence:"+mk)
        if selected!=manifest.get("selection"):errors.append("coherence:selection")
        identity=expected_report_id if expected_report_id is not None else directory.name
        if identity!=manifest.get("report_id"):errors.append("coherence:report_id")
        if hashlib.sha256(canonical(document)).hexdigest()!=manifest.get("document_sha256"):errors.append("coherence:document_sha256")
    except (OSError,TypeError,ValueError,KeyError,ReportError) as error:errors.append(str(error) or "verification_error")
    return {"contract":"labfy.report_verification.v1","valid":not errors,"errors":errors}

def publish(document,workspace:Path,intention:str,*,fail_at=None):
    if not isinstance(document,dict) or document.get("contract")!=CONTRACT or not isinstance(intention,str) or not intention or len(intention)>128:raise ReportError("Document ou intention de rapport invalide")
    root=workspace/"exports/reports";state=workspace/".labfy/reports";root.mkdir(mode=0o700,parents=True,exist_ok=True)
    for path in (state/"staging",state/"intents",state/"locks"):path.mkdir(mode=0o700,parents=True,exist_ok=True)
    fingerprint=hashlib.sha256(canonical(document)).hexdigest();key=hashlib.sha256((document["investigation_id"]+"\0"+intention).encode()).hexdigest();report_id=key[:32];final=root/report_id;intent_path=state/"intents"/(key+".json")
    with (state/"locks"/(key+".lock")).open("a+b") as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        if intent_path.exists():
            prior=_load(intent_path,MAX_MANIFEST)
            if not isinstance(prior,dict) or prior.get("contract")!=INTENT:raise ReportError("Intention durable illisible")
            if prior.get("fingerprint")!=fingerprint:raise ReportError("Intention déjà utilisée avec un autre document")
            if final.exists():
                if verify(final)["valid"]:return report_id,True
                raise ReportError("RECOVERY_REQUIRED: publication existante non vérifiable")
        else:_atomic_json(intent_path,{"contract":INTENT,"report_id":report_id,"investigation_id":document["investigation_id"],"fingerprint":fingerprint,"state":"GENERATING"})
        if fail_at=="intent":raise ReportError("Faute synthétique après intention durable")
        staging=Path(tempfile.mkdtemp(prefix=report_id+"-",dir=state/"staging"));os.chmod(staging,0o700)
        try:
            (staging/"report.json").write_bytes(canonical(document));(staging/"report.html").write_bytes(render_html(document));(staging/"NOTICE.txt").write_text("Vérifier avec: python3 report_bundle.py verify DOSSIER\nIntégrité interne sans preuve d'authenticité.\n",encoding="utf-8")
            if fail_at=="staging":raise ReportError("Faute synthétique après staging")
            render_pdf(document,staging/"report.pdf")
            if fail_at=="pdf":raise ReportError("Faute synthétique de rendu PDF")
            entries=[]
            for name in sorted(PAYLOAD):
                data=(staging/name).read_bytes();entries.append({"path":name,"media_type":MEDIA[name],"size":len(data),"sha256":hashlib.sha256(data).hexdigest()})
            manifest={"contract":MANIFEST,"report_id":report_id,"investigation_id":document["investigation_id"],"report_revision":document["revision"],"document_sha256":fingerprint,"selection":[x["id"] for x in document["objects"] if x["selection_role"]=="selected"],"rule_version":document["rule_version"],"profile":document["profile"],"exclusions":["originals","sqlite","full_eml","stdout_stderr","absolute_paths","secrets"],"files":entries}
            (staging/"manifest.json").write_bytes(canonical(manifest))
            if not verify(staging,report_id)["valid"]:raise ReportError("Bundle produit invalide")
            if final.exists():raise ReportError("RECOVERY_REQUIRED: destination déjà occupée")
            staging.rename(final)
            if fail_at=="published":raise ReportError("Faute synthétique après publication")
            _atomic_json(intent_path,{"contract":INTENT,"report_id":report_id,"investigation_id":document["investigation_id"],"fingerprint":fingerprint,"state":"READY"})
            if fail_at=="receipt":raise ReportError("Faute synthétique après reçu")
            return report_id,False
        finally:
            if staging.exists():shutil.rmtree(staging)

if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest="command",required=True);check=sub.add_parser("verify");check.add_argument("directory",type=Path);args=parser.parse_args();result=verify(args.directory);print(json.dumps(result,ensure_ascii=False,sort_keys=True));raise SystemExit(0 if result["valid"] else 1)
