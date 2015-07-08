@echo off
set MYLOG_DIR=log
set curdir=%~dp0
set logdir="%curdir%"%MYLOG_DIR%
set NSPR_LOG_MODULES=smtp:5,imap:5,timestamp,sync
set dataandtime=%date:~0,4%%date:~5,2%%date:~8,2%-%time:~0,2%%time:~3,2%%time:~6,2%%time:~9,2%
set logfilename=.\%MYLOG_DIR%\%dataandtime%.log
set NSPR_LOG_FILE=Firemail.log

if not exist %logdir% (
        md %logdir%
) else (
        echo dir ready.
)

if not exist %logdir% (
        echo can't create ./log dir.
        pause
) else (
        start .\Firemail.exe
)
exit
