#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define F 20
#define TIME 10000
#define P 10

/*存储玩家所包含的点*/
typedef struct node
{
    int x;
    int y;
}node;

void *handle_request();
void *send_message();

int setTicker(int n_msecs);
void set_foods();
void add_single_area(node foods[], int j, int num);
void add_some_area(int num1, int num2);

int count_fd = 0;   //客户端数量
int now_fd[10];     //客户端连接号
int alive = 0;      //存活客户端数量

fd_set fdsr;

int area_count[10] = {0};
node nodes[10][256];
node foods[F];

pthread_mutex_t mutex_lock=PTHREAD_MUTEX_INITIALIZER;

int cols = 85;
int lines = 21;

main()
{
    int socket;
    int fd;
    char buf[BUFSIZ];
    pthread_t t[10];
    int i;
    int maxsock;
    struct timeval tv;
    socklen_t sin_size;
    struct sockaddr_in client_addr;
    int ret;

    socket = make_server_socket();

    pthread_create(&t[0], NULL, send_message, NULL);

    sin_size = sizeof(client_addr);
    maxsock = socket;
    while(1){
        FD_ZERO(&fdsr);
        FD_SET(socket, &fdsr);

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        for(i = 0; i < P; i++){
            if(now_fd[i] != 0 && now_fd[i] != -1){
                FD_SET(now_fd[i], &fdsr);
            }
        }

        ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);
        if(ret == -1){
            if(errno == EINTR){
                continue;
            }
            perror("select");
            exit(1);
        }

        if(FD_ISSET(socket, &fdsr)){
            fd = accept(socket, (struct sockaddr *)&client_addr, &sin_size);
            if(fd == -1){
                perror("cannot accept");
                exit(1);
            }
            if(fd > maxsock){
                maxsock = fd;
            }
            strcpy(buf, "NUM");
            buf[3] = '0' + count_fd;
            write(fd, buf, 4);//给客户端发送他的编号
            printf("%s has joined.\n", buf);
            now_fd[count_fd++] = fd;
            alive++;

            sleep(1);
            strcpy(buf, "FOOD");
            for(i = 0; i < F; i++){
                buf[4 + 4 * i] = '0' + foods[i].x / 10;
                buf[5 + 4 * i] = '0' + foods[i].x % 10;
                buf[6 + 4 * i] = '0' + foods[i].y / 10;
                buf[7 + 4 * i] = '0' + foods[i].y % 10;
            }
            write(fd, buf, sizeof(buf));

            usleep(1000);
            memset(buf, '\0', sizeof(buf));
            strcpy(buf, "ALIVE");
            buf[5] = alive + '0';
            for(i = 0; i < count_fd; i++){
                write(now_fd[i], buf, 6);
            }
            pthread_create(&t[count_fd], NULL, handle_request, NULL);
        }
    }
}

