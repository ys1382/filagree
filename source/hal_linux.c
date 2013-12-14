/*
 * File:   hal_linux.c
 * Author: Sinbad Flyce
 *
 * Created on December 5, 2013, 8:45 PM
 */



#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "struct.h"
#include "file.h"
#include "vm.h"
#include "hal.h"
#include "compile.h"

#ifndef NO_UI
#include <gtk/gtk.h>
#include <gdk/gdk.h>
static GtkWidget *window = NULL;
static GtkLayout *layout = NULL;
#endif

#define CONTROL_HEIGH   25

/*
 *  RECT struct
 */
typedef struct tagRECT {
    int x,y;
    int width, height;

} RECT;

/*
 * Actionifier
 */
typedef struct tagActionifier {
    struct variable *logic;
    struct context *context;
    struct variable *uictx;
    struct variable *param;
    struct variable *data;
    int timer;
} Actionifier;

/*
 * actionifier_new
 */
Actionifier * actionifier_new(struct context *f, struct variable *u, struct variable *c, struct variable *d)
{
    Actionifier *bp = (Actionifier *)malloc(sizeof(Actionifier));
    bp->logic = c;
    bp->context = f;
    bp->uictx = u;
    bp->data = d;
    bp->param = NULL;
    bp->timer = 0;
    return bp;
}
/*
 *  whereAmI
 */
RECT whereAmI(int x, int y, int w, int h)
{
    RECT rcResult = {x,y,w,h};
    return rcResult;
}

/*
 * resize
 */
void resize(GtkWidget *control, int32_t *w, int32_t *h)
{
    if (*w && *h)
        return;

    RECT rect = {0};
    gtk_widget_get_size_request(control,w,h);
    RECT new_rect = whereAmI(0,0, *w,*h);
    gtk_layout_move(layout,control,rect.x,rect.y);
    gtk_widget_set_size_request(control,rect.width,rect.height);
}

/*
 * act_callback
 */
void act_callback(Actionifier *act)
{
    if (act->logic && act->logic->type != VAR_NIL)
    {
        gil_lock(act->context, "pressed");
        vm_call(act->context, act->logic, act->uictx, act->param, NULL);
        gil_unlock(act->context, "pressed");
    }
}

/*
 *  on_press
 */
void on_press( GtkWidget *widget, gpointer data )
{
    g_print ("Hello again - %s was pressed\n", (char *) data);
    act_callback((Actionifier *)data);
}

/*
 *  
 */
GtkWidget *create_list_view()
{
    GtkListStore *liststore = gtk_list_store_new(1,G_TYPE_STRING);
    GtkTreeIter treeiter;
    
    int w = 0, h = 0;
    gtk_layout_get_size(layout,&w,&h);
    
    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(liststore));
    gtk_container_add(GTK_CONTAINER(layout), treeview);
    
    GtkCellRenderer *cellrenderertext = gtk_cell_renderer_text_new();
    
    
    GtkTreeViewColumn *treeviewcolumn = gtk_tree_view_column_new_with_attributes("Item",
                                                                                 cellrenderertext,
                                                                                 "text", 0,
                                                                                 NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), treeviewcolumn);
    
    gtk_widget_set_size_request(treeview, w, h);
    gtk_layout_put((GtkLayout *)layout, treeview, 0, 0);   
    return treeview;
}

void reload_listview(GtkWidget *listview, Actionifier *act)
{   
    GtkListStore *liststore = (GtkListStore*)gtk_tree_view_get_model((GtkTreeView*)listview);
    GtkTreeIter treeiter;
    gtk_list_store_clear(liststore);
    for (int ja = 0; ja < act->data->list.ordered->length; ja++) {
        struct variable *item = array_get(act->data->list.ordered, (uint32_t)ja);
        const char *name = variable_value_str(act->context, item);
        gtk_list_store_append(liststore, &treeiter);
        gtk_list_store_set(liststore, &treeiter,0,name,-1);       
    }
}

/*
 *  hal_print
 */
