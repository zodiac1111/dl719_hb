/**
 * @file heb102.cpp (Dl719ss.cpp)
 * 河北电力719(ice-60870-102)规约实现,继承base102.规约文本见doc/目录
 */
#include "Dl719ss.h"
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include "define.h"
#include "sys_utl.h"
#include "loopbuf.h"
#include "rtclocklib.h"
#include "HisDB.h"
#include <unistd.h>
#include "log.h"
#include "Hisfile.h"
#include "color.h"
#include <time.h>
//版本信息,通过宏定义版本号/ GET_DLL_VERSION在
#define PROTOCOL_VERSION 0x102
GET_DLL_VERSION
//unsigned char ERTU_TIME_CHECK;
int MsgQid;
unsigned char Main_Update_OK;

void CDl719s::Clear_Continue_Flag()
{
	Command = 	C_NULL;
	m_TI = C_NULL;
	m_ACD = C_NULL;
	Continue_Flag = 0;
	Continue_Step_Flag = 0;
	Send_DataEnd = 0;
	c_TI_tmp = 0xff;
	c_FCB_Tmp = 0xff;
	m_Resend = 0;
	printf("m_ACD=%d \n", m_ACD);
}

void CDl719s::Clear_FrameFlags()
{
	Command = 0;
	c_TI = 0;
	m_ACD = 0;
	m_Resend = 0;
}

void CDl719s::Read_Command_Time(unsigned char * data)
{
	if (data[7]==C_LTOU_TA_2) {
		c_Start_MON = data[18]&0x0f;
		c_Start_YL = data[19]&0x7f;
		c_End_MON = data[23]&0x0f;
		c_End_YL = data[24]&0x7f;
		if (c_Start_MON==12) {
			c_Start_MON = 1;
			c_Start_YL++;
		}else{
			c_Start_MON++;
		}
		if (c_End_MON==12) {
			c_End_MON = 1;
			c_End_YL++;
		}
		else
			c_Start_MON++;
	}
	if (data[7]==C_SP_NB_2) {
		c_Start_MIN = data[13]&0x3f;
		c_Start_H = data[14]&0x1f;
		c_Start_D = data[15]&0x1f;
		c_Start_MON = data[16]&0x0f;
		c_Start_YL = data[17]&0x7f;
		c_End_MIN = data[18]&0x3f;
		c_End_H = data[19]&0x1f;
		c_End_D = data[20]&0x1f;
		c_End_MON = data[21]&0x0f;
		c_End_YL = data[22]&0x7f;
	}
	else {
		c_Start_MIN = data[15]&0x3f;
		c_Start_H = data[16]&0x1f;
		c_Start_D = data[17]&0x1f;
		c_Start_MON = data[18]&0x0f;
		c_Start_YL = data[19]&0x7f;
		c_End_MIN = data[20]&0x3f;
		c_End_H = data[21]&0x1f;
		c_End_D = data[22]&0x1f;
		c_End_MON = data[23]&0x0f;
		c_End_YL = data[24]&0x7f;
	}
	return;
}

CDl719s::CDl719s()
		: CBASE102()
{
	syn_char_num = 4;
	Syn_Head_Rece_Flag = 0;
	m_ACD = 0;
	Command = 0;
	m_Resend = 0;
	Continue_Flag = 0;
	Send_DataEnd = 0;
	Send_num = 0;
	Send_Times = 0;
	Send_RealTimes = 0;
	SendT_RealTimes = 0;
	Send_Total = 0;
	Info_Size = 0;
	m_Resend = 0;
	m_Now_Frame_Seq = 0;
	m_Now_Frame_Seq_tmp = 0;
	m_Frame_Total = 0;
	c_FCB = 0;
	c_FCB_Tmp = 0xff;
	c_TI_tmp = 0xff;
	memset(mirror_buf, 0, sizeof(mirror_buf));
#if DEBUG_LOG
	this->fp_log=fopen(HB719_debug_log,"a");
#endif
}

int CDl719s::Init(struct stPortConfig *tmp_portcfg)
{
	CBASE102::Init(tmp_portcfg);
	c_Link_Address_H = tmp_portcfg->m_ertuaddr>>8;
	c_Link_Address_L = (unsigned char) tmp_portcfg->m_ertuaddr;
	printf(LIB_DBG"tmp_portcfg->m_ertuaddr= %04X \n",tmp_portcfg->m_ertuaddr);
	m_checktime_valifalg = tmp_portcfg->m_checktime_valiflag;
	syn_loopbuff_ptr(&m_recvBuf);
	m_suppwd = tmp_portcfg->m_usrsuppwd;
	m_pwd1 = tmp_portcfg->m_usrpwd1;
	m_pwd2 = tmp_portcfg->m_usrpwd2;
	this->Sequence_number=0;
	return 0;
}

CDl719s::~CDl719s()
{
#if DEBUG_LOG
	fclose(this->fp_log);
#endif
}

int CDl719s::dl719_synchead(unsigned char * data)
{
	if ((*data==0x68)&&(*(data+3)==0x68)) {
		return 0;
	}else if ((*data==0x10)&&((*(data+2)==c_Link_Address_L)&&(*(data+3)==c_Link_Address_H)||(*(data+2)==0xff)&&(*(data+3)==0xff))) {
		return 0;
	}else{
		return -1;
	}
}

unsigned long CDl719s::RealTime_SSZ_Get(unsigned char Meter_No, unsigned char point)
{
	unsigned long value;
	if (Meter_No>MAXMETER||point>19){
		return 0;
	}
	if (point<3)
		value = m_meterData[Meter_No].m_wU[point];
	else if (point<6)
		value = m_meterData[Meter_No].m_wI[point-3];
	else if (point<10)
		value = m_meterData[Meter_No].m_iP[point-6];
	else if (point<14)
		value = m_meterData[Meter_No].m_wQ[point-10];
	else if (point<18)
		value = m_meterData[Meter_No].m_wPF[point-14];
	else
		value = m_meterData[Meter_No].m_cF;
	return value;
}

void CDl719s::Send_MFrame(unsigned char cot)
{
	unsigned char i, tmp_sum;

	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return;
	}

	memcpy(m_transBuf.m_transceiveBuf, &mirror_buf[1], mirror_buf[0]);
	m_transBuf.m_transceiveBuf[9] = cot;
	m_transBuf.m_transceiveBuf[4] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transCount = mirror_buf[0];
	m_LastSendBytes = m_transBuf.m_transCount;

	for (i = 4, tmp_sum = 0; i<mirror_buf[0]-2; i++) {
		tmp_sum += m_transBuf.m_transceiveBuf[i];
	}
	m_transBuf.m_transceiveBuf[mirror_buf[0]-2] = tmp_sum;
	return;
}

/*------------------------------------------------------------
实时分时电量和实时总电量
0-总电量 1-分时电量
 --------------------------------------------------------------*/
int CDl719s::M_IT_NA_2(unsigned char flag)
{
	unsigned char mtr_no, buffptr, i, info_no, j, sum;
	unsigned short info_total;
	unsigned int e_val, e_flag;
	struct m_tSystime systime;

	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	if (Send_DataEnd) {
		Clear_Continue_Flag();
		E5H_Yes();
		return 0;
	}
	if (!Continue_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		m_TI = flag ? M_IT_NA_B_2 : 2;
		if (c_Stop_Info>=sysConfig->meter_num*4) {
			c_Stop_Info = sysConfig->meter_num*4-1;
		}
		/*-----------------------------------------------

		 ------------------------------------------------*/
		if (c_Stop_Info<=c_Start_Info) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num*4) {
			Send_Total = sysConfig->meter_num*4;
		}

		m_IOA = c_Start_Info;

		Info_Size = flag ? (4*FEILVMAX+3) : 7;
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;

		Continue_Flag = 1;
		Send_RealTimes = 0;
	}

	if (Send_RealTimes<Send_Times-1) {
		m_VSQ = Send_num;
	} else {
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
		/*-------------------------------------------------
		 ־
		 ---------------------------------------------------*/
		Send_DataEnd = 1;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;
	printf(LIB_DBG"实时总(分时)电量 信息体地址:%d \n",c_Record_Addr);
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA/4;
		info_no = m_IOA%4;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA;
		if (!flag) {//总电量
			e_val = 1000*m_meterData[mtr_no].m_iTOU[5*info_no];
			e_flag = m_meterData[mtr_no].Flag_TOU&(1<<5*info_no);
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
			m_transBuf.m_transceiveBuf[buffptr++ ] =
			                e_flag ? 0 : 0x80;
			sum = 0;
			for (j = 0; j<6; j++)
				sum += m_transBuf.m_transceiveBuf[buffptr-6+j];
			m_transBuf.m_transceiveBuf[buffptr++ ] = sum;     //jiao yan he

		} else {//分时电量
			for (j = 0; j<FEILVMAX; j++) {
				e_val =
				                1000*m_meterData[mtr_no].m_iTOU[5*info_no+j];
				TransLong2BinArray(
				                &m_transBuf.m_transceiveBuf[buffptr],
				                e_val);
				buffptr += 4;
			}
			//e_flag=m_meterData[mtr_no].Flag_TOU&(0x0001f<<5*(m_IOA%4));
			m_transBuf.m_transceiveBuf[buffptr++ ] =
			                (m_meterData[mtr_no].Flag_TOU&(0x0001f<<5*(m_IOA%4))) ?
			                                0 : 0x80;
			sum = 0;
			for (j = 0; j<1+4*FEILVMAX+1; j++)
				sum +=
				                m_transBuf.m_transceiveBuf[buffptr-(2+4*FEILVMAX)+j];
			m_transBuf.m_transceiveBuf[buffptr++ ] = sum;     //jiao yan he

		}
		m_IOA++;
	}
	GetSystemTime_RTC(&systime);
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.min;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.hour;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.day;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.mon;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.year;
	sum = 0;
	for (i = 4; i<buffptr; i++) {
		sum += m_transBuf.m_transceiveBuf[i];
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	/*---------------------------------------
	 ?ѷ???֡??????
	 -----------------------------------------*/
	Send_RealTimes++;
	return 0;
}

/*------------------------------------------------------------
 历史电量 typ 120 一定时间范围和地址范围的电量信息 回复typ 2
 flag 0 总电量 1 分时电量+总电量
 根据RAD区分缓冲区1还是缓冲区2/3
 RAD 11 = 缓冲区1  = 4个基础总电量 正有 反有 正无 反无
 RAD 12(13) =缓冲区2(3) 22个扩展量
 --------------------------------------------------------------*/
