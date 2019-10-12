#include "includes.h"

#define F_CPU   16000000UL   // CPU frequency = 16 Mhz
#include <avr/io.h>   
#include <util/delay.h>
#define TASK_STK_SIZE OS_TASK_DEF_STK_SIZE
/* task 숫자 세서 수정할 부분 */
#define N_TASKS 6
#define NULL (void*)0
#define ON 1 //SW1 를 눌러 준 뒤, 실행 하고 있는 상태라는 의미로 ONOFFstate의 상태
#define OFF 0 
#define SUC 1 //성공시 state
#define FAIL 0 //실패시 state

volatile int fndcnt = 0; //동기화를 위해 시간 카운트 해주는 변수. 1당 1ms 의미
volatile int buz_state = 0; //부저가 울리는것을 제어 하기 위한 변수
volatile int ONOFFstate = OFF; 
volatile int melody = 17; //TCNT0에 넣어줄 값. 변하는 건 melody값임.
int random_array[8]; //  standard 난수 8개 넣어줄 배열
unsigned char digit[10] =
{ 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x27, 0x7f, 0x6f };
unsigned char fnd_sel[4] = { 0x01, 0x02, 0x04, 0x08 };
unsigned char fail[4] = { 0x71, 0x77, 0x06, 0x38 }; // fnd용 FAIL 문자
char mel[3] = { 17, 77, 117 }; // 도 파 시 -> 성공시 나오는 부저 음계
char mel2[3] = { 117, 77,17 }; // 시 파 도 -> 실패시 나오는 부저 음계

volatile int count = 0x00;//ONOFFstate의 상태 바꿔주기 위해 이용하는 변수. 
volatile int stand_cnt = 0x00; // standardnumber 의 랜덤 인덱스를 결정해주기 위해 쓰이는변수. SW4로 인터럽트 받음
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE];

OS_EVENT *Sem;                                  /*Semaphore*/

												//mailbox
OS_EVENT *Mbox; //standard number : randomtask->counttask로 보내줌
OS_EVENT *NOTUSE;// randomtask와 counttask에서 pend용으로만 쓰일 Mailbox

OS_EVENT *MQNumber; // SUC시 FND에게 decide_number 보내줌 : 메세지큐
OS_EVENT *MQState; //SUC / FAIL를 넘겨주는 메세지큐

/*message queue*/
void *MQ_number[1];
void *MQ_state[N_TASKS];


//OS Event flag : synchronization
OS_FLAG_GRP *flag_grp;



void RandomTask(void *pdata);
void CountTask(void *pdata);

ISR(INT4_vect) { //SW1 : state을 설정 OFF -> ON
	count++;
	if (count > 0)
		ONOFFstate = ON;
}

ISR(INT5_vect) { //SW2 : standard_number 정하기 위한 0~7사이 반복하는 숫자중 무작위
	//멈춤 및 선택
	//
	stand_cnt++;


}

void setup() {

	DDRA = 0xff; //led 8개 전부 다 사용

	//fnd 전부 사용
	DDRC = 0xff; //fnd의 불을 켜기위한 출력 포트로 설정
	DDRG = 0x0f; //어떤 fnd를 선택할 것인지 

	 //buzzer setting
	DDRB = 0x10; //버저 출력 설정 PB4

	// //timer
	TCCR2 = 0x03; // 32분주
	TIMSK |= 0x40; // timer 2  overflow interrupt 설정


	 //SW1,2 설정
	DDRE = 0xcf;
	EIMSK = 0x30;
	EICRB = 0x0a;
	SREG |= 1 << 7;
}

/* OSEvent 사용할 함수들 */
void RandomTask(void*pdata) { // random_arryay[8] 생성 및 채워넣기
							  //난수 값 하나 뽑아서 counttask로 보내줌 : mailbox사용 + eventflag사용
	int i;
	int err;
	int flags;
	int randIndex, standard_num;

	INT8U*data = (INT8U*)pdata;


	while (1) { 
		OSSemPend(Sem, 0, &err); // 세마포어 입장
		//SW2을 누르면, 0~7 사이 돌던 i가 멈추고 그 i 가 랜덤인덱스가 된다.
		while (stand_cnt == 0) {
			for (i = 0; i < 8; i++) {
				if (stand_cnt > 0) {
					randIndex = i;
					break;
				}
			}
		}

		//random_array에서 결정된 값을 비교 할 기준 값, standard_num으로 넣어줌
		standard_num = random_array[randIndex];
		OSSemPost(Sem); //세마포어 퇴장

		//CountTask에게 배열에서 뽑은 난수 보내줌
		OSMboxPost(Mbox, (void*)&standard_num); //기준 값을 메일 박스를 통해 decisiontask에 보내줌
		OSMboxPend(NOTUSE, 0, &err); // 그 다음 태스크가 돌아갈수 있도록 사용하지 않는 메일박스를 pend 함으로써 평생 기다려줌
		
	}
}

