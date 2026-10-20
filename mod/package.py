from pathlib import Path
import re
root=Path(__file__).parent
seen=set()
def expand(path):
    if path.name in seen:return ''
    seen.add(path.name)
    text=path.read_text(encoding='utf-8-sig')
    text=re.sub(r'^#pragma once\s*$', '',text,flags=re.M)
    return re.sub(r'^#include "([^"]+)"\s*$',lambda m:expand(root/m[1]),text,flags=re.M)
out=root/'dist';out.mkdir(exist_ok=True)
source=expand(root/'sound-native.wh.cpp')
for forbidden in ['CreateWindowExW(', 'DesktopWindowXamlSource', 'WindowsXamlManager::Initialize']:
    assert forbidden not in source, forbidden
(out/'refined-quick-settings-and-sound.wh.cpp').write_text(source+'\n/* Lucide license:\n'+(root/'LUCIDE-LICENSE.txt').read_text(encoding='utf-8')+'\n*/\n',encoding='utf-8')
print('Native source packaged:',out/'refined-quick-settings-and-sound.wh.cpp')
