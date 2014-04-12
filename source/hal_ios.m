#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <OpenGLES/ES1/gl.h>
#import <AVFoundation/AVFoundation.h>

#include "AppDelegate.h"
#include "struct.h"
#include "util.h"
#include "variable.h"
#include "vm.h"
#include "hal.h"
#include "struct.h"
#include "file.h"
#include "sys.h"
#include "compile.h"

#define MARGIN_X 10
#define MARGIN_Y 30


static UIView *content = NULL;
static NSString *doc_dir = NULL;


NSString *byte_array_to_nsstring(const struct byte_array *str)
{
    const char *str2 = byte_array_to_string(str);
    return [NSString stringWithUTF8String:str2];
}



struct byte_array *read_resource(const char *path)
{
    NSString *path2 = [NSString stringWithUTF8String:path];
    NSString *resource = [path2 stringByDeletingPathExtension];
    NSString *type = [path2 pathExtension];
    NSString* path3 = [[NSBundle mainBundle] pathForResource:resource ofType:type inDirectory:@""];
    if (!path3) {
        NSLog(@"can't load file");
        return NULL;
    }
    NSError *error = nil;
    NSString* contents = [NSString stringWithContentsOfFile:path3 encoding:NSUTF8StringEncoding error:&error];
    if (error) {
        NSLog(@"%@", [error localizedDescription]);
        return NULL;
    }
    
    const char *contents2 = [contents UTF8String];
    return byte_array_from_string(contents2);
}


CGRect whereAmI(int x, int y, int w, int h)
{
    return CGRectMake(x + MARGIN_X, y + MARGIN_Y, w, h);
}

void resize(UIView *control,
            int32_t *w, int32_t *h)
{
    if (*w && *h)
        return;
    [control sizeToFit];
    CGSize size = control.bounds.size;
    *w = size.width;
    *h = size.height;
    CGRect rect = whereAmI(0,0, *w,*h);
    [control setFrame:rect];
}

void hal_ui_put(void *widget, int32_t x, int32_t y, int32_t w, int32_t h)
{
    UIControl *control = (__bridge UIControl*)widget;
    CGRect rect = whereAmI(x, y, w, h);
    [control setFrame:rect];
}

void *hal_label(struct variable *uictx,
                int32_t *w, int32_t *h,
                const char *str,
                void *widget)
{
    if (widget) return widget;
    CGRect rect = whereAmI(0,0, *w,*h);
    UILabel* label = [[UILabel alloc] initWithFrame: rect];
    NSString *string = [NSString stringWithUTF8String:str];
    [label setText:string];
    [label setTextColor: [UIColor orangeColor]];
    [content addSubview:label];

    resize(label, w, h);
    return (void *)CFBridgingRetain(label);
}

/////// Actionifier - delegate for handling events

@interface Actionifier  : NSObject <UITableViewDataSource, UITableViewDelegate>
{
    struct variable *logic;
    struct context *context;
    struct variable *uictx;
    struct variable *param;
    struct variable *data;
    NSTimer *timer;
}

-(void)setData:(struct variable*)value;
-(void)setTimer:(double)interval repeats:(bool)repeats;
-(IBAction)pressed:(id)sender;
-(void)timerCallback:(NSTimer*)timer;
-(void)callback;
-(void)resized;

@end

@implementation Actionifier

+(Actionifier*) fContext:(struct context *)f
               uiContext:(struct variable*)u
                callback:(struct variable*)c
                userData:(struct variable*)d
{
    Actionifier *bp = [Actionifier alloc];
    bp->logic = c;
    bp->context = f;
    bp->uictx = u;
    bp->data = d;
    bp->param = NULL;
    bp->timer = NULL;
    return bp;
}

-(void)setTimer:(double)interval repeats:(bool)repeats
{
    if (interval >= 1000)
        interval /= 1000;
    else
        interval *= -1;

    self->timer = [NSTimer scheduledTimerWithTimeInterval:interval
                                                   target:self
                                                 selector:@selector(timerCallback:)
                                                 userInfo:nil
                                                  repeats:repeats];

    [[NSRunLoop mainRunLoop] addTimer:self->timer forMode:NSDefaultRunLoopMode];
}

