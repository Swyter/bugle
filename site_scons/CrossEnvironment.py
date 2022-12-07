#!/usr/bin/env python
from SCons.Environment import Environment
import SCons.Util

class CrossEnvironment(Environment):
    '''
    Environment that supports cross-compilation with the GCC toolchain.
    If the construction variable HOST is set, it is used as a prefix
    to search for toolchain binaries.

    This environment also overrides the SharedLibrary method to do a
    better job with regards to sonames, version numbering and symbol
    visibility.
    '''

    _cross_tools = ['cc', 'gcc', 'g++', 'as', 'gas', 'ranlib', 'ar']

    def Detect(self, progs):
        """Return the first available program in progs.

        In this version, we first try to find a cross-tool, then fall back
        to looking for a native tool
        """

        if 'HOST' in self:
            if not SCons.Util.is_List(progs):
                progs = [ progs ]
            if progs[0] in self._cross_tools:
                for prog in progs:
                    cross_prog = self['HOST'] + '-' + prog
                    path = self.WhereIs(cross_prog)
                    if path: return cross_prog
                # If we get here, we failed to find a cross-tool above
                print('WARNING: did not find %s-%s, falling back' % (self['HOST'], progs[0]))

        return Environment.Detect(self, progs)
