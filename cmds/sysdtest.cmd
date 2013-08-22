/* test */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

do i = 1 to 100
call SysSleep .25 
'logger -a 1 -f 3 -p 5 This is the big test message'
'logger -a 2 -f 3 -p 5 This is the big test message' 
end