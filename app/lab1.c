#include "includes.h"

#define F_CPU   16000000UL   // CPU frequency = 16 Mhz
#include <avr/io.h>   
#include <util/delay.h>
#define TASK_STK_SIZE OS_TASK_DEF_STK_SIZE
/* task ���� ���� ������ �κ� */
#define N_TASKS 6
#define NULL (void*)0
#define ON 1 //SW1 �� ���� �� ��, ���� �ϰ� �ִ� ���¶�� �ǹ̷� ONOFFstate�� ����
#define OFF 0 
#define SUC 1 //������ state
#define FAIL 0 //���н� state

volatile int fndcnt = 0; //����ȭ�� ���� �ð� ī��Ʈ ���ִ� ����. 1�� 1ms �ǹ�
volatile int buz_state = 0; //������ �︮�°��� ���� �ϱ� ���� ����
volatile int ONOFFstate = OFF; 
volatile int melody = 17; //TCNT0�� �־��� ��. ���ϴ� �� melody����.
int random_array[8]; //  standard ���� 8�� �־��� �迭
unsigned char digit[10] =
{ 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x27, 0x7f, 0x6f };
unsigned char fnd_sel[4] = { 0x01, 0x02, 0x04, 0x08 };
unsigned char fail[4] = { 0x71, 0x77, 0x06, 0x38 }; // fnd�� FAIL ����
char mel[3] = { 17, 77, 117 }; // �� �� �� -> ������ ������ ���� ����
char mel2[3] = { 117, 77,17 }; // �� �� �� -> ���н� ������ ���� ����

volatile int count = 0x00;//ONOFFstate�� ���� �ٲ��ֱ� ���� �̿��ϴ� ����. 
volatile int stand_cnt = 0x00; // standardnumber �� ���� �ε����� �������ֱ� ���� ���̴º���. SW4�� ���ͷ�Ʈ ����
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE];

OS_EVENT *Sem;                                  /*Semaphore*/

												//mailbox
OS_EVENT *Mbox; //standard number : randomtask->counttask�� ������
OS_EVENT *NOTUSE;// randomtask�� counttask���� pend�����θ� ���� Mailbox

OS_EVENT *MQNumber; // SUC�� FND���� decide_number ������ : �޼���ť
OS_EVENT *MQState; //SUC / FAIL�� �Ѱ��ִ� �޼���ť

/*message queue*/
void *MQ_number[1];
void *MQ_state[N_TASKS];


//OS Event flag : synchronization
OS_FLAG_GRP *flag_grp;



void RandomTask(void *pdata);
void CountTask(void *pdata);

ISR(INT4_vect) { //SW1 : state�� ���� OFF -> ON
	count++;
	if (count > 0)
		ONOFFstate = ON;
}

ISR(INT5_vect) { //SW2 : standard_number ���ϱ� ���� 0~7���� �ݺ��ϴ� ������ ������
	//���� �� ����
	//
	stand_cnt++;


}

void setup() {

	DDRA = 0xff; //led 8�� ���� �� ���

	//fnd ���� ���
	DDRC = 0xff; //fnd�� ���� �ѱ����� ��� ��Ʈ�� ����
	DDRG = 0x0f; //� fnd�� ������ ������ 

	 //buzzer setting
	DDRB = 0x10; //���� ��� ���� PB4

	// //timer
	TCCR2 = 0x03; // 32����
	TIMSK |= 0x40; // timer 2  overflow interrupt ����


	 //SW1,2 ����
	DDRE = 0xcf;
	EIMSK = 0x30;
	EICRB = 0x0a;
	SREG |= 1 << 7;
}

