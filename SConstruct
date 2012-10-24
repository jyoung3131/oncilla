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

mpi_path = '/usr/lib64/mvapich2/'
mpi_libpath = mpi_path + 'lib/'
mpi_include = mpi_path + 'include/'
mpi_libs = ['mpich']

gcc = '/usr/lib64/mvapich2/bin/mpicc'
gccfilter = './gccfilter -c '

if int(ARGUMENTS.get('filter', 0)): # prefix gccfilter to assist with compilation
    gcc = gccfilter + gcc

ccflags = ['-Wall', '-Wextra', '-Werror', '-Winline']
ccflags.extend(['-Wno-unused-parameter', '-Wno-unused-function'])

cpath = [os.getcwd() + '/inc', mpi_include]

libpath = [mpi_libpath]
libs = [mpi_libs]

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