/**
*   处理每个客户端发来的信息
*   格式：MOVE00(x)00(y)0(client)
**/
void *handle_request()
{
    int fd = now_fd[count_fd - 1];
    char buf[BUFSIZ];
    int x, y;
    int i, j, k, m;
    int now_num = count_fd - 1;
    int count;
    int ret;

    while(1){
        ret = read(fd, buf, sizeof(buf));
        if(ret <= 0 && errno != EINTR){
            printf("player %d has closed.\n", now_num);
            close(now_fd[now_num]);
            FD_CLR(now_fd[now_num], &fdsr);
            now_fd[now_num] = -1;
            if(nodes[now_num][0].x != 1024 && nodes[now_num][0].y != 1024){
                alive--;
                if(alive == 1){
                    strcpy(buf, "WIN");
                    for(i = 0; i < count_fd; i++){
                        write(now_fd[i], buf, 3);
                    }
                }
                strcpy(buf, "MOVE");
                buf[4] = buf[5] = 0 + '0';
                buf[6] = now_num + '0';
                for(j = 0; j < count_fd; j++){
                    write(now_fd[j], buf, sizeof(buf));
                }
            }
            strcpy(buf, "ALIVE");
            buf[5] = alive + '0';
            for(i = 0; i < count_fd; i++){
                write(now_fd[i], buf, 6);
            }
            break;
        }
        /*printf("Total amount: %d\n", count_fd);//显示连接客户端总数*/
        if(fd == -1){
            break;
        }
        if(strncmp(buf, "CUR", 3) == 0){
            cols = (buf[6] - '0') * 100 + (buf[7] - '0') * 10 + buf[8] - '0';
            lines = (buf[9] - '0') * 100 + (buf[10] - '0') * 10 + buf[11] - '0';
            printf("cols: %d, lines: %d\n", cols, lines);///////////////////////////////////////////
        }
        if(strncmp(buf, "MOVE", 4) == 0){
            count = area_count[now_num] = (buf[4] - '0') * 10 + buf[5] - '0';

            printf("move:");///////////////////////////////////////////////////
            for(i = 0; i <area_count[now_num]; i++){
                nodes[now_num][i].x = (buf[7 + 4 * i] - '0') * 10 + buf[8 + 4 * i] - '0';
                nodes[now_num][i].y = (buf[9 + 4 * i] - '0') * 10 + buf[10 + 4 * i] - '0';
                printf("(%d, %d)", nodes[now_num][i].x, nodes[now_num][i].y);///////////////////////
            }
            printf("\n");////////////////////////////////////////////////////////////////////////////////

            for(i = 0; i < count_fd; i++){//给其他客户端发送位置
                if(i == (buf[6] - '0')){
                    continue;
                }
                if(now_fd[i] != -1){
                    write(now_fd[i], buf, sizeof(buf));
                }
            }
            /*吃食物*/
            for(i = 0; i < area_count[now_num]; i++){
                for(j = 0; j < F; j++){
                    if(((nodes[now_num][i].x == foods[j].x - 1 && nodes[now_num][i].y == foods[j].y) ||
                       (nodes[now_num][i].x == foods[j].x + 1 && nodes[now_num][i].y == foods[j].y) ||
                       (nodes[now_num][i].y == foods[j].y - 1 && nodes[now_num][i].x == foods[j].x) ||
                       (nodes[now_num][i].y == foods[j].y + 1 && nodes[now_num][i].x == foods[j].x))
                       && foods[j].x != 1024){
                        pthread_mutex_lock(&mutex_lock);
                        add_single_area(foods, j, now_num);
                        pthread_mutex_unlock(&mutex_lock);
                        buf[4] = area_count[now_num] / 10 + '0';
                        buf[5] = area_count[now_num] % 10 + '0';
                        for(i = 0; i < (area_count[now_num] - count); i++){
                            buf[7 + count * 4 + i * 4] = nodes[now_num][count + i].x / 10 + '0';
                            buf[8 + count * 4 + i * 4] = nodes[now_num][count + i].x % 10 + '0';
                            buf[9 + count * 4 + i * 4] = nodes[now_num][count + i].y / 10 + '0';
                            buf[10 + count * 4 + i * 4] = nodes[now_num][count + i].y % 10 + '0';
                        }
                        for(m = 0; m < count_fd; m++){
                            write(now_fd[m], buf, sizeof(buf));
                            printf("success to %d: %s\n", now_fd[i], buf);////////////////////////////////
                        }
                    }
                }
            }
            /*与玩家相遇*/
            for(k = 0; k < count_fd; k++){//遍历其他玩家
                for(i = 0; i < area_count[now_num]; i++){//当前玩家所含的点
                    for(j = 0; j < area_count[k]; j++){//其他玩家所含的点
                        if(now_fd[i] != -1 && k != now_num &&
                           ((nodes[now_num][i].x == nodes[k][j].x - 1 && nodes[now_num][i].y == nodes[k][j].y) ||
                            (nodes[now_num][i].x == nodes[k][j].x + 1 && nodes[now_num][i].y == nodes[k][j].y) ||
                            (nodes[now_num][i].x == nodes[k][j].x && nodes[now_num][i].y == nodes[k][j].y - 1) ||
                            (nodes[now_num][i].x == nodes[k][j].x && nodes[now_num][i].y == nodes[k][j].y + 1))){
                            printf("eating others...\n");
                            add_some_area(now_num, k);
                        }
                    }
                }
            }
        }
        if(strncmp(buf, "IHave", 5) == 0){///////////////////////
            printf("%s\n", buf);
        }
    }
}

void *send_message()
{
    signal(SIGALRM, set_foods);
    setTicker(TIME);
}

