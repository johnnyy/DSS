#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

/*
*   Developed by:
*       - José Robertty de Freitas Costa (graduating computer engineering)
*       - Johnny Marcos Silva Soares (graduating computer engineering)
*/

/*
 * Este presente trabalho foi proposto na disciplina Sistemas Operacionais 2, onde visa adaptar o RTOS didático DICK (visto na sala
 * de aula). As adaptações são: 
 * - Os semáforos devem aplicar a política de herança de prioridades
 * - O escalonador deve suportar tarefas aperiódicas. Escolha um escalonador de tarefa aperiódica dinâmica e implemente.
 * - Suporte ao CAB (Ciclical Assyncronous Buffer)
 * - O tick de relógio será efetuado através da chamada de uma função, logo implemente em seu programa principal as chamadas do tick 
 *   de relógio.
 * Considerações:
 * - O time_unit (tempo de tick) foi fixado como 1
 * - Só tarefas periódicas tem acesso a semáforos
 * - Só tarefas aperiódicas tem acesso ao CAB
 * - Utilizamos o escalonamento DSS
 * - A cada tick temos a probabilidade de 1 / VALUE_MOD de acontecer uma ativação aperiódica
 * - Todas as tarefas periódicas são ativadas em sys_clock = 0 (no tempo zero do sistema)
 * - Ele irá executar durante TIME_LIMIT ticks de tempo
 * - A capacidade do servidor é de 4 ticks de execução
 */

// leitura ou escrita
#define MODE_READING 0
#define MODE_WRITING 1

#define MAXLEN 20
#define MAXPROC 32
#define MAXSEM  32
// Quantidade de registos para troca de contexto
#define MAXREGISTERS 10
#define MAXTASKSSERVER 4
#define MAXBUFFER 5
#define MAXDLINE 0x7FFFFFFF
#define PRT_LEV 255
#define NIL -1
#define TRUE 1
#define FALSE 0
#define LIFETIME (MAXDLINE - PRT_LEV)

// Capacidade do servidor (tempo que ele pode suportar tarefas)

#define CAPACITY_SERVER 4

// valores para decidir se a tarefa aperiódica executa ou não

#define VALUE_RANDOM 1
#define VALUE_MOD 3

// tempo limite de execução

#define TIME_LIMIT 30


/*
 * TASKS TYPES 
*/

#define PERIODIC 1
#define APERIODIC 2
#define SERVER 3

/*
 * TASKS STATES
*/

#define FREE 0
#define READY 1
#define RUN 2
#define SLEEP 3
#define IDLE 4
#define WAIT 5
#define ZOMBIE 6

/*
 * ERROR MESSAGE
 */
 
#define OK 0
#define TIME_OVERFLOW -1
#define TIME_EXPIRED -2
#define NO_GUARANTEE -3
#define NO_TCB -4
#define NO_SEM -5
#define NO_TASK_SERVER -6

typedef int queue;
typedef int sem;
typedef int proc;

typedef struct buff{
	int next;
	int use;
	char* data;
}buffer;
	
typedef buffer*	pointer;
typedef struct CAB{
	buffer buf[5];
	int free;
	int mrb;
	int max_buf;
	int cont;
}cab;

struct tcb {
	char name[MAXLEN+1];
	proc (*addr)();
	int type;
// vetor para saber o que acontece durante sua execução (usada para saber se tem acesso a semáforo ou não
	int executed[TIME_LIMIT];
// o tempo de execução da tarefa
	int moment_executed;
// usado para saber o tempo que falta para tarefa executar
	int time_in_executed;
// semafóro que ele tem acesso
	sem semaphore;
// armazena a prioridade real da tarefa no protocolo de herança de prioridade
	int priority_dynamic_semaphore;
// modo de operação (leitura ou escrita)
	int mode_operation;
// vetor com os caracteres de leitura
	char information[MAXLEN+1];
	int state;
	long dline;
	int period;
	int prt;
	int wcet;
	float util;
	int *context;
	proc next;
	proc prev;
};

struct scb{
// nome do semáforo (pra ficar fácil debugar)
	char name[MAXLEN+1];
// armazena o processo que tá com o controle do semáforo
	proc proc_executed;
	int	count;
	queue qsem;
	sem next;
};

struct tcb vdes[MAXPROC];
struct scb vsem[MAXSEM];
// registros para troca de contexto
int registers[MAXREGISTERS];

