#!/usr/bin/python

# $Id: 20121130$
# $Date: 2012-11-30 07:21:51$
# $Author: Marek Lukaszuk$

from time import time
from struct import pack,unpack
from hmac import HMAC
from hashlib import sha1
from base64 import b32decode
from os import getenv
from sys import exit

try:
  fd = open(getenv("HOME")+"/.gauth.conf")
except:
  print "Can't read ~/.gauth.conf file"
  exit(1)

tm = int(time() / 30)

lines = fd.read().split("\n")
for s in lines:
  try:
    s = s.split("|")
    secretkey = b32decode(s[1].replace(" ","").rstrip().upper())
  except:
    continue

  # convert timestamp to raw bytes
  b = pack(">q", tm)

  # generate HMAC-SHA1 from timestamp based on secret key
  hm = HMAC(secretkey, b, sha1).digest()

  # extract 4 bytes from digest based on LSB
  offset = ord(hm[-1]) & 0x0F
  truncatedHash = hm[offset:offset+4]

  # get the code from it
  code = unpack(">L", truncatedHash)[0]
  code &= 0x7FFFFFFF;
  code %= 1000000;

  print str(s[0])+": "+"0"*(6-len(str(code)))+str(code)
  s = fd.read()
