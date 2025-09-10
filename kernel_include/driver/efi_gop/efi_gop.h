#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <graphics/screen.h>
#include <stdbool.h>

// EFI GOP üzerinden mevcut video modlarını tespit eder, verilen ekranın
// video_modes listesine ekler, en yüksek çözünürlüklü moda geçer ve
// screen->mode işaretçisini seçilen moda ayarlar.
// Başarılıysa true döner; hata durumunda false döner ve ekran state’i değiştirmemeye çalışır.
bool efi_gop_detect(ScreenInfo* screen);

#ifdef __cplusplus
}
#endif