// vetor de tarefas aperiódicas
proc vdes_aperiodic[MAXTASKSSERVER];

// Quantidade de tarefas aperiodicas
int tasks_aperiodic;
// Quantidade de tarefas em execução
int tasks_executed;
// Aramazena a tarefa servidor
int task_server;
// Capacidade máxima do servidor
int capacity_server = CAPACITY_SERVER;
// Server para saber se o servidor tá em período de execução ou não
int flag_server;

proc pexe;
queue ready;
queue idle;
queue zombie;
queue freetcb;
queue freesem;
queue aperiodic;
float util_fact;

// nome da tarefa do servidor
char nameServer[MAXLEN+1]  = {'S','e','r','v','e','r',' ','S','p','o','r','a','d','i','c','\0'};

unsigned long sys_clock = 1;
float time_unit;

cab cab_system;

/*
 * DECLARATION FUNCTIONS
 */

void ini_system(void);
void save_context(void);
void load_context(void);
void insert(proc i, queue *que);
void insert_sem(proc i, queue *que);
proc extract(proc i, queue *que);
proc getfirst(queue *que);
proc search_server(queue *que);
long firstdline(queue *que);
int empty(queue *que);
void schedule(void);
void dispatch(void);
void wake_up(void);
proc create(char name[MAXLEN+1], proc (*addr)(), int type, float period, float wcet);
int guarantee(proc p);
int activate(proc p);
void sleep1(void);
void end_cycle(void);
void end_process(void);
void kill(proc p);
sem newsem(char name[MAXLEN-1], int n);
void delsem(sem s);
int wait(sem s);
void signal(sem s);
float get_time(void);
int get_state(proc p);
long get_dline(proc p);
float get_period(proc p);
void msgError(int msg);
cab createCab();
int reserve(cab* c);
void putmes(cab c, int p);
int getmes(cab* c);
void unget(cab* c, int p);


int main(){
	proc task;
	int tasks_periodic, semaphores, i;
	float timeComputer = 0;
	tasks_executed = 0;
	char name[MAXLEN+1];
	proc (*addr)(); 
	float period;
	float wcet;
	ini_system();
	time_unit = 1;
	printf("DICK adapted for use of traffic lights with priority inheritance and support for apaeriodic tasks\n\n\n");
	
	printf("\nEnter the amount of system semaphores: ");
	scanf("%d", &semaphores);
	if(semaphores > MAXSEM)
		msgError(NO_SEM);
	
	for(i = 0; i < semaphores; i++){
		printf("\n\nEnter the semaphore name: ");
		scanf("%s",name);
		sem semaphore = newsem(name, 0);
		printf("Semaphore %s shall be identified by the %d\n", name, semaphore);
	}
	
	printf("\nEnter the amount of system tasks periodic: ");
	scanf("%d",&tasks_periodic);
	tasks_executed += tasks_periodic;
	if(tasks_executed > MAXPROC)
		msgError(NO_TCB);

	for(i = 0; i < tasks_periodic; i++){
		printf("\n\nEnter the task name: ");
		scanf("%s",name);
		printf("Enter the task period(ms): ");
		scanf("%f",&period);
		printf("Enter the task computing time(ms): ");
		scanf("%f",&wcet);
		addr = malloc(sizeof(proc));
		task = create(name, addr, PERIODIC, period, wcet);
		insert(task, &ready);
	}
	
	printf("\nEnter the amount of system tasks aperiodic: ");
	scanf("%d",&tasks_aperiodic);
	
	if(tasks_aperiodic > MAXTASKSSERVER)
		msgError(NO_TASK_SERVER);
	if(tasks_aperiodic > 0)
		cab_system = createCab();
	for(i = 0; i < tasks_aperiodic; i++){
		printf("\n\nEnter the task name: ");
		scanf("%s",name);
		printf("Enter the task computing time(ms): ");
		scanf("%f",&wcet);
		addr = malloc(sizeof(proc));
		task = create(name, addr, APERIODIC, wcet, wcet);
		vdes_aperiodic[i] = task;
		timeComputer += wcet;
	}
	if(tasks_aperiodic > 0){
		printf("\n\nEnter the period (ms) of execution of the server charge: ");
		scanf("%f",&period);
		addr = malloc(sizeof(proc));
		task_server = create(nameServer, addr, SERVER, period, capacity_server);
		tasks_executed++;	
	}else{
		capacity_server = 0;
		period = 1;
		task_server = NIL;
	}

	system("clear");
	
	dispatch();
	while(sys_clock <= TIME_LIMIT){
		sleep(time_unit);
		wake_up();
	}
	
	return 0;
}