/**
*   服务器选择放置食物位置
**/
void set_foods()
{
    int i, j, k, l;
    int nx[F];
    int ny[F];
    char buf[BUFSIZ];

    srand((int)time(NULL));
    for(i = 0; i < F; i++){
        nx[i] = rand() % cols + 1;
    }
    for(i = 0; i < F; i++){
        ny[i] = rand() % (lines) + 3;
    }

    for(i = 0; i < F; i++){
        foods[i].x = nx[i];
        foods[i].y = ny[i];
    }

    /*判断点是否在玩家面积下，如果在，移除*/
    for(i = 0; i < count_fd; i++){
        if(now_fd[i] != 1){
            for(j = 0; j < area_count[i]; j++){
                for(k = 0; k < F; k++){
                    if(nodes[i][j].x == foods[k].x && nodes[i][j].y == foods[k].y){
                        foods[k].x = 1024;
                        foods[k].y = 1024;
                    }
                }
            }
        }
    }
    /*判断是否在玩家旁边，如果在，被吞*/
    for(k = 0; k < count_fd; k++){
        if(now_fd[k] != 1){
            for(i = 0; i < 10; i++){
                for(j = 0; j < F; j++){
                    if(((nodes[k][i].x == foods[j].x - 1 && nodes[k][i].y == foods[j].y) ||
                       (nodes[k][i].x == foods[j].x + 1 && nodes[k][i].y == foods[j].y) ||
                       (nodes[k][i].y == foods[j].y - 1 && nodes[k][i].x == foods[j].x) ||
                       (nodes[k][i].y == foods[j].y + 1 && nodes[k][i].x == foods[j].x))
                       && foods[j].x != 1024){
                        printf("adding...\n");
                        pthread_mutex_lock(&mutex_lock);
                        add_single_area(foods, j, k);
                        pthread_mutex_unlock(&mutex_lock);
                        strcpy(buf, "MOVE");
                        buf[4] = area_count[k] / 10 + '0';
                        buf[5] = area_count[k] % 10 + '0';
                        buf[6] = k + '0';
                        for(l = 0; l < area_count[k]; l++){
                            buf[7 + l * 4] = nodes[k][l].x / 10 + '0';
                            buf[8 + l * 4] = nodes[k][l].x % 10 + '0';
                            buf[9 + l * 4] = nodes[k][l].y / 10 + '0';
                            buf[10 + l * 4] = nodes[k][l].y % 10 + '0';
                        }
                        for(l = 0; l < count_fd; l++){
                            write(now_fd[l], buf, sizeof(buf));
                        }
                    }
                }
            }
        }
    }

    printf("food:");//////////////////////////////////////////
    for(i = 0; i < F; i++){
        printf("(%d, %d) ", nx[i], ny[i]);
    }
    printf("\n");/////////////////////////////////////////////

    strcpy(buf, "FOOD");
    for(i = 0; i < F; i++){
        buf[4 + 4 * i] = '0' + nx[i] / 10;
        buf[5 + 4 * i] = '0' + nx[i] % 10;
        buf[6 + 4 * i] = '0' + ny[i] / 10;
        buf[7 + 4 * i] = '0' + ny[i] % 10;
    }

    for(i = 0; i < count_fd; i++){
        write(now_fd[i], buf, sizeof(buf));
    }
}

/**
*   设置定时器
*   参数越大，时间间隔越大，即变化越慢
**/
int setTicker(int n_msecs)
{
    struct itimerval new_timeset;
    long    n_sec, n_usecs;

    n_sec = n_msecs / 1000 ;
    n_usecs = ( n_msecs % 1000 ) * 1000L ;
    new_timeset.it_interval.tv_sec  = n_sec;
    new_timeset.it_interval.tv_usec = n_usecs;
    n_msecs = 1;
    n_sec = n_msecs / 1000 ;
    n_usecs = ( n_msecs % 1000 ) * 1000L ;
    new_timeset.it_value.tv_sec = n_sec  ;
    new_timeset.it_value.tv_usec = n_usecs ;
    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}

/**
*   吞噬后面积增加
*   即将被吞掉的食物信息加入数组
**/
void add_single_area(node foods[], int j, int num)
{
    nodes[num][area_count[num]].x = foods[j].x;
    nodes[num][area_count[num]].y = foods[j].y;

    printf("food num:%d x:%d, y:%d\n", num, nodes[num][area_count[num]].x, nodes[num][area_count[num]].y);

    area_count[num]++;

    foods[j].x = 1024;
    foods[j].y = 1024;
}