/* OSEvent ����� �Լ��� */
void RandomTask(void*pdata) { // random_arryay[8] ���� �� ä���ֱ�
							  //���� �� �ϳ� �̾Ƽ� counttask�� ������ : mailbox��� + eventflag���
	int i;
	int err;
	int flags;
	int randIndex, standard_num;

	INT8U*data = (INT8U*)pdata;


	while (1) { 
		OSSemPend(Sem, 0, &err); // �������� ����
		//SW2�� ������, 0~7 ���� ���� i�� ���߰� �� i �� �����ε����� �ȴ�.
		while (stand_cnt == 0) {
			for (i = 0; i < 8; i++) {
				if (stand_cnt > 0) {
					randIndex = i;
					break;
				}
			}
		}

		//random_array���� ������ ���� �� �� ���� ��, standard_num���� �־���
		standard_num = random_array[randIndex];
		OSSemPost(Sem); //�������� ����

		//CountTask���� �迭���� ���� ���� ������
		OSMboxPost(Mbox, (void*)&standard_num); //���� ���� ���� �ڽ��� ���� decisiontask�� ������
		OSMboxPend(NOTUSE, 0, &err); // �� ���� �½�ũ�� ���ư��� �ֵ��� ������� �ʴ� ���Ϲڽ��� pend �����ν� ��� ��ٷ���
		
	}
}

/*decide_num�� �̰�, ��, �׸��� �� ������ �½�ũ���� ������ �ѱ�� ���� ��*/
void CountTask(void *pdata) {
	pdata = pdata;
	int err, decide_num, i, standard_num;
	int sucNum; //������ decide_num
	int resultstate = 0;

	while (1) {
		
		//randomtask���� array[8] ���� standard number �޾ƿ� 
		standard_num = *(int *)OSMboxPend(Mbox, 0, &err);

		//decide a number
		while (ONOFFstate == OFF) {
			PORTA = 0x01; //SW1�� ���ͷ�Ʈ�� ��ٸ��»��¶�� ���� �˷��ֱ� ���� led���
			for (i = 0; i < 100; i++) {
				decide_num = i;
				if (ONOFFstate == ON)
					break;
			}

		}
		//if decide_num > standard_num ==> we win! => SUC
		if (decide_num > standard_num) { //������
			resultstate = SUC;
			sucNum = decide_num;
		}
		else { //���н�
			resultstate = FAIL;

		}

		if (resultstate == SUC) { //fnd���� ������ ��
			OSQPost(MQNumber, (void*)&sucNum); // decide number�� �޼��� ť�� ���ؼ� ����

		}
		for (i = 0; i < 3; i++)
			OSQPost(MQState, (void*)&resultstate); //SUC/FAIL �� MQState�� ���ؼ� result state���� -> led, fnd, buzzer  

		OSMboxPend(NOTUSE, 0, &err); //��� ��ٸ���
	}

}
// 2018.12.15 06:32 AM ������� ��




void ledfunc() {
	int result_state, err;
	int value = 0x80;
	int value2 = 0x55;
	int chkf = 0;
	result_state = *(int*)OSQPend(MQState, 0, &err); // counttask���� 
	while (1) {
		if (ONOFFstate == ON) {
			if (result_state == SUC) {//������
				if (fndcnt % 10 == 0) { //10ms �϶� ����
					PORTA = value;
					value >>= 1;  // ��ĭ�� ���������� shift
					if (value == 0) // ������ ���� ���
						value = 128; //���� ������ �ʱ�ȭ
				}

			}
			else if (result_state == FAIL) { //���н�
				if (fndcnt % 100 == 0) { //100ms -> 1�� ��
					if (value2 == 0xaa)
						value2 = 0x55;
					else
						value2 = 0xaa;
					PORTA = value2;
				}

			}
		}
		OSFlagPost(flag_grp, 0x01, OS_FLAG_SET, &err); //led->fnd event flagset����
		OSFlagPend(flag_grp, 0x04, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err); // buzzer -> led �½�ũ ������ �Ѿ���� ��ٸ� 
	}
}


