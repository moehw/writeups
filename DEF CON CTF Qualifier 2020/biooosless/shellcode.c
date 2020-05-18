#define DMA_BUFF        0x3000
#define PIC0_OCW2		0x0020

#define DEF_PORT_DMAC0_STATUS       0x08  
#define DEF_PORT_DMAC0_COMMAND      0x08  
#define DEF_PORT_DMAC0_REQUEST      0x09  
#define DEF_PORT_DMAC0_SINGLE_MASK  0x0A  
#define DEF_PORT_DMAC0_MODE         0x0B  
#define DEF_PORT_DMAC0_CLEAR_BP     0x0C 
#define DEF_PORT_DMAC0_TEMPORARY    0x0D  
#define DEF_PORT_DMAC0_MASTER_CLEAR 0x0D  
#define DEF_PORT_DMAC0_MASK_RESET   0x0E 
#define DEF_PORT_DMAC0_ALL_MASK     0x0F  

#define STATUS_REGISTER_A 0x3F0
#define STATUS_REGISTER_B 0x3F1
#define DIGITAL_OUTPUT_REGISTER 0x3F2
#define TAPE_DRIVE_REGISTER 0x3F3
#define MAIN_STATUS_REGISTER 0x3F4
#define DATARATE_SELECT_REGISTER 0x3F4
#define DATA_FIFO 0x3F5
#define DIGITAL_INPUT_REGISTER 0x3F7
#define CONFIGURATION_CONTROL_REGISTER 0x3F7

#define WRITE_DATA 0x05
#define READ_DATA  0x06

int _start();
void print(short* lfb, char* floppy);
int in_8(int port);
void out_8(int port, char value);
void init_dma(short addr, short size);
void read_mode_dma();
void int_handler_6(int *esp);
void flpc_reset();
void msr_check_resultphase();
void flpyirq_wait();
void issue(int port, char command);
void dma_valid();
void motor_on();
void issue_command_read(char cylinder, char head, char sector);
void read_lba(int lba);
void read_sectors_lba(int lba, int sectors, char *buf);
void trans_to_buff(char *buf);


static char ReceivedIRQ6 = 1;
static char motor = 0;


int _start() {
    asm("mov esp, 0xfffe");

    // to real mode
    // asm("mov eax, cr0");
    // asm("xor eax, 0x1");
    // asm("mov cr0, eax");
    // asm("jmp 0:$+7");

    read_sectors_lba(34, 1, 0x10000);
    
    char* floppy = 0x10000;
    short* vga_buf = 0x0B8000;

    print(vga_buf, floppy);

    while(1);
}


void print(short* vga_buf, char* floppy) {
    int f_count = 0;
    int f_num = 60;
    for (int i = 0; i <= 0x200; i++) {
        lfb[i] = 0x0700 | floppy[i];
    }
}


int in_8(int port) {
    asm("mov dx, [ebp+8]");
    asm("in al, dx");
    asm("mov [ebp+8], al");
    return port;
}

void out_8(int port, char value) {
    asm("mov al, [ebp+0x0C]");
	asm("mov dx, [ebp+0x08]");
	asm("out dx, al");
    return;
}


void init_dma(short addr, short size) {
    // mask
    out_8(DEF_PORT_DMAC0_SINGLE_MASK, 0x06);

    out_8(DEF_PORT_DMAC0_CLEAR_BP, 0xff);

    out_8(0x04, addr & 0x00ff);
    out_8(0x04, (addr & 0xff00) >> 8);
    out_8(DEF_PORT_DMAC0_CLEAR_BP, 0xff);

    out_8(0x05, size & 0x00ff);
    out_8(0x05, (size & 0xff00) >> 8);

    out_8(0x81, 0);

    out_8(DEF_PORT_DMAC0_SINGLE_MASK, 0x02);

    return;
}

void read_mode_dma() {
    out_8(DEF_PORT_DMAC0_SINGLE_MASK, 0x06);
    out_8(DEF_PORT_DMAC0_MODE, 0x56);
    out_8(DEF_PORT_DMAC0_SINGLE_MASK, 0x02);
    return;
}

// unused
void int_handler_6(int *esp) {
    // PIC
    out_8(PIC0_OCW2, 0x66);
    ReceivedIRQ6 = 1;
    return;
}

void flpc_reset() {
    out_8(DIGITAL_OUTPUT_REGISTER, 0x00);
    out_8(DIGITAL_OUTPUT_REGISTER, 0x04);
    return;
}

// MSR check
void msr_check() {
    for(;;) {
        if((in_8(MAIN_STATUS_REGISTER) & 0xc0) == 0x80) {
            break;
        }
    }
}

// result_phase
void msr_check_resultphase() {
    for(;;) {
        if((in_8(MAIN_STATUS_REGISTER) & 0x50) == 0x50) {
            break;
        }
    }
    char result;
    result = in_8(DATA_FIFO);
    return;
}

// IRQ
void flpyirq_wait(){
    // while(ReceivedIRQ6 != 1); // won't do it
    // ReceivedIRQ6 = 0;
    return;
}

void issue(int port, char command) {
    msr_check();
    out_8(port, command);
    return;
}

void dma_valid() {
    msr_check();
    out_8(DATA_FIFO, 0x03);
    msr_check();
    out_8(DATA_FIFO, 0x00);
    msr_check();
    out_8(DATA_FIFO, 0x00);
    return;
}

// motor
void motor_on() {
    if(motor != 1) {
        out_8(DIGITAL_OUTPUT_REGISTER, 0x1c);
        motor = 1;
    }
    return;
}

void issue_command_read(char cylinder, char head, char sector) {

    short addr = DMA_BUFF;
    short size = 0x200;

    motor_on();
    init_dma(addr, size);
    dma_valid();
    read_mode_dma();

    // read data
    issue(DATA_FIFO, READ_DATA);
    issue(DATA_FIFO, 0x04&head);
    issue(DATA_FIFO, cylinder);
    issue(DATA_FIFO, head);
    issue(DATA_FIFO, sector);
    issue(DATA_FIFO, 0x02);
    issue(DATA_FIFO, 0x12);
    issue(DATA_FIFO, 0x1b);
    issue(DATA_FIFO, 0xff);

    // IRQ
    flpyirq_wait();

    msr_check_resultphase();
    msr_check_resultphase();
    msr_check_resultphase();
    msr_check_resultphase();
    msr_check_resultphase();
    msr_check_resultphase();
    msr_check_resultphase();

    return;
}

// LBA
void read_lba(int lba) {
    char cylinder;
    char head;
    char sector;

    cylinder = ((char)lba) / (18 * 2);
    head     = ((char)lba / 18) % 2;
    sector   = (((char)lba) % 18) + 1;

	issue_command_read(cylinder, head, sector);

    return;
}

void read_sectors_lba(int lba, int sectors, char *buf) {
    
    int i;
    for(i = lba; i < lba + sectors; i++) {
        read_lba(i);
        trans_to_buff(buf);
        buf = buf + 0x200; // + 1 sector
    }

    return;
}

void trans_to_buff(char *buf) {
    int i;
    for(i = 0; i < 512; i++){
        *(buf+i) = *((char*)DMA_BUFF + i);
    }
    return;
}

