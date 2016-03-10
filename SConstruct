#SConstruct

# needed imports
from collections import (defaultdict, Counter as collections_counter)
import binascii
import errno
import itertools
import subprocess
import sys
import os
import SCons.Util

# Disable injecting tools into default namespace
SCons.Defaults.DefaultEnvironment(tools = [])

def message(program,msg):
	print "%s: %s" % (program.program_message_prefix, msg)

# endianess-checker
def checkEndian():
    if ARGUMENTS.has_key('endian'):
        r = ARGUMENTS['endian']
        if r == "little" or r == "big":
            return r
        raise SCons.Errors.UserError("Unknown endian value: %s" % r)
    import struct
    array = struct.pack('cccc', '\x01', '\x02', '\x03', '\x04')
    i = struct.unpack('i', array)
    if i == struct.unpack('<i', array):
        return "little"
    elif i == struct.unpack('>i', array):
        return "big"
    return "unknown"

def get_Werror_string(l):
	if l and '-Werror' in l:
		return '-W'
	return '-Werror='

class StaticSubprocess:
	from shlex import split as shlex_split
	class CachedCall:
		def __init__(self,out,err,returncode):
			self.out = out
			self.err = err
			self.returncode = returncode
	# @staticmethod delayed so that default arguments pick up the
	# undecorated form.
	def pcall(args,stderr=None,_call_cache={},_CachedCall=CachedCall):
		# Use repr since callers may construct the same argument
		# list independently.
		## >>> a = ['git', '--version']
		## >>> b = ['git', '--version']
		## >>> a is b
		## False
		## >>> id(a) == id(b)
		## False
		## >>> a == b
		## True
		## >>> repr(a) == repr(b)
		## True
		a = repr(args)
		try:
			return _call_cache[a]
		except KeyError:
			pass
		p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=stderr)
		(o, e) = p.communicate()
		_call_cache[a] = c = _CachedCall(o, e, p.wait())
		return c
	def qcall(args,stderr=None,_pcall=pcall,_shlex_split=shlex_split):
		return _pcall(_shlex_split(args),stderr)
	@staticmethod
	def get_version_head(cmd,_qcall=qcall):
		v = _qcall('%s %s' % (cmd, '--version'), stderr=subprocess.PIPE)
		try:
			return v.__version_head
		except AttributeError:
			v.__version_head = r = (v.out or v.err).splitlines()[0] if not v.returncode and (v.out or v.err) else None
			return r
	pcall = staticmethod(pcall)
	qcall = staticmethod(qcall)
	shlex_split = staticmethod(shlex_split)

class Git(StaticSubprocess):
	__path_git = None
	@classmethod
	def __pcall_missing_git(cls,args,stderr=None,_missing_git=StaticSubprocess.CachedCall(None, None, 1)):
		return _missing_git
	@classmethod
	def __pcall_found_git(cls,args,stderr=None,_pcall=StaticSubprocess.pcall):
		return _pcall(cls.__path_git + args, stderr=stderr)
	@classmethod
	def pcall(cls,args,stderr=None):
		git = cls.__path_git
		if git is None:
			cls.__path_git = git = cls.shlex_split(os.environ.get('GIT', 'git'))
		cls.pcall = f = cls.__pcall_found_git if git else cls.__pcall_missing_git
		return f(args, stderr)
	@classmethod
	def spcall(cls,args,stderr=None):
		g = cls.pcall(args, stderr)
		if g.returncode:
			return None
		return g.out

