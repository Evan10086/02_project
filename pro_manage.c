#include <stdio.h>
#include "lvgl/lvgl.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>

//全局
static lv_obj_t* t0;
static lv_obj_t* t1;
static lv_obj_t* t2;
static lv_obj_t* t3;

pthread_mutex_t mutex_lv;                           //lvgl线程锁
static char local_music_path[]="/evan_work";        //音乐路径
static char local_pic_path[]="/evan_work";          //图片路径
static char local_words_path[]="/evan_work";        //歌词路径
static char local_video_path[]="/evan_work";        //视频路径
static char music_path[100][1024];                  //音乐路径
static char pic_path[100][1024];                    //图片路径
static char words_path[100][1024];                  //歌词路径
static char video_path[100][1024];                  //视频路径
static char words_buf[5*1024]={0};                  //放歌词的数组

static FILE *fp_mplayer=NULL;                       //播放器传回的管道，接收播放进度
static FILE *fp_words=NULL;                         //歌词文件
static int fd_mplayer = 0;                          //定义管道文件描述符,发送指令给mplayer
static int music_num=0;                             //音乐数量
static int music_index=-1;                          //当前音乐的下标值，用于寻找上下首
static int video_index=-1;                          //当前视频的下标值，用于寻找上下首
static int video_num;                             //视频的数量
static int percent_pos;                 //当前播放百分比
static float time_length;               //歌曲长度
static float time_pos;                  //当前播放进度
static bool list_flag=1;                //开关列表
static bool words_flag=1;               //歌词列表
static bool play_flag=0;                //播放音乐开关
static bool start=0;                    //启动，线程读mplayer返回信息
static lv_obj_t *play_mode;             //下拉列表对象，播放模式
static lv_obj_t *speed_obj;             //下拉列表对象，播放速度
static lv_obj_t * music_list;           //音乐列表对象
static lv_obj_t *pic_obj;               //图片对象
static lv_obj_t * volume_slider;        //音量滑条对象
static lv_obj_t * play_slider;          //播放进度条对象
static lv_obj_t * cont;                 //音乐盒的背景对象
static lv_obj_t * video_list;           //视频列表对象
static lv_obj_t *title_label;           //图片信息标签
static lv_obj_t *words_label;           //歌词标签
static lv_obj_t * volume_label;         //音量标签
static lv_obj_t *time_length_label;     //时长标签
static lv_obj_t *app1;                  //app1
static lv_obj_t *app2;                  //app1
static lv_obj_t *app3;                  //app1
static lv_obj_t *app4;                  //app1
static lv_obj_t *app5;                  //app1
static lv_obj_t *app6;                  //app1
static lv_obj_t *app7;                  //app1
static lv_obj_t *app8;                  //app1
static lv_obj_t *app9;                  //app1
static lv_obj_t *app10;                  //app1

static lv_obj_t *time_pos_label;        //当前时间标签
static lv_obj_t *words_list;            //歌词标签  
pthread_cond_t cond;                    //条件变量，用于暂停读取mplayer
pthread_cond_t cond1;
pthread_mutex_t mutex;
pthread_mutex_t mutex1;
pthread_t tid_read;         //读mplayer的线程id
pthread_t tid_write;        //写mplayer的线程id

//信号任务
void signal_10_task(int arg)
{
    if (arg == 10)
    {
        printf("收到信号 %d 线程任务(读取mplayer)暂停\n", arg);
        pthread_kill(tid_write,12); //停止写
        pthread_cond_wait(&cond, &mutex);   //停止读
        pthread_cond_signal(&cond1);//唤醒获取时间
        puts("继续工作,读mplayer");
        return;
    }
}


//信号任务
void signal_12_task(int arg)
{
    //printf("收到信号 %d\n",arg);
    printf("收到信号 %d 线程任务(get_time_pos)暂停\n", arg);
    pthread_cond_wait(&cond1, &mutex1);
    puts("继续工作,写mplayer");  
    return;
}
static char words_line[1024]={0};