void hal_print(const char *str)
{
	printf("%s\n", str);
}

/*
 *  stub
 */
void stub(const char *name)
{
	printf("%s is a stub\n", name);
}

void hal_graphics(const struct variable *shape)         { stub("hal_graphics"); }
void hal_image()                                        { stub("hal_image"); }
void hal_sound()                                        { stub("hal_sound"); }
void hal_synth(const uint8_t *bytes, uint32_t length)   { stub("hal_synth"); }
void hal_audioloop()                                    { stub("hal_audioloop");
}

/*
 *  hal_window
 */
void *hal_window(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic)
{
    gtk_window_get_size(GTK_WINDOW(window),(gint*)w,(gint*)h);
    
    if (layout == NULL) {
        layout = (GtkLayout*)gtk_layout_new(NULL,NULL);
        gtk_layout_set_size(layout,*w,*h);
        gtk_container_add(GTK_CONTAINER(window),(GtkWidget*)layout);
    }
    else {
        gtk_container_remove(GTK_CONTAINER(window),(GtkWidget*)layout);
        layout = NULL;
    }
    
    return window;
}

/*
 *  hal_label
 */
void *hal_label (struct variable *uictx,
                 int32_t *w, int32_t *h,
                 const char *str)
{
    RECT rect = whereAmI(0,0,*w,*h);
    rect.width = 120;
    rect.height = 30;
    GtkWidget *pLabel = gtk_label_new(str);
    gtk_container_add(GTK_CONTAINER(layout),pLabel);
    gtk_layout_move(layout,pLabel,rect.x,rect.y);
    gtk_widget_set_size_request(pLabel,rect.width,rect.height); 
    resize(pLabel,w,h);
    return pLabel;
}

/*
 *  hal_input
 */
void *hal_input (struct variable *uictx,
                 int32_t *w, int32_t *h,
                 const char *hint,
                 bool multiline,
                 bool readonly)
{
    GtkWidget *edit = gtk_text_view_new();
    gtk_container_add(GTK_CONTAINER(layout), edit);
    gtk_widget_set_size_request(edit, 120, CONTROL_HEIGH);
    gtk_layout_move((GtkLayout *)layout, edit, 0,0);
    resize(edit,w,h);
    return (void*)edit;
}

/*
 *  hal_button
 */
void *hal_button(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic,
                 const char *str, const char *img)
{
    GtkWidget *button;
    Actionifier *act = actionifier_new(context,uictx,logic,NULL);
    button = gtk_button_new_with_label (str);
    gtk_container_add (GTK_CONTAINER (layout), button);

    g_signal_connect (button, "clicked",G_CALLBACK(on_press), (gpointer) act);
    gtk_widget_set_size_request(button, 300, CONTROL_HEIGH);
    gtk_layout_move((GtkLayout *)layout, button, 0, 0);
    resize(button,w,h);

    return (void*)button;
}

/*
 *  hal_table
 */
void *hal_table (struct context *context,
                 struct variable *uictx,
                 struct variable *list,
                 struct variable *logic)                
{
    assert_message(list && ((list->type == VAR_LST) || (list->type == VAR_NIL)), "not a list");
    if (list->type == VAR_NIL)
        list = variable_new_list(context, NULL);
    
    // create list view
    Actionifier *act = actionifier_new(context,uictx,logic,list);
    GtkWidget *list_view = create_list_view();
    g_object_set_data((GObject*)list_view,"userdata",act);
    reload_listview(list_view,act);
    return list_view;
}

void hal_sound_url(const char *address)                 { stub("hal_sound_url"); }
void hal_sound_bytes(const uint8_t *bytes,
                     uint32_t length)                   { stub("hal_sound_bytes"); }

void hal_save(struct context *context,
              const struct byte_array *key,
              const struct variable *value)             { stub("hal_save"); }
struct variable *hal_load(struct context *context,
                          const struct byte_array *key) { stub("hal_load"); return NULL; }