class ConfigureTests:
	class Collector:
		class RecordedTest:
			def __init__(self,name,desc):
				self.name = name
				self.desc = desc

		def __init__(self):
			self.tests = []
			self.record = self.tests.append
		def __call__(self,f):
			desc = None
			doc = getattr(f, '__doc__', None)
			if doc is not None:
				doc = doc.rstrip().splitlines()
				if doc and doc[-1].startswith("help:"):
					desc = doc[-1][5:]
			self.record(self.RecordedTest(f.__name__, desc))
			return f

	class Cxx11RequiredFeature:
		def __init__(self,name,text,main=''):
			self.name = name
			name = {'N' : 'test_' + name.replace(' ', '_')}
			self.text = text % name
			self.main = ('{' + (main % name) + '}\n') if main else ''
	class PCHAction:
		def __init__(self,context):
			self._context = context
		def __call__(self,text,ext):
			# Ignore caller-supplied text, since _Test always includes a
			# definition of main().  Using the caller-supplied text
			# would provide one main() in the PCH and another in the
			# test which includes the PCH.
			env = self._context.env
			s = env['OBJSUFFIX']
			env['OBJSUFFIX'] = '.h.gch'
			result = self._context.TryCompile('''
/* Define this here.  Use it in the file which includes the PCH.  If the
 * compiler skips including the PCH, and does not fail for that reason
 * alone, then it will fail when the symbol is used in the later test,
 * since the only definition comes from the PCH.
 */
#define dxx_compiler_supports_pch
			''', ext)
			env['OBJSUFFIX'] = s
			return result
	class PreservedEnvironment:
		# One empty list for all the defaults.  The comprehension
		# creates copies, so it is safe for the default value to be
		# shared.
		def __init__(self,env,keys,_l=[]):
			self.flags = {k: env.get(k, _l)[:] for k in keys}
			self.__getitem__ = self.flags.__getitem__
		def restore(self,env):
			env.Replace(**self.flags)
	class ForceVerboseLog:
		def __init__(self,env):
			# Force verbose output to sconf.log
			self.cc_env_strings = {}
			for k in (
				'CXXCOMSTR',
				'LINKCOMSTR',
			):
				try:
					# env is like a dict, but does not have .pop(), so
					# emulate it with a lookup + delete.
					self.cc_env_strings[k] = env[k]
					del env[k]
				except KeyError:
					pass
		def restore(self,env):
			# Restore potential quiet build options
			env.Replace(**self.cc_env_strings)
	class pkgconfig:
		def _get_pkg_config_exec_path(context,msgprefix,pkgconfig):
			Display = context.Display
			if not pkgconfig:
				Display("%s: pkg-config disabled by user settings\n" % msgprefix)
				return pkgconfig
			if os.sep in pkgconfig:
				Display("%s: using pkg-config at user specified path %s\n" % (msgprefix, pkgconfig))
				return pkgconfig
			join = os.path.join
			# No path specified, search in $PATH
			for p in os.environ.get('PATH', '').split(os.pathsep):
				fp = join(p, pkgconfig)
				try:
					os.close(os.open(fp, os.O_RDONLY))
				except OSError as e:
					# Ignore on permission errors.  If pkg-config is
					# runnable but not readable, the user must
					# specify its path.
					if e.errno == errno.ENOENT or e.errno == errno.EACCES:
						continue
					raise
				Display("%s: using pkg-config at discovered path %s\n" % (msgprefix, fp))
				return fp
			Display("%s: no usable pkg-config %r found in $PATH\n" % (msgprefix, pkgconfig))
		def __get_pkg_config_path(context,message,user_settings,display_name,
				_get_pkg_config_exec_path=_get_pkg_config_exec_path,
				_cache={}):
			pkgconfig = user_settings.PKG_CONFIG
			if pkgconfig is None:
				CHOST = user_settings.CHOST
				pkgconfig = ('%s-pkg-config' % CHOST) if CHOST else 'pkg-config'
				if sys.platform == 'win32':
					pkgconfig += '.exe'
			try:
				return _cache[pkgconfig]
			except KeyError:
				_cache[pkgconfig] = path = _get_pkg_config_exec_path(context, message, pkgconfig)
			return path
		@staticmethod
		def merge(context,message,user_settings,pkgconfig_name,display_name,
				guess_flags,
				__get_pkg_config_path=__get_pkg_config_path,
				_cache={}):
			Display = context.Display
			Display("%s: checking %s pkg-config %s\n" % (message, display_name, pkgconfig_name))
			pkgconfig = __get_pkg_config_path(context, message, user_settings, display_name)
			if not pkgconfig:
				Display("%s: skipping %s pkg-config; using default flags %r\n" % (message, display_name, guess_flags))
				return guess_flags
			cmd = '%s --cflags --libs %s' % (pkgconfig, pkgconfig_name)
			try:
				flags = _cache[cmd]
				Display("%s: reusing %s settings from `%s`: %r\n" % (message, display_name, cmd, flags))
				return flags
			except KeyError as e:
				Display("%s: reading %s settings from `%s`\n" % (message, display_name, cmd))
				try:
					flags = {
						k:v for k,v in context.env.ParseFlags('!' + cmd).items()
							if v and (k[0] == 'C' or k[0] == 'L')
					}
					Display("%s: %s settings: %r\n" % (message, display_name, flags))
				except OSError as o:
					Display("%s: %s pkg-config failed; using default flags for `%s`: %r\n" % (message, display_name, cmd, guess_flags))
					flags = guess_flags
				_cache[cmd] = flags
				return flags
	# Force test to report failure
	sconf_force_failure = 'force-failure'
	# Force test to report success, and modify flags like it
	# succeeded
	sconf_force_success = 'force-success'
	# Force test to report success, do not modify flags
	sconf_assume_success = 'assume-success'
	expect_sconf_success = 'success'
	expect_sconf_failure = 'failure'
	_implicit_test = Collector()
	_custom_test = Collector()
	implicit_tests = _implicit_test.tests
	custom_tests = _custom_test.tests
	comment_not_supported = '/* not supported */'
	__flags_Werror = {k:['-Werror'] for k in ['CXXFLAGS']}
	_cxx_conformance_cxx11 = 11
	_cxx_conformance_cxx14 = 14
	__cxx_conformance = None
	__cxx11_required_features = [
		Cxx11RequiredFeature('constexpr', '''
struct %(N)s {};
static constexpr %(N)s get_%(N)s(){return {};}
''', '''
	get_%(N)s();
'''),
		Cxx11RequiredFeature('nullptr', '''
#include <cstddef>
std::nullptr_t %(N)s1 = nullptr;
int *%(N)s2 = nullptr;
''', '''
	%(N)s2 = %(N)s1;
'''),
		Cxx11RequiredFeature('explicit operator bool', '''
struct %(N)s {
	explicit operator bool();
};
'''),
		Cxx11RequiredFeature('template aliases', '''
using %(N)s_typedef = int;
template <typename>
struct %(N)s_struct;
template <typename T>
using %(N)s_alias = %(N)s_struct<T>;
''', '''
	%(N)s_struct<int> *a = nullptr;
	%(N)s_alias<int> *b = a;
	%(N)s_typedef *c = nullptr;
	(void)b;
	(void)c;
'''),
		Cxx11RequiredFeature('trailing function return type', '''
auto %(N)s()->int;
'''),
		Cxx11RequiredFeature('class scope static constexpr assignment', '''
struct %(N)s_instance {
};
struct %(N)s_container {
	static constexpr %(N)s_instance a = {};
};
'''),
		Cxx11RequiredFeature('braced base class initialization', '''
struct %(N)s_base {
	int a;
};
struct %(N)s_derived : %(N)s_base {
	%(N)s_derived(int e) : %(N)s_base{e} {}
};
'''),
		Cxx11RequiredFeature('std::unordered_map::emplace', '''
#include <unordered_map>
''', '''
	std::unordered_map<int,int> m;
	m.emplace(0, 0);
'''
),
]
	def __init__(self,msgprefix,user_settings,platform_settings):
		self.msgprefix = msgprefix
		self.user_settings = user_settings
		self.platform_settings = platform_settings
		self.successful_flags = defaultdict(list)
		self._sconf_results = []
		self.__tool_versions = []
	def _quote_macro_value(v):
		return v.strip().replace('\n', ' \\\n')
	def _check_sconf_forced(self,calling_function):
		return self._check_forced(calling_function), self._check_expected(calling_function)
	@staticmethod
	def _find_calling_sconf_function():
		try:
			1//0
		except ZeroDivisionError:
			frame = sys.exc_info()[2].tb_frame.f_back.f_back
			while frame is not None:
				co_name = frame.f_code.co_name
				if co_name[:6] == 'check_':
					return co_name[6:]
				frame = frame.f_back
		# This assertion is hit if a test is asked to deduce its caller
		# (calling_function=None), but no function in the call stack appears to
		# be a checking function.
		assert False, "SConf caller not specified and no acceptable caller in stack."
	def _check_forced(self,name):
		# This getattr will raise AttributeError if called for a function which
		# is not a registered test.  Tests must be registered as an implicit
		# test (in implicit_tests, usually by applying the @_implicit_test
		# decorator) or a custom test (in custom_tests, usually by applying the
		# @_custom_test decorator).
		#
		# Unregistered tests are never documented and cannot be overridden by
		# the user.
		return getattr(self.user_settings, 'sconf_%s' % name)
	def _check_expected(self,name):
		# The remarks for _check_forced apply here too.
		r = getattr(self.user_settings, 'expect_sconf_%s' % name)
		if r is not None:
			if r == self.expect_sconf_success:
				return 1
			if r == self.expect_sconf_failure:
				return 0
		return r
	def _check_macro(self,context,macro_name,macro_value,test,_comment_not_supported=comment_not_supported,**kwargs):
		r = self.Compile(context, text="""
#define {macro_name} {macro_value}
{test}
""".format(macro_name=macro_name, macro_value=macro_value, test=test), **kwargs)
		context.sconf.Define(macro_name, macro_value if r else _comment_not_supported)
	implicit_tests.append(_implicit_test.RecordedTest('check_ccache_distcc_ld_works', "assume ccache, distcc, C++ compiler, and C++ linker work"))
	implicit_tests.append(_implicit_test.RecordedTest('check_ccache_ld_works', "assume ccache, C++ compiler, and C++ linker work"))
	implicit_tests.append(_implicit_test.RecordedTest('check_distcc_ld_works', "assume distcc, C++ compiler, and C++ linker work"))
	implicit_tests.append(_implicit_test.RecordedTest('check_ld_works', "assume C++ compiler and linker work"))
	# This must be the first custom test.  This test verifies the compiler
	# works and disables any use of ccache/distcc for the duration of the
	# configure run.
	#
	# SCons caches configuration results and tests are usually very small, so
	# ccache will provide limited benefit.
	#
	# Some tests are expected to raise a compiler error.  If distcc is used
	# and DISTCC_FALLBACK prevents local retries, then distcc interprets a
	# compiler error as an indication that the volunteer which served that
	# compile is broken and should be blacklisted.  Suppress use of distcc for
	# all tests to avoid spurious blacklist entries.
	#
	# During the main build, compiling remotely can allow more jobs to run in
	# parallel.  Tests are serialized by SCons, so distcc is helpful during
	# testing only if compiling remotely is faster than compiling locally.
	# This may be true for embedded systems that distcc to desktops, but will
	# not be true for desktops or laptops that distcc to similar sized
	# machines.
	@_custom_test
	def check_cxx_works(self,context):
		"""
help:assume C++ compiler works
"""
		cenv = context.env
		penv = cenv['ENV']
		self.__cxx_com_prefix = cenv['CXXCOM']
		# Require ccache to run the next stage, but allow it to write the
		# result to cache.  This lets the test validate that ccache fails for
		# an unusable CCACHE_DIR and also validate that the next stage handles
		# the input correctly.  Without this, a cached result may hide that
		# the next stage compiler (or wrapper) worked when a prior run
		# performed the test, but is now broken.
		CCACHE_RECACHE = penv.get('CCACHE_RECACHE', None)
		penv['CCACHE_RECACHE'] = '1'
		most_recent_error = self._check_cxx_works(context)
		if most_recent_error is not None:
			raise SCons.Errors.StopError(most_recent_error)
		if CCACHE_RECACHE is None:
			del penv['CCACHE_RECACHE']
		else:
			penv['CCACHE_RECACHE'] = CCACHE_RECACHE
		# If ccache/distcc are in use, disable them during testing.
		# This assignment is also done in _check_cxx_works, but only on an
		# error path.  Repeat it here so that it is effective on the success
		# path.  It cannot be moved above the call to _check_cxx_works because
		# some tests in _check_cxx_works rely on its original value.
		cenv['CXXCOM'] = cenv._dxx_cxxcom_no_prefix
		self._check_cxx_conformance_level(context)
	def _show_tool_version(self,context,tool,desc,save_tool_version=True):
		# These version results are not used for anything, but are
		# collected here so that users who post only a build log will
		# still supply at least some useful information.
		#
		# This is split into two lines so that the first line is printed
		# before the function call required to format the string for the
		# second line.
		Display = context.Display
		Display('%s: checking version of %s %r ... ' % (self.msgprefix, desc, tool))
		try:
			v = StaticSubprocess.get_version_head(tool)
		except OSError as e:
			if e.errno == errno.ENOENT or e.errno == errno.EACCES:
				Display('error: %s\n' % e.strerror)
				raise SCons.Errors.StopError('Failed to run %s.' % tool)
			raise
		if save_tool_version:
			self.__tool_versions.append((tool, v))
		Display('%r\n' % v)
	def _show_indirect_tool_version(self,context,CXX,tool,desc):
		Display = context.Display
		Display('%s: checking path to %s ... ' % (self.msgprefix, desc))
		# Include $LINKFLAGS since -fuse-ld=gold influences the path
		# printed for the linker.
		tool = context.env.subst('$CXX $CXXFLAGS $LINKFLAGS -print-prog-name=%s' % tool)
		name = StaticSubprocess.qcall(tool).out.strip()
		self.__tool_versions.append((tool, name))
		if not name:
			# Strange, but not fatal for this to fail.
			Display('! %r\n' % name)
			return
		Display('%r\n' % name)
		self._show_tool_version(context,name,desc)
	def _check_cxx_works(self,context,_crc32=binascii.crc32):
		# Test whether the compiler+linker+optional wrapper(s) work.  If
		# anything fails, a StopError is guaranteed on return.  However, to
		# help the user, this function pushes through all the combinations and
		# reports the StopError for the least complicated issue.  If both the
		# compiler and the linker fail, the compiler will be reported, since
		# the linker might work once the compiler is fixed.
		#
		# If a test fails, then the pending StopError allows this function to
		# safely modify the construction environment and process environment
		# without reverting its changes.
		most_recent_error = None
		Link = self.Link
		cenv = context.env
		use_distcc = self.user_settings.distcc
		use_ccache = self.user_settings.ccache
		if self.user_settings.show_tool_version:
			CXX = cenv['CXX']
			self._show_tool_version(context, CXX, 'C++ compiler')
			self._show_indirect_tool_version(context, CXX, 'as', 'assembler')
			self._show_indirect_tool_version(context, CXX, 'ld', 'linker')
			if use_distcc:
				self._show_tool_version(context, use_distcc, 'distcc', False)
			if use_ccache:
				self._show_tool_version(context, use_ccache, 'ccache', False)
		# Use C++ single line comment so that it is guaranteed to extend
		# to the end of the line.  repr ensures that embedded newlines
		# will be escaped and that the final character will not be a
		# backslash.
		self.__commented_tool_versions = s = ''.join('// %r => %r\n' % (v[0], v[1]) for v in self.__tool_versions)
		self.__tool_versions = '''
/* This test is always false.  Use a non-trivial condition to
 * discourage external text scanners from recognizing that the block
 * is never compiled.
 */
#if 1 < -1
%.8x
%s
#endif
''' % (_crc32(s), s)
		if use_ccache:
			if use_distcc:
				if Link(context, text='', msg='whether ccache, distcc, C++ compiler, and linker work', calling_function='ccache_distcc_ld_works'):
					return
				most_recent_error = 'ccache and C++ linker work, but distcc does not work.'
				# Disable distcc so that the next call to self.Link tests only
				# ccache+linker.
				del cenv['ENV']['CCACHE_PREFIX']
			if Link(context, text='', msg='whether ccache, C++ compiler, and linker work', calling_function='ccache_ld_works'):
				return most_recent_error
			most_recent_error = 'C++ linker works, but ccache does not work.'
		elif use_distcc:
			if Link(context, text='', msg='whether distcc, C++ compiler, and linker work', calling_function='distcc_ld_works'):
				return
			most_recent_error = 'C++ linker works, but distcc does not work.'
		else:
			# This assertion fails if the environment's $CXXCOM was modified
			# to use a prefix, but both user_settings.ccache and
			# user_settings.distcc evaluate to false.
			assert cenv._dxx_cxxcom_no_prefix is cenv['CXXCOM'], "Unexpected prefix in $CXXCOM."
		# If ccache/distcc are in use, then testing with one or both of them
		# failed.  Disable them so that the next test can check whether the
		# local linker works.
		#
		# If they are not in use, this assignment is a no-op.
		cenv['CXXCOM'] = cenv._dxx_cxxcom_no_prefix
		if Link(context, text='', msg='whether C++ compiler and linker work', calling_function='ld_works'):
			return most_recent_error
		# Force only compile, even if LTO is enabled.
		elif self._Compile(context, text='', msg='whether C++ compiler works', calling_function='cxx_works'):
			return 'C++ compiler works, but C++ linker does not work.'
		else:
			return 'C++ compiler does not work.'
	implicit_tests.append(_implicit_test.RecordedTest('check_cxx11', "assume C++ compiler supports C++11"))
	implicit_tests.append(_implicit_test.RecordedTest('check_cxx14', "assume C++ compiler supports C++14"))
	__cxx_conformance_CXXFLAGS = [None]
	def _check_cxx_conformance_level(self,context,_levels=(
			# List standards in descending order of preference
			_cxx_conformance_cxx14,
			# C++11 is required, so list it last.  Omit the comma as a
			# reminder not to append elements to the list.
			_cxx_conformance_cxx11
		), _CXXFLAGS=__cxx_conformance_CXXFLAGS,
		_successflags={'CXXFLAGS' : __cxx_conformance_CXXFLAGS}
		):
		# Testing the compiler option parser only needs Compile, even when LTO
		# is enabled.
		Compile = self._Compile
		# GCC started with -std=gnu++0x for C++0x (later C++11).  In gcc-4.7,
		# GCC began accepting -std=gnu++11.  Since gcc-4.6 does not accept
		# some constructs used in the code, use the newer name here.
		#
		# Accepted options by version:
		#
		#	gcc-4.6 -std=gnu++0x
		#
		#	gcc-4.7 -std=gnu++0x
		#	gcc-4.7 -std=gnu++11
		#
		#	gcc-4.8 -std=gnu++0x
		#	gcc-4.8 -std=gnu++11
		#	gcc-4.8 -std=gnu++1y
		#
		#	gcc-4.9 -std=gnu++0x
		#	gcc-4.9 -std=gnu++11
		#	gcc-4.9 -std=gnu++1y
		#	gcc-4.9 -std=gnu++14
		#
		#	gcc-5 -std=gnu++0x
		#	gcc-5 -std=gnu++11
		#	gcc-5 -std=gnu++1y
		#	gcc-5 -std=gnu++14
		#	gcc-5 -std=gnu++1z
		#	gcc-5 -std=gnu++17
		#
		# In all supported cases except gcc-4.8, gcc accepts the number-only
		# form if it accepts the approximated form.  The only C++14 feature of
		# interest in gcc-4.8 is return type deduction, which cannot be used
		# until gcc-4.7 is retired.  Therefore, it is acceptable for this
		# check not to detect C++14 support in gcc-4.8.
		for level in _levels:
			opt = '-std=gnu++%u' % level
			_CXXFLAGS[0] = opt
			if Compile(context, text='', msg='whether C++ compiler accepts {opt}'.format(opt=opt), successflags=_successflags, calling_function='cxx%s' % level):
				self.__cxx_conformance = level
				return
		raise SCons.Errors.StopError('C++ compiler does not accept any supported C++ -std option.')
	def Compile(self,context,**kwargs):
		# Some tests check the functionality of the compiler's
		# optimizer.
		#
		# When LTO is used, the optimizer is deferred to link time.
		# Force all tests to be Link tests when LTO is enabled.
		self.Compile = self.Link if self.user_settings.lto else self._Compile
		return self.Compile(context, **kwargs)
	def _Test(self,context,text,msg,action,main='',ext='.cpp',testflags={},successflags={},skipped=None,successmsg=None,failuremsg=None,expect_failure=False,calling_function=None):
		if calling_function is None:
			calling_function = self._find_calling_sconf_function()
		context.Message('%s: checking %s...' % (self.msgprefix, msg))
		if skipped is not None:
			context.Result('(skipped){skipped}'.format(skipped=skipped))
			if self.user_settings.record_sconf_results:
				self._sconf_results.append((calling_function, 'skipped'))
			return
		env_flags = self.PreservedEnvironment(context.env, successflags.keys() + testflags.keys() + self.__flags_Werror.keys() + ['CPPDEFINES'])
		context.env.MergeFlags(successflags)
		forced, expected = self._check_sconf_forced(calling_function)
		caller_modified_env_flags = self.PreservedEnvironment(context.env, self.__flags_Werror.keys() + testflags.keys())
		# Always pass -Werror
		context.env.Append(**self.__flags_Werror)
		context.env.Append(**testflags)
		# If forced is None, run the test.  Otherwise, skip the test and
		# take an action determined by the value of forced.
		if forced is None:
			r = action('''
%s
%s

#undef main	/* avoid -Dmain=SDL_main from libSDL */
int main(int argc,char**argv){(void)argc;(void)argv;
%s

;}
''' % (self.__tool_versions, text, main), ext)
			# Some tests check that the compiler rejects an input.
			# SConf considers the result a failure when the compiler
			# rejects the input.  For tests that consider a rejection to
			# be the good result, this conditional flips the sense of
			# the result so that a compiler rejection is reported as
			# success.
			if expect_failure:
				r = not r
			context.Result((successmsg if r else failuremsg) or r)
			if expected is not None and r != expected:
				raise SCons.Errors.StopError('Expected and actual results differ.  Test should %s, but it did not.' % ('succeed' if expected else 'fail'))
		else:
			choices = (self.sconf_force_failure, self.sconf_force_success, self.sconf_assume_success)
			if forced not in choices:
				try:
					forced = choices[int(forced)]
				except ValueError:
					raise SCons.Errors.UserError("Unknown force value for sconf_%s: %s" % (co_name[6:], forced))
				except IndexError:
					raise SCons.Errors.UserError("Out of range force value for sconf_%s: %s" % (co_name[6:], forced))
			if forced == self.sconf_force_failure:
				# Pretend the test returned a failure result
				r = False
			elif forced == self.sconf_force_success or forced == self.sconf_assume_success:
				# Pretend the test succeeded.  Forced success modifies
				# the environment as if the test had run and succeeded.
				# Assumed success modifies the environment as if the
				# test had run and failed.
				#
				# The latter is used when the user arranges for the
				# environment to be correct.  For example, if the
				# compiler understands C++14, but uses a non-standard
				# name for the option, the user would set assume-success
				# and add the appropriate option to CXXFLAGS.
				r = True
			else:
				raise SCons.Errors.UserError("Unknown force value for sconf_%s: %s" % (co_name[6:], forced))
			# Flip the sense of the forced value, so that users can
			# treat "force-failure" as force-bad-result regardless of
			# whether the bad result is that the compiler rejected good
			# input or that the compiler accepted bad input.
			if expect_failure:
				r = not r
			context.Result('(forced){inverted}{forced}'.format(forced=forced, inverted='(inverted)' if expect_failure else ''))
		# On success, revert to base flags + successflags
		# On failure, revert to base flags
		if r and forced != self.sconf_assume_success:
			caller_modified_env_flags.restore(context.env)
			context.env.Replace(CPPDEFINES=env_flags['CPPDEFINES'])
			f = self.successful_flags
			# Move most CPPDEFINES to the generated header, so that
			# command lines are shorter.
			for k, v in successflags.iteritems():
				if k == 'CPPDEFINES':
					continue
				f[k].extend(v)
			d = successflags.get('CPPDEFINES', None)
			if d:
				append_CPPDEFINE = f['CPPDEFINES'].append
				for v in d:
					# v is 'NAME' for -DNAME
					# v is ('NAME', 'VALUE') for -DNAME=VALUE
					d = (v, None) if isinstance(v, str) else v
					if d[0] in ('_REENTRANT',):
						# Blacklist defines that must not be moved to the
						# configuration header.
						append_CPPDEFINE(v)
						continue
					context.sconf.Define(d[0], d[1])
		else:
			env_flags.restore(context.env)
		self._sconf_results.append((calling_function, r))
		return r
	def _Compile(self,context,_Test=_Test,**kwargs):
		return _Test(self, context,action=context.TryCompile, **kwargs)
	def Link(self,context,_Test=_Test,**kwargs):
		return _Test(self, context,action=context.TryLink, **kwargs)
	# Compile and link a program that uses a system library.  On
	# success, return None.  On failure, return a tuple indicating which
	# stage failed and providing a text error message to show to the
	# user.  Some callers handle failure by retrying with other options.
	# Others abort the SConf run.
	def _soft_check_system_library(self,context,header,main,lib,successflags={}):
		include = '\n'.join(['#include <%s>' % h for h in header])
		header = header[-1]
		# Test library.  On success, good.  On failure, test header to
		# give the user more help.
		if self.Link(context, text=include, main=main, msg='for usable library %s' % lib, successflags=successflags):
			return
		if self.Compile(context, text=include, main=main, msg='for usable header %s' % header, testflags=successflags):
			return (0, "Header %s is usable, but library %s is not usable." % (header, lib))
		if self.Compile(context, text=include, main='', msg='for parseable header %s' % header, testflags=successflags):
			return (1, "Header %s is parseable, but cannot compile the test program." % header)
		return (2, "Header %s is missing or unusable." % header)
	# Compile and link a program that uses a system library.  On
	# success, return None.  On failure, abort the SConf run.
	def _check_system_library(self,*args,**kwargs):
		e = self._soft_check_system_library(*args, **kwargs)
		if e:
			raise SCons.Errors.StopError(e[1])
	# User settings tests are hidden because they do not respect sconf_*
	# overrides, so the user should not be offered an override.
	@_custom_test
	def _check_user_settings_opengl(self,context,_CPPDEFINES_OGLES=('OGL', 'OGLES'),_CPPDEFINES_OGL=('OGL',)):
		user_settings = self.user_settings
		Result = context.Result
		if user_settings.opengles:
			s = '%s: building with OpenGL ES'
			CPPDEFINES = _CPPDEFINES_OGLES
		elif user_settings.opengl:
			s = '%s: building with OpenGL'
			CPPDEFINES = _CPPDEFINES_OGL
		else:
			Result('%s: building with software renderer' % self.msgprefix)
			return
		self.successful_flags['CPPDEFINES'].append(CPPDEFINES)
		Result(s % self.msgprefix)
	def _result_check_user_setting(self,context,condition,CPPDEFINES,label):
		if condition:
			self.successful_flags['CPPDEFINES'].extend(CPPDEFINES)
		context.Result('%s: checking whether to enable %s...%s' % (self.msgprefix, label, 'yes' if condition else 'no'))
	@_custom_test
	def _check_user_settings_debug(self,context,_CPPDEFINES=(('NDEBUG',), ('RELEASE',))):
		self._result_check_user_setting(context, not self.user_settings.debug, _CPPDEFINES, 'release options')
	@_custom_test
	def _check_user_settings_memdebug(self,context,_CPPDEFINES=(('DEBUG_MEMORY_ALLOCATIONS',),)):
		self._result_check_user_setting(context, self.user_settings.memdebug, _CPPDEFINES, 'memory allocation tracking')
	@_custom_test
	def _check_user_settings_editor(self,context,_CPPDEFINES=(('EDITOR',),)):
		self._result_check_user_setting(context, self.user_settings.editor, _CPPDEFINES, 'level editor')
	@_custom_test
	def _check_user_settings_ipv6(self,context,_CPPDEFINES=(('IPv6',),)):
		self._result_check_user_setting(context, self.user_settings.ipv6, _CPPDEFINES, 'IPv6 support')
	@_custom_test
	def _check_user_settings_udp(self,context,_CPPDEFINES=(('USE_UDP',),)):
		self._result_check_user_setting(context, self.user_settings.use_udp, _CPPDEFINES, 'multiplayer over UDP')
	@_custom_test
	def _check_user_settings_tracker(self,context,_CPPDEFINES=(('USE_TRACKER',),)):
		self._result_check_user_setting(context, self.user_settings.use_tracker, _CPPDEFINES, 'UDP game tracker')
	@_custom_test
	def check_libphysfs(self,context,_header=('physfs.h',)):
		main = '''
	PHYSFS_File *f;
	char b[1] = {0};
	PHYSFS_init("");
	f = PHYSFS_openWrite("a");
	PHYSFS_sint64 w = PHYSFS_write(f, b, 1, 1);
	(void)w;
	PHYSFS_close(f);
	f = PHYSFS_openRead("a");
	PHYSFS_sint64 r = PHYSFS_read(f, b, 1, 1);
	(void)r;
	PHYSFS_close(f);
'''
		l = ['physfs']
		successflags = {'LIBS' : l}
		e = self._soft_check_system_library(context, header=_header, main=main, lib='physfs', successflags=successflags)
		if not e:
			return
		if e[0] == 0:
			context.Display("%s: physfs header usable; adding zlib and retesting library\n" % self.msgprefix)
			l.append('z')
			e = self._soft_check_system_library(context, header=_header, main=main, lib='physfs', successflags=successflags)
		if e:
			raise SCons.Errors.StopError(e[1])
	@_custom_test
	def _check_SDL(self,context):
		if self.user_settings.sdl2:
			self.check_libSDL2(context)
			self.check_SDL2_mixer(context)
		else:
			self.check_libSDL(context)
			self.check_SDL_mixer(context)
	@_implicit_test
	def check_libSDL(self,context,_guess_flags={
			'CPPFLAGS' : ['-I/usr/include/SDL'],
			'LIBS' : ['SDL'],
		}):
		self._check_libSDL(context, '', _guess_flags)
	@_implicit_test
	def check_libSDL2(self,context,_guess_flags={
			'CPPFLAGS' : ['-I/usr/include/SDL2'],
			'LIBS' : ['SDL2'],
		}):
		self._check_libSDL(context, '2', _guess_flags)
	def _check_libSDL(self,context,sdl2,guess_flags):
		user_settings = self.user_settings
		successflags = self.pkgconfig.merge(context, self.msgprefix, user_settings, 'sdl%s' % sdl2, 'SDL%s' % sdl2, guess_flags).copy()
		if user_settings.max_joysticks:
			# If joysticks are enabled, but all possible inputs are
			# disabled, then disable joystick support.
			if not (user_settings.max_axes_per_joystick or user_settings.max_buttons_per_joystick or user_settings.max_hats_per_joystick):
				user_settings.max_joysticks = 0
		else:
			# If joysticks are disabled, then disable all possible
			# inputs.
			user_settings.max_axes_per_joystick = user_settings.max_buttons_per_joystick = user_settings.max_hats_per_joystick = 0
		successflags['CPPDEFINES'] = CPPDEFINES = successflags.get('CPPDEFINES', [])[:]
		CPPDEFINES.extend((
			('MAX_JOYSTICKS', user_settings.max_joysticks),
			('MAX_AXES_PER_JOYSTICK', user_settings.max_axes_per_joystick),
			('MAX_BUTTONS_PER_JOYSTICK', user_settings.max_buttons_per_joystick),
			('MAX_HATS_PER_JOYSTICK', user_settings.max_hats_per_joystick),
		))
		context.Display('%s: checking whether to enable joystick support...%s\n' % (self.msgprefix, 'yes' if user_settings.max_joysticks else 'no'))
		# SDL2 removed CD-rom support.
		init_cdrom = '0' if sdl2 else 'SDL_INIT_CDROM'
		self._check_system_library(context,header=['SDL.h'],main='''
	SDL_RWops *ops = reinterpret_cast<SDL_RWops *>(argv);
#if MAX_JOYSTICKS
#define DXX_SDL_INIT_JOYSTICK	SDL_INIT_JOYSTICK |
#else
#define DXX_SDL_INIT_JOYSTICK
#endif
	SDL_Init(DXX_SDL_INIT_JOYSTICK %s | SDL_INIT_VIDEO | SDL_INIT_AUDIO);
#if MAX_JOYSTICKS
	auto n = SDL_NumJoysticks();
	(void)n;
#endif
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_FreeRW(ops);
	SDL_Quit();
''' % init_cdrom,
			lib='SDL', successflags=successflags
		)
	# SDL_mixer/SDL2_mixer use the same -I line as SDL/SDL2
	@_implicit_test
	def check_SDL_mixer(self,context,_guess_flags={
			'LIBS' : ['SDL_mixer'],
		}):
		self._check_SDL_mixer(context, '', _guess_flags)
	@_implicit_test
	def check_SDL2_mixer(self,context,_guess_flags={
			'LIBS' : ['SDL2_mixer'],
		}):
		self._check_SDL_mixer(context, '2', _guess_flags)
	def _check_SDL_mixer(self,context,sdl2,guess_flags):
		mixer = 'SDL%s_mixer' % sdl2
		user_settings = self.user_settings
		context.Display('%s: checking whether to use %s...%s\n' % (self.msgprefix, mixer, 'yes' if user_settings.sdlmixer else 'no'))
		# SDL_mixer support?
		if not user_settings.sdlmixer:
			return
		self.successful_flags['CPPDEFINES'].append('USE_SDLMIXER')
		successflags = self.pkgconfig.merge(context, self.msgprefix, user_settings, mixer, mixer, guess_flags)
		if user_settings.host_platform == 'darwin':
			successflags = successflags.copy()
			successflags['FRAMEWORKS'] = [mixer]
			relative_headers = 'Library/Frameworks/%s.framework/Headers' % mixer
			successflags['CPPPATH'] = [os.path.join(os.getenv("HOME"), relative_headers), '/%s' % relative_headers]
		self._check_system_library(context,header=['SDL_mixer.h'],main='''
	int i = Mix_Init(MIX_INIT_FLAC | MIX_INIT_OGG);
	(void)i;
	Mix_Pause(0);
	Mix_ResumeMusic();
	Mix_Quit();
''',
			lib=mixer, successflags=successflags)
	@_custom_test
	def check_compiler_missing_field_initializers(self,context,
		_testflags_warn={'CXXFLAGS' : ['-Wmissing-field-initializers']},
		_successflags_nowarn={'CXXFLAGS' : ['-Wno-missing-field-initializers']}
	):
		"""
Test whether the compiler warns for a statement of the form

	variable={};

gcc-4.x warns for this form, but -Wno-missing-field-initializers silences it.
gcc-5 does not warn.

This form is used extensively in the code as a shorthand for resetting
variables to their default-constructed value.
"""
		text = 'struct A{int a;};'
		main = 'A a{};(void)a;'
		if self.Compile(context, text=text, main=main, msg='whether C++ compiler accepts {} initialization', testflags=_testflags_warn) or \
			self.Compile(context, text=text, main=main, msg='whether C++ compiler understands -Wno-missing-field-initializers', successflags=_successflags_nowarn) or \
			not self.Compile(context, text=text, main=main, msg='whether C++ compiler always errors for {} initialization', expect_failure=True):
			return
		raise SCons.Errors.StopError("C++ compiler errors on {} initialization, even with -Wno-missing-field-initializers.")
	@_custom_test
	def check_attribute_error(self,context):
		"""
Test whether the compiler accepts and properly implements gcc's function
attribute [__attribute__((__error__))][1].

A proper implementation will compile correctly if the function is
declared and, after optimizations are applied, the function is not
called.  If this function attribute is not supported, then
DXX_ALWAYS_ERROR_FUNCTION results in link-time errors, rather than
compile-time errors.

This test will report failure if the optimizer does not remove the call
to the marked function.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-g_t_0040code_007berror_007d-function-attribute-3097

help:assume compiler supports __attribute__((error))
"""
		self._check_function_dce_attribute(context, 'error')
	def _check_function_dce_attribute(self,context,attribute):
		__attribute__ = '__%s__' % attribute
		f = '''
void a()__attribute__((%s("a called")));
''' % __attribute__
		macro_name = '__attribute_%s(M)' % attribute
		if self.Compile(context, text=f, main='if("0"[0]==\'1\')a();', msg='whether compiler optimizes function __attribute__((%s))' % __attribute__):
			context.sconf.Define('DXX_HAVE_ATTRIBUTE_%s' % attribute.upper())
			context.sconf.Define(macro_name, '__attribute__((%s(M)))' % __attribute__)
		else:
			self.Compile(context, text=f, msg='whether compiler accepts function __attribute__((%s))' % __attribute__) and \
			self.Compile(context, text=f, main='a();', msg='whether compiler understands function __attribute__((%s))' % __attribute__, expect_failure=True)
			context.sconf.Define(macro_name, self.comment_not_supported)
	@_custom_test
	def check_builtin_bswap(self,context,
		_main='''
	(void)__builtin_bswap64(static_cast<uint64_t>(argc));
	(void)__builtin_bswap32(static_cast<uint32_t>(argc));
#ifdef DXX_HAVE_BUILTIN_BSWAP16
/* Added in gcc-4.8 */
	(void)__builtin_bswap16(static_cast<uint16_t>(argc));
#endif
''',
		_successflags_bswap16={'CPPDEFINES' : ['DXX_HAVE_BUILTIN_BSWAP', 'DXX_HAVE_BUILTIN_BSWAP16']},
		_successflags_bswap={'CPPDEFINES' : ['DXX_HAVE_BUILTIN_BSWAP']},
	):
		"""
Test whether the compiler accepts the gcc byte swapping intrinsic
functions.  These functions may be optimized into architecture-specific
swap instructions when the idiomatic swap is not.

	u16 = (u16 << 8) | (u16 >> 8);

The 32-bit version ([__builtin_bswap32][1]) and 64-bit version
([__builtin_bswap64][2]) are present in all supported versions of
gcc.  The 16-bit version ([__builtin_bswap16][3]) was added in
gcc-4.8.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-g_t_005f_005fbuiltin_005fbswap32-4135
[2]: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-g_t_005f_005fbuiltin_005fbswap64-4136
[3]: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-g_t_005f_005fbuiltin_005fbswap16-4134
"""
		include = '''
#include <cstdint>
'''
		if self.Compile(context, text=include, main=_main, msg='whether compiler implements __builtin_bswap{16,32,64} functions', successflags=_successflags_bswap16) or \
			self.Compile(context, text=include, main=_main, msg='whether compiler implements __builtin_bswap{32,64} functions', successflags=_successflags_bswap):
			return
	@_custom_test
	def check_builtin_constant_p(self,context):
		"""
Test whether the compiler accepts and properly implements gcc's
intrinsic [__builtin_constant_p][1].  A proper implementation will
compile correctly if the intrinsic is recognized and, after
optimizations are applied, the intrinsic returns true for a constant
input and that return value is used to optimize away dead code.
If this intrinsic is not supported, or if applying optimizations
does not make the intrinsic report true, then the test reports
failure.  A failure here disables some compile-time sanity checks.

This test is known to fail when optimizations are disabled.  The failure
is not a bug in the test or in the compiler.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-g_t_005f_005fbuiltin_005fconstant_005fp-4082

help:assume compiler supports compile-time __builtin_constant_p
"""
		f = '''
int c(int);
static int a(int b){
	return __builtin_constant_p(b) ? 1 : %s;
}
'''
		main = 'return a(1) + a(2)'
		if self.Link(context, text=f % 'c(b)', main=main, msg='whether compiler optimizes __builtin_constant_p'):
			context.sconf.Define('DXX_HAVE_BUILTIN_CONSTANT_P')
			context.sconf.Define('dxx_builtin_constant_p(A)', '__builtin_constant_p(A)')
			context.sconf.Define('DXX_CONSTANT_TRUE(E)', '(__builtin_constant_p((E)) && (E))')
		else:
			self.Compile(context, text=f % '2', main=main, msg='whether compiler accepts __builtin_constant_p')
			context.sconf.Define('dxx_builtin_constant_p(A)', '((void)(A),0)')
	@_custom_test
	def check_builtin_expect(self,context):
		"""
Test whether the compiler accepts gcc's intrinsic
[__builtin_expect][1].  This intrinsic is a hint to the optimizer,
which it may ignore.  The test does not try to detect whether the
optimizer respects such hints.

When this test succeeds, conditional tests can hint to the optimizer
which path should be considered hot.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-g_t_005f_005fbuiltin_005fexpect-4083
"""
		main = '''
return __builtin_expect(argc == 1, 1) ? 1 : 0;
'''
		if self.Compile(context, text='', main=main, msg='whether compiler accepts __builtin_expect'):
			context.sconf.Define('likely(A)', '__builtin_expect(!!(A), 1)')
			context.sconf.Define('unlikely(A)', '__builtin_expect(!!(A), 0)')
		else:
			macro_value = '(!!(A))'
			context.sconf.Define('likely(A)', macro_value)
			context.sconf.Define('unlikely(A)',  macro_value)
	@_custom_test
	def check_builtin_object_size(self,context):
		"""
Test whether the compiler accepts and optimizes gcc's intrinsic
[__builtin_object_size][1].  If this intrinsic is optimized,
compile-time checks can verify that the caller-specified constant
size of a variable does not exceed the compiler-determined size of
the variable.  If the compiler cannot determine the size of the
variable, no compile-time check is done.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Object-Size-Checking.html#index-g_t_005f_005fbuiltin_005fobject_005fsize-3657

help:assume compiler supports __builtin_object_size
"""
		f = '''
/* a() is never defined.  An optimizing compiler will eliminate the
 * attempt to call it, allowing the Link to succeed.  A non-optimizing
 * compiler will emit the call, and the Link will fail.
 */
int a();
static inline int a(char *c){
	return __builtin_object_size(c,0) == 4 ? 1 : %s;
}
'''
		main = '''
	char c[4];
	return a(c);
'''
		if self.Link(context, text=f % 'a()', main=main, msg='whether compiler optimizes __builtin_object_size'):
			context.sconf.Define('DXX_HAVE_BUILTIN_OBJECT_SIZE')
		else:
			self.Compile(context, text=f % '2', main=main, msg='whether compiler accepts __builtin_object_size')
	@_custom_test
	def check_embedded_compound_statement(self,context,
		_compound_statement_native=('', ''),
		_compound_statement_emulated=('[&]', '()')
	):
		"""
Test whether the compiler implements gcc's [statement expression][1]
extension.  If this extension is present, statements can be used where
expressions are expected.  If it is absent, it is emulated by defining,
calling, and then discarding a lambda function.  The compiler produces
better error messages for errors in statement expressions than for
errors in lambdas, so prefer statement expressions when they are
available.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
"""
		f = '''
	return ({ 1 + 2; });
'''
		t = _compound_statement_native if self.Compile(context, text='', main=f, msg='whether compiler understands embedded compound statements') else _compound_statement_emulated
		context.sconf.Define('DXX_BEGIN_COMPOUND_STATEMENT', t[0])
		context.sconf.Define('DXX_END_COMPOUND_STATEMENT', t[1])
		context.sconf.Define('DXX_ALWAYS_ERROR_FUNCTION(F,S)', r'( DXX_BEGIN_COMPOUND_STATEMENT {	\
	void F() __attribute_error(S);	\
	F();	\
} DXX_END_COMPOUND_STATEMENT )', '''
Declare a function named F and immediately call it.  If gcc's
__attribute__((__error__)) is supported, __attribute_error will expand
to use __attribute__((__error__)) with the explanatory string S, causing
it to be a compilation error if this expression is not optimized out.

Use this macro to implement static assertions that depend on values that
are known to the optimizer, but are not considered "compile time
constant expressions" for the purpose of the static_assert intrinsic.

C++11 deleted functions cannot be used here because the compiler raises
an error for the call before the optimizer has an opportunity to delete
the call via a dead code elimination pass.
''')
	@_custom_test
	def check_attribute_always_inline(self,context):
		"""
help:assume compiler supports __attribute__((always_inline))
"""
		macro_name = '__attribute_always_inline()'
		macro_value = '__attribute__((__always_inline__))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test='%s static inline void a(){}' % macro_name, main='a();', msg='for function __attribute__((always_inline))')
	@_custom_test
	def check_attribute_alloc_size(self,context):
		"""
help:assume compiler supports __attribute__((alloc_size))
"""
		macro_name = '__attribute_alloc_size(A,...)'
		macro_value = '__attribute__((alloc_size(A, ## __VA_ARGS__)))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
