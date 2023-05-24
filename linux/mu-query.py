#!/usr/bin/env python3
import functools
import subprocess
import os
import sys
import shutil
import time
import math
import base64
import email.utils
import mimetypes
import uuid
import email.header

print = functools.partial(print, flush=True)

MAIL_TEMPLATE="""To: 
From: 
"""
FUSE_MODE = False
MAIL_EXTRACTDIR = "/tmp/mu-extract/"
MAIL_SYNC = "mbsync -a"
MAIL_EXTRACT = "mu extract --save-all --nocolor"
MAIL_WIDTH = 64
MAIL_OPEN = "xdg-open"
MAIL_INDEX = "mu index -q"
MAIL_PAGE = 100
MAIL_START = 1
MAIL_FIND = "mu find -s d --reverse --nocolor -f \1l\1s\1d\1f\1t -t"
MAIL_VIEW = "mu view --nocolor"
MAIL_QUERY = ""
MAIL_SEND="msmtp --read-envelope-from"
MAIL_COMPL="mu cfind --nocolor --format=mutt-ab"

PROGNAME = sys.argv[0]
MODE = 'main'
MSG = False
QUERY = ""

def parse_args(args):
    skip = False
    global MODE
    global MSG
    global QUERY
    for i in range(1, len(args)):
        if skip:
            skip = False
            continue
        if args[i] == '--message':
            MODE = 'message'
            MSG = args[i+1]
            skip = True
        elif args[i] == '--compose':
            MODE = 'compose'
        elif args[i] == '--reply':
            MODE = 'reply'
            MSG = args[i+1]
            skip = True
        elif args[i] == '--forward':
            MODE = 'forward'
            MSG = args[i+1]
            skip = True
        else:
            if QUERY != "":
                QUERY += " "
            QUERY += args[i]


def email_get_flags(fname):
    idx = fname.rfind(":2,")
    if idx == -1:
        return ''
    return fname[idx+3:]

def email_del_flags(fname):
    idx = fname.rfind(":2,")
    if idx == -1:
        return fname
    return fname[0:idx]

def email_set_flags(fname, flags):
    fname = email_del_flags(fname)
    return fname + ":2," + flags

def email_mark_read(fname):
    if not fname.find("/new/"):
        return fname
    fname2 = email_del_flags(fname.replace("/new/", "/cur/", 1))
    flags = email_get_flags(fname)
    if 'S' not in flags:
        flags += 'S'
    fname2 = email_set_flags(fname2, flags)
    os.rename(fname, fname2)
    return fname2

def email_mark_reply(fname):
    if not fname.find("/cur/"):
        return fname
    flags = email_get_flags(fname)
    if flags.find('R'):
        return fname
    flags = 'R' + flags
    fname2 = email_set_flags(email_del_flags(fname2), flags)
    os.rename(fname, fname2)
    return fname2

def email_sent(body):
    mdir = os.getenv('MAILDIR')
    if not mdir or not os.path.exists(mdir + "/sent/cur"):
        return False
    fname = "%s/sent/cur/%d.%s.%s:2,S" %(mdir, time.time(), uuid.uuid1().hex[0:16], os.uname()[1])
    with open(fname, "w") as f:
        for l in body:
            f.write(l+"\n")
    print("Written: %s" % fname)
    return True

def email_process(lines):
    body = []
    headers = []
    attach = []
    hdr = True
    for l in lines:
        if not hdr:
            body.append(l)
        elif l == "":
            hdr = False
        elif l.startswith("Attach:"):
            f = l[7:].strip()
            if not os.path.isfile(f):
                print("Wrong attach: " % f)
                return None
            attach.append(f)
        elif not l.startswith("Content-Type:"):
            headers.append(l)
    headers.append('Date: '+email.utils.formatdate(localtime=True))
    if not len(attach):
        return headers + ["Content-Type: text/plain; charset=UTF-8"] + [""] + body
    bound = "--============--"
    ret = headers
    ret.append('Content-Type: multipart/mixed; boundary="' + bound + '"')
    ret.append("")
    ret.append("This is a MIME formatted message. "
            "If you see this text it means that your "
            "email software does not support MIME formatted messages.")
    ret.append("")
    ret.append("--" + bound)
    ret.append("Content-Type: text/plain; charset=UTF-8")
    ret.append("Content-Disposition: inline")
    ret.append("")
    ret += body
    ret.append("--" + bound)
    for a in attach:
        ret.append("Content-Type: %s; name=\"%s\""%
            (mimetypes.MimeTypes().guess_type(a)[0], os.path.basename(a)))
        ret.append("Content-Transfer-Encoding: base64")
        with open(a, "rb") as f:
            blob = base64.b64encode(f.read())
            ret.append("Content-Disposition: attachment; filename=\"%s\";"%
                os.path.basename(a))
            ret.append("")
            ret += blob.decode().splitlines()
            ret.append("--" + bound)
    return ret

