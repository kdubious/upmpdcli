from __future__ import print_function
import sys

APPVERSION="0.14"

# user-agent
try:
    import platform
    USER_AGENT = "%s/%s (%s %s; %s/%s (%s))" % (
            "Upmpdcli", APPVERSION, platform.system(), platform.machine(),
            platform.linux_distribution()[0], platform.linux_distribution()[1],
            platform.linux_distribution()[2])
except:
    USER_AGENT = "Upmpdcli/" + APPVERSION


class Logger:
    def mprint(self, m):
        #print("%s"%m, file=sys.stderr)
        pass
    def error(self, m):
        self.mprint(m)
    def warn(self, m):
        self.mprint(m)
    def info(self, m):
        self.mprint(m)
    def debug(self, m):
        self.mprint(m)