char*a(int)__attribute_alloc_size(1);
char*b(int,int)__attribute_alloc_size(1,2);
""", msg='for function __attribute__((alloc_size))')
	@_custom_test
	def check_attribute_cold(self,context):
		"""
Test whether the compiler accepts gcc's function attribute
[__attribute__((cold))][1].  Use this to annotate functions which are
rarely called, such as error reporting functions.

[1]: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-g_t_0040code_007bcold_007d-function-attribute-3090

help:assume compiler supports __attribute__((cold))
"""
		macro_name = '__attribute_cold'
		macro_value = '__attribute__((cold))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
__attribute_cold char*a(int);
""", msg='for function __attribute__((cold))')
	@_custom_test
	def check_attribute_format_arg(self,context):
		"""
help:assume compiler supports __attribute__((format_arg))
"""
		macro_name = '__attribute_format_arg(A)'
		macro_value = '__attribute__((format_arg(A)))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
char*a(char*)__attribute_format_arg(1);
""", msg='for function __attribute__((format_arg))')
	@_custom_test
	def check_attribute_format_printf(self,context):
		"""
help:assume compiler supports __attribute__((format(printf)))
"""
		macro_name = '__attribute_format_printf(A,B)'
		macro_value = '__attribute__((format(printf,A,B)))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
int a(char*,...)__attribute_format_printf(1,2);
int b(char*)__attribute_format_printf(1,0);
""", msg='for function __attribute__((format(printf)))')
	@_custom_test
	def check_attribute_malloc(self,context):
		"""
help:assume compiler supports __attribute__((malloc))
"""
		macro_name = '__attribute_malloc()'
		macro_value = '__attribute__((malloc))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
int *a()__attribute_malloc();
""", msg='for function __attribute__((malloc))')
	@_custom_test
	def check_attribute_nonnull(self,context):
		"""
help:assume compiler supports __attribute__((nonnull))
"""
		macro_name = '__attribute_nonnull(...)'
		macro_value = '__attribute__((nonnull __VA_ARGS__))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
int a(int*)__attribute_nonnull();
int b(int*)__attribute_nonnull((1));
""", msg='for function __attribute__((nonnull))')
	@_custom_test
	def check_attribute_noreturn(self,context):
		"""
help:assume compiler supports __attribute__((noreturn))
"""
		macro_name = '__attribute_noreturn'
		macro_value = '__attribute__((noreturn))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test='%s void a();void a(){for(;;);}' % macro_name, main='a();', msg='for function __attribute__((noreturn))')
	@_custom_test
	def check_attribute_used(self,context):
		"""
help:assume compiler supports __attribute__((used))
"""
		macro_name = '__attribute_used'
		macro_value = '__attribute__((used))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
static void a()__attribute_used;
static void a(){}
""", msg='for function __attribute__((used))')
	@_custom_test
	def check_attribute_unused(self,context):
		"""