void ini_system(void){
	proc i;
	
	/* initialize the list of free TCBs and semaphores */
	for (i=0; i<MAXPROC-1; i++) 
		vdes[i].next = i+1;
	vdes[MAXPROC-1].next = NIL;
	for (i=0; i<MAXSEM-1; i++) 
		vsem[i].next = i+1;
	vsem[MAXSEM-1].next = NIL;
	
	ready = NIL;
	idle = NIL;
	zombie = NIL;
	aperiodic = NIL;
	freetcb = 0;
	freesem = 0;
	util_fact = 0;
	flag_server = FALSE;
}

void save_context(void){
	int *pc;
	pc = vdes[pexe].context;
	proc i;
	for(i = 0; i < MAXREGISTERS; i++)
		pc[i] = registers[i];
	vdes[pexe].context = pc;
}

void load_context(void){
	int *pc;
	pc = vdes[pexe].context;
	proc i;
	for(i = 0; i < MAXREGISTERS; i++)
		registers[i] = pc[i];
}

void insert(proc i, queue *que){
	
	long dl;
	int p;
	int q;

	p = NIL;
	q = *que;
	dl = vdes[i].dline;
	
	while ((q != NIL) && (dl >= vdes[q].dline)) {
		p = q;
		q = vdes[q].next;
	}
	if (p != NIL) vdes[p].next = i;
	else *que = i;
	if (q != NIL) vdes[q].prev = i;
	vdes[i].next = q;
	vdes[i].prev = p;
	
	// Diz em qual fila a tarefa foi inserida
	if(que == &ready)
		printf("Task: %s inserted successfully in READY!\n",vdes[i].name);
	else if(que == &idle)
		printf("Task: %s inserted successfully in IDLE!\n",vdes[i].name);
	else if(que == &zombie)
		printf("Task: %s inserted successfully in ZOMBIE!\n",vdes[i].name);
	else if(que == &freesem)
		printf("Task inserted successfully in FREESEM!\n");
	else if(que == &freetcb)
		printf("Task inserted successfully in FREETCB!\n");
	else if(que == &aperiodic)
		printf("Task APERIODIC inserted successfully in READY!\n");
}

// Insere como uma fila, muda as prioridades e indepente de deadline

void insert_sem(proc i, queue *que){
	int p;
	int q;

	p = NIL;
	q = *que;
	long dl = vdes[i].dline;
	while (q != NIL) {
		if(vdes[q].dline > dl){
			if(vdes[q].priority_dynamic_semaphore == NIL)
				vdes[q].priority_dynamic_semaphore = vdes[q].dline;
			vdes[q].dline = dl;
		}
		p = q;
		q = vdes[q].next;
	}
	if (p != NIL) vdes[p].next = i;
	else *que = i;
	if (q != NIL) vdes[q].prev = i;
	vdes[i].next = q;
	vdes[i].prev = p;
	
}

proc extract(proc i, queue *que){
	int p, q;
	p = vdes[i].prev;
	q = vdes[i].next;
	if (p == NIL) *que = q;
	else vdes[p].next = vdes[i].next;
	if (q != NIL) vdes[q].prev = vdes[i].prev;
	
	// Diz de qual fila a tarefa foi removida
	if(que == &ready)
		printf("Task: %s removed successfully in READY!\n",vdes[i].name);
	else if(que == &idle)
		printf("Task: %s removed successfully in IDLE!\n",vdes[i].name);
	else if(que == &zombie)
		printf("Task: %s removed successfully in ZOMBIE!\n",vdes[i].name);
	else if(que == &freesem)
		printf("Task removed successfully in FREESEM!\n");
	else if(que == &freetcb)
		printf("Task removed successfully in FREETCB!\n");
	else if(que == &aperiodic)
		printf("Task APERIODIC removed successfully in READY!\n");
	
	return(i);
}

/*
 * Busca a tarefa servidor em qualquer fila
 * 
 */

proc search_server(queue *que){
	int p;
	p = *que;
	while (p != NIL){
		if(vdes[p].type == SERVER)
			break;
		p = vdes[p].next;
	}
	return(p);
}

