"""Read-only constant-pool API inventory; references do not prove execution."""
import argparse
import hashlib
import json
import re
import struct
import zipfile
from collections import Counter
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def references(data):
    if data[:4]!=b'\xca\xfe\xba\xbe':raise ValueError('Invalid class magic')
    count=struct.unpack_from('>H',data,8)[0];cp=[None]*count;offset=10;i=1
    while i<count:
        tag=data[offset];offset+=1
        if tag==1:
            size=struct.unpack_from('>H',data,offset)[0];offset+=2
            if offset+size>len(data):raise ValueError('Truncated UTF8')
            cp[i]=(tag,data[offset:offset+size].decode('utf-8',errors='replace'));offset+=size
        elif tag in (7,8,16,19,20):
            cp[i]=(tag,struct.unpack_from('>H',data,offset)[0]);offset+=2
        elif tag in (9,10,11,12,17,18):
            cp[i]=(tag,*struct.unpack_from('>HH',data,offset));offset+=4
        elif tag in (3,4):offset+=4
        elif tag in (5,6):offset+=8;i+=1
        elif tag==15:offset+=3
        else:raise ValueError(f'Unsupported constant pool tag {tag}')
        if offset>len(data):raise ValueError('Truncated constant pool')
        i+=1
    classes=set();methods=set()
    for entry in cp:
        if not entry:continue
        if entry[0]==7:classes.add(cp[entry[1]][1])
        if entry[0] in (10,11):
            owner=cp[cp[entry[1]][1]][1];pair=cp[entry[2]]
            methods.add(owner+'.'+cp[pair[1]][1]+cp[pair[2]][1])
    return classes,methods

def main():
    parser=argparse.ArgumentParser();parser.add_argument('folder',type=Path)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    source=(ROOT/'native/vm.c').read_text()
    section=source[source.index('static int builtin_name('):source.index('static void freeclass(')]
    builtins=set(re.findall(r'"((?:java/|javax/|com/)[^"]+)"',section))
    def known(name):return name in builtins or name.endswith(('Exception','Error')) or name=='java/lang/Throwable'
    files=[];methods_all=Counter();missing_all=Counter()
    for path in sorted(args.folder.rglob('*')):
        if not path.is_file() or path.suffix.lower()!='.jar':continue
        raw=path.read_bytes();classes=set();methods=set();warnings=[]
        try:
            with zipfile.ZipFile(path) as archive:
                bundled={info.filename[:-6] for info in archive.infolist() if info.filename.endswith('.class')}
                for info in archive.infolist():
                    if not info.filename.endswith('.class'):continue
                    if info.file_size>320*1024:warnings.append(info.filename+': exceeds runtime member limit');continue
                    try:
                        names,calls=references(archive.read(info));classes.update(names);methods.update(calls)
                    except (ValueError,IndexError,struct.error,RuntimeError,zipfile.BadZipFile) as error:warnings.append(info.filename+': '+str(error))
                required={name for name in classes if not name.startswith('[') and name not in bundled}
                missing=sorted(name for name in required if not known(name))
                external=sorted(call for call in methods if call.split('.',1)[0] not in bundled or known(call.split('.',1)[0]))
        except (OSError,zipfile.BadZipFile,RuntimeError) as error:
            warnings.append(str(error));bundled=set();missing=[];external=[]
        methods_all.update(external);missing_all.update(missing)
        files.append(dict(path=str(path),sha256=hashlib.sha256(raw).hexdigest(),class_count=len(bundled),
                          external_method_references=external,unregistered_class_references=missing,warnings=warnings))
    args.output.mkdir(parents=True,exist_ok=True)
    report=dict(source=str(args.folder),limitation='Static references may be optional/unreachable; registered classes may still lack methods or semantics. This is not CLDC/MIDP certification.',
                files=files,unregistered_classes_by_jar_count=dict(missing_all.most_common()),external_methods_by_jar_count=dict(methods_all.most_common()))
    (args.output/'api-inventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    lines=['# Tham chiếu API trong toàn bộ JAR','',report['limitation'],'',
           f'{len(files)} JAR; chỉ đọc ZIP/class constant pool, không sửa tệp game.','',
           '| Class chưa đăng ký trong lõi | Số JAR tham chiếu |','|---|---|',
           *[f'| {name} | {count} |' for name,count in missing_all.most_common()], '',
           'Chi tiết class/method theo từng JAR và SHA-256: api-inventory.json. Không suy tỷ lệ tương thích từ danh sách này.']
    (args.output/'api-inventory.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(f'{len(files)} JAR inventoried; {len(missing_all)} unregistered class names; {len(methods_all)} external method signatures')

if __name__=='__main__':main()