void hal_file_listen(struct context *context,
                     const char *path,
                     struct variable *listener)         { stub("hal_file_listen");
}

/*
 *  hal_doc_path
 */
const char *hal_doc_path(const struct byte_array *path)
{
    return path ? byte_array_to_string(path) : NULL;
}

bool hal_open(const char *path)				{ stub("hal_open"); return false;	}

void hal_loop(struct context *context)			{ stub("hal_loop");
}

/*
 *  hal_ui_get
 */
struct variable *hal_ui_get(struct context *context,
                            void *field)
{
    if (GTK_IS_TEXT_VIEW(field)) {
        GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(field);
        char *str = gtk_text_buffer_get_text(text_buffer,NULL,NULL,0);
        if (str != NULL)
            return variable_new_str_chars(context, str);
        return NULL;
    }

    return NULL;
}


gboolean ide_reload_message(gpointer gdata)
{
    Actionifier *act = g_object_get_data((GObject*)gdata,"userdata");
    reload_listview((GtkWidget*)gdata,act);
    return 0;
}


/*
 *  hal_ui_set
 */
void hal_ui_set(void *field, struct variable *value)
{
   if (GTK_IS_TEXT_VIEW(field)) {
        const char *value2 = byte_array_to_string(value->str);
        GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(field);
        if (value2 != NULL)
             gtk_text_buffer_set_text(text_buffer,value2,strlen(value2));
    }
   else if (GTK_IS_TREE_VIEW(field)) {
       Actionifier *act = g_object_get_data((GObject*)field,"userdata");
       if (act) {
           act->data = value;
           gdk_threads_add_idle(ide_reload_message,field);
       }
   }
}

/*
 *
 */
void hal_ui_put(void *field, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (field != NULL) {
        RECT rect = whereAmI(x,y,w,h);
        if (GTK_IS_TREE_VIEW(field)) {
            int offset = 4 * CONTROL_HEIGH + 5;                     
            gtk_layout_put(layout,field,0,offset);
            gtk_widget_set_size_request(field,rect.width,rect.height - offset);
        }
        else {           
            gtk_layout_put(layout,field,rect.x,rect.y);
            gtk_widget_set_size_request(field,rect.width,rect.height);
            gtk_widget_show_now((GtkWidget *)field);
        }
    }
}

void hal_sleep(int32_t miliseconds)                     { stub("hal_sleep"); }
void hal_timer(struct context *context,
               int32_t milliseconds,
               struct variable *logic,
               bool repeats)                            { stub("hal_timer");
}

/*
 *
 */
struct byte_array * read_bytes_from_resource(const char* resourceFile)
{
    char *scriptPath = (char*)resourceFile;
    struct byte_array *filename = byte_array_from_string(scriptPath);
    struct byte_array *bytereads = read_file(filename);
    byte_array_del(filename);
    //free(scriptPath);
    return bytereads;
}

/*
 * attach_filagree
 */
void attach_filagree()
{
#if 0
    struct byte_array *script = byte_array_from_string("sys.print('hi')");
#elif 0
    struct byte_array *script = read_bytes_from_resource("..//ui.fg");
#else
    struct byte_array *ui = read_bytes_from_resource("..//ui.fg");
    struct byte_array *sync = read_bytes_from_resource("sync.fg");
    struct byte_array *mesh = read_bytes_from_resource("../mesh.fg");
    struct byte_array *im_client = read_bytes_from_resource("sync_client.fg");
    struct byte_array *script = byte_array_concatenate(4, ui, mesh, sync, im_client);
#endif
    struct byte_array *program = build_string(script);
    execute(program);
}

/*
 *  GTK MAIN
 */
int main(int argc, char** argv)
{

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 960, 600);
    gtk_window_set_title(GTK_WINDOW(window), "Gtkfui");
    gtk_widget_show(window);

    g_signal_connect(window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
    
    attach_filagree();
    gtk_widget_show_all(window);
    gtk_main();
    return (EXIT_SUCCESS);
}

