/* gui/gui.c – a compact GTK‑3 front‑end for the scheduler */

#include <gtk/gtk.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"           /* the public API from step 3 */

#define COL_PID     0
#define COL_STATE   1
#define COL_PC      2
#define COL_PRIO    3
#define COL_LO      4
#define COL_HI      5


static bool sim_running = false;
static struct program gui_progs[3] = {
    {"tmp1.txt", 0, 0},
    {"tmp2.txt", 0, 2},
    {"tmp3.txt", 0, 4}
};
static int gui_nprogs = 3;

/* ======================  GLOBAL UI HANDLE STRUCT  ==================== */
typedef struct {
    GtkWidget *win;

    GtkLabel  *lbl_clock;
    GtkLabel  *lbl_alg;
    GtkLabel  *lbl_total;

    GtkTreeView   *proc_tv;
    GtkListStore  *proc_store;

    GtkTreeView   *ready_tv;
    GtkListStore  *ready_store;

    GtkTreeView   *block_tv[NUM_RESOURCES];
    GtkListStore  *block_store[NUM_RESOURCES];

    GtkLabel  *res_lbl[NUM_RESOURCES];

    GtkLabel  *mem_lbl[60];

    GtkTextBuffer *log_buf;

    GtkComboBoxText *algobox;
    GtkSpinButton   *spin_quant;
    GtkButton       *btn_start, *btn_stop, *btn_step, *btn_reset;
} Ui;
static Ui ui;



/* worker thread */
static pthread_t worker_tid;
static int       worker_running = 0;
static int       auto_delay_us  = 100000;   /* 0.1s */

/* ------------ helpers ------------------------------------------------ */

static const char* state_to_str(int s)
{
    switch(s){
        case NEW:      return "New";
        case READY:    return "Ready";
        case RUNNING:  return "Running";
        case WAITING:  return "Blocked";   /* WAITING == blocked */
        case TERMINATED:return "Terminated";
        default:       return "?";
    }
}

/* ------------ populate GTK models from snapshot --------------------- */
static void populate_ui(const SimSnapshot *s)
{
    /* dashboard */
    char buf[64];
    sprintf(buf,"%d", s->clock);
    gtk_label_set_text(ui.lbl_clock, buf);

    gtk_label_set_text(ui.lbl_alg,
        s->algorithm==FCFS ?"FCFS":
        s->algorithm==RR   ?"Round‑Robin":"MLFQ");

    sprintf(buf,"%d", s->procs_total);
    gtk_label_set_text(ui.lbl_total, buf);

    /* processes ------------------------------------------------ */
    gtk_list_store_clear(ui.proc_store);
    for(int i=0;i<s->procs_total;i++){
        GtkTreeIter it;
        gtk_list_store_append(ui.proc_store,&it);
        gtk_list_store_set(ui.proc_store,&it,
            COL_PID,   s->proc[i].pid,
            COL_STATE, state_to_str(s->proc[i].state),
            COL_PC,    s->proc[i].pc,
            COL_PRIO,  s->proc[i].prio,
            COL_LO,    s->proc[i].mem_lo,
            COL_HI,    s->proc[i].mem_hi,
            -1);
    }
    /* ready queue --------------------------------------------- */
    gtk_list_store_clear(ui.ready_store);
    for(int i=0;i<s->ready_len;i++){
        GtkTreeIter it;
        gtk_list_store_append(ui.ready_store,&it);
        gtk_list_store_set(ui.ready_store,&it, 0, s->ready[i], -1);
    }
    /* block queues -------------------------------------------- */
    for(int r=0;r<NUM_RESOURCES;r++){
        gtk_list_store_clear(ui.block_store[r]);
        for(int i=0;i<s->block_len[r];i++){
            GtkTreeIter it;
            gtk_list_store_append(ui.block_store[r],&it);
            gtk_list_store_set(ui.block_store[r],&it, 0,
                               s->block[r][i], -1);
        }
    }
    /* resources ------------------------------------------------ */
    gtk_label_set_text(ui.res_lbl[USER_INPUT],
        s->res_free[USER_INPUT]?"free":"locked");
    gtk_label_set_text(ui.res_lbl[USER_OUTPUT],
        s->res_free[USER_OUTPUT]?"free":"locked");
    gtk_label_set_text(ui.res_lbl[FILE_ACCESS],
        s->res_free[FILE_ACCESS]?"free":"locked");

    /* memory grid – quick & dirty: show address number or "-"   */
    for(int i=0;i<60;i++){
        sprintf(buf,"%02d",i);
        gtk_label_set_text(ui.mem_lbl[i],buf);
    }
}

