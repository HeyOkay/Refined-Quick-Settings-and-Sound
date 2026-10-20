from pathlib import Path
from PIL import Image,ImageChops,GifImagePlugin
import subprocess,time,sys
r=Path(sys.argv[1]);o=Path(sys.argv[2])
p=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-i',str(r/'presentation-final.mkv'),'-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE)
prev=None;n=0;start=time.monotonic()
with o.open('wb') as f:
 while True:
  raw=p.stdout.read(490*885*3)
  if not raw:break
  im=Image.frombytes('RGB',(490,885),raw)
  if prev is None:box=(0,0,490,885)
  else:box=ImageChops.difference(im,prev).getbbox() or (0,0,1,1)
  patch=im.crop(box);pal=patch.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
  q=patch.quantize(palette=pal,dither=Image.Dither.FLOYDSTEINBERG)
  if prev is None:
   for block in GifImagePlugin._get_global_header(q,{'loop':0}):f.write(block)
  GifImagePlugin._write_frame_data(f,q,(box[0],box[1]),{'duration':20,'disposal':1,'include_color_table':prev is not None})
  prev=im;n+=1
  if n%200==0:print('GIF frames:',n,flush=True)
 f.write(b';')
p.stdout.close();assert p.wait()==0
print('HQ GIF:',n,'frames;',o.stat().st_size,'bytes;',round(time.monotonic()-start,1),'seconds')
