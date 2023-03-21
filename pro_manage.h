#ifndef __PROMANAGE_H
#define __PROMANAGE_H
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

void Video_music_player(void);

#endif