help:assume compiler supports __attribute__((unused))
"""
		macro_name = '__attribute_unused'
		macro_value = '__attribute__((unused))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
__attribute_unused
static void a(){}
""", msg='for function __attribute__((unused))')
	@_custom_test
	def check_attribute_warn_unused_result(self,context):
		"""
help:assume compiler supports __attribute__((warn_unused_result))
"""
		macro_name = '__attribute_warn_unused_result'
		macro_value = '__attribute__((warn_unused_result))'
		self._check_macro(context,macro_name=macro_name,macro_value=macro_value,test="""
int a()__attribute_warn_unused_result;
int a(){return 0;}
""", msg='for function __attribute__((warn_unused_result))')
	@_custom_test
	def check_attribute_warning(self,context,_check_function_dce_attribute=_check_function_dce_attribute):
		_check_function_dce_attribute(self, context, 'warning')
	def Cxx14Compile(self,context,*args,**kwargs):
		"""
Test whether the compiler supports a C++14 feature.  If the compiler
failed the test for C++14 support, then the test is not run, but a
message about skipping the test is printed and the test is assumed to
fail.
"""
		self.__skip_missing_cxx_std(self._cxx_conformance_cxx14, 'no C++14 support', kwargs)
		return self.Compile(context,*args,**kwargs)
	def __skip_missing_cxx_std(self,level,text,kwargs):
		if self.__cxx_conformance < level:
			kwargs.setdefault('skipped', text)
	@_implicit_test
	def check_boost_array(self,context,**kwargs):
		"""
help:assume Boost.Array works
"""
		return self.Compile(context, msg='for Boost.Array', successflags={'CPPDEFINES' : ['DXX_HAVE_BOOST_ARRAY']}, **kwargs)
	@_implicit_test
	def check_cxx_array(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX_ARRAY']},**kwargs):
		"""
help:assume <array> works
"""
		return self.Compile(context, msg='for <array>', successflags=_successflags, **kwargs)
	@_implicit_test
	def check_cxx_tr1_array(self,context,**kwargs):
		"""
help:assume <tr1/array> works
"""
		return self.Compile(context, msg='for <tr1/array>', successflags={'CPPDEFINES' : ['DXX_HAVE_CXX_TR1_ARRAY']}, **kwargs)
	@_custom_test
	def _check_cxx_array(self,context):
		include = '''
#include "compiler-array.h"
'''
		main = '''
	array<int,2>b;
	b[0]=1;
'''
		how = self.check_cxx_array(context, text=include, main=main) or self.check_boost_array(context, text=include, main=main) or self.check_cxx_tr1_array(context, text=include, main=main)
		if not how:
			raise SCons.Errors.StopError("C++ compiler does not support <array> or Boost.Array or <tr1/array>.")
	def _check_static_assert_method(self,context,msg,f,testflags={},_tdict={'expr' : 'true&&true'},_fdict={'expr' : 'false||false'},**kwargs):
		_Compile = self.Compile
		return _Compile(context, text=f % _tdict, main='f(A());', msg=msg % 'true', testflags=testflags, **kwargs) and \
			_Compile(context, text=f % _fdict, main='f(A());', msg=msg % 'false', expect_failure=True, successflags=testflags, **kwargs)
	@_implicit_test
	def check_boost_static_assert(self,context,f):
		"""
help:assume Boost.StaticAssert works
"""
		return self._check_static_assert_method(context, 'for Boost.StaticAssert when %s', f, testflags={'CPPDEFINES' : ['DXX_HAVE_BOOST_STATIC_ASSERT']})
	@_implicit_test
	def check_c_typedef_static_assert(self,context,f):
		"""
help:assume C typedef-based static assertion works
"""
		return self._check_static_assert_method(context, 'for C typedef static assertion when %s', f, testflags={'CPPDEFINES' : ['DXX_HAVE_C_TYPEDEF_STATIC_ASSERT']})
	@_implicit_test
	def check_cxx11_static_assert(self,context,f):
		"""
help:assume compiler supports C++ intrinsic static_assert
"""
		return self._check_static_assert_method(context, 'for C++11 intrinsic static_assert when %s', f, testflags={'CPPDEFINES' : ['DXX_HAVE_CXX11_STATIC_ASSERT']})
	@_custom_test
	def _check_static_assert(self,context):
		f = '''
#include "compiler-static_assert.h"
static_assert(%(expr)s, "global");
struct A
{
	static const bool value = %(expr)s;
	static_assert(%(expr)s, "class literal");
	static_assert(A::value, "class static");
	A()
	{
		static_assert(%(expr)s, "constructor literal");
		static_assert(value, "constructor static");
	}
};
template <typename>
struct B
{
	static const bool value = %(expr)s;
	static_assert(%(expr)s, "template class literal");
	static_assert(value, "template class static");
	B(A a)
	{
		static_assert(%(expr)s, "constructor literal");
		static_assert(value, "constructor self static");
		static_assert(A::value, "constructor static");
		static_assert(a.value, "constructor member");
	}
	template <typename R>
		B(B<R> &&b)
		{
			static_assert(%(expr)s, "template constructor literal");
			static_assert(value, "template constructor self static");
			static_assert(B<R>::value, "template constructor static");
			static_assert(b.value, "template constructor member");
		}
};
template <typename T>
static void f(B<T> b)
{
	static_assert(%(expr)s, "template function literal");
	static_assert(B<T>::value, "template function static");
	static_assert(b.value, "template function member");
}
void f(A a);
void f(A a)
{
	static_assert(%(expr)s, "function literal");
	static_assert(A::value, "function static");
	static_assert(a.value, "function member");
	f(B<long>(B<int>(a)));
}
'''
		how = self.check_cxx11_static_assert(context,f) or self.check_boost_static_assert(context,f) or self.check_c_typedef_static_assert(context,f)
	@_implicit_test
	def check_boost_type_traits(self,context,f):
		"""
help:assume Boost.TypeTraits works
"""
		return self.Compile(context, text=f, msg='for Boost.TypeTraits', ext='.cpp', successflags={'CPPDEFINES' : ['DXX_HAVE_BOOST_TYPE_TRAITS']})
	@_implicit_test
	def check_cxx11_type_traits(self,context,f,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX11_TYPE_TRAITS']}):
		"""
help:assume <type_traits> works
"""
		return self.Compile(context, text=f, msg='for <type_traits>', ext='.cpp', successflags=_successflags)
	@_custom_test
	def _check_type_traits(self,context):
		f = '''
#define DXX_INHERIT_CONSTRUCTORS	/* bypass sanity check */
#include "compiler-type_traits.h"
typedef tt::conditional<true,int,long>::type a;
typedef tt::conditional<false,int,long>::type b;
'''
		if self.check_cxx11_type_traits(context, f) or self.check_boost_type_traits(context, f):
			return
		raise SCons.Errors.StopError("C++ compiler does not support <type_traits> or Boost.TypeTraits.")
	@_implicit_test
	def check_boost_foreach(self,context,**kwargs):
		"""
help:assume Boost.Foreach works
"""
		return self.Compile(context, msg='for Boost.Foreach', successflags={'CPPDEFINES' : ['DXX_HAVE_BOOST_FOREACH']}, **kwargs)
	@_implicit_test
	def check_cxx11_range_for(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX11_RANGE_FOR']},**kwargs):
		return self.Compile(context, msg='for C++11 range-based for', successflags=_successflags, **kwargs)
	@_custom_test
	def _check_range_based_for(self,context):
		include = '''
#include "compiler-range_for.h"
'''
		main = '''
	int b[2];
	range_for(int&c,b)c=0;
'''
		if not self.check_cxx11_range_for(context, text=include, main=main) and not self.check_boost_foreach(context, text=include, main=main):
			raise SCons.Errors.StopError("C++ compiler does not support range-based for or Boost.Foreach.")
	@_custom_test
	def check_cxx11_required_features(self,context):
		features = self.__cxx11_required_features
		text = ''
		main = ''
		for f in features:
			text += f.text
			main += f.main
		# First test all the features at once.  If all work, then done.
		# If any fail, then the configure run will stop.
		if self.Compile(context, text=text, main=main, msg='for required C++11 features'):
			return
		# Some failed.  Run each test separately and report to the user
		# which ones failed.
		raise SCons.Errors.StopError("C++ compiler does not support %s." %
			', '.join(
				[f.name for f in features if not self.Compile(context, text=f.text, main=f.main, msg='for C++11 %s' % f.name)]
			)
		)
	@_custom_test
	def check_constexpr_union_constructor(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CONSTEXPR_UNION_CONSTRUCTOR']}):
		# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56583
		# <=gcc-4.7.x ICE on constexpr union constructors with anonymous
		# substructure.
		# Works fine without the substructure.
		# Works fine in >=gcc-4.8 regardless of substructure.
		f = '''
union U {
	struct {
		int a;
	};
	constexpr U(int b) :
		a(b)
	{
	}
};
U a{640};
'''
		self.Compile(context, text=f, msg='whether compiler supports constexpr union constructors', successflags=_successflags)
	def _show_pch_count_message(self,context,which,user_setting):
		count = user_setting if user_setting else 0
		context.Display('%s: checking when to pre-compile %s headers...%s\n' % (self.msgprefix, which, ('if used at least %u time%s' % (count, 's' if count > 1 else '')) if count > 0 else 'never'))
		return count > 0
	implicit_tests.append(_implicit_test.RecordedTest('check_pch_compile', "assume C++ compiler can create pre-compiled headers"))
	implicit_tests.append(_implicit_test.RecordedTest('check_pch_use', "assume C++ compiler can use pre-compiled headers"))
	@_custom_test
	def _check_pch(self,context,
		_Test=_Test,
		_testflags_compile_pch={'CXXFLAGS' : ['-x', 'c++-header']},
		_testflags_use_pch={'CXXFLAGS' : ['-Winvalid-pch', '-include', None]}
	):
		self.pch_flags = None
		# Always evaluate both
		co = self._show_pch_count_message(context, 'own', self.user_settings.pch)
		cs = self._show_pch_count_message(context, 'system', self.user_settings.syspch)
		context.did_show_result = True
		if not co and not cs:
			return
		context.Display('%s: checking when to compute pre-compiled header input *pch.cpp...%s\n' % (self.msgprefix, 'if missing' if self.user_settings.pch_cpp_assume_unchanged else 'always'))
		result = _Test(self, context, action=self.PCHAction(context), text='', msg='whether compiler can create pre-compiled headers', testflags=_testflags_compile_pch, calling_function='pch_compile')
		if not result:
			raise SCons.Errors.StopError("C++ compiler cannot create pre-compiled headers.")
		_testflags_use_pch = _testflags_use_pch.copy()
		_testflags_use_pch['CXXFLAGS'][-1] = str(context.lastTarget)[:-4]
		result = self.Compile(context, text='''
/* This symbol is defined in the PCH.  If the PCH is included, this
 * symbol will preprocess away to nothing.  If the PCH is not included,
 * then the compiler is not using PCHs as expected.
 */
dxx_compiler_supports_pch
''', msg='whether compiler uses pre-compiled headers', testflags=_testflags_use_pch, calling_function='pch_use')
		if not result:
			raise SCons.Errors.StopError("C++ compiler cannot use pre-compiled headers.")
		self.pch_flags = _testflags_compile_pch
	@_custom_test
	def _check_cxx11_explicit_delete(self,context):
		# clang 3.4 warns when a named parameter to a deleted function
		# is not used, even though there is no body in which it could be
		# used, so every named parameter to a deleted function is always
		# unused.
		f = 'int a(int %s)=delete;'
		if self.check_cxx11_explicit_delete_named(context, f):
			# No bug: named parameters with explicitly deleted functions
			# work correctly.
			return
		if self.check_cxx11_explicit_delete_named_unused(context, f):
			# Clang bug hit.  Called function adds -Wno-unused-parameter
			# to work around the bug, but affected users will not get
			# warnings about parameters that are unused in regular
			# functions.
			return
		if self.check_cxx11_explicit_delete_anonymous(context, f):
			raise SCons.Errors.StopError("C++ compiler rejects explicitly deleted functions with named parameters, even with -Wno-unused-parameter.")
		raise SCons.Errors.StopError("C++ compiler does not support explicitly deleted functions.")
	@_implicit_test
	def check_cxx11_explicit_delete_named(self,context,f):
		"""
help:assume compiler supports explicitly deleted functions with named parameters
"""
		return self.Compile(context, text=f % 'b', msg='for explicitly deleted functions with named parameters')
	@_implicit_test
	def check_cxx11_explicit_delete_named_unused(self,context,f,_successflags={'CXXFLAGS' : ['-Wno-unused-parameter']}):
		"""
help:assume compiler supports explicitly deleted functions with named parameters with -Wno-unused-parameter
"""
		return self.Compile(context, text=f % 'b', msg='for explicitly deleted functions with named parameters and -Wno-unused-parameter', successflags=_successflags)
	@_implicit_test
	def check_cxx11_explicit_delete_anonymous(self,context,f):
		"""
help:assume compiler supports explicitly deleted functions with anonymous parameters
"""
		return self.Compile(context, text=f % '', msg='for explicitly deleted functions with anonymous parameters')
	@_implicit_test
	def check_cxx11_free_begin(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX11_BEGIN']},**kwargs):
		return self.Compile(context, msg='for C++11 functions begin(), end()', successflags=_successflags, **kwargs)
	@_implicit_test
	def check_boost_free_begin(self,context,**kwargs):
		return self.Compile(context, msg='for Boost.Range functions begin(), end()', successflags={'CPPDEFINES' : ['DXX_HAVE_BOOST_BEGIN']}, **kwargs)
	@_custom_test
	def _check_free_begin_function(self,context):
		f = '''
#include "compiler-begin.h"
struct A {
	typedef int *iterator;
	typedef const int *const_iterator;
	iterator begin(){return 0;}
	iterator end(){return 0;}
	const_iterator begin() const{return 0;}
	const_iterator end() const{return 0;}
};
#define F(C){\
	C int a[1]{0};\
	C A b{};\
	if(begin(a)||end(a)||begin(b)||end(b))return 1;\
}
'''
		main = 'F()F(const)'
		if not self.check_cxx11_free_begin(context, text=f, main=main) and not self.check_boost_free_begin(context, text=f, main=main):
			raise SCons.Errors.StopError("C++ compiler does not support free functions begin() and end().")
	@_implicit_test
	def check_cxx11_addressof(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX11_ADDRESSOF']},**kwargs):
		return self.Compile(context, msg='for C++11 function addressof()', successflags=_successflags, **kwargs)
	@_implicit_test
	def check_boost_addressof(self,context,**kwargs):
		return self.Compile(context, msg='for Boost.Utility function addressof()', successflags={'CPPDEFINES' : ['DXX_HAVE_BOOST_ADDRESSOF']}, **kwargs)
	@_custom_test
	def _check_free_addressof_function(self,context):
		f = '''
#include "compiler-addressof.h"
struct A {
	void operator&();
};
'''
		main = '''
	A b;
	return addressof(b) != 0;
'''
		if not self.check_cxx11_addressof(context, text=f, main=main) and not self.check_boost_addressof(context, text=f, main=main):
			raise SCons.Errors.StopError("C++ compiler does not support free function addressof().")
	@_custom_test
	def check_cxx14_exchange(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX14_EXCHANGE']}):
		f = '''
#include "compiler-exchange.h"
'''
		self.Cxx14Compile(context, text=f, main='return exchange(argc, 5)', msg='for C++14 exchange', successflags=_successflags)
	@_custom_test
	def check_cxx14_integer_sequence(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX14_INTEGER_SEQUENCE']}):
		f = '''
#include <utility>
using std::integer_sequence;
using std::index_sequence;
'''
		self.Cxx14Compile(context, text=f, msg='for C++14 integer_sequence', successflags=_successflags)
	@_custom_test
	def check_cxx14_make_unique(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_CXX14_MAKE_UNIQUE']}):
		f = '''
#include "compiler-make_unique.h"
'''
		main = '''
	make_unique<int>(0);
	make_unique<int[]>(1);
'''
		self.Cxx14Compile(context, text=f, main=main, msg='for C++14 make_unique', successflags=_successflags)
	@_implicit_test
	def check_cxx11_inherit_constructor(self,context,text,_macro_value=_quote_macro_value('''
	typedef B,##__VA_ARGS__ _dxx_constructor_base_type;
	using _dxx_constructor_base_type::_dxx_constructor_base_type;'''),
		**kwargs):
		"""
help:assume compiler supports inheriting constructors
"""
		blacklist_clang_libcxx = '''
/* Test for bug where clang + libc++ + constructor inheritance causes a
 * compilation failure when returning nullptr.
 *
 * Works: gcc
 * Works: clang + gcc libstdc++
 * Works: old clang + old libc++ (cutoff date unknown).
 * Works: new clang + new libc++ + unique_ptr<T>
 * Fails: new clang + new libc++ + unique_ptr<T[]> (v3.6.0 confirmed broken).

memory:2676:32: error: no type named 'type' in 'std::__1::enable_if<false, std::__1::unique_ptr<int [], std::__1::default_delete<int []> >::__nat>'; 'enable_if' cannot be used to disable this declaration
            typename enable_if<__same_or_less_cv_qualified<_Pp, pointer>::value, __nat>::type = __nat()) _NOEXCEPT
                               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.sconf_temp/conftest_43.cpp:26:11: note: in instantiation of member function 'std::__1::unique_ptr<int [], std::__1::default_delete<int []> >::unique_ptr' requested here
        using B::B;
                 ^
.sconf_temp/conftest_43.cpp:30:2: note: while substituting deduced template arguments into function template 'I' [with _Pp = I]
        return nullptr;
        ^
 */
#include <memory>
class I : std::unique_ptr<int[]>
{
public:
	typedef std::unique_ptr<int[]> B;
	using B::B;
};
I a();
I a()
{
	return nullptr;
}
'''
		return _macro_value \
			if self.Compile(context, text=text.format(leading_text=blacklist_clang_libcxx, macro_value=_macro_value), msg='for C++11 inherited constructors with good unique_ptr<T[]> support', **kwargs) \
			else None
	@_implicit_test
	def check_cxx11_variadic_forward_constructor(self,context,text,_macro_value=_quote_macro_value('''
    template <typename... Args>
        D(Args&&... args) :
            B,##__VA_ARGS__(std::forward<Args>(args)...) {}
'''),**kwargs):
		"""
help:assume compiler supports variadic template-based constructor forwarding
"""
		return _macro_value \
			if self.Compile(context, text=text.format(leading_text='#include <algorithm>\n', macro_value=_macro_value), msg='for C++11 variadic templates on constructors', **kwargs) \
			else None
	@_custom_test
	def _check_forward_constructor(self,context,_text='''
{leading_text}
#define DXX_INHERIT_CONSTRUCTORS(D,B,...) {macro_value}
struct A {{
	A(int){{}}
}};
struct B:A {{
DXX_INHERIT_CONSTRUCTORS(B,A);
}};
''',
		_macro_define='DXX_INHERIT_CONSTRUCTORS(D,B,...)',
		_methods=(check_cxx11_inherit_constructor, check_cxx11_variadic_forward_constructor)
):
		for f in _methods:
			macro_value = f(self, context, text=_text, main='B(0)')
			if macro_value:
				context.sconf.Define(_macro_define, macro_value)
				return
		raise SCons.Errors.StopError("C++ compiler does not support constructor forwarding.")
	@_custom_test
	def check_cxx11_ref_qualifier(self,context):
		text = '''
struct A {
	int a()const &{return 1;}
	int a()const &&{return 2;}
};
'''
		main = '''
	A a;
	return a.a() != A().a();
'''
		if self.Compile(context, text=text, main=main, msg='for C++11 reference qualified methods'):
			context.sconf.Define('DXX_HAVE_CXX11_REF_QUALIFIER')
	@_custom_test
	def check_deep_tuple(self,context):
		text = '''
#include <tuple>
static inline std::tuple<{type}> make() {{
	return std::make_tuple({value});
}}
static void a(){{
	std::tuple<{type}> t = make();
	(void)t;
}}
'''
		count = 20
		if self.Compile(context, text=text.format(type=','.join(('int',)*count), value=','.join(('0',)*count)), main='a()', msg='whether compiler handles 20-element tuples'):
			return
		count = 2
		raise SCons.Errors.StopError(
			"Compiler cannot handle tuples of 20 elements.  Raise the template instantiation depth." \
			if self.Compile(context, text=text.format(type=','.join(('int',)*count), value=','.join(('0',)*count)), main='a()', msg='whether compiler handles 2-element tuples') \
			else "Compiler cannot handle tuples of 2 elements."
		)
	@_implicit_test
	def check_poison_valgrind(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_POISON_VALGRIND']}):
		'''
help:add Valgrind annotations; wipe certain freed memory when running under Valgrind
'''
		context.Message('%s: checking %s...' % (self.msgprefix, 'whether to use Valgrind poisoning'))
		r = 'valgrind' in self.user_settings.poison
		context.Result(r)
		if not r:
			return
		text = '''
#include "poison.h"
'''
		main = '''
	DXX_MAKE_MEM_UNDEFINED(&argc, sizeof(argc));
'''
		if self.Compile(context, text=text, main=main, msg='whether Valgrind memcheck header works', successflags=_successflags):
			return True
		raise SCons.Errors.StopError("Valgrind poison requested, but <valgrind/memcheck.h> does not work.")
	@_implicit_test
	def check_poison_overwrite(self,context):
		'''
help:always wipe certain freed memory
'''
		context.Message('%s: checking %s...' % (self.msgprefix, 'whether to use overwrite poisoning'))
		r = 'overwrite' in self.user_settings.poison
		context.Result(r)
		if r:
			context.sconf.Define('DXX_HAVE_POISON_OVERWRITE')
		return r
	@_custom_test
	def _check_poison_method(self,context,
		_methods=(check_poison_valgrind, check_poison_overwrite),
		poison = None
	):
		# Always run both checks.  The user may want a program that
		# always uses overwrite poisoning and, when running under
		# Valgrind, marks the memory as undefined.
		for f in _methods:
			if f(self, context):
				poison = True
		if poison:
			context.sconf.Define('DXX_HAVE_POISON')
	@_custom_test
	def check_strcasecmp_present(self,context,_successflags={'CPPDEFINES' : ['DXX_HAVE_STRCASECMP']}):
		main = '''
	return !strcasecmp(argv[0], argv[0] + 1) && !strncasecmp(argv[0] + 1, argv[0], 1);
'''
		self.Compile(context, text='#include <cstring>', main=main, msg='for strcasecmp', successflags=_successflags)
	__preferred_compiler_options = (
		'-fvisibility=hidden',
		'-Wsuggest-attribute=noreturn',
		'-Wlogical-op',
	)
	__preferred_win32_linker_options = (
		'-Wl,--large-address-aware',
		'-Wl,--dynamicbase',
		'-Wl,--nxcompat',
		'-Wl,--insert-timestamp',
	)
	def __mangle_compiler_option_name(opt):
		return 'check_compiler_option%s' % opt.replace('-', '_').replace('=', '_')
	def __mangle_linker_option_name(opt):
		return 'check_linker_option%s' % opt.replace('-', '_').replace(',', '_')
	@_custom_test
	def check_preferred_compiler_options(self,context,
		ccopts=__preferred_compiler_options,
		ldopts=(),
		_mangle_compiler_option_name=__mangle_compiler_option_name,
		_mangle_linker_option_name=__mangle_linker_option_name
	):
		if self.user_settings.host_platform == 'win32':
			ldopts = self.__preferred_win32_linker_options
		f, desc = (self.Link, 'linker') if ldopts else (self.Compile, 'compiler')
		if f(context, text='', main='', msg='whether %s accepts preferred options' % desc, successflags={'CXXFLAGS' : ccopts, 'LINKFLAGS' : ldopts}, calling_function='preferred_%s_options' % desc):
			# Everything is supported.  Skip individual tests.
			return
		# Compiler+linker together failed.  Check if compiler alone will work.
		# If not ldopts, then next self.Compile is equivalent to previous
		# f(...).
		if not ldopts or not self.Compile(context, text='', main='', msg='whether compiler accepts preferred options', successflags={'CXXFLAGS' : ccopts}):
			# Compiler alone failed.
			# Run down the individual compiler options to find any that work.
			for opt in ccopts:
				self.Compile(context, text='', main='', msg='whether compiler accepts option %s' % opt, successflags={'CXXFLAGS' : (opt,)}, calling_function=_mangle_compiler_option_name(opt)[6:])
		# Run down the individual linker options to find any that work.
		for opt in ldopts:
			self.Link(context, text='', main='', msg='whether linker accepts option %s' % opt, successflags={'LINKFLAGS' : (opt,)}, calling_function=_mangle_linker_option_name(opt)[6:])
	@classmethod
	def register_preferred_compiler_options(cls,
		ccopts = __preferred_compiler_options,
		# Always register target-specific tests on the class.  Individual
		# targets will decide whether to run the tests.
		ldopts = __preferred_win32_linker_options,
		RecordedTest = Collector.RecordedTest,
		record = implicit_tests.append,
		_mangle_compiler_option_name=__mangle_compiler_option_name,
		_mangle_linker_option_name=__mangle_linker_option_name
	):
		del cls.register_preferred_compiler_options
		record(RecordedTest('check_preferred_linker_options', 'assume linker accepts preferred options'))
		mangle = _mangle_compiler_option_name
		for opt in ccopts:
			record(RecordedTest(mangle(opt), 'assume compiler accepts %s' % opt))
		mangle = _mangle_linker_option_name
		for opt in ldopts:
			record(RecordedTest(mangle(opt), 'assume linker accepts %s' % opt))
		assert cls.custom_tests[0].name == cls.check_cxx_works.__name__, cls.custom_tests[0].name
		assert cls.custom_tests[-1].name == cls._restore_cxx_prefix.__name__, cls.custom_tests[-1].name
	# This must be the last custom test.  It does not test the environment,
	# but is responsible for reversing test-environment-specific changes made
	# by check_cxx_works.
	@_custom_test
	def _restore_cxx_prefix(self,context):
		sconf = context.sconf
		sconf.config_h_text = self.__commented_tool_versions + sconf.config_h_text
		context.env['CXXCOM'] = self.__cxx_com_prefix
		context.did_show_result = True

ConfigureTests.register_preferred_compiler_options()

class cached_property(object):
	def __init__(self,f):
		self.method = f
		self.name = f.__name__
	def __get__(self,instance,cls):
		# This should never be accessed directly on the class.
		assert instance is not None
		name = self.name
		d = instance.__dict__
		# After the first access, Python should find the cached value in
		# the instance dictionary instead of calling __get__ again.
		assert name not in d
		d[name] = r = self.method(instance)
		return r

class LazyObjectConstructor(object):
	def __lazy_objects(self,source,
			cache={},
			__strip_extension=lambda _, name, _splitext=os.path.splitext: _splitext(name)[0]
		):
		env = self.env
		# Use id because name needs to be hashable and have a 1-to-1
		# mapping to source.
		name = (id(env), id(source))
		try:
			return cache[name]
		except KeyError as e:
			StaticObject = env.StaticObject
			OBJSUFFIX = env['OBJSUFFIX']
			builddir = self.user_settings.builddir
			value = []
			extend = value.extend
			for s in source:
				if isinstance(s, str):
					transform_target = __strip_extension
					s = (s,)
				else:
					transform_target = s.get('transform_target', __strip_extension)
					s = s['source']
				extend([
					StaticObject(target='%s%s%s' % (builddir, transform_target(self, srcname), OBJSUFFIX), source=srcname) for srcname in s
				])
			# Convert to a tuple so that attempting to modify a cached
			# result raises an error.
			value = tuple(value)
			cache[name] = value
			return value

	@staticmethod
	def create_lazy_object_property(sources,__lazy_objects=__lazy_objects):
		return property(lambda s, _f=__lazy_objects, _sources=sources: _f(s, _sources))

class FilterHelpText:
	_sconf_align = None
	def __init__(self):
		self.visible_arguments = []
	def FormatVariableHelpText(self, env, opt, help, default, actual, aliases):
		if not opt in self.visible_arguments:
			return ''
		if not self._sconf_align:
			self._sconf_align = len(max((s for s in self.visible_arguments if s[:6] == 'sconf_'), key=len))
		l = []
		if default is not None:
			if isinstance(default, str) and not default.isalnum():
				default = '"%s"' % default
			l.append("default: {default}".format(default=default))
		actual = getattr(self, opt, None)
		if actual is not None:
			if isinstance(actual, str) and not actual.isalnum():
				actual = '"%s"' % actual
			l.append("current: {current}".format(current=actual))
		return (" {opt:%u}  {help}{l}\n" % (self._sconf_align if opt[:6] == 'sconf_' else 15)).format(opt=opt, help=help, l=(" [" + "; ".join(l) + "]" if l else ''))

class PCHManager(object):
	class ScannedFile:
		def __init__(self,candidates):
			self.candidates = candidates

	syspch_cpp_filename = None
	ownpch_cpp_filename = None
	syspch_object_node = None
	ownpch_object_node = None
	required_pch_object_node = None

	# Compile on first use, so that non-PCH builds skip the compile
	_re_preproc_match = None
	_re_include_match = None
	_re_singleline_comments_sub = None
	# Source files are tracked at class scope because all builds share
	# the same source tree.
	_cls_scanned_files = None

	# Import required modules when the first PCHManager is created, then
	# remove the hook and use object.__new__ to create PCHManager
	# instances.
	def __new__(cls,*args):
		from re import compile as c
		# Match C preprocessor directives that start with i or e,
		# capture any arguments, and allow no arguments.  This accepts:
		# - #if
		# - #ifdef
		# - #ifndef
		# - #else
		# - #endif
		# - #include
		# - #error
		cls._re_preproc_match = c(r'#\s*([ie]\w+)(?:\s+(.*))?').match
		# Capture the argument in a #include statement, including the
		# angle brackets or quotes.
		#
		# Rebirth currently has no computed includes, so ignore computed
		# includes.
		cls._re_include_match = c(r'(<[^>]+>|"[^"]+")').match
		# Strip a single-line C++ style comment ("// Text") and any
		# preceding whitespace.  The C preprocessor will discard these
		# when compiling the header.  Discarding them from the
		# environment Value will prevent rebuilding pch.cpp when the
		# only change is in such a comment.
		cls._re_singleline_comments_sub = c(r'\s*//.*').sub
		# dict with key=filename with path, value=ScannedFile
		cls._cls_scanned_files = {}
		from tempfile import mkstemp
		cls._tempfile_mkstemp = staticmethod(mkstemp)
		del cls.__new__
		return cls.__new__(cls,*args)

	def __init__(self,user_settings,env,pch_subdir,configure_pch_flags,common_pch_manager):
		assert user_settings.syspch or user_settings.pch
		self.user_settings = user_settings
		self.env = env
		# dict with key=fs.File, value=ScannedFile
		self._instance_scanned_files = {}
		self._common_pch_manager = common_pch_manager
		syspch_cpp_filename = ownpch_cpp_filename = None
		syspch_object_node = None
		CXXFLAGS = env['CXXFLAGS'] + configure_pch_flags['CXXFLAGS']
		File = env.File
		if user_settings.syspch:
			self.syspch_cpp_filename = syspch_cpp_filename = os.path.join(user_settings.builddir, pch_subdir, 'syspch.cpp')
			self.syspch_cpp_node = File(syspch_cpp_filename)
			self.required_pch_object_node = self.syspch_object_node = syspch_object_node = env.StaticObject(target='%s.gch' % syspch_cpp_filename, source=self.syspch_cpp_node, CXXCOM=env._dxx_cxxcom_no_ccache_prefix, CXXFLAGS=CXXFLAGS)
		if user_settings.pch:
			self.ownpch_cpp_filename = ownpch_cpp_filename = os.path.join(user_settings.builddir, pch_subdir, 'ownpch.cpp')
			self.ownpch_cpp_node = File(ownpch_cpp_filename)
			if syspch_object_node:
				CXXFLAGS += ['-include', syspch_cpp_filename, '-Winvalid-pch']
			self.required_pch_object_node = self.ownpch_object_node = ownpch_object_node = env.StaticObject(target='%s.gch' % ownpch_cpp_filename, source=self.ownpch_cpp_node, CXXCOM=env._dxx_cxxcom_no_ccache_prefix, CXXFLAGS=CXXFLAGS)
			env.Depends(ownpch_object_node, File(os.path.join(self.user_settings.builddir, 'dxxsconf.h')))
			if syspch_object_node:
				env.Depends(ownpch_object_node, syspch_object_node)
		self.pch_CXXFLAGS = ['-include', ownpch_cpp_filename or syspch_cpp_filename, '-Winvalid-pch']
		# If assume unchanged and the file exists, set __files_included
		# to a dummy value.  This bypasses scanning source files and
		# guarantees that the text of pch.cpp is not changed.  SCons
		# will still recompile pch.cpp into a new .gch file if pch.cpp
		# includes files that SCons recognizes as changed.
		if user_settings.pch_cpp_assume_unchanged and \
			(not syspch_cpp_filename or os.path.exists(syspch_cpp_filename)) and \
			(not ownpch_cpp_filename or os.path.exists(ownpch_cpp_filename)):
			self.__files_included = True
		else:
			# collections.defaultdict with key from ScannedFile.candidates,
			# value is a collections.Counter with key=tuple of preprocessor
			# guards, value=count of times this header was included under
			# that set of guards.
			self.__files_included = defaultdict(collections_counter)
		self.__env_Program = env.Program
		self.__env_StaticObject = env.StaticObject
		env.Program = self.Program
		env.StaticObject = self.StaticObject

	def record_file(self,env,source_file):
		# Every scanned file goes into self._cls_scanned_files to
		# prevent duplicate scanning from multiple targets.
		f = self._cls_scanned_files.get(source_file, None)
		if f is None:
			self._cls_scanned_files[source_file] = f = self.scan_file(env, source_file)
		self._instance_scanned_files[source_file] = f
		return f

	# Scan a file for preprocessor directives related to conditional
	# compilation and file inclusion.
	#
	# The #include directives found are eventually written to pch.cpp
	# with their original preprocessor guards in place.  Since the
	# preprocessor guards are kept and the C preprocessor will evaluate
	# them when compiling the header, this scanner does not attempt to
	# track which branches are true and which are false.
	#
	# This scanner makes no attempt to normalize guard conditions.  It
	# considers each of these examples to be distinct guards, even
	# though a full preprocessor will produce the same result for each:
	#
	#	#if 1
	#	#if 2
	#	#if 3 < 5
	#
	# or:
	#
	#	#ifdef A
	#	#if defined(A)
	#	#if (defined(A))
	#	#if !!defined(A)
	#
	# or:
	#
	# 	#ifndef A
	# 	#else
	# 	#endif
	#
	# 	#ifdef A
	# 	#endif
	#
	# Include directives are followed only if the calling file will not
	# be part of the output pch.cpp.  When the calling file will be in
	# pch.cpp, then the C preprocessor will include the called file, so
	# there is no need to scan it for other headers to include.
	#
	# This scanner completely ignores pragmas, #define, #undef, and
	# computed includes.
	@classmethod
	def scan_file(cls,env,source_filenode):
		match_preproc_directive = cls._re_preproc_match
		match_include_directive = cls._re_include_match
		preceding_line = None
		lines_since_preprocessor = None
		# defaultdict with key=name of header to include, value=set of
		# preprocessor guards under which an include was seen.  Set is
		# used because duplicate inclusions from a single source file
		# should not adjust the usage counter.
		candidates = defaultdict(set)
		# List of currently active preprocessor guards
		guard = []
		header_search_path = None
		for line in map(str.strip, source_filenode.get_contents().splitlines()):
			if preceding_line is not None:
				# Basic support for line continuation.
				line = '%s %s' % (preceding_line[:-1], line)
				preceding_line = None
			elif not line.startswith('#'):
				# Allow unlimited non-preprocessor lines before the
				# first preprocessor line.  Once one preprocessor line
				# is found, track how many lines since the most recent
				# preprocessor line was seen.  If too many
				# non-preprocessor lines appear in a row, assume the
				# scanner is now down in source text and move to the
				# next file.
				if lines_since_preprocessor is not None:
					lines_since_preprocessor += 1
					if lines_since_preprocessor > 50:
						break
				continue
			lines_since_preprocessor = 0
			# Joined lines are rare.  Ignore complicated quoting.
			if line[-1] == '\\':
				preceding_line = line
				continue
			m = match_preproc_directive(line)
			if not m:
				# Not a preprocessor directive or not a directive that
				# this scanner handles.
				continue
			directive = m.group(1)
			if directive == 'include':
				m = match_include_directive(m.group(2))
				if not m:
					# This directive is probably a computed include.
					continue
				name = m.group(1)
				bare_name = name[1:-1]
				if name[0] == '"':
					# Canonicalize paths to non-system headers
					if name == '"dxxsconf.h"':
						# Ignore generated header here.  PCH generation
						# will insert it in the right order.
						continue
					if header_search_path is None:
						header_search_path = [
							d for d in ([os.path.dirname(str(source_filenode))] + env['CPPPATH'])
							# Filter out SDL paths
							if not d.startswith('/')
						]
					name = None
					for d in header_search_path:
						effective_name = os.path.join(d, bare_name)
						if os.path.exists(effective_name):
							name = effective_name
							break
					if name is None:
						# name is None if:
						# - A system header was included using quotes.
						#
						# - A game-specific header was included in a
						# shared file.  A game-specific preprocessor
						# guard will prevent the preprocessor from
						# including the file, but the PCH scan logic
						# looks inside branches that the C preprocessor
						# will evaluate as false.
						continue
					name = env.File(name)
					name.__filename = '"%s"' % effective_name
				candidates[name].add(tuple(guard))
			elif directive == 'endif':
				# guard should always be True here, but test to avoid
				# ugly errors if scanning an ill-formed source file.
				if guard:
					guard.pop()
			elif directive == 'else':
				# #else is handled separately because it has no
				# arguments
				guard.append('#%s' % directive)
			elif directive in (
				'elif',
				'if',
				'ifdef',
				'ifndef',
			):
				guard.append('#%s %s' % (directive, m.group(2)))
			elif directive not in ('error',):
				raise SCons.Errors.StopError("Scanning %s found unhandled C preprocessor directive %r" % (str(source_filenode), directive))
		return cls.ScannedFile(candidates)

	def _compute_pch_text(self):
		self._compute_indirect_includes()
		own_header_inclusion_threshold = self.user_settings.pch
		sys_header_inclusion_threshold = self.user_settings.syspch
		configured_threshold = 0x7fffffff if self.user_settings.pch_cpp_exact_counts else None
		# defaultdict with key=name of tuple of active preprocessor
		# guards, value=tuple of (included file, count of times file was
		# seen with this set of guards, count of times file would be
		# included with this set of guards defined).
		syscpp_includes = defaultdict(list)
		owncpp_includes = defaultdict(list) if own_header_inclusion_threshold else None
		for included_file, usage_dict in self.__files_included.iteritems():
			if isinstance(included_file, str):
				# System header
				cpp_includes = syscpp_includes
				name = included_file
				threshold = own_header_inclusion_threshold if sys_header_inclusion_threshold is None else sys_header_inclusion_threshold
			else:
				# Own header
				cpp_includes = owncpp_includes
				name = included_file.__filename
				threshold = own_header_inclusion_threshold
			if not threshold:
				continue
			count_threshold = configured_threshold or threshold
			g = usage_dict.get((), 0)
			# As a special case, if this header is included
			# without any preprocessor guards, ignore all the
			# conditional includes.
			guards = \
				[((), g)] if (g >= threshold) else \
				sorted(usage_dict.iteritems(), reverse=True)
			while guards:
				preprocessor_guard_directives, local_count_seen = guards.pop()
				total_count_seen = local_count_seen
				if total_count_seen < count_threshold:
					# If not eligible on its own, add in the count from
					# preprocessor guards that are always true when this
					# set of guards is true.  Since the scanner does not
					# normalize preprocessor directives, this is a
					# conservative count of always-true guards.
					g = preprocessor_guard_directives
					while g and total_count_seen < count_threshold:
						g = g[:-1]
						total_count_seen += usage_dict.get(g, 0)
					if total_count_seen < threshold:
						# If still not eligible, skip.
						continue
				cpp_includes[preprocessor_guard_directives].append((name, local_count_seen, total_count_seen))

		if syscpp_includes:
			self.__generated_syspch_lines = self._compute_pch_generated_lines(syscpp_includes)
		if owncpp_includes:
			self.__generated_ownpch_lines = self._compute_pch_generated_lines(owncpp_includes)

	def _compute_pch_generated_lines(self,cpp_includes):
		generated_pch_lines = []
		# Append guarded #include directives for files which passed the
		# usage threshold above.  This loop could try to combine related
		# preprocessor guards, but:
		# - The C preprocessor handles the noncombined guards correctly.
		# - As a native program optimized for text processing, the C
		# preprocessor almost certainly handles guard checking faster
		# than this script could handle guard merging.
		# - This loop runs whenever pch.cpp might be regenerated, even
		# if the result eventually shows that pch.cpp has not changed.
		# The C preprocessor will only run over the file when it is
		# actually changed and is processed to build a new .gch file.
		for preprocessor_guard_directives, included_file_tuples in sorted(cpp_includes.iteritems()):
			generated_pch_lines.append('')
			generated_pch_lines.extend(preprocessor_guard_directives)
			# local_count_seen is the direct usage count for this
			# combination of preprocessor_guard_directives.
			#
			# total_count_seen is the sum of local_count_seen and all
			# guards that are a superset of this
			# preprocessor_guard_directives.  The total stops counting
			# when it reaches threshold, so it may undercount actual
			# usage.
			for (name, local_count_seen, total_count_seen) in sorted(included_file_tuples):
				generated_pch_lines.append('#include %s\t// %u %u' % (name, local_count_seen, total_count_seen))
			# d[2] == l if d is '#else' or d is '#elif'
			# Only generate #endif when d is a '#if*' directive, since
			# '#else/#elif' do not introduce a new scope.
			generated_pch_lines.extend('#endif' for d in preprocessor_guard_directives if d[2] != 'l')
		return generated_pch_lines

	def _compute_indirect_includes(self):
		own_header_inclusion_threshold = None if self.user_settings.pch_cpp_exact_counts else self.user_settings.pch
		# Count how many times each header is used for each preprocessor
		# guard combination.  After the outer loop finishes,
		# files_included is a dictionary that maps the name of the
		# include file to a collections.counter instance.  The mapped
		# counter maps a preprocessor guard to a count of how many times
		# it was used.
		#
		# Given:
		#
		# a.cpp
		# #include "a.h"
		# #ifdef A
		# #include "b.h"
		# #endif
		#
		# b.cpp
		# #include "b.h"
		#
		# files_included = {
		#	'a.h' : { () : 1 }
		#	'b.h' : {
		#		('#ifdef A',) : 1,	# From a.cpp
		#		() : 1,	# From b.cpp
		#	}
		# }
		files_included = self.__files_included
		for scanned_file in self._instance_scanned_files.itervalues():
			for included_file, guards in scanned_file.candidates.iteritems():
				i = files_included[included_file]
				for g in guards:
					i[g] += 1
		# If own_header_inclusion_threshold == 1, then every header
		# found will be listed in pch.cpp, so any indirect headers will
		# be included by the C preprocessor.
		if own_header_inclusion_threshold == 1:
			return
		# For each include file which is below the threshold, scan it
		# for includes which may end up above the threshold.
		File = self.env.File
		includes_to_check = sorted(files_included.iterkeys(), key=str)
		while includes_to_check:
			included_file = includes_to_check.pop()
			if isinstance(included_file, str):
				# System headers are str.  Internal headers are
				# fs.File.
				continue
			guards = files_included[included_file]
			unconditional_use_count = guards.get((), 0)
			if unconditional_use_count >= own_header_inclusion_threshold and own_header_inclusion_threshold:
				# Header will be unconditionally included in the PCH.
				continue
			f = self.record_file(self.env, File(included_file))
			for nested_included_file, nested_guards in sorted(f.candidates.iteritems(), key=str):
				if not isinstance(included_file, str) and not nested_included_file in files_included:
					# If the header is a system header, it will be
					# str.  Skip system headers.
					#
					# Otherwise, if it has not been seen before,
					# append it to includes_to_check for recursive
					# scanning.
					includes_to_check.append(nested_included_file)
				i = files_included[nested_included_file]
				# If the nested header is included
				# unconditionally, skip the generator.
				for g in (nested_guards if unconditional_use_count else (a + b for a in guards for b in nested_guards)):
					i[g] += 1

	@classmethod
	def write_pch_inclusion_file(cls,target,source,env):
		target = str(target[0])
		fd, path = cls._tempfile_mkstemp(suffix='', prefix='%s.' % os.path.basename(target), dir=os.path.dirname(target), text=True)
		# source[0].get_contents() returns the comment-stripped form
		os.write(fd, source[0].__generated_pch_text)
		os.close(fd)
		os.rename(path, target)

	def StaticObject(self,target,source,CXXFLAGS=None,*args,**kwargs):
		env = self.env
		source = env.File(source)
		o = self.__env_StaticObject(target=target, source=source, CXXFLAGS=self.pch_CXXFLAGS + (env['CXXFLAGS'] if CXXFLAGS is None else CXXFLAGS), *args, **kwargs)
		# Force an order dependency on the .gch file.  It is never
		# referenced by the command line or the source files, so SCons
		# may not recognize it as an input.
		env.Requires(o, self.required_pch_object_node)
		if self.__files_included is not True:
			self.record_file(env, source)
		return o

	def Program(self,*args,**kwargs):
		self._register_pch_commands()
		if self._common_pch_manager:
			self._common_pch_manager._register_pch_commands()
		return self.__env_Program(*args, **kwargs)

	def _register_pch_commands(self):
		if self.__files_included:
			# Common code calls this function once for each game which
			# uses it.  Only one call is necessary for proper operation.
			# Ignore all later calls.
			return
		self._compute_pch_text()
		syspch_lines = self.__generated_syspch_lines
		pch_begin_banner_template = '''
// BEGIN PCH GENERATED FILE
// %r
// Threshold=%u
'''
		pch_end_banner = ('''
// END PCH GENERATED FILE
''',)
		if self.syspch_object_node:
			self._register_write_pch_inclusion(self.syspch_cpp_node,
				(
					(pch_begin_banner_template % (self.syspch_cpp_filename, self.user_settings.syspch),),
					syspch_lines,
					pch_end_banner,
				)
			)
			# ownpch.cpp will include syspch.cpp.gch from the command
			# line, so clear syspch_lines to avoid repeating system
			# includes in ownpch.cpp
			syspch_lines = ()
		if self.ownpch_object_node:
			self._register_write_pch_inclusion(self.ownpch_cpp_node,
				(
					(pch_begin_banner_template % (self.ownpch_cpp_filename, self.user_settings.pch),),
					('// System headers' if syspch_lines else '',),
					syspch_lines,
					('''
// SConf generated header
#include "dxxsconf.h"

// Own headers
''',),
					self.__generated_ownpch_lines,
					pch_end_banner,
				)
			)

	def _register_write_pch_inclusion(self,node,lineseq):
		# The contents of pch.cpp are taken from the iterables in
		# lineseq.  Set the contents as an input Value instead of
		# listing file nodes, so that pch.cpp is not rebuilt if a change
		# to a source file does not change what headers are listed in
		# pch.cpp.
		#
		# Strip C++ single line comments so that changes in comments are
		# ignored when deciding whether pch.cpp needs to be rebuilt.
		env = self.env
		text = '\n'.join(itertools.chain.from_iterable(lineseq))
		v = env.Value(self._re_singleline_comments_sub('', text))
		v.__generated_pch_text = text
		# Use env.Command instead of env.Textfile or env.Substfile since
		# the latter use their input both for freshness and as a data
		# source.  This Command writes the commented form of the Value
		# to the file, but uses a comment-stripped form for SCons
		# freshness checking.
		env.Command(node, v, self.write_pch_inclusion_file)

class DXXCommon(LazyObjectConstructor):
	pch_manager = None
	__endian = checkEndian()
	@cached_property
	def program_message_prefix(self):
		return '%s.%d' % (self.PROGRAM_NAME, self.program_instance)
	# Settings which affect how the files are compiled
	class UserBuildSettings:
		class IntVariable(object):
			def __new__(cls,key,help,default):
				return (key, help, default, cls._validator, int)
			@staticmethod
			def _validator(key, value, env):
				try:
					int(value)
					return True
				except ValueError:
					raise SCons.Errors.UserError('Invalid value for integer-only option %s: %s.' % (key, value))
		class UIntVariable(IntVariable):
			@staticmethod
			def _validator(key, value, env):
				try:
					if int(value) < 0:
						raise ValueError
					return True
				except ValueError:
					raise SCons.Errors.UserError('Invalid value for unsigned-integer-only option %s: %s.' % (key, value))
		# Paths for the Videocore libs/includes on the Raspberry Pi
		RPI_DEFAULT_VC_PATH='/opt/vc'
		default_OGLES_LIB = 'GLES_CM'
		_default_prefix = '/usr/local'
		__stdout_is_not_a_tty = None
		__has_git_dir = None
		def default_builddir(self):
			builddir_prefix = self.builddir_prefix
			builddir_suffix = self.builddir_suffix
			default_builddir = builddir_prefix or ''
			if builddir_prefix is not None or builddir_suffix is not None:
				fields = [
					self.host_platform,
					os.path.basename(self.CXX) if self.CXX else None,
				]
				compiler_flags = '\n'.join((getattr(self, attr) or '').strip() for attr in ['CPPFLAGS', 'CXXFLAGS'])
				if compiler_flags:
					# Mix in CRC of CXXFLAGS to get reasonable uniqueness
					# when flags are changed.  A full hash is
					# unnecessary here.
					crc = binascii.crc32(compiler_flags)
					if crc < 0:
						crc = crc + 0x100000000
					fields.append('{:08x}'.format(crc))
				if self.pch:
					fields.append('p%u' % self.pch)
				elif self.syspch:
					fields.append('sp%u' % self.syspch)
				fields.append(''.join(a[1] if getattr(self, a[0]) else (a[2] if len(a) > 2 else '')
				for a in (
					('debug', 'dbg'),
					('lto', 'lto'),
					('editor', 'ed'),
					('opengl', 'ogl', 'sdl'),
					('opengles', 'es'),
					('raspberrypi', 'rpi'),
				)))
				default_builddir += '-'.join([f for f in fields if f])
				if builddir_suffix is not None:
					default_builddir += builddir_prefix
			return default_builddir
		def default_memdebug(self):
			return self.debug
		# automatic setup for raspberrypi
		def default_opengles(self):
			if self.raspberrypi:
				return True
			return False
		@classmethod
		def default_verbosebuild(cls):
			# Enable verbosebuild when the output is not directed to a
			# terminal.  When output is not a terminal, it is likely
			# either a pager or a log file, both of which can readily
			# handle the very long lines generated by verbose mode.
			r = cls.__stdout_is_not_a_tty
			if r is None:
				isatty = getattr(sys.stdout, 'isatty', None)
				# If isatty is None, then assume output is a TTY.
				cls.__stdout_is_not_a_tty = r = False if isatty is None else not isatty()
			return r
		def selected_OGLES_LIB(self):
			if self.raspberrypi:
				return 'GLESv2'
			return self.default_OGLES_LIB
		def __default_DATA_DIR(self):
			return '%s/share/games/%s' % (self.prefix, self._program.target)
		def _generic_variable(key,help,default):
			return (key, help, default)
		@staticmethod
		def __get_configure_tests(tests):
			# Construct combined list on first use, then cache it
			# forever.
			try:
				return tests.__configure_tests
			except AttributeError:
				tests.__configure_tests = c = tests.implicit_tests + tests.custom_tests
				return c
		@classmethod
		def __get_has_git_dir(cls):
			r = cls.__has_git_dir
			if r is None:
				# SConstruct is always at the top of the repository.
				# The user might have run `scons` from elsewhere and
				# used `-f` to indicate this file, but a false negative
				# is acceptable here.
				cls.__has_git_dir = r = os.path.exists(os.environ.get('GIT_DIR', '.git'))
			return r
		def _options(self,
				generic_variable=_generic_variable,
				BoolVariable=BoolVariable,
				EnumVariable=EnumVariable,
				conftests=ConfigureTests,
				getenv=os.environ.get
			):
			tests = self.__get_configure_tests(conftests)
			return (
			{
				'variable': EnumVariable,
				'arguments': [
					('expect_sconf_%s' % t.name[6:],
						None,
						None,
						{'allowed_values' : ['0', '1', conftests.expect_sconf_success, conftests.expect_sconf_failure]}
					) for t in tests if t.name[0] != '_'
				],
			},
			{
				'variable': EnumVariable,
				'arguments': [
					('sconf_%s' % t.name[6:],
						None,
						t.desc or ('assume result of %s' % t.name),
						{'allowed_values' : ['0', '1', '2', conftests.sconf_force_failure, conftests.sconf_force_success, conftests.sconf_assume_success]}
					) for t in tests if t.name[0] != '_'
				],
			},
			{
				'variable': BoolVariable,
				'arguments': (
					('record_sconf_results', False, 'write sconf results to dxxsconf.h'),
					('raspberrypi', False, 'build for Raspberry Pi (automatically sets opengles and opengles_lib)'),
					('git_describe_version', self.__get_has_git_dir(), 'include git --describe in extra_version'),
					('git_status', True, 'include git status'),
					('versid_depend_all', False, 'rebuild vers_id.cpp if any object file changes'),
				),
			},
			{
				'variable': generic_variable,
				'arguments': (
					('rpi_vc_path', self.RPI_DEFAULT_VC_PATH, 'directory for RPi VideoCore libraries'),
					('opengles_lib', self.selected_OGLES_LIB, 'name of the OpenGL ES library to link against'),
					('prefix', self._default_prefix, 'installation prefix directory (Linux only)'),
					('sharepath', self.__default_DATA_DIR, 'directory for shared game data (Linux only)'),
				),
			},
			{
				'variable': self.UIntVariable,
				'arguments': (
					('lto', 0, 'enable gcc link time optimization'),
					('pch', None, 'pre-compile own headers used at least this many times'),
					('syspch', None, 'pre-compile system headers used at least this many times'),
					('max_joysticks', 8, 'maximum number of usable joysticks'),
					('max_axes_per_joystick', 128, 'maximum number of axes per joystick'),
					('max_buttons_per_joystick', 128, 'maximum number of buttons per joystick'),
					('max_hats_per_joystick', 4, 'maximum number of hats per joystick'),
				),
			},
			{
				'variable': BoolVariable,
				'arguments': (
					('pch_cpp_assume_unchanged', False, 'assume text of *pch.cpp is unchanged'),
					('pch_cpp_exact_counts', False, None),
					('check_header_includes', False, 'compile test each header (developer option)'),
					('debug', False, 'build DEBUG binary which includes asserts, debugging output, cheats and more output'),
					('memdebug', self.default_memdebug, 'build with malloc tracking'),
					('opengl', True, 'build with OpenGL support'),
					('opengles', self.default_opengles, 'build with OpenGL ES support'),
					('editor', False, 'include editor into build (!EXPERIMENTAL!)'),
					('sdl2', False, 'use libSDL2+SDL2_mixer (!EXPERIMENTAL!)'),
					('sdlmixer', True, 'build with SDL_Mixer support for sound and music (includes external music support)'),
					('ipv6', False, 'enable UDP/IPv6 for multiplayer'),
					('use_udp', True, 'enable UDP support'),
					('use_tracker', True, 'enable Tracker support (requires UDP)'),
					('verbosebuild', self.default_verbosebuild, 'print out all compiler/linker messages during building'),
					('register_compile_target', True, 'report compile targets to SCons core'),
					('register_cpp_output_targets', None, None),
					# This is intentionally undocumented.  If a bug
					# report includes a log with this set to False, the
					# reporter will be asked to provide a log with the
					# value set to True.  Try to prevent the extra round
					# trip by hiding the option.
					('show_tool_version', True, None)
				),
			},
			{
				'variable': generic_variable,
				'arguments': (
					('CHOST', getenv('CHOST'), 'CHOST of output'),
					('CXX', getenv('CXX'), 'C++ compiler command'),
					('PKG_CONFIG', getenv('PKG_CONFIG'), 'PKG_CONFIG to run (Linux only)'),
					('RC', getenv('RC'), 'Windows resource compiler command'),
					('extra_version', None, 'text to append to version, such as VCS identity'),
					('ccache', None, 'path to ccache'),
					('distcc', None, 'path to distcc'),
					('distcc_hosts', getenv('DISTCC_HOSTS'), 'hosts to distribute compilation'),
				),
			},
			{
				'variable': generic_variable,
				'stack': ' ',
				'arguments': (
					('CPPFLAGS', getenv('CPPFLAGS'), 'C preprocessor flags'),
					('CXXFLAGS', getenv('CXXFLAGS'), 'C++ compiler flags'),
					('LDFLAGS', getenv('LDFLAGS'), 'Linker flags'),
					('LIBS', getenv('LIBS'), 'Libraries to link'),
				),
			},
			{
				'variable': EnumVariable,
				'arguments': (
					('host_platform', 'linux' if sys.platform == 'linux2' else sys.platform, 'cross-compile to specified platform', {'allowed_values' : ['win32', 'darwin', 'linux']}),
				),
			},
			{
				'variable': ListVariable,
				'arguments': (
					('poison', 'none', 'method for poisoning free memory', {'names' : ('valgrind', 'overwrite')}),
				),
			},
			{
				'variable': generic_variable,
				'arguments': (
					('builddir_prefix', None, 'prefix to generated build directory'),
					('builddir_suffix', None, 'suffix to generated build directory'),
					# This must be last so that default_builddir will
					# have access to other properties.
					('builddir', self.default_builddir, 'build in specified directory'),
				),
			},
		)
		_generic_variable = staticmethod(_generic_variable)
		@staticmethod
		def _names(name,prefix):
			return ['%s%s%s' % (p, '_' if p else '', name) for p in prefix]
		def __init__(self,program=None):
			self._program = program
		def register_variables(self,prefix,variables):
			self.known_variables = []
			append_known_variable = self.known_variables.append
			add_variable = variables.Add
			for grp in self._options():
				variable = grp['variable']
				stack = grp.get('stack', None)
				for opt in grp['arguments']:
					(name,value,help) = opt[0:3]
					kwargs = opt[3] if len(opt) > 3 else {}
					if name not in variables.keys():
						if help is not None:
							filtered_help.visible_arguments.append(name)
						add_variable(variable(key=name, help=help, default=None if callable(value) else value, **kwargs))
					names = self._names(name, prefix)
					for n in names:
						if n not in variables.keys():
							add_variable(variable(key=n, help=help, default=None, **kwargs))
					if not name in names:
						names.append(name)
					append_known_variable((names, name, value, stack))
					if stack:
						for n in names:
							add_variable(self._generic_variable(key='%s_stop' % n, help=None, default=None))
		def read_variables(self,variables,d):
			for (namelist,cname,dvalue,stack) in self.known_variables:
				value = None
				found_value = False
				for n in namelist:
					try:
						v = d[n]
						found_value = True
						if stack:
							if callable(v):
								value = v(dvalue=dvalue, value=value, stack=stack)
							else:
								if value:
									value = stack.join([value, v])
								else:
									value = v
							if d.get('%s_stop' % n, None):
								break
							continue
						value = v
						break
					except KeyError as e:
						pass
				if not found_value:
					value = dvalue
				if callable(value):
					value = value()
				setattr(self, cname, value)
			if self.builddir != '' and self.builddir[-1:] != '/':
				self.builddir += '/'
		def clone(self):
			clone = DXXCommon.UserBuildSettings(None)
			for grp in clone._options():
				for o in grp['arguments']:
					name = o[0]
					value = getattr(self, name)
					setattr(clone, name, value)
			return clone
	class UserInstallSettings:
		def _options(self):
			return (
			{
				'variable': self._generic_variable,
				'arguments': (
					('DESTDIR', None, 'installation stage directory'),
					('program_name', None, 'name of built program'),
				),
			},
			{
				'variable': BoolVariable,
				'arguments': (
					('register_install_target', True, 'report install target to SCons core'),
				),
			},
		)
	class UserSettings(UserBuildSettings,UserInstallSettings):
		def _options(self):
			return DXXCommon.UserBuildSettings._options(self) + DXXCommon.UserInstallSettings._options(self)
	# Base class for platform-specific settings processing
	class _PlatformSettings:
		tools = ('g++', 'gnulink')
		ogllibs = ''
		platform_objects = []
		def __init__(self,program,user_settings):
			self.__program = program
			self.user_settings = user_settings
		@property
		def env(self):
			return self.__program.env
	# Settings to apply to mingw32 builds
	class Win32PlatformSettings(_PlatformSettings):
		ogllibs = ('opengl32',)
		tools = ('mingw',)
		def adjust_environment(self,program,env):
			env.Append(CPPDEFINES = ['_WIN32', 'WIN32_LEAN_AND_MEAN'])
	class DarwinPlatformSettings(_PlatformSettings):
		def adjust_environment(self,program,env):
			library_frameworks = os.path.join(os.getenv("HOME"), 'Library/Frameworks')
			env.Append(
				CPPDEFINES = ['HAVE_STRUCT_TIMESPEC', '__unix__'],
				CPPPATH = [os.path.join(library_frameworks, 'SDL.framework/Headers'), '/Library/Frameworks/SDL.framework/Headers'],
				FRAMEWORKS = ['ApplicationServices', 'Cocoa', 'SDL'],
				FRAMEWORKPATH = [library_frameworks, '/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks'],
			)
			if self.user_settings.opengl or self.user_settings.opengles:
				env.Append(FRAMEWORKS = ['OpenGL'])
	# Darwin targets include Objective-C (not Objective-C++) code to
	# access Apple-specific functionality.  Add 'gcc' to the target
	# list to support this.
	DarwinPlatformSettings.tools += ('gcc',)
	# Settings to apply to Linux builds
	class LinuxPlatformSettings(_PlatformSettings):
		@property
		def ogllibs(self):
			user_settings = self.user_settings
			return (user_settings.opengles_lib, 'EGL') if user_settings.opengles else ('GL', 'GLU')
		def adjust_environment(self,program,env):
			env.Append(
				CPPDEFINES = ['HAVE_STRUCT_TIMESPEC'],
				CXXFLAGS = ['-pthread'],
			)

	def __init__(self,__program_instance=itertools.count(1)):
		self.program_instance = next(__program_instance)

	def create_header_targets(self,__shared_header_file_list=[],__shared_cpp_dict={}):
		fs = SCons.Node.FS.get_default_fs()
		builddir = self.user_settings.builddir
		env = self.env
		try:
			check_header_includes = __shared_cpp_dict[builddir]
		except KeyError:
			check_header_includes = env.File(os.path.join(builddir, 'check_header_includes.cpp'))
			# Generate the list once, on first use.  Any other targets
			# will reuse it.
			#
			# Touch the file into existence.  It is always empty, but
			# must exist and have an extension of '.cpp'.
			check_header_includes = env.Textfile(target=check_header_includes, source=env.Value('''
/* This file is always empty.  It is only present to act as the source
 * file for SCons targets that test individual headers.
 */
'''))
			__shared_cpp_dict[builddir] = check_header_includes
			check_header_includes[0].get_found_includes = \
				lambda env, scanner, path, __get_found_includes=check_header_includes[0].get_found_includes: \
					__get_found_includes(env, scanner, path) + [fs.File(env['DXX_EFFECTIVE_SOURCE'])]
		if not __shared_header_file_list:
			headers = Git.pcall(['ls-files', '-z', '--', '*.h']).out
			if not headers:
				g = Git.pcall(['--version'], stderr=subprocess.STDOUT)
				if g.returncode:
					raise SCons.Errors.StopError("`git ls-files` failed.  `git --version` failed.  Check that Git is installed.")
				raise SCons.Errors.StopError("`git ls-files` failed, but `git --version` works.  Check that scons is run from a Git repository.")
			# Filter out OS X related directories.  Files in those
			# directories assume they are only ever built on OS X, so
			# they unconditionally include headers specific to OS X.
			excluded_directories = (
				'common/arch/cocoa/',
				'common/arch/carbon/',
			)
			__shared_header_file_list.extend([h for h in headers.split('\0') if h and not h.startswith(excluded_directories)])
			if not __shared_header_file_list:
				raise SCons.Errors.StopError("`git ls-files` found headers, but none can be checked.")
		dirname = os.path.join(builddir, self.srcdir)
		Depends = env.Depends
		StaticObject = env.StaticObject
		CPPFLAGS_template = env['CPPFLAGS']
		CPPFLAGS_no_sconf = CPPFLAGS_template + ['-g0', '-include', '$DXX_EFFECTIVE_SOURCE']
		CPPFLAGS_with_sconf = ['-include', 'dxxsconf.h'] + CPPFLAGS_no_sconf
		CXXCOMSTR = env.__header_check_output_COMSTR
		CXXFLAGS = env['CXXFLAGS']
		target = os.path.join('%s/chi/${DXX_EFFECTIVE_SOURCE}%s' % (dirname, env['OBJSUFFIX']))
		for name in __shared_header_file_list:
			if not name:
				continue
			if self.srcdir == 'common' and not name.startswith('common/'):
				# Skip game-specific headers when testing common
				continue
			if self.srcdir[0] == 'd' and name[0] == 'd' and not name.startswith(self.srcdir):
				# Skip d1 in d2 and d2 in d1
				continue
			# Compiler feature headers cannot include dxxsconf.h because
			# it confuses the dependency resolver when SConf runs.
			# Calling code must include dxxsconf.h before including the
			# compiler feature header, so add the inclusion here.
			#
			# For best test coverage, only headers that must avoid
			# including dxxsconf.h receive an implicit include.  Any
			# header which needs dxxsconf.h and can include it without
			# side effects must do so.
			StaticObject(target=target,
				CPPFLAGS=CPPFLAGS_with_sconf if name[:24] == 'common/include/compiler-' else CPPFLAGS_no_sconf,
				CXXCOMSTR=CXXCOMSTR,
				CXXFLAGS=CXXFLAGS,
				DXX_EFFECTIVE_SOURCE=name,
				source=check_header_includes)

	def _cpp_output_StaticObject(self,target=None,source=None,DXX_EFFECTIVE_SOURCE='$SOURCE',*args,**kwargs):
		CPPFLAGS = kwargs.get('CPPFLAGS', None)
		CXXFLAGS = kwargs.get('CXXFLAGS', None)
		env = self.env
		OBJSUFFIX = env['OBJSUFFIX']
		StaticObject = env.__cpp_output_StaticObject
		StaticObject(
			target='%s.i' % (target[:-len(OBJSUFFIX)] if target.endswith(OBJSUFFIX) else target),
			source=source, OBJSUFFIX='.i',
			# Bypass ccache
			CXXCOM=env._dxx_cxxcom_no_prefix,
			CPPFLAGS=(env['CPPFLAGS'] if CPPFLAGS is None else CPPFLAGS),
			CXXFLAGS=(env['CXXFLAGS'] if CXXFLAGS is None else CXXFLAGS) + ['-E'],
			CXXCOMSTR=env.__generate_cpp_output_COMSTR,
			DXX_EFFECTIVE_SOURCE=DXX_EFFECTIVE_SOURCE,
		)
		return StaticObject(target=target, source=source, DXX_EFFECTIVE_SOURCE=DXX_EFFECTIVE_SOURCE, *args, **kwargs)

	def create_special_target_nodes(self,archive):
		user_settings = self.user_settings
		if user_settings.register_cpp_output_targets:
			env = self.env
			env.__cpp_output_StaticObject = env.StaticObject
			env.StaticObject = self._cpp_output_StaticObject
		if user_settings.check_header_includes:
			# Create header targets before creating the PCHManager, so that
			# create_header_targets() does not call the PCHManager
			# StaticObject hook.
			self.create_header_targets()
		configure_pch_flags = archive.configure_pch_flags
		if configure_pch_flags:
			self.pch_manager = PCHManager(self.user_settings, self.env, self.srcdir, configure_pch_flags, archive.pch_manager)

	@staticmethod
	def _quote_cppdefine(s,f=repr):
		r = ''
		prior = False
		b2a_hex = binascii.b2a_hex
		for c in f(s):
			# No xdigit support in str
			if c in ' ()*+,-./:=[]_' or (c.isalnum() and not (prior and (c.isdigit() or c in 'abcdefABCDEF'))):
				r += c
			elif c == '\n':
				r += r'\n'
			else:
				r += r'\\x%s' % b2a_hex(c)
				prior = True
				continue
			prior = False
		return '\\"%s\\"' % r

	def prepare_environment(self):
		# Prettier build messages......
		# Move target to end of C++ source command
		target_string = ' -o $TARGET'
		env = self.env
		# Get traditional compiler environment variables
		for cc in ('CXX', 'RC'):
			value = getattr(self.user_settings, cc)
			if value is not None:
				env[cc] = value
		# Expand $CXX immediately.
		# $CCFLAGS is never used.  Remove it.
		cxxcom = env['CXXCOM'] \
			.replace('$CXX ', '%s ' % env['CXX']) \
			.replace('$CCFLAGS ', '')
		if target_string + ' ' in cxxcom:
			cxxcom = '%s%s' % (cxxcom.replace(target_string, ''), target_string)
		env._dxx_cxxcom_no_prefix = cxxcom
		distcc_path = self.user_settings.distcc
		distcc_cxxcom = ('%s %s' % (distcc_path, cxxcom)) if distcc_path else cxxcom
		env._dxx_cxxcom_no_ccache_prefix = distcc_cxxcom
		ccache_path = self.user_settings.ccache
		# Add ccache/distcc only for compile, not link
		if ccache_path:
			cxxcom = '%s %s' % (ccache_path, cxxcom)
			if distcc_path is not None:
				penv = self.env['ENV']
				if distcc_path:
					penv['CCACHE_PREFIX'] = distcc_path
				elif distcc_path is not None:
					penv.pop('CCACHE_PREFIX', None)
		elif distcc_path:
			cxxcom = distcc_cxxcom
		# Expand $LINK immediately.
		linkcom = env['LINKCOM'].replace('$LINK ', '%s ' % env['LINK'])
		# Move target to end of link command
		if target_string + ' ' in linkcom:
			linkcom = '%s%s' % (linkcom.replace(target_string, ''), target_string)
		# Add $CXXFLAGS to link command
		cxxflags = '$CXXFLAGS '
		if ' ' + cxxflags not in linkcom:
			linkflags = '$LINKFLAGS'
			linkcom = linkcom.replace(linkflags, cxxflags + linkflags)
		env.Replace(
			CXXCOM = cxxcom,
			LINKCOM = linkcom,
		)
		# Custom DISTCC_HOSTS per target
		distcc_hosts = self.user_settings.distcc_hosts
		if distcc_hosts is not None:
			env['ENV']['DISTCC_HOSTS'] = distcc_hosts
		if self.user_settings.verbosebuild:
			env.__header_check_output_COMSTR	= None
			env.__generate_cpp_output_COMSTR	= None
		else:
			target = self.target[:3]
			format_tuple = (target, self.user_settings.builddir or '.')
			env.__header_check_output_COMSTR	= "CHK %s %s $DXX_EFFECTIVE_SOURCE" % format_tuple
			env.__generate_cpp_output_COMSTR	= "CPP %s %s $DXX_EFFECTIVE_SOURCE" % format_tuple
			env.Replace(
				CXXCOMSTR						= "CXX %s %s $SOURCE" % format_tuple,
			# `builddir` is implicit since $TARGET is the full path to
			# the output
				LINKCOMSTR						= "LD  %s $TARGET" % target,
			)

		Werror = get_Werror_string(self.user_settings.CXXFLAGS)
		env.Prepend(CXXFLAGS = [
			'-Wall',
			Werror + 'extra',
			Werror + 'format=2',
			Werror + 'missing-braces',
			Werror + 'missing-include-dirs',
			Werror + 'unused',
			Werror + 'uninitialized',
			Werror + 'undef',
			Werror + 'pointer-arith',
			Werror + 'cast-qual',
			Werror + 'missing-declarations',
			Werror + 'redundant-decls',
			Werror + 'vla',
		])
		env.Append(
			CXXFLAGS = ['-funsigned-char'],
			CPPPATH = ['common/include', 'common/main', '.', self.user_settings.builddir],
			CPPFLAGS = SCons.Util.CLVar('-Wno-sign-compare'),
		)
		add_flags = {}
		if self.user_settings.editor:
			add_flags['CPPPATH'] = ['common/include/editor']
		CLVar = SCons.Util.CLVar
		for flags in ('CPPFLAGS', 'CXXFLAGS', 'LIBS'):
			value = getattr(self.user_settings, flags)
			if value is not None:
				add_flags[flags] = CLVar(value)
		if self.user_settings.LDFLAGS:
			add_flags['LINKFLAGS'] = CLVar(self.user_settings.LDFLAGS)
		env.Append(**add_flags)
		if self.user_settings.lto:
			env.Append(CXXFLAGS = [
				# clang does not support =N syntax
				('-flto=%s' % self.user_settings.lto) if self.user_settings.lto > 1 else '-flto',
			])

	def check_endian(self):
		# set endianess
		if (self.__endian == "big"):
			message(self, "BigEndian machine detected")
			self.env.Append(CPPDEFINES = ['WORDS_BIGENDIAN'])
		elif (self.__endian == "little"):
			message(self, "LittleEndian machine detected")

	@cached_property
	def platform_settings(self):
		# windows or *nix?
		platform_name = self.user_settings.host_platform
		message(self, "compiling on %s for %s into %s%s" % (sys.platform, platform_name, self.user_settings.builddir or '.',
			(' with prefix list %s' % str(self._argument_prefix_list)) if self._argument_prefix_list else ''))
		return (
			self.Win32PlatformSettings if platform_name == 'win32' else (
				self.DarwinPlatformSettings if platform_name == 'darwin' else
				self.LinuxPlatformSettings
			)
		)(self, self.user_settings)

	@cached_property
	def env(self):
		platform_settings = self.platform_settings
		# Acquire environment object...
		env = Environment(ENV = os.environ, tools = platform_settings.tools + ('textfile',))
		platform_settings.adjust_environment(self, env)
		return env

	def process_user_settings(self):
		env = self.env
		CPPDEFINES = []
		add_cpp_define = CPPDEFINES.append
		user_settings = self.user_settings

		env.Prepend(CXXFLAGS = ['-g', '-O2'])
		# Raspberry Pi?
		if user_settings.raspberrypi:
			rpi_vc_path = user_settings.rpi_vc_path
			message(self, "Raspberry Pi: using VideoCore libs in %r" % rpi_vc_path)
			env.Append(
				CPPDEFINES = ['RPI', 'WORDS_NEED_ALIGNMENT'],
			# use CPPFLAGS -isystem instead of CPPPATH because these those header files
			# are not very clean and would trigger some warnings we usually consider as
			# errors. Using them as system headers will make gcc ignoring any warnings.
				CPPFLAGS = [
				'-isystem=%s/include' % rpi_vc_path,
				'-isystem=%s/include/interface/vcos/pthreads' % rpi_vc_path,
				'-isystem=%s/include/interface/vmcs_host/linux' % rpi_vc_path,
			],
				LIBPATH = '%s/lib' % rpi_vc_path,
				LIBS = ['bcm_host'],
			)
		env.Append(CPPDEFINES = CPPDEFINES)

class DXXArchive(DXXCommon):
	PROGRAM_NAME = 'DXX-Archive'
	_argument_prefix_list = None
	srcdir = 'common'
	target = 'dxx-common'
	__objects_common = DXXCommon.create_lazy_object_property([os.path.join(srcdir, f) for f in [
'main/getht.cpp',
'2d/2dsline.cpp',
'2d/bitblt.cpp',
'2d/bitmap.cpp',
'2d/box.cpp',
'2d/canvas.cpp',
'2d/circle.cpp',
'2d/disc.cpp',
'2d/gpixel.cpp',
'2d/line.cpp',
'2d/pixel.cpp',
'2d/rect.cpp',
'2d/rle.cpp',
'2d/scalec.cpp',
'3d/draw.cpp',
'3d/globvars.cpp',
'3d/instance.cpp',
'3d/matrix.cpp',
'3d/points.cpp',
'3d/rod.cpp',
'3d/setup.cpp',
'arch/sdl/event.cpp',
'arch/sdl/joy.cpp',
'arch/sdl/key.cpp',
'arch/sdl/mouse.cpp',
'arch/sdl/timer.cpp',
'arch/sdl/window.cpp',
'main/cli.cpp',
'main/cmd.cpp',
'main/cvar.cpp',
'maths/fixc.cpp',
'maths/rand.cpp',
'maths/tables.cpp',
'maths/vecmat.cpp',
'mem/mem.cpp',
'misc/error.cpp',
'misc/hash.cpp',
'misc/hmp.cpp',
'misc/ignorecase.cpp',
'misc/strutil.cpp',
'texmap/ntmap.cpp',
'texmap/scanline.cpp'
]
])
	objects_use_sdl1 = DXXCommon.create_lazy_object_property([os.path.join(srcdir, f) for f in [
'arch/sdl/rbaudio.cpp',
]
])
	objects_editor = DXXCommon.create_lazy_object_property([os.path.join(srcdir, f) for f in [
'editor/autosave.cpp',
'editor/func.cpp',
'ui/button.cpp',
'ui/checkbox.cpp',
'ui/dialog.cpp',
'ui/file.cpp',
'ui/gadget.cpp',
'ui/icon.cpp',
'ui/inputbox.cpp',
'ui/keypad.cpp',
'ui/keypress.cpp',
'ui/listbox.cpp',
'ui/menu.cpp',
'ui/menubar.cpp',
'ui/message.cpp',
'ui/radio.cpp',
'ui/scroll.cpp',
'ui/ui.cpp',
'ui/uidraw.cpp',
'ui/userbox.cpp',

]
])
	# for non-ogl
	objects_arch_sdl = DXXCommon.create_lazy_object_property([os.path.join(srcdir, f) for f in [
'3d/clipper.cpp',
'texmap/tmapflat.cpp'
]
])
	# for ogl
	objects_arch_ogl = DXXCommon.create_lazy_object_property([os.path.join(srcdir, f) for f in [
'arch/ogl/ogl_extensions.cpp',
'arch/ogl/ogl_sync.cpp'
]
])
	objects_arch_sdlmixer = DXXCommon.create_lazy_object_property([os.path.join(srcdir, f) for f in [
'arch/sdl/digi_mixer_music.cpp',
]
])
	class Win32PlatformSettings(DXXCommon.Win32PlatformSettings):
		platform_objects = LazyObjectConstructor.create_lazy_object_property([
'common/arch/win32/messagebox.cpp'
])
	class DarwinPlatformSettings(DXXCommon.DarwinPlatformSettings):
		platform_objects = LazyObjectConstructor.create_lazy_object_property([
			'common/arch/cocoa/messagebox.mm',
			'common/arch/cocoa/SDLMain.m'
		])

	@property
	def objects_common(self):
		value = list(self.__objects_common)
		extend = value.extend
		if not self.user_settings.sdl2:
			extend(self.objects_use_sdl1)
		extend(self.platform_settings.platform_objects)
		return value

	def __init__(self,user_settings):
		DXXCommon.__init__(self)
		self.user_settings = user_settings.clone()
		if not user_settings.register_compile_target:
			return
		self.prepare_environment()
		self.check_endian()
		self.process_user_settings()
		self.configure_environment()
		self.create_special_target_nodes(self)

	def configure_environment(self):
		fs = SCons.Node.FS.get_default_fs()
		builddir = fs.Dir(self.user_settings.builddir or '.')
		tests = ConfigureTests(self.program_message_prefix, self.user_settings, self.platform_settings)
		log_file=fs.File('sconf.log', builddir)
		conf = self.env.Configure(custom_tests = {
				k.name:getattr(tests, k.name) for k in tests.custom_tests
			},
			conf_dir=fs.Dir('.sconf_temp', builddir),
			log_file=log_file,
			config_h=fs.File('dxxsconf.h', builddir),
			clean=False,
			help=False
		)
		self.configure_added_environment_flags = tests.successful_flags
		self.configure_pch_flags = None
		if not conf.env:
			return
		cc_env_strings = tests.ForceVerboseLog(conf.env)
		try:
			for k in tests.custom_tests:
				getattr(conf, k.name)()
		except SCons.Errors.StopError as e:
			raise SCons.Errors.StopError('{e0}  See {log_file} for details.'.format(e0=e.args[0], log_file=log_file), *e.args[1:])
		cc_env_strings.restore(conf.env)
		if self.user_settings.record_sconf_results:
			conf.config_h_text += '''
/*
%s
 */
''' % '\n'.join(['check_%s=%s' % (n,v) for n,v in tests._sconf_results])
		conf.config_h_text += '''
#if defined(DXX_BUILD_DESCENT_I) || defined(DXX_BUILD_DESCENT_II)
#if defined(DXX_BUILD_DESCENT_I)
#define dsx d1x
#else
#define dsx d2x
#endif
namespace dsx {	/* Force type mismatch on attempted nesting */
	class dcx;	/* dcx declared inside dsx */
	class dsx;	/* dsx declared inside dsx */
}
using namespace dsx;
#else
class dsx;	/* dsx declared in common-only code */
#endif

namespace dcx {	/* Force type mismatch on attempted nesting */
	class dcx;	/* dcx declared inside dcx */
	class dsx;	/* dsx declared inside dcx */
}
using namespace dcx;
namespace {
	class dcx;	/* dcx declared inside anonymous */
	class dsx;	/* dsx declared inside anonymous */
}
'''
		conf.Finish()
		self.configure_pch_flags = tests.pch_flags
		self.env.MergeFlags(self.configure_added_environment_flags)

class DXXProgram(DXXCommon):
	# version number
	VERSION_MAJOR = 0
	VERSION_MINOR = 58
	VERSION_MICRO = 1
	static_archive_construction = {}
	# None when unset.  Tuple of one once cached.
	_computed_extra_version = None
	def _apply_target_name(self,name):
		return os.path.join(os.path.dirname(name), '.%s.%s' % (self.target, os.path.splitext(os.path.basename(name))[0]))
	objects_similar_arch_ogl = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join('similar', f) for f in [
'arch/ogl/gr.cpp',
'arch/ogl/ogl.cpp',
]
],
		'transform_target':_apply_target_name,
	}])
	objects_similar_arch_sdl = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join('similar', f) for f in [
'arch/sdl/gr.cpp',
]
],
		'transform_target':_apply_target_name,
	}])
	objects_similar_arch_sdlmixer = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join('similar', f) for f in [
'arch/sdl/digi_mixer.cpp',
'arch/sdl/jukebox.cpp'
]
],
		'transform_target':_apply_target_name,
	}])
	__objects_common = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join('similar', f) for f in [
'2d/font.cpp',
'2d/palette.cpp',
'2d/pcx.cpp',
'3d/interp.cpp',
'arch/sdl/digi.cpp',
'arch/sdl/digi_audio.cpp',
'arch/sdl/init.cpp',
'main/ai.cpp',
'main/aipath.cpp',
'main/automap.cpp',
'main/bm.cpp',
'main/cntrlcen.cpp',
'main/collide.cpp',
'main/config.cpp',
'main/console.cpp',
'main/controls.cpp',
'main/credits.cpp',
'main/digiobj.cpp',
'main/effects.cpp',
'main/endlevel.cpp',
'main/fireball.cpp',
'main/fuelcen.cpp',
'main/fvi.cpp',
'main/game.cpp',
'main/gamecntl.cpp',
'main/gamefont.cpp',
'main/gamemine.cpp',
'main/gamerend.cpp',
'main/gamesave.cpp',
'main/gameseg.cpp',
'main/gameseq.cpp',
'main/gauges.cpp',
'main/hostage.cpp',
'main/hud.cpp',
'main/iff.cpp',
'main/inferno.cpp',
'main/kconfig.cpp',
'main/kmatrix.cpp',
'main/laser.cpp',
'main/lighting.cpp',
'main/menu.cpp',
'main/mglobal.cpp',
'main/mission.cpp',
'main/morph.cpp',
'main/multi.cpp',
'main/multibot.cpp',
'main/newdemo.cpp',
'main/newmenu.cpp',
'main/object.cpp',
'main/paging.cpp',
'main/physics.cpp',
'main/piggy.cpp',
'main/player.cpp',
'main/playsave.cpp',
'main/polyobj.cpp',
'main/powerup.cpp',
'main/render.cpp',
'main/robot.cpp',
'main/scores.cpp',
'main/segment.cpp',
'main/slew.cpp',
'main/songs.cpp',
'main/state.cpp',
'main/switch.cpp',
'main/terrain.cpp',
'main/texmerge.cpp',
'main/text.cpp',
'main/titles.cpp',
'main/vclip.cpp',
'main/wall.cpp',
'main/weapon.cpp',
'misc/args.cpp',
'misc/physfsx.cpp',
]
],
		'transform_target':_apply_target_name,
	}])
	objects_editor = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join('similar', f) for f in [
'editor/centers.cpp',
'editor/curves.cpp',
'main/dumpmine.cpp',
'editor/eglobal.cpp',
'editor/elight.cpp',
'editor/eobject.cpp',
'editor/eswitch.cpp',
'editor/group.cpp',
'editor/info.cpp',
'editor/kbuild.cpp',
'editor/kcurve.cpp',
'editor/kfuncs.cpp',
'editor/kgame.cpp',
'editor/khelp.cpp',
'editor/kmine.cpp',
'editor/ksegmove.cpp',
'editor/ksegsel.cpp',
'editor/ksegsize.cpp',
'editor/ktmap.cpp',
'editor/kview.cpp',
'editor/med.cpp',
'editor/meddraw.cpp',
'editor/medmisc.cpp',
'editor/medrobot.cpp',
'editor/medsel.cpp',
'editor/medwall.cpp',
'editor/mine.cpp',
'editor/objpage.cpp',
'editor/segment.cpp',
'editor/seguvs.cpp',
'editor/texpage.cpp',
'editor/texture.cpp',
]
],
		'transform_target':_apply_target_name,
	}])

	objects_use_udp = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join('similar', f) for f in [
'main/net_udp.cpp',
]
],
		'transform_target':_apply_target_name,
	}])
	class UserSettings(DXXCommon.UserSettings):
		@property
		def BIN_DIR(self):
			# installation path
			return '%s/bin' % self.prefix
	# Settings to apply to mingw32 builds
	class Win32PlatformSettings(DXXCommon.Win32PlatformSettings):
		def __init__(self,program,user_settings):
			DXXCommon.Win32PlatformSettings.__init__(self,program,user_settings)
			user_settings.sharepath = ''
			self.platform_objects = self.platform_objects[:]
		def adjust_environment(self,program,env):
			DXXCommon.Win32PlatformSettings.adjust_environment(self, program, env)
			rcbasename = os.path.join(program.srcdir, 'arch/win32/%s' % program.target)
			self.platform_objects.append(env.RES(target='%s%s%s' % (program.user_settings.builddir, rcbasename, env["OBJSUFFIX"]), source='%s.rc' % rcbasename))
			env.Append(
				CPPPATH = [os.path.join(program.srcdir, 'arch/win32/include')],
				LINKFLAGS = ['-mwindows'],
				LIBS = ['glu32', 'wsock32', 'ws2_32', 'winmm', 'mingw32', 'SDLmain', 'SDL'],
			)
	# Settings to apply to Apple builds
	class DarwinPlatformSettings(DXXCommon.DarwinPlatformSettings):
		def __init__(self,program,user_settings):
			DXXCommon.DarwinPlatformSettings.__init__(self,program,user_settings)
			user_settings.sharepath = ''
			self.platform_objects = self.platform_objects[:]
		def adjust_environment(self,program,env):
			DXXCommon.DarwinPlatformSettings.adjust_environment(self, program, env)
			VERSION = '%s.%s' % (program.VERSION_MAJOR, program.VERSION_MINOR)
			if (program.VERSION_MICRO):
				VERSION += '.%s' % program.VERSION_MICRO
			env.Replace(
				VERSION_NUM = VERSION,
				VERSION_NAME = '%s v%s' % (program.PROGRAM_NAME, VERSION),
			)
	# Settings to apply to Linux builds
	class LinuxPlatformSettings(DXXCommon.LinuxPlatformSettings):
		def __init__(self,program,user_settings):
			DXXCommon.LinuxPlatformSettings.__init__(self,program,user_settings)
			if user_settings.sharepath and user_settings.sharepath[-1] != '/':
				user_settings.sharepath += '/'

	@property
	def objects_common(self):
		value = list(self.__objects_common)
		extend = value.extend
		if self.user_settings.use_udp:
			extend(self.objects_use_udp)
		extend(self.platform_settings.platform_objects)
		return value

	def __init__(self,prefix,variables):
		self.variables = variables
		self._argument_prefix_list = prefix
		DXXCommon.__init__(self)
		print('===== %s v%s.%s.%s =====' % (self.PROGRAM_NAME, self.VERSION_MAJOR, self.VERSION_MINOR, self.VERSION_MICRO))
		self.user_settings = user_settings = self.UserSettings(program=self)
		user_settings.register_variables(prefix=prefix, variables=variables)

	def init(self,substenv):
		self.user_settings.read_variables(self.variables, substenv)
		archive = DXXProgram.static_archive_construction.get(self.user_settings.builddir, None)
		if archive is None:
			DXXProgram.static_archive_construction[self.user_settings.builddir] = archive = DXXArchive(self.user_settings)
		if self.user_settings.register_compile_target:
			self.prepare_environment(archive)
			self.process_user_settings()
		self.register_program()

	def prepare_environment(self,archive,
			_DXX_VERSION_SEQ=('DXX_VERSION_SEQ', ','.join([str(VERSION_MAJOR), str(VERSION_MINOR), str(VERSION_MICRO)]))
		):
		self.check_endian()
		DXXCommon.prepare_environment(self)
		env = self.env
		env.MergeFlags(archive.configure_added_environment_flags)
		self.create_special_target_nodes(archive)
		env.Append(
			CPPDEFINES = [
				self.env_CPPDEFINES,
				_DXX_VERSION_SEQ,
		# For PRIi64
				('__STDC_FORMAT_MACROS',),
				('SHAREPATH', self._quote_cppdefine(self.user_settings.sharepath, f=str)),
			],
			CPPPATH = [os.path.join(self.srcdir, 'main')],
			LIBS = ['m'],
		)

	def register_program(self):
		exe_target = self.user_settings.program_name
		if not exe_target:
			exe_target = os.path.join(self.srcdir, self.target)
			if self.user_settings.editor:
				exe_target += '-editor'
		exe_target = os.path.join(self.user_settings.builddir, exe_target)
		PROGSUFFIX = self.env['PROGSUFFIX']
		if PROGSUFFIX and not exe_target.endswith(PROGSUFFIX):
			exe_target += PROGSUFFIX
		if self.user_settings.register_compile_target:
			exe_target = self._register_program(exe_target)
		if self.user_settings.register_install_target:
			self._register_install(self.shortname, exe_target)

	@classmethod
	def compute_extra_version(cls):
		c = cls._computed_extra_version
		if c is None:
			s = ds = None
			v = cls._compute_extra_version()
			if v:
				s = Git.spcall(['status', '--short', '--branch'])
				ds = Git.spcall(['diff', '--stat', 'HEAD'])
			cls._computed_extra_version = c = (v or '', s, ds)
		return c

	@classmethod
	def _compute_extra_version(cls):
		try:
			g = Git.pcall(['describe', '--tags', '--abbrev=8'], stderr=subprocess.PIPE)
		except OSError as e:
			if e.errno == errno.ENOENT:
				return None
			raise
		if g.returncode:
			return None
		c = Git.pcall(['diff', '--quiet', '--cached']).returncode
		d = Git.pcall(['diff', '--quiet']).returncode
		return g.out.split('\n')[0] + ('+' if c else '') + ('*' if d else '')

	def _register_program(self,exe_target):
		env = self.env
		static_archive_construction = self.static_archive_construction[self.user_settings.builddir]
		objects = static_archive_construction.objects_common
		objects.extend(self.objects_common)
		if self.user_settings.sdlmixer:
			objects.extend(static_archive_construction.objects_arch_sdlmixer)
			objects.extend(self.objects_similar_arch_sdlmixer)
		if self.user_settings.opengl or self.user_settings.opengles:
			env.Append(LIBS = self.platform_settings.ogllibs)
			objects.extend(static_archive_construction.objects_arch_ogl)
			objects.extend(self.objects_similar_arch_ogl)
		else:
			objects.extend(static_archive_construction.objects_arch_sdl)
			objects.extend(self.objects_similar_arch_sdl)
		if self.user_settings.editor:
			objects.extend(self.objects_editor)
			objects.extend(static_archive_construction.objects_editor)
		versid_build_environ = ['CXX', 'CPPFLAGS', 'CXXFLAGS', 'LINKFLAGS']
		versid_cppdefines = env['CPPDEFINES'][:]
		versid_cppdefines.extend([('DESCENT_%s' % k, self._quote_cppdefine(env.get(k, ''))) for k in versid_build_environ])
		v = StaticSubprocess.get_version_head(env['CXX'])
		versid_cppdefines.append(('DESCENT_CXX_version', self._quote_cppdefine(v)))
		versid_build_environ.append('CXX_version')
		extra_version = self.user_settings.extra_version
		if extra_version is None:
			extra_version = 'v%u.%u' % (self.VERSION_MAJOR, self.VERSION_MINOR)
			if self.VERSION_MICRO:
				extra_version += '.%u' % self.VERSION_MICRO
		git_describe_version = (self.compute_extra_version() if self.user_settings.git_describe_version else ('', '', ''))
		if git_describe_version[0] and not (extra_version and (extra_version == git_describe_version[0] or (extra_version[0] == 'v' and extra_version[1:] == git_describe_version[0]))):
			# Suppress duplicate output
			if extra_version:
				extra_version += ' '
			extra_version += git_describe_version[0]
		versid_cppdefines.append(('DESCENT_VERSION_EXTRA', self._quote_cppdefine(extra_version, f=str)))
		versid_cppdefines.append(('DESCENT_git_status', self._quote_cppdefine(git_describe_version[1])))
		versid_build_environ.append('git_status')
		versid_cppdefines.append(('DESCENT_git_diffstat', self._quote_cppdefine(git_describe_version[2])))
		versid_build_environ.append('git_diffstat')
		versid_cppdefines.append(('DXX_RBE"(A)"', '"%s"' % ''.join(['A(%s)' % k for k in versid_build_environ])))
		versid_environ = self.env['ENV'].copy()
		# Direct mode conflicts with __TIME__
		versid_environ['CCACHE_NODIRECT'] = 1
		versid_cpp = 'similar/main/vers_id.cpp'
		versid_obj = env.StaticObject(target='%s%s%s' % (self.user_settings.builddir, self._apply_target_name(versid_cpp), self.env["OBJSUFFIX"]), source=versid_cpp, CPPDEFINES=versid_cppdefines, ENV=versid_environ)
		if self.user_settings.versid_depend_all:
			# Optional fake dependency to force vers_id to rebuild so
			# that it picks up the latest timestamp.
			env.Depends(versid_obj, objects)
		objects.append(versid_obj)
		# finally building program...
		return env.Program(target=exe_target, source = objects)

	def _register_install(self,dxxstr,exe_node):
		env = self.env
		if self.user_settings.host_platform != 'darwin':
				install_dir = '%s%s' % (self.user_settings.DESTDIR or '', self.user_settings.BIN_DIR)
				env.Install(install_dir, exe_node)
				env.Alias('install', install_dir)
		else:
			syspath = sys.path[:]
			cocoa = 'common/arch/cocoa'
			sys.path += [cocoa]
			import tool_bundle
			sys.path = syspath
			tool_bundle.TOOL_BUNDLE(env)
			env.MakeBundle(os.path.join(self.user_settings.builddir, '%s.app' % self.PROGRAM_NAME), exe_node,
					'free.%s-rebirth' % dxxstr, os.path.join(cocoa, 'Info.plist'),
					typecode='APPL', creator='DCNT',
					icon_file=os.path.join(cocoa, '%s-rebirth.icns' % dxxstr),
					resources=[[os.path.join(self.srcdir, s), s] for s in ['English.lproj/InfoPlist.strings']])

	def GenerateHelpText(self):
		return self.variables.GenerateHelpText(self.env)

