#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void* thread_func(void* arg) {
    int id = *(int*)arg;
    char filename[32];
    sprintf(filename, "file_%d.txt", id);
    
    fs_create(filename);
    int fd = fs_open(filename);
    
    if(fd >= 0) {
        char data[] = "Hello File System!";
        fs_write(fd, data, sizeof(data));
        
        char read_buf[64];
        fs_read(fd, read_buf, sizeof(data));
        fs_close(fd);
    }
    return NULL;
}

int main() {
    printf("--- File System Implementation ---\n");
    
    if (disk_init("vdisk.bin", 100) != 0) {
        printf("Disk init failed\n");
        return -1;
    }
    
    printf("[*] Formatting File System...\n");
    fs_format(100);
    
    printf("[*] Mounting File System...\n");
    fs_mount();
    
    printf("[*] Running Multithreaded Stress Test...\n");
    pthread_t threads[4];
    int ids[4] = {1, 2, 3, 4};
    
    for(int i = 0; i < 4; i++) {
        printf("%d ",i);
        pthread_create(&threads[i], NULL, thread_func, &ids[i]);
    }
    printf("\n");

    for(int i = 0; i < 4; i++) {
        printf("%d ",i);
        pthread_join(threads[i], NULL);
    }
    printf("\n");
    
    disk_close();
    printf("[*] File System Shutdown gracefully.\n");
    return 0;
}
