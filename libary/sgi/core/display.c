#include <sgi/sgi.h>
#include <sgi/sgii.h>
#include <sys/srvcall.h>
#include <sys/res.h>
#include <sys/ipc.h>
#include <srv/guisrv.h>
#include <string.h>
#include <stdio.h>

bool SGI_DisplayWindowInfoCheck(SGI_Display *display)
{
    int i;
    for (i = 0; i < SGI_WINDOW_HANDLE_NR; i++) {
        if (!display->winfo_table[i].winid)    /* 找到一个空闲的句柄 */
            return true;
    }
    return false;
}

/*
把窗口id添加到窗口句柄表，并返回句柄值
*/
int SGI_DisplayWindowInfoAdd(SGI_Display *display, SGI_WindowInfo *winfo)
{
    int i;
    for (i = 0; i < SGI_WINDOW_HANDLE_NR; i++) {
        if (!display->winfo_table[i].winid)    /* 找到一个空闲的句柄 */
            break;
    }
    if (i >= SGI_WINDOW_HANDLE_NR)
        return -1;
    display->winfo_table[i] = *winfo;
    return i;   /* 返回窗口句柄值 */
}

/*
* 把窗口从窗口句柄表中移除
*/
int SGI_DisplayWindowInfoDel(SGI_Display *display, SGI_Window window)
{
    int i;
    for (i = 0; i < SGI_WINDOW_HANDLE_NR; i++) {
        if (display->winfo_table[i].winid && i == window) {
            display->winfo_table[i].winid = 0;
            display->winfo_table[i].shmid = -1;
            display->winfo_table[i].mapped_addr = NULL;
            return 0;
        }
    }
    return -1;
}

SGI_Display *SGI_OpenDisplay()
{
    SGI_Display *display = SGI_Malloc(sizeof(SGI_Display));
    if (display == NULL) {
        return NULL;
    }
    memset(display, 0, sizeof(SGI_Display));

    DEFINE_SRVARG(srvarg);
    SETSRV_ARG(&srvarg, 0, GUISRV_OPEN_DISPLAY, 0);
    SETSRV_ARG(&srvarg, 1, display, sizeof(SGI_Display));
    SETSRV_IO(&srvarg, SRVIO_USER << 1);
    SETSRV_RETVAL(&srvarg, -1);

    if (srvcall(SRV_GUI, &srvarg)) {
        goto od_free_display;
    }
    if (GETSRV_RETVAL(&srvarg, int) == -1) {
        goto od_free_display;
    }
    /* 链接失败 */
    if (!display->connected)
        goto od_free_display;
    /* success! */
    char msgname[16];
    memset(msgname, 0, 16);
    sprintf(msgname, "guisrv-display%d", display->id);
    printf("[SGI] open msg %s.\n", msgname);
    /* 创建一个消息队列，用来和客户端进程交互 */
    int msgid = res_open(msgname, RES_IPC | IPC_MSG | IPC_CREAT, 0);
    if (msgid < 0) 
        goto od_free_display;
        
    /* 消息id */
    display->msgid = msgid;

    SGI_WindowInfo winfo;
    winfo.winid = display->root_window;
    winfo.shmid = -1;
    winfo.mapped_addr = NULL;
    
    /* 把根窗口添加到窗口句柄 */
    display->root_window = SGI_DisplayWindowInfoAdd(display, &winfo);
    printf("[SGI] oepn display: root window:%d width:%d height:%d\n",
        display->root_window, display->width, display->height);

    return display;
od_free_display:
    SGI_Free(display);
    return NULL;
}

int SGI_CloseDisplay(SGI_Display *display)
{
    if (!display) {
        return -1;
    }
    /* ！！！释放显示的所有窗口，0号是根窗口，不释放 */
    int i;
    for (i = 1; i < SGI_WINDOW_HANDLE_NR; i++) {
        /* 关闭窗口 */
        SGI_DisplayWindowInfoDel(display, i);
    }

    if (display->msgid >= 0)
        res_close(display->msgid);

    /* 发送关闭显示服务请求 */
    DEFINE_SRVARG(srvarg);
    SETSRV_ARG(&srvarg, 0, GUISRV_CLOSE_DISPLAY, 0);
    SETSRV_ARG(&srvarg, 1, display->id, 0);
    SETSRV_RETVAL(&srvarg, -1);
    if (srvcall(SRV_GUI, &srvarg)) {
        goto close_display_error;
    }
    if (GETSRV_RETVAL(&srvarg, int) == -1) {
        goto close_display_error;
    }

    SGI_Free(display);
    return 0;

close_display_error:
    return -1;
}
