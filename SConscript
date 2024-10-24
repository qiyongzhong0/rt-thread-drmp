from building import *

cwd = GetCurrentDir()
path = [cwd+'/inc']
src  = Glob('src/*.c')

if GetDepend(['DRMP_USING_SAMPLE']):
    src += Glob('sample/*.c')
    
group = DefineGroup('drmp', src, depend = ['PKG_USING_DRMP'], CPPPATH = path)

Return('group')