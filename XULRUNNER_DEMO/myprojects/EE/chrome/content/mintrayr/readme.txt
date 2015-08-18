动态库放到 EE\bin\lib下
tray_x86_64-msvc.dll
tray_x86-msvc.dll

调用 方法，在主窗口中加入
<?xul-overlay href="mintrayr/chrome/content/EE/titlemin.xul"?>

<image id="titlebar-buttons-minimize" onclick="gMinTrayR.minimize();" />  ---最小化按钮调用这个命令