-(void)setData:(struct variable*)value {
    self->data = value;
}

- (NSInteger)numberOfSections {
    return 1;
}

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return self->data->list.ordered->length;
}

//4
-(UITableViewCell *)tableView:(UITableView *)tableView
        cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *cellIdentifier = @"TableCell";

    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:cellIdentifier];

    if (cell == nil)
    {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                      reuseIdentifier:cellIdentifier];
    }

    struct variable *item = array_get(self->data->list.ordered, (uint32_t)indexPath.row);
    const char *name = variable_value_str(self->context, item);
    NSString *name2 = [NSString stringWithUTF8String:name];
    name2 = [name2 stringByReplacingOccurrencesOfString:@"'" withString:@""];

    [cell.textLabel setText:name2];
    //[cell.detailTextLabel setText:@"detail"];
    return cell;
}

- (void)tableView:(UITableView *)table didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    int32_t row = (int32_t)indexPath.row;
    if (row == -1)
        return;
    self->param = variable_new_int(self->context, row);
    [self pressed:table];
}

-(IBAction)pressed:(id)sender {
    [self callback];
}

-(void)timerCallback:(NSTimer*)timer {
    [self callback];
}

-(void)callback
{
    if (self->logic && self->logic->type != VAR_NIL)
    {
        gil_lock(self->context, "pressed");
        vm_call(self->context, self->logic, self->uictx, self->param, NULL);
        gil_unlock(self->context, "pressed");
    }
}

-(void)resized
{
    gil_lock(self->context, "resize");
    vm_call(self->context, self->logic, self->uictx, self->param, NULL);
    gil_unlock(self->context, "resize");
}

@end // Actionifier implementation

/////// ViewController

@interface ViewController : UIViewController <UITextFieldDelegate>
{
    Actionifier *actionifier;
    IBOutlet UIScrollView *theScrollView;
    UITextField *activeTextField;
}


- (IBAction)dismissKeyboard:(id)sender;

+ (ViewController *)sharedViewController;

@end

static ViewController *theViewController = NULL;

@implementation ViewController


+ (ViewController *)sharedViewController {
    return theViewController;
}

-(void) setDelegate:(struct context *)context uictx:(struct variable *)uictx logic:(struct variable *)logic
{
    Actionifier *a = [Actionifier fContext:context
                                 uiContext:uictx
                                  callback:logic
                                  userData:NULL];
    CFRetain((__bridge CFTypeRef)(a));
    self->actionifier = a;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    theViewController = self;
    content = self.view;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(keyboardWasShown:)
                                                 name:UIKeyboardDidShowNotification
                                               object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(keyboardWillHide:)
                                                 name:UIKeyboardWillHideNotification
                                               object:nil];

    struct byte_array *ui = read_resource("ui.fg");
    struct byte_array *mesh = read_resource("mesh.fg");
    struct byte_array *client = read_resource("im_client.fg");
    struct byte_array *args = byte_array_from_string("id='IOS'");
    struct byte_array *script = byte_array_concatenate(4, ui, mesh, args, client);
    
    struct byte_array *program = build_string(script, NULL);
    execute(program);
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
	CGFloat w = content.bounds.size.width;
	CGFloat h = content.bounds.size.height;
    NSLog(@"resized to %f,%f", w, h);

    [self->actionifier resized];
}

/////// handle keyboard appearing

- (void)keyboardWasShown:(NSNotification *)notification
{
    
    // Step 1: Get the size of the keyboard.
    CGSize keyboardSize = [[[notification userInfo] objectForKey:UIKeyboardFrameBeginUserInfoKey] CGRectValue].size;
    
    
    // Step 2: Adjust the bottom content inset of your scroll view by the keyboard height.
    UIEdgeInsets contentInsets = UIEdgeInsetsMake(0.0, 0.0, keyboardSize.height, 0.0);
    theScrollView.contentInset = contentInsets;
    theScrollView.scrollIndicatorInsets = contentInsets;
    
    
    // Step 3: Scroll the target text field into view.
    CGRect aRect = self.view.frame;
    aRect.size.height -= keyboardSize.height;
    if (!CGRectContainsPoint(aRect, activeTextField.frame.origin) ) {
        CGPoint scrollPoint = CGPointMake(0.0, activeTextField.frame.origin.y - (keyboardSize.height-15));
        [theScrollView setContentOffset:scrollPoint animated:YES];
    }
}