//线程任务 读播放器返回内容
static char show_time_buf[100]={0};
void *read_mplayer_task(void *arg)
{
    //注册一个信号
    signal(10, signal_10_task);//收到10就暂停
    char line[1024]={0};                //返回信息
    char *p;
    while (1)
    {
        memset(line, 0, 1024);
        fgets(line,1024,fp_mplayer);  //读取进程返回的内容
        printf("返回 %s",line); 


        if(strstr(line,"ANS_TIME_POSITION"))//当前播放秒数
        {
            p=strrchr(line,'=');
            p++;    
            sscanf(p,"%f",&time_pos);
            printf("播放时间 %.2f\n",time_pos);
            char tmp[100]={0};
            int tmp_time=time_pos;
            sprintf(tmp,"%02d:%02d",tmp_time/60,tmp_time % 60);
            if(strcmp(show_time_buf,tmp))//不相同
            {
                strcpy(show_time_buf,tmp);
                //lv_label_set_text(time_pos_label,show_time_buf); //设置标签
                pthread_mutex_lock(&mutex_lv);//上锁
                lv_label_set_text(time_pos_label,show_time_buf);
                pthread_mutex_unlock(&mutex_lv);//解锁
            }
            puts(tmp);//打印时间  00:00
            //*********开启线程找歌词**********//
            // pthread_t tid;
            // pthread_create(&tid,NULL,find_words_task,tmp);
            // pthread_detach(tid);    
            //********************************//
        }
        if(strstr(line,"ANS_PERCENT_POSITION"))//播放百分比
        {
            p=strrchr(line,'=');//p++;    
            int percent=0;
            sscanf(++p,"%d",&percent);
            printf("播放百分比 %d\n",percent_pos);
             if(percent!=percent_pos)
            {
            percent_pos=percent;
            //lv_slider_set_value(play_slider,percent_pos,LV_ANIM_OFF);//设置进度条值
            pthread_mutex_lock(&mutex_lv);//上锁
            lv_slider_set_value(play_slider,percent_pos,LV_ANIM_OFF);
            pthread_mutex_unlock(&mutex_lv);
            }
            if(percent_pos>=99){
                usleep(1000);
                if(++video_index >= video_num)video_index=0;
                play_one_video(); //播放下一个视频
            }
            // if(percent_pos>=99)//播完了
            // {

            //     lv_label_set_text(time_pos_label,"0:00");
            //     //播放模式
            //     int mod = lv_dropdown_get_selected(play_mode);
            //     switch (mod)
            //         {
            //             case 0:{
            //                     pthread_kill(tid_read,10);//停止读写mplayer
            //                     break;
            //                     }
            //             case 1://循环播放
            //                     break;          
            //             case 2://列表循环
            //                     {
            //                     if(++music_index >= music_num)music_index=0;
            //                     play_one_music();
            //                     break;
            //                     }
            //             case 3://随机播放
            //                     {
            //                     srand((unsigned)time(NULL));
            //                     music_index = (music_index+rand())%music_num;
            //                     play_one_music();
            //                     break;
            //                     }              
            //             default:
            //                     break;
            //         }
            // }
        }
        if(strstr(line,"ANS_LENGTH"))//歌曲长度
        {
            p=strrchr(line,'=');p++;    
            sscanf(p,"%f",&time_length);
            printf("歌曲长度 %.2f\n",time_length);
            char time_buf[100]={0};
            int length=time_length;
            sprintf(time_buf,"%02d:%02d",length/60,length % 60);
            puts(time_buf);
            pthread_mutex_lock(&mutex_lv);//上锁
            lv_label_set_text(time_length_label,time_buf); //设置标签
            pthread_mutex_unlock(&mutex_lv);//解锁
        }
    }
}


//线程任务 发送命令给mplayer
void *write_mplayer_task(void *arg)
{
    //注册一个信号
    signal(12, signal_12_task);//收到12就暂停
    char cmd1[1024]="get_time_pos\n";
    char cmd[1024]="get_percent_pos\n";
    while(1)
    {
        write(fd_mplayer,cmd1,strlen(cmd1));//发送获取时间命令
        usleep(400*1000);
        write(fd_mplayer,cmd,strlen(cmd));//发送获取百分比
        usleep(400*1000);
        //sleep(1);      
    }
}


// static void show_list()
// {
//     //创建列表文本框
//     music_list = lv_list_create(cont);
//     //添加风格
//     // lv_obj_add_style(music_list, &font_style, 0);
//     lv_obj_set_size(music_list, 180, 300);
//     //列表位置
//     //lv_obj_center(music_list);
//     lv_obj_align(music_list, LV_ALIGN_RIGHT_MID, -20, -80);
//     //添加文本
//     lv_list_add_text(music_list, "Music list");
//     /*在列表中添加按钮*/
//     lv_obj_t *btn;
//     for(int i = 0; i < music_num; i++)
//     {
//         char tmp[100]={0};
//         char *p = music_path[i];
//         p = strrchr(p,'/');
//         strcpy(tmp,++p);//裁剪到只剩音乐名字  music.mp3
//         //参数：列表对象，图标宏，按钮名
//         btn = lv_list_add_btn(music_list, NULL,strtok(tmp,"."));
//         //列表按钮风格
//         // lv_obj_add_style(music_list, &font_style, 0);
//         //触发事件，把下标传递
//         lv_obj_add_event_cb(btn, event_handler_music_list, LV_EVENT_CLICKED, i);
//     }
// }


