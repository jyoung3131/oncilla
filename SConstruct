#! /usr/bin/env python

# Script info

""" Build script for the OncillaMem project.
"""

__author__ = "Alexander Merritt"
__email__ = "merritt.alex@gatech.edu"

# Python includes

import os
import commands
import sys

Help("""
      Type: 'scons ' to build the optimized version,
            'scons -c' to clean the build directory,
            'scons debug=1' to build the debug version,
            'scons timing=1' to enable timers for the optimized build,
            'scons extoll=1' or 'scons ib=1' to build EXTOLL or IB code exclusively.
      """)

# C configuration environment
libpath = []
libs = ['rt']
cpath = [os.getcwd() + '/inc']

gcc = 'clang'

ccflags = ['-Wall', '-Wextra', '-Werror', '-Winline']
ccflags.extend(['-Wno-unused-parameter', '-Wno-unused-function'])
libflags = []

#src/rdma.c:107 throws an aliasing error when compiled in the optimized case
#At some point we need to check on this...
#ccflags.extend(['-fno-strict-aliasing'])

if int(ARGUMENTS.get('timing', 0)): # add timing macro to allow for use of in-place timers
   ccflags.extend(['-DTIMING'])

if int(ARGUMENTS.get('debug', 0)): # set debug flags (no MPI debugging here)
   ccflags.extend(['-ggdb','-O0'])
   libflags.extend(['-ggdb','-O0'])
else:
   ccflags.extend(['-O3'])
   ccflags.extend(['-fno-strict-aliasing'])
   libs.append('mcheck')

#Detect whether the user wants to compile with IB, EXTOLL, or all networks
#available
if int(ARGUMENTS.get('ib', 0)): # specify IB or EXTOLL compilation path
   compilepath = 'ib'
   ccflags.extend(['-DINFINIBAND'])
elif int(ARGUMENTS.get('extoll', 0)):
   compilepath = 'extoll'
   ccflags.extend(['-DEXTOLL'])
else:
   compilepath = 'all'
   ccflags.extend(['-DINFINIBAND','-DEXTOLL'])

#Add IB libs if IB network is supported
if compilepath == 'extoll':
  ib_libs = []
else:
  ib_libs = ['rdmacm', 'ibverbs']
#Add RMA libs if EXTOLL network is supported
#librma2 is located at /extoll2/lib/librma2.so
if compilepath == 'ib':
  extoll_libs = []
else:
  extoll_libs = ['librma2']
  libpath.extend(['/extoll2/lib'])

#Add IB and EXTOLL libs (if defined)
libs.extend([ib_libs,extoll_libs])

env = Environment(CC = gcc, CCFLAGS = ccflags, CPPPATH = cpath)
env.Append(LIBPATH = libpath, LIBFLAGS = libflags, LIBS = libs)

# Gather sources

sources = []
files = os.listdir('src/')
for f in files:
    (name,ext) = os.path.splitext(f)
    if 'main' in name:
        continue
    if 'lib' in name:
        continue
    #Check so we don't compile extoll code on systems
    #with only IB drivers or vice versa
    if 'rdma' in name and compilepath == 'extoll':
        continue
    elif 'extoll' in name and compilepath == 'ib':
        continue
    elif '.c' == ext.lower():
        sources.append('src/' + f)

# Specify binaries

binary = env.Program('bin/oncillamem', ['src/main.c', sources])
libfiles = ['src/lib.c', 'src/pmsg.c', 'src/queue.c']
if compilepath != 'extoll':
  libfiles.append('src/rdma.c')
  libfiles.append('src/rdma_server.c')
  libfiles.append('src/rdma_client.c')

if compilepath != 'ib':
  libfiles.append('src/extoll.c')
  libfiles.append('src/extoll_server.c')
  libfiles.append('src/extoll_client.c')
solib = env.SharedLibrary('lib/libocm.so', libfiles)
SConscript(['test/SConscript'])
