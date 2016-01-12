#!/usr/bin/env python2
from __future__ import print_function

import sys
import logging

from StreamDecoder import StreamDecoder

class myCfg:
    def __init__(self):
        self.cf = dict()
        self.cf["url_timeout"] = "20"

    def getConfigValue(self, s):
        if s in self.cf:
            return self.cf[s]
        else:
            return None

    def setConfigValue(self, s, v):
        self.cf[s] = v



logger = logging.getLogger('radiotray')
logger.setLevel(logging.ERROR)
handler = logging.StreamHandler()
#handler = logging.NullHandler()
formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
handler.setFormatter(formatter)
logger.addHandler(handler)



decoder = StreamDecoder(myCfg())


urlInfo = decoder.getMediaStreamInfo(sys.argv[1])

while urlInfo is not None and urlInfo.isPlaylist():
    playlist = decoder.getPlaylist(urlInfo)
    if len(playlist) == 0:
        logger.error("Received empty stream from station", file=sys.stderr)
        sys.exit(1)
    stream = playlist.pop(0)
    logger.info('Stream %s' % stream)
    urlInfo = decoder.getMediaStreamInfo(stream)

if urlInfo is not None:
    logger.info("Result: isplaylist %d content-type %s url %s",
                urlInfo.isPlaylist(), urlInfo.getContentType(),
                urlInfo.getUrl())
    print("%s" % urlInfo.getUrl())
else:
    logger.error("Ended with null urlinfo")
    print

    
