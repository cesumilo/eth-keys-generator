#!/usr/bin/env python3
# -*- coding: utf-8 -*-
'''
argparser – command line argument parser library

Copyright © 2013  Mattias Andrée (maandree@member.fsf.org)

This library is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this library.  If not, see <http://www.gnu.org/licenses/>.
'''
import sys
import os


class ArgParser():
    '''
    Simple argument parser
    
    @author  Mattias Andrée, maandree@member.fsf.org
    '''
    
    
    ARGUMENTLESS = 0
    '''
    :int  Option takes no arguments
    '''
    
    ARGUMENTED = 1
    '''
    :int  Option takes one argument per instance
    '''
    
    OPTARGUMENTED = 2
    '''
    :int  Option takes optionally one argument per instance
    '''
    
    VARIADIC = 3
    '''
    :int  Option consumes all following arguments
    '''
    
    
    def __init__(self, description, usage, longdescription = None, program = None, usestderr = False, abbreviations = None):
        '''
        Constructor.
        The short description is printed on same line as the program name
        
        @param  description:str                                               Short, single-line, description of the program
        @param  usage:str?                                                    Formated, multi-line, usage text, may be `None`
        @param  longdescription:str?                                          Long, multi-line, description of the program, may be `None`
        @param  program:str?                                                  The name of the program, `None` for automatic
        @param  usestderr:bool                                                Whether to use stderr instead of stdout
        @param  (opt:str, opts:list<str>, mapping:dict<str, str>?=None)→str?  Function that expands abbrevatied options
        '''
        self.linuxvt = ('TERM' in os.environ) and (os.environ['TERM'] == 'linux')
        self.program = sys.argv[0] if program is None else program
        self.__description = description
        self.__usage = usage
        self.__longdescription = longdescription
        self.__options = []
        self.opts = {}
        self.optmap = {}
        self.__out = sys.stderr.buffer if usestderr else sys.stdout.buffer
        self.abbreviations = abbreviations if abbreviations is not None else lambda arg, candidates, mapping = None : None
        def get_mapping(map_):
            rc = {}
            for key in map_.keys():
                rc[key] = map_[key][0]
            return rc
        self.__abbreviations = lambda arg : self.abbreviations(arg, list(self.optmap.keys()), get_mapping(self.optmap))
    
    
    @staticmethod
    def standard_abbreviations():
        '''
        Gets the standard abbrevation expender
        
        @return  :(opt:str, opts:list<str>, mapping:dict<str, str>)→str?  The standard abbrevation expender
        '''
        def uniq(items):
            if len(items) < 2:
                return items
            items = sorted(items)
            rc = items[:1]
            for elem in items[1:]:
                if not elem == rc[-1]:
                    rc.append(elem)
            return rc
        one = lambda arg : arg[0] if len(arg) == 1 else None
        lfilter = lambda f, l : list(filter(f, l))
        map_ = lambda mapping, args : [mapping[a] for a in args]
        test = lambda arg : lambda a : a.startswith(arg)
        return lambda arg, candidates, mapping : one(uniq(map_(mapping, lfilter(test(arg), candidates))))
    
    
    @staticmethod
    def parent_name(levels = 1, hasinterpretor = False):
        '''
        Gets the name of the parent process
        
        @param   levels:int           The number of parents to walk, 0 for self, and 1 for direct parent
        @param   hasinterpretor:bool  Whether the parent process is an interpretor
        @return  :str?                The name of the parent process, `None` if not found
        '''
        pid = os.readlink('/proc/self')
        lvl = levels
        while lvl > 0:
            with open('/proc/%s/status' % pid, 'r') as file:
                lines = file.readlines()
                found = False
                for line in lines:
                    if line.startswith('PPid:'):
                        line = line[5:].replace('\t', '').replace(' ', '').replace('\n', '')
                        pid = int(line)
                        lvl -= 1
                        found = True
                        break
                if not found:
                    return None
        data = []
        with open('/proc/%d/cmdline' % pid, 'rb') as file:
            while True:
                read = file.read(4096)
                if len(read) == 0:
                    break
                data += list(read)
        cmdline = bytes(data[:-1]).decode('utf-8', 'replace').split('\0')
        if not hasinterpretor:
            rc = cmdline[0]
            return None if len(rc) == 0 else rc
        dashed = False
        (i, n) = (1, len(cmdline))
        while i < n:
            if dashed:
                return cmdline[i]
            if cmdline[i] == '--':
                dashed = Τrue
            elif cmdline[i] in ('-c', '-m', '-W'):
                i += 1
            elif not cmdline[i].startwith('-'):
                return cmdline[i]
            i += 1
        return None
    
    
    def __print(self, text = '', end = '\n'):
        '''
        Hack to enforce UTF-8 in output (in the future, if you see anypony not using utf-8 in
        programs by default, report them to Princess Celestia so she can banish them to the moon)
        
        @param  text:__str__()→str  The text to print (empty string is default)
        @param  end:str             The appendix to the text to print (line breaking is default)
        '''
        self.__out.write((str(text) + end).encode('utf-8'))
    
    
    def add_argumentless(self, alternatives, default = 0, help = None, trigger = None):
        '''
        Add option that takes no arguments
        
        @param  alternatives:list<str>    Option names
        @param  default:str|int           The default argument's name or index
        @param  help:str?                 Short description, use `None` to hide the option
        @param  trigger:(str, str)?→void  Function to invoke when the option is used, with the used alternative and the standard alternative
        '''
        stdalt = alternatives[default] if isinstance(default, int) else default
        self.__options.append((ArgParser.ARGUMENTLESS, alternatives, None, help, stdalt))
        self.opts[stdalt] = None
        for alt in alternatives:
            self.optmap[alt] = (stdalt, ArgParser.ARGUMENTLESS, trigger if trigger is not None else lambda used, std : True)
    
    
    def add_argumented(self, alternatives, default = 0, arg = 'ARG', help = None, trigger = None):
        '''
        Add option that takes one argument
        
        @param  alternatives:list<str>          Option names
        @param  default:str|int                 The default argument's name or index
        @param  arg:str                         The name of the takes argument, one word
        @param  help:str?                       Short description, use `None` to hide the option
        @param  trigger:(str, str, str?)?→void  Function to invoke when the option is used, with the used alternative, the standard alternative and the used argument and `None` if there are not more arguments
        '''
        stdalt = alternatives[default] if isinstance(default, int) else default
        self.__options.append((ArgParser.ARGUMENTED, alternatives, arg, help, stdalt))
        self.opts[stdalt] = None
        for alt in alternatives:
            self.optmap[alt] = (stdalt, ArgParser.ARGUMENTED, trigger if trigger is not None else lambda used, std, value : True)
    
    
    def add_optargumented(self, alternatives, default = 0, arg = 'ARG', help = None, trigger = None, stickless = None):
        '''
        Add option that optionally takes one argument
        
        @param  alternatives:list<str>          Option names
        @param  default:str|int                 The default argument's name or index
        @param  arg:str                         The name of the takes argument, one word
        @param  help:str?                       Short description, use `None` to hide the option
        @param  trigger:(str, str, str?)?→void  Function to invoke when the option is used, with the used alternative, the standard alternative and the used argument if any
        @parma  stickless:(str)?→bool           Function that should return true if the (feed) next argument can used for the argument without being sticky
        '''
        stdalt = alternatives[default] if isinstance(default, int) else default
        self.__options.append((ArgParser.OPTARGUMENTED, alternatives, arg, help, stdalt))
        self.opts[stdalt] = None
        for alt in alternatives:
            trigger = trigger if trigger is not None else lambda used, std, value : True
            stickless = stickless if stickless is not None else lambda arg : not (arg.startswith('-') or arg.startswith('+'))
            self.optmap[alt] = (stdalt, ArgParser.OPTARGUMENTED, trigger, stickless)
    
    
    def add_variadic(self, alternatives, default = 0, arg = 'ARG', help = None, trigger = None):
        '''
        Add option that takes all following argument
        
        @param  alternatives:list<str>    Option names
        @param  default:str|int           The default argument's name or index
        @param  arg:str                   The name of the takes arguments, one word
        @param  help:str?                 Short description, use `None` to hide the option
        @param  trigger:(str, str)?→void  Function to invoke when the option is used, with the used alternative and the standard alternative
        '''
        stdalt = alternatives[default] if isinstance(default, int) else default
        self.__options.append((ArgParser.VARIADIC, alternatives, arg, help, stdalt))
        self.opts[stdalt] = None
        for alt in alternatives:
            self.optmap[alt] = (stdalt, ArgParser.VARIADIC, trigger if trigger is not None else lambda used, std : True)
    
    
    def parse(self, argv = sys.argv, alternative = False):
        '''
        Parse arguments
        
        @param   args:list<str>    The command line arguments, should include the execute file at index 0, `sys.argv` is default
        @param   alternative:bool  Use single dash/plus for long options
        @return  :bool             Whether no unrecognised option is used
        '''
        self.argcount = len(argv) - 1
        self.arguments = argv[1:]
        self.files = []
        
        argqueue = []
        optqueue = []
        queue = []
        for arg in argv[1:]:
            queue.append(arg)
        
        dashed = False
        tmpdashed = False
        get = 0
        dontget = 0
        self.rc = True
        
        self.unrecognisedCount = 0
        def unrecognised(arg):
            self.unrecognisedCount += 1
            if self.unrecognisedCount <= 5:
                self.__print('%s: warning: unrecognised option %s' % (self.program, arg))
                self.__out.flush()
            self.rc = False
        
        while len(queue) != 0:
            arg = queue[0]
            queue = queue[1:]
            if (get > 0) and (dontget == 0):
                arg_opt = optqueue[-get]
                option = self.optmap[arg_opt]
                passed = True
                get -= 1
                if option[1] == ArgParser.OPTARGUMENTED:
                    if not option[3](arg):
                        passed = False
                        option[2](arg_opt, option[0], None)
                        argqueue.append(None)
                if passed:
                    option[2](arg_opt, option[0], arg)
                    argqueue.append(arg)
                    continue
            if tmpdashed:
                self.files.append(arg)
                tmpdashed = False
            elif dashed:        self.files.append(arg)
            elif arg == '++':   tmpdashed = True
            elif arg == '--':   dashed = True
            elif (len(arg) > 1) and (arg[0] in ('-', '+')):
                if alternative or ((len(arg) > 2) and (arg[:2] in ('--', '++'))):
                    if dontget > 0:
                        dontget -= 1
                    elif (arg in self.optmap) and (self.optmap[arg][1] == ArgParser.ARGUMENTLESS):
                        self.optmap[arg][2](arg, self.optmap[arg][0])
                        optqueue.append(arg)
                        argqueue.append(None)
                    elif '=' in arg:
                        arg_opt = arg[:arg.index('=')]
                        value = arg[arg.index('=') + 1:]
                        if (arg_opt in self.optmap) and (self.optmap[arg_opt][1] >= ArgParser.ARGUMENTED):
                            optqueue.append(arg_opt)
                            argqueue.append(value)
                            if self.optmap[arg_opt][1] == ArgParser.VARIADIC:
                                dashed = True
                                self.optmap[arg_opt][2](arg_opt, self.optmap[arg_opt][0])
                            else:
                                self.optmap[arg_opt][2](arg_opt, self.optmap[arg_opt][0], value)
                        else:
                            _arg = self.__abbreviations(arg_opt)
                            if _arg is None:
                                unrecognised(arg_opt)
                            else:
                                queue[0:0] = ['%s=%s' % (_arg, value)]
                    elif (arg in self.optmap) and (self.optmap[arg][1] <= ArgParser.OPTARGUMENTED):
                        optqueue.append(arg)
                        get += 1
                    elif (arg in self.optmap) and (self.optmap[arg][1] == ArgParser.VARIADIC):
                        self.optmap[arg][2](arg, self.optmap[arg][0])
                        optqueue.append(arg)
                        argqueue.append(None)
                        dashed = True
                    else:
                        _arg = self.__abbreviations(arg)
                        if _arg is None:
                            unrecognised(arg)
                        else:
                            queue[0:0] = [_arg]
                else:
                    sign = arg[0]
                    i = 1
                    n = len(arg)
                    while i < n:
                        narg = sign + arg[i]
                        i += 1
                        if (narg in self.optmap):
                            if self.optmap[narg][1] == ArgParser.ARGUMENTLESS:
                                self.optmap[narg][2](narg, self.optmap[narg][0])
                                optqueue.append(narg)
                                argqueue.append(None)
                            elif self.optmap[narg][1] < ArgParser.VARIADIC:
                                optqueue.append(narg)
                                nargarg = arg[i:]
                                if len(nargarg) == 0:
                                    get += 1
                                else:
                                    argqueue.append(nargarg)
                                    self.optmap[narg][2](narg, self.optmap[narg][0], nargarg)
                                break
                            elif self.optmap[narg][1] == ArgParser.VARIADIC:
                                self.optmap[narg][2](narg, self.optmap[narg][0])
                                optqueue.append(narg)
                                nargarg = arg[i:]
                                argqueue.append(nargarg if len(nargarg) > 0 else None)
                                dashed = True
                                break
                        else:
                            unrecognised(narg)
            else:
                self.files.append(arg)
        
        i = 0
        n = len(optqueue)
        while i < n:
            opt = optqueue[i]
            arg = argqueue[i] if len(argqueue) > i else None
            if len(argqueue) <= i:
                option = self.optmap[opt]
                option[2](opt, option[0], None)
            i += 1
            opt = self.optmap[opt][0]
            if (opt not in self.opts) or (self.opts[opt] is None):
                self.opts[opt] = []
            self.opts[opt].append(arg)
        
        for arg in self.__options:
            if arg[0] == ArgParser.VARIADIC:
                varopt = self.opts[arg[4]]
                if varopt is not None:
                    if varopt[0] is None:
                        self.opts[arg[4]] = self.files
                    else:
                        self.opts[arg[4]] = varopt + self.files
                    self.files = []
                    break
        
        self.message = ' '.join(self.files) if len(self.files) > 0 else None
        
        if self.unrecognisedCount > 5:
            self.__print('%s: warning: %i more unrecognised %s' % (self.program, self.unrecognisedCount - 5, 'options' if self.unrecognisedCount == 6 else 'options'))
        
        return self.rc
    
    
    def support_alternatives(self):
        '''
        Maps up options that are alternatives to the first alternative for each option
        '''
        for alt in self.optmap:
            self.opts[alt] = self.opts[self.optmap[alt][0]]
    
    
    def test_exclusiveness(self, exclusives, exit_value = None):
        '''
        Checks for option conflicts
        
        @param   exclusives:set<str>  Exclusive options
        @param   exit_value:int?      The value to exit with on the check does not pass, `None` if not to exit
        @return  :bool                Whether at most one exclusive option was used
        '''
        used = []
        
        for opt in self.opts:
            if (self.opts[opt] is not None) and (opt in exclusives):
                used.append((opt, self.optmap[opt][0]))
        
        if len(used) > 1:
            msg = self.program + ': conflicting options:'
            for opt in used:
                if opt[1] == opt[0]:
                    msg += ' %s' % opt[0]
                else:
                    msg += ' %s(%s)' % opt
            self.__print(msg)
            self.__out.flush()
            if exit_value is not None:
                sys.exit(exit_value)
            return False
        return True
    
    
    def test_allowed(self, allowed, exit_value = None):
        '''
        Checks for out of context option usage
        
        @param   allowed:set<str>  Allowed options
        @param   exit_value:int?   The value to exit with on the check does not pass, `None` if not to exit
        @return  :bool             Whether only allowed options was used
        '''
        rc = True
        for opt in self.opts:
            if (self.opts[opt] is not None) and (opt not in allowed):
                msg = self.program + ': option used out of context: ' + opt
                if opt != self.optmap[opt][0]:
                    msg += '(' + self.optmap[opt][0] + ')'
                self.__print(msg)
                self.__out.flush()
                rc = False
        if (not rc) and (exit_value is not None):
            sys.exit(exit_value)
        return rc
    
    
    def test_files(self, min = 0, max = None, exit_value = None):
        '''
        Checks the correctness of the number of used non-option arguments
        
        @param   min:int          The minimum number of files
        @param   max:int?         The maximum number of files, `None` for unlimited
        @param   exit_value:int?  The value to exit with on the check does not pass, `None` if not to exit
        @return  :bool            Whether the usage was correct
        '''
        rc = (min <= len(self.files)) if max is None else (min <= len(self.files) <= max)
        if (not rc) and (exit_value is not None):
            sys.exit(exit_value)
        return rc
    
    
    def help(self, use_colours = None):
        '''
        Prints a help message
        
        @param  use_colours:bool?  Whether to use colours, `None` for if not piped
        '''
        if use_colours is None:
            use_colours = self.__out.isatty()
        
        bold      = lambda text      : ('\033[01m%s\033[21m' if use_colours else '%s') % text
        dim       = lambda text      : ('\033[02m%s\033[22m' if use_colours else '%s') % text
        emph      = lambda text      : ('\033[04m%s\033[24m' if use_colours else '%s') % text
        colourise = lambda text, clr : ('\033[%sm%s\033[00m' % (clr, text)) if use_colours else text
        
        self.__print('%s %s %s' % (bold(self.program), '-' if self.linuxvt else '—', self.__description))
        self.__print()
        if self.__longdescription is not None:
            desc = self.__longdescription
            if not use_colours:
                while '\033' in desc:
                    esc = desc.find('\033')
                    desc = desc[:esc] + desc[desc.find('m', esc) + 1:]
            self.__print(desc)
        self.__print()
        
        if self.__usage is not None:
            self.__print(bold('USAGE:'), end='')
            first = True
            for line in self.__usage.split('\n'):
                if first:
                    first = False
                else:
                    self.__print('    or', end='')
                if not use_colours:
                    while '\033' in line:
                        esc = line.find('\033')
                        line = line[:esc] + line[line.find('m', esc) + 1:]
                self.__print('\t%s' % line)
            self.__print()
        
        maxfirstlen = ['']
        for opt in self.__options:
            opt_alts = opt[1]
            opt_help = opt[3]
            if opt_help is None:
                continue
            first = opt_alts[0]
            last = opt_alts[-1]
            if first is not last:
                maxfirstlen.append(first)
        maxfirstlen = len(max(maxfirstlen, key = len))
        
        self.__print(bold('SYNOPSIS:'))
        (lines, lens) = ([], [])
        for opt in self.__options:
            opt_type = opt[0]
            opt_alts = opt[1]
            opt_arg = opt[2]
            opt_help = opt[3]
            if opt_help is None:
                continue
            (line, l) = ('', 0)
            first = opt_alts[0]
            last = opt_alts[-1]
            alts = ['', last] if first is last else [first, last]
            alts[0] += ' ' * (maxfirstlen - len(alts[0]))
            for opt_alt in alts:
                if opt_alt is alts[-1]:
                    line += '\0' + opt_alt
                    l += len(opt_alt)
                    if   opt_type == ArgParser.ARGUMENTED:     line += ' %s'      % emph(opt_arg);  l += len(opt_arg) + 1
                    elif opt_type == ArgParser.OPTARGUMENTED:  line += ' [%s]'    % emph(opt_arg);  l += len(opt_arg) + 3
                    elif opt_type == ArgParser.VARIADIC:       line += ' [%s...]' % emph(opt_arg);  l += len(opt_arg) + 6
                else:
                    line += '    %s  ' % dim(opt_alt)
                    l += len(opt_alt) + 6
            lines.append(line)
            lens.append(l)
        
        col = max(lens)
        col += 8 - ((col - 4) & 7)
        index = 0
        for opt in self.__options:
            opt_help = opt[3]
            if opt_help is None:
                continue
            first = True
            colour = '36' if (index & 1) == 0 else '34'
            self.__print(lines[index].replace('\0', ('\033[%s;01m' % colour) if use_colours else ''), end=' ' * (col - lens[index]))
            for line in opt_help.split('\n'):
                if first:
                    first = False
                    self.__print('%s' % (line), end='\033[00m\n' if use_colours else '\n')
                else:
                    self.__print('%s%s' % (' ' * col, colourise(line, colour)))
            index += 1
        
        self.__print()
        self.__out.flush()

