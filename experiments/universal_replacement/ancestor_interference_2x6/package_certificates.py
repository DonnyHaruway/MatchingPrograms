"""Archive verified proof files, or stream-extract them for independent checks."""
import argparse
import hashlib
import json
import lzma
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parent


def digest(stream):
    h=hashlib.sha256()
    while True:
        block=stream.read(1024*1024)
        if not block:return h.hexdigest()
        h.update(block)


def package(remove_raw):
    dest=ROOT/'results'
    path=dest/'certificate_files.json'
    old=json.loads(path.read_text()) if path.exists() else {}
    metadata={}
    for mode in ('complete','incomplete'):
        search=json.loads((dest/f'{mode}_summary.json').read_text())
        check=json.loads((dest/f'{mode}_independent_verification.json').read_text())
        assert search['completed_requested_range'] and not search['stopped_on_target']
        assert search['all_branches_fail_inputs']==check['all_branches_fail_inputs']==0
        assert search['preference_profiles']==check['preference_profiles_verified']
        assert search['profile_graph_inputs']==check['profile_graph_inputs_verified']
        assert search['stable_path_certificates']==check['stable_deletion_paths_verified']
        raw=dest/f'{mode}_graph_cover.bin';archive=raw.with_suffix('.bin.xz')
        if raw.exists():
            with raw.open('rb') as source:sha=digest(source)
            if not (archive.exists() and old.get(mode,{}).get('uncompressed_sha256')==sha):
                with raw.open('rb') as source,lzma.open(archive,'wb',preset=4) as target:
                    shutil.copyfileobj(source,target,1024*1024)
            with lzma.open(archive,'rb') as source:assert digest(source)==sha
            size=raw.stat().st_size
        else:
            sha=old[mode]['uncompressed_sha256'];size=old[mode]['uncompressed_bytes']
            with lzma.open(archive,'rb') as source:assert digest(source)==sha
        with archive.open('rb') as source:compressed_sha=digest(source)
        metadata[mode]=dict(file=archive.name,uncompressed_bytes=size,compressed_bytes=archive.stat().st_size,
                            uncompressed_sha256=sha,compressed_sha256=compressed_sha,
                            independently_verified=True)
        if remove_raw and raw.exists():raw.unlink()
    path.write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps(metadata,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--remove-raw',action='store_true')
    parser.add_argument('--extract',nargs=2,metavar=('ARCHIVE','OUTPUT'))
    args=parser.parse_args()
    if args.extract:
        with lzma.open(args.extract[0],'rb') as source,open(args.extract[1],'wb') as target:
            shutil.copyfileobj(source,target,1024*1024)
    else:package(args.remove_raw)
