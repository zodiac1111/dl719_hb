###########################
# 河北dl719(102)规约 Makefile文件
###########################

#host如果本地编译则定义,如果使用192.168.1.246服务器编译则不定义(注销) 
host=1 
ifdef host
	#hl3104项目根目录,svn检出`svn co svn:192.168.1.246/hl3104`得到的
	PRO_PERFIX=/home/zodiac1111/hl3104
else
	PRO_PERFIX=../../../..
endif

#指示预处理/连接的头文件和库文件路径
INCS 	= -I$(PRO_PERFIX)/trunk/hl3104-v1.3/include \
	  -I$(PRO_PERFIX)/trunk/hl3104-v1.3/src/protocollib/base102/include \
	  -I./include
LIB 	= $(PRO_PERFIX)/trunk/hl3104-v1.3/lib
LIBS	= -L$(LIB) -lsys_utl -lbase102

CC	= arm-linux-g++
NAME	= Dl719ss
SOURCE	= src/Dl719ss.cpp
CXXFLAGS = -Os
LDFLAGS	= -s
MAKEDATE = \"$(shell date +%g:%m:%d:%H:%M)\"
MARCO	= -DMAKEDATE=$(MAKEDATE)  -DHL3104JD
wflag	= -Wfatal-errors # -Wall

##主目标 生成so动态库
all:$(NAME)
$(NAME):$(SOURCE)
	$(CC) $(LDFLAGS) $(CXXFLAGS) \
	-shared $(MARCO) $(INCS) $^ -o lib$@.so $(LIBS) $(wflag)

##清理目标
clean::
	rm -rf linux/*.so

##上传目标
#生成后上传:IP:192.168.1.189 用户名:anonymous 密码:holley  
#文件名:libDl719ss.so  上传至目录:lib/ (根据ftp开放的根目录不同可能有所区别)
update:all
	lftp -u anonymous,holley 192.168.1.189 \
	-e "put libDl719ss.so  -o nor/lib/;quit"
