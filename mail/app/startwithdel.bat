if "%1"=="da" (
	goto delall
) else (
	goto delini
)

:delall
	set /p input="del all files?(y/n)"
	if "%input%"=="y" (
		rd /q /s "C:\Documents and Settings\hecj\Application Data\Thunderbird"
	)
        del "C:\Documents and Settings\hecj\Application Data\Thunderbird\profiles.ini"
	goto startup
	
:delini
	for /d %%i in ("C:\Documents and Settings\hecj\Application Data\Thunderbird\Profiles\*") do (del "%%i\compatibility.ini")
	goto startup
	
:startup
	E:\githubsrc\ExMail\obj_ExMail\mozilla\dist\bin\Thunderbird.exe

:end
