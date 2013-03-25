#ifndef DL719S_H
#define DL719S_H
#include "CBASE102s.h"
//监视方向
#define M_SP_TA_2 1  //单点信息
#define M_IT_TA_2 2  //记帐(计费)电能累计量，每个量为四个八位位组
#define M_IT_TD_2 5  //周期复位电能量信息，4字节
#define M_EI_NA_2 70 //初始化结束
#define P_MP_NA_2 71 //制造厂和设备规范
#define M_TI_NA_2 72 //终端设备当前系统时间
//#define M_IT_TA_B_2  125 //对应于120 for debug 061215
#define M_IT_NA_B_2 159//对应于169
#define M_IT_TA_B_2 160//对应于170
#define M_YC_NA_2 161//对应于171
#define M_YC_TA_2 162//对应于172
#define M_MNT_NA_2 163//对应于173
#define M_MNT_TA_2 164//对应于174
#define M_XX_NA_2 166//对应于176
#define M_XX_TA_2 167//对应于177
#define M_LTOU_TA_2 168//对应于178
#define M_FILE_TRAN 165//文件传输
#define M_TC_TA_1 201 //透抄表计当前数据
//控制方向
#define C_RD_NA_2 100 //读制造厂和产品规范
#define C_SP_NA_2 101 //读带时标的单点信息的记录
#define C_SP_NB_2 102//读一个选定时间范围内的带时标的单点信息的记录
#define C_TI_NB_2 103//读电能量终端设备的当前系统时间
#define C_CI_NC_2 107 //制定地址范围的当前总电量
#define C_CI_NQ_2 120//指定时间和地址范围的历史总电量
//#define C_CI_NR_2 121//读负荷曲线
#define C_SYN_TA_2  72   //系统时间同步
#define C_CI_NA_B_2 169//指定地址范围的当前分时电量
#define C_CI_TA_B_2 170//制定地址和时间范围的历史分时电量
#define C_YC_NA_2 171//指定地址范围的遥测当前值
#define C_YC_TA_2 172//指定时间和地址范围的遥测历史值
#define C_MNT_NA_2 173//指定地址范围的需量及时间当前值
#define C_MNT_TA_2 174//指定时间和地址范围的需量及时间历史值
#define C_XX_NA_2  176  //指定地址范围内的四象限无功
#define C_XX_TA_2  177  //指定地址范围内的四象限无功历史值
#define C_LTOU_TA_2 178  //传送月末冻结电量
#define C_TC_NA_1 211 //透抄表计当前数据
//#define C_FILE_TRAN 175//文件传输
//#define C_PARA_TRAN 190//参数上装
//#define C_PARA_SET    191//参数下装
//#define C_CTL_MON_ON	192	/*控制端口报文监视开*/
//#define C_CTL_MON_OFF	193 /*控制端口报文监视关*/

class CDl719s: public CBASE102
{
public:
	//CDl719s(unsigned short ertu_addr,unsigned char flag);
	CDl719s();
	~CDl719s();
	virtual int Init(struct stPortConfig *tmp_portcfg);
	void SendProc();
	int ReciProc();
protected:

	void Send_MFrame(unsigned char cot);
	int dl719_Recieve();
	void dl719_Assemble();
	int dl719_Process(unsigned char* data, unsigned char len);

	int dl719_synchead(unsigned char* data);

	int M_IT_NA_2(unsigned char flag);
	int M_IT_NA_T2(unsigned char flag);
	int M_SSZ_NA_2();     //传送当前遥测量
	int M_SSZ_NA_T2();     //传送历史遥测量

	int M_MaxND_NA_2();     //传送当前需量
	int M_MaxND_NA_T2();     //传送历史 需量
	int M_QR_NA_2();     //送当前象限无功
	int M_QR_NA_T2();     //传送历史象限值
	void Clear_Continue_Flag();
	void Clear_FrameFlags();
	void Read_Command_Time(unsigned char * data);
	int Process_Short_Frame(unsigned char *data);
	int Process_Long_Frame(unsigned char *data);
	//void Clear_FrameFlags();
	void C_PL1_NA2();
	int M_SYN_TA_2();
	int M_Return_SysTime();
	int M_SP_TA_2N();
	unsigned long RealTime_SSZ_Get(unsigned char Meter_No, unsigned char point);
	/*------------------------------------------------------
	 主站命令镜像
	 --------------------------------------------------------*/
	//unsigned char  mirror_buf[64];
	//unsigned char  last_send_buf[256];

//unsigned char Main_Update_OK;
	/*----------------------------------------------------
	 控制端口报文监视开关0:off,1:on
	 ---------------------------------------------------------------*/
	unsigned char PORT_ON;
private:
};
#endif

