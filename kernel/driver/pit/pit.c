#include <driver/DriverBase.h>
#include <memory/memory.h>
#include <time/timer.h>
#include <irq/IRQ.h>
#include <arch.h>
#include <debug/debug.h>
#include <list.h>

// PIT (8253/8254) Channel 0 – IRQ0
#define IRQ_PIT 0

// PIT I/O ports
#define PIT_CH0_PORT 0x40
#define PIT_MODE_PORT 0x43

// PIT input clock (Hz)
#define PIT_BASE_FREQ 1193182u

// Mode 3 (square wave), channel 0, lobyte/hibyte
#define PIT_CMD_CH0_LOHI_MODE3 0x36

HardwareTimer* pit_timer; // Pointer to the PIT timer
List* pit_timer_tick_handlers; // List of PIT timer handlers

static bool pit_running = false;

extern DriverBase pit_driver; // PIT driver instance
extern void pit_timer_isr(); // PIT timer interrupt service routine
void pit_timer_handler();

// Forward declarations for timer ops
static void pit_timer_init_wrapper();
int pit_start();
int pit_stop();
int pit_setPrescaler(uint32_t prescaler);
int pit_setFrequency(uint32_t frequency);
void pit_add_callback(void (*callback)());
void pit_remove_callback(void (*callback)());

bool pit_init() {
    // HardwareTimer nesnesini statik instance üzerinden bağlayacağız
    extern HardwareTimer pit_timer_instance; // aşağıda tanımlı
    pit_timer = &pit_timer_instance;

    // Varsayılan frekans (Hz)
    pit_timer->frequency = 1000; // 100 Hz klasik OS tick

    // ISR kaydı (IRQ0 -> IDT vektör 32)
    if (irq_controller && irq_controller->register_handler) {
        irq_controller->register_handler(IRQ_PIT, pit_timer_isr);
    } else {
        ERROR("PIT: IRQ controller not initialized");
        return false;
    }

    pit_timer_tick_handlers = NULL; // callback listesi lazy oluşturulacak

    LOG("PIT: Initialized (default %zu Hz)", pit_timer->frequency);

    pit_driver.enabled = false; // henüz başlatılmadı
    return true;
}

int pit_start()
{
    if (!pit_timer) return -1;

    // Seçili frekansı programla
    if (pit_setFrequency((uint32_t)pit_timer->frequency) != 0) {
        return -1;
    }

    // IRQ0'ı enable et (PIC/APIC fark etmeksizin)
    if (irq_controller && irq_controller->enable) {
        irq_controller->enable(IRQ_PIT);
    }

    pit_running = true;
    pit_driver.enabled = true;
    LOG("PIT: Started at %zu Hz", pit_timer->frequency);
    return 0;
}

int pit_stop()
{
    // PIT'i durdurmak için genelde maskelenir veya divisor 0 yapılmaz; IRQ'yu kapatmak yeterlidir
    if (irq_controller && irq_controller->disable) {
        irq_controller->disable(IRQ_PIT);
    }
    pit_running = false;
    pit_driver.enabled = false;
    LOG("PIT: Stopped");
    return 0;
}

int pit_setPrescaler(uint32_t prescaler)
{
    if (prescaler == 0) prescaler = 0x10000; // 65536 anlamına gelir

    // Komut yaz
    outb(PIT_MODE_PORT, PIT_CMD_CH0_LOHI_MODE3);

    // Divisor low/high
    uint8_t lo = (uint8_t)(prescaler & 0xFF);
    uint8_t hi = (uint8_t)((prescaler >> 8) & 0xFF);
    outb(PIT_CH0_PORT, lo);
    outb(PIT_CH0_PORT, hi);

    // Etkili frekansı güncelle
    pit_timer->frequency = (prescaler == 0x10000) ? (PIT_BASE_FREQ / 65536u) : (PIT_BASE_FREQ / prescaler);

    return 0;
}

int pit_setFrequency(uint32_t frequency)
{
    if (frequency == 0) return -1;

    uint32_t divisor = PIT_BASE_FREQ / frequency;
    if (divisor == 0) divisor = 1; // max freq
    if (divisor > 0x10000) divisor = 0x10000; // min freq

    // Programla
    int rc = pit_setPrescaler(divisor);
    if (rc == 0) {
        // pit_setPrescaler zaten effective frequency'i güncelledi
        LOG("PIT: Frequency set -> target %u Hz, effective %zu Hz (div=%u)", frequency, pit_timer->frequency, divisor == 0x10000 ? 65536u : divisor);
    }
    return rc;
}

void pit_add_callback(void (*callback)())
{
    if (!callback) return;
    if (!pit_timer_tick_handlers) {
        pit_timer_tick_handlers = List_Create();
    }
    List_Add(pit_timer_tick_handlers, callback);
}

void pit_remove_callback(void (*callback)())
{
    if (pit_timer_tick_handlers && callback) {
        List_Remove(pit_timer_tick_handlers, callback);
    }
}

static void pit_enable() {
    pit_start();
}

static void pit_disable() {
    pit_stop();
}

DriverBase pit_driver = {
    .name = "PIT Driver",
    .init = pit_init,
    .context = NULL,
    .enabled = false,
    .version = 1,
    .type = DRIVER_TYPE_TIMER,
    .enable = pit_enable,
    .disable = pit_disable
};

HardwareTimer pit_timer_instance = {
    .name = "PIT",
    .frequency = 1000, // default OS tick (Hz)
    .context = NULL,
    .init = pit_timer_init_wrapper,
    .start = pit_start,
    .stop = pit_stop,
    .setPrescaler = pit_setPrescaler,
    .setFrequency = pit_setFrequency,
    .add_callback = pit_add_callback,
    .remove_callback = pit_remove_callback
};

void pit_timer_handler() 
{
    // Callback'leri çağır
    if (pit_timer_tick_handlers && pit_timer_tick_handlers->count > 0) {
        LIST_FOR_EACH(pit_timer_tick_handlers, it) {
            void (*cb)() = (void (*)())it->data;
            if (cb) cb();
        }
    }

    // EOI gönder
    if (irq_controller && irq_controller->acknowledge) {
        irq_controller->acknowledge(IRQ_PIT);
    }
}

static void pit_timer_init_wrapper()
{
    // HardwareTimer.init kontratı void; gerçek başlatmayı pit_start yapar
    // Burada sadece donanımı hedef frekans için programlayabiliriz.
    // Ancak sistem sürücü yaşam döngüsü pit_start'ı çağıracağından burada
    // ekstra iş yapmadan bırakıyoruz.
}