- (void) keyboardWillHide:(NSNotification *)notification
{
    UIEdgeInsets contentInsets = UIEdgeInsetsZero;
    theScrollView.contentInset = contentInsets;
    theScrollView.scrollIndicatorInsets = contentInsets;
}

- (void)textFieldDidBeginEditing:(UITextField *)textField {
    self->activeTextField = textField;
}

- (void)textFieldDidEndEditing:(UITextField *)textField {
    self->activeTextField = nil;
}

- (IBAction)dismissKeyboard:(id)sender {
    [activeTextField resignFirstResponder];
}

@end // ViewController implementation


void *hal_button(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic,
                 const char *str, const char *img,
                 void *button)
{
    UIButton *my;
    if (NULL == button)
    {
        CGRect rect = whereAmI(0,0, *w,*h);
        my = [UIButton buttonWithType:UIButtonTypeRoundedRect];
        [my setFrame:rect];

        [content addSubview: my];
        NSString *string = [NSString stringWithUTF8String:str];
        [my setTitle:string forState: UIControlStateNormal];

        if (img)
        {
            string = [NSString stringWithUTF8String:img];
            NSURL* url = [NSURL fileURLWithPath:string];
            UIImage *image = [UIImage imageWithData:[NSData dataWithContentsOfURL:url]];
            [my setImage:image forState:UIControlStateNormal];
        }

        Actionifier *act = [Actionifier fContext:context
                                       uiContext:uictx
                                        callback:logic
                                        userData:NULL];
        CFRetain((__bridge CFTypeRef)(act));
        [my addTarget:act action:@selector(pressed:) forControlEvents:UIControlEventTouchUpInside];
    }
    else my = (__bridge UIButton *)(button);
    resize(my, w, h);
    return (void *)CFBridgingRetain(my);
}

void *hal_input(struct variable *uictx,
                int32_t *w, int32_t *h,
                const char *hint,
                bool multiline,
                bool readonly,
                void *input)
{
    *w = content.bounds.size.width / 2;
    *h = 20;
    CGRect rect = whereAmI(0,0, *w,*h);
    UIView *textField;
    if (NULL != input)
    {
        textField = (__bridge UIView *)(input);
        [textField setFrame:rect];
        return input;
    }

    //NSString *string = hint ? [NSString stringWithUTF8String:hint] : NULL;

    if (multiline)
    {
        textField = [[UITextView alloc] initWithFrame:rect];
        [(UITextView*)textField setEditable:!readonly];
        ((UITextView*)textField).layer.borderWidth = 1;
        ((UITextView*)textField).layer.borderColor = [[UIColor grayColor] CGColor];
    }
    else
    {
        textField = [[UITextField alloc] initWithFrame:rect];
        [(UITextField*)textField setEnabled:!readonly];
        [(UITextField*)textField setBorderStyle:UITextBorderStyleBezel];
        [(UITextField*)textField setDelegate:theViewController];
    }
//    if (NULL != string)
//        ((UITextView*)textField).text = string;

    
    [content addSubview:textField];
    return (void *)CFBridgingRetain(textField);
}

struct variable *hal_ui_get(struct context *context, void *widget)
{
    NSObject *widget2 = (__bridge NSObject*)widget;
    if ([widget2 isKindOfClass:[UITextField class]])
    {
        UITextField *widget3 = (__bridge UITextField*)widget;
        NSString *value = widget3.text;
        const char *value2 = [value UTF8String];
        return variable_new_str_chars(context, value2);
    }
    return variable_new_nil(context);
}

