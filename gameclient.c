#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <ncurses.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>

#define F 20

/*存储玩家所包含的点*/
typedef struct node
{
    int x;
    int y;
}node;

void *init_game();
void show_player(int n);

void *receive();
void *walk();

void handle_server_message(int fd);
int connect_to_server();
void death();
void win();

void create_a_node();

static

int now_fd;
int alive = 1;
int num;    //玩家编号
int ch; //输入的命令
int area_count[10];//面积
node old_nodes[10][256];
node nodes[10][256];//玩家所含面积包含的点坐标
node old_foods[F];
node foods[F];

main()
{
    int fd;
    pthread_t t[10];

    /*先连接服务器*/
    pthread_create(&t[0], NULL, receive, NULL);
    /*pthread_join(t[0], NULL);*/

    sleep(1);

    init_game();//初始化curses*/

    pthread_create(&t[1], NULL, walk, NULL);
    /*pthread_join(t[0], NULL);*/
    /*pthread_join(t[1], NULL);*/

    while(1);

    endwin();

}

/**
*   初始化游戏
**/
void *init_game()
{
    int i;
    char buf[BUFSIZ];
    char title[BUFSIZ];
    char area[BUFSIZ];

    initscr();  //开启curses模式
    cbreak();   //除特殊控制字元外一切输入的字元将被一一读取
    nonl();
    noecho();   //从键盘输入字元时不显示在终端机上
    curs_set(0);//把光标置为不可见
    intrflush(stdscr,false);
    keypad(stdscr, true);//可使用键盘上一些特殊字元, 如上下左右
    refresh();

    /*初始化各项数据*/
    area_count[10] = 0;
    create_a_node();

    for(i = 0; i < F; i++){
        old_foods[i].x = 1024;
        old_foods[i].y = 1024;
    }

    sprintf(title, "NUM%d", num);
    sprintf(area, "Current area: %d", area_count[num]);

    mvwhline(stdscr, 0, 0, '-', COLS);
    mvwvline(stdscr, 1, 0, '|', 1);
    mvwvline(stdscr, 1, COLS - 1, '|', 1);
	mvwhline(stdscr, 2, 0, '-', COLS);
	move(1, 1);
	addstr(area);
	move(1, (COLS - 6) / 2);
	attron(A_REVERSE);
    attron(A_BOLD);
	addstr(title);
	attroff(A_REVERSE);
	attroff(A_BOLD);
	move(1, COLS - 21);
	addstr("Number of players: ");
	mvwvline(stdscr, 3, 0, '|', LINES - 4);
	mvwvline(stdscr, 3, COLS - 1, '|', LINES - 4);
	mvwhline(stdscr, LINES - 1, 0, '-', COLS);

    strcpy(buf, "CUR");
    buf[6] = (COLS - 2) / 100 + '0';
    buf[7] = (COLS - 2) / 10 % 10 + '0';
    buf[8] = (COLS - 2) % 10 + '0';
    buf[9] = (LINES - 5) / 100 + '0';
    buf[10] = (LINES - 5) / 10 % 10 + '0';
    buf[11] = (LINES - 5) % 10 + '0';
    write(now_fd, buf, sizeof(buf));
}

/**
*   接收服务器信息
**/
void *receive()
{
    int fd;

    fd = connect_to_server();
    if(fd == -1){
        perror("can not connect to server\n");
        exit(1);
    }
    handle_server_message(fd);//获得建立连接后服务器发来的消息
    close(fd);
}

