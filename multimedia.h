#ifndef _MYPLAY_H_

#define _MYPLAY_H_

#include "lvgl/examples/lv_examples.h"

#include <stdlib.h>

#include <string.h>

#include <dirent.h>

#include <stdio.h>

#include <pthread.h>

#include <sys/types.h>

#include <sys/stat.h>

#include <fcntl.h>

#include <unistd.h>

#include <signal.h>

#include<time.h>

#include <semaphore.h>

#define FIFO "/tmp/fifo4test"
void Pro_BrowerCreate(void);

#endif
