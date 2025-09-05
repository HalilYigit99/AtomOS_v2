# Storage Read Test

Bu test örneği, BlockDevice soyutlaması üzerinden diskin ilk sektör(ler)ini nasıl okuyabileceğinizi gösterir. ATA PIO yolu şu an aktif; AHCI DMA okuma bir sonraki aşamada eklenecek. Test, sürücüleriniz bir veya daha fazla `BlockDevice` kaydettiğinde LBA 0 ve LBA 1'den okur ve küçük bir hexdump basar.

Dosyalar
- `tests/block_read_test.h` — test fonksiyon deklarasyonu
- `tests/block_read_test.c` — test uygulaması

Nasıl Çalıştırılır
1) Kernel boot akışında depolama sürücülerinin yüklendiğinden emin olun. `kernel/boot/boot.c` içinde AHCI ve ATA sürücüleri zaten register+enable ediliyor.
2) Geçici olarak test fonksiyonunu kernel içine çağırın; örneğin `kernel/kmain.c` içinde, ekran çizimi ve girişlerden sonra çağırabilirsiniz:

```
// kmain.c başına ekleyin
extern void block_read_test_run(void); // tests/block_read_test.h

// uygun bir noktada (sürücüler yüklendikten sonra)
block_read_test_run();
```

Notlar
- Test, `BlockDevice_InitRegistry()` çağırır ve kayıtlı aygıtları enumerate eder.
- Sadece okuma yapar; yazma ve flush no-op olarak kalır.
- ATA PIO uygulaması 512 bayt sektörlerde çalışır. 512 dışındaki sektör boyutları şimdilik testte atlanır.

