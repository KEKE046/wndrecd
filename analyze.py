import re
import math
import pandas as pd
import matplotlib.pyplot as plt
import datetime
import sys

args = sys.argv[1:]

def read_data(ltime=None, rtime=None, file='~/.wndrecd.txt'):
    df = pd.read_csv(file, index_col='time', delimiter=',',
                     names=['time', 'action', 'win'], 
                     dtype={'action': 'category', 'win': str},
                     parse_dates=['time'])
    df = df.loc[ltime:rtime].copy()
    df = df.loc[(df.action != ' scrunlock')&(df.action != ' stopped')]
    df_tmp = df.iloc[:-1].copy()
    df_tmp['duration'] = pd.Series(df.index).diff().iloc[1:].to_numpy()
    df = df_tmp; del df_tmp
    return df

ltime = rtime = None
today = datetime.date.today()
oneday = datetime.timedelta(days=1)
if 'today' in args:
    ltime = today
if 'yesterday' in args:
    if 'today' not in args:
        rtime = today
    ltime = today - oneday
date_pat = re.compile('\d\d\d\d-\d\d-\d\d')
day = tuple(filter(date_pat.fullmatch, args))
if day:
    ltime = datetime.date.fromisoformat(day[0])
    if len(day) == 1:
        rtime = ltime + oneday
    else:
        rtime = datetime.date.fromisoformat(day[-1]) + oneday

df = read_data(ltime, rtime)

if 'nolock' in args:
    df = df.loc[df.action != ' scrlock']

def aggrate_win(data):
    split_pat = re.compile('\s+[—-]\s+')
    label = df.win.apply(lambda n: tuple(split_pat.split(n.strip(' ')))[::-1])
    duration = df.groupby(label).duration.sum()
    agg = pd.DataFrame({'pct': duration / duration.sum(), 'dur':duration})
    return agg

agg = aggrate_win(df)

max_depth = 3
min_percent = 0.5

tree = {}
data = {}

for k, v in agg.iterrows():
    k = k[:max_depth]
    for i in range(len(k)):
        if k[:i] not in tree:
            tree[k[:i]] = []
        if k[i] not in tree[k[:i]]:
            tree[k[:i]].append(k[i])
    if k in data:
        data[k] += v.to_numpy()
    else:
        data[k] = v.to_numpy()

def sort_child(p=tuple()):
    if p not in tree:
        return
    for c in tree[p]:
        child = p + (c,)
        sort_child(child)
        if p not in data:
            data[p] = data[child]
        else:
            data[p] = data[p] + data[child]
    tree[p] = sorted(tree[p], key=lambda c: data[p + (c,)][0], reverse=False)
sort_child()


def strfdelta(tdelta):
    d = {"days": tdelta.days}
    d["hours"], rem = divmod(tdelta.seconds, 3600)
    d["minutes"], d["seconds"] = divmod(rem, 60)
    return '{hours}:{minutes}:{seconds}'.format(**d)
def strf(name, dur, d, p, n=15):
    sp = [name[i: i + n] for i in range(0, len(name), n)]
    dur = strfdelta(dur)
    L = int(d * p * 40)
    if L <= 1: return sp[0][:n-5] + ': ' + dur
    else: return '\n'.join(sp[:max(L - 1, 1)] + [dur])

def draw(p, acc, dep):
    pct, dur = data[p]
    if pct < min_percent / 100:
        return
    S = 2 * math.pi
    mid = (acc + pct / 2)
    plt.barh([dep], [S * pct], [1], left=S * acc)
    rot = mid * 360
    if rot > 90 and rot < 270:
        rot = rot - 180
    annot = strf(p[-1], dur, dep, pct)
    plt.annotate(annot, (S * mid, dep), ha='center', va='center', rotation=rot)

def visualize(p=tuple(), acc=0, dep=0):
    if dep > 0:
        draw(p, acc, dep)
    if p not in tree: return
    for c in tree[p]:
        child = p + (c,)
        visualize(child, acc, dep + 1)
        acc += data[child][0]

fig, ax = plt.subplots(subplot_kw=dict(projection="polar"), figsize=(10, 10))
visualize()
plt.annotate('Total\n' + strfdelta(data[()][1]), (0, 0), ha='center', va='center')
ax.set_axis_off()
plt.tight_layout()
plt.show()