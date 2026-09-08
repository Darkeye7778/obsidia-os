#include "rtc.h"
#include "acpi.h"
#include "idt.h"
#include <stdint.h>

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71
#define CMOS_STATUS_A 0x0a
#define CMOS_STATUS_B 0x0b

typedef struct{uint8_t second,minute,hour,day,month,year,century,status_b;}rtc_sample_t;
static uint8_t ready,century_register;
extern void serial_write(const char*);

static uint8_t cmos_read(uint8_t index){outb(CMOS_INDEX,(uint8_t)(0x80|index));return inb(CMOS_DATA);}
static int same(const rtc_sample_t*a,const rtc_sample_t*b){
    return a->second==b->second&&a->minute==b->minute&&a->hour==b->hour&&a->day==b->day&&a->month==b->month&&a->year==b->year&&a->century==b->century;
}
static int sample(rtc_sample_t*s){
    for(uint32_t wait=0;wait<1000000;wait++)if(!(cmos_read(CMOS_STATUS_A)&0x80))goto stable;
    return-1;
stable:
    s->second=cmos_read(0);s->minute=cmos_read(2);s->hour=cmos_read(4);s->day=cmos_read(7);s->month=cmos_read(8);s->year=cmos_read(9);s->status_b=cmos_read(CMOS_STATUS_B);s->century=century_register?cmos_read(century_register):0;return 0;
}
static uint32_t bcd(uint8_t value){return(value&15)+10U*(value>>4);}
static int leap(uint32_t year){return year%4==0&&(year%100!=0||year%400==0);}

int rtc_read_utc_seconds(uint64_t*seconds){
    if(!ready||!seconds)return-1;
    uint64_t flags;__asm__ volatile("pushfq;pop %0;cli":"=r"(flags)::"memory");
    rtc_sample_t a={0},b={0};int status=-1;
    for(uint32_t retry=0;retry<8;retry++){if(sample(&a)||sample(&b))break;if(same(&a,&b)){status=0;break;}a=b;}
    outb(CMOS_INDEX,0);if(flags&(1ULL<<9))__asm__ volatile("sti":::"memory");
    if(status)return-1;
    uint32_t second=a.second,minute=a.minute,hour=a.hour,day=a.day,month=a.month,year=a.year,century=a.century;
    if(!(a.status_b&4)){second=bcd(a.second);minute=bcd(a.minute);hour=bcd((uint8_t)(a.hour&0x7f));year=bcd(a.year);if(century_register)century=bcd(a.century);}
    else hour&=0x7f;
    if(!(a.status_b&2)){uint32_t pm=a.hour&0x80;if(hour==12)hour=0;if(pm)hour+=12;}
    year=century_register&&century?century*100+year:(year<70?2000:1900)+year;
    static const uint8_t month_days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(year<1970||month<1||month>12||day<1||day>month_days[month-1]+(month==2&&leap(year))||hour>23||minute>59||second>59)return-1;
    uint64_t days=0;for(uint32_t y=1970;y<year;y++)days+=365+leap(y);for(uint32_t m=1;m<month;m++)days+=month_days[m-1]+(m==2&&leap(year));days+=day-1;
    *seconds=days*86400ULL+hour*3600ULL+minute*60ULL+second;return 0;
}

int rtc_init(void){
    const acpi_platform_info_t*platform=acpi_platform_info();century_register=platform?platform->rtc_century_register:0;ready=1;uint64_t now;
    if(rtc_read_utc_seconds(&now)){ready=0;return-1;}serial_write("RTC: CMOS UTC wall clock ready\n");return 0;
}

