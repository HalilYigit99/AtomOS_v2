// Basit 64-bit aritmetik testleri.
#include <stdint.h>

// Assembly yardımcıları (libgcc benzeri)
uint64_t __adddi3(uint64_t, uint64_t);
uint64_t __subdi3(uint64_t, uint64_t);
int __cmpdi2(int64_t, int64_t);
int __ucmpdi2(uint64_t, uint64_t);
uint64_t __ashldi3(uint64_t, unsigned int);
uint64_t __lshrdi3(uint64_t, unsigned int);
int64_t __ashrdi3(int64_t, unsigned int);
uint64_t __udivdi3(uint64_t, uint64_t);
uint64_t __umoddi3(uint64_t, uint64_t);
int64_t __divdi3(int64_t, int64_t);
int64_t __moddi3(int64_t, int64_t);
uint64_t __muldi3(uint64_t, uint64_t);
int64_t __negdi2(int64_t);
int64_t __divmoddi4(int64_t, int64_t, int64_t*);

// Test sonucu: 0 == başarı, >0 hata sayısı
volatile int u64_test_failures = 0;

static int fails = 0;
#define T(expr) do { if(!(expr)) fails++; } while(0)

void run_u64_tests(void){
    // Toplama / çıkarma
    T(__adddi3(1ULL,2ULL)==3ULL);
    T(__subdi3(5ULL,3ULL)==2ULL);
    T(__adddi3(0xFFFFFFFFFFFFFFFFULL,1ULL)==0ULL); // taşma wrap

    // Karşılaştırma
    T(__cmpdi2(5,5)==0);
    T(__cmpdi2(-1,0)<0);
    T(__ucmpdi2(10ULL,9ULL)>0);
    T(__ucmpdi2(0ULL,0ULL)==0);

    // Kaydırmalar
    T(__ashldi3(1ULL,0)==1ULL);
    T(__ashldi3(1ULL,32)==(1ULL<<32));
    T(__ashldi3(1ULL,63)==(1ULL<<63));
    T(__lshrdi3(0x8000000000000000ULL,63)==1ULL);
    T(__lshrdi3(0xF0ULL,4)==0x0FULL);
    T(__ashrdi3((int64_t)0xFFFFFFFF80000000ULL,16)==(int64_t)0xFFFFFFFFFFFF8000ULL);
    T(__ashrdi3(-1LL,63)==-1LL);

    // Çarpma (düşük 64-bit)
    T(__muldi3(0xFFFFFFFFULL,0xFFFFFFFFULL)==0xFFFFFFFE00000001ULL);
    T(__muldi3(0x100000001ULL,2ULL)==0x200000002ULL);

    // Bölme / Mod (unsigned)
    T(__udivdi3(10ULL,3ULL)==3ULL);
    T(__umoddi3(10ULL,3ULL)==1ULL);
    T(__udivdi3(0x100000000ULL,2ULL)==0x80000000ULL);
    T(__umoddi3(0x100000000ULL,3ULL)==(0x100000000ULL%3ULL));

    // Bölme / Mod (signed - C99 trunc toward zero)
    T(__divdi3(-10LL,3LL)==-3LL);
    T(__moddi3(-10LL,3LL)==-1LL);
    T(__divdi3(10LL,-3LL)==-3LL);
    T(__moddi3(10LL,-3LL)==1LL);
    T(__divdi3(-10LL,-3LL)==3LL);
    T(__moddi3(-10LL,-3LL)==-1LL);

    // Negasyon
    T(__negdi2(5)==-5);
    T(__negdi2(-5)==5);

    // Birleşik divmod
    int64_t r=0; int64_t q=__divmoddi4(-10,3,&r); T(q==-3 && r==-1);

    u64_test_failures = fails;
}

// start.asm içinden çağrılacak; çıktıyı debugger/bochs üzerinden izleyebilirsiniz.