int CDl719s::M_IT_NA_T2(unsigned char flag)
{
	unsigned char mtr_no, inf_no;
	unsigned char buffptr, i, j, e_quality, sum, Save_BZ, Save_Have;
	unsigned char Start_YL, Start_MON, Start_D, Start_MIN, Start_H;
	unsigned short Circle;//周期(总尖峰平谷+正有/反有
	unsigned short Circle_4R;//4象限
	unsigned char Save_Point, Save_XH, m;
	int ret;
	unsigned long T1, T2, T3, T4, e_val;
	unsigned char backTime[5], Save_Num, Save_XL[20], tempData[32];
	struct m_tSystime systime;
	float value;
	unsigned int val;
	printf(LIB_DBG YELLOW"************  进入  *************\n"_COLOR);
	if (m_Resend) {
		printf(LIB_DBG"重发\n");
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	//printf(LIB_DBG"120 指定地址范围和时间范围的电量信息\n");
	if (Send_DataEnd) {
		printf(LIB_DBG"结束数据传输\n");
		Clear_Continue_Flag();
		Send_MFrame(10);
		return 0;
	}
#if 1
	printf(LIB_DBG"缓冲区值(RAD):%d\n",this->c_Record_Addr);
	if(c_Record_Addr==0 ||c_Record_Addr==11){
		printf(LIB_DBG"缓冲区1\n");
		flag=0;
	}else if(c_Record_Addr==12){
		printf(LIB_DBG"缓冲区2\n");
		flag=1;
	}else if(c_Record_Addr==13){
		printf(LIB_DBG"缓冲区3\n");
		flag=1;
	}else{
		printf(LIB_DBG"缓冲区错误:%d 没有这个RAD(缓冲区)\n",c_Record_Addr);
		Clear_Continue_Flag();
		Send_MFrame(10);
		return -1000;
	}
#endif
	if (!Continue_Flag&&!Continue_Step_Flag) {
		printf(LIB_DBG"数据传输中\n");
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		printf(LIB_DBG"缓冲区值(RAD):%d\n",this->c_Record_Addr);
		if(c_Record_Addr==0 ||c_Record_Addr==11){
			printf(LIB_DBG"缓冲区1\n");
			flag=0;
		}else if(c_Record_Addr==12){
			printf(LIB_DBG"缓冲区2\n");
			flag=1;
		}else if(c_Record_Addr==13){
			printf(LIB_DBG"缓冲区3\n");
			flag=1;
		}else{
			printf(LIB_DBG"缓冲区错误:%d 没有这个RAD(缓冲区)\n",c_Record_Addr);
			Clear_Continue_Flag();
			Send_MFrame(10);
			return -1000;
		}
		//m_TI = flag ? M_IT_TA_B_2 : M_IT_TA_2;
		//不管缓冲区几都是typ2回复,通过rad区分数据类型
		m_TI = flag ? M_IT_TA_2 : M_IT_TA_2;
		//缓冲区1 4个基本量
		if (flag==0 && c_Stop_Info>=sysConfig->meter_num*4) {
			c_Stop_Info = sysConfig->meter_num*4;
		}
		//缓冲区2 22个扩展量
		if (flag==1 && c_Stop_Info>=sysConfig->meter_num*22) {
			c_Stop_Info = sysConfig->meter_num*22;
		}
		/*-----------------------------------------------
		 ioa范围判断
		 ------------------------------------------------*/
		if (c_Stop_Info<=c_Start_Info) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -1;
		}
		Send_Total = c_Stop_Info-c_Start_Info+1;
		//printf("Send_Total =%d\n",Send_Total);
		if (flag==0 &&  Send_Total>=sysConfig->meter_num*4) {
			Send_Total = sysConfig->meter_num*4;
		}
		if (flag==1 &&  Send_Total>=sysConfig->meter_num*22) {
			Send_Total = sysConfig->meter_num*22;
		}
		//printf("Send_Total =%d\n",Send_Total);
		m_IOA = c_Start_Info;     //20080302
		//Info_Size = flag ? (4*FEILVMAX+3) : 7;
		Info_Size = 7;
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;
		/*-------------------------------------------------------------
		 ??վ?ٻ???ʼʱ???ࡰ?��??ա???????
		 ??վ?ٻ?????ʱ???ࡰ?��??ա???????
		 ??ǰʱ?̾ࡰ?��??ա???????
		 ---------------------------------------------------------------*/
		GetSystemTime_RTC(&systime);
		T1 = Calc_Time_102(
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		T2 = Calc_Time_102(
		                c_End_MIN,
		                c_End_H,
		                c_End_D,
		                c_End_MON,
		                c_End_YL);
		T3 = Calc_Time_102(
		                systime.min,
		                systime.hour,
		                systime.day,
		                systime.mon,
		                systime.year);
		/*-----------------------------------------------
		 ????״̬??ʱ??????????
		 ------------------------------------------------*/
		if ((T1==0)||(T2==0)||(T3==0)) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -2;
		}
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		ret = Get_Valid_Start_Save_Minute(
		                &Start_YL,
		                &Start_MON,
		                &Start_D,
		                &Start_H,
		                &Start_MIN);
		if (ret<0) {
			Send_MFrame(10);
			//E5H_Yes();
			Clear_Continue_Flag();
			//printf("m_ACD=%d \n",m_ACD);
			return -3;
		}
		//保存的数据的起始时刻
		T4 = Calc_Time_102(
		                Start_MIN,
		                Start_H,
		                Start_D,
		                Start_MON,
		                Start_YL);
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/

		if ((T1>T3)||(T2<T4)||(T1>T2)) {
			Send_MFrame(10);
			// E5H_Yes();
			if(T1>T3){
				printf(LIB_DBG"T1>T3 起始时刻属于未来 [___] B \n");
			}
			if(T2<T4){
				printf(LIB_DBG"T2<T4 结束时刻小于最早记录时刻 E [___]\n");
			}
			if(T1>T2){
				printf(LIB_DBG"T1>T2 起始时刻大于结束时刻 End___Begin X \n");
			}
			printf(LIB_DBG"开始T1=%d \n结束T2=%d\n "
				"当前T3=%d \nT4=%d\n",T1,T2,T3,T4);
			printf(LIB_DBG"T1=%d-%d-%d %d:%d\n"
					,c_Start_YL,c_Start_MON,c_Start_D
					,c_Start_H,c_Start_MIN);
			printf(LIB_DBG"T2=%d-%d-%d %d:%d\n"
					,c_End_YL,c_End_MON,c_End_D,c_End_H,c_End_MIN);
			printf(LIB_DBG"T4=%d-%d-%d %d:%d\n"
				,Start_YL,Start_MON,Start_D,Start_H,Start_MIN);
			Clear_Continue_Flag();
			printf("m_ACD=%d \n", m_ACD);
			return -4;
		}
		/*-----------------------------------------------
		 ȷ????ȷ????ʼʱ???ͽ???ʱ??
		 ------------------------------------------------*/
		if (T1<=T4) {
			T1 = T4;
			c_Start_YL = Start_YL;
			c_Start_MON = Start_MON;
			c_Start_D = Start_D;
		}
		if (T2>=T3)
			T2 = T3;

		if (!flag){
			printf(LIB_DBG"----总电量\n");
			ret = GetFileName_Day(
			                &filename,
			                c_Start_MON,
			                c_Start_D,
			                c_Start_Info/4,
			                TASK_TE);
		}else{
			printf(LIB_DBG"====分时电量\n");
			ret = GetFileName_Day(
			                &filename_tou,
			                c_Start_MON,
			                c_Start_D,
			                c_Start_Info/22,
			                TASK_TOU);
		}
		/*-----------------------------------------------
		 ????״̬?????????ݿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -5;
		}
		if (!flag){
			Circle = Search_CircleDBS(
			                filename,
			                c_Start_YL,
			                c_Start_MON,
			                c_Start_D,
			                &Save_Num,
			                &Save_XL[0],
			                TASK_TE);
		}else{
			Circle = Search_CircleDBS(
			                filename_tou,
			                c_Start_YL,
			                c_Start_MON,
			                c_Start_D,
			                &Save_Num,
			                &Save_XL[0],
			                TASK_TOU);
		}
		/*-----------------------------------------------
		 ????״̬???洢???ڴ???
		 ------------------------------------------------*/
		if (Circle==0) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -6;
		}
		/*------------------------------------------------------------------------------
		 ?ҵ?????????????ʷ?ļ???ʼ?洢?㣺?????洢????Ϊ30???ӣ???ʼʱ??Ϊ17:35????ʵ????ʼ???ļ??洢??ӦΪ18:00;
		 ?ҵ?????????????ʷ?ļ??Ľ????洢?㣺?????洢????Ϊ30???ӣ?????ʱ??Ϊ22:55,??ʵ?ʽ??????ļ??洢??ӦΪ22:30??
		 ??ǰʱ????Ӧ??Ч?洢?㣬?????洢????Ϊ30???ӣ???ǰʱ??Ϊ19:27,??????ʱ???????洢??Ϊ19:00;
		 ?????ܲ???(ʱ????)
		 ??????ʷ??��????һ????????????ʼʱ??
		 --------------------------------------------------------------------------------*/
		c_CircleTime = Circle;
		if ((T1%Circle)!=0)
			T1 = T1-T1%Circle+Circle;
		if ((T2%Circle)!=0)
			T2 = T2-T2%Circle;
		if ((T3%Circle)!=0)
			T3 = T3-T3%Circle;

		if (T2<T3)
			Steps = (T2-T1)/Circle+1;
		else if (T2>=T3)
			Steps = (T3-T1)/Circle+1;
		printf(
		               LIB_DBG"In history te T1=%d,T2=%d.T3=%d,T4=%d,steps=%d \n",
		                T1,
		                T2,
		                T3,
		                T4,
		                Steps);
		c_HisStartT = T1;

		Continue_Flag = 0;
		Continue_Step_Flag = 0;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	/*---------------------------------------------------------
	 ????ѭ???????????γ??ϴ?????
	 -----------------------------------------------------------*/
	if (!Continue_Step_Flag&&!Continue_Flag) {
		Continue_Flag = 1;
		Continue_Step_Flag = 1;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	} else if (!Continue_Flag&&Continue_Step_Flag) {
		Continue_Flag = 1;
		Send_RealTimes = 0;
		m_IOA = c_Start_Info;     //20080302
		SendT_RealTimes++;
	} else if (Continue_Flag&&Continue_Step_Flag) {
		Send_RealTimes++;
	}
	/*----------------------------------------------------------
	 ????ѭ???????????γ?ѭ??????
	 ------------------------------------------------------------*/
	//printf("Send_RealTimes= %d, Send_Times= %d\n",Send_RealTimes,Send_Times);
	if (Send_RealTimes>=Send_Times-1) {
		if (SendT_RealTimes==Steps-1) {
			Send_DataEnd = 1;
		} else {
			Continue_Flag = 0;
		}
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
		//printf("Send_Total =%d\n",Send_Total);
	} else {
		m_VSQ = Send_num;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = logic_Ertu_lo;
	m_transBuf.m_transceiveBuf[buffptr++ ] = logic_Ertu_hi;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;

	c_HisSendT = c_HisStartT+c_CircleTime*SendT_RealTimes;     //??ʷ?洢??ʱ??
	Back_DataTime(c_HisSendT, &backTime[0]);     //?????õ???Ӧ??????ʱ??

	Start_MIN = backTime[0];
	Start_H = backTime[1];
	Start_D = backTime[2];
	Start_MON = backTime[3];
	Start_YL = backTime[4];
	printf(
	                LIB_DBG"Start_YL=%d,Start_MON=%d,Start_D=%d,Start_H=%d,Start_MIN=%d\n",
	                Start_YL,
	                Start_MON,
	                Start_D,
	                Start_H,
	                Start_MIN);
	//printf("m_VSQ %d\n",m_VSQ);
	//序列号
	this->Sequence_number++;
	//printf(LIB_DBG"\t\tSequence_number=%d\n",Sequence_number);
	int ov;
	if(Sequence_number>0b1111){
		ov=0b00100000;
		Sequence_number=0;
	}else{
		ov=0;
	}
	for (i = 0; i<m_VSQ; i++) {//一个一个数据循环,一帧内的循环量
		if(flag==0){//仅4个总电量的 缓冲区1
			mtr_no = (m_IOA-1)/4;
			inf_no = (m_IOA-1)%4;
		}else{//全部电量+部分瞬时量 缓冲区2/3
			mtr_no = (m_IOA-1)/22;
			inf_no = (m_IOA-1)%22;
		}
		//信息体地址,ioa字节仅取1-255 多的进位的有逻辑设备地址logic ertu确定
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA%256; //20080302
		//获取储存文件名
		if (flag==0){//电量4个量
			ret = GetFileName_Day(
			                &filename,
			                Start_MON,
			                Start_D,
			                mtr_no,
			                TASK_TE);
		}else{//其他各种电量 22个量
			switch(inf_no+1){
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
				ret = GetFileName_Day(
				                &filename_tou,
				                Start_MON,
				                Start_D,
				                mtr_no,
				                TASK_TOU);
				break;
			case 11:
			case 12:
			case 13:
			case 14:
				ret = GetFileName_Day(
				                &filename_qr,
				                Start_MON,
				                Start_D,
				                mtr_no,
				                TASK_QR);
				break;
			case 15://最大需量
				ret = GetFileName_Day(
				                &filename_mnt,
				                Start_MON,
				                Start_D,
				                mtr_no,
				                TASK_MNT);
				break;
			case 16://瞬时量 包括3相电流+3相电压 1个有功功率总
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
				ret = GetFileName_Day(
				                &filename_ta,
				                Start_MON,
				                Start_D,
				                mtr_no,
				                TASK_TA);
				break;
			default:
				printf(LIB_ERR"错误的 缓冲区2 项目\n");
				break;
			}

		}
#if 1
		printf(LIB_DBG"信息体数目m_VSQ:%d/%d ,总信息体地址:%d 字节:%d\n"
			LIB_DBG	"表号 %d 表内增量 %d\n"
			LIB_DBG	"时刻 %d-%d-%d %d:%d\n"
			,i,m_VSQ,m_IOA,m_IOA%256,
			mtr_no,inf_no
			,backTime[4], backTime[3], backTime[2], backTime[1], backTime[0]
			);
#endif
		//printf("TE history trans filename is %s\n", filename.c_str());
		/*-----------------------------------------------
		 ????״̬???ļ??????ڻ????Ѿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -7;
		}

		if (!flag){
			printf(LIB_DBG"总电量 查询周期\n");
			Circle = Search_CircleDBS(
			                filename,
			                Start_YL,
			                Start_MON,
			                Start_D,
			                &Save_Num,
			                Save_XL,
			                TASK_TE);
		}else{
			switch(inf_no+1){
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
				printf(LIB_DBG"分时电量 查询周期 项目号:%d\n",inf_no+1);
				Circle = Search_CircleDBS(
						filename_tou,
				                Start_YL,
				                Start_MON,
				                Start_D,
				                &Save_Num,
				                Save_XL,
				                TASK_TOU);
				break;
			case 11:
			case 12:
			case 13:
			case 14:
				Circle = Search_CircleDBS(
						filename_qr,
				                Start_YL,
				                Start_MON,
				                Start_D,
				                &Save_Num,
				                Save_XL,
				                TASK_QR);
				printf(LIB_DBG"4象限无功 查询周期 项目号:%d\n"
					LIB_DBG"文件名:%s 周期:%d\n"
					,inf_no+1,filename_qr.c_str(),Circle);
				break;
			case 15:
				printf(LIB_INF"正向有功最大需量\n");
				Circle = Search_CircleDBS(
						filename_mnt,
				                Start_YL,
				                Start_MON,
				                Start_D,
				                &Save_Num,
				                &Save_XL[0],
				                TASK_MNT);
				break;
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22://瞬时量 3个电压 3个电流 1个有功功率总
				Circle = Search_CircleDBS(
						filename_ta,
				                Start_YL,
				                Start_MON,
				                Start_D,
				                &Save_Num,
				                Save_XL,
				                TASK_TA);
				break;
			default:
				printf(LIB_ERR"错误的 缓冲区2 周期项目\n");
				Circle=c_CircleTime;
				break;
			}
			//printf(LIB_DBG"分时电量 查询周期\n");
		}
		///储存周期有问题
		if (!Circle||(Circle!=c_CircleTime)) {
			printf(LIB_ERR"存储周期 c_CircleTime=%d Circle=%d\n"
					,c_CircleTime,Circle);
			Clear_Continue_Flag();
			Send_MFrame(10);
			return -8;
		}
		if (!flag) { //总电量
			GetDnlSSlDataDBS(
			                tempData,
			                filename,
			                Start_MIN,
			                Start_H,
			                inf_no,
			                c_CircleTime,
			                Save_Num,
			                TASK_TE);
			memcpy(&value, tempData, 4);
			val = value*1000;
			printf(LIB_DBG"总电量 取数据 val=%d\n",val);
			memcpy(tempData, &val, 4);
			memcpy(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                tempData,
			                5);
			buffptr += 5;
			/** 数据有效字节
			 * 7  6  5  4  3 2 1 0
			 * IV,CA,CY,LW  序列号
			 */
			//数据有效,溢出,序列号位
			m_transBuf.m_transceiveBuf[buffptr-1]|=this->Sequence_number;
			//printf(LIB_INF"\t\tSequence_number=%d [%X]\n"
			//	,Sequence_number,m_transBuf.m_transceiveBuf[buffptr-1]);
			m_transBuf.m_transceiveBuf[buffptr-1]|=ov;
			//printf(LIB_INF"\t\tSequence_number=%d [%X]\n"
			//	,Sequence_number,m_transBuf.m_transceiveBuf[buffptr-1]);
			sum = 0;
			//电量校验和
#define  TI_CHK_SUM 2 //是否需要电能量数据保护校验
#if TI_CHK_SUM ==1
			for (j = 0; j<6; j++){
				sum += m_transBuf.m_transceiveBuf[buffptr-6+j];
			}

#elif TI_CHK_SUM==2 //另一种校验和方式,多种数据一起校验,TODO
			sum=m_TI+logic_Ertu_lo+logic_Ertu_hi+c_Record_Addr;
			sum+=backTime[0];
			sum+=backTime[1];
			sum+=backTime[2];
			sum+=backTime[3];
			sum+=backTime[4];
			for(j=0;j<6;j++){
				sum+=m_transBuf.m_transceiveBuf[buffptr-6+j];
			}
#endif
			m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
		}else {//第2缓冲区
			//int item_offset=inf_no+1;
			unsigned char ivByte=80;//有效字节
			printf(LIB_INF"ioa 项目地址:%d/%d 项目号:%d\n"
					,m_IOA,Send_Total,inf_no+1);
			switch(inf_no+1){
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10://
				printf(LIB_INF"正反向总+分时系列2*5个\n");
				GetDnlSSlDataDBS(
					tempData,
					filename_tou,
					Start_MIN,
					Start_H,
					inf_no,
					c_CircleTime,
					Save_Num,
					TASK_TOU);
				break;
			case 11://因为4个无功功率排列不同.所以一个一个区分
				printf(LIB_INF"象限无功4个 no1 file:%s\n",filename_qr.c_str());
				GetDnlSSlDataDBS(
					tempData,
					filename_qr,
					Start_MIN,
					Start_H,
					0,//一个项目内的偏移,象限无功 4(象限)*5(总尖..)=20项
					//第一维:象限 [1 4 2 3] 象限
					//第2维:分时 [总 尖 峰 平 谷]
					c_CircleTime,
					Save_Num,
					TASK_QR);
				break;
			case 12:
				printf(LIB_INF"象限无功4个 no2 file:%s\n",filename_qr.c_str());
				GetDnlSSlDataDBS(
					tempData,
					filename_qr,
					Start_MIN,
					Start_H,
					10,
					c_CircleTime,
					Save_Num,
					TASK_QR);
				break;
			case 13:
				printf(LIB_INF"象限无功4个 no3 file:%s\n",filename_qr.c_str());
				GetDnlSSlDataDBS(
					tempData,
					filename_qr,
					Start_MIN,
					Start_H,
					15,//一个项目内的偏移
					c_CircleTime,
					Save_Num,
					TASK_QR);
				break;
			case 14:
				printf(LIB_INF"象限无功4个 no4 file:%s\n",filename_qr.c_str());
				GetDnlSSlDataDBS(
					tempData,
					filename_qr,
					Start_MIN,
					Start_H,
					5,
					c_CircleTime,
					Save_Num,
					TASK_QR);
				break;
			case 15:
				printf(LIB_INF"正向有功最大需量\n");
				GetMntDataDBS(
				                tempData,
				                filename_mnt,
				                Start_MIN,
				                Start_H,
				                0,
				                c_CircleTime,
				                Save_Num,
				                TASK_MNT);
				//需量单个结构 10 字节:
				// 年 月 日 时 分 (4字节数据) 1字节有效 1字节校验
				//memcpy(tempData,tempData+5,4);
				printf(LIB_INF"Save_XH=%d *****最大需量",Save_XH);
				int k;
				for(k=0;k<32;k++){
					printf(" %02X",tempData[k]);
				}
				printf("\n");
				break;
			case 16://顺序不对区分case
				printf(LIB_INF"有功功率总\n");
				GetDnlSSlDataDBS(
					tempData,
					filename_ta,
					Start_MIN,
					Start_H,
					6,//Vabc +Iabc=6个
					c_CircleTime,
					Save_Num,
					TASK_TA);
				break;
			case 17:
			case 18:
			case 19:
				printf(LIB_INF"电流3相\n");
				GetDnlSSlDataDBS(
					tempData,
					filename_ta,
					Start_MIN,
					Start_H,
					//项目内部偏移3,前面有3个电压,
					//缓冲区偏移17(从1开始).
					3+(inf_no+1)-17,
					c_CircleTime,
					Save_Num,
					TASK_TA);
				break;
			case 20:
			case 21:
			case 22:
				printf(LIB_INF"电压3相\n");
				GetDnlSSlDataDBS(
					tempData,
					filename_ta,
					Start_MIN,
					Start_H,
					//项目内部偏移0,每个项目有电压开始的
					//缓冲区偏移20(从1开始).
					0+(inf_no+1)-20,
					c_CircleTime,
					Save_Num,
					TASK_TA);
				break;
			default:
				printf(LIB_ERR"错误的 缓冲区2 项目\n");
				memset(tempData,0x00,4);
				break;
			}
			Save_BZ = 0x80;//默认是无效
			if((inf_no+1)==15){//只有最大需量的有效字节在后面
				Save_BZ=tempData[9];
			}else{//其他电量的有效字节就跟在4字节电量之后
				Save_BZ=tempData[4];
			}
			if((Save_BZ&0b10000000)/*最高位iv*/!=0){//数据无效
				memset(tempData,0x00,4);
			}
			/*GetDnlSSlDataDBS(
			                tempData,
			                filename,
			                Start_MIN,
			                Start_H,
			                inf_no,
			                c_CircleTime,
			                Save_Num,
			                TASK_TOU);
			*/
			memcpy(&value, tempData, 4);
			val = value*1000;
			printf(LIB_DBG"项目号:%d val=%.3f\n"
				,inf_no+1,value);
			memcpy(tempData, &val, 4);
			memcpy(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                tempData,
			                5);
			buffptr += 5;
			/** 数据有效字节
			 * 7  6  5  4  3 2 1 0
			 * IV,CA,CY,LW  序列号
			 */
			//数据有效,溢出,序列号位
			m_transBuf.m_transceiveBuf[buffptr-1]=Save_BZ;//有效字节
			m_transBuf.m_transceiveBuf[buffptr-1]|=this->Sequence_number;
			m_transBuf.m_transceiveBuf[buffptr-1]|=ov;
#if 0
			for (j = 0; j<FEILVMAX; j++) {
				Save_Point = inf_no*FEILVMAX+j;
				Save_Have = 0;
				for (m = 0; m<Save_Num; m++) {
					if (Save_XL[m]==Save_Point) {
						Save_Have = 1;
						Save_XH = Save_Point;
						break;
					}
				}
				printf(
				                "#######In tou history save_Have is %d ,save_num is %d######\n",
				                Save_Have,
				                Save_Num);
				if (Save_Have){
					GetDnlSSlDataDBS(
					                tempData+j*5,
					                filename,
					                Start_MIN,
					                Start_H,
					                Save_XH,
					                c_CircleTime,
					                Save_Num,
					                TASK_TOU);
				} else {
					memset(tempData+j*5, 0, 5);
					tempData[j*5+4] = 0x80;
				}
				memcpy(&value, tempData+j*5, 4);
				val = value*1000;
				memcpy(tempData+j*5, &val, 4);
				memcpy(
				                &m_transBuf.m_transceiveBuf[buffptr],
				                tempData+j*5,
				                4);
				buffptr += 4;
			}
			m_transBuf.m_transceiveBuf[buffptr++ ] =
			                tempData[4]&tempData[9]&tempData[14]&tempData[19]&tempData[24];
#endif
			sum = 0;
			//电量校验位
			sum=m_TI+logic_Ertu_lo+logic_Ertu_hi+c_Record_Addr;
			sum+=backTime[0];
			sum+=backTime[1];
			sum+=backTime[2];
			sum+=backTime[3];
			sum+=backTime[4];
			for(j=0;j<6;j++){
				sum+=m_transBuf.m_transceiveBuf[buffptr-6+j];
			}
			m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
		}
		m_IOA++;
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[0];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[1];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[2];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[3];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[4];
	//链路校验位
	sum = 0;
	for (i = 4; i<buffptr; i++)
		sum += m_transBuf.m_transceiveBuf[i];
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;
}
/*========== ============== ????˲ʱ��(171/161)==========================*/

/*------------------------------------------------------------
 实时遥测电量
  ÿ????Ϣ????��????????ң??��??3U,3I,4P,4Q,4PF,1F
 ÿ????Ϣ????1?ֽ???Ϣ????ַ+19*4?ֽ?????+1?ֽ?Ʒ??+1?ֽ?????У??????
 --------------------------------------------------------------*/
int CDl719s::M_SSZ_NA_2()
{
	unsigned char mtr_no, buffptr, i, info_no, e_flag, j, sum;
	unsigned short info_total;
	unsigned int e_val;
	struct m_tSystime systime;

	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	if (Send_DataEnd) {
		Clear_Continue_Flag();
		Send_MFrame(10);
		//E5H_Yes();
		return 0;
	}
	if (!Continue_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		m_TI = M_YC_NA_2;
		if (c_Stop_Info>=sysConfig->meter_num) {
			c_Stop_Info = sysConfig->meter_num-1;
		}
		/*-----------------------------------------------
		 ????״̬????Ϣ????ַ??Χ????
		 ------------------------------------------------*/
		if (c_Stop_Info<c_Start_Info) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num) {
			Send_Total = sysConfig->meter_num;
		}

		m_IOA = c_Start_Info-1;     //20080302

		Info_Size = 1+19*4+1+1;
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;

		Continue_Flag = 1;
		Send_RealTimes = 0;
	}

	if (Send_RealTimes<Send_Times-1) {
		m_VSQ = Send_num;
	} else {
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
		/*-------------------------------------------------
		 ???????ݴ??????ϱ?־
		 ---------------------------------------------------*/
		Send_DataEnd = 1;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA+1;
		for (j = 0; j<3; j++) {
			e_val = 10*m_meterData[mtr_no].m_wU[j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
		}
		for (j = 0; j<3; j++) {
			e_val = 1000*m_meterData[mtr_no].m_wI[j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
		}
		for (j = 0; j<4; j++) {
			e_val = 1000*m_meterData[mtr_no].m_iP[j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
		}
		for (j = 0; j<4; j++) {
			e_val = 1000*m_meterData[mtr_no].m_wQ[j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
		}
		for (j = 0; j<4; j++) {
			e_val = 1000*m_meterData[mtr_no].m_wPF[j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
		}
		e_val = 1000*m_meterData[mtr_no].m_cF;
		TransLong2BinArray(&m_transBuf.m_transceiveBuf[buffptr], e_val);
		buffptr += 4;
		m_transBuf.m_transceiveBuf[buffptr++ ] = 0;
		m_transBuf.m_transceiveBuf[buffptr++ ] = 0;
		m_IOA++;
	}
	GetSystemTime_RTC(&systime);
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.min;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.hour;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.day;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.mon;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.year;
	sum = 0;
	for (i = 4; i<buffptr; i++) {
		sum += m_transBuf.m_transceiveBuf[i];
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	/*---------------------------------------
	 ?ѷ???֡??????
	 -----------------------------------------*/
	Send_RealTimes++;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;
}
/*------------------------------------------------------------
 历史遥测电量
  ÿ????Ϣ????��????????ң??��??3U,3I,4P,4Q,4PF,1F
 ÿ????Ϣ????1?ֽ???Ϣ????ַ+19*4?ֽ?????+1?ֽ?Ʒ??+1?ֽ?????У??????
 --------------------------------------------------------------*/
int CDl719s::M_SSZ_NA_T2()
{
	unsigned char mtr_no, inf_no;
	unsigned char buffptr, i, j, e_quality, sum, Save_BZ, Save_Have;
	unsigned char Start_YL, Start_MON, Start_D, Start_MIN, Start_H;
	unsigned short Circle;
	unsigned char Save_Point, Save_XH, m;
	int ret;
	unsigned long T1, T2, T3, T4, e_val;
	unsigned char backTime[5], Save_Num, Save_XL[20], tempData[5];
	struct m_tSystime systime;
	float value;
	unsigned int val;
	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}

	if (Send_DataEnd) {
		Clear_Continue_Flag();
		Send_MFrame(10);
		//E5H_Yes();
		return 0;
	}

	if (!Continue_Flag&&!Continue_Step_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 05;
		m_TI = M_YC_TA_2;
		if (c_Stop_Info>=sysConfig->meter_num-1) {
			c_Stop_Info = sysConfig->meter_num-1;
		}
		/*-----------------------------------------------
		 ????״̬????Ϣ????ַ??Χ????
		 ------------------------------------------------*/
		if (c_Stop_Info<c_Start_Info) {     //=
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num) {
			Send_Total = sysConfig->meter_num;
		}

		m_IOA = c_Start_Info;
		Info_Size = 1+19*4+1+1;
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;
		/*-------------------------------------------------------------
		 ??վ?ٻ???ʼʱ???ࡰ?��??ա???????
		 ??վ?ٻ?????ʱ???ࡰ?��??ա???????
		 ??ǰʱ?̾ࡰ?��??ա???????
		 ---------------------------------------------------------------*/
		printf(
		                "In TA history trans c_Start_MIN=%d,c_Start_H=%d,c_Start_D=%d,c_Start_MON=%d,c_Start_YL=%d\n",
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		printf(
		                "In TA history trans c_End_MIN=%d,c_End_H=%d,c_Start_D=%d,c_End_MON=%d,c_End_YL=%d\n",
		                c_End_MIN,
		                c_End_H,
		                c_Start_D,
		                c_End_MON,
		                c_End_YL);
		GetSystemTime_RTC(&systime);
		T1 = Calc_Time_102(
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		T2 = Calc_Time_102(
		                c_End_MIN,
		                c_End_H,
		                c_End_D,
		                c_End_MON,
		                c_End_YL);
		T3 = Calc_Time_102(
		                systime.min,
		                systime.hour,
		                systime.day,
		                systime.mon,
		                systime.year);
		/*-----------------------------------------------
		 ????״̬??ʱ??????????
		 ------------------------------------------------*/
		if ((T1==0)||(T2==0)||(T3==0)) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -2;
		}
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		ret = Get_Valid_Start_Save_Minute(
		                &Start_YL,
		                &Start_MON,
		                &Start_D,
		                &Start_H,
		                &Start_MIN);
		if (ret<0) {
			Send_MFrame(10);
			//E5H_Yes();
			Clear_Continue_Flag();
			return -3;
		}
		T4 = Calc_Time_102(
		                Start_MIN,
		                Start_H,
		                Start_D,
		                Start_MON,
		                Start_YL);
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		if ((T1>T3)||(T2<T4)||(T1>T2)) {
			Send_MFrame(10);
			// E5H_Yes();
			Clear_Continue_Flag();
			return -4;
		}
		/*-----------------------------------------------
		 ȷ????ȷ????ʼʱ???ͽ???ʱ??
		 ------------------------------------------------*/
		if (T1<=T4) {
			T1 = T4;
			c_Start_YL = Start_YL;
			c_Start_MON = Start_MON;
			c_Start_D = Start_D;
		}
		if (T2>=T3)
			T2 = T3;

		ret = GetFileName_Day(
		                &filename,
		                c_Start_MON,
		                c_Start_D,
		                c_Start_Info,
		                TASK_TA);

		/*-----------------------------------------------
		 ????״̬?????????ݿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -5;
		}

		Circle = Search_CircleDBS(
		                filename,
		                c_Start_YL,
		                c_Start_MON,
		                c_Start_D,
		                &Save_Num,
		                &Save_XL[0],
		                TASK_TA);

		/*-----------------------------------------------
		 ????״̬???洢???ڴ???
		 ------------------------------------------------*/
		if (Circle==0) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -6;
		}
		/*------------------------------------------------------------------------------
		 ?ҵ?????????????ʷ?ļ???ʼ?洢?㣺?????洢????Ϊ30???ӣ???ʼʱ??Ϊ17:35????ʵ????ʼ???ļ??洢??ӦΪ18:00;
		 ?ҵ?????????????ʷ?ļ??Ľ????洢?㣺?????洢????Ϊ30???ӣ?????ʱ??Ϊ22:55,??ʵ?ʽ??????ļ??洢??ӦΪ22:30??
		 ??ǰʱ????Ӧ??Ч?洢?㣬?????洢????Ϊ30???ӣ???ǰʱ??Ϊ19:27,??????ʱ???????洢??Ϊ19:00;
		 ?????ܲ???(ʱ????)
		 ??????ʷ??��????һ????????????ʼʱ??
		 --------------------------------------------------------------------------------*/
		c_CircleTime = Circle;
		if ((T1%Circle)!=0)
			T1 = T1-T1%Circle+Circle;
		if ((T2%Circle)!=0)
			T2 = T2-T2%Circle;
		if ((T3%Circle)!=0)
			T3 = T3-T3%Circle;

		if (T2<T3)
			Steps = (T2-T1)/Circle+1;
		else if (T2>=T3)
			Steps = (T3-T1)/Circle+1;

		c_HisStartT = T1;

		Continue_Flag = 0;
		Continue_Step_Flag = 0;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	/*---------------------------------------------------------
	 ????ѭ???????????γ??ϴ?????
	 -----------------------------------------------------------*/
	if (!Continue_Step_Flag&&!Continue_Flag) {
		Continue_Flag = 1;
		Continue_Step_Flag = 1;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	else if (!Continue_Flag&&Continue_Step_Flag) {
		Continue_Flag = 1;
		Send_RealTimes = 0;
		m_IOA = c_Start_Info;
		SendT_RealTimes++;
	}
	else if (Continue_Flag&&Continue_Step_Flag) {
		Send_RealTimes++;
	}
	/*----------------------------------------------------------
	 ????ѭ???????????γ?ѭ??????
	 ------------------------------------------------------------*/
	if (Send_RealTimes>=Send_Times-1) {
		if (SendT_RealTimes==Steps-1) {
			Send_DataEnd = 1;
		}
		else {
			Continue_Flag = 0;
		}
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
	}
	else {
		m_VSQ = Send_num;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;

	c_HisSendT = c_HisStartT+c_CircleTime*SendT_RealTimes;     //??ʷ?洢??ʱ??
	Back_DataTime(c_HisSendT, &backTime[0]);     //?????õ???Ӧ??????ʱ??

	Start_MIN = backTime[0];
	Start_H = backTime[1];
	Start_D = backTime[2];
	Start_MON = backTime[3];
	Start_YL = backTime[4];

	printf(
	                "Start_YL=%d,Start_MON=%d,Start_D=%d,Start_H=%d,Start_MIN=%d\n",
	                Start_YL,
	                Start_MON,
	                Start_D,
	                Start_H,
	                Start_MIN);
	printf(
	                "In ta history m_vsq=%d,send_realtimes=%d,sendt_realtimes=%d\n",
	                m_VSQ,
	                Send_RealTimes,
	                SendT_RealTimes);
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA-1;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA;

		ret = GetFileName_Day(
		                &filename,
		                Start_MON,
		                Start_D,
		                mtr_no,
		                TASK_TA);
		printf(
		                "#In yc history trans filename is %s#\n",
		                filename.c_str());
		/*-----------------------------------------------
		 ????״̬???ļ??????ڻ????Ѿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -7;
		}

		Circle = Search_CircleDBS(
		                filename,
		                Start_YL,
		                Start_MON,
		                Start_D,
		                &Save_Num,
		                Save_XL,
		                TASK_TA);

		/*-----------------------------------------------
		 ????״̬?????ݴ洢???ڹ???
		 ------------------------------------------------*/
		if (!Circle||(Circle!=c_CircleTime)) {
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			return -8;
		}
		printf("In TA history trans save_num is %d \n", Save_Num);
		for (j = 0; j<19; j++) {
			Save_Point = j;
			Save_Have = 0;
			for (m = 0; m<Save_Num; m++) {
				//if(Save_XL[m]==Save_Point){Save_Have=1;Save_XH=Save_Point;break;}
				if (Save_XL[m]==Save_Point) {
					Save_Have = 1;
					Save_XH = m;
					break;
				}
			}

			//printf("In TA history send savwe_have=%d\n",Save_Have);
			if (Save_Have)
				GetDnlSSlDataDBS(
				                tempData,
				                filename,
				                Start_MIN,
				                Start_H,
				                Save_XH,
				                c_CircleTime,
				                Save_Num,
				                TASK_TA);
			else {
				memset(tempData, 0, 4);
			}
			memcpy(&value, tempData, 4);
			val = value*1000;
			memcpy(tempData, &val, 4);/*----------------*/
			memcpy(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                tempData,
			                4);
			buffptr += 4;
		}

		m_transBuf.m_transceiveBuf[buffptr++ ] = 0;
		sum = 0;
		for (j = 0; j<1+4*19+1; j++)
			sum += m_transBuf.m_transceiveBuf[buffptr-(2+4*19)+j];
		m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
		m_IOA++;
	}

	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[0];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[1];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[2];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[3];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[4];
	sum = 0;
	for (i = 4; i<buffptr; i++)
		sum += m_transBuf.m_transceiveBuf[i];
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;
}

/*========== ======???͵?ǰ??��(173/163)==========================*/
/*------------------------------------------------------------
 ???͵?ǰ??��?ͷ???ʱ??
 ???????У????У????ޣ?????4????Ϣ?壬???ݰ?��?ܣ??⣬?壬ƽ????
 ÿ????Ϣ????1?ֽ???Ϣ????ַ+5*(4?ֽ???��+4?ֽ?ʱ??)+1?ֽ?Ʒ??+1?ֽ?????У??????
 --------------------------------------------------------------*/

int CDl719s::M_MaxND_NA_2()
{
	unsigned char mtr_no, buffptr, i, info_no, e_flag, j, sum;
	unsigned short info_total;
	unsigned int e_val;
	struct m_tSystime systime;
	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	if (Send_DataEnd) {
		Clear_Continue_Flag();
		E5H_Yes();
		return 0;
	}
	if (!Continue_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		m_TI = M_MNT_NA_2;
		if (c_Stop_Info>=sysConfig->meter_num*4) {
			c_Stop_Info = sysConfig->meter_num*4-1;
		}
		/*-----------------------------------------------
		 ????״̬????Ϣ????ַ??Χ????
		 ------------------------------------------------*/
		if (c_Stop_Info<=c_Start_Info) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num*4) {
			Send_Total = sysConfig->meter_num*4;
		}

		m_IOA = c_Start_Info;

		Info_Size = (9*FEILVMAX+3);
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;

		Continue_Flag = 1;
		Send_RealTimes = 0;
	}

	if (Send_RealTimes<Send_Times-1) {
		m_VSQ = Send_num;
	}
	else {
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
		/*-------------------------------------------------
		 ???????ݴ??????ϱ?־
		 ---------------------------------------------------*/
		Send_DataEnd = 1;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;
	GetSystemTime_RTC(&systime);
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA/4;
		info_no = m_IOA%4;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA;

		for (j = 0; j<FEILVMAX; j++) {
			e_val = 1000*m_meterData[mtr_no].m_iMaxN[5*info_no+j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
			e_val = m_meterData[mtr_no].m_iMaxNT[5*info_no+j];
			//TransLong2BinArray(&m_transBuf.m_transceiveBuf[buffptr], e_val);
			TransLong2BinArray5(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val,
			                systime.year);
			buffptr += 5;
		}
		//e_flag=m_meterData[mtr_no].Flag_MN&(0x0001f<<((m_IOA%4)*5));
		m_transBuf.m_transceiveBuf[buffptr++ ] =
		                (m_meterData[mtr_no].Flag_MN&(0x0001f<<((m_IOA%4)*5))) ?
		                                0 : 0x80;
		m_transBuf.m_transceiveBuf[buffptr++ ] = 0;
		m_IOA++;
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.min;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.hour;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.day;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.mon;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.year;
	sum = 0;
	for (i = 4; i<buffptr; i++) {
		sum += m_transBuf.m_transceiveBuf[i];
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	/*---------------------------------------
	 ?ѷ???֡??????
	 -----------------------------------------*/
	Send_RealTimes++;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;

}

/******************************??????ʷ??��(174/164)*****************************/
/*------------------------------------------------------------
 ???͵?ǰ??��?ͷ???ʱ??
 ???????У????У????ޣ?????4????Ϣ?壬???ݰ?��?ܣ??⣬?壬ƽ????
 ÿ????Ϣ????1?ֽ???Ϣ????ַ+5*(4?ֽ???��+4?ֽ?ʱ??)+1?ֽ?Ʒ??+1?ֽ?????У??????
 --------------------------------------------------------------*/
int CDl719s::M_MaxND_NA_T2()
{
	unsigned char mtr_no, inf_no;
	unsigned char buffptr, i, j, e_quality, sum, Save_BZ, Save_Have;
	unsigned char Start_YL, Start_MON, Start_D, Start_MIN, Start_H;
	unsigned short Circle;
	unsigned char Save_Point, Save_XH, m;
	int ret;
	unsigned long T1, T2, T3, T4, e_val[2];
	unsigned char backTime[5], Save_Num, Save_XL[20], tempData[64];
	unsigned char test;
	struct m_tSystime systime;
	float value;
	unsigned int val;
	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	//printf("In MNT history continueflag is %d,continue_step_flag is %d,send_dataend is %d\n",Continue_Flag,Continue_Step_Flag,Send_DataEnd);
	if (Send_DataEnd) {
		Clear_Continue_Flag();
		//E5H_Yes();
		Send_MFrame(10);
		return 0;
	}

	if (!Continue_Flag&&!Continue_Step_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		m_TI = M_MNT_TA_2;
		if (c_Stop_Info>=sysConfig->meter_num*4) {
			c_Stop_Info = sysConfig->meter_num*4-1;
		}
		/*-----------------------------------------------
		 ????״̬????Ϣ????ַ??Χ????
		 ------------------------------------------------*/
		if (c_Stop_Info<=c_Start_Info) {
			Clear_Continue_Flag();
			//E5H_Yes();
			Send_MFrame(10);
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num*4) {
			Send_Total = sysConfig->meter_num*4;
		}

		m_IOA = c_Start_Info;

		Info_Size = (9*FEILVMAX+3);
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;
		/*-------------------------------------------------------------
		 ??վ?ٻ???ʼʱ???ࡰ?��??ա???????
		 ??վ?ٻ?????ʱ???ࡰ?��??ա???????
		 ??ǰʱ?̾ࡰ?��??ա???????
		 ---------------------------------------------------------------*/
		GetSystemTime_RTC(&systime);
		printf(
		                "In mnt history trans c_Start_MIN=%d,c_Start_H=%d,c_Start_D=%d,c_Start_MON=%d,c_Start_YL=%d\n",
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		printf(
		                "In mnt history trans c_End_MIN=%d,c_End_H=%d,c_Start_D=%d,c_End_MON=%d,c_End_YL=%d\n",
		                c_End_MIN,
		                c_End_H,
		                c_Start_D,
		                c_End_MON,
		                c_End_YL);
		T1 = Calc_Time_102(
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		T2 = Calc_Time_102(
		                c_End_MIN,
		                c_End_H,
		                c_End_D,
		                c_End_MON,
		                c_End_YL);
		T3 = Calc_Time_102(
		                systime.min,
		                systime.hour,
		                systime.day,
		                systime.mon,
		                systime.year);
		/*-----------------------------------------------
		 ????״̬??ʱ??????????
		 ------------------------------------------------*/
		if ((T1==0)||(T2==0)||(T3==0)) {
			Clear_Continue_Flag();
			//E5H_Yes();
			Send_MFrame(10);
			return -2;
		}
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		ret = Get_Valid_Start_Save_Minute(
		                &Start_YL,
		                &Start_MON,
		                &Start_D,
		                &Start_H,
		                &Start_MIN);
		if (ret<0) {
			//E5H_Yes();
			Send_MFrame(10);
			Clear_Continue_Flag();
			return -3;
		}
		T4 = Calc_Time_102(
		                Start_MIN,
		                Start_H,
		                Start_D,
		                Start_MON,
		                Start_YL);
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		if ((T1>T3)||(T2<T4)||(T1>T2)) {
			//E5H_Yes();
			Send_MFrame(10);
			Clear_Continue_Flag();
			return -4;
		}
		/*-----------------------------------------------
		 ȷ????ȷ????ʼʱ???ͽ???ʱ??
		 ------------------------------------------------*/
		if (T1<=T4) {
			T1 = T4;
			c_Start_YL = Start_YL;
			c_Start_MON = Start_MON;
			c_Start_D = Start_D;
		}
		if (T2>=T3)
			T2 = T3;

		ret = GetFileName_Day(
		                &filename,
		                c_Start_MON,
		                c_Start_D,
		                c_Start_Info/4,
		                TASK_MNT);
		printf(
		                "In mnt history trans filename is %s\n",
		                filename.c_str());
		/*-----------------------------------------------
		 ????״̬?????????ݿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			//E5H_Yes();
			Send_MFrame(10);
			return -5;
		}

		Circle = Search_CircleDBS(
		                filename,
		                c_Start_YL,
		                c_Start_MON,
		                c_Start_D,
		                &Save_Num,
		                &Save_XL[0],
		                TASK_MNT);
		/*-----------------------------------------------
		 ????״̬???洢???ڴ???
		 ------------------------------------------------*/
		if (Circle==0) {
			Clear_Continue_Flag();
			//E5H_Yes();
			Send_MFrame(10);
			return -6;
		}
		/*------------------------------------------------------------------------------
		 ?ҵ?????????????ʷ?ļ???ʼ?洢?㣺?????洢????Ϊ30???ӣ???ʼʱ??Ϊ17:35????ʵ????ʼ???ļ??洢??ӦΪ18:00;
		 ?ҵ?????????????ʷ?ļ??Ľ????洢?㣺?????洢????Ϊ30???ӣ?????ʱ??Ϊ22:55,??ʵ?ʽ??????ļ??洢??ӦΪ22:30??
		 ??ǰʱ????Ӧ??Ч?洢?㣬?????洢????Ϊ30???ӣ???ǰʱ??Ϊ19:27,??????ʱ???????洢??Ϊ19:00;
		 ?????ܲ???(ʱ????)
		 ??????ʷ??��????һ????????????ʼʱ??
		 --------------------------------------------------------------------------------*/
		c_CircleTime = Circle;
		if ((T1%Circle)!=0)
			T1 = T1-T1%Circle+Circle;
		if ((T2%Circle)!=0)
			T2 = T2-T2%Circle;
		if ((T3%Circle)!=0)
			T3 = T3-T3%Circle;

		if (T2<T3)
			Steps = (T2-T1)/Circle+1;
		else if (T2>=T3)
			Steps = (T3-T1)/Circle+1;

		c_HisStartT = T1;

		Continue_Flag = 0;
		Continue_Step_Flag = 0;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	/*---------------------------------------------------------
	 ????ѭ???????????γ??ϴ?????
	 -----------------------------------------------------------*/
	if (!Continue_Step_Flag&&!Continue_Flag) {
		Continue_Flag = 1;
		Continue_Step_Flag = 1;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	else if (!Continue_Flag&&Continue_Step_Flag) {
		Continue_Flag = 1;
		Send_RealTimes = 0;
		m_IOA = c_Start_Info;
		SendT_RealTimes++;
	}
	else if (Continue_Flag&&Continue_Step_Flag) {
		Send_RealTimes++;
	}
	/*----------------------------------------------------------
	 ????ѭ???????????γ?ѭ??????
	 ------------------------------------------------------------*/
	if (Send_RealTimes>=Send_Times-1) {
		if (SendT_RealTimes==Steps-1) {
			Send_DataEnd = 1;
		}
		else {
			Continue_Flag = 0;
		}
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
	}
	else {
		m_VSQ = Send_num;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;

	c_HisSendT = c_HisStartT+c_CircleTime*SendT_RealTimes;     //??ʷ?洢??ʱ??
	Back_DataTime(c_HisSendT, &backTime[0]);     //?????õ???Ӧ??????ʱ??

	Start_MIN = backTime[0];
	Start_H = backTime[1];
	Start_D = backTime[2];
	Start_MON = backTime[3];
	Start_YL = backTime[4];
	printf(
	                "Start_YL=%d,Start_MON=%d,Start_D=%d,Start_H=%d,Start_MIN=%d\n",
	                Start_YL,
	                Start_MON,
	                Start_D,
	                Start_H,
	                Start_MIN);
	printf(
	                "In MNT history m_vsq=%d,send_realtimes=%d,sendt_realtimes=%d\n",
	                m_VSQ,
	                Send_RealTimes,
	                SendT_RealTimes);
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA/4;
		inf_no = m_IOA%4;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA;

		ret = GetFileName_Day(
		                &filename,
		                Start_MON,
		                Start_D,
		                mtr_no,
		                TASK_MNT);
		printf(
		                "#In mnt history trans filename is %s#\n",
		                filename.c_str());
		/*-----------------------------------------------
		 ????״̬???ļ??????ڻ????Ѿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			//E5H_Yes();
			Send_MFrame(10);
			return -7;
		}

		Circle = Search_CircleDBS(
		                filename,
		                Start_YL,
		                Start_MON,
		                Start_D,
		                &Save_Num,
		                Save_XL,
		                TASK_MNT);
		/*-----------------------------------------------
		 ????״̬?????ݴ洢???ڹ???
		 ------------------------------------------------*/
		if (!Circle||(Circle!=c_CircleTime)) {
			Clear_Continue_Flag();
			//E5H_Yes();
			Send_MFrame(10);
			return -8;
		}
		{
			memset(tempData, 0, sizeof(tempData));     //061204
			for (j = 0; j<FEILVMAX; j++) {
				Save_Point = inf_no*FEILVMAX+j;
				Save_Have = 0;
				for (m = 0; m<Save_Num; m++) {
					if (Save_XL[m]==Save_Point) {
						Save_Have = 1;
						Save_XH = Save_Point;
						break;
					}
				}
				if (Save_Have)
					GetMntDataDBS(
					                tempData+j*10,
					                filename,
					                Start_MIN,
					                Start_H,
					                Save_XH,
					                Circle,
					                Save_Num,
					                TASK_MNT);
				else {
					memset(tempData+j*10, 0, 10);
					tempData[j*10+9] = 0x80;
				}
				memcpy(&value, tempData+j*10, 4);
				val = 1000*value;
				memcpy(tempData, &val, 4);
				memcpy(
				                &m_transBuf.m_transceiveBuf[buffptr],
				                tempData+j*10,
				                9);
				buffptr += 9;
			}
			//printf("%d,%d,%d,%d\n",tempData[9],tempData[19],tempData[29],tempData[39],tempData[49]);
			m_transBuf.m_transceiveBuf[buffptr++ ] =
			                tempData[9]&tempData[19]&tempData[29]&tempData[39]&tempData[49];
			sum = 0;
			for (j = 0; j<1+9*FEILVMAX+1; j++)
				sum +=
				                m_transBuf.m_transceiveBuf[buffptr-(2+9*FEILVMAX)+j];
			m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
		}
		m_IOA++;
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[0];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[1];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[2];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[3];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[4];
	sum = 0;
	for (i = 4; i<buffptr; i++)
		sum += m_transBuf.m_transceiveBuf[i];
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;
}

/**************************??ǰ???????޹?(176/166)*******************************/

int CDl719s::M_QR_NA_2()
{
	unsigned char mtr_no, buffptr, i, info_no, j, sum;
	unsigned short info_total;
	unsigned int e_val, e_flag;
	struct m_tSystime systime;
	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	if (Send_DataEnd) {
		Clear_Continue_Flag();
		E5H_Yes();
		return 0;
	}
	if (!Continue_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		m_TI = M_XX_NA_2;
		if (c_Stop_Info>=sysConfig->meter_num*4) {
			c_Stop_Info = sysConfig->meter_num*4-1;
		}
		/*-----------------------------------------------
		 ????״̬????Ϣ????ַ??Χ????
		 ------------------------------------------------*/
		if (c_Stop_Info<=c_Start_Info) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num*4) {
			Send_Total = sysConfig->meter_num*4;
		}

		m_IOA = c_Start_Info;

		Info_Size = (4*FEILVMAX+3);
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;

		Continue_Flag = 1;
		Send_RealTimes = 0;
	}

	if (Send_RealTimes<Send_Times-1) {
		m_VSQ = Send_num;
	}else {
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
		/*-------------------------------------------------
		 ???????ݴ??????ϱ?־
		 ---------------------------------------------------*/
		Send_DataEnd = 1;
	}
	GetSystemTime_RTC(&systime);
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA/4;
		info_no = m_IOA%4;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA;
		for (j = 0; j<FEILVMAX; j++) {
			e_val = 1000*m_meterData[mtr_no].m_iQR[5*info_no+j];
			TransLong2BinArray(
			                &m_transBuf.m_transceiveBuf[buffptr],
			                e_val);
			buffptr += 4;
		}
		e_flag = m_meterData[mtr_no].Flag_QR&(0x0001f<<((m_IOA%4)*5));
		m_transBuf.m_transceiveBuf[buffptr++ ] = e_flag ? 0 : 0x80;
		m_transBuf.m_transceiveBuf[buffptr++ ] = 0;
		m_IOA++;
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.min;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.hour;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.day;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.mon;
	m_transBuf.m_transceiveBuf[buffptr++ ] = systime.year;
	sum = 0;
	for (i = 4; i<buffptr; i++) {
		sum += m_transBuf.m_transceiveBuf[i];
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	/*---------------------------------------
	 ?ѷ???֡??????
	 -----------------------------------------*/
	Send_RealTimes++;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;
}

/************??ʷ????ֵ(177/167)*********************************/
int CDl719s::M_QR_NA_T2()
{
	unsigned char mtr_no, inf_no;
	unsigned char buffptr, i, j, e_quality, sum, Save_BZ, Save_Have;
	unsigned char Start_YL, Start_MON, Start_D, Start_MIN, Start_H;
	unsigned short Circle;
	unsigned char Save_Point, Save_XH, m;
	int ret;
	unsigned long T1, T2, T3, T4, e_val;
	unsigned char backTime[5], Save_Num, Save_XL[20], tempData[32];
	unsigned char test;
	struct m_tSystime systime;
	unsigned int val;
	float value;
	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	printf(
	                "In QR history continueflag is %d,continue_step_flag is %d,send_dataend is %d\n",
	                Continue_Flag,
	                Continue_Step_Flag,
	                Send_DataEnd);
	if (Send_DataEnd) {
		Clear_Continue_Flag();
		E5H_Yes();
		return 0;
	}

	if (!Continue_Flag&&!Continue_Step_Flag) {
		/*------------------------------
		 ??ʼ??֡??־
		 --------------------------------*/
		m_COT = 5;
		m_TI = M_XX_TA_2;
		if (c_Stop_Info>=sysConfig->meter_num*4) {
			c_Stop_Info = sysConfig->meter_num*4-1;
		}
		/*-----------------------------------------------
		 ????״̬????Ϣ????ַ??Χ????
		 ------------------------------------------------*/
		if (c_Stop_Info<=c_Start_Info) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -1;
		}

		Send_Total = c_Stop_Info-c_Start_Info+1;
		if (Send_Total>=sysConfig->meter_num*4) {
			Send_Total = sysConfig->meter_num*4;
		}

		m_IOA = c_Start_Info;
		GetSystemTime_RTC(&systime);
		Info_Size = (4*FEILVMAX+3);
		Send_num = (MAXSENDBUF-DL719FIXLEN)/Info_Size;
		Send_Times = Send_Total/Send_num;
		if (Send_Total%Send_num)
			Send_Times++;
		/*-------------------------------------------------------------
		 ??վ?ٻ???ʼʱ???ࡰ?��??ա???????
		 ??վ?ٻ?????ʱ???ࡰ?��??ա???????
		 ??ǰʱ?̾ࡰ?��??ա???????
		 ---------------------------------------------------------------*/
		printf(
		                "In mnt history trans c_Start_MIN=%d,c_Start_H=%d,c_Start_D=%d,c_Start_MON=%d,c_Start_YL=%d\n",
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		printf(
		                "In mnt history trans c_End_MIN=%d,c_End_H=%d,c_Start_D=%d,c_End_MON=%d,c_End_YL=%d\n",
		                c_End_MIN,
		                c_End_H,
		                c_Start_D,
		                c_End_MON,
		                c_End_YL);
		T1 = Calc_Time_102(
		                c_Start_MIN,
		                c_Start_H,
		                c_Start_D,
		                c_Start_MON,
		                c_Start_YL);
		T2 = Calc_Time_102(
		                c_End_MIN,
		                c_End_H,
		                c_End_D,
		                c_End_MON,
		                c_End_YL);
		T3 = Calc_Time_102(
		                systime.min,
		                systime.hour,
		                systime.day,
		                systime.mon,
		                systime.year);
		/*-----------------------------------------------
		 ????״̬??ʱ??????????
		 ------------------------------------------------*/
		if ((T1==0)||(T2==0)||(T3==0)) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -2;
		}
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		ret = Get_Valid_Start_Save_Minute(
		                &Start_YL,
		                &Start_MON,
		                &Start_D,
		                &Start_H,
		                &Start_MIN);
		if (ret<0) {
			E5H_Yes();
			Clear_Continue_Flag();
			return -3;
		}
		T4 = Calc_Time_102(
		                Start_MIN,
		                Start_H,
		                Start_D,
		                Start_MON,
		                Start_YL);
		/*-----------------------------------------------
		 ????״̬????Чʱ?䷶Χ????
		 ------------------------------------------------*/
		if ((T1>T3)||(T2<T4)||(T1>T2)) {
			E5H_Yes();
			Clear_Continue_Flag();
			return -4;
		}
		/*-----------------------------------------------
		 ȷ????ȷ????ʼʱ???ͽ???ʱ??
		 ------------------------------------------------*/
		if (T1<=T4) {
			T1 = T4;
			c_Start_YL = Start_YL;
			c_Start_MON = Start_MON;
			c_Start_D = Start_D;
		}
		if (T2>=T3)
			T2 = T3;

		ret = GetFileName_Day(
		                &filename,
		                c_Start_MON,
		                c_Start_D,
		                c_Start_Info/4,
		                TASK_MNT);
		/*-----------------------------------------------
		 ????״̬?????????ݿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -5;
		}

		Circle = Search_CircleDBS(
		                filename,
		                c_Start_YL,
		                c_Start_MON,
		                c_Start_D,
		                &Save_Num,
		                &Save_XL[0],
		                TASK_QR);

		/*-----------------------------------------------
		 ????״̬???洢???ڴ???
		 ------------------------------------------------*/
		if (Circle==0) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -6;
		}
		/*------------------------------------------------------------------------------
		 ?ҵ?????????????ʷ?ļ???ʼ?洢?㣺?????洢????Ϊ30???ӣ???ʼʱ??Ϊ17:35????ʵ????ʼ???ļ??洢??ӦΪ18:00;
		 ?ҵ?????????????ʷ?ļ??Ľ????洢?㣺?????洢????Ϊ30???ӣ?????ʱ??Ϊ22:55,??ʵ?ʽ??????ļ??洢??ӦΪ22:30??
		 ??ǰʱ????Ӧ??Ч?洢?㣬?????洢????Ϊ30???ӣ???ǰʱ??Ϊ19:27,??????ʱ???????洢??Ϊ19:00;
		 ?????ܲ???(ʱ????)
		 ??????ʷ??��????һ????????????ʼʱ??
		 --------------------------------------------------------------------------------*/
		c_CircleTime = Circle;
		if ((T1%Circle)!=0)
			T1 = T1-T1%Circle+Circle;
		if ((T2%Circle)!=0)
			T2 = T2-T2%Circle;
		if ((T3%Circle)!=0)
			T3 = T3-T3%Circle;

		if (T2<T3)
			Steps = (T2-T1)/Circle+1;
		else if (T2>=T3)
			Steps = (T3-T1)/Circle+1;

		c_HisStartT = T1;

		Continue_Flag = 0;
		Continue_Step_Flag = 0;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	/*---------------------------------------------------------
	 ????ѭ???????????γ??ϴ?????
	 -----------------------------------------------------------*/
	if (!Continue_Step_Flag&&!Continue_Flag) {
		Continue_Flag = 1;
		Continue_Step_Flag = 1;
		Send_RealTimes = 0;
		SendT_RealTimes = 0;
	}
	else if (!Continue_Flag&&Continue_Step_Flag) {
		Continue_Flag = 1;
		Send_RealTimes = 0;
		m_IOA = c_Start_Info;
		SendT_RealTimes++;
	}
	else if (Continue_Flag&&Continue_Step_Flag) {
		Send_RealTimes++;
	}
	/*----------------------------------------------------------
	 ????ѭ???????????γ?ѭ??????
	 ------------------------------------------------------------*/
	if (Send_RealTimes>=Send_Times-1) {
		if (SendT_RealTimes==Steps-1) {
			Send_DataEnd = 1;
		}
		else {
			Continue_Flag = 0;
		}
		m_VSQ = Send_Total-Send_num*Send_RealTimes;
	}
	else {
		m_VSQ = Send_num;
	}
	buffptr = 0;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 9+m_VSQ*(Info_Size)+5;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[buffptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_L;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Dev_Address_H;
	m_transBuf.m_transceiveBuf[buffptr++ ] = c_Record_Addr;

	c_HisSendT = c_HisStartT+c_CircleTime*SendT_RealTimes;     //??ʷ?洢??ʱ??
	Back_DataTime(c_HisSendT, &backTime[0]);     //?????õ???Ӧ??????ʱ??

	Start_MIN = backTime[0];
	Start_H = backTime[1];
	Start_D = backTime[2];
	Start_MON = backTime[3];
	Start_YL = backTime[4];
	printf(
	                "Start_YL=%d,Start_MON=%d,Start_D=%d,Start_H=%d,Start_MIN=%d\n",
	                Start_YL,
	                Start_MON,
	                Start_D,
	                Start_H,
	                Start_MIN);
	printf(
	                "In qr history m_vsq=%d,send_realtimes=%d,sendt_realtimes=%d\n",
	                m_VSQ,
	                Send_RealTimes,
	                SendT_RealTimes);
	for (i = 0; i<m_VSQ; i++) {
		mtr_no = m_IOA/4;
		inf_no = m_IOA%4;
		m_transBuf.m_transceiveBuf[buffptr++ ] = m_IOA;

		ret = GetFileName_Day(
		                &filename,
		                Start_MON,
		                Start_D,
		                mtr_no,
		                TASK_QR);
		printf(
		                "#In qr history trans filename is %s#\n",
		                filename.c_str());
		/*-----------------------------------------------
		 ????״̬???ļ??????ڻ????Ѿ?????
		 ------------------------------------------------*/
		if (ret<0) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -7;
		}
		Circle = Search_CircleDBS(
		                filename,
		                Start_YL,
		                Start_MON,
		                Start_D,
		                &Save_Num,
		                Save_XL,
		                TASK_QR);
		/*-----------------------------------------------
		 ????״̬?????ݴ洢???ڹ???
		 ------------------------------------------------*/
		if (!Circle||(Circle!=c_CircleTime)) {
			Clear_Continue_Flag();
			E5H_Yes();
			return -8;
		}

		{
			for (j = 0; j<FEILVMAX; j++) {
				Save_Point = inf_no*FEILVMAX+j;
				Save_Have = 0;
				for (m = 0; m<Save_Num; m++) {
					if (Save_XL[m]==Save_Point) {
						Save_Have = 1;
						Save_XH = Save_Point;
						break;
					}
				}
				if (Save_Have)
					GetDnlSSlDataDBS(
					                tempData+j*5,
					                filename,
					                Start_MIN,
					                Start_H,
					                Save_XH,
					                Circle,
					                Save_Num,
					                TASK_QR);
				else {
					memset(tempData+j*5, 0, 5);
					tempData[j*5+4] = 0x80;
				}
				memcpy(&value, tempData+j*5, 4);
				val = 1000*value;
				memcpy(tempData+j*5, &val, 4);
				memcpy(
				                &m_transBuf.m_transceiveBuf[buffptr],
				                tempData+j*5,
				                4);
				buffptr += 4;
			}
			m_transBuf.m_transceiveBuf[buffptr++ ] =
			                tempData[4]&tempData[9]&tempData[14]&tempData[19]&tempData[24];
			sum = 0;
			for (j = 0; j<1+4*FEILVMAX+1; j++)
				sum +=
				                m_transBuf.m_transceiveBuf[buffptr-(2+4*FEILVMAX)+j];
			m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
		}
		m_IOA++;
	}
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[0];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[1];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[2];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[3];
	m_transBuf.m_transceiveBuf[buffptr++ ] = backTime[4];
	sum = 0;
	for (i = 4; i<buffptr; i++)
		sum += m_transBuf.m_transceiveBuf[i];
	m_transBuf.m_transceiveBuf[buffptr++ ] = sum;
	m_transBuf.m_transceiveBuf[buffptr++ ] = 0x16;
	m_transBuf.m_transCount = buffptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	return 0;
}
/****************************************************************
 读一个全部/一定数量的单点信息的记录
 *****************************************************************/
int CDl719s::M_SP_NA_2N()
{
	unsigned char tempsum = 0;
	unsigned int ptr = 0, j = 0;
	unsigned char Start_YL, Start_MON, Start_D, Start_MIN, Start_H;
	unsigned char temp_buf[264];
	unsigned long T1, T2;
	int ret;
	if (m_Resend) {
		DP_HERE;
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	memset(temp_buf, 0, sizeof(temp_buf));
	m_COT = 0x05;
	m_TI = 1;
	m_VSQ = 0;
	if (Continue_Flag==1) {
		//printf("df102->Continue_Flag=YES enter continue send data\n");
		goto CONSDEVT;
	}
	//df102->Continue_Flag=YES;
	CONSDEVT:
	if (Continue_Flag==1){
		if (!SendT_RealTimes){
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			SendT_RealTimes = 0;
			return -1;
		}
	}

	//printf("@@@start enter read history event!&&&&&&& SendT_RealTimes=%d\n",SendT_RealTimes);
	//ret=read_hisevt(temp_buf, T1, T2, SendT_RealTimes);
	unsigned char evt_num=0;
	#define READ_EVT_NUM 5
	//读取事件,个数放到evt_num中,事件数组数据放到temp_buf中
	ret=Read_Cur_Evt(temp_buf,&evt_num);
	printf(LIB_DBG"ent_num=%d\n",evt_num);
	/*ret = ReadSomeRecord(
	                temp_buf,
	                T1,
	                T2,
	                SendT_RealTimes,
	                LEN_PER_HISEVT,
	                EVENTFILE,
	                MAXEVENT,
	                1);
*/
	// printf("@@@read out history event,ret=%d!&&&&&&\n,  ",ret);
	 for(j=0;j<200;j++)
		 printf(" %02x",temp_buf[j]);
	 printf("\n");

	if ((ret<0)||(evt_num==0)){
		printf(LIB_INF"ret=%d evt_num=%d\n",ret,evt_num);
		Clear_Continue_Flag();
		Send_MFrame(10);
		//E5H_Yes();
		SendT_RealTimes = 0;
		return -2;
	}
	if (!temp_buf[3]){
		//SendT_RealTimes = 0;
	}else{
		//SendT_RealTimes = temp_buf[1]+temp_buf[2]*0x100;
	}
	if (!Continue_Flag){
		//c_HisStartT =
		//                temp_buf[4]+temp_buf[5]*0x100+temp_buf[6]*0x10000+temp_buf[7]*0x1000000;
		//c_HisSendT =
		//                temp_buf[8]+temp_buf[9]*0x100+temp_buf[10]*0x10000+temp_buf[11]*0x1000000;
	}
	m_VSQ =evt_num>READ_EVT_NUM?READ_EVT_NUM:evt_num;
	printf("@@@df102->VSQ = %d!&&&&&&  \n", m_VSQ);

	ptr = 0;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[ptr++ ] = 9+m_VSQ*9;
	m_transBuf.m_transceiveBuf[ptr++ ] = 9+m_VSQ*9;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;

	m_transBuf.m_transceiveBuf[ptr++ ] = m_ACD ? 0x028 : 0x08;	//??????
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_TI;     //???ͱ?ʶ
	m_transBuf.m_transceiveBuf[ptr++ ] = m_VSQ;     //???巢????Ϣ??????
	m_transBuf.m_transceiveBuf[ptr++ ] = m_COT;     //????ԭ??
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_lo;
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_hi;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Record_Addr;
	for (j = 0; j<m_VSQ; j++){
		memcpy(m_transBuf.m_transceiveBuf+ptr, temp_buf+j*9, 9);
		ptr += 9;
	}
	tempsum = 0;
	for (j = 4; j<ptr; j++)
		tempsum += m_transBuf.m_transceiveBuf[j];
	m_transBuf.m_transceiveBuf[ptr++ ] = tempsum;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x16;
	m_transBuf.m_transCount = ptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	printf(
	                "@@@m_transBuf.m_transCount = %d!&&&&&&  \n",
	                m_transBuf.m_transCount);
	Continue_Flag = 1;
	return 0;
}
/****************************************************************
 读一个选定时间范围内的带时标的单点信息的记录
 *****************************************************************/
int CDl719s::M_SP_TA_2N()
{
	unsigned char tempsum = 0;
	unsigned int ptr = 0, j = 0;
	unsigned char Start_YL, Start_MON, Start_D, Start_MIN, Start_H;
	unsigned char temp_buf[264];
	unsigned long T1, T2;
	int ret;
	if (m_Resend) {
		DP_HERE;
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	memset(temp_buf, 0, sizeof(temp_buf));
	m_COT = 0x05;
	m_TI = 1;
	m_VSQ = 0;
	if (Continue_Flag==1) {
		//printf("df102->Continue_Flag=YES enter continue send data\n");
		goto CONSDEVT;
	}
	//printf("In M_sp_ta_2n:");
	printf("c_Start_MIN=%d,c_Start_H=%d,c_Start_D=%d,c_Start_MON=%d,c_Start_YL=%d\n",c_Start_MIN,c_Start_H,c_Start_D,c_Start_MON,c_Start_YL);
	printf("c_end_MIN=%d,c_end_H=%d,c_end_D=%d,c_end_MON=%d,c_end_YL=%d\n",c_End_MIN,c_End_H,c_End_D,c_End_MON,c_End_YL);
	T1 = Calc_Time_102(
	                c_Start_MIN,
	                c_Start_H,
	                c_Start_D,
	                c_Start_MON,
	                c_Start_YL);	//??վ?ٻ???ʼʱ???ࡰ?��??ա???????
	T2 = Calc_Time_102(c_End_MIN, c_End_H, c_End_D, c_End_MON, c_End_YL);     //??վ?ٻ?????ʱ???ࡰ?��??ա???????
	//printf("T1=%d,T2=%d\n",T1,T2);
	if ((T1==0)||(T2==0))
	                {
		Clear_Continue_Flag();
		Send_MFrame(10);
		//E5H_Yes();
		SendT_RealTimes = 0;
		printf("error t1 or t2 =0\n");
		return -3;
	}
	c_HisStartT = T1;
	c_HisSendT = T2;
	//df102->Continue_Flag=YES;
	CONSDEVT:
	if (Continue_Flag==1){
		if (!SendT_RealTimes){
			Clear_Continue_Flag();
			Send_MFrame(10);
			//E5H_Yes();
			SendT_RealTimes = 0;
			return -1;
		}
	}
	T1 = c_HisStartT;
	T2 = c_HisSendT;

	//printf("@@@start enter read history event!&&&&&&& SendT_RealTimes=%d\n",SendT_RealTimes);
	//ret=read_hisevt(temp_buf, T1, T2, SendT_RealTimes);
	ret=Read_evt_kkk(temp_buf, c_HisStartT, c_HisSendT,EVENTFILE,SendT_RealTimes);
	/*ret = ReadSomeRecord(
	                temp_buf,
	                T1,
	                T2,
	                SendT_RealTimes,
	                LEN_PER_HISEVT,
	                EVENTFILE,
	                MAXEVENT,
	                1);
*/
	// printf("@@@read out history event,ret=%d!&&&&&&\n,  ",ret);
//	 for(j=0;j<200;j++)
//	 printf(" %02x",temp_buf[j]);
//	 printf("\n");

	if ((ret<0)||(!temp_buf[0])){
		printf(LIB_INF"ret=%d temp_buf[0]=%d\n",ret,temp_buf[0]);
		Clear_Continue_Flag();
		Send_MFrame(10);
		//E5H_Yes();
		SendT_RealTimes = 0;
		return -2;
	}
	if (!temp_buf[3]){
		SendT_RealTimes = 0;
	}else{
		SendT_RealTimes = temp_buf[1]+temp_buf[2]*0x100;
	}
	if (!Continue_Flag){
		c_HisStartT =
		                temp_buf[4]+temp_buf[5]*0x100+temp_buf[6]*0x10000+temp_buf[7]*0x1000000;
		c_HisSendT =
		                temp_buf[8]+temp_buf[9]*0x100+temp_buf[10]*0x10000+temp_buf[11]*0x1000000;
	}
	m_VSQ = temp_buf[0];
	printf("@@@df102->VSQ = %d!&&&&&&  \n", m_VSQ);

	ptr = 0;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[ptr++ ] = 9+m_VSQ*9;
	m_transBuf.m_transceiveBuf[ptr++ ] = 9+m_VSQ*9;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;

	m_transBuf.m_transceiveBuf[ptr++ ] = m_ACD ? 0x028 : 0x08;	//??????
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_TI;     //???ͱ?ʶ
	m_transBuf.m_transceiveBuf[ptr++ ] = m_VSQ;     //???巢????Ϣ??????
	m_transBuf.m_transceiveBuf[ptr++ ] = m_COT;     //????ԭ??
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_lo;
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_hi;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Record_Addr;
	for (j = 0; j<m_VSQ; j++){
		memcpy(m_transBuf.m_transceiveBuf+ptr, temp_buf+12+j*9, 9);
		ptr += 9;
	}
	tempsum = 0;
	for (j = 4; j<ptr; j++)
		tempsum += m_transBuf.m_transceiveBuf[j];
	m_transBuf.m_transceiveBuf[ptr++ ] = tempsum;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x16;
	m_transBuf.m_transCount = ptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	printf(
	                "@@@m_transBuf.m_transCount = %d!&&&&&&  \n",
	                m_transBuf.m_transCount);
	Continue_Flag = 1;
	return 0;
}
/**
 * 传输1类数据
 */
void CDl719s::C_PL1_NA2(void)
{
	printf(LIB_INF"传输1类数据\n");
	int ret;
	switch (c_TI) {
	case C_TI_NB_2:
		printf(LIB_INF"返回系统时间\n");
		ret = M_Return_SysTime();
		break;
	case C_SYN_TA_2:
		printf(LIB_INF"对时 系统时间\n");
		ret = M_SYN_TA_2();
		break;
	case C_CI_NC_2:     //实时总电量
		printf(LIB_INF"实时总电量\n");
		ret = M_IT_NA_2(0);
		break;
	case C_CI_NQ_2:     //历史总电量/分时电量
		ret = M_IT_NA_T2(0);
		printf(LIB_INF"历史总电量 Te history ret %d \n", ret);
		break;
	case C_CI_NA_B_2:	//实时分时电量
		printf(LIB_INF"实时分时电量\n");
		ret = M_IT_NA_2(1);
		break;
	case C_CI_TA_B_2:	//历史分时电量
		printf(LIB_INF"历史分时电量\n");
		ret = M_IT_NA_T2(1);
		printf("In c_PL1_NA2  ret %d \n", ret);
		break;
	case C_YC_NA_2:     //实时遥测电量
		printf(LIB_INF"实时遥测电量\n");
		ret = M_SSZ_NA_2();
		break;
	case C_YC_TA_2:     //??ʷ˲ʱ��
		printf(LIB_INF"历史遥测电量\n");
		ret = M_SSZ_NA_T2();
		printf("In c_PL1_NA2  ret %d \n", ret);
		break;
	case C_MNT_NA_2:
		printf(LIB_INF"实时需量\n");
		ret = M_MaxND_NA_2();
		break;
	case C_MNT_TA_2:
		printf(LIB_INF"历史需量\n");
		ret = M_MaxND_NA_T2();
		printf("M_MaxND_NA_T2 ret %d \n", ret);
		break;
	case C_XX_NA_2:
		printf(LIB_INF"实时四象限无功\n");
		ret = M_QR_NA_2();
		printf("M_QR_NA_2 ret %d \n", ret);
		break;
	case C_XX_TA_2:
		printf(LIB_INF"历史四象限无功电量\n");
		ret = M_QR_NA_T2();
		printf("M_QR_NA_T2  ret %d \n", ret);
		break;
	case C_LTOU_TA_2:
		printf(LIB_INF"月冻结电量 Process ldd history!\n");
//		ret=M_Last_Month_TOU();
		printf("In c_PL1_NA2  ret %d \n", ret);
		break;
	case C_PARA_TRAN:
		printf(LIB_INF"终端参数上传\n");
		Ertu_Para_NA2();
		break;
	case C_PARA_SET:
		printf(LIB_INF"终端参数设置\n");
		ret = M_Cfg_Para_Update_NA2(1);
		printf("M_Cfg_Para_Update_NA2 ret=%d\n", ret);
		break;
	case C_CTL_MON_ON:
		printf(LIB_INF"M_CTL_PORT_MOn \n");
		ret = M_CTL_PORT_Open();
		printf(LIB_INF"M_CTL_PORT_MOn ret=%d\n", ret);
		break;
	case C_CTL_MON_OFF:
		printf(LIB_INF"M_CTL_PORT_MON OFF \n");
		ret = M_CTL_PORT_Close();
		break;
	case C_SP_NB_2:
		printf(LIB_INF"读一个选定时间范围内的带时标的单点信息的记录\n");
		ret = M_SP_TA_2N();
		printf(LIB_INF"M_SP_TA_2N ret=%d\n", ret);
		break;
	case C_SP_NA_2:
		printf(LIB_INF"读单点信息的记录\n");
		ret = M_SP_NA_2N();
		printf(LIB_INF"C_SP_NA_2 ret=%d\n", ret);
		break;
	default:
		printf(LIB_ERR"未知的type %d[%X]\n",c_TI,c_TI);
		break;
	}
}
///处理接收到的短帧
int CDl719s::Process_Short_Frame(unsigned char *data)
{
	//unsigned char m_func,m_func_tmp;
	c_func = *(data+1)&0x0f;
	printf(LIB_INF"FC %d[0x%X]  \n",c_func,c_func);
	if (c_func!=c_func_tmp){
		//DP_RET(c_func)
		c_FCB_Tmp = 0xff;
	}
	c_func_tmp = c_func;
	unsigned char fcv=(*(data+1)&0b00010000)>>4;//帧计数有效位
	c_FCB = *(data+1)&0x20;
	if ((c_FCB_Tmp==c_FCB)&&(fcv==1) ){
		printf(LIB_INF"FCB 原始 %X ,现在 %X \n",c_FCB_Tmp,c_FCB);
		m_transBuf.m_transCount = m_LastSendBytes;
		return 0;
	}
	printf(LIB_DBG"帧计数有效位 fcv:%d\n",fcv);
	if(fcv==1){
		c_FCB_Tmp = c_FCB;
	}
	c_FCV = *(data+1)&0x10;
	//DP_RET(c_func)
	printf("In %s m_func %d \n",__FUNCTION__,c_func);
	switch (c_func) {
	case 0:
		printf(LIB_INF"fc 0 复位远方通讯单元\n");
		Command = C_RCU_NA_2;
		M_NV_NA_2(0);
		C_RCU_NAF();//复位通信单元reset  commint unit
		this->Sequence_number=0;
		break;
	case 9:
		printf(LIB_INF"fc 9 请求链路状态\n");
		Command = C_RLK_NA_3;
		M_NV_NA_2(1);
		break;
	case 10:
		printf(LIB_INF"fc 10 召唤一类数据\n");
		//printf(LIB_INF"In short Frame(2 is C_CON_ACT,3 is C_DATA_TRAN) Command is %d\n",
		//                Command);
		if (C_CON_ACT==Command) {
			printf(LIB_INF"响应 传输原因7确认激活(镜像帧) C_CON_ACT\n");
			Send_MFrame(7);
			Command = C_DATA_TRAN;
		} else if (C_DATA_TRAN==Command) {
			printf(LIB_INF"数据传输 1类 C_DATA_TRAN\n");
			C_PL1_NA2();
		} else {
			printf(LIB_INF"其他 传输原因 10激活中止(镜像帧) \n");
			Send_MFrame(10);
			Command = C_NULL;
			Clear_FrameFlags();	//
		}
		break;
	case 11://召唤2类数据
		printf(LIB_INF"fc 11 召唤二类数据\n");
		if (m_ACD){//没有2类,有1类数据需要访问
			printf(LIB_INF"有1类,没2类\n");
			M_NV_NA_2(2);
		}else{//没有二类数据
			printf(LIB_INF"1,2类都没有 SC回复\n");
			E5H_Yes();
		}
		break;
	case 15:
		break;
	default:
		printf(LIB_ERR"未知的功能码 %d[0x%X]\n",c_func,c_func);
		break;
	}
	return 0;
}
/*-------?ն?ʱ??ͬ??-------------------------
 m_ti=128
 ------------------------------------------------------*/
int CDl719s::M_SYN_TA_2()
{
	unsigned char ptr, i;
	unsigned char sum;
	struct m_tSystime Sys_Time;
	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}

	m_TI = C_SYN_TA_2;
	m_VSQ = 1;
	m_COT = 0x30;
	ptr = 0;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x10;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x10;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_lo;
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_hi;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Record_Addr;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0;
	GetSystemTime_RTC(&Sys_Time);
	m_transBuf.m_transceiveBuf[ptr++ ] = Sys_Time.sec<<2;
	m_transBuf.m_transceiveBuf[ptr++ ] = Sys_Time.min;     //Sys_Time.min;
	m_transBuf.m_transceiveBuf[ptr++ ] = Sys_Time.hour;     //Sys_Time.hour;
	m_transBuf.m_transceiveBuf[ptr++ ] = Sys_Time.day;     //Sys_Time.day;
	m_transBuf.m_transceiveBuf[ptr++ ] = Sys_Time.mon;     //Sys_Time.mon;
	m_transBuf.m_transceiveBuf[ptr++ ] = Sys_Time.year;     //Sys_Time.year;
	sum = 0;
	for (i = 4; i<ptr; i++)
		sum += m_transBuf.m_transceiveBuf[i];
	m_transBuf.m_transceiveBuf[ptr++ ] = sum;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x16;
	m_transBuf.m_transCount = ptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	Clear_Continue_Flag();
	Clear_FrameFlags();
	return 0;
}
/***********************(103/72)********************************************/
int CDl719s::M_Return_SysTime()
{
	unsigned char i, ptr, sum;
	struct m_tSystime systime;

	if (m_Resend) {
		m_transBuf.m_transCount = m_LastSendBytes;
		m_Resend = 0;
		return 0;
	}
	m_TI = M_TI_NA_2;
	m_VSQ = 1;
	m_COT = 0x05;
	ptr = 0;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x10;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x10;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x68;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_ACD ? 0x28 : 0x08;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_L;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Link_Address_H;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_TI;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_VSQ;
	m_transBuf.m_transceiveBuf[ptr++ ] = m_COT;
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_lo;
	m_transBuf.m_transceiveBuf[ptr++ ] = logic_Ertu_hi;
	m_transBuf.m_transceiveBuf[ptr++ ] = c_Record_Addr;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0;
	GetSystemTime_RTC(&systime);
	m_transBuf.m_transceiveBuf[ptr++ ] = systime.sec<<2;
	m_transBuf.m_transceiveBuf[ptr++ ] = systime.min;
	m_transBuf.m_transceiveBuf[ptr++ ] = systime.hour;
	m_transBuf.m_transceiveBuf[ptr++ ] = systime.day;
	m_transBuf.m_transceiveBuf[ptr++ ] = systime.mon;
	m_transBuf.m_transceiveBuf[ptr++ ] = systime.year;
	printf(LIB_INF"%d-%d-%d %d:%d %d\n",
		systime.year,systime.mon,systime.day,
		systime.hour,systime.min,systime.sec<<2);
	sum = 0;
	for (i = 4; i<ptr; i++)
		sum += m_transBuf.m_transceiveBuf[i];
	m_transBuf.m_transceiveBuf[ptr++ ] = sum;
	m_transBuf.m_transceiveBuf[ptr++ ] = 0x16;
	m_transBuf.m_transCount = ptr;
	m_LastSendBytes = m_transBuf.m_transCount;
	Clear_Continue_Flag();
	Clear_FrameFlags();
	return 0;
}

int CDl719s::Process_Long_Frame(unsigned char * data)
{
	char ret;
	unsigned char tmp_TI, tmp_VSQ, tmp_Cot, tmp_Fun;
	unsigned char evtbuf[9];
	//Clear_Continue_Flag();
	//C_RCU_NAF();
	c_TI = *(data+7);
	if (c_TI!=c_TI_tmp){
		c_FCB_Tmp = 0xff;
	}
	c_TI_tmp = c_TI;
	c_FCB = *(data+4)&0x20;
	c_func = *(data+4)&0x0f;
	unsigned char fcv=(*(data+4)&0b00010000)>>4;//帧计数有效位
	if (c_FCB==c_FCB_Tmp && fcv==1) {
		printf(LIB_INF"重发\n");
		m_transBuf.m_transCount = m_LastSendBytes;
		return 0;
	}

	if(fcv==1){
		c_FCB_Tmp = c_FCB;
	}
	c_FCV = *(data+4)&0x10;
	tmp_VSQ = *(data+8);
	tmp_Cot = *(data+9);
	if ((0x03==c_func)/*&&(0x01==tmp_VSQ)*/) {
		printf(LIB_INF"功能码3 数据传输 c_TI_tmp=%d\n",c_TI_tmp);
		logic_Ertu_lo=*(data+10);
		logic_Ertu_hi=*(data+11);
		switch (c_TI_tmp) {
		case C_SYN_TA_2://系统时间同步
			c_TI = C_SYN_TA_2;
			m_ACD = 1;
			if (/*!ERTU_TIME_CHECK&&*/m_checktime_valifalg) {
				E5H_Yes();
				//Command=C_CON_ACT;
				Command = C_DATA_TRAN;
				TMStruct Sys_Time;
				Sys_Time.second = *(data+14)>>2;
				Sys_Time.minute = *(data+15)&0X3F;
				Sys_Time.hour = *(data+16)&0X1F;
				Sys_Time.dayofmonth = *(data+17)&0X1F;
				Sys_Time.month = *(data+18)&0X0F;
				Sys_Time.year = 2000+(*(data+19)&0X7F);
				//m_COT=
				Write_RTC_Time(&Sys_Time);
				*ERTU_TIME_CHECK = 1;
				//Clear_Continue_Flag();
				//Clear_FrameFlags();
			}
			break;
		case C_RD_NA_2://读制造厂和产品规范
			c_TI = C_RD_NA_2;
			m_ACD = 1;
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_TI_NB_2:   //读电能量终端设备的当前系统时间
			printf("Read system time !\n");
			c_TI = C_TI_NB_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			E5H_Yes();
			//Command=C_CON_ACT;
			Command = C_DATA_TRAN;
			break;
		case C_CI_NC_2:     //制定地址范围的当前总电量
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_CI_NC_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_CI_NA_B_2://指定地址范围的当前分时电量
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			printf("process time of use !\n");
			c_TI = C_CI_NA_B_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_CI_NQ_2: //指定时间和地址范围的历史总电量
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_FCB_Tmp = c_FCB;
			printf("Process TE !\n");
			c_TI = C_CI_NQ_2;
			m_ACD = 1;
			//逻辑扩展地址,从1开始
			logic_Ertu_lo=*(data+10);
			logic_Ertu_hi=*(data+11);
			printf(LIB_DBG"逻辑设备地址 低:%d 高:%d\n"
				,logic_Ertu_lo,logic_Ertu_hi);
			c_Record_Addr = *(data+12);
			//扩展的终端逻辑地址,从1开始,当ioa每增加255时加1
			int ioa_weight;
			ioa_weight=(logic_Ertu_hi*255+logic_Ertu_lo)-1;
			c_Start_Info =ioa_weight*255+ *(data+13);
			c_Stop_Info =ioa_weight*255+ *(data+14);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_CI_TA_B_2://制定地址和时间范围的历史分时电量
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_FCB_Tmp = c_FCB;
			c_TI = C_CI_TA_B_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_YC_NA_2://指定地址范围的遥测当前值
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			printf("Process YC !\n");
			c_TI = C_YC_NA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_YC_TA_2://指定时间和地址范围的遥测历史值
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			printf("Process YC history!\n");
			c_TI = C_YC_TA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+10)+*(data+11)*256;
			c_Stop_Info = *(data+13)+*(data+14)*256;
			printf(
			                "In process yc history startinfo is %d,stopinfo is %d\n",
			                c_Start_Info,
			                c_Stop_Info);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_MNT_NA_2://指定地址范围的需量及时间当前值
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_MNT_NA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_MNT_TA_2://指定时间和地址范围的需量及时间历史值
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_MNT_TA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_XX_NA_2://指定地址范围内的四象限无功
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_XX_NA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_XX_TA_2://指定地址范围内的四象限无功历史值
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_XX_TA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_LTOU_TA_2://传送月末冻结电量
			printf("Process Ldd \n");
			c_TI = C_LTOU_TA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_PARA_TRAN://参数上传
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_PARA_TRAN;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			c_COT = *(data+9);
			printf("In long frame process m_cot_tmp=%d\n", c_COT);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_PARA_SET://参数设置
			c_TI = C_PARA_SET;
			m_ACD = 1;
			m_VSQ_tmp = *(data+8);
			c_Record_Addr = *(data+12);
			if ((*(data+14)<<8|*(data+13))==m_suppwd) {
				printf(
				                "In long frame process c_Record_Addr=%02x\n",
				                c_Record_Addr);
				m_COT_tmp = *(data+9);
				printf(
				                "In long frame process m_cot_tmp=%d\n",
				                m_COT_tmp);
				if (m_VSQ_tmp) {
					m_Now_Frame_Seq = m_Calc_Now_Frame_Seq(
					                data);
					printf(
					                "In long frame process m_Now_Frame_Seq=%d\n",
					                m_Now_Frame_Seq);
					Update_Cfg_File(data+15);
				}
				E5H_Yes();
				Command = C_CON_ACT;
			}
			else {
				m_Update_Flag = 0;
				M_Cfg_Para_Update_NA2(1);
			}
			break;
		case C_CTL_MON_ON:

			c_TI = C_CTL_MON_ON;
			PORT_ON = 1;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			E5H_Yes();
			printf(
			                "CTL_PORT_MON begin,POrt_ON=%d,PORT_NO=%d\n",
			                PORT_ON,
			                c_Record_Addr);
			Command = C_CON_ACT;
			break;
		case C_CTL_MON_OFF:
			c_TI = C_CTL_MON_OFF;
			PORT_ON = 0;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			//E5H_Yes();
			//Command=C_CON_ACT;
			Send_MFrame(10);
			break;
		case C_SP_NA_2:
			c_TI = C_SP_NA_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_SP_NB_2:
			/*
			 if(c_FCB_Tmp==c_FCB)m_Resend=1;
			 else
			 m_Resend=0;
			 c_FCB_Tmp=c_FCB;
			 */
			c_TI = C_SP_NB_2;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			c_Start_Info = *(data+13);
			c_Stop_Info = *(data+14);
			Read_Command_Time(data);
			E5H_Yes();
			Command = C_CON_ACT;
			break;
		case C_FILE_TRAN:
			c_TI = C_FILE_TRAN;
			m_Update_Flag = 0;
			m_ACD = 1;
			c_Record_Addr = *(data+12);
			m_Now_Frame_Seq = (*(data+13))*0x100+*(data+14);
			m_Frame_Total = (*(data+15))*0x100+*(data+16);
			printf(
			                "In file tran m_now_frame =%04x,m_frame_total=%04x\n",
			                m_Now_Frame_Seq,
			                m_Frame_Total);
			//if(!m_Now_Frame_Seq)
			//Close_Task();//close contrltsk
			m_Update_Flag = M_Write_Main_File(data);
			if (m_Resend>=3){
				M_Soft_Update_Can();
				m_Resend = 0;
				Main_Update_OK = 0;
			}else{
				M_Cfg_Para_Update_NA2(0);
			}
			if (m_Now_Frame_Seq==m_Frame_Total-1&&m_Update_Flag) {
				Main_Update_OK = 1;
				rename(MainFilebak, MainFile);
			}
			break;
		case C_Soft_Reset:
			*Reboot_Flag = 1;
			Clear_FrameFlags();
			break;
		default:
			break;
		}
	}
	return 0;
}

int CDl719s::dl719_Process(unsigned char* data, unsigned char len)
{
	unsigned char ret;
	/*if((0x16!=*(data+len-1))||(Pro102_check_sum(data,len))){
	 return -1;
	 }*/
	if (0x16!=*(data+len-1)) {
		return -1;
	}
	if (0x68==*data) {
		if (((*(data+5)==c_Link_Address_L)&&(*(data+6)==c_Link_Address_H))||((*(data+5)==0xff)||(*(data+6)==0xff))) {
			mirror_buf[0] = len;
			memcpy(&mirror_buf[1], data, len);		//
			printf(LIB_INF"长帧\n");
			ret = Process_Long_Frame(data);			//
			return 1;
		} else {
			return -3;
		}
	} else if (0x10==*data) {
		printf(LIB_INF"短帧\n");
		ret = Process_Short_Frame(data);
		return 1;
	}
	return -2;
}
///打印数组
int printArray(unsigned char* a, int n)
{

	for (int i = 0; i<n; i++) {
		printf("%02X ", a[i]);
	}
	//printf("\n");
}
///打印数组
int fprintArray(FILE* fp,unsigned char* a, int n)
{

	for (int i = 0; i<n; i++) {
		fprintf(fp,"%02X ", a[i]);
	}
	return 0;
	//printf("\n");
}
int fprintf_now(FILE *fp)
{
	time_t t;
	struct tm stm;
	t=time(NULL);
	gmtime_r(&t,&stm);
	fprintf(fp,"%02d-%02d %02d:%02d:%02d",
		/*stm.tm_year+1900,*/stm.tm_mon+1,stm.tm_mday,
		stm.tm_hour,stm.tm_min,stm.tm_sec);
	fflush(fp);
	return 0;
}
int CDl719s::dl719_Recieve()
{
	unsigned char len, reallen;
	unsigned char readbuf[255];
	len = get_num(&m_recvBuf);
	//printf(LIB_INF"len=%d\n",len);
	if (!len) {
		return -1;
	}
	while (len>=syn_char_num) {
		copyfrom_buf(readbuf, &m_recvBuf, len);
		printArray(readbuf, len);
		printf("\n");
		if (!dl719_synchead(readbuf)) {
			if (0x68==*readbuf) {
				expect_charnum = *(readbuf+1)+6;
			} else {
				expect_charnum = 6;
			}
			Syn_Head_Rece_Flag = 1;
			break;
		} else {
			pop_char(&m_recvBuf);
			len = get_num(&m_recvBuf);
		}
	}
	if (Syn_Head_Rece_Flag) {
		len = get_num(&m_recvBuf);
		if (len>=expect_charnum) {
			reallen = get_buff(readbuf, &m_recvBuf, expect_charnum);
			printf(LIB_INF"Rece <<< ");
			printArray(readbuf, reallen);
			printf("\n");
#if DEBUG_LOG
			fprintf(this->fp_log,"RX");
			fprintf(this->fp_log,"[");
			fprintf_now(this->fp_log);
			fprintf(this->fp_log,"] ");
			fprintArray(this->fp_log,readbuf, reallen);
			fprintf(this->fp_log,"\n");
			fflush(this->fp_log);
#endif
//			printf("dl719 recieve:");
//			for(int i=0;i<reallen;i++){
//				printf(" %02x",readbuf[i]);
//			}
//			printf("\n");
			len = dl719_Process(readbuf, reallen);
			printf(LIB_INF"Send >>> ");
			printArray(m_transBuf.m_transceiveBuf, m_transBuf.m_transCount);
			printf("\n");
#if DEBUG_LOG
			fprintf(this->fp_log,"TX");
			fprintf(this->fp_log,"[");
			fprintf_now(this->fp_log);
			fprintf(this->fp_log,"] ");
			fprintArray(this->fp_log,
				m_transBuf.m_transceiveBuf,  m_transBuf.m_transCount);
			fprintf(this->fp_log,"\n");
			fflush(this->fp_log);
#endif
			return len;
		} else {
			return -2;
		}
		Syn_Head_Rece_Flag = 0;
	}
	return -3;
}

void CDl719s::dl719_Assemble()
{
	//do nothing
}

void CDl719s::SendProc()
{
	//调用父类,是否发送心跳帧
	CBASE102::SendProc();
	dl719_Assemble();
}

int CDl719s::ReciProc()
{
	//gprs 心跳帧,父类判断/实现
	CBASE102::ReciProc();
	return dl719_Recieve();
}

extern "C" CProtocol *CreateCProto_dl719()
{
	PRINT_BUILD_TIME
	return new CDl719s;
}
