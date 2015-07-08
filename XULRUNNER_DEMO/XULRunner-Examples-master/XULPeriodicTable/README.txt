The XUL Periodic Table's contents were downloaded from http://www.hevanet.com/acorbin/xul and modified slightly. The images were missing from the download so I replaced them with other images. There were a few typos in the XUL files that prevented them from rendering properly so I fixed them. The source tab in the viewer actually looks at whatever is in the .txt version of the specific XUL file you're looking at. These txt files may or may not reflect the actual source. I only updated the .txt files corresponding to the xul files I edited and may have missed one or two. It's best to look at the actual xul files in a text editor if you run into trouble though, the source tab looks just awesome and it's probably ok.

In the past this content could be viewed in firefox like any other webpage. That is no longer possible, so the content has been changed into a XUL Runner application.

If you want to use the bat files to launch this application on XUL Runner you will need to adjust the paths in them to suit your system and copy XUL Periodic Table.exe into the same folder where xulrunner.exe is. Of course you can launch this app by calling <path to xulrunner>\xulrunner.exe --app application.ini from this directory but, you won't get the fancy icon and convenient clicky bat file. If you're not using xulrunner 18.0.1 you'll have to recreate `XUL Periodic Table.exe` using a copy of `xulrunner-stub.exe` that came with your verson of xulrunner.

All XUL Periodic Table.exe is, is the xulrunner-stub.exe with an application icon injected into it using resourcehacker. The application icon is easy to create using gimp, creating each layer of the image as the different sizes of icon, and exporting the image as an icon. Use the resource hacker to pull the appicon out of the exe and look at it in gimp. It's really easy to make.

http://angusj.com/resourcehacker/
http://www.gimp.org/tutorials/Creating_Icons/

You can also create shortcuts to the bat files and modify the shortcuts's properties so they'll use the icon from XUL Periodic Table.exe. You can specify that the bat file is to be launched minimized so it doesn't flash across the screen.

Have fun!


Kastor

☭ Hial Atropa!! ☭