proc getfirst(queue *que) {
	int q;
	q = *que;
	if (q == NIL) return(NIL);
	*que = vdes[q].next;
	vdes[*que].prev = NIL;
	return(q);
}

long firstdline(queue *que){
	return(vdes[*que].dline);
}

int empty(queue *que){
	if (*que == NIL)
		return TRUE;
	else
		return FALSE;
}

void schedule(void){
	if(pexe == NIL)
		dispatch();
	else if(firstdline(&ready) < vdes[pexe].dline){
		vdes[pexe].state = READY;
		insert(pexe, &ready);
		dispatch();
	}
}

void dispatch(void){
	pexe = getfirst(&ready);
	if(pexe != NIL)
		vdes[pexe].state = RUN;
}

// lista todas as tarefas em seus respectivos estados

void list_state(void){
	int p;
	printf("\n\n STATE RUN: \n");
	if(pexe != NIL) printf("Task %s",vdes[pexe].name);
	printf("\n\n STATE READY: \n");
	p = ready;
	while(p != NIL){
		printf("Task %s\n",vdes[p].name);
		p = vdes[p].next;
	}
	printf("\n\n STATE IDLE: \n");
	p = idle;
	while(p != NIL){
		printf("Task %s\n",vdes[p].name);
		p = vdes[p].next;
	}
	printf("\n\n STATE SLEEP: \n");
	p = aperiodic;
	while(p != NIL){
		printf("Task %s\n",vdes[p].name);
		p = vdes[p].next;
	}
	printf("\n\n STATE ZOMBIE: \n");
	p = zombie;
	while(p != NIL){
		printf("Task %s\n",vdes[p].name);
		p = vdes[p].next;
	}
}

// interrupção por timer