/* ============  THREAD‑SAFE CALLBACK  ================ */
static gboolean idle_apply_snapshot(gpointer data)
{
    SimSnapshot *snap = data;
    populate_ui(snap);

    /* append to log */
    char line[128];
    sprintf(line,"[C=%d] snapshot\n", snap->clock);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(ui.log_buf,&end);
    gtk_text_buffer_insert(ui.log_buf,&end,line,-1);

    free(snap);
    return G_SOURCE_REMOVE;
}

/* ============  Worker   ================ */
static void *worker(void *arg) {
    while (worker_running) {
        printf("[DEBUG] Worker loop started\n");
        SimSnapshot *snap = malloc(sizeof *snap);
        if (!snap) {
            perror("malloc failed");
            break;
        }

        int alive = sim_step(snap);
        printf("[DEBUG] sim_step returned alive=%d\n", alive);

        g_idle_add(idle_apply_snapshot, snap); // UI thread owns it

        if (!alive) {
            printf("[DEBUG] Simulation complete\n");
            worker_running = 0;
            break;
        }

        usleep(auto_delay_us);
    }
    printf("[DEBUG] Worker thread exiting\n");
    return NULL;
}

/* ============  BUTTON callbacks  ==================== */
static void on_start(GtkButton*btn,gpointer d)
{
    if(worker_running) return;

    
    SCHEDULING_ALGORITHM alg =
        gtk_combo_box_get_active(GTK_COMBO_BOX(ui.algobox));
    int q = gtk_spin_button_get_value_as_int(ui.spin_quant);

    /* reset & init the engine on our GLOBAL gui_progs[] array */
    sim_reset();
    sim_init(
      gui_progs,
      gui_nprogs,
      alg,
      q
    );

    /* now we really are running */
    sim_running = true;

    /* un‑grey the “Step” button */
    gtk_widget_set_sensitive(GTK_WIDGET(ui.btn_step), TRUE);


    gtk_widget_set_sensitive(GTK_WIDGET(ui.btn_step), TRUE);

        /* fire off the auto‑stepper thread */
    worker_running = 1;
    pthread_create(&worker_tid,NULL,worker,NULL);
}
static void on_stop(GtkButton*b,gpointer d){
    if(worker_running){
        worker_running = 0;
        pthread_join(worker_tid,NULL);
    }
}
static void on_step(GtkButton*b,gpointer d){
        /* if we haven’t pressed “Start” yet, don’t do anything */
        if (!sim_running) 
            return;
    
        /* grab one snapshot from the engine */
        SimSnapshot *snap = malloc(sizeof *snap);
        int alive = sim_step(snap);
    
        /* push it back onto the UI thread */
        g_idle_add(idle_apply_snapshot, snap);
    
        /* when the engine says “I’m done”, grey out Step again */
        if (!alive) {
            sim_running = false;
            gtk_widget_set_sensitive(GTK_WIDGET(ui.btn_step), FALSE);
        }
    }
static void on_reset(GtkButton*b,gpointer d){
    on_stop(NULL,NULL);
    sim_reset();
    gtk_text_buffer_set_text(ui.log_buf,"",0);
}

/* ============  UI construction ======================= */
static GtkWidget *make_tree(GtkListStore **store,
                            const char *col_title)
{
    *store = gtk_list_store_new(1,G_TYPE_INT);
    GtkWidget *tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(*store));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col =
        gtk_tree_view_column_new_with_attributes(col_title,r,"text",0,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tv),col);
    return tv;
}