void fndfunc() {
	int i, fnd[4];
	int err, randnum;

	int result_state = *(int*)OSQPend(MQState, 0, &err); // SUC / FAIL ���¸� �޾ƿö����� ��ٸ�.
	if (result_state == SUC) { //������ ��� 
		randnum = *(int*)OSQPend(MQNumber, 0, &err); //decide_num�޾ƿ�
	}
	while (1) {
		OSFlagPend(flag_grp, 0x01, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err);//led�� ���� �½�Ʈ ������ ��ٸ� 
		if (result_state == SUC) { // ������ : fnd�� ���� ���
								   //���ڴ� 0~99 ���� ���̹Ƿ� fnd[0], fnd[1]�� ���
			fnd[3] = 0x00; // õ �ڸ�  = 0
			fnd[2] = 0x00; //���ڸ� = 0
			fnd[1] = (randnum / 10); // ���� �ڸ�
			fnd[0] = randnum % 10; // ���� �ڸ�

			for (i = 0; i < 4; i++) {
				PORTC = digit[fnd[i]];
				PORTG = fnd_sel[i];
				_delay_us(2500);

			}
			fndcnt++; // fndcnt == 1 => 1ms (100�� �� 1��)
		}
		else if (result_state == FAIL) { // ���н�  : fnd�� FAIL ���

			for (i = 0; i < 4; i++) {
				PORTC = fail[i];
				PORTG = fnd_sel[3 - i];
				_delay_us(2500);
			}
			fndcnt++;
		}
		OSFlagPost(flag_grp, 0x02, OS_FLAG_SET, &err); // fnd->buzzer �� �˸��� ���� �̺�Ʈ �÷���
	}
}


void buzzerfunc() {
	int i = 1, err;
	int j = 0;

	int result_state = *(int*)OSQPend(MQState, 0, &err); //SUC/ FAIL ���� �޾ƿö����� ��ٸ�

	while (1) {
		OSFlagPend(flag_grp, 0x02, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err); //fnd�½�ũ�� �ѹ� �� �������� pend
		if (result_state == SUC) { //������
			if (fndcnt % 200 == 0) { // 2�ʸ���
					melody = mel[i]; //��ε� �� �� �� �ݺ�
					i = (i + 1) % 3;
			}
		}
		else if (result_state == FAIL) {//���н�
			if (j == 0) {
				melody = 117; //ù �� �÷� ����
				j++;
			}
			if (fndcnt % 200 == 0) { // 2�ʸ���
				melody = mel2[i]; // ���ĵ� �ݺ�
				i = (i + 1) % 3;
			}
		}
		OSFlagPost(flag_grp, 0x04, OS_FLAG_SET, &err); //���� �������� �˸��� led���� task�纸. 
	}
}

ISR(TIMER2_OVF_vect) { 
	if (ONOFFstate == ON) {
		if (buz_state == 0) { //off state
			PORTB |= 1 << 4; //buzzer on
			buz_state = 1;
		}
		else if (buz_state == 1) { // on state
			PORTB &= ~(1 << 4); //buzzzer off
			buz_state = 0;
		}

		TCNT2 = melody; //�ش� melody���� ���� 
	}
}

int main() {
	int err, i;

	OSInit();
	OS_ENTER_CRITICAL();
	TCCR0 = 0x07; 
	TIMSK = _BV(TOIE0);
	TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
	OS_EXIT_CRITICAL();
	/* setup for ATmega128 */
	setup();
	/*random_array ���� ���� �־���*/
	for (i = 0; i < 8; i++) {
		random_array[i] = rand() % 100;
	}

	/*create semaphore, mailbox, message queue, event flag*/
	Sem = OSSemCreate(1);

	Mbox = OSMboxCreate(NULL);
	NOTUSE = OSMboxCreate(NULL);
	
	MQNumber = OSQCreate(MQ_number, 1);
	MQState = OSQCreate(MQ_state, N_TASKS);//led fnd buzzer =3
	//init flag , create them
	flag_grp = OSFlagCreate(0x00, &err);

	//create task
	OSTaskCreate(RandomTask, (void*)0, (void*)&TaskStk[0][TASK_STK_SIZE - 1], 0);
	OSTaskCreate(CountTask, (void*)0, (void*)&TaskStk[1][TASK_STK_SIZE - 1], 1);
	OSTaskCreate(ledfunc, (void*)0, (void*)&TaskStk[2][TASK_STK_SIZE - 1], 2);
	OSTaskCreate(fndfunc, (void*)0, (void*)&TaskStk[3][TASK_STK_SIZE - 1], 3);
	OSTaskCreate(buzzerfunc, (void*)0, (void*)&TaskStk[4][TASK_STK_SIZE - 1], 4);

	OSStart();

	return 0;
}