//按钮事件
static void btn_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //获取传递的参数
    char *msg = lv_event_get_user_data(e);
    if(code == LV_EVENT_CLICKED) //点击按钮
    {
        if(strcmp(msg,"pause") == 0)
        {
            if(!start)return;
            pthread_kill(tid_read,10);//暂停读mplayer返回内容      
            system("killall -19 mplayer");
        }
        if(strcmp(msg,"play") == 0)
        {
            if(!start)return;
            system("killall -18 mplayer");//播放器继续
            pthread_cond_signal(&cond);//唤醒读写mplayer
            usleep(1000);
            pthread_cond_signal(&cond);//防止被再次暂停
        }
        if(strcmp(msg,"forward") == 0)
        {
            if(!start)return;
            usleep(1000);
            char  cmd[1024]={"seek +10\n"};
            write(fd_mplayer,cmd,strlen(cmd));  
            strcpy(cmd,"get_percent_pos\n");
            write(fd_mplayer,cmd,strlen(cmd));//发送获取时间命令    
        }
        if(strcmp(msg,"back") == 0)
        {
            if(!start)return;
            usleep(10000);
            char  cmd[1024]={"seek -10\n"};
            write(fd_mplayer,cmd,strlen(cmd));  
            strcpy(cmd,"get_percent_pos\n");
            write(fd_mplayer,cmd,strlen(cmd));//发送获取时间命令  
        }
        if(strcmp(msg,"next_music") == 0)
        {
            usleep(1000);
            //播放模式
            int mod = lv_dropdown_get_selected(play_mode);
            switch (mod)
            {
            case 1:
            {
                play_one_music();break;
            }
            case 0:
            case 2://列表循环
            {
                if(++music_index >= music_num)music_index=0;
                play_one_music();
                break;
            }
            case 3://随机播放
            {
                srand((unsigned)time(NULL));
                music_index = (music_index+rand())%music_num;
                printf("music_index %d\n",music_index);
                play_one_music();
                break;
            }              
            default:
                break;
            }
        }
        if(strcmp(msg,"prev_music") == 0)
        {
            usleep(1000);
            //播放模式
            int mod = lv_dropdown_get_selected(play_mode);
            switch (mod)
            {
            case 1://循环播放
            { char cmd[1024]={"loop -1\n"};write(fd_mplayer,cmd,strlen(cmd));
            strcpy(cmd,"loop 1\n");write(fd_mplayer,cmd,strlen(cmd));play_one_music();break;}
            case 0://单曲
            case 2://列表循环
            {
                if(--music_index <= 0)music_index=music_num-1;
                play_one_music();
                break;
            }
            case 3://随机播放
            {
                srand((unsigned)time(NULL));
                music_index = (music_index+rand())%music_num;
                play_one_music();
                break;
            }              
            default:
                break;
            }
        }
        if(strcmp(msg,"show_list") == 0)
        {
            list_flag=!list_flag;
            if(list_flag)
                lv_obj_clear_flag(music_list,LV_OBJ_FLAG_HIDDEN);        
            else
                lv_obj_add_flag(music_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
}





//----------属于返回按钮的函数
void Back_btn(lv_event_t * e)
{
    system("killall -9 mplayer");
    lv_obj_del(lv_obj_get_parent((lv_obj_t *)(e->user_data)));  
}







//**************************************************************************************************************************************************//
//下面是视频的函数
//获取视频的路径，检测本地节目单
void get_video_path()
{
    //读目录,avi后缀保存到数组
    video_num =0;
    DIR *dirp = opendir(local_video_path);
    if(dirp==NULL)
    {
        perror(local_video_path);
        exit(0);
    }
        perror;

    struct dirent* msg;
    while(1)
    {
        msg = readdir(dirp);
        if(msg == NULL)break;
        if(msg->d_name[0]=='.')continue;
        if(strstr(msg->d_name,".avi"))
        {
            sprintf(video_path[video_num], "%s/%s", local_video_path, msg->d_name);
            video_num++;
            puts(video_path[video_num - 1]);
        }
    }
    printf("检索视频完成 共%d\n",video_num);
    printf("%d\n",video_num);
}


//线程任务 创建子进程播放视频
void *play_video_task(void *arg)
{
    printf("---- %ld线程任务------------------\n",pthread_self());
    //拼接命令
    char cmd[1024]="mplayer -quiet -slave -zoom -x 530 -y 340 -geometry 125:9 -input file=/pipe";
    sprintf(cmd,"%s %s",cmd,video_path[video_index]);
    printf("命令：%s\n", cmd);
    fp_mplayer = popen(cmd, "r");
    if (fp_mplayer == NULL)
    perror("popen fail:");
    puts("----线程任务(启动播放视频)完成 -----------------------\n");
    strcpy(cmd,"get_time_length\n");    //获取长度
    write(fd_mplayer,cmd,strlen(cmd));
    strcpy(cmd,"get_time_pos\n");    //发送获取时间命令
    write(fd_mplayer,cmd,strlen(cmd));
    pthread_exit(NULL);//线程结束


}


//播放一个视频
void play_one_video()
{  
    if(play_flag != 0)//看看有没有线程在使用播放器
    {
        system("killall -9 mplayer");
    }
    play_flag=1;
    printf("视频索引%d\n", video_index);
    //打开线程播放视频
    pthread_t tid;
    int pt = pthread_create(&tid, NULL, play_video_task, NULL);
    if(pt != 0)
    {
        perror("创建进程失败");
        return -1;
    }
    pthread_detach(tid);//分离属性，自动回收
    if(!start)
    {
        sleep(1);
        //读取mplayer的内容
        pthread_create(&tid_read, NULL, read_mplayer_task, NULL);//新建线程
        pthread_detach(tid_read);//分离属性，自动回收
        //发送获取时间命令
        pthread_create(&tid_write, NULL, write_mplayer_task, NULL);
        pthread_detach(tid_write);//分离属性，自动收回
        printf("tid_read %ld  tid_write %ld\n",tid_read,tid_write);
        start=1;
    }
   pthread_cond_signal(&cond);//唤醒线程读取mplayer返回的内容
   // *******************************************************************//
    //音量
    int volume=(int)lv_slider_get_value(volume_slider);
    char cmd[1024]={0};
    sprintf(cmd,"volume %d 1\n",volume);
    write(fd_mplayer,cmd,strlen(cmd));
}


//视频按键事件
static void btn_handler2(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    //获取传递的参数
    char *msg = lv_event_get_user_data(e);
    if(code == LV_EVENT_CLICKED) //点击按钮
    {
        if(strcmp(msg,"pause") == 0)
        {
            if(!start)return;
            pthread_kill(tid_read,10);//暂停读mplayer返回内容      
            system("killall -19 mplayer");
        }
        if(strcmp(msg,"play") == 0)
        {
            if(!start)return;
            system("killall -18 mplayer");//播放器继续
            pthread_cond_signal(&cond);//唤醒读写mplayer
            usleep(1000);
            pthread_cond_signal(&cond);//防止被再次暂停
        }
        if(strcmp(msg,"forward") == 0)
        {
            if(!start)return;
            usleep(1000);
            char  cmd[1024]={"seek +10\n"};
            write(fd_mplayer,cmd,strlen(cmd));  
            strcpy(cmd,"get_percent_pos\n");
            write(fd_mplayer,cmd,strlen(cmd));//发送获取时间命令    
        }
        if(strcmp(msg,"back") == 0)
        {
            if(!start)return;
            usleep(10000);
            char  cmd[1024]={"seek -10\n"};
            write(fd_mplayer,cmd,strlen(cmd));  
            strcpy(cmd,"get_percent_pos\n");
            write(fd_mplayer,cmd,strlen(cmd));//发送获取时间命令  
        }
        if(strcmp(msg,"next_music") == 0)
        {
            usleep(1000);
            if(++video_index >= video_num)video_index=0;
                play_one_video();
        }
        if(strcmp(msg,"prev_music") == 0)
        {
            usleep(1000);
            if(--video_index <= 0)video_index=video_num-1;
                play_one_video();
        }
        if(strcmp(msg,"music_show_list") == 0)
        {
            list_flag=!list_flag;
            if(list_flag)
                lv_obj_clear_flag(video_list,LV_OBJ_FLAG_HIDDEN);        
            else
                lv_obj_add_flag(video_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
}


//显示视频按钮
static void show_button_tv()
{
    static lv_style_t btn_style;
    lv_style_init(&btn_style);
    lv_style_set_radius(&btn_style,90);
    lv_style_set_bg_opa(&btn_style, LV_OPA_COVER);
    lv_style_set_bg_color(&btn_style, lv_palette_lighten(LV_PALETTE_BLUE, 1));//按钮色
    //Add a shadow
    lv_style_set_shadow_width(&btn_style, 55);
    lv_style_set_shadow_color(&btn_style, lv_palette_main(LV_PALETTE_BLUE));//背景色
    //按钮的标签
    lv_obj_t *label;
    //暂停按钮
    lv_obj_t *btn_pause = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_pause, btn_handler2, LV_EVENT_ALL,"pause");  
    lv_obj_align(btn_pause, LV_ALIGN_BOTTOM_MID,35,0);
    lv_obj_set_size(btn_pause,60,60);
    //图标
    lv_obj_set_style_bg_img_src(btn_pause,LV_SYMBOL_PAUSE,0);
    //播放按钮
    lv_obj_t *btn_play = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_play, btn_handler2, LV_EVENT_ALL,"play");  
    lv_obj_align(btn_play, LV_ALIGN_BOTTOM_MID,-45,0);
    lv_obj_set_size(btn_play,60,60);
    lv_obj_set_style_bg_img_src(btn_play,LV_SYMBOL_PLAY,0);
     //快进
    lv_obj_t *btn_forward = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_forward, btn_handler2, LV_EVENT_ALL,"forward");
    lv_obj_set_size(btn_forward,60,60);
    lv_obj_align(btn_forward, LV_ALIGN_BOTTOM_MID,115,0);
    lv_obj_set_style_bg_img_src(btn_forward,LV_SYMBOL_RIGHT LV_SYMBOL_RIGHT,0);
    //快退
    lv_obj_t *btn_back = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_back, btn_handler2, LV_EVENT_ALL,"back");
    lv_obj_set_size(btn_back,60,60);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID,-125,0);
    lv_obj_set_style_bg_img_src(btn_back,LV_SYMBOL_LEFT LV_SYMBOL_LEFT,0);
    //下一首
    lv_obj_t *btn_next = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_next, btn_handler2, LV_EVENT_ALL,"next_music");
    lv_obj_set_size(btn_next,60,60);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_MID,195,0);
    lv_obj_set_style_bg_img_src(btn_next,LV_SYMBOL_NEXT,0);
    //上一首
    lv_obj_t *btn_prev = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_prev, btn_handler2, LV_EVENT_ALL,"prev_music");
    lv_obj_set_size(btn_prev,60,60);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_MID,-205,0);
    lv_obj_set_style_bg_img_src(btn_prev,LV_SYMBOL_PREV,0);
    //显示,隐藏  歌单列表
    //风格设置
    static lv_style_t style;
    lv_style_init(&style);
    //设置背景颜色和半径
    lv_style_set_radius(&style, 5);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 1));
    //添加一个影子
    lv_style_set_shadow_width(&style, 55);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    //视频单列表
    lv_obj_t *btn_list = lv_btn_create(cont);
    lv_obj_add_event_cb(btn_list, btn_handler2, LV_EVENT_ALL,"music_show_list");
    lv_obj_set_size(btn_list,60,50);
    lv_obj_align(btn_list, LV_ALIGN_BOTTOM_RIGHT,-0,0);
    label = lv_label_create(btn_list);
    // lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text(label, "menu");
    lv_obj_center(label);
    lv_obj_add_flag(btn_list, LV_OBJ_FLAG_CHECKABLE);
}