void wake_up(void){
	
	proc p;
	int count = 0;
	
	if (sys_clock >= LIFETIME) 
		msgError(TIME_EXPIRED);
	
	printf("\n\n\n\n\n");
	//system("clear");
	
	printf("\n\nTime System: %ld \n\n",sys_clock);
	// serve para forçar a alguem entrar em execução
	int flag = TRUE;
	while(flag){
		if(pexe != NIL && vdes[pexe].type == PERIODIC){ 
			int return_wait = -2;
			// verifica se no momento da execução da tarefa tem acesso a um semáforo
			if(vdes[pexe].executed[vdes[pexe].moment_executed] != NIL && (vdes[pexe].moment_executed  == 0 || vdes[pexe].executed[vdes[pexe].moment_executed - 1] == NIL)){
				return_wait = wait(vdes[pexe].semaphore);
			}
			if(return_wait != NIL || vdes[pexe].executed[vdes[pexe].moment_executed] == NIL){
				flag = FALSE;
				// decremeta o tempo que sobra de execução
				vdes[pexe].time_in_executed = vdes[pexe].time_in_executed - 1;
				// incrementa o momento da execução
				vdes[pexe].moment_executed++;
				// Observa se ele já gastou o tempo de execução no semáforo
				if(vdes[pexe].type == PERIODIC && vdes[pexe].executed[vdes[pexe].moment_executed - 1] != NIL){
					if(vdes[pexe].executed[vdes[pexe].moment_executed] == NIL){
						signal(vdes[pexe].semaphore);
					}
				}
			}
		}else{
			flag = FALSE;	
		}
	}
	if(pexe != NIL) printf("Task %s running at time %ld \n",vdes[pexe].name,sys_clock);
	// Ver o tempo de execução da tarefa que tá executando no servidor
	if(pexe != NIL){
		if(vdes[pexe].type == SERVER){
			vdes[aperiodic].time_in_executed = vdes[aperiodic].time_in_executed - 1;
		}
		save_context();
	}
	// para gerar realmente aleatório
	srand((unsigned)time(NULL));
	// ativação de uma tarefa aperiódica
	if(rand() % VALUE_MOD == VALUE_RANDOM && task_server != NIL){
		proc t_aperiodic = vdes_aperiodic[rand() % tasks_aperiodic];
		printf("Activation of the aperiodic task %s \n", vdes[t_aperiodic].name);
		insert(t_aperiodic, &aperiodic);
		if(vdes[pexe].type != SERVER && search_server(&ready) == NIL){
			insert(task_server, &ready);
			if(!flag_server){
				vdes[task_server].dline = sys_clock + (long)vdes[task_server].period;
				vdes[task_server].time_in_executed = vdes[task_server].period;
			}
			schedule();
		}
		flag_server = TRUE;
	}
	// flag_server serve para controla o momento onde vai acontecer a restauração do servidor
	if(flag_server) vdes[task_server].time_in_executed--;
	if(vdes[task_server].time_in_executed == 0 && flag_server == TRUE){
		printf("Restoring server capacity!\n");
		vdes[task_server].time_in_executed = capacity_server;
		flag_server = FALSE;
	}
	if(vdes[pexe].type == SERVER){
		if(vdes[aperiodic].mode_operation == MODE_READING){
			int texto_saida = getmes(&cab_system);
			printf("message %s received\n",cab_system.buf[texto_saida].data); 
			unget(&cab_system, texto_saida);
		}else{
			int buffer1 = reserve(&cab_system);
			strcpy(cab_system.buf[buffer1].data,vdes[aperiodic].information);
			printf("Buffer: %s, USE: %d, NEXT: %d\n",cab_system.buf->data,cab_system.buf[buffer1].use,cab_system.buf[buffer1].next);
			putmes(cab_system, buffer1);
		}
	}
	
	if(pexe != NIL){ 
		if(vdes[pexe].type == PERIODIC && vdes[pexe].time_in_executed == 0)	
			end_cycle();
		else if(vdes[pexe].type == SERVER && vdes[aperiodic].time_in_executed == 0){
			vdes[aperiodic].time_in_executed = vdes[aperiodic].wcet;
			getfirst(&aperiodic);
			if(aperiodic == NIL){
				pexe = NIL;
				schedule();
				vdes[pexe].time_in_executed = vdes[pexe].wcet;
				vdes[pexe].dline += (long)vdes[pexe].period;
				vdes[pexe].state = RUN;
			}
		}
	}
	if (pexe != NIL && vdes[pexe].type == PERIODIC)
		if (sys_clock > vdes[pexe].dline)
			msgError(TIME_OVERFLOW);
	while (!empty(&zombie) && (firstdline(&zombie) <= sys_clock)){
		p = getfirst(&zombie);
		util_fact = util_fact - vdes[p].util;
		vdes[p].state = FREE;
		insert(p, &freetcb);
	}
	while (!empty(&idle) && (firstdline(&idle) <= sys_clock)) {
		p = getfirst(&idle);
		vdes[p].time_in_executed = vdes[p].wcet;
		vdes[p].dline += (long)vdes[p].period;
		if(vdes[p].executed[vdes[p].moment_executed] != NIL && vdes[p].executed[vdes[p].moment_executed - 1] != NIL && vdes[p].executed[vdes[p].moment_executed] == vdes[p].executed[vdes[p].moment_executed - 1]){
			sem semaphore_aux = vdes[p].executed[vdes[p].moment_executed];
			int q;
			q = vsem[semaphore_aux].qsem;
			long min_deadline = vdes[p].dline;
			while (q != NIL) {
				if(vdes[q].dline < min_deadline){
					min_deadline = vdes[q].dline;
				}
				q = vdes[q].next;
				if(min_deadline < vdes[p].dline){
					vdes[p].priority_dynamic_semaphore = vdes[p].dline;
					vdes[p].dline = min_deadline;
				}
			}
			
		}
		vdes[p].state = READY;
		insert(p, &ready);
		count++;
	}

	if (count > 0) 
		schedule();
	if(pexe != NIL) load_context();

	list_state();
	sys_clock++;
}