void hal_ui_set(void *widget, struct variable *value)
{
    NSObject *widget2 = (__bridge NSObject*)widget;
    if ([widget2 isKindOfClass:[UITextField class]] || [widget2 isKindOfClass:[UITextView class]])
    {
        UITextField *widget3 = (__bridge UITextField*)widget;
        const char *value2 = byte_array_to_string(value->str);
        NSString *value3 = [NSString stringWithUTF8String:value2];
        [[NSOperationQueue mainQueue] addOperationWithBlock:^ {
            widget3.text = value3;
        }];
    }

    else if ([widget2 isKindOfClass:[UITableView class]])
    {
        UITableView *widget3 = (__bridge UITableView*)widget;
        Actionifier *a = (Actionifier*)[widget3 delegate];
        CFRetain((__bridge CFTypeRef)(a));
        [a setData:value];
        [widget3 reloadData];
    }
    else
        exit_message("unknown ui widget type");
}

void *hal_table(struct context *context,
                struct variable *uictx,
                struct variable *list,
                struct variable *logic,
                void *table)
{
    if (NULL != table)
        return table;

    assert_message(list && ((list->type == VAR_LST) || (list->type == VAR_NIL)), "not a list");
    if (list->type == VAR_NIL)
        list = variable_new_list(context, NULL);

    UITableView *tableView = [[UITableView alloc] init];

    Actionifier *a = [Actionifier fContext:context
                                 uiContext:uictx
                                  callback:logic
                                  userData:list];
    CFRetain((__bridge CFTypeRef)(a));
    [tableView setDelegate:a];
    [tableView setDataSource:(id<UITableViewDataSource>)a];

    [content addSubview:tableView];
    return (void *)CFBridgingRetain(tableView);
}

// todo: implement. requires callback.
bool hal_alert (const char *title, const char *message) {
    return true;
}

void *hal_window(struct context *context,
                 struct variable *uictx,
                 int32_t *w, int32_t *h,
                 struct variable *logic)
{
    [[ViewController sharedViewController] setDelegate:context uictx:uictx logic:logic];

    NSArray *subviews = [NSArray arrayWithArray:[content subviews]];
    [[NSOperationQueue mainQueue] addOperationWithBlock:^ {
        [subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
    }];
    [content setNeedsDisplay];

    CGSize size = content.bounds.size;
    *w = size.width  - MARGIN_X*2;
    *h = size.height - MARGIN_Y*2;

    return NULL;
}

void hal_log(const char *str) {
    NSLog(@"%s", str);
}

NSString *doc_path2(const struct byte_array *path)
{
    if (NULL == doc_dir)
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        doc_dir = [paths objectAtIndex:0];
    }

    NSString *path3;
    if (NULL == path)
        path3 = @"";
    else
    {
        char* path2 = byte_array_to_string(path);
        path3 = [NSString stringWithUTF8String:path2];
        free(path2);
    }
    return [doc_dir stringByAppendingFormat:@"/%@", path3];
}

const char *hal_doc_path(const struct byte_array *path)
{
    NSString *path2 = doc_path2(path);
    return [path2 UTF8String];
}

struct variable *sys_mkdir(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);

    NSError *error;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:doc_path2(path->str)
                                   withIntermediateDirectories:YES
                                                    attributes:nil
                                                         error:&error])
    {
        NSLog(@"Create directory error: %@", error);
    }

    return NULL;
}

struct variable *sys_mv(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *src = (struct variable*)array_get(value->list.ordered, 1);
    struct variable *dst = (struct variable*)array_get(value->list.ordered, 2);

    NSString *src2 = doc_path2(src->str);
    NSString *dst2 = doc_path2(dst->str);

    NSError *error;
    if (![[NSFileManager defaultManager] moveItemAtPath:src2 toPath:dst2 error:&error])
        NSLog(@"File move error: %@", error);
    
    return NULL;
}


struct variable *sys_rm(struct context *context)
{
    struct variable *value = (struct variable*)stack_pop(context->operand_stack);
    struct variable *path = (struct variable*)array_get(value->list.ordered, 1);