/**
*   吞噬其他玩家
**/
void add_some_area(int num1, int num2)
{
    int i, j;
    char buf[BUFSIZ];
    int area1 = area_count[num1];
    int area2 = area_count[num2];

    printf("add_some_area: area1=%d, area2=%d\n", area1, area2);

    if(area1 > area2){
        for(i = 0; i < area2; i++){
            nodes[num1][area1 + i].x = nodes[num2][i].x;
            nodes[num1][area1 + i].y = nodes[num2][i].y;
            area_count[num1]++;
        }
        for(i = 0; i < area2; i++){
            nodes[num2][i].x = nodes[num2][i].y = 1024;
        }
        area_count[num2] = 0;

        strcpy(buf, "MOVE");
        buf[4] = area_count[num1] / 10 + '0';
        buf[5] = area_count[num1] % 10 + '0';
        buf[6] = num1 + '0';
        for(j = 0; j < area_count[num1]; j++){
            buf[7 + j *4] = nodes[num1][j].x / 10 + '0';
            buf[8 + j *4] = nodes[num1][j].x % 10 + '0';
            buf[9 + j *4] = nodes[num1][j].y / 10 + '0';
            buf[10 + j *4] = nodes[num1][j].y % 10 + '0';
        }
        for(j = 0; j < count_fd; j++){
            write(now_fd[j], buf, sizeof(buf));
        }

        usleep(1);
        strcpy(buf, "MOVE");
        buf[4] = buf[5] = 0 + '0';
        buf[6] = num2 + '0';
        for(j = 0; j < count_fd; j++){
            write(now_fd[j], buf, sizeof(buf));
        }

        usleep(1);
        strcpy(buf, "DEAD");
        buf[4] = alive + '0';
        write(now_fd[num2], buf, 5);
        alive--;

        if(alive == 1){
            strcpy(buf, "WIN");
            write(now_fd[num1], buf, 3);
        }
        usleep(1);
        memset(buf, '\0', sizeof(buf));
        strcpy(buf, "ALIVE");
        buf[5] = alive + '0';
        for(i = 0; i < count_fd; i++){
            write(now_fd[i], buf, 6);
        }
        close(now_fd[num2]);
        FD_CLR(now_fd[num2], &fdsr);
        now_fd[num2] = -1;
    }else if(area1 < area2){
        for(i = 0; i < area1; i++){
            nodes[num2][area2 + i].x = nodes[num1][i].x;
            nodes[num2][area2 + i].y = nodes[num1][i].y;
            area_count[num2]++;
        }
        for(i = 0; i < area1; i++){
            nodes[num1][i].x = nodes[num1][i].y = 1024;
        }
        area_count[num1] = 0;

        strcpy(buf, "MOVE");
        buf[4] = area_count[num2] / 10 + '0';
        buf[5] = area_count[num2] % 10 + '0';
        buf[6] = num2 + '0';
        for(j = 0; j < area_count[num2]; j++){
            buf[7 + j *4] = nodes[num2][j].x / 10 + '0';
            buf[8 + j *4] = nodes[num2][j].x % 10 + '0';
            buf[9 + j *4] = nodes[num2][j].y / 10 + '0';
            buf[10 + j *4] = nodes[num2][j].y % 10 + '0';
        }
        for(j = 0; j < count_fd; j++){
            write(now_fd[j], buf, sizeof(buf));
        }

        usleep(1);
        strcpy(buf, "MOVE");
        buf[4] = buf[5] = '0';
        buf[6] = num1+ '0';
        for(j = 0; j < count_fd; j++){
            write(now_fd[j], buf, sizeof(buf));
        }

        usleep(1);
        strcpy(buf, "DEAD");
        buf[4] = alive + '0';
        write(now_fd[num1], buf, 5);
        alive--;

        if(alive == 1){
            strcpy(buf, "WIN");
            write(now_fd[num2], buf, 3);
        }
        usleep(1);
        memset(buf, '\0', sizeof(buf));
        strcpy(buf, "ALIVE");
        buf[5] = alive + '0';
        for(i = 0; i < count_fd; i++){
            write(now_fd[i], buf, 6);
        }
        close(now_fd[num1]);
        FD_CLR(now_fd[num1], &fdsr);
        now_fd[num1] = -1;
    }
}

/**
*   建立连接
**/
int make_server_socket()
{
    int sock;
    struct sockaddr_in addr;
    int fd;
    int n;
    int opt = 1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    addr.sin_family = PF_INET;
    addr.sin_port=htons(8080);
    addr.sin_addr.s_addr=INADDR_ANY;

    if(bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1){
        perror("cannot bind");
        exit(1);
    }

    listen(sock, 4);

    printf("server is ready\n");

    return sock;
}
