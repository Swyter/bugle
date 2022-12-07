from SCons import *

def _wrapper(self, target, source, replacements, **kw):
    sources = [source, self.Value(replacements)]
    return self.SubstFileReal(target, sources, **kw)

def substfile(target, source, env):
    src = source[0].get_contents().decode('utf-8')
    substs = source[1].read()
    for key, value in substs.items():
        src = src.replace('@' + key + '@', value)
    f = open(str(target[0]), 'wb')
    f.write(src.encode('utf-8'))
    f.close()

def generate(env, **kw):
    substfilereal_builder = env.Builder(action = Action.Action(substfile, '$SUBSTFILECOMSTR'))
    env.AddMethod(_wrapper, 'SubstFile')
    env.Append(
            BUILDERS = {'SubstFileReal': substfilereal_builder},
            SUBSTFILECOMSTR = 'SUBST $TARGET ${SOURCES[0]}')

def exists(env):
    return 1