class D1XProgram(DXXProgram):
	PROGRAM_NAME = 'D1X-Rebirth'
	target = 'd1x-rebirth'
	srcdir = 'd1x-rebirth'
	shortname = 'd1x'
	env_CPPDEFINES = ('DXX_BUILD_DESCENT_I',)

	# general source files
	__objects_common = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join(srcdir, f) for f in [
'main/bmread.cpp',
'main/custom.cpp',
'main/snddecom.cpp',
#'tracker/client/tracker_client.c'
]
],
	}])
	@property
	def objects_common(self):
		value = DXXProgram.objects_common.fget(self)
		value.extend(self.__objects_common)
		return value

	# for editor
	__objects_editor = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join(srcdir, f) for f in [
'main/hostage.cpp',
'editor/ehostage.cpp',
]
],
	}])
	@property
	def objects_editor(self):
		value = list(DXXProgram.objects_editor.fget(self))
		value.extend(self.__objects_editor)
		return value

class D2XProgram(DXXProgram):
	PROGRAM_NAME = 'D2X-Rebirth'
	target = 'd2x-rebirth'
	srcdir = 'd2x-rebirth'
	shortname = 'd2x'
	env_CPPDEFINES = ('DXX_BUILD_DESCENT_II',)

	# general source files
	__objects_common = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join(srcdir, f) for f in [
'libmve/decoder8.cpp',
'libmve/decoder16.cpp',
'libmve/mve_audio.cpp',
'libmve/mvelib.cpp',
'libmve/mveplay.cpp',
'main/escort.cpp',
'main/gamepal.cpp',
'main/movie.cpp',
'misc/physfsrwops.cpp',
]
],
	}])
	@property
	def objects_common(self):
		value = DXXProgram.objects_common.fget(self)
		value.extend(self.__objects_common)
		return value

	# for editor
	__objects_editor = DXXCommon.create_lazy_object_property([{
		'source':[os.path.join(srcdir, f) for f in [
'main/bmread.cpp',
]
],
	}])
	@property
	def objects_editor(self):
		value = list(DXXProgram.objects_editor.fget(self))
		value.extend(self.__objects_editor)
		return value