proc create(char name[MAXLEN+1], proc (*addr)(), int type, float period, float wcet){
	sem semaphore = NIL;
	int number_utility_sem, i;
	proc p;
	p = getfirst(&freetcb);
	for(i = 0; i < TIME_LIMIT; i++)
		vdes[p].executed[i] = NIL;
	if(type == APERIODIC){
		int mode = NIL;
		while(mode != MODE_READING && mode != MODE_WRITING){
			printf("Enter 0 for read mode or 1 for write mode: ");
			scanf("%d",&mode);
			if(mode != MODE_READING && mode != MODE_WRITING) 
				printf("\nMode operation invalid!!! \n\n");
		}
		if(mode == MODE_WRITING){
			printf("Enter the information you want to insert: ");
			scanf("%s",vdes[p].information);
		}
		vdes[p].mode_operation = mode;
	}
	if(type == PERIODIC){
		printf("Enter -1 if the task does not use semaphore, otherwise enter the semaphore used: ");
		scanf("%d", &semaphore);
	}
	if(semaphore < NIL || semaphore >= MAXSEM)
		msgError(NO_SEM);
	if(semaphore != NIL){
		printf("Enter the number of times that the semaphore %s is used: ",vsem[semaphore].name);
		scanf("%d", &number_utility_sem);
		for(i = 0; i < number_utility_sem; i++){
			int start, end, j;
			printf("Enter the time interval of the first execution of the traffic light: ");
			scanf("%d %d", &start, &end);
			if(start < 0 || end < 0 || start > TIME_LIMIT || end > TIME_LIMIT || end < start){
				printf("\n Range presents error! \n");
				i--;
				continue;
			}
			for(j = start; j <= end; j++)
				vdes[p].executed[j] = semaphore;
		}
	}
	if (p == NIL) 
		msgError(NO_TCB);
	vdes[p].util = wcet / period;
	if (type == PERIODIC || type == SERVER)
		if (!guarantee(p)) 
			msgError(NO_GUARANTEE);
	for(i = 0; i < MAXLEN+1; i++)
		vdes[p].name[i] = name[i];
	
	vdes[p].addr = addr;
	vdes[p].type = type;
	if(type == PERIODIC)
		vdes[p].state = READY;
	else if(type == SERVER)
		vdes[p].state = IDLE;
	else
		vdes[p].state = APERIODIC;
	vdes[p].context = (int *)  malloc(sizeof(int) * MAXREGISTERS);
	vdes[p].period = (int)(period / time_unit);
	vdes[p].wcet = (int)(wcet / time_unit);
	vdes[p].time_in_executed = vdes[p].wcet;
	vdes[p].util = wcet / period;
	vdes[p].prt = (int)period;
	vdes[p].dline = sys_clock + period;
	vdes[p].semaphore = semaphore;
	vdes[p].moment_executed = 0;
	vdes[p].priority_dynamic_semaphore = NIL;
	return(p);
}

int guarantee(proc p){
	util_fact = util_fact + vdes[p].util;
	if(util_fact > 1.0) {
		util_fact = util_fact - vdes[p].util;
		return FALSE;
	}
	else return TRUE;
}

int activate(proc p){
	save_context();
	if(vdes[p].type == PERIODIC)
		vdes[p].dline = sys_clock + (long)vdes[p].period;
	vdes[p].state = READY;
	insert(p, &ready);
	schedule();
	load_context();
	return OK;
}

// Mudei o nome só pra não dá conflito com a função sleep do C

void sleep1(void){
	save_context();
	vdes[pexe].state = SLEEP;
	dispatch();
	load_context();
}

void end_cycle(void){
	long dl;
	save_context();
	dl = vdes[pexe].dline;
	vdes[pexe].time_in_executed = vdes[pexe].wcet;
	if (sys_clock < dl) {
		vdes[pexe].state = IDLE;
		insert(pexe, &idle);
	}else {
		dl = dl + (long)vdes[pexe].period;
		vdes[pexe].dline = dl;
		vdes[pexe].state = READY;
		insert(pexe, &ready);
	}
	dispatch();
}

void end_process(void){
	if (vdes[pexe].type == PERIODIC)
		insert(pexe, &zombie);
	else{
		vdes[pexe].state = FREE;
		insert(pexe, &freetcb);
	}
	dispatch();
	load_context();
}

void kill(proc p) {
	if (pexe == p) {
		end_process();
		return;
	}
	if (vdes[p].state == READY) extract(p, &ready);
	if (vdes[p].state == IDLE) extract(p, &idle);
	if (vdes[p].type == PERIODIC)
		insert(p, &zombie);
	else {
		vdes[p].state = FREE;
		insert(p, &freetcb);
	}
}

sem newsem(char name[MAXLEN-1], int n){
	int i;
	sem s;
	s = freesem;
	if (s == NIL) 
		msgError(NO_SEM);
	for(i = 0; i < MAXLEN+1; i++)
		vsem[s].name[i] = name[i];
	freesem = vsem[s].next;
	vsem[s].count = n;
	vsem[s].qsem = NIL;
	return(s);
}

void delsem(sem s){
	vsem[s].next = freesem;
	freesem = s;
}

