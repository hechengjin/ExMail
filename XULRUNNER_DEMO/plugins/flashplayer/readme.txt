1、adobe官网下载并安装flash player插件。   http://www.adobe.com/cn/
2、将C:/windows/System32/Macromed/Flash下文件flashplayer.xpt和 NPSWF32.dll  [NPSWF32_18_0_0_186.dll]
复制到 C:/Program Files/Mozilla FireFox/plugins 针对XULRunner 复制到C:\xulrunner\plugins
3、更改C:/windows/System32/Macromed/Flash 下mms.cfg文件内容。具体方法如下：

  在非系统盘（如E盘）下建立mms.txt 内容为
     AutoUpdateDisable=0
     SilentAutoUpdateEnable=1
     ProtectedMode=0
  另存为mms.cfg ，并复制到C:/windows/System32/Macromed/Flash 覆盖原mms.cfg文件。