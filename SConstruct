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

gcc = 'clang'
envcompilepath = ''
#Disable GPU support by default
cuda_flag = 0
cuda_libs = ['']

#Use this function to check and see if a particular application is installed
def run(cmd, env):
  """Run a Unix command and return the exit code."""
  res = os.system(cmd)
  if (os.WIFEXITED(res)):
    code = os.WEXITSTATUS(res)
    return code
  # Assumes that if a process doesn't call exit, it was successful
  return 0


env = Environment()
conf = Configure(env)
if not env.GetOption('clean'):
  print 'Testing to see if InfiniBand is installed'
  if run('/usr/sbin/ibstat', env):
    print 'IB not found\n'
    envcompilepath = 'extoll'

  #Search for the EXTOLL installation
  if run('find /extoll2/include -maxdepth 1 -name \'extolldrv.h\'', env):
    print 'EXTOLL install not found\n'
    envcompilepath = 'ib'

  if not (envcompilepath == 'extoll') and not (envcompilepath == 'ib'):
    print 'EXTOLL and IB install are available'
    envcompilepath = 'All'
  else:
    print '%s path(s) selected\n\n' % envcompilepath

print 'Testing to see if clang is installed'
if run('which clang', env):
  print 'clang not found - using gcc\n\n'
  gcc = 'gcc'

print 'Testing to see if CUDA is installed'
if not run('nvcc --version', env):
  print 'CUDA found\n'
  cuda_flag = 1
env = conf.Finish()

# C configuration environment
libpath = []
libs = ['rt']
cpath = [os.getcwd() + '/inc']

ccflags = ['-Wall', '-Wextra', '-Werror', '-Winline']
ccflags.extend(['-Wno-unused-parameter', '-Wno-unused-function'])
libflags = []

#src/rdma.c:107 throws an aliasing error when compiled in the optimized case
#At some point we need to check on this...
#ccflags.extend(['-fno-strict-aliasing'])

#Always compile with the timing flag for this branch
ccflags.extend(['-DTIMING'])

if int(ARGUMENTS.get('debug', 0)): # set debug flags (no MPI debugging here)
   ccflags.extend(['-ggdb','-O0'])
   libflags.extend(['-ggdb','-O0'])
   #Be careful using memcheck as it seems like it may affect some buffer used in the IB CM process.
   #This error shows up as the IB client trying to finish a connection and failing.
   #libs.append('mcheck')
else:
   ccflags.extend(['-O2'])
   ccflags.extend(['-fno-strict-aliasing'])

#Specify if GPU support is available
if cuda_flag == 1:
  ccflags.extend(['-DCUDA'])
  libpath.extend(['/usr/local/cuda/lib64'])
  cpath.extend(['/usr/local/cuda/include'])
  cuda_libs.extend(['cuda','cudart'])

#Detect whether the user wants to compile with IB, EXTOLL, or all networks
#available
#Arguments allow the user to override detected compile options or default compilation
#will compile the EXTOLL and IB code if it is supported
if int(ARGUMENTS.get('ib', 0)):
   compilepath = 'ib'
   ccflags.extend(['-DINFINIBAND'])
elif int(ARGUMENTS.get('extoll', 0)):
   compilepath = 'extoll'
   cpath.extend(['/extoll2/include'])
   ccflags.extend(['-DEXTOLL'])
else:
   if envcompilepath == 'ib':
      compilepath = 'ib'
      ccflags.extend(['-DINFINIBAND'])
   elif envcompilepath == 'extoll':
      compilepath = 'extoll'
      cpath.extend(['/extoll2/include'])
      ccflags.extend(['-DEXTOLL'])
   else:
      compilepath = 'all'
      ccflags.extend(['-DINFINIBAND','-DEXTOLL'])
      cpath.extend(['/extoll2/include'])

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
libs.extend([ib_libs,extoll_libs,cuda_libs])

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

#Export variables set in this file so they can be imported into the SConscript
exp_env = Environment()
Export('env','gcc','compilepath','libpath','libs')
#Then call SConscript 
SConscript(['test/SConscript'])