def email_headers(fname):
    tags = { "From", "To", "Message-ID", "References", "Cc", "Subject" }
    ret = {
        "From": "",
        "To": "",
        "Subject": ""
    }
    with open(fname, "r") as f:
        hdr = []
        for l in f.read().splitlines():
            if not l:
                break
            if l.startswith(" ") or l.startswith("\t"):
                hdr[-1] += " " + l.strip()
            else:
                hdr.append(l)
        for l in hdr:
            if not ":" in l:
                continue
            a = l.split(":", 1)
            if a[0] in tags:
                hh = email.header.decode_header(a[1].strip())
                txt = []
                for h in hh:
                    txt.append(h[0].decode(h[1] or "utf-8") if not isinstance(h[0], str) else h[0])
                ret[a[0]] = "".join(txt)
    return ret

def exec_grab(prog):
    p = subprocess.run(prog, capture_output = True)
    return p.stdout.decode().replace("\r", "")

class Acme:
    def __init__(self):
        self.mounts = 0
        self.ns = False
        self.fuse = FUSE_MODE
    def mount(self):
        if not self.fuse:
            return
        self.mounts += 1
        if self.ns:
            return self.ns
        ns = exec_grab(['namespace']).strip()
        if not os.path.exists(ns + "/acme9p/index"):
            os.path.exists(ns + "/acme9p") or os.mkdir(ns + "/acme9p")
            os.system("9pfuse %s %s" % (ns + "/acme", ns + "/acme9p"))
        self.ns = ns + '/acme9p'
        return self.ns

    def umount(self):
        if not self.fuse:
            return
        if self.ns:
            self.mounts -= 1
            if self.mounts == 0:
                os.system("fusermount -u %s" % self.ns)
                self.ns = False
    def __del__(self):
        self.umount()

    def new(self):
        id = self.read('new', 'ctl')[0].split()[0]
        return Window(self, id)

    def read(self, id, file):
        if self.ns:
            with open('%s/%s/%s' % (self.ns, id, file), "r") as f:
                return f.read().splitlines()
        return exec_grab(['9p', 'read', 'acme/%s/%s'%(id, file)]).splitlines()

    def write(self, id, file, s):
        if type(s) == str:
            s = [ s ]
        if self.ns:
            with open('%s/%s/%s' % (self.ns, id, file), "a") as f:
                for l in s:
                    f.write(l)
            return
        p = subprocess.Popen(['9p', 'write', 'acme/%s/%s'%(id, file)],
            stdin=subprocess.PIPE)
        for l in s:
            p.stdin.write(l.encode())
        p.stdin.close()
        p.wait()

class Window:
    def __init__(self, acme, id):
        self.acme = acme
        self.id = id
        self.pipe = None
    def clean(self):
        self.write('ctl', 'clean')
    def tag(self, t):
        self.write('ctl', 'cleartag')
        self.write('tag', t)
    def read(self, f):
        return self.acme.read(self.id, f)
    def write(self, f, t):
        return self.acme.write(self.id, f, t)
    def text(self, t):
        self.write('addr', '0,$')
        self.data(t)
        self.toline(0)
        self.clean()
    def data(self, t):
        self.acme.mount()
        self.write('data', t)
        self.acme.umount()
    def toline(self, nr):
        self.write('addr', str(nr))
        self.write('ctl', 'dot=addr')
        self.write('ctl', 'show')
    def event(self):
        if not self.pipe:
            self.pipe = subprocess.Popen(['9p', 'read', 'acme/%s/event' % self.id],
                stdout = subprocess.PIPE,
                stderr = subprocess.DEVNULL).stdout
        r = self.pipe.readline().decode()
        if r == "":
            return None
        return r.split()
    def defevent(self, r):
        if len(r) > 2 and r[2].isdigit() and (int(r[2]) & 2):
            self.acme.write(self.id, 'event', '%s %s\n'%(r[0], r[1]))
    def __del__(self):
        if self.pipe:
            self.pipe.close()

    def run(self, fn):
        while ((ev := self.event()) is not None):
            if len(ev) == 0 or len(ev) <= 4:
                self.defevent(ev)
                continue
            if not fn(*(ev + (6 - len(ev)) * [None])):
                self.defevent(ev)
class BaseWindow:
    def __init__(self):
        self.acme = Acme()
        self.win = self.acme.new()

    def event(self, ev, arg, opt):
        if ev.startswith('Mx'):
            fn = getattr(self, 'ev' + arg, None)
            return fn(opt) if fn else False
        return False

    def run(self):
        self.win.run(lambda *a: self.event(a[0], a[4], a[5]))