    NSError *error;
    if (![[NSFileManager defaultManager] removeItemAtPath:doc_path2(path->str) error:&error])
        NSLog(@"File delete error: %@", error);
    
    return NULL;
}

void hal_file_listen(struct context *context, const char *path, struct variable *listener)
{
    printf("hal_file_listen not implemented\n");
}

/////// timer

void hal_timer(struct context *context,
               int32_t milliseconds,
               struct variable *logic,
               bool repeats)
{
    Actionifier *actionifier = [Actionifier fContext:context
                                           uiContext:NULL
                                            callback:logic
                                            userData:NULL];
    CFRetain((__bridge CFTypeRef)(actionifier));
    [actionifier setTimer:milliseconds repeats:repeats];
}

/////// open-in

@interface Docufier : NSObject <UIDocumentInteractionControllerDelegate>
{
}

@end

@implementation Docufier

- (UIViewController *)documentInteractionControllerViewControllerForPreview: (UIDocumentInteractionController *)controller {
    return [ViewController sharedViewController];
}

@end // Docufier implementation

bool hal_open(const char *path)
{
    NSString *path2 = [doc_dir stringByAppendingFormat:@"/%s", path];
    NSURL *fileUrl = [NSURL fileURLWithPath:path2];
    UIDocumentInteractionController *dic = [UIDocumentInteractionController interactionControllerWithURL:fileUrl];
    Docufier *df = [Docufier alloc];
    [dic setDelegate:df];

    CFRetain((__bridge CFTypeRef)(dic));
    CFRetain((__bridge CFTypeRef)(df));

//    CGRect rect = CGRectMake(0, 0, 10, 10);
//    return [dic presentOpenInMenuFromRect:rect inView:content animated:YES];
    return [dic presentPreviewAnimated:YES];
}

//////////////////////////////////////////////////////////// graphics

void hal_graphics(const struct variable *shape) {
    printf("\nhal_graphics not implemented\n");
}

void hal_loop() {}

void hal_image()
{
    CGRect rect = content.bounds;
    rect.origin.x = rect.size.width/2;
    UIImageView *iv = [[UIImageView alloc] initWithFrame:rect];

    NSURL *url = [NSURL URLWithString:@"http://www.cgl.uwaterloo.ca/~csk/projects/starpatterns/noneuclidean/323ball.jpg"];
    UIImage *pic = [UIImage imageWithData:[NSData dataWithContentsOfURL:url]];
    if (pic)
        [iv setImage:pic];
    [content addSubview:iv];
}


///////////////////////////////////////////////////////////////////// sound

void hal_sound(const char *address)
{
    //NSURL *url = [NSURL URLWithString:@"http://www.wavlist.com/soundfx/011/duck-quack3.wav"];
    NSString *str = [NSString stringWithCString:address encoding:NSUTF8StringEncoding];
    NSURL *url = [NSURL URLWithString:str];
    NSError *error = nil;
	AVAudioPlayer* audioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
    [audioPlayer play];
}

struct Sound {
    short *samples;
	size_t buf_size;
    unsigned sample_rate;
    int seconds;
};

#define CASE_RETURN(err) case (err): return "##err"
const char* al_err_str(ALenum err) {
    switch(err) {
            CASE_RETURN(AL_NO_ERROR);
            CASE_RETURN(AL_INVALID_NAME);
            CASE_RETURN(AL_INVALID_ENUM);
            CASE_RETURN(AL_INVALID_VALUE);
            CASE_RETURN(AL_INVALID_OPERATION);
            CASE_RETURN(AL_OUT_OF_MEMORY);
    }
    return "unknown";
}
#undef CASE_RETURN

#define __al_check_error(file,line) \
do { \
ALenum err = alGetError(); \
for(; err!=AL_NO_ERROR; err=alGetError()) { \
printf("AL Error %s at %s:%d\n", al_err_str(err), file, line ); \
} \
}while(0)

#define al_check_error() \
__al_check_error(__FILE__, __LINE__)