/**
*   玩家移动
**/
void *walk()
{
    int i, j;
    char buf[BUFSIZ];
    char ch_num = '0' + num;

    while(1){
        ch = getch();
        for(i = 0; i < area_count[num]; i++){
            old_nodes[num][i].x = nodes[num][i].x;
            old_nodes[num][i].y = nodes[num][i].y;

            move(nodes[num][i].y, nodes[num][i].x);
            addstr(" ");
        }
        if(KEY_UP == ch){//上
            for(i = 0; i < area_count[num]; i++){
                nodes[num][i].y -= 1;
            }
            for(i = 0; i < area_count[num]; i++){//判断出界
                if(nodes[num][i].y < 3){
                    for(j = 0; j <area_count[num]; j++){
                        nodes[num][j].y = old_nodes[num][j].y;
                    }
                    break;
                }
            }
        }
        if(KEY_DOWN == ch){//下
            for(i = 0; i < area_count[num]; i++){
                nodes[num][i].y += 1;
            }
            for(i = 0; i < area_count[num]; i++){//判断出界
                if(nodes[num][i].y >= LINES - 1){
                    for(j = 0; j <area_count[num]; j++){
                        nodes[num][j].y = old_nodes[num][j].y;
                    }
                    break;
                }
            }
        }
        if(KEY_RIGHT == ch){//->
            for(i = 0; i < area_count[num]; i++){
                nodes[num][i].x += 1;
            }
            for(i = 0; i < area_count[num]; i++){//判断出界
                if(nodes[num][i].x >= COLS - 1){
                    for(j = 0; j <area_count[num]; j++){
                        nodes[num][j].x -= 1;
                    }
                    break;
                }
            }
        }
        if(KEY_LEFT == ch){//<-
            for(i = 0; i < area_count[num]; i++){
                nodes[num][i].x -= 1;
            }
            for(i = 0; i < area_count[num]; i++){//判断出界
                if(nodes[num][i].x < 1){
                    for(j = 0; j <area_count[num]; j++){
                        nodes[num][j].x = old_nodes[num][j].x;
                    }
                    break;
                }
            }
        }

        strcpy(buf, "MOVE");
        buf[4] = area_count[num] / 10 + '0';
        buf[5] = area_count[num] % 10 + '0';
        buf[6] = num + '0';
        for(i = 0; i < area_count[num]; i++){
            buf[7 + 4 * i] = '0' + nodes[num][i].x / 10;
            buf[8 + 4 * i] = '0' + nodes[num][i].x % 10;
            buf[9 + 4 * i] = '0' + nodes[num][i].y / 10;
            buf[10 + 4 * i] = '0' + nodes[num][i].y % 10;
        }
        write(now_fd, buf, sizeof(buf));
        show_player(num);
    }
}

/**
*   解析服务器的消息
**/
void handle_server_message(int fd)
{
    char buf[256];
    pthread_t t, t1;
    int i;
    int other_num;
    fd_set rfds;
    struct timeval tv;
    int retval, maxfd;
    int rank;
    char a[2];
    int count;
    char area[3];

    while(1){
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd, &rfds);

        maxfd = 0;
        if(maxfd < fd){
            maxfd = fd;
        }

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        retval = select(fd + 1, &rfds, NULL, NULL, &tv);
        if(retval == -1){
            perror("select");
            exit(1);
        }

        if(FD_ISSET(fd, &rfds)){
            read(fd, buf, sizeof(buf));
            if(strncmp(buf, "START", 5) == 0){
                init_game();
                pthread_create(&t1, NULL, init_game, NULL);

                strcpy(buf, "IHaveGotTheMessageSTART");///////////
                write(fd, buf, sizeof(buf));/////////////////////////
                pthread_create(&t, NULL, walk, NULL);
                pthread_join(t, NULL);
            }
            if(strncmp(buf, "NUM", 3) == 0){//连接后接收编号
                printf("%s is waiting...\n", buf);
                num = buf[3] - '0';
            }
            if(strncmp(buf, "FOOD", 4) == 0){
                for(i = 0; i < F; i++){
                    foods[i].x = (buf[4 + 4 * i] - '0') * 10 + buf[5 + 4 * i] - '0';
                    foods[i].y = (buf[6 + 4 * i] - '0') * 10 + buf[7 + 4 * i] - '0';
                }
                for(i = 0; i < F; i++){
                    mvaddstr(old_foods[i].y, old_foods[i].x, " ");
                    mvaddstr(foods[i].y, foods[i].x, "*");
                }
                for(i = 0; i < F; i++){
                    old_foods[i].x = foods[i].x;
                    old_foods[i].y = foods[i].y;
                }
                for(i = 0; i < 10; i++){
                    show_player(i);
                }
                refresh();
            }
            if(strncmp(buf, "MOVE", 4) == 0){
                /*move(LINES-2, 0);///////////////////////////////////////////////
                /*addstr(buf);/////////////////////////////////////////////////////////*/

                other_num = buf[6] - '0';
                count = (buf[4] - '0') * 10 + buf[5] - '0';

                if(other_num == num){
                    if(count < 10){
                        area[0] = buf[5];
                        area[1] = ' ';
                        area[2] = '\0';
                    }else{
                        area[0] = buf[4];
                        area[1] = buf[5];
                        area[2] = '\0';
                    }
                    move(1, 15);
                    addstr(area);
                    refresh();
                }

                for(i = 0; i < area_count[other_num]; i++){
                    move(old_nodes[other_num][i].y, old_nodes[other_num][i].x);
                    addstr(" ");
                }
                if(count != 0){
                    for(i = 0; i < count; i++){
                        nodes[other_num][i].x = (buf[7 + 4 * i] - '0') * 10 + buf[8 + 4 * i] - '0';
                        nodes[other_num][i].y = (buf[9 + 4 * i] - '0') * 10 + buf[10 + 4 * i] - '0';
                    }
                }else{
                    for(i = 0; i < area_count[other_num]; i++){
                        nodes[other_num][i].x = nodes[other_num][i].y = 1024;
                    }
                }
                for(i = 0; i < count; i++){
                    old_nodes[other_num][i].x = nodes[other_num][i].x;
                    old_nodes[other_num][i].y = nodes[other_num][i].y;
                }
                area_count[other_num] = count;
                for(i = 0; i < 10; i++){
                    show_player(i);
                }
                refresh();
            }
            if(strncmp(buf, "DEAD", 4) == 0){//吃掉其他玩家后吸收面积
                rank = buf[4] - '0';
                death(rank);
                alive--;
            }
            if(strncmp(buf, "WIN", 3) == 0){
                win();
            }
            if(strncmp(buf, "ALIVE", 5) == 0){
                alive = buf[5] - '0';
                a[0] = buf[5];
                a[1] = '\0';
                move(1, COLS - 2);
                addstr(a);
                refresh();
            }
        }
    }
}