//视频滑动条事件
static void slider_event_cb2(lv_event_t * e)
{
    if(!start)return;
    //获取传递的参数
    char *msg = lv_event_get_user_data(e);
    //puts(msg);
    if(strcmp(msg,"volume")==0)
    {  
        lv_obj_t * slider = lv_event_get_target(e);//获取事件对象
        char buf[8];
        int volume=(int)lv_slider_get_value(slider);//获取值
        lv_snprintf(buf, sizeof(buf), "%d",volume);
        lv_label_set_text(volume_label, buf);           //更新音量标签值
        lv_obj_align_to(volume_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
        usleep(100);            //修改音量值
        char cmd[1024]={0};
        sprintf(cmd,"volume %d 1\n",volume);
        write(fd_mplayer,cmd,strlen(cmd));  
    }  
    if(strcmp(msg,"play")==0)
    {
        puts("松开了");
        if(start)//启动就先暂停
        {
            pthread_kill(tid_read,10);//暂停读mplayer返回内容      
            system("killall -19 mplayer");
        }
        int rate = (int)lv_slider_get_value(play_slider);//获取值
        float new_time = time_length * rate * 0.01;
        int seek_time = new_time - time_pos;
        char  cmd[1024]={0};
        sprintf(cmd,"seek %d\n",seek_time);
        //puts(cmd);
        write(fd_mplayer,cmd,strlen(cmd));
        system("killall -18 mplayer");//播放器继续
        pthread_cond_signal(&cond);//唤醒读写mplayer
        usleep(1000);
        pthread_cond_signal(&cond);//防止被再次暂停
    }
}


//视频显示滑动条  -5
static void show_slider_tv(void)
{      //音量    
    volume_slider = lv_slider_create(cont);
    lv_obj_align(volume_slider,LV_ALIGN_LEFT_MID,25,0);//位置    
    lv_obj_add_event_cb(volume_slider, slider_event_cb2, LV_EVENT_VALUE_CHANGED, "volume");//事件    
    lv_obj_set_size(volume_slider,18,180);//大小
    lv_slider_set_value(volume_slider,100,LV_ANIM_OFF);//初始值  
    //标签 音量大小
    volume_label = lv_label_create(cont);
    lv_label_set_text(volume_label, "100");
    lv_obj_align_to(volume_label, volume_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_obj_t *label = lv_label_create(cont);
    // lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text(label, "volume");
    lv_obj_align_to(label, volume_label, LV_ALIGN_OUT_TOP_MID, 0, -210);


    //播放进度条
    play_slider = lv_slider_create(cont);
    lv_obj_align(play_slider,LV_ALIGN_CENTER,-5,135);//位置  
    lv_obj_set_width(play_slider,570);//宽度
    lv_obj_add_event_cb(play_slider, slider_event_cb2, LV_EVENT_RELEASED , "play");//事件    
    lv_slider_set_value(play_slider,0,LV_ANIM_OFF);//初始值
    lv_slider_set_range(play_slider, 0, 100);//范围
    //在滑条下创建标签
    time_length_label = lv_label_create(cont);
    lv_label_set_text(time_length_label, "0:00");
    lv_obj_align_to(time_length_label, play_slider, LV_ALIGN_OUT_RIGHT_BOTTOM,-5,20);
    time_pos_label = lv_label_create(cont);
    lv_label_set_text(time_pos_label, "0:00");
    lv_obj_align_to(time_pos_label, play_slider, LV_ALIGN_OUT_LEFT_BOTTOM,-5,20);
}


//列表处理函数,播放列表中的视频
static lv_event_cb_t event_handler_video_list(lv_event_t * e)
{


   //获取事件码
    lv_event_code_t code = lv_event_get_code(e);
    //获取事件对象,这里是按钮
    lv_obj_t * obj = lv_event_get_target(e);
    //如果点击按钮
    if(code == LV_EVENT_CLICKED)
    {
        printf("Clicked  %s\n", lv_list_get_btn_text(video_list, obj));
        //当前播放中的video下标更新为点击列表的下标
        video_index = lv_event_get_user_data(e);
        play_one_video();
    }
}


//显示节目单列表
static void show_list2()
{
    // //创建列表文本框
    // video_list = lv_list_create(cont);
    // lv_obj_set_size(video_list, 120, 300);
    // //列表位置
    // lv_obj_align(video_list, LV_ALIGN_RIGHT_MID, 5, -60);
    // //添加文本
    // lv_list_add_text(video_list, "list");


    //创建列表文本框
    video_list = lv_list_create(cont);
    lv_obj_set_size(video_list, 120, 300);
    //列表位置
    lv_obj_align(video_list, LV_ALIGN_RIGHT_MID, 5, -60);
    //添加文本
    lv_list_add_text(video_list, "video_list");
    

    //在列表中添加按钮
    lv_obj_t *btn;
    for(int i = 0; i < video_num; i++)
    {
        char tmp[100]={0};
        char *p = video_path[i];
        p = strrchr(p,'/');
        strcpy(tmp,++p);//裁剪


        // 参数：列表对象，图标宏，按钮名
        btn = lv_list_add_btn(video_list, NULL,strtok(tmp,"."));
        //列表按钮风格
        // lv_obj_add_style(video_list, &font_style, 0);
        // 触发事件，把下标传递
        lv_obj_add_event_cb(btn, event_handler_video_list, LV_EVENT_CLICKED, i);
        
    }
}
static void k(lv_event_t *e)
{
    system("killall -9 mplayer");
}


//视频显示界面
static void display_interface2()
{
    //创建背景cont，所有显示基于背景
    cont = lv_obj_create(lv_scr_act());//声明一个背景
    lv_obj_set_size(cont, 800, 480);//设置背景的范围
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    //基于cont背景创新返回按钮的对象
    lv_obj_t *btn_back = lv_btn_create(cont);
    lv_obj_t *lab6 = lv_label_create(btn_back);
    lv_label_set_text(lab6, "X");
    lv_obj_center(lab6);
    lv_obj_set_style_radius(btn_back,5,0);//设置对象边角为圆角 角度自己调整中间值
    lv_obj_set_size(btn_back, 40, 40);
    lv_obj_set_pos(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, Back_btn, LV_EVENT_RELEASED, btn_back); //传入按钮父对象的父对象
    //整一个杀视频的按钮
    lv_obj_t* btn01 = lv_btn_create(cont);
    lv_obj_align(btn01, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t* label01 = lv_label_create(btn01);
    lv_label_set_text(label01, "kill");
    lv_obj_center(label01);
    lv_obj_add_event_cb(btn01, k, LV_EVENT_CLICKED, NULL);//按键事件
    //初始化视频内容
    // get_video_path();====
    // //显示视频列表
    // //显示按钮
     show_button_tv();
    // //显示歌单列表
     show_list2();
    // //显示滑动条
     show_slider_tv();
}


//---------------------------------------------------下面是开始界面的函数

//视频播放器的main
static void video_playback(lv_event_t* e) {
    if(access("/pipe", F_OK))
    {
        mkfifo("/pipe", 0644);
    }
    fd_mplayer = open("/pipe", O_RDWR);
    if(fd_mplayer < 0)
    {
        perror("打开管道文件失败");
        exit(0);
    }

    display_interface2();
}

//音乐播放器的main
static void music_player(lv_event_t* e)
{
    if(access("/pipe" ,F_OK))
    {
        mkfifo("/pipe", 0644);
    }
    fd_mplayer = open("/pipe", O_RDWR);
    if(fd_mplayer < 0)
    {
        perror("打开管道文件失败");
        exit(0);
    }
    //检索本地歌单
    get_music_path();
    //初始化字体
    // init_font_style();
    //显示见面函数
    display_interface();
    return 0;
}

//*************************//
//  下面开始画主要屏幕跟标签 //
//   test ver1.0.1         //
//*************************//   
 

// 处理滚动事件，完成页面循环切换(无限切换)--回调函数
static void scroll_begin_event(lv_event_t * e)
{
    lv_obj_t * cont = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    lv_obj_t * ttt = lv_obj_get_parent(cont);

    if(lv_event_get_code(e) == LV_EVENT_SCROLL_END) {
        lv_tabview_t * tabview = (lv_tabview_t *)ttt;

        lv_coord_t s = lv_obj_get_scroll_x(cont);

        lv_point_t p;
        lv_obj_get_scroll_end(cont, &p);

        lv_coord_t w = lv_obj_get_content_width(cont);
        lv_coord_t t;

        if(lv_obj_get_style_base_dir(ttt, LV_PART_MAIN) == LV_BASE_DIR_RTL)  t = -(p.x - w / 2) / w;
        else t = (p.x + w / 2) / w;

        if(s < 0) t = tabview->tab_cnt - 1;
        else if((t == (tabview->tab_cnt - 1)) && (s > p.x)) t = 0;

        bool new_tab = false;
        if(t != lv_tabview_get_tab_act(ttt)) new_tab = true;
        lv_tabview_set_act(ttt, t, LV_ANIM_ON);
    }
}

//开始输出屏幕
void Pro_BrowerCreate()
{
    //主画面
    t0 = lv_obj_create(lv_scr_act());               // 创建一个对象容器 cont
    lv_obj_set_size(t0, 800, 460);                            // 设置对象容器大小
    lv_obj_align(t0, LV_ALIGN_TOP_MID, 0, 5);                 // 设置对象容器基于屏幕中间对齐
    lv_obj_set_style_pad_all(t0, 55, LV_PART_MAIN);           // 设置对象容器内部 item 与容器边的上下左右间距
    lv_obj_set_style_pad_row(t0, 70, LV_PART_MAIN);           // 设置对象容器内部 item 之间的行间距
    lv_obj_set_style_pad_column(t0, 30, LV_PART_MAIN);        // 设置对象容器内部 item 之间的列间距
    lv_obj_clear_flag(t0, LV_OBJ_FLAG_SCROLL_ELASTIC);            // 取消滚动条
    lv_obj_set_flex_flow(t0, LV_FLEX_FLOW_ROW_WRAP);          // 设置对象容器使用基于行的流失弹性布局flex，设置超出部分换行模式
    /**
     * 设置容器弹性模式
     * 1. 容器指针
     * 2. LV_FLEX_ALIGN_SPACE_EVENLY 均匀分部子元素之间的间距
     * 3. LV_FLEX_ALIGN_END          容器中所有的子元素底部对齐
     * 4. LV_FLEX_ALIGN_CENTER       容器中内容的子元素向容器顶部对齐
    */
    lv_obj_set_flex_align(t0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_SPACE_EVENLY,0);

    
    get_video_path();//开始检索东西

     //视频播放器的按钮
    lv_obj_t *btn01 = lv_btn_create(t0);
    lv_obj_set_size(btn01,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn01,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn01,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn01,lv_palette_main(LV_PALETTE_PINK),0);
    
    lv_obj_t *btn01_img = lv_img_create(btn01);
    lv_img_set_src(btn01_img,"S:/evan_work/001.png");
    lv_obj_set_align(btn01_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn01_img,140);

    app1 = lv_label_create(btn01);
    lv_label_set_text(app1, "video");
    lv_obj_align_to(app1, volume_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    
    lv_obj_add_event_cb(btn01, video_playback, LV_EVENT_RELEASED, NULL);//按键事件

    printf("%d\n",video_num);





    lv_obj_t *btn02 = lv_btn_create(t0);
    lv_obj_set_size(btn02,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn02,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn02,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn02,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn02_img = lv_img_create(btn02);
    lv_img_set_src(btn02_img,"S:/evan_work/002.png");
    lv_obj_set_align(btn02_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn02_img,140);
    app2 = lv_label_create(btn02);
    lv_label_set_text(app2, "video");
    lv_obj_align_to(app2, btn02, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);


    lv_obj_t *btn03 = lv_btn_create(t0);
    lv_obj_set_size(btn03,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn03,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn03,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn03,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn03_img = lv_img_create(btn03);
    lv_img_set_src(btn03_img,"S:/evan_work/003.png");
    lv_obj_set_align(btn03_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn03_img,140);
    app3 = lv_label_create(btn03);
    lv_label_set_text(app3, "video");
    lv_obj_align_to(app3, btn03, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn04 = lv_btn_create(t0);
    lv_obj_set_size(btn04,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn04,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn04,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn04,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn04_img = lv_img_create(btn04);
    lv_img_set_src(btn04_img,"S:/evan_work/004.png");
    lv_obj_set_align(btn04_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn04_img,140);
    app4 = lv_label_create(btn04);
    lv_label_set_text(app4, "video");
    lv_obj_align_to(app4, btn04, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn05 = lv_btn_create(t0);
    lv_obj_set_size(btn05,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn05,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn05,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn05,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn05_img = lv_img_create(btn05);
    lv_img_set_src(btn05_img,"S:/evan_work/005.png");
    lv_obj_set_align(btn05_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn05_img,140);
    app5 = lv_label_create(btn05);
    lv_label_set_text(app5, "video");
    lv_obj_align_to(app5, btn05, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn06 = lv_btn_create(t0);
    lv_obj_set_size(btn06,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn06,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn06,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn06,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn06_img = lv_img_create(btn06);
    lv_img_set_src(btn06_img,"S:/evan_work/006.png");
    lv_obj_set_align(btn06_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn06_img,140);
    app6 = lv_label_create(btn06);
    lv_label_set_text(app6, "video");
    lv_obj_align_to(app6, btn06, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn07 = lv_btn_create(t0);
    lv_obj_set_size(btn07,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn07,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn07,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn07,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn07_img = lv_img_create(btn07);
    lv_img_set_src(btn07_img,"S:/evan_work/007.png");
    lv_obj_set_align(btn07_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn07_img,140);
    app7 = lv_label_create(btn07);
    lv_label_set_text(app7, "video");
    lv_obj_align_to(app7, btn07, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn08 = lv_btn_create(t0);
    lv_obj_set_size(btn08,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn08,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn08,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn08,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn08_img = lv_img_create(btn08);
    lv_img_set_src(btn08_img,"S:/evan_work/008.png");
    lv_obj_set_align(btn08_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn08_img,140);
    app8 = lv_label_create(btn08);
    lv_label_set_text(app8, "video");
    lv_obj_align_to(app8, btn08, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn09 = lv_btn_create(t0);
    lv_obj_set_size(btn09,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn09,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn09,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn09,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn09_img = lv_img_create(btn09);
    lv_img_set_src(btn09_img,"S:/evan_work/009.png");
    lv_obj_set_align(btn09_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn09_img,140);
    app9 = lv_label_create(btn09);
    lv_label_set_text(app9, "video");
    lv_obj_align_to(app9, btn09, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn010 = lv_btn_create(t0);
    lv_obj_set_size(btn010,140,140);//设置按钮对象大小
    lv_obj_clear_flag(btn010,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_opa(btn010,LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(btn010,lv_palette_main(LV_PALETTE_PINK),0);
    lv_obj_t *btn010_img = lv_img_create(btn010);
    lv_img_set_src(btn010_img,"S:/evan_work/0010.png");
    lv_obj_set_align(btn010_img,LV_ALIGN_CENTER);
    lv_img_set_zoom(btn010_img,140);
    app10 = lv_label_create(btn010);
    lv_label_set_text(app10, "video");
    lv_obj_align_to(app10, btn010, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);




    return 0;
}
