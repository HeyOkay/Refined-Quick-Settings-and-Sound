from pathlib import Path
import subprocess,re,json,struct,io,sys,winreg
import bisect,csv
from PIL import Image,ImageDraw,ImageFilter
r=Path(sys.argv[1]);source=r/'presentation-hq.mkv'
log=(r/'capture.log').read_text(encoding='utf-8-sig')
tb=re.search(r'config in time_base:\s*(\d+)/(\d+)',log)
first=re.search(r'\bn:\s*0\s+pts:\s*(\d+)',log)
if not(tb and first):raise RuntimeError('Missing capture timestamps')
start_us=int(first.group(1))*int(tb.group(1))*1_000_000/int(tb.group(2))
positions=[tuple(map(int,row)) for row in csv.reader((r/'cursor.csv').open())]; times=[p[0] for p in positions]
assert abs(positions[0][0]-start_us)<3_000_000,(start_us,positions[0][0])
# Use original system ANI frames, original hotspots and native jiffy timing.
with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Control Panel\Cursors') as key:cursor_path=winreg.QueryValueEx(key,'Arrow')[0]
ani=Path(cursor_path).read_bytes()
def chunks(data,at=12):
 while at+8<=len(data):
  tag,size=struct.unpack_from('<4sI',data,at);yield tag,data[at+8:at+8+size];at+=8+size+(size&1)
icons=[];rates=None;sequence=None;header=None
for tag,value in chunks(ani):
 if tag==b'anih':header=struct.unpack('<9I',value[:36])
 elif tag==b'rate':rates=list(struct.unpack('<'+'I'*(len(value)//4),value))
 elif tag==b'seq ':sequence=list(struct.unpack('<'+'I'*(len(value)//4),value))
 elif tag==b'LIST' and value[:4]==b'fram':icons=[v for t,v in chunks(value,4) if t==b'icon']
assert header and icons
frames=[];hotspots=[]
for icon in icons:
 count=struct.unpack_from('<H',icon,4)[0]
 entries=[struct.unpack_from('<BBBBHHII',icon,6+i*16) for i in range(count)]
 entry=next(e for e in entries if (e[0] or 256)==32)
 w,h,colors,zero,hx,hy,size,offset=entry
 cur=struct.pack('<HHH',0,2,1)+struct.pack('<BBBBHHII',w,h,colors,zero,hx,hy,size,22)+icon[offset:offset+size]
 frames.append(Image.open(io.BytesIO(cur)).convert('RGBA'));hotspots.append((hx,hy))
sequence=sequence or list(range(header[2]));rates=rates or [header[7]]*len(sequence)
steps=[];total=0
for index,rate in zip(sequence,rates):total+=rate/60;steps.append(total)
print('Native ANI:',len(frames),'frames; cycle',total,'seconds; size',frames[0].size,'hotspot',hotspots[0],flush=True)
decode=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-i',str(source),'-vf','fps=50','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE)
encode=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','rawvideo','-pix_fmt','rgb24','-s','490x885','-r','50','-i','pipe:0','-an','-c:v','libx264rgb','-crf','0','-preset','ultrafast',str(r/'presentation-final.mkv')],stdin=subprocess.PIPE)
n=0
try:
 while True:
  raw=decode.stdout.read(490*1050*3)
  if not raw:break
  if len(raw)!=490*1050*3:raise RuntimeError('Incomplete frame')
  frame=Image.frombytes('RGB',(490,1050),raw)
  at=start_us+n*20_000
  i=max(0,min(len(positions)-2,bisect.bisect_right(times,at)-1));a0,a1=positions[i],positions[i+1]
  t=max(0,min(1,(at-a0[0])/max(1,a1[0]-a0[0])))
  step=min(len(sequence)-1,bisect.bisect_right(steps,(n/50)%total));index=sequence[step]
  cursor=frames[index];hx,hy=hotspots[index]
  x=(a0[1]+(a1[1]-a0[1])*t)-2070-hx
  y=(a0[2]+(a1[2]-a0[2])*t)-390-hy
  frame.paste(cursor,(round(x),round(y)),cursor)
  frame=frame.crop((0,165,490,1050))
  if n in [150,500,800]:frame.save(r/('system-frame-'+str(n)+'.png'))
  if n>=75:encode.stdin.write(frame.tobytes())
  n+=1
finally:
 decode.stdout.close();encode.stdin.close()
assert decode.wait()==0 and encode.wait()==0
(r/'system-render.json').write_text(json.dumps({'frames':n-75,'fps':50,'duration':(n-75)/50,'cursor':'original system Rainbow Glow Dark Gray ANI, all 72 native frames, original timing and hotspot','crop_top':165,'source':'RGB lossless'},indent=2),encoding='utf-8')
print('Composited',n-75,'frames with original native ANI animation; duration',(n-75)/50)
