#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Buffer node structure - her node'un arkasında data_size kadar alan var
typedef struct BufferNode {
    struct BufferNode* next;
    size_t data_size;        // Bu node'da saklanan verinin boyutu
    // Buradan sonra data_size kadar alan var (flexible array member kullanımı)
    uint8_t data[];         // Gerçek veri buraya yazılır
} BufferNode;

// Buffer structure - FIFO mantığında çalışır
typedef struct Buffer {
    BufferNode* head;       // İlk eleman (pop edilecek)
    BufferNode* tail;       // Son eleman (push edilen)
    size_t count;           // Buffer'daki eleman sayısı
    size_t total_size;      // Buffer'daki toplam veri boyutu
    size_t default_data_size; // Varsayılan veri boyutu
} Buffer;

// Buffer creation and destruction
Buffer* buffer_create(size_t default_data_size);
void buffer_destroy(Buffer* buffer);
void buffer_clear(Buffer* buffer);

// FIFO operations
int buffer_push(Buffer* buffer, const void* data);
int buffer_push_default(Buffer* buffer, const void* data);
void* buffer_pop(Buffer* buffer);
void* buffer_peek(Buffer* buffer);

// Buffer information
size_t buffer_count(Buffer* buffer);
size_t buffer_total_size(Buffer* buffer);
size_t buffer_data_size(Buffer* buffer);
bool buffer_is_empty(Buffer* buffer);

// Advanced operations
int buffer_push_copy(Buffer* buffer, const void* data);
BufferNode* buffer_pop_node(Buffer* buffer);
void buffer_free_node(BufferNode* node);

// Iterator functions
BufferNode* buffer_iterator_begin(Buffer* buffer);
BufferNode* buffer_iterator_next(BufferNode* current);
void* buffer_node_data(BufferNode* node);
size_t buffer_node_data_size(BufferNode* node);

#ifdef __cplusplus
}
#endif