class Main(BaseWindow):
    def __init__(self, query = ""):
        self.page = 1
        self.query = query
        super().__init__()
        self.request()
        self.show()

    def email_title(self, nr, m):
        fmt = "%2d/ %+4s %s%s" % (nr + (self.page - 1) * MAIL_PAGE,
            m["flags"], m["pfx"], m["subj"])
        f =  m["from"]
        if '<' in f:
            f = f.split('<', 1)[0].strip()
        fmt = (fmt[:MAIL_WIDTH-3] + '...') if len(fmt) >= MAIL_WIDTH else fmt
        return "%-64s %s|%s" % (fmt, m["date"][:14], f)

    def show(self):
        self.win.acme.mount()
        self.win.tag(' Mark Remove Compose Read Sync Next | Page %d' % self.page)
        self.win.write('addr', '0,$')
        if len(self.msg) == 0:
            self.win.data("No data\n")
        idx = 0
        for m in self.msg:
            idx += 1
            self.win.data(self.email_title(idx, m)+'\n')
        self.win.toline(0)
        self.win.clean()
        self.win.acme.umount()

    def request(self):
        os.system(MAIL_INDEX)

        req = MAIL_FIND.split() + [self.query]

        p = subprocess.Popen(req, stdout = subprocess.PIPE)
        msg = []
        start = (self.page - 1) * MAIL_PAGE
        idx = 0
        while (r := p.stdout.readline()):
            if idx >= start:
                m = r.decode().strip().split("\1")
                email = {
                    "pfx": m[0],
                    "path": m[1],
                    "flags": email_get_flags(m[1]),
                    "subj": m[2],
                    "date": m[3],
                    "from": m[4]
                }
                msg.append(email)
            idx += 1
            if idx >= start + MAIL_PAGE:
                break
        p.kill()
        self.msg = msg

    def mark_read(self, nr):
        fname = email_mark_read(self.msg[nr]["path"])
        if fname == self.msg[nr]["path"]:
            return
        self.msg[nr]["path"] = fname
        self.msg[nr]["flags"] = email_get_flags(fname)
        self.win.write('addr', "%d,%d"%(nr+1, nr+1))
        self.win.data(self.email_title(nr + 1, self.msg[nr])+"\n")

    def email_numbers(self):
        r = []
        for l in self.win.read('rdsel'):
            if (nr := self.check_email_number(l.split()[0])) is not None:
                r.append(nr)
        return r

    def check_email_number(self, t):
        t = t.strip()
        if t.endswith("/"):
            t = t[:-1]
        if not t.isdigit():
            return False
        t = int(t) - (self.page - 1) * MAIL_PAGE - 1
        if t < 0 or t >= len(self.msg):
            return None
        return t

    def evSync(self, opt):
        print ("Sync start")
        os.system(MAIL_SYNC)
        self.show()
        return True

    def evRemove(self, opt):
        for nr in self.email_numbers():
            os.remove(self.msg[nr]["path"])
            self.msg[nr]["deleted"] = True
        self.msg = [m for m in self.msg if not "deleted" in m]
        self.show()
        return True

    def evMark(self, opt):
        for nr in self.email_numbers():
            self.mark_read(nr)
        return True

    def evNext(self, opt):
        nr = 0
        for m in self.msg:
            if 'S' not in m["flags"]:
                self.mark_read(nr)
                self.win.toline(nr+1)
                os.system("%s --message '%s' &" % (PROGNAME, self.msg[nr]["path"]))
                return True
            nr += 1
        return True
    def evPage(self, opt):
        if opt is None:
            page = self.page + 1
        else:
            page = int(opt) if opt.isdigit() else 1
            page = max(page, 1)
        if page != self.page:
            self.page = page
            self.request()
            self.show()
            return True

    def evRead(self, opt):
        self.request()
        self.show()
        return True

    def evCompose(self, opt):
        os.system("%s --compose &" % PROGNAME)
        return True

    def event(self, ev, arg, opt):
        if ev.startswith('Mx'):
            fn = getattr(self, 'ev' + arg, None)
            return fn(opt) if fn else False
        elif ev.startswith('ML') and (nr := self.check_email_number(arg)) is not None:
            self.mark_read(nr)
            self.win.toline(nr+1)
            os.system("%s --message '%s' &" % (PROGNAME, self.msg[nr]["path"]))
            return True
        return False

