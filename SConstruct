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

# C configuration environment

mpi_path = os.getenv('MPI_PATH')
print mpi_path
#mpi_path = '/usr/lib64/openmpi'
mpi_libpath = mpi_path + 'lib/'
#mpi_include = '/usr/include/openmpi-x86_64'
mpi_include = mpi_path + 'include'
mpi_libs = ['mpi']

ib_libs = ['rdmacm', 'ibverbs']

gcc = mpi_path + '/bin/mpicc'
gccfilter = './gccfilter -c '

ccflags = ['-Wall', '-Wextra', '-Werror', '-Winline']
ccflags.extend(['-Wno-unused-parameter', '-Wno-unused-function'])
#src/rdma.c:107 throws an aliasing error when compiled in the optimized case
#At some point we need to check on this...
#ccflags.extend(['-fno-strict-aliasing'])

if int(ARGUMENTS.get('timing', 0)): # add timing macro to allow for use of in-place timers
   ccflags.extend(['-DTIMING'])

if int(ARGUMENTS.get('dbg', 0)): # prefix gccfilter to assist with compilation
   ccflags.extend(['-ggdb','-O0'])
else:
   ccflags.extend(['-O3','-fno-strict-aliasing'])

if int(ARGUMENTS.get('filter', 0)): # prefix gccfilter to assist with compilation
   gcc = gccfilter + gcc

if int(ARGUMENTS.get('ib', 0)): # specify IB or EXTOLL compilation path
   compilepath = 'ib'
   ccflags.extend(['-DINFINIBAND'])
elif int(ARGUMENTS.get('extoll', 0)):
   compilepath = 'extoll'
   ccflags.extend(['-DEXTOLL'])
else:
   compilepath = 'all'
   ccflags.extend(['-DINFINIBAND','-DEXTOLL'])

cpath = [os.getcwd() + '/inc', mpi_include]

libpath = [mpi_libpath]
libs = [mpi_libs, ib_libs, 'rt']

env = Environment(CC = gcc, CCFLAGS = ccflags, CPPPATH = cpath)
env.Append(LIBPATH = libpath, LIBS = libs)

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
libfiles = ['src/lib.c', 'src/pmsg.c', 'src/queue.c', 'src/rdma.c']
libfiles.append('src/rdma_server.c')
libfiles.append('src/rdma_client.c')
solib = env.SharedLibrary('lib/libocm.so', libfiles)
SConscript(['test/SConscript'])
