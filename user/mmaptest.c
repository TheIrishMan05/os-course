#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void test_anon() {
  printf("Testing Anonymous Mapping...\n");
  char *p = (char *)mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
  if((uint64)p == -1) {
    printf("mmap anon failed\n");
    exit(1);
  }
  
  strcpy(p, "hello world");
  printf("mmap anon success: %s\n", p);
  
  if(munmap(p, 4096) < 0){
    printf("munmap anon failed\n");
    exit(1);
  }
  printf("munmap anon success\n");
}

void test_private_file() {
  printf("Testing MAP_PRIVATE File Mapping...\n");
  
  int fd = open("mmaptest.txt", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("open failed\n");
    exit(1);
  }
  
  char *data = "This is a test file for mmap.";
  write(fd, data, strlen(data));
  close(fd);
  
  fd = open("mmaptest.txt", O_RDONLY);
  if(fd < 0){
    printf("open read failed\n");
    exit(1);
  }
  
  char *p = (char *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if((uint64)p == -1) {
    printf("mmap file failed\n");
    exit(1);
  }
  
  printf("mmap file success. Content: %s\n", p);
  
  if(strcmp(p, data) != 0){
    printf("Content mismatch!\n");
    exit(1);
  }
  
  strcpy(p, "Modified Data");
  printf("Modified memory: %s\n", p);
  
  close(fd);
  
  fd = open("mmaptest.txt", O_RDONLY);
  char buf[64];
  read(fd, buf, sizeof(buf));
  printf("File content after modify: %s\n", buf);
  
  if(strcmp(buf, data) != 0){
    printf("File was modified! MAP_PRIVATE violation.\n");
    exit(1);
  }
  close(fd);

  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0){
    if(strcmp(p, "Modified Data") != 0){
      printf("Child: Content mismatch!\n");
      exit(1);
    }
    strcpy(p, "Child Data");
    if(strcmp(p, "Child Data") != 0){
      printf("Child: unable to write!\n");
      exit(1);
    }
    exit(0);
  } else {
    wait(0);
    if(strcmp(p, "Modified Data") != 0){
      printf("Parent: Content changed by child! COW violation.\n");
      exit(1);
    }
    printf("Parent: COW private mapping works\n");
  }

  if(munmap(p, 4096) < 0){
    printf("munmap file failed\n");
    exit(1);
  }
  printf("munmap file success\n");
  
  printf("test of private mapping: success.\n");
}

void test_steal() {
  printf("Testing Page Stealing...\n");
  
  int pipefd[2];
  if(pipe(pipefd) < 0){
    printf("pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0){
    close(pipefd[0]);
    
    char *secret = (char *)mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    strcpy(secret, "Secret Data");
    
    write(pipefd[1], &secret, sizeof(secret));
    
    printf("Child: allocated %s, waiting to be robbed...\n", secret);
    sleep(20);
    
    printf("Child: Attempting to access secret... (expecting trap)\n");
    printf("Child: \n%s\n", secret); // If we have crash here, test is succeed.
    
    printf("Child: SURVIVED?! (Test Failed)\n");
    exit(0);
  } else {

    close(pipefd[1]);
    
    char *victim_addr;
    read(pipefd[0], &victim_addr, sizeof(victim_addr));
    
    printf("Parent: received victim address %p\n", victim_addr);
    
    sleep(5);
    
    char *dst = sbrk(0);
    dst = (char*)((uint64)dst + 4096*2);
    dst = (char*)((uint64)dst & ~(4096-1));
    if((uint64)dst % 4096 != 0) dst += (4096 - ((uint64)dst % 4096));

    printf("Parent: stealing from PID %d addr %p to %p\n", pid, victim_addr, dst);
    void *res = mmap(dst, 4096, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, pid, (int)((uint64)victim_addr)); 
    
    if(res == (void*)-1){
       printf("Parent: steal failed\n");
       kill(pid);
       wait(0);
       exit(1);
    }
    
    printf("Parent: steal success!\n");
    printf("Parent: stolen data says: %s\n", (char*)dst);
    
    if(strcmp((char*)dst, "Secret Data") == 0){
        printf("Parent: Data verifies!\n");
    } else {
        printf("Parent: Data MISMATCH!\n");
    }
    
    sleep(30);
    kill(pid);
    wait(0);
  }
}

int main() {
  test_anon();
  test_steal();
  test_private_file();
  exit(0);
}