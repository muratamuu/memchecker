/*!
 * memory check
 * 20110412 1.0 new release
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <syscall.h>

// define declare
#define INVALID_TAGID (-1)
#define SHMKEY (54321)
#define MSGKEY (12345)

enum event_type
{
  EVENT_INITTAG = 1,
  EVENT_UPDATETAG,
  EVENT_LEAKCHECK,
  EVENT_RESULT,
};

// function declare
static void hook_init(void);
static void backup_org_hook(void);
static void hook_start(void);
static void hook_end(void);
static void* malloc_hook(size_t size, const void* ra);
static void* realloc_hook(void* ptr, size_t size, const void* ra);
static void* memalign_hook(size_t alignment, size_t size, const void* ra);
static void free_hook(void* ptr, const void* ra);
static void console_app(pid_t pid);
static void top_menu(void);
static int check_menu(void);
static void addctx(void* adr, size_t size, const void* ra);
static void delctx(void* adr);
static void alldelctx(void);
static void inittag(void);
static void updatetag(void);
static unsigned long counttag(int tagno);
static void notifytag(pid_t consolepid, int tagno);
static void printtag(int shmid, unsigned long tagcount);
static void printformat(struct memctx* ctx, unsigned long tagcount);
static void setsignal(void);
static void sighandler(int signame);
static void eventHandler(struct event* e);
static int send_event(pid_t pid, struct event* e);
static int recv_event(struct event* e);
static int init_event(pid_t pid);
static int update_event(pid_t pid);
static int check_event(pid_t pid, int tagno);
static int result_event(pid_t pid, int shmid, int tagcount);
static const char* gettname(void);
static const char* getpname(void);

// class & structure declare
struct event_data
{
  pid_t pid;
  int tagno;
  int shmid;
  unsigned long tagcount;
};

struct event
{
  long type;
  struct event_data data;
};

class semaphore_impl
{
public:
  semaphore_impl() {
    if ((semid_ = semget(IPC_PRIVATE, 1, 0666)) == -1) {
      perror("semget");
      return;
    }
    ctlarg_.val = 1; // initial condition -> signal
    if (semctl(semid_, 0, SETVAL, ctlarg_) == -1) {
      perror("semctl");
      return;
    }
  }

  ~semaphore_impl() {
    if (semctl(semid_, 0, IPC_RMID, ctlarg_) == -1)
      perror("semctl(delete)");
  }

  void lock() {
    struct sembuf semb;
    semb.sem_num = 0;
    semb.sem_op  = -1; // wait
    semb.sem_flg = SEM_UNDO;

    if (semop(semid_, &semb, 1) == -1)
      perror("semop(wait)");
  }

  void unlock() {
    struct sembuf semb;
    semb.sem_num = 0;
    semb.sem_op  = 1; // signal
    semb.sem_flg = SEM_UNDO;

    if (semop(semid_, &semb, 1) == -1)
      perror("semop(signal)");
  }

private:
  union ctlarg
  {
    int val;               // for SETVAL
    struct semid_ds *buf;  // for IPC_STAT, IPC_SET
    unsigned short *array; // for GETALL, SETALL
    struct seminfo *__buf; // for IPC_INFO
  };
  union ctlarg ctlarg_;
  int semid_;
};

class syncobj
{
public:
  syncobj();
  ~syncobj();
};

class signal_register
{
public:
  signal_register() {
    setsignal();
  }
};

struct memctx
{
  int tag;
  pid_t pid;
  pid_t tid;
  char pname[16];
  char tname[16];
  void* adr;
  size_t size;
  const void* ra;
};

class mem_
{
public:
  mem_() {}
  ~mem_() {
     hook_end();
   }
  std::map<void*, struct memctx*> tbl;
};

// global object
static semaphore_impl sem;
static signal_register sigreg;
//static std::map<void*, struct memctx*> memtbl;
static mem_ mem;
static int tag = 0;
static void*(*org_malloc_hook)(size_t, const void*);
static void*(*org_realloc_hook)(void*, size_t, const void*);
static void*(*org_memalign_hook)(size_t, size_t, const void*);
static void(*org_free_hook)(void*, const void*);

// weak symbol
extern "C" {
void (* volatile __malloc_initialize_hook)(void) = hook_init;
}

//
// global function
//

// console application entry
extern "C" int appentry(int argc, char** argv)
{
  if (argc != 2) {
    printf("usage: %s process-id\n", argv[0]);
    return -1;
  }
  pid_t pid = atoi(argv[1]);
  if (pid <= 1) {
    perror("error pid");
    return -1;
  }

  console_app(pid);
  return 0;
}

//
// local function
//

syncobj::syncobj()
{
  sem.lock();
}

syncobj::~syncobj()
{
  sem.unlock();
}

// malloc api hooking functions
void hook_init(void)
{
  backup_org_hook();
//  hook_start();
}

void backup_org_hook(void)
{
  org_malloc_hook = __malloc_hook;
  org_realloc_hook = __realloc_hook;
  org_memalign_hook = __memalign_hook;
  org_free_hook = __free_hook;
}

void hook_start(void)
{
  __malloc_hook = malloc_hook;
  __realloc_hook = realloc_hook;
  __memalign_hook = memalign_hook;
  __free_hook = free_hook;
}

void hook_end(void)
{
  __malloc_hook = org_malloc_hook;
  __realloc_hook = org_realloc_hook;
  __memalign_hook = org_memalign_hook;
  __free_hook = org_free_hook;
}

void* malloc_hook(size_t size, const void* ra)
{
  syncobj obj;
  void *p = NULL;
  hook_end();
  p = malloc(size);
  if (p) addctx(p, size, ra);
  hook_start();
  return p;
}

void* realloc_hook(void* ptr, size_t size, const void* ra)
{
  syncobj obj;
  void *p = NULL;
  hook_end();
  p = realloc(ptr, size);
  if (p) {
    delctx(ptr);
    addctx(p, size, ra);
  }
  hook_start();
  return p;
}

void* memalign_hook(size_t alignment, size_t size, const void* ra)
{
  syncobj obj;
  void *p = NULL;
  hook_end();
  p = memalign(alignment, size);
  if (p) addctx(p, size, ra);
  hook_start();
  return p;
}

void free_hook(void* ptr, const void* ra)
{
  syncobj obj;
  hook_end();
  if (ptr) delctx(ptr);
  free(ptr);
  hook_start();
  return;
}

// console app impl
void console_app(pid_t pid)
{
  setsignal();

  char inbuf[64];
  while(1) {
    top_menu();
    memset(inbuf, 0, sizeof(inbuf));
    if (fgets(inbuf, sizeof(inbuf)-1, stdin) == NULL) return;

    switch(inbuf[0]) {
    case 'i':
    case 'I':
      init_event(pid);
      break;
    case 'u':
    case 'U':
      update_event(pid);
      break;
    case 'c':
    case 'C':
      check_event(pid, check_menu());
      break;
    case 'q':
    case 'Q':
      return;
    }
  }
}

// display menu
void top_menu(void)
{
  printf("--- menu ---\n");
  printf("[i] init\n");
  printf("[u] update\n");
  printf("[c] check\n");
  printf("[q] exit\n");
}

int check_menu(void)
{
  char inbuf[64];

  printf("input update count->");
  memset(inbuf, 0, sizeof(inbuf));
  if (fgets(inbuf, sizeof(inbuf)-1, stdin) == NULL)
    return 0;
  return atoi(inbuf);
}

// memory context operations
void addctx(void* adr, size_t size, const void* ra)
{
  if (struct memctx* ctx = (struct memctx*)malloc(sizeof(struct memctx))) {
    memset(ctx, 0, sizeof(struct memctx));
    ctx->tag  = INVALID_TAGID;
    ctx->pid  = getpid();
    ctx->tid  = syscall(SYS_gettid);
    strncpy(ctx->pname, getpname(), sizeof(ctx->pname)-1);
    strncpy(ctx->tname, gettname(), sizeof(ctx->tname)-1);
    ctx->adr  = adr;
    ctx->size = size;
    ctx->ra   = ra;
    mem.tbl[adr] = ctx;
  }
  else
    perror("addctx error");
  return;
}

void delctx(void* adr)
{
  struct memctx* ctx = mem.tbl[adr];
  if (ctx) free(ctx);
  mem.tbl.erase(adr);
  return;
}

void alldelctx(void)
{
  // not sync
  for (std::map<void*, struct memctx*>::iterator it = mem.tbl.begin();
       it != mem.tbl.end(); it++) {
    if ((*it).second) free((*it).second);
    mem.tbl.erase((*it).first);
  }
}

// tag operations
void inittag(void)
{
  syncobj obj;
  hook_end();
  alldelctx();
  hook_start();
  tag = 0;
}

void updatetag(void)
{
  syncobj obj;
  for (std::map<void*, struct memctx*>::iterator it = mem.tbl.begin();
       it != mem.tbl.end(); it++) {
    if ((*it).second->tag == INVALID_TAGID) (*it).second->tag = tag;
  }
  tag++;
}

unsigned long counttag(int tagno)
{
  // not sync
  unsigned long count = 0;
  for (std::map<void*, struct memctx*>::iterator it = mem.tbl.begin();
       it != mem.tbl.end(); it++) {
    if ((*it).second->tag == tagno) {
      count++;
    }
  }
  return count;
}

void notifytag(pid_t consolepid, int tagno)
{
  syncobj obj;

  unsigned long tags = counttag(tagno);
  int shmid;

  if (tags == 0) return;
  if ((shmid = shmget(IPC_PRIVATE, tags*sizeof(memctx), 0666|IPC_CREAT)) == -1) {
    perror("shmget");
    return;
  }

  struct memctx* ctx;
  if ((ctx = (struct memctx*)shmat(shmid, NULL, 0)) == (void*)-1) {
    perror("shmat");
    shmctl(shmid, IPC_RMID, NULL);
    return;
  }

  unsigned long count = 0;
  for (std::map<void*, struct memctx*>::iterator it = mem.tbl.begin();
       it != mem.tbl.end(); it++) {
    if ((*it).second->tag == tagno) {
      if (count < tags)
        memcpy(&ctx[count++], (*it).second, sizeof(struct memctx));
    }
  }

  if (shmdt(ctx) == -1) {
    perror("shmdt");
    shmctl(shmid, IPC_RMID, NULL);
    return;
  }
  result_event(consolepid, shmid, count);
}

void printtag(int shmid, unsigned long tagcount)
{
  struct memctx* ctx;
  if ((ctx = (struct memctx*)shmat(shmid, NULL, SHM_RDONLY)) == (void*)-1) {
    perror("shmat");
    shmctl(shmid, IPC_RMID, NULL);
    return;
  }

  printformat(ctx, tagcount);

  if (shmdt(ctx) == -1) {
    perror("shmdt");
  }
  shmctl(shmid, IPC_RMID, NULL);
}

void printformat(struct memctx* ctx, unsigned long tagcount)
{
  static const char* head =
    "---- PID ------ TID ------ ADRRESS ------ SIZE -------- RA -----";
  // -- AAAAAAAA - AAAAAAAA - 0xAAAAAAAA - 0xAAAAAAAA - 0xAAAAAAAA --\n

  printf("%s\n", head);
  for (unsigned long i = 1; i <= tagcount; ctx++, i++) {
    printf("-- %-8s - %-8s - 0x%p - 0x%08x - 0x%p --\n",
           ctx->pname, ctx->tname, ctx->adr,
           ctx->size, ctx->ra);
    if (!(i%16)) {
      char inbuf[32];
      printf("--- quit[q] continue[Enter]->");
      memset(inbuf, 0, sizeof(inbuf));
      if (fgets(inbuf, sizeof(inbuf)-1, stdin) == NULL) break;
      if (inbuf[0] == 'q' || inbuf[0] == 'Q') break;
      printf("%s\n", head);
    }
  }
}

// signal callback functions
void setsignal(void)
{
  if (signal(SIGUSR1, sighandler) == SIG_ERR)
    perror("signal");
}

void sighandler(int signame)
{
  struct event e;
  recv_event(&e);
  eventHandler(&e);
  setsignal();
  return;
}

void eventHandler(struct event* e)
{
  switch(e->type) {
  case EVENT_INITTAG:
    inittag();
    break;
  case EVENT_UPDATETAG:
    updatetag();
    break;
  case EVENT_LEAKCHECK:
    if (e->data.tagno == 0)
      notifytag(e->data.pid, INVALID_TAGID);
    else if (0 < e->data.tagno && e->data.tagno <= tag)
      notifytag(e->data.pid, tag - e->data.tagno);
    break;
  case EVENT_RESULT:
    printtag(e->data.shmid, e->data.tagcount);
    break;
  }
}

// event send-recv operations
int send_event(pid_t pid, struct event* e)
{
  e->data.pid = getpid();

  int id;
  if ((id = msgget((key_t)MSGKEY, 0666|IPC_CREAT)) == -1) {
    perror("msgget");
    return -1;
  }

  if (msgsnd(id, (struct msgbuf*)e, sizeof(e->data), 0) == -1) {
    perror("msgsnd");
    if (msgctl(id, IPC_RMID, NULL) == -1)
      perror("msgctl");
    return -1;
  }

  if (kill(pid, SIGUSR1) == -1) {
    perror("kill");
    if (msgctl(id, IPC_RMID, NULL) == -1)
      perror("msgctl");
    return -1;
  }
  return 0;
}

int recv_event(struct event* e)
{
  int id;
  if ((id = msgget((key_t)MSGKEY, 0666)) == -1) {
    perror("msgget");
    return -1;
  }

  if (msgrcv(id, (struct msgbuf*)e, sizeof(e->data), 0, 0) == -1) {
    perror("msgrcv");
    if (msgctl(id, IPC_RMID, NULL) == -1)
      perror("msgctl");
    return -1;
  }

  return 0;
}

// event push function
int init_event(pid_t pid)
{
  struct event e;
  e.type = EVENT_INITTAG;
  return send_event(pid, &e);
}

int update_event(pid_t pid)
{
  struct event e;
  e.type = EVENT_UPDATETAG;
  return send_event(pid, &e);
}

int check_event(pid_t pid, int tagno)
{
  struct event e;
  e.type = EVENT_LEAKCHECK;
  e.data.tagno = tagno;
  return send_event(pid, &e);
}

int result_event(pid_t pid, int shmid, int tagcount)
{
  struct event e;
  e.type = EVENT_RESULT;
  e.data.shmid = shmid;
  e.data.tagcount = tagcount;
  return send_event(pid, &e);
}

// util function
const char* gettname(void)
{
  const char* p = NULL;
  if (!p) p = "????????";
  return p;
}

const char* getpname(void)
{
  static char proc_name[64];

  if (proc_name[0] == '\0') {
    const char *path = "/proc/self/stat";
    FILE *fp = fopen(path, "r");
    if (fp) {
      char temp[64];
      memset(temp, 0, sizeof(temp));
      if (fgets(temp, sizeof(temp)-1, fp)) {
        if (strchr(temp, ')'))
          *strchr(temp, ')') = 0;
        if (strchr(temp, '('))
          strncpy(proc_name, strchr(temp, '(') + 1, sizeof(proc_name)-1);
      }
      fclose(fp);
    }
  }
  return proc_name;
}

// EOF
