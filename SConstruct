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

mpi_path = '/usr/lib/openmpi/'
mpi_libpath = mpi_path + 'lib/'
mpi_include = mpi_path + 'include/'
mpi_libs = ['mpi']

evpath_path = '/opt/share/evpath/'
evpath_libpath = evpath_path + 'lib/'
evpath_include = evpath_path + 'include/'
evpath_libs = ['evpath', 'gen_thread', 'ffs', 'cod']
evpath_libs.extend(['atl', 'fm', 'cercs_env', 'dill', 'cmsockets'])

evpath_slibs = [] # create list containing paths to static evpath libs
for f in evpath_libs:
    staticlib = evpath_libpath + 'lib' + f + '.a'
    evpath_slibs.append(File(staticlib))

if int(ARGUMENTS.get('evstatic', 0)):
    evpath_libraries = evpath_slibs
else:
    evpath_libraries = evpath_libs

gcc = 'mpicc'
gccfilter = './gccfilter -c '

if int(ARGUMENTS.get('filter', 0)): # prefix gccfilter to assist with compilation
    gcc = gccfilter + gcc

ccflags = ['-Wall', '-Wextra', '-Werror', '-Winline']
ccflags.extend(['-Wno-unused-parameter', '-Wno-unused-function'])

cpath = [os.getcwd() + '/inc', mpi_include, evpath_include]

libpath = [mpi_libpath, evpath_libpath]
libs = [mpi_libs, evpath_libraries]

env = Environment(CC = gcc, CCFLAGS = ccflags, CPPPATH = cpath)
env.Append(LIBPATH = libpath, LIBS = libs)

# Gather sources

sources = []
files = os.listdir('src/')
for f in files:
    (name,ext) = os.path.splitext(f)
    if 'main' in name:
        continue
    if '.c' == ext.lower():
        sources.append('src/' + f)

# Specify binaries

binary = env.Program('bin/oncillamem', ['src/main.c', sources])