/* initialize OpenAL */
ALuint init_al() {
	ALCdevice *dev = NULL;
	ALCcontext *ctx = NULL;

	const char *defname = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    printf("Default ouput device: %s\n", defname);

	dev = alcOpenDevice(defname);
	ctx = alcCreateContext(dev, NULL);
	alcMakeContextCurrent(ctx);

	/* Create buffer to store samples */
	ALuint buf;
	alGenBuffers(1, &buf);
	al_check_error();
    return buf;
}

/* Dealloc OpenAL */
void exit_al() {
	ALCdevice *dev = NULL;
	ALCcontext *ctx = NULL;
	ctx = alcGetCurrentContext();
	dev = alcGetContextsDevice(ctx);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(ctx);
	alcCloseDevice(dev);
}

void hal_sleep(int32_t miliseconds)
{
    struct timespec req={0};
    time_t sec = (int)(miliseconds/1000);
    miliseconds = (int32_t)(miliseconds - (sec * 1000));
    req.tv_sec = sec;
    req.tv_nsec = miliseconds * 1000000L;
    while (nanosleep(&req,&req) == -1)
        continue;
}

#define SYNTH_SAMPLE_RATE 44100 // CD quality

void hal_synth(const uint8_t *bytes, uint32_t length)
{
    short *samples = (short*)bytes;
    uint32_t size = length / sizeof(short);
    float duration = size * 1.0f / SYNTH_SAMPLE_RATE;

    ALuint buf = init_al();
    /* Download buffer to OpenAL */
	alBufferData(buf, AL_FORMAT_MONO16, samples, size, SYNTH_SAMPLE_RATE);
	al_check_error();

	/* Set-up sound source and play buffer */
	ALuint src = 0;
	alGenSources(1, &src);
	alSourcei(src, AL_BUFFER, buf);
	alSourcePlay(src);

	/* While sound is playing, sleep */
	al_check_error();

    hal_sleep(duration*1000);

	exit_al();
}

#define QUEUEBUFFERCOUNT 2
#define QUEUEBUFFERSIZE 9999