class Message(BaseWindow):
    def __init__(self, msg):
        super().__init__()
        self.win.tag(' Reply Forward Raw Extract')
        self.msg = msg
        self.show()
        self.raw = False

    def show(self):
        t = exec_grab(MAIL_VIEW.split() + [self.msg]).splitlines(keepends=True)
        self.win.text(t)

    def evExtract(self, opt):
        self.win.toline("$,$")
        os.path.exists(MAIL_EXTRACTDIR) and shutil.rmtree(MAIL_EXTRACTDIR)
        os.mkdir(MAIL_EXTRACTDIR)
        os.system(MAIL_EXTRACT +
            " --target-dir='%s' '%s'" % (MAIL_EXTRACTDIR, self.msg))
        self.acme.mount()
        self.win.data("\n\n*** Extracted to: %s\n" % MAIL_EXTRACTDIR)
        for (path, dirs, files) in os.walk(MAIL_EXTRACTDIR):
            for d in dirs:
              self.win.data(path + d + "/\n")
            for d in files:
              self.win.data(MAIL_OPEN + " " + path + d + "\n")
        self.acme.umount()
        return True

    def evForward(self, opt):
        os.system("%s --forward '%s' &" % (PROGNAME, self.msg))
        return True

    def evReply(self, opt):
        os.system("%s --reply '%s' &" % (PROGNAME, self.msg))
        return True

    def evRaw(self, opt):
        self.raw = not self.raw
        if not self.raw:
            return self.show()
        with open(self.msg, "r") as f:
            self.win.text(f.read().splitlines(keepends=True))
        return True

class Compose(BaseWindow):
    def __init__(self, msg=None, forward=False):
        super().__init__()
        self.msg = msg
        self.forward = forward
        self.show()
    def show(self):
        self.acme.mount()
        self.fname = '/tmp/message-' + str(math.floor(time.time())) + '.txt'
        self.win.write('ctl', 'name ' + self.fname + '\n')
        self.win.tag(' Put Post |fmt Addr Paste Undo')
        self.win.write('addr', '0,$')
        if self.forward or not self.msg:
            self.win.data(MAIL_TEMPLATE)
        if self.msg:
            t = exec_grab(MAIL_VIEW.split() + [self.msg]).splitlines(keepends=True)
            t = t[4:]
            hdr = email_headers(self.msg)
            if not self.forward:
                To = hdr["From"] if "," not in hdr["From"] else hdr["To"]
                From = hdr["To"] if "," not in hdr["To"] else ""
                subj = hdr["Subject"]
                if not subj.startswith("Re:"):
                    subj = "Re: " + subj
                self.win.data("To: " + To + "\n"
                    "From: " + From + "\n"
                    "Subject: " + subj + "\n")
                if "Cc" in hdr:
                    self.win.data("Cc: " + hdr["Cc"] + "\n")
                if "Message-ID" in hdr:
                    self.win.data("In-Reply-To: " + hdr["Message-ID"] + "\n")
            else:
                self.win.data("Subject: Fwd: " + hdr["Subject"] + "\n\n"
                    "================ Forwarded message ================\n"
                    "To: " + hdr["To"] + "\n"
                    "From: " + hdr["From"] + "\n"
                    "Subject: " + hdr["Subject"] + "\n")

            self.win.data("\n")
            if self.forward:
                self.win.data(t)
            else:
                for l in t:
                    self.win.data(">"+ ("" if l.startswith(">") else " ") + l)
        else:
            self.win.data("Subject: \n\n")
        self.win.toline(0)
        self.acme.umount()
    def evAddr(self, opt):
        addr = self.win.read('rdsel')
        if not addr:
            return True
        compl = exec_grab(MAIL_COMPL.split() + [addr[0]]).splitlines()
        first = False
        for v in compl:
            a = v.replace("\t", " ").split(" ", 1)
            if '@' not in a[0]:
                continue
            l = "%s <%s>" % (a[1], a[0])
            if not first:
                first = True
                self.win.write('wrsel', l)
            else:
                print(l)

    def evPost(self, opt):
        self.win.clean()
        to = ""
        self.win.write('addr', '0,$')
        lines = self.win.read('data')
        for l in lines:
            if l.startswith("To:"):
                to = l[3:].strip()
                break

        if not to or "@" not in to:
            print("Wrong To: %s" % to)
            return True

        lines = email_process(lines)

        if lines is None:
            print("Can't process message")
            return True

        p = subprocess.Popen(MAIL_SEND.split() + [to],
            stdin=subprocess.PIPE)

        for l in lines:
            p.stdin.write((l + '\n').encode())
        p.stdin.close()

        if p.wait() != 0:
            print("Error while sending")
            return True
        email_sent(lines)
        self.win.write('ctl', 'del')
        return True

parse_args(sys.argv)

if MODE == 'main':
    Main(QUERY).run()
elif MODE == 'message':
    Message(MSG).run()
elif MODE == 'compose':
    Compose().run()
elif MODE == 'reply':
    Compose(MSG).run()
elif MODE == 'forward':
    Compose(MSG, forward=True).run()
