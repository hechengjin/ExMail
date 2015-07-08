if "%1"=="da" (
	goto delall
) else (
	goto delini
)

:delall
	set /p input="del all files?(y/n)"
	if "%input%"=="y" (
		rd /q /s "C:\Users\Administrator\AppData\Roaming\Firemail"
	)
        del "C:\Users\Administrator\AppData\Roaming\Firemail\profiles.ini"
	goto startup
	
:delini
	for /d %%i in ("C:\Users\Administrator\AppData\Roaming\Firemail\Profiles\*") do (del "%%i\compatibility.ini")
	goto startup
	
:startup
	E:\github\ExMail\obj_Firemail\mozilla\dist\bin\Firemail.exe

:end