int wait(sem s){
	printf("Task %s used semaphore %s \n",vdes[pexe].name,vsem[s].name);
	if (vsem[s].count >= 0){ 
		// Tarefa acessa livremente o servidor
		vsem[s].count --;
		vsem[s].proc_executed = pexe;
	}else{
		printf("Task blocked in semaphore %s!! \n",vsem[vdes[pexe].moment_executed].name);			
		save_context();
		vdes[pexe].state = WAIT;
		// Tarefa é bloqueada
		// mudança de prioridade com base na herança de prioridades
		if(vdes[pexe].dline < vdes[vsem[s].proc_executed].dline){
			vdes[vsem[s].proc_executed].priority_dynamic_semaphore = vdes[vsem[s].proc_executed].dline;
			vdes[vsem[s].proc_executed].dline = vdes[pexe].dline;
			int aux_return = extract(vsem[s].proc_executed,&ready);
			if(aux_return != NIL) insert(vsem[s].proc_executed,&ready);
		}
		// inserve na fila do semáforo
		insert(pexe, &vsem[s].qsem);
		// puxa alguém que vá executar 
		dispatch();
		if(pexe != NIL) load_context();
		return NIL; 
	}
	return OK;
}

void signal(sem s){
	printf("Task %s released semaphore %s \n",vdes[pexe].name,vsem[s].name);
	
	// Ajeita a prioridade da tarefa
	
	if(vdes[pexe].priority_dynamic_semaphore != NIL){
		vdes[pexe].dline = vdes[pexe].priority_dynamic_semaphore;
		vdes[pexe].priority_dynamic_semaphore = NIL;
	}
	
	if (!empty(&vsem[s].qsem)) {
		vsem[s].proc_executed = getfirst(&vsem[s].qsem);
		vdes[vsem[s].proc_executed].state = READY;
		insert(vsem[s].proc_executed, &ready);
		vsem[s].count++;
	}else{
		vsem[s].count++;
		vsem[s].proc_executed = NIL;
	}
}

float get_time(void){
	return(time_unit * sys_clock);
}

int get_state(proc p){
	return(vdes[p].state);
}

long get_dline(proc p){
	return(vdes[p].dline);
}

float get_period(proc p){
	return(vdes[p].period);
}


cab createCab(){
	cab c;
	c.max_buf = MAXBUFFER;
	c.cont= 0;
	int i;
	for(i = 0 ; i < MAXBUFFER; i++)
		c.buf[i].use = 0;
	for(i = 0; i < MAXBUFFER; i++){
		c.buf[i].data = (char*) malloc(sizeof(char)*30);
		if(i == MAXBUFFER - 1){
			c.buf[i].next = 0;
			break;
		}
		c.buf[i].next= i+1;
	}

	c.free = 0;
	c.mrb = 0;
	return c;
}

int reserve(cab* c){
	
	if(c->cont != 0){
		c->free = c->buf[c->free].next;
	}
		
	printf("Buffer reserved \n");
	return (c->free);
}

void putmes(cab c, int p){
	c.cont++;
	if(c.buf[c.mrb].use != 0){
		c.free = c.buf[c.free].next;
		c.mrb = p;
		printf("\nMessage %s sent\n",c.buf[c.mrb].data);
	}
	else{
		c.mrb = p;
		printf("\nMessage %s sent\n",c.buf[c.mrb].data);
	}
}

int getmes(cab* c){
	c->buf[c->mrb].use = c->buf[c->mrb].use + 1;
	printf("\nNumber of tasks accessed: %d\n",c->buf[c->mrb].use);
	return c->mrb;
	
}

void unget(cab* c, int p){
	c->buf[p].use = c->buf[p].use - 1;
	printf("\nReleased - number of tasks accessed: %d\n",c->buf[c->mrb].use);
	if((c->buf[p].use == 0) && (&c->buf[p] != &c->buf[c->mrb])) {
		c->free = p;
		printf("\nDeallocated message\n");
	}
}

void msgError(int msg){
	// Dá a mensagem de erro e cai fora
	system("clear");
	if(msg == OK){
		printf("Normal system operation!\n");
		return;
	}else if(msg == TIME_OVERFLOW)
		printf("Time exceeded, temporary requirement breach!\n");
	else if(msg == TIME_EXPIRED)
		printf("Lifetime of the finished system!\n");
	else if(msg == NO_GUARANTEE)
		printf("Processes are not scalable!\n");
	else if(msg == NO_TCB)
		printf("TCB not available!\n");
	else if(msg == NO_SEM)
		printf("SEM not available!\n");
	else if(msg == NO_TASK_SERVER)
		printf("TASK SERVER not available!\n");
	exit(0);
}
