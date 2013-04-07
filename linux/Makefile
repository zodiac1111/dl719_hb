host=1
ifdef host
#指定头文件路径(3个)和库文件路径 1.3
INCS = -I../include \
 -I/home/zodiac1111/mycheckout/hl3104/trunk/hl3104-v1.3/include \
  -I/home/zodiac1111/mycheckout/hl3104/trunk/hl3104-v1.3/src/protocollib/base102/include
LIB = /home/zodiac1111/mycheckout/hl3104/trunk/hl3104jd/软件/hl3104/lib

#指定头文件路径(3个)和库文件路径 
#INCS = -I../include \
#  -I/home/lee/test/mycheckout/hl3104/trunk/hl3104jd/软件/hl3104/include \
#  -I/home/lee/test/mycheckout/hl3104/trunk/hl3104jd/软件/hl3104/src/protocollib/base102/include
#LIB = /home/lee/test/mycheckout/hl3104/trunk/hl3104jd/软件/hl3104/lib

else

INCS = -I../include -I../../../../include -I../../base102/include
LIB = ../../../../lib
endif
LIBS    =       -L$(LIB) -lsys_utl -lbase102
CC	= arm-linux-g++
NAME    = Dl719ss
SOURCE  =       ../src/Dl719ss.cpp
CXXFLAGS = -DHL3104JD -Os
LDFLAGS = -s
MAKEDATE=\"$(shell date +%g:%m:%d:%H:%M)\"
MARCO = -DMAKEDATE=$(MAKEDATE)
all:$(NAME)
$(NAME):$(SOURCE)
	$(CC) $(LDFLAGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) $^ -o lib$@.so $(LIBS) # -Wall
#生成后上传, 用户名:anonymous 密码:holley IP:192.168.1.143 
#文件名:libDl719ss.so  上传至目录:lib/
	lftp -u anonymous,holley 192.168.1.189 -e "put libDl719ss.so  -o nor/lib/;quit"