/*decide_num을 뽑고, 비교, 그리고 각 나머지 태스크에게 정보들 넘기는 역할 함*/
void CountTask(void *pdata) {
	pdata = pdata;
	int err, decide_num, i, standard_num;
	int sucNum; //성공시 decide_num
	int resultstate = 0;

	while (1) {
		
		//randomtask에서 array[8] 에서 standard number 받아옴 
		standard_num = *(int *)OSMboxPend(Mbox, 0, &err);

		//decide a number
		while (ONOFFstate == OFF) {
			PORTA = 0x01; //SW1의 인터럽트를 기다리는상태라는 것을 알려주기 위해 led출력
			for (i = 0; i < 100; i++) {
				decide_num = i;
				if (ONOFFstate == ON)
					break;
			}

		}
		//if decide_num > standard_num ==> we win! => SUC
		if (decide_num > standard_num) { //성공시
			resultstate = SUC;
			sucNum = decide_num;
		}
		else { //실패시
			resultstate = FAIL;

		}

		if (resultstate == SUC) { //fnd에게 보내줄 값
			OSQPost(MQNumber, (void*)&sucNum); // decide number을 메세지 큐를 통해서 보냄

		}
		for (i = 0; i < 3; i++)
			OSQPost(MQState, (void*)&resultstate); //SUC/FAIL 을 MQState를 통해서 result state보냄 -> led, fnd, buzzer  

		OSMboxPend(NOTUSE, 0, &err); //평생 기다린다
	}

}
// 2018.12.15 06:32 AM 여기까지 함




void ledfunc() {
	int result_state, err;
	int value = 0x80;
	int value2 = 0x55;
	int chkf = 0;
	result_state = *(int*)OSQPend(MQState, 0, &err); // counttask에서 
	while (1) {
		if (ONOFFstate == ON) {
			if (result_state == SUC) {//성공시
				if (fndcnt % 10 == 0) { //10ms 일때 마다
					PORTA = value;
					value >>= 1;  // 한칸씩 오른쪽으로 shift
					if (value == 0) // 오른쪽 끝일 경우
						value = 128; //왼쪽 끝으로 초기화
				}

			}
			else if (result_state == FAIL) { //실패시
				if (fndcnt % 100 == 0) { //100ms -> 1초 시
					if (value2 == 0xaa)
						value2 = 0x55;
					else
						value2 = 0xaa;
					PORTA = value2;
				}

			}
		}
		OSFlagPost(flag_grp, 0x01, OS_FLAG_SET, &err); //led->fnd event flagset해줌
		OSFlagPend(flag_grp, 0x04, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err); // buzzer -> led 태스크 선점이 넘어오길 기다림 
	}
}


void fndfunc() {
	int i, fnd[4];
	int err, randnum;

	int result_state = *(int*)OSQPend(MQState, 0, &err); // SUC / FAIL 상태를 받아올때까지 기다림.
	if (result_state == SUC) { //성공일 경우 
		randnum = *(int*)OSQPend(MQNumber, 0, &err); //decide_num받아옴
	}
	while (1) {
		OSFlagPend(flag_grp, 0x01, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err);//led로 부터 태스트 끝나길 기다림 
		if (result_state == SUC) { // 성공시 : fnd에 난수 출력
								   //숫자는 0~99 사이 값이므로 fnd[0], fnd[1]만 사용
			fnd[3] = 0x00; // 천 자리  = 0
			fnd[2] = 0x00; //백자리 = 0
			fnd[1] = (randnum / 10); // 십의 자리
			fnd[0] = randnum % 10; // 일의 자리

			for (i = 0; i < 4; i++) {
				PORTC = digit[fnd[i]];
				PORTG = fnd_sel[i];
				_delay_us(2500);

			}
			fndcnt++; // fndcnt == 1 => 1ms (100개 시 1초)
		}
		else if (result_state == FAIL) { // 실패시  : fnd에 FAIL 출력

			for (i = 0; i < 4; i++) {
				PORTC = fail[i];
				PORTG = fnd_sel[3 - i];
				_delay_us(2500);
			}
			fndcnt++;
		}
		OSFlagPost(flag_grp, 0x02, OS_FLAG_SET, &err); // fnd->buzzer 를 알리기 위한 이벤트 플래그
	}
}


void buzzerfunc() {
	int i = 1, err;
	int j = 0;

	int result_state = *(int*)OSQPend(MQState, 0, &err); //SUC/ FAIL 상태 받아올때까지 기다림

	while (1) {
		OSFlagPend(flag_grp, 0x02, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err); //fnd태스크가 한번 다 돌때까지 pend
		if (result_state == SUC) { //성공시
			if (fndcnt % 200 == 0) { // 2초마다
					melody = mel[i]; //멜로디 도 파 시 반복
					i = (i + 1) % 3;
			}
		}
		else if (result_state == FAIL) {//실패시
			if (j == 0) {
				melody = 117; //첫 음 시로 설정
				j++;
			}
			if (fndcnt % 200 == 0) { // 2초마다
				melody = mel2[i]; // 시파도 반복
				i = (i + 1) % 3;
			}
		}
		OSFlagPost(flag_grp, 0x04, OS_FLAG_SET, &err); //부저 끝났음을 알리고 led에게 task양보. 
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

		TCNT2 = melody; //해당 melody음을 낸다 
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
	/*random_array 난수 만들어서 넣어줌*/
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