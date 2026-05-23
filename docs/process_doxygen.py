import os,yaml
from glob import glob

os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/docs_root")

all_files = glob("doxygen/*.md")

def get_target_path(fname, omit_fname, omit_parent=False):
    parent = '/'.join(fname.split('/')[:-1])
    fname = fname.split('/')[-1]
    result = '/'.join(fname.split('-')[:(-1 if omit_fname else None)])
    if omit_parent:
        return result
    return f'{parent}/{result}'

def replace_all_links(fname):
    with open(fname, 'r') as f:
        contents = f.read()
        
    r = fname.split('/')[-1]
    p = '../' * r.count('-')

    for filename in all_files:
        reference = filename.split('/')[-1]
        contents = contents.replace(reference, p + get_target_path(filename, False, True))
    
    with open(fname, 'w') as f:
        f.write(contents)

print("Build directory scaffold...")
for new_dir in set([get_target_path(f, True) for f in all_files]):
    os.makedirs(new_dir, exist_ok=True)

print("Replace links...")
for fname in all_files:
    replace_all_links(fname)
    
print("Move all markdown files to target paths...")
all_targets = [get_target_path(fname, False) for fname in all_files]
for fname,target in zip(all_files,all_targets):
    os.rename(fname, target)

print("Build Doxygen-based mkdocs navigation...")
nav_section = []

def add_nav_entry(nav, target, value):
    if len(target) == 1:
        if value is not None:
            nav.append({target[0]: value})
        return
    
    for subdict in nav:
        if target[0] in subdict:
            add_nav_entry(subdict[target[0]], target[1:], value)
            break
    else:
        subdict = {target[0]: []}
        nav.append(subdict)
        add_nav_entry(subdict[target[0]], target[1:], value)

for fname in all_files:
    target_split = get_target_path(fname, False, True).split('/')
    value = target_split[-1]
    target_split[-1] = ' ' + '.'.join(target_split[-1].split('.')[:-1])
    add_nav_entry(nav_section, target_split, get_target_path(fname, False))

def gen_sort_key(name):
    if name.startswith(' '):
        return name[1:] + '1'
    else:
        return name + '9'
    
def sort_nav_list(nav):
    if type(nav) != list:
        return
    nav.sort(key=lambda subdict: gen_sort_key(list(subdict.keys())[0]))
    for subdict in nav:
        assert len(subdict.keys()) == 1
        key = list(subdict.keys())[0]
        value = subdict[key]
        subdict.clear()
        subdict[key.strip()] = value
        sort_nav_list(value)

sort_nav_list(nav_section)

print("Build updated mkdocs.yml...")
with open('../mkdocs_source.yml', 'r') as f:
    data = yaml.safe_load(f)

def replace_doxygen_stub(data, repl):
    if type(data) == dict:
        for _,val in data.items():
            replace_doxygen_stub(val, repl)
    if type(data) == list:
        if '__doxygen_nav_stub__' in data:
            data[data.index('__doxygen_nav_stub__')] = repl
        else:
            for val in data:
                replace_doxygen_stub(val, repl)

replace_doxygen_stub(data, {'Doxygen - Full Reference': nav_section})

with open('../mkdocs.yml', 'w+') as f:
    f.write(yaml.dump(data))

print("Done")