/**
*   显示指定玩家
**/
void show_player(int n)
{
    char cnum[2];
    int i;

    cnum[0] = '0' + n;
    cnum[1] = '\0';

    for(i = 0; i < area_count[n]; i++){
        move(nodes[n][i].y, nodes[n][i].x);
        attron(A_BOLD);
        addstr(cnum);
        attroff(A_BOLD);
        refresh();
    }
    refresh();
}

/**
*   连接服务器
**/
int connect_to_server()
{
    int sock;
    struct sockaddr_in addr;
    int n;

    now_fd = sock = socket(PF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");/////

    if(connect(sock, (const struct sockaddr *)&addr, sizeof(addr)) == -1){
        endwin();
        perror("cannot connect");
		exit(1);
    }

    return sock;
}

/**
*   创建一个点为玩家最初形态
**/
void create_a_node()
{
    if((num % 4) == 0){
        nodes[num][0].x = 1;
        nodes[num][0].y = 3;
    }else if((num % 4) == 1){
        nodes[num][0].x = COLS - 2;
        nodes[num][0].y = 3;
    }else if((num % 4) == 2){
        nodes[num][0].x = 1;
        nodes[num][0].y = LINES - 2;
    }else if((num % 4) == 3){
        nodes[num][0].x = COLS - 2;
        nodes[num][0].y = LINES - 2;
    }
    area_count[num] = 1;
}

/**
*   死亡后弹窗
**/
void death(int rank)
{
    char temp[2];

    temp[0] = rank + '0';
    temp[1] = '\0';

    WINDOW *alertWindow;
    alertWindow = newwin(8, 30, 6, 20);
    box(alertWindow, '|', '-');
	mvwaddstr(alertWindow, 2, 8, "Game Over!");
	mvwaddstr(alertWindow, 4, 8, "rank:");
	mvwaddstr(alertWindow, 4, 14, temp);
	touchwin(alertWindow);
	wrefresh(alertWindow);
	getch();
	touchwin(stdscr);
	wrefresh(stdscr);
	delwin(alertWindow);
	sleep(1);

	endwin();
}

/**
*   获胜后弹窗
**/
void win(){
	WINDOW *alertWindow;
	alertWindow = newwin(8, 30, 6, 20);
	box(alertWindow, '|', '-');
	mvwaddstr(alertWindow, 2, 8, "You are the winner!");
	mvwaddstr(alertWindow, 4, 8, "rank: 1");
	touchwin(alertWindow);
	wrefresh(alertWindow);
	getch();
	touchwin(stdscr);
	wrefresh(stdscr);
	delwin(alertWindow);
	sleep(1);

	endwin();
}