ALvoid hal_audio_loop(ALvoid)
{
    ALCdevice   *pCaptureDevice;
    const       ALCchar *szDefaultCaptureDevice;
    ALint       lSamplesAvailable;
    ALchar      Buffer[QUEUEBUFFERSIZE];
    ALuint      SourceID, TempBufferID;
    ALuint      BufferID[QUEUEBUFFERCOUNT];
    ALuint      ulBuffersAvailable = QUEUEBUFFERCOUNT;
    ALuint      ulUnqueueCount, ulQueueCount;
    ALint       lLoop, lFormat, lFrequency, lBlockAlignment, lProcessed, lPlaying;
    ALboolean   bPlaying = AL_FALSE;
    ALboolean   bPlay = AL_FALSE;

    // does not setup the Wave Device's Audio Mixer to select a recording input or recording level.

	ALCdevice *dev = NULL;
	ALCcontext *ctx = NULL;

	const char *defname = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    printf("Default ouput device: %s\n", defname);

	dev = alcOpenDevice(defname);
	ctx = alcCreateContext(dev, NULL);
	alcMakeContextCurrent(ctx);

    // Generate a Source and QUEUEBUFFERCOUNT Buffers for Queuing
    alGetError();
    alGenSources(1, &SourceID);

    for (lLoop = 0; lLoop < QUEUEBUFFERCOUNT; lLoop++)
        alGenBuffers(1, &BufferID[lLoop]);

    if (alGetError() != AL_NO_ERROR) {
        printf("Failed to generate Source and / or Buffers\n");
        return;
    }

    ulUnqueueCount = 0;
    ulQueueCount = 0;

    // Get list of available Capture Devices
    const ALchar *pDeviceList = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    if (pDeviceList) {
        printf("Available Capture Devices are:-\n");

        while (*pDeviceList) {
            printf("%s\n", pDeviceList);
            pDeviceList += strlen(pDeviceList) + 1;
        }
    }

    szDefaultCaptureDevice = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
    printf("\nDefault Capture Device is '%s'\n\n", szDefaultCaptureDevice);

    // The next call can fail if the WaveDevice does not support the requested format, so the application
    // should be prepared to try different formats in case of failure

    lFormat = AL_FORMAT_MONO16;
    lFrequency = 44100;
    lBlockAlignment = 2;

    long lTotalProcessed = 0;
    long lOldSamplesAvailable = 0;
    long lOldTotalProcessed = 0;

    pCaptureDevice = alcCaptureOpenDevice(szDefaultCaptureDevice, lFrequency, lFormat, lFrequency);
    if (pCaptureDevice) {
        printf("Opened '%s' Capture Device\n\n", alcGetString(pCaptureDevice, ALC_CAPTURE_DEVICE_SPECIFIER));

        printf("start capture\n");
        alcCaptureStart(pCaptureDevice);
        bPlay = AL_TRUE;

        for (;;) {
            //alcCaptureStop(pCaptureDevice);

            alGetError();
            alcGetIntegerv(pCaptureDevice, ALC_CAPTURE_SAMPLES, 1, &lSamplesAvailable);

            if ((lOldSamplesAvailable != lSamplesAvailable) || (lOldTotalProcessed != lTotalProcessed)) {
                printf("Samples available is %d, Buffers Processed %ld\n", lSamplesAvailable, lTotalProcessed);
                lOldSamplesAvailable = lSamplesAvailable;
                lOldTotalProcessed = lTotalProcessed;
            }

            // If the Source is (or should be) playing, get number of buffers processed
            // and check play status
            if (bPlaying) {
                alGetSourcei(SourceID, AL_BUFFERS_PROCESSED, &lProcessed);
                while (lProcessed) {
                    lTotalProcessed++;

                    // Unqueue the buffer
                    alSourceUnqueueBuffers(SourceID, 1, &TempBufferID);

                    // Update unqueue count
                    if (++ulUnqueueCount == QUEUEBUFFERCOUNT)
                        ulUnqueueCount = 0;

                    // Increment buffers available
                    ulBuffersAvailable++;

                    lProcessed--;
                }

                // If the Source has stopped (been starved of data) it will need to be
                // restarted
                alGetSourcei(SourceID, AL_SOURCE_STATE, &lPlaying);
                if (lPlaying == AL_STOPPED) {
                    printf("Buffer Stopped, Buffers Available is %d\n", ulBuffersAvailable);
                    bPlay = AL_TRUE;
                }
            }

            if ((lSamplesAvailable > (QUEUEBUFFERSIZE / lBlockAlignment)) && !(ulBuffersAvailable)) {
                printf("underrun!\n");
            }

            // When we have enough data to fill our QUEUEBUFFERSIZE byte buffer, grab the samples
            else if ((lSamplesAvailable > (QUEUEBUFFERSIZE / lBlockAlignment)) && (ulBuffersAvailable)) {
                // Consume Samples
                alcCaptureSamples(pCaptureDevice, Buffer, QUEUEBUFFERSIZE / lBlockAlignment);
                alBufferData(BufferID[ulQueueCount], lFormat, Buffer, QUEUEBUFFERSIZE, lFrequency);

                // Queue the buffer, and mark buffer as queued
                alSourceQueueBuffers(SourceID, 1, &BufferID[ulQueueCount]);
                if (++ulQueueCount == QUEUEBUFFERCOUNT)
                    ulQueueCount = 0;

                // Decrement buffers available
                ulBuffersAvailable--;

                // If we need to start the Source do it now IF AND ONLY IF we have queued at least 2 buffers
                if ((bPlay) && (ulBuffersAvailable <= (QUEUEBUFFERCOUNT - 2))) {
                    alSourcePlay(SourceID);
                    printf("Buffer Starting \n");
                    bPlaying = AL_TRUE;
                    bPlay = AL_FALSE;
                }
            }
        }
        alcCaptureCloseDevice(pCaptureDevice);
    } else
        printf("WaveDevice is unavailable, or does not supported the request format\n");

    alSourceStop(SourceID);
    alDeleteSources(1, &SourceID);
    for (lLoop = 0; lLoop < QUEUEBUFFERCOUNT; lLoop++)
        alDeleteBuffers(1, &BufferID[lLoop]);
}
