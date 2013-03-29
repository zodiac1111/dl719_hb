#ifndef DL719S_H
#define DL719S_H
#include "CBASE102s.h"
//���ӷ���
#define M_SP_TA_2 1  //������Ϣ
#define M_IT_TA_2 2  //����(�Ʒ�)�����ۼ�����ÿ����Ϊ�ĸ���λλ��
#define M_IT_TD_2 5  //���ڸ�λ��������Ϣ��4�ֽ�
#define M_EI_NA_2 70 //��ʼ������
#define P_MP_NA_2 71 //���쳧���豸�淶
#define M_TI_NA_2 72 //�ն��豸��ǰϵͳʱ��
//#define M_IT_TA_B_2  125 //��Ӧ��120 for debug 061215
#define M_IT_NA_B_2 159//��Ӧ��169
#define M_IT_TA_B_2 160//��Ӧ��170
#define M_YC_NA_2 161//��Ӧ��171
#define M_YC_TA_2 162//��Ӧ��172
#define M_MNT_NA_2 163//��Ӧ��173
#define M_MNT_TA_2 164//��Ӧ��174
#define M_XX_NA_2 166//��Ӧ��176
#define M_XX_TA_2 167//��Ӧ��177
#define M_LTOU_TA_2 168//��Ӧ��178
#define M_FILE_TRAN 165//�ļ�����
#define M_TC_TA_1 201 //͸����Ƶ�ǰ����
//���Ʒ���
#define C_RD_NA_2 100 //�����쳧�Ͳ�Ʒ�淶
#define C_SP_NA_2 101 //����ʱ��ĵ�����Ϣ�ļ�¼
#define C_SP_NB_2 102//��һ��ѡ��ʱ�䷶Χ�ڵĴ�ʱ��ĵ�����Ϣ�ļ�¼
#define C_TI_NB_2 103//���������ն��豸�ĵ�ǰϵͳʱ��
#define C_CI_NC_2 107 //�ƶ���ַ��Χ�ĵ�ǰ�ܵ���
#define C_CI_NQ_2 120//ָ��ʱ��͵�ַ��Χ����ʷ�ܵ���
//#define C_CI_NR_2 121//����������
#define C_SYN_TA_2  72   //ϵͳʱ��ͬ��
#define C_CI_NA_B_2 169//ָ����ַ��Χ�ĵ�ǰ��ʱ����
#define C_CI_TA_B_2 170//�ƶ���ַ��ʱ�䷶Χ����ʷ��ʱ����
#define C_YC_NA_2 171//ָ����ַ��Χ��ң�⵱ǰֵ
#define C_YC_TA_2 172//ָ��ʱ��͵�ַ��Χ��ң����ʷֵ
#define C_MNT_NA_2 173//ָ����ַ��Χ��������ʱ�䵱ǰֵ
#define C_MNT_TA_2 174//ָ��ʱ��͵�ַ��Χ��������ʱ����ʷֵ
#define C_XX_NA_2  176  //ָ����ַ��Χ�ڵ��������޹�
#define C_XX_TA_2  177  //ָ����ַ��Χ�ڵ��������޹���ʷֵ
#define C_LTOU_TA_2 178  //������ĩ�������
#define C_TC_NA_1 211 //͸����Ƶ�ǰ����
//��¼��־
#define DEBUG_LOG 1
#if DEBUG_LOG
#define HB719_debug_log  "/mnt/nor/goahead.log"
#endif
//#define C_FILE_TRAN 175//�ļ�����
//#define C_PARA_TRAN 190//������װ
//#define C_PARA_SET    191//������װ
//#define C_CTL_MON_ON	192	/*���ƶ˿ڱ��ļ��ӿ�*/
//#define C_CTL_MON_OFF	193 /*���ƶ˿ڱ��ļ��ӹ�*/
///��ӡ���빹�������ں�ʱ�䣬���ƣ�Dec  3 2012 09:59:57
#define PRINT_BUILD_TIME {					\
		printf(LIB_INF"Build time:\t"RED"%s %s"_COLOR"\n",\
		 __DATE__, __TIME__);				\
	}
/// ��ӡ�����ǰλ�õ��ļ�,����,�������� Debug Print
#define DP_HERE {						\
	printf(LIB_DBG"[File:%s Line:%d] Fun:%s .\n",	\
	__FILE__, __LINE__, __FUNCTION__);			\
	}
#define DP_RET(ret) {						\
	printf(LIB_DBG"[File:%s Line:%d] Fun:%s ret %d .\n",	\
	__FILE__, __LINE__, __FUNCTION__,(ret));		 	\
	}
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
	int M_SSZ_NA_2();     //���͵�ǰң����
	int M_SSZ_NA_T2();     //������ʷң����

	int M_MaxND_NA_2();     //���͵�ǰ����
	int M_MaxND_NA_T2();     //������ʷ ����
	int M_QR_NA_2();     //�͵�ǰ�����޹�
	int M_QR_NA_T2();     //������ʷ����ֵ
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
	int M_SP_NA_2N();//ȫ��������Ϣ
	unsigned long RealTime_SSZ_Get(unsigned char Meter_No, unsigned char point);
	/*------------------------------------------------------
	 ��վ�����
	 --------------------------------------------------------*/
	//unsigned char  mirror_buf[64];
	//unsigned char  last_send_buf[256];

//unsigned char Main_Update_OK;
	/*----------------------------------------------------
	 ���ƶ˿ڱ��ļ��ӿ���0:off,1:on
	 ---------------------------------------------------------------*/
	unsigned char PORT_ON;
	int Sequence_number;
	int logic_Ertu_lo;
	int logic_Ertu_hi;
	//��/�����й���+��ʱ �ļ�
	std::string   filename_tou;
	//4�����޹�
	std::string   filename_qr;
	std::string filename_mnt;//��������ļ�
	std::string filename_ta;//˲ʱ���ļ�
#if DEBUG_LOG
	//����Լ��־
	FILE *fp_log;
#endif
private:
};
#endif

