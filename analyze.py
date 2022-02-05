import re
import math
import pandas as pd
import matplotlib.pyplot as plt

MAX_DEPTH = 4
MIN_PERCENT = 0.5

df = pd.read_csv('~/.wndrecd.txt', index_col=False, delimiter=',', names=['time', 'action', 'win'])
df = df.loc[df.action != ' scrunlock']
df.time = pd.to_datetime(df.time)

df_tmp = df.iloc[:-1].copy()
df_tmp['duration'] = df.time.diff().iloc[1:].to_numpy()
df = df_tmp; del df_tmp
df = df.loc[df.action != ' stopped']

def extract_ent(name):
    global mx_level
    name = name.strip(' ')
    tokens = re.split('\s+[—-]\s+', name)
    return ' == '.join(tokens[::-1])

df['win_type'] = df.win.apply(extract_ent)
duration = df.groupby('win_type').duration.sum()
win_df = pd.DataFrame({'pct': duration / duration.sum(), 'dur':duration})
win_df.head()

tree = {}
count = 0
for k, v in win_df.iterrows():
    ptr = tree
    for i, d in enumerate(k.split(' == ')):
        if i == MAX_DEPTH - 1: break
        if d not in ptr:
            ptr[d] = {'__name__': d}
            count += 1
        ptr = ptr[d]
    ptr['__pct__'] = v['pct']
    ptr['__dur__'] = v['dur']
S = 2 * math.pi
idx = 0
def strfdelta(tdelta):
    d = {"days": tdelta.days}
    d["hours"], rem = divmod(tdelta.seconds, 3600)
    d["minutes"], d["seconds"] = divmod(rem, 60)
    return '{hours}:{minutes}:{seconds}'.format(**d)
def strf(name, dur, d, p, n=15):
    sp = [name[i: i + n] for i in range(0, len(name), n)]
    dur = strfdelta(dur)
    L = int(d * p * 40)
    if L <= 1:
        return sp[0] + ': ' + dur
    else:
        return '\n'.join(sp[:max(L - 1, 1)] + [dur])
def visit(p, acc=0, d=0):
    global idx
    idx += 1
    pct, dur = None, None
    if '__pct__' in p:
        pct, dur = p['__pct__'], p['__dur__']
    for k, v in p.items():
        if k.startswith('__'):
            continue
        vpct, vdur = visit(v, acc + (pct or 0), d+1)
        pct = vpct if pct is None else pct + vpct
        dur = vdur if dur is None else dur + vdur
    if d > 0:
        mid = (acc + pct / 2)
        plt.barh([d], [S * pct], [1], left=S * acc)
        rot = mid * 360
        if rot > 90 and rot < 270:
            rot = rot - 180
        annot = strf(p['__name__'], dur, d, pct)
        if pct > MIN_PERCENT / 100:
            plt.annotate(annot, (S * mid, d), ha='center', va='center', rotation=rot)
    return pct, dur

fig, ax = plt.subplots(subplot_kw=dict(projection="polar"), figsize=(10, 10))
pct, dur = visit(tree)
plt.annotate('Total\n' + strfdelta(dur), (0, 0), ha='center', va='center')
ax.set_axis_off()
plt.tight_layout()
plt.show()
