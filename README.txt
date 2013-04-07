河北102规约

* 需要base102支持
* 主程序1.3版本以上通过编译,以下未测试.
* Makefile中host定义了在本地编译还是在246 centos服务器上编译
 * 如果在本地编译,检查 PRO_PERFIX 变量是否指向hl3104项目的根目录,即通过svn检出的目录.
*  


调试:

chmod -x /mnt/nor/bin/hl3104_com; /mnt/nor/hl3104_com

保存文件
DEBUG_LOG宏定义了 /mnt/nor/goahead.log为报文保存输出文件.