variables = Variables([v for (k,v) in ARGLIST if k == 'site'] or ['site-local.py'], ARGUMENTS)
filtered_help = FilterHelpText()
variables.FormatVariableHelpText = filtered_help.FormatVariableHelpText
def _filter_duplicate_prefix_elements(e,s):
	r = e not in s
	s.add(e)
	return r
def register_program(program,other_program):
	s = program.shortname
	l = [v for (k,v) in ARGLIST if k == s or k == 'dxx'] or [other_program.shortname not in ARGUMENTS]
	# Fallback case: build the regular configuration.
	if len(l) == 1:
		try:
			r = int(l[0])
		except ValueError:
			# If not an integer, treat this as a configuration profile.
			pass
		else:
			return [program((s,''), variables)] if r else []
	r = []
	seen = set()
	for e in l:
		for prefix in itertools.product(*[v.split('+') for v in e.split(',')]):
			duplicates = set()
			prefix = tuple(p for p in prefix if _filter_duplicate_prefix_elements(p, duplicates))
			if prefix in seen:
				continue
			seen.add(prefix)
			prefix = tuple('%s%s%s' % (s, '_' if p else '', p) for p in prefix) + prefix
			r.append(program(prefix, variables))
	return r
d1x = register_program(D1XProgram, D2XProgram)
d2x = register_program(D2XProgram, D1XProgram)

# show some help when running scons -h
h = """DXX-Rebirth, SConstruct file help:

	Type 'scons' to build the binary.
	Type 'scons install' to build (if it hasn't been done) and install.
	Type 'scons -c' to clean up.
	
	Extra options (add them to command line, like 'scons extraoption=value'):
	d1x=[0/1]        Disable/enable D1X-Rebirth
	d1x=prefix-list  Enable D1X-Rebirth with prefix-list modifiers
	d2x=[0/1]        Disable/enable D2X-Rebirth
	d2x=prefix-list  Enable D2X-Rebirth with prefix-list modifiers
	dxx=VALUE        Equivalent to d1x=VALUE d2x=VALUE
"""
substenv = SCons.Environment.SubstitutionEnvironment()
variables.Update(substenv)
dxx = d1x + d2x
for d in dxx:
	d.init(substenv)
	h += '%s.%d:\n%s' % (d.PROGRAM_NAME, d.program_instance, d.GenerateHelpText())
Help(h)
unknown = variables.UnknownVariables()
# Delete known unregistered variables
unknown.pop('d1x', None)
unknown.pop('d2x', None)
unknown.pop('dxx', None)
unknown.pop('site', None)
ignore_unknown_variables = unknown.pop('ignore_unknown_variables', '0')
if dxx and unknown:
	try:
		ignore_unknown_variables = int(ignore_unknown_variables)
	except ValueError:
		ignore_unknown_variables = False
	if not ignore_unknown_variables:
		raise SCons.Errors.StopError('Unknown values specified on command line.' +
''.join(['\n\t%s' % k for k in unknown.keys()]) +
'\nRemove unknown values or set ignore_unknown_variables=1 to continue.')

#EOF