static void build_ui(void)
{
    ui.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui.win),"Scheduler Simulator");
    gtk_window_set_default_size(GTK_WINDOW(ui.win),900,600);
    g_signal_connect(ui.win,"destroy",G_CALLBACK(gtk_main_quit),NULL);

    /* ---- main grid ---- */
    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(ui.win),grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid),6);
    gtk_grid_set_column_spacing(GTK_GRID(grid),6);

    /* DASHBOARD */
    GtkWidget *dash = gtk_frame_new("Dashboard");
    GtkWidget *dash_box = gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_container_add(GTK_CONTAINER(dash),dash_box);
    ui.lbl_clock = GTK_LABEL(gtk_label_new("0"));
    ui.lbl_alg   = GTK_LABEL(gtk_label_new("‑"));
    ui.lbl_total = GTK_LABEL(gtk_label_new("0"));
    gtk_box_pack_start(GTK_BOX(dash_box),
        gtk_label_new("Clock:"),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dash_box),
        GTK_WIDGET(ui.lbl_clock),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dash_box),
        gtk_label_new("Algorithm:"),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dash_box),
        GTK_WIDGET(ui.lbl_alg),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dash_box),
        gtk_label_new("Total processes:"),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(dash_box),
        GTK_WIDGET(ui.lbl_total),FALSE,FALSE,0);

    /* PROCESS TABLE */
    ui.proc_store = gtk_list_store_new(6,G_TYPE_INT,G_TYPE_STRING,
                                       G_TYPE_INT,G_TYPE_INT,
                                       G_TYPE_INT,G_TYPE_INT);
    ui.proc_tv = GTK_TREE_VIEW(
        gtk_tree_view_new_with_model(GTK_TREE_MODEL(ui.proc_store)));

    const char *titles[]={"PID","State","PC","Prio","Lo","Hi"};
    for(int c=0;c<6;c++){
        GtkCellRenderer *rend = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *col =
            gtk_tree_view_column_new_with_attributes(
                  titles[c],rend,"text",c,NULL);
        gtk_tree_view_append_column(ui.proc_tv,col);
    }

    /* READY queue */
    GtkWidget *ready_frame = gtk_frame_new("Ready Queue");
    ui.ready_tv = GTK_TREE_VIEW(make_tree(&ui.ready_store,"PID"));
    gtk_container_add(GTK_CONTAINER(ready_frame),
                      GTK_WIDGET(ui.ready_tv));

    /* BLOCK queues */
    const char *res_name[]={"userInput","userOutput","file"};
    GtkWidget *block_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);
    for(int r=0;r<NUM_RESOURCES;r++){
        char title[32]; sprintf(title,"Blocked ‑ %s",res_name[r]);
        GtkWidget *fr = gtk_frame_new(title);
        ui.block_tv[r] = GTK_TREE_VIEW(
            make_tree(&ui.block_store[r],"PID"));
        gtk_container_add(GTK_CONTAINER(fr),
                          GTK_WIDGET(ui.block_tv[r]));
        gtk_box_pack_start(GTK_BOX(block_box),fr,TRUE,TRUE,0);
    }

    /* MEMORY grid (10×6) */
    GtkWidget *mem_frame = gtk_frame_new("Memory (60 words)");
    GtkWidget *mem_grid  = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(mem_frame),mem_grid);
    gtk_grid_set_row_spacing(GTK_GRID(mem_grid),2);
    gtk_grid_set_column_spacing(GTK_GRID(mem_grid),2);

    for(int i=0;i<60;i++){
        ui.mem_lbl[i] = GTK_LABEL(gtk_label_new("--"));
        gtk_grid_attach(GTK_GRID(mem_grid),
                        GTK_WIDGET(ui.mem_lbl[i]), i%10, i/10,1,1);
    }

    /* RESOURCE status */
    GtkWidget *res_frame = gtk_frame_new("Mutexes");
    GtkWidget *res_grid  = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(res_frame),res_grid);
    for(int r=0;r<NUM_RESOURCES;r++){
        gtk_grid_attach(GTK_GRID(res_grid),
            gtk_label_new(res_name[r]),0,r,1,1);
        ui.res_lbl[r] = GTK_LABEL(gtk_label_new("free"));
        gtk_grid_attach(GTK_GRID(res_grid),
            GTK_WIDGET(ui.res_lbl[r]),1,r,1,1);
    }

    /* LOG */
    GtkWidget *log_frame = gtk_frame_new("Execution Log");
    GtkWidget *scr = gtk_scrolled_window_new(NULL,NULL);
    gtk_container_add(GTK_CONTAINER(log_frame),scr);
    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv),FALSE);
    gtk_container_add(GTK_CONTAINER(scr),tv);
    ui.log_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));

    /* CONTROL panel */
    GtkWidget *ctrl = gtk_frame_new("Controls");
    GtkWidget *ctrl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);
    gtk_container_add(GTK_CONTAINER(ctrl),ctrl_box);

    ui.algobox = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    gtk_combo_box_text_append_text(ui.algobox,"FCFS");
    gtk_combo_box_text_append_text(ui.algobox,"Round‑Robin");
    gtk_combo_box_text_append_text(ui.algobox,"MLFQ");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ui.algobox),0);

    ui.spin_quant = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1,10,1));
    gtk_spin_button_set_value(ui.spin_quant,2);

    ui.btn_start = GTK_BUTTON(gtk_button_new_with_label("Start"));
    ui.btn_stop  = GTK_BUTTON(gtk_button_new_with_label("Stop"));
    ui.btn_step  = GTK_BUTTON(gtk_button_new_with_label("Step"));
    ui.btn_reset = GTK_BUTTON(gtk_button_new_with_label("Reset"));

    g_signal_connect(ui.btn_start,"clicked",G_CALLBACK(on_start),NULL);
    g_signal_connect(ui.btn_stop ,"clicked",G_CALLBACK(on_stop ),NULL);
    g_signal_connect(ui.btn_step ,"clicked",G_CALLBACK(on_step ),NULL);
    g_signal_connect(ui.btn_reset,"clicked",G_CALLBACK(on_reset),NULL);

    gtk_box_pack_start(GTK_BOX(ctrl_box),GTK_WIDGET(ui.algobox),
                       FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ctrl_box),
        gtk_label_new("Quantum:"),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ctrl_box),
        GTK_WIDGET(ui.spin_quant),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ctrl_box),
        GTK_WIDGET(ui.btn_start),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ctrl_box),
        GTK_WIDGET(ui.btn_stop),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ctrl_box),
        GTK_WIDGET(ui.btn_step),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(ctrl_box),
        GTK_WIDGET(ui.btn_reset),FALSE,FALSE,0);

    /* — lay everything on the main grid — */
    gtk_grid_attach(GTK_GRID(grid),dash,       0,0,1,1);
    gtk_grid_attach(GTK_GRID(grid),
        GTK_WIDGET(ui.proc_tv),                1,0,2,1);
    gtk_grid_attach(GTK_GRID(grid),ready_frame,0,1,1,1);
    gtk_grid_attach(GTK_GRID(grid),block_box,  1,1,2,1);
    gtk_grid_attach(GTK_GRID(grid),mem_frame,  0,2,2,1);
    gtk_grid_attach(GTK_GRID(grid),res_frame,  2,2,1,1);
    gtk_grid_attach(GTK_GRID(grid),log_frame,  0,3,3,1);
    gtk_grid_attach(GTK_GRID(grid),ctrl,       0,4,3,1);

    gtk_widget_show_all(ui.win);


        /* right after gtk_widget_show_all(ui.win); */
    gtk_widget_set_sensitive(GTK_WIDGET(ui.btn_step), FALSE);
}

/* ==================== PUBLIC ENTRY ==================== */
void gui_init(int *argc,char ***argv)
{
    gtk_init(argc,argv);
    build_ui();
    gtk_